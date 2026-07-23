#pragma once
#include "hsi/Types.h"
#include "hsi/SvmModel.h"
#include <map>
#include <string>

namespace hsi {

// Produces the two parallel LULC datasets called for in the workflow:
// a supervised classification (trained on labeled samples) and an
// unsupervised one (clustering with no labels) -- compared downstream
// by ChangeDetector / for accuracy assessment.
class LulcClassifier {
public:
    // k = number of land cover clusters.
    // k-means clusters are numbered 1..k with no inherent meaning -- if
    // `clusterLabelsOut` is non-null, it's filled with a best-guess semantic
    // label per cluster (e.g. "likely Water"), derived by running the same
    // NDVI/NDWI/NDBI/BSI thresholds used in LandCoverMapper against each
    // cluster's centroid spectrum. These are guesses to orient a human
    // reader, not a substitute for supervised classification -- always
    // sanity-check a couple of pixels from each cluster against the image.
    static RasterCube unsupervisedKMeans(const RasterCube& cube, int k, int attempts = 5,
                                          std::map<int, std::string>* clusterLabelsOut = nullptr);

    // Trains an SvmModel on the given labeled samples and classifies the
    // whole cube. Returns both the trained model (so it can be saved/reused)
    // and the classified raster.
    struct SupervisedResult {
        RasterCube classified;
        SvmModel model;
    };
    static SupervisedResult supervised(const RasterCube& cube,
                                        const std::vector<std::vector<float>>& trainFeatures,
                                        const std::vector<int>& trainLabels,
                                        const SvmModel::Options& svmOptions = {});
};

} // namespace hsi
