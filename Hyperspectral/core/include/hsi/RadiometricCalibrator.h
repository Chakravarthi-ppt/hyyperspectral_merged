#pragma once
#include "hsi/Types.h"

namespace hsi {

// Converts raw 12-bit Hyperion DN to spectral radiance (W / m^2 / sr / um),
// following the standard split scaling factor: VNIR bands divided by 40,
// SWIR bands divided by 80. Band ranges are configurable for other sensors.
class RadiometricCalibrator {
public:
    struct Options {
        int vnirStartBand = 8,  vnirEndBand = 57;   // Hyperion calibrated VNIR range
        double vnirScaleFactor = 40.0;
        int swirStartBand = 77, swirEndBand = 224;  // Hyperion calibrated SWIR range
        double swirScaleFactor = 80.0;
        // Bands outside both ranges (overlap zone 58-76, uncalibrated tail
        // 225-242) are passed through unscaled with a logged warning --
        // callers should run BandSelector first to drop them.
    };

    // `dnCube.bandNumbers` must hold the original sensor band index (1-indexed)
    // for each stored band so the correct scale factor can be looked up.
    static RasterCube dnToRadiance(const RasterCube& dnCube, const Options& opt);
    static RasterCube dnToRadiance(const RasterCube& dnCube); // uses default Options
};

} // namespace hsi
