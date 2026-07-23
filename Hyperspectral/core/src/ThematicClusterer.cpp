#include "thematicclusterer.h"
#include "hsi/Logger.h"

#include <opencv2/core.hpp>
#include <algorithm>

namespace hsi {

ThematicClusterer::Result ThematicClusterer::cluster(const RasterCube& cube, const Options& opt) {
    if (cube.pixelCount() == 0 || cube.bands == 0)
        throw HsiError("ThematicClusterer::cluster: empty cube.");

    int k = std::max(2, std::min(20, opt.k));

    std::vector<int> bands = opt.bandIndices;
    if (bands.empty()) {
        // Same representative Blue/Green/Red/NIR/SWIR subset used by
        // SurfaceObjectMask's k-means -- enough spectral variance to
        // separate distinct cover types without the memory cost of
        // clustering on all ~198 bands.
        for (int idx : {3, 12, 20, 22, 63})
            if (idx < cube.bands) bands.push_back(idx);
        if (bands.empty()) bands = {0};
    }

    const int nFeatures = static_cast<int>(bands.size());
    const long nSamples = static_cast<long>(cube.pixelCount());
    cv::Mat samples(static_cast<int>(nSamples), nFeatures, CV_32F);

    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            int sampleIdx = row * cube.width + col;
            for (int j = 0; j < nFeatures; ++j)
                samples.at<float>(sampleIdx, j) = cube.at(bands[j], row, col);
        }
    }

    cv::Mat labels, centers;
    cv::kmeans(samples, k, labels,
               cv::TermCriteria(cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS, 50, 0.5),
               std::max(1, opt.attempts), cv::KMEANS_PP_CENTERS, centers);

    Result result;
    result.clusters.allocate(cube.width, cube.height, 1);
    result.clusters.geoTransform = cube.geoTransform;
    result.clusters.projectionWkt = cube.projectionWkt;
    result.clusters.bandNames = { "thematic_clusters" };

    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            int sampleIdx = row * cube.width + col;
            int c = labels.at<int>(sampleIdx, 0);
            result.clusters.at(0, row, col) = static_cast<float>(c);
        }
    }

    result.clusterMeanBrightness.assign(k, 0.0);
    for (int c = 0; c < k; ++c) {
        double sum = 0.0;
        for (int j = 0; j < nFeatures; ++j) sum += centers.at<float>(c, j);
        result.clusterMeanBrightness[c] = sum / std::max(1, nFeatures);
    }

    Logger::log("ThematicClusterer",
        "K-means thematic clustering complete: k=" + std::to_string(k) +
        " over " + std::to_string(nFeatures) + " bands, " +
        std::to_string(nSamples) + " pixels.");

    return result;
}

} // namespace hsi
