#include "hsi/LandCoverMapper.h"
#include "hsi/SpectralIndices.h"
#include "hsi/SamClassifier.h"
#include "hsi/Logger.h"
#include <cmath>
#include <map>
#include <algorithm>

namespace hsi {

// ── Index range diagnostics ────────────────────────────────────────────────
// Background/no-data pixels get forced to exactly 0.0f by normDiff()/bsi()'s
// zero-denominator guard, and typically dominate pixel count for a narrow
// swath on a rectangular grid (see the black margins around a Hyperion
// strip). A genuine computed index landing on exactly 0.0 is practically
// impossible for continuous reflectance data, so excluding exact zeros is a
// pragmatic way to report the real scene's actual range instead of a range
// swamped by background -- which is what you need to pick a sane threshold
// instead of guessing one (0.00 is a common default that may not exist
// anywhere in a particular scene's real NDBI/BSI values).
// Returns {p10, p50, p90} of non-background pixels and logs the range.
// Used to auto-adapt thresholds to each scene instead of using fixed values
// that may lie outside the actual index distribution (the root cause of the
// 97.9% Unclassified result: thresholds of 0.00 for NDBI/BSI were valid
// defaults but fell outside [-0.35, -0.10] for this particular Hyperion strip).
static std::array<float,3> logIndexStats(const std::string& name, const RasterCube& idx) {
    std::vector<float> vals;
    vals.reserve(idx.data.size() / 4);
    for (float v : idx.data) if (v != 0.0f) vals.push_back(v);
    if (vals.empty()) {
        Logger::log("LandCoverMapper", name + ": no non-background pixels.");
        return {0.f, 0.f, 0.f};
    }
    std::sort(vals.begin(), vals.end());
    auto pct = [&](double p) -> float {
        return vals[static_cast<size_t>(p * (vals.size() - 1))];
    };
    float lo = vals.front(), hi = vals.back(), mean = 0.f;
    for (float v : vals) mean += v;
    mean /= static_cast<float>(vals.size());
    float p10 = pct(0.10), p50 = pct(0.50), p90 = pct(0.90);
    Logger::log("LandCoverMapper",
        name + " scene stats: min=" + std::to_string(lo) +
        " p10=" + std::to_string(p10) +
        " p50=" + std::to_string(p50) +
        " p90=" + std::to_string(p90) +
        " max=" + std::to_string(hi) +
        " mean=" + std::to_string(mean) +
        " (n=" + std::to_string(vals.size()) + " real pixels)");
    return {p10, p50, p90};
}

// Auto-adapt a threshold to the scene.
// threshold_pct in [0,1]: what fraction of real pixels should be "above" threshold.
// E.g. 0.15 means top-15% of real values are classified as this class.
static float adaptThreshold(const std::array<float,3>& stats, float threshold_pct) {
    // stats = {p10, p50, p90}. Interpolate within this range.
    // threshold_pct=0.15 -> threshold at p85 (top 15%): p50 + 0.7*(p90-p50)
    float alpha = 1.0f - threshold_pct;   // 0.85 for top-15%
    if (alpha <= 0.5f)
        return stats[1] + (alpha / 0.5f) * (stats[0] - stats[1]);  // p50 .. p10
    else
        return stats[1] + ((alpha - 0.5f) / 0.5f) * (stats[2] - stats[1]);  // p50 .. p90
}

// ── Mode 1: Index-based ───────────────────────────────────────────────────
RasterCube LandCoverMapper::classifyByIndices(const RasterCube& cube,
                                               SpectralIndices::BandSet bands,
                                               const IndexThresholds& thr,
                                               bool autoDetect) {
    if (autoDetect && !cube.bandNumbers.empty())
        bands = HyperionBandFinder::autoDetect(cube);

    // Per-class band pairs: Built-up and Barren-land each get their own SWIR
    // target, and Forest/Vegetation gets its own Red/NIR target, instead of
    // sharing one global BandSet (see SpectralIndices::ClassBandSet).
    SpectralIndices::ClassBandSet classBands;
    if (autoDetect && !cube.bandNumbers.empty())
        classBands = HyperionBandFinder::autoDetectClassBands(cube);

    // Compute each index, threshold, build priority-ordered mask list.
    // Priority: Water > Forest > Built-up > Vegetation > Bare Soil.
    // Built-up is deliberately checked *before* the general Vegetation
    // class: Vegetation's threshold (NDVI > ~0.2) is loose enough that most
    // real urban/mixed pixels -- which usually carry at least a little
    // green signal from roadside trees, parks, lawns -- would otherwise be
    // claimed as Vegetation before NDBI ever got a chance, leaving
    // Built-up permanently empty regardless of its own threshold.
    std::vector<std::pair<int, RasterCube>> masks;

    // For each index we:
    //   1. Compute the index map.
    //   2. Log its scene statistics (min/p10/p50/p90/max/mean).
    //   3. If the user-supplied threshold lies *outside* the real [p10,p90]
    //      range, auto-adapt it using the scene distribution so that a
    //      reasonable fraction of pixels gets classified rather than 0%.
    //      The user threshold is used as-is when it's already sensible.
    //      This is what fixed the 97.9% Unclassified result: the default
    //      ndbi=0.00 / bsi=0.00 thresholds fell well above the actual range
    //      of this Hyperion strip, so no pixel ever exceeded them.

    auto withinRange = [](float t, const std::array<float,3>& s) {
        return t >= s[0] && t <= s[2]; // within [p10,p90]
    };

    if (bands.green >= 0 && bands.nir >= 0) {
        auto ndwi = SpectralIndices::ndwi(cube, bands.green, bands.nir);
        auto s = logIndexStats("NDWI", ndwi);
        float t = withinRange(thr.ndwi, s) ? thr.ndwi : adaptThreshold(s, 0.10f);
        Logger::log("LandCoverMapper", "NDWI threshold used: " + std::to_string(t) +
            (withinRange(thr.ndwi, s) ? " (user)" : " (auto-adapted from scene stats)"));
        masks.push_back({ static_cast<int>(LandClass::Water),
                          SpectralIndices::threshold(ndwi, t, true) });
    }

    // NDVI (Forest + Vegetation) uses the Soil/Forest/Vegetation Red/NIR
    // target (~660-665 nm Red, ~815 nm NIR) rather than the shared BandSet's
    // red/nir, falling back to the shared BandSet if per-class bands weren't
    // resolved (e.g. autoDetect disabled and bands passed in explicitly).
    int ndviRed = classBands.vegRed >= 0 ? classBands.vegRed : bands.red;
    int ndviNir = classBands.vegNir >= 0 ? classBands.vegNir : bands.nir;
    bool haveNdvi = ndviNir >= 0 && ndviRed >= 0;
    RasterCube ndvi;
    if (haveNdvi) {
        ndvi = SpectralIndices::ndvi(cube, ndviNir, ndviRed);
        auto s = logIndexStats("NDVI", ndvi);
        float tForest = withinRange(thr.ndviForest, s) ? thr.ndviForest : adaptThreshold(s, 0.20f);
        Logger::log("LandCoverMapper", "NDVI-forest threshold used: " + std::to_string(tForest) +
            (withinRange(thr.ndviForest, s) ? " (user)" : " (auto-adapted)"));
        masks.push_back({ static_cast<int>(LandClass::Forest),
                          SpectralIndices::threshold(ndvi, tForest, true) });
    }

    // NDBI (Built-up / Urban) uses the Urban SWIR/NIR target (~1590 nm SWIR,
    // ~860 nm NIR), falling back to the shared BandSet's swir1/nir if
    // per-class bands weren't resolved.
    int urbanSwir = classBands.urbanSwir >= 0 ? classBands.urbanSwir : bands.swir1;
    int urbanNir  = classBands.urbanNir  >= 0 ? classBands.urbanNir  : bands.nir;
    if (urbanSwir >= 0 && urbanNir >= 0) {
        auto ndbi = SpectralIndices::ndbi(cube, urbanSwir, urbanNir);
        auto s = logIndexStats("NDBI", ndbi);
        float t = withinRange(thr.ndbi, s) ? thr.ndbi : adaptThreshold(s, 0.15f);
        Logger::log("LandCoverMapper", "NDBI threshold used: " + std::to_string(t) +
            (withinRange(thr.ndbi, s) ? " (user)" : " (auto-adapted)"));
        masks.push_back({ static_cast<int>(LandClass::BuiltUp),
                          SpectralIndices::threshold(ndbi, t, true) });
    }

    if (haveNdvi) {
        auto s = logIndexStats("NDVI-veg", ndvi);
        float tVeg = withinRange(thr.ndviVegetation, s) ? thr.ndviVegetation : adaptThreshold(s, 0.40f);
        Logger::log("LandCoverMapper", "NDVI-vegetation threshold used: " + std::to_string(tVeg) +
            (withinRange(thr.ndviVegetation, s) ? " (user)" : " (auto-adapted)"));
        masks.push_back({ static_cast<int>(LandClass::Vegetation),
                          SpectralIndices::threshold(ndvi, tVeg, true) });
    }

    // Barren-land / Bare Soil: per the spec this is a simple normalised-
    // difference of its own SWIR/NIR pair (~1620 nm SWIR, ~860 nm NIR) --
    // not the 4-band BSI formula, which used the shared BandSet's
    // red/nir/swir1/blue. Falls back to BSI if per-class bands weren't
    // resolved and the shared BandSet has all four bands available.
    int barrenSwir = classBands.barrenSwir >= 0 ? classBands.barrenSwir : bands.swir1;
    int barrenNir  = classBands.barrenNir  >= 0 ? classBands.barrenNir  : bands.nir;
    if (barrenSwir >= 0 && barrenNir >= 0) {
        auto barren = SpectralIndices::ndbi(cube, barrenSwir, barrenNir);
        auto s = logIndexStats("BarrenLandIndex", barren);
        float t = withinRange(thr.bsi, s) ? thr.bsi : adaptThreshold(s, 0.10f);
        Logger::log("LandCoverMapper", "Barren-land index threshold used: " + std::to_string(t) +
            (withinRange(thr.bsi, s) ? " (user)" : " (auto-adapted)"));
        masks.push_back({ static_cast<int>(LandClass::BareSoil),
                          SpectralIndices::threshold(barren, t, true) });
    } else if (bands.swir1 >= 0 && bands.red >= 0 && bands.nir >= 0 && bands.blue >= 0) {
        auto bsi = SpectralIndices::bsi(cube, bands.swir1, bands.red, bands.nir, bands.blue);
        auto s = logIndexStats("BSI", bsi);
        float t = withinRange(thr.bsi, s) ? thr.bsi : adaptThreshold(s, 0.10f);
        Logger::log("LandCoverMapper", "BSI threshold used: " + std::to_string(t) +
            (withinRange(thr.bsi, s) ? " (user)" : " (auto-adapted)"));
        masks.push_back({ static_cast<int>(LandClass::BareSoil),
                          SpectralIndices::threshold(bsi, t, true) });
    }

    if (masks.empty())
        throw HsiError("LandCoverMapper::classifyByIndices: no usable bands in BandSet. "
                        "Ensure bandNumbers[] are populated in the reflectance cube.");

    auto result = SpectralIndices::mergeClassMasks(masks, cube.width, cube.height,
                                                    cube.geoTransform, cube.projectionWkt);
    result.bandNames = { "land_cover_index_based" };

    long counts[7] = {};
    for (float v : result.data) {
        int c = static_cast<int>(v);
        if (c >= 0 && c <= 6) ++counts[c];
    }
    Logger::log("LandCoverMapper",
        "Index-based classification complete. "
        "Built-up=" + std::to_string(counts[1]) +
        " Forest=" + std::to_string(counts[2]) +
        " Vegetation=" + std::to_string(counts[3]) +
        " Water=" + std::to_string(counts[4]) +
        " BareSoil=" + std::to_string(counts[5]) +
        " Unclassified=" + std::to_string(counts[0]));
    return result;
}

// ── Mode 2: SAM library-based ─────────────────────────────────────────────
RasterCube LandCoverMapper::classifyByLibrary(const RasterCube& cube,
                                               const SpectralLibrary& library,
                                               double angleThresholdRad) {
    std::map<int, std::string> legend;
    RasterCube samResult = SamClassifier::classify(cube, library, angleThresholdRad, &legend);

    // Re-map SAM class indices to LandClass IDs using the class name.
    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;
    out.bandNames = { "land_cover_library_based" };

    auto nameToClass = [](const std::string& name) -> int {
        // Case-insensitive prefix matching on standard names.
        auto lc = name;
        for (auto& ch : lc) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lc.find("built") != std::string::npos || lc.find("urban") != std::string::npos)
            return static_cast<int>(LandClass::BuiltUp);
        if (lc.find("forest") != std::string::npos || lc.find("tree") != std::string::npos)
            return static_cast<int>(LandClass::Forest);
        if (lc.find("veg") != std::string::npos || lc.find("grass") != std::string::npos
            || lc.find("crop") != std::string::npos || lc.find("agri") != std::string::npos)
            return static_cast<int>(LandClass::Vegetation);
        if (lc.find("water") != std::string::npos || lc.find("river") != std::string::npos
            || lc.find("lake") != std::string::npos || lc.find("sea") != std::string::npos)
            return static_cast<int>(LandClass::Water);
        if (lc.find("soil") != std::string::npos || lc.find("bare") != std::string::npos
            || lc.find("sand") != std::string::npos)
            return static_cast<int>(LandClass::BareSoil);
        return static_cast<int>(LandClass::Other);
    };

