#pragma once
#include "hsi/Types.h"
#include <vector>

namespace hsi {

// Unsupervised spectral clustering used for the "Thematic" pipeline step.
// Unlike SurfaceObjectMask (which always collapses k-means down to a binary
// 0/1 surface-vs-object mask), this returns the raw k-cluster labeling
// (0..k-1) so the scene's natural spectral groupings can be viewed directly
// as a thematic map -- no attempt is made to name or merge clusters into
// land-cover classes; that's what the Classification step is for.
class ThematicClusterer {
public:
    struct Options {
        int k = 5;                     // number of clusters to show (2-20)
        int attempts = 3;               // k-means restarts
        std::vector<int> bandIndices;   // empty = representative subset (see .cpp)
    };

    struct Result {
        RasterCube clusters;            // single-band raster, values 0..k-1
        std::vector<double> clusterMeanBrightness; // per-cluster mean over used bands, for palette ordering
    };

    static Result cluster(const RasterCube& cube, const Options& opt);
};

} // namespace hsi
