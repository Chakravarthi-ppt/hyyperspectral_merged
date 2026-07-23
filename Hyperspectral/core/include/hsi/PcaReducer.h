#pragma once
#include "hsi/Types.h"

namespace hsi {

// PCA dimensionality reduction (Eigen-based) over a contiguous band range,
// e.g. bands 70-224 collapsing the SWIR portion of the cube to one or a
// few principal-component bands before stacking.
class PcaReducer {
public:
    struct Result {
        RasterCube components;                 // `numComponents` bands, ordered by decreasing variance
        std::vector<double> explainedVarianceRatio; // length == numComponents
    };

    // startBandIndex/endBandIndex are 0-based, inclusive, into `cube`.
    static Result reduce(const RasterCube& cube, int startBandIndex, int endBandIndex, int numComponents = 1);
};

} // namespace hsi