    for (size_t i = 0; i < samResult.pixelCount(); ++i) {
        int samIdx = static_cast<int>(samResult.data[i]);
        if (samIdx == 0) { out.data[i] = 0.0f; continue; }
        auto it = legend.find(samIdx);
        out.data[i] = (it != legend.end())
                          ? static_cast<float>(nameToClass(it->second))
                          : static_cast<float>(LandClass::Other);
    }

    Logger::log("LandCoverMapper", "Library-based (SAM) classification complete.");
    return out;
}

// ── Mode 3: SVM click-to-train ────────────────────────────────────────────
LandCoverMapper::SvmResult LandCoverMapper::classifyBySvm(
    const RasterCube& cube,
    const std::map<LandClass, std::vector<std::pair<int,int>>>& classSamples,
    const SvmModel::Options& svmOpts)
{
    std::vector<std::vector<float>> features;
    std::vector<int> labels;

    for (const auto& [cls, pixels] : classSamples) {
        for (auto [row, col] : pixels) {
            if (row < 0 || row >= cube.height || col < 0 || col >= cube.width) continue;
            features.push_back(cube.pixelSpectrum(row, col));
            labels.push_back(static_cast<int>(cls));
        }
    }
    if (features.empty())
        throw HsiError("LandCoverMapper::classifyBySvm: no valid sample pixels provided.");

    SvmResult result;
    result.model.train(features, labels, svmOpts);
    result.classified = result.model.classifyCube(cube);
    result.classified.bandNames = { "land_cover_svm" };

    Logger::log("LandCoverMapper", "SVM land-cover classification complete ("
                + std::to_string(features.size()) + " training pixels, "
                + std::to_string(classSamples.size()) + " classes).");
    return result;
}

