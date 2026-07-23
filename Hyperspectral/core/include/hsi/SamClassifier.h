#pragma once
#include "hsi/Types.h"
#include "hsi/SpectralLibrary.h"
#include <map>

namespace hsi {

// Spectral Angle Mapper: classifies each pixel by the smallest spectral
// angle to any reference signature in the library. Pixels whose best angle
// exceeds `angleThresholdRad` are left unclassified (class 0).
class SamClassifier {
public:
    // Output is a single-band integer raster: 0 = unclassified, 1..N = class
    // index into `library.signatures` (1-indexed). `classLegend` is filled
    // with index -> class name if provided.
    static RasterCube classify(const RasterCube& cube,
                                const SpectralLibrary& library,
                                double angleThresholdRad,
                                std::map<int, std::string>* classLegend = nullptr);
};

} // namespace hsi
