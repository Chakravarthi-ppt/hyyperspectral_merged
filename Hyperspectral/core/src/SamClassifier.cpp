#include "hsi/SamClassifier.h"
#include "hsi/Logger.h"

#include <cmath>
#include <limits>

namespace hsi {

RasterCube SamClassifier::classify(const RasterCube& cube,
                                    const SpectralLibrary& library,
                                    double angleThresholdRad,
                                    std::map<int, std::string>* classLegend) {
    if (library.signatures.empty()) throw HsiError("SamClassifier: spectral library is empty.");

    // Ensure every signature has exactly cube.bands values.
    // If the library was built from a 198-band reflectance cube but the
    // caller is now classifying a 206-band fused stack (or vice versa),
    // we resample by linear interpolation rather than throwing -- so SAM
    // still works without requiring the user to rebuild the library.
    std::vector<SpectralSignature> resampledSigs;
    resampledSigs.reserve(library.signatures.size());
    for (const auto& sig : library.signatures) {
        if (static_cast<int>(sig.meanReflectance.size()) == cube.bands) {
            resampledSigs.push_back(sig);
            continue;
        }
        // Resample by linear interpolation from src.size() to cube.bands values.
        SpectralSignature r;
        r.className = sig.className;
        r.meanReflectance.resize(cube.bands);
        const int srcN = static_cast<int>(sig.meanReflectance.size());
        if (srcN == 0) throw HsiError("SamClassifier: signature '" + sig.className + "' has 0 bands.");
        for (int i = 0; i < cube.bands; ++i) {
            double pos = static_cast<double>(i) * (srcN - 1) / std::max(1, cube.bands - 1);
            int    lo  = static_cast<int>(pos);
            int    hi  = std::min(lo + 1, srcN - 1);
            double frac = pos - lo;
            r.meanReflectance[i] = sig.meanReflectance[lo] * (1.0 - frac)
                                 + sig.meanReflectance[hi] * frac;
        }
        Logger::log("SamClassifier", "Resampled signature '" + sig.className + "' from " +
                    std::to_string(srcN) + " to " + std::to_string(cube.bands) + " bands.");
        resampledSigs.push_back(std::move(r));
    }
    // Use resampled library for the rest of the function
    const std::vector<SpectralSignature>& sigs = resampledSigs;

    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;
    out.bandNames = { "sam_class" };

    if (classLegend) {
        classLegend->clear();
        (*classLegend)[0] = "unclassified";
        for (size_t i = 0; i < sigs.size(); ++i)
            (*classLegend)[static_cast<int>(i) + 1] = sigs[i].className;
    }

    long unclassifiedCount = 0;
    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            auto pixel = cube.pixelSpectrum(row, col);

            double pixelNorm = 0.0;
            for (double v : pixel) pixelNorm += v * v;
            pixelNorm = std::sqrt(pixelNorm);

            double bestAngle = std::numeric_limits<double>::max();
            int bestClass = 0;
            if (pixelNorm > 1e-9) {
                for (size_t s = 0; s < sigs.size(); ++s) {
                    const auto& ref = sigs[s].meanReflectance;
                    double dot = 0.0, refNorm = 0.0;
                    for (int b = 0; b < cube.bands; ++b) {
                        dot += pixel[b] * ref[b];
                        refNorm += ref[b] * ref[b];
                    }
                    refNorm = std::sqrt(refNorm);
                    if (refNorm < 1e-9) continue;  // skip zero-length library signature
                    double cosAngle = dot / (pixelNorm * refNorm);
                    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
                    double angle = std::acos(cosAngle);
                    if (angle < bestAngle) { bestAngle = angle; bestClass = static_cast<int>(s) + 1; }
                }
            }

            if (bestAngle > angleThresholdRad) {
                bestClass = 0;
                ++unclassifiedCount;
            }
            out.at(0, row, col) = static_cast<float>(bestClass);
        }
    }

    Logger::log("SamClassifier", "SAM classification complete against " +
                std::to_string(sigs.size()) + " signature(s); " +
                std::to_string(unclassifiedCount) + " of " + std::to_string(cube.pixelCount()) +
                " pixels left unclassified (angle threshold = " + std::to_string(angleThresholdRad) + " rad).");
    return out;
}

} // namespace hsi