// ── Combine two classifications ───────────────────────────────────────────
RasterCube LandCoverMapper::combine(const RasterCube& primary, const RasterCube& fallback) {
    if (!primary.sameGridAs(fallback))
        throw HsiError("LandCoverMapper::combine: primary and fallback are on different grids.");

    RasterCube out;
    out.allocate(primary.width, primary.height, 1);
    out.geoTransform = primary.geoTransform;
    out.projectionWkt = primary.projectionWkt;
    out.bandNames = { "land_cover_combined" };

    for (size_t i = 0; i < primary.pixelCount(); ++i)
        out.data[i] = (primary.data[i] < 0.5f) ? fallback.data[i] : primary.data[i];

    Logger::log("LandCoverMapper", "Combined two land-cover layers.");
    return out;
}

} // namespace hsi

// ── Convenience overloads ─────────────────────────────────────────────────
namespace hsi {

RasterCube LandCoverMapper::classifyByIndices(const RasterCube& cube) {
    SpectralIndices::BandSet bs;
    LandCoverIndexThresholds thr;
    return classifyByIndices(cube, bs, thr, true);
}

LandCoverMapper::SvmResult LandCoverMapper::classifyBySvm(
    const RasterCube& cube,
    const std::map<LandClass, std::vector<std::pair<int,int>>>& classSamples) {
    return classifyBySvm(cube, classSamples, SvmModel::Options());
}

} // namespace hsi
