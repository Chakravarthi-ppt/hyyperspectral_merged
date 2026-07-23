#pragma once
#include "hsi/Types.h"

namespace hsi {

class Orthorectifier {
public:
    struct Options {
        std::string targetSrsWkt;      // empty = keep dataset's own SRS / WGS84 UTM fallback
        double pixelSizeX = 30.0;      // output pixel size, metres (Hyperion native)
        double pixelSizeY = 30.0;
        std::string resampleAlg = "bilinear"; // nearest | bilinear | cubic
    };

    struct Outcome {
        bool wasAlreadyOrthorectified = false;
        std::string outputPath;        // path actually used downstream (input or new warped file)
    };

    // If `inputPath` already looks orthorectified (RasterIO::looksOrthorectified),
    // this is a no-op and outputPath == inputPath. Otherwise it warps the
    // dataset (using its GCPs/RPCs) to a projected grid at outputPath.
    static Outcome ensureOrthorectified(const std::string& inputPath,
                                         const std::string& outputPath,
                                         const Options& opts);

    // Convenience overload using default Options (30m pixel size, bilinear).
    static Outcome ensureOrthorectified(const std::string& inputPath,
                                         const std::string& outputPath);
};

} // namespace hsi
