#include "hsi/BandSelector.h"
#include "hsi/Logger.h"
#include <algorithm>

namespace hsi {

bool BandSelector::isKnownBadBand(int sensorBand) {
    return (sensorBand >= 120 && sensorBand <= 132) ||  // water vapor absorption
           (sensorBand >= 165 && sensorBand <= 182) ||  // water vapor absorption
           (sensorBand >= 221 && sensorBand <= 224);    // low SNR tail of SWIR
}

RasterCube BandSelector::selectCalibratedBands(const RasterCube& fullCube) {
    return selectCalibratedBands(fullCube, Rule());
}

RasterCube BandSelector::selectCalibratedBands(const RasterCube& fullCube, const Rule& rule) {
    if (fullCube.bandNumbers.empty() || static_cast<int>(fullCube.bandNumbers.size()) != fullCube.bands) {
        throw HsiError("BandSelector: cube has no per-band sensor band numbers.");
    }

    // A reversed or negative range is a config mistake, not a valid "no
    // bands in this range" case -- it silently produced an empty keepIndices
    // that would only surface later as a confusing "no bands matched" error
    // with no clue why.
    if (rule.vnirStartBand < 0 || rule.vnirStartBand > rule.vnirEndBand ||
        rule.swirStartBand < 0 || rule.swirStartBand > rule.swirEndBand) {
        throw HsiError("BandSelector: invalid band range in Rule (VNIR " +
                        std::to_string(rule.vnirStartBand) + "-" + std::to_string(rule.vnirEndBand) +
                        ", SWIR " + std::to_string(rule.swirStartBand) + "-" + std::to_string(rule.swirEndBand) +
                        ") -- start must be <= end and both must be >= 0.");
    }

    // Duplicate sensor band numbers in the source cube (e.g. from a prior
    // merge/archive-extraction bug) would otherwise get selected twice here,
    // silently doubling that wavelength's weight in every downstream index
    // and classifier. Catch it at the source instead.
    {
        std::vector<int> sorted = fullCube.bandNumbers;
        std::sort(sorted.begin(), sorted.end());
        auto dup = std::adjacent_find(sorted.begin(), sorted.end());
        if (dup != sorted.end()) {
            throw HsiError("BandSelector: input cube has duplicate sensor band number " +
                            std::to_string(*dup) + " -- check how it was assembled (merge/extract step).");
        }
    }

    std::vector<int> keepIndices;
    int droppedBadBands = 0;
    for (int b = 0; b < fullCube.bands; ++b) {
        int sb = fullCube.bandNumbers[b];
        bool inVnir = sb >= rule.vnirStartBand && sb <= rule.vnirEndBand;
        bool inSwir = sb >= rule.swirStartBand && sb <= rule.swirEndBand;
        if (!(inVnir || inSwir)) continue;
        if (rule.excludeKnownBadBands && isKnownBadBand(sb)) { ++droppedBadBands; continue; }
        keepIndices.push_back(b);
    }

    if (keepIndices.empty()) {
        throw HsiError("BandSelector: no bands matched the VNIR/SWIR calibrated ranges.");
    }

    RasterCube out;
    out.allocate(fullCube.width, fullCube.height, static_cast<int>(keepIndices.size()));
    out.geoTransform = fullCube.geoTransform;
    out.projectionWkt = fullCube.projectionWkt;

    for (size_t i = 0; i < keepIndices.size(); ++i) {
        int srcBand = keepIndices[i];
        size_t srcBase = static_cast<size_t>(srcBand) * fullCube.width * fullCube.height;
        size_t dstBase = i * fullCube.width * fullCube.height;
        std::copy(fullCube.data.begin() + srcBase, fullCube.data.begin() + srcBase + fullCube.pixelCount(),
                  out.data.begin() + dstBase);
        out.bandNumbers[i] = fullCube.bandNumbers[srcBand];
        out.bandNames[i] = fullCube.bandNames[srcBand];
    }

    Logger::log("BandSelector", "Kept " + std::to_string(keepIndices.size()) + " of " +
                std::to_string(fullCube.bands) + " bands (dropped overlap zone " +
                std::to_string(rule.vnirEndBand + 1) + "-" + std::to_string(rule.swirStartBand - 1) +
                ", any band outside " + std::to_string(rule.vnirStartBand) + "-" +
                std::to_string(rule.swirEndBand) +
                (rule.excludeKnownBadBands ? (", and " + std::to_string(droppedBadBands) +
                                               " known noisy/water-absorption bands") : std::string()) +
                ").");
    return out;
}

} // namespace hsi
