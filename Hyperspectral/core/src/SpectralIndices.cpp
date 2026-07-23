#include "hsi/SpectralIndices.h"
#include "hsi/Logger.h"
#include <cmath>
#include <algorithm>

namespace hsi {

// ── Hyperion wavelength approximation ────────────────────────────────────
static double sensorBandToWavelength(int sensorBand) {
    // VNIR: bands 1-70
    if (sensorBand <= 70) return 356.0 + (sensorBand - 1) * 10.17;
    // SWIR: bands 71-242
    return 852.0 + (sensorBand - 71) * 10.09;
}

int HyperionBandFinder::findClosest(const RasterCube& cube, double targetNm) {
    if (cube.bandNumbers.empty()) return -1;
    int bestIdx = -1;
    double bestDelta = 1e12;
    for (int i = 0; i < cube.bands; ++i) {
        if (cube.bandNumbers[i] <= 0) continue;
        double wl = sensorBandToWavelength(cube.bandNumbers[i]);
        double delta = std::fabs(wl - targetNm);
        if (delta < bestDelta) { bestDelta = delta; bestIdx = i; }
    }
    return bestIdx;
}

SpectralIndices::BandSet HyperionBandFinder::autoDetect(const RasterCube& cube) {
    SpectralIndices::BandSet bs;
    bs.blue  = findClosest(cube, 469.0);
    bs.green = findClosest(cube, 549.0);
    bs.red   = findClosest(cube, 660.0);
    bs.nir   = findClosest(cube, 865.0);
    bs.swir1 = findClosest(cube, 1610.0);
    bs.swir2 = findClosest(cube, 2150.0);
    return bs;
}

SpectralIndices::ClassBandSet HyperionBandFinder::autoDetectClassBands(const RasterCube& cube,
                                                                        double toleranceNm) {
    // Named per-class targets (nm). See ClassBandSet in SpectralIndices.h
    // for which land class each pair drives.
    static constexpr double kUrbanSwirNm  = 1590.0;
    static constexpr double kUrbanNirNm   = 860.0;
    static constexpr double kBarrenSwirNm = 1620.0;
    static constexpr double kBarrenNirNm  = 860.0;
    static constexpr double kVegRedNm     = 662.5; // midpoint of the 660/665 nm spec
    static constexpr double kVegNirNm     = 815.0;

    auto resolve = [&](double targetNm, const char* label) -> int {
        int idx = findClosest(cube, targetNm);
        if (idx >= 0 && !cube.bandNumbers.empty()) {
            double wl = sensorBandToWavelength(cube.bandNumbers[idx]);
            double delta = std::fabs(wl - targetNm);
            if (delta > toleranceNm) {
                Logger::log("HyperionBandFinder",
                    std::string(label) + ": nearest band is " + std::to_string(wl) +
                    " nm, " + std::to_string(delta) + " nm away from the requested " +
                    std::to_string(targetNm) + " nm (+/-" + std::to_string(toleranceNm) +
                    " nm) -- using closest available band anyway.");
            }
        }
        return idx;
    };

    SpectralIndices::ClassBandSet cb;
    cb.urbanSwir  = resolve(kUrbanSwirNm,  "Urban SWIR");
    cb.urbanNir   = resolve(kUrbanNirNm,   "Urban NIR");
    cb.barrenSwir = resolve(kBarrenSwirNm, "Barren-land SWIR");
    cb.barrenNir  = resolve(kBarrenNirNm,  "Barren-land NIR");
    cb.vegRed     = resolve(kVegRedNm,     "Soil/Forest/Vegetation Red");
    cb.vegNir     = resolve(kVegNirNm,     "Soil/Forest/Vegetation NIR");
    return cb;
}

// ── Generic normalised-difference helper ──────────────────────────────────
static RasterCube normDiff(const RasterCube& cube, int aIdx, int bIdx,
                            const std::string& name) {
    if (aIdx < 0 || bIdx < 0 || aIdx >= cube.bands || bIdx >= cube.bands)
        throw HsiError("SpectralIndices: invalid band indices for " + name);

    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform   = cube.geoTransform;
    out.projectionWkt  = cube.projectionWkt;
    out.bandNames      = { name };

    for (int row = 0; row < cube.height; ++row)
        for (int col = 0; col < cube.width; ++col) {
            float a = cube.at(aIdx, row, col);
            float b = cube.at(bIdx, row, col);
            float denom = a + b;
            out.at(0, row, col) = std::fabs(denom) < 1e-9f ? 0.0f : (a - b) / denom;
        }
    return out;
}

// ── Public index methods ──────────────────────────────────────────────────
RasterCube SpectralIndices::ndvi(const RasterCube& c, int nir, int red)
    { return normDiff(c, nir, red, "NDVI"); }

RasterCube SpectralIndices::ndwi(const RasterCube& c, int green, int nir)
    { return normDiff(c, green, nir, "NDWI"); }

RasterCube SpectralIndices::ndbi(const RasterCube& c, int swir1, int nir)
    { return normDiff(c, swir1, nir, "NDBI"); }

RasterCube SpectralIndices::bsi(const RasterCube& c, int swir1, int red, int nir, int blue) {
    if (swir1<0||red<0||nir<0||blue<0) throw HsiError("SpectralIndices::bsi: invalid band indices.");
    RasterCube out; out.allocate(c.width, c.height, 1);
    out.geoTransform = c.geoTransform; out.projectionWkt = c.projectionWkt;
    out.bandNames = {"BSI"};
    for (int row = 0; row < c.height; ++row)
        for (int col = 0; col < c.width; ++col) {
            float s = c.at(swir1,row,col), r = c.at(red,row,col),
                  n = c.at(nir,row,col),   b = c.at(blue,row,col);
            float num   = (s + r) - (n + b);
            float denom = (s + r) + (n + b);
            out.at(0,row,col) = std::fabs(denom)<1e-9f ? 0.0f : num/denom;
        }
    return out;
}

RasterCube SpectralIndices::evi(const RasterCube& c, int nir, int red, int blue,
                                 double G, double C1, double C2, double L) {
    if (nir<0||red<0||blue<0) throw HsiError("SpectralIndices::evi: invalid band indices.");
    RasterCube out; out.allocate(c.width, c.height, 1);
    out.geoTransform = c.geoTransform; out.projectionWkt = c.projectionWkt;
    out.bandNames = {"EVI"};
    for (int row = 0; row < c.height; ++row)
        for (int col = 0; col < c.width; ++col) {
            double n = c.at(nir,row,col), r = c.at(red,row,col), b = c.at(blue,row,col);
            double denom = n + C1*r - C2*b + L;
            out.at(0,row,col) = std::fabs(denom)<1e-9 ? 0.0f : static_cast<float>(G*(n-r)/denom);
        }
    return out;
}

RasterCube SpectralIndices::computeAll(const RasterCube& cube, const BandSet& bs) {
    std::vector<RasterCube> parts;
    if (bs.nir>=0 && bs.red>=0)                           parts.push_back(ndvi(cube,bs.nir,bs.red));
    if (bs.green>=0 && bs.nir>=0)                         parts.push_back(ndwi(cube,bs.green,bs.nir));
    if (bs.swir1>=0 && bs.nir>=0)                         parts.push_back(ndbi(cube,bs.swir1,bs.nir));
    if (bs.swir1>=0 && bs.red>=0 && bs.nir>=0 && bs.blue>=0) parts.push_back(bsi(cube,bs.swir1,bs.red,bs.nir,bs.blue));
    if (bs.nir>=0 && bs.red>=0 && bs.blue>=0)             parts.push_back(evi(cube,bs.nir,bs.red,bs.blue));

    if (parts.empty()) throw HsiError("SpectralIndices::computeAll: no valid band indices in BandSet.");

    RasterCube out; out.allocate(cube.width, cube.height, static_cast<int>(parts.size()));
    out.geoTransform = cube.geoTransform; out.projectionWkt = cube.projectionWkt;
    for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
        size_t n = cube.pixelCount();
        std::copy(parts[i].data.begin(), parts[i].data.begin()+n,
                  out.data.begin() + static_cast<size_t>(i)*n);
        out.bandNames[i] = parts[i].bandNames[0];
    }
    Logger::log("SpectralIndices", "Computed " + std::to_string(parts.size()) + " indices.");
    return out;
}

RasterCube SpectralIndices::threshold(const RasterCube& idx, float thresh, bool aboveIsOne) {
    RasterCube out; out.allocate(idx.width, idx.height, 1);
    out.geoTransform = idx.geoTransform; out.projectionWkt = idx.projectionWkt;
    for (size_t i = 0; i < idx.pixelCount(); ++i) {
        bool above = idx.data[i] > thresh;
        out.data[i] = (above == aboveIsOne) ? 1.0f : 0.0f;
    }
    return out;
}

RasterCube SpectralIndices::mergeClassMasks(
    const std::vector<std::pair<int, RasterCube>>& masks,
    int w, int h, const std::array<double,6>& gt, const std::string& proj)
{
    RasterCube out; out.allocate(w, h, 1);
    out.geoTransform = gt; out.projectionWkt = proj;
    out.bandNames = {"land_cover_class"};
    // default: all unclassified (0)
    for (const auto& [label, mask] : masks)
        for (size_t i = 0; i < mask.pixelCount(); ++i)
            if (mask.data[i] > 0.5f && out.data[i] < 0.5f) out.data[i] = static_cast<float>(label);
    return out;
}

} // namespace hsi
