#include "hsi/LulcClassifier.h"
#include "hsi/Logger.h"
#include "hsi/SpectralIndices.h"

#include <opencv2/core.hpp>
#include <cmath>

namespace hsi {

namespace {

float safeNormDiff(float a, float b) {
    float d = a + b;
    return std::fabs(d) < 1e-9f ? 0.0f : (a - b) / d;
}

// Same priority order and default thresholds as LandCoverMapper::classifyByIndices
// (Water > Forest > Built-up > Vegetation > Bare Soil), just evaluated once
// against a cluster's centroid spectrum instead of per-pixel. This is a
// best-effort guess to make "Cluster 3" mean something to a human at a
// glance -- it is not a replacement for verifying against known ground
// samples or the supervised classifier.
std::string guessClusterLabel(const cv::Mat& centers, int row, const SpectralIndices::BandSet& bd) {
    auto v = [&](int idx) -> float { return idx >= 0 ? centers.at<float>(row, idx) : 0.0f; };

    bool haveNdwi = bd.green >= 0 && bd.nir >= 0;
    bool haveNdvi = bd.nir >= 0 && bd.red >= 0;
    bool haveNdbi = bd.swir1 >= 0 && bd.nir >= 0;
    bool haveBsi  = bd.swir1 >= 0 && bd.red >= 0 && bd.nir >= 0 && bd.blue >= 0;

    // Use un-normalised centres (de-normalised before this call) for meaningful thresholds.
    // Test in the same priority order as LandCoverMapper so labels are consistent.
    // Added absolute-value guard: skip index tests if both bands are near zero
    // (the de-normalised centre for a noise cluster can have tiny values that
    // produce misleading ratios).
    float nirVal  = haveNdvi ? v(bd.nir)  : 0.0f;
    float greenVal= haveNdwi ? v(bd.green): 0.0f;

    // Water: strongly negative NIR AND positive NDWI
    if (haveNdwi && nirVal < 0.08f && safeNormDiff(v(bd.green), v(bd.nir)) > 0.15f)
        return "Water / River";
    // Forest: high NIR plateau, moderate-high NDVI
    if (haveNdvi && nirVal > 0.15f && safeNormDiff(v(bd.nir), v(bd.red)) > 0.35f)
        return "Forest";
    // Built-up: moderate NIR, positive NDBI (SWIR > NIR)
    if (haveNdbi && nirVal > 0.05f && safeNormDiff(v(bd.swir1), v(bd.nir)) > 0.05f)
        return "Built-up";
    // Vegetation: low-moderate NDVI
    if (haveNdvi && nirVal > 0.10f && safeNormDiff(v(bd.nir), v(bd.red)) > 0.15f)
        return "Vegetation";
    // Bare Soil: BSI positive
    if (haveBsi) {
        float s = v(bd.swir1), r = v(bd.red), n = v(bd.nir), b = v(bd.blue);
        if (s + r > 0.05f && safeNormDiff(s + r, n + b) > 0.05f) return "Bare Soil";
    }
    // Low-reflectance cluster — likely shadow or transition zone
    if (nirVal < 0.05f && greenVal < 0.05f) return "Shadow / Low-reflectance";
    return "Mixed / Unclear";
}

} // namespace

RasterCube LulcClassifier::unsupervisedKMeans(const RasterCube& cube, int k, int attempts,
                                               std::map<int, std::string>* clusterLabelsOut) {
    if (k < 2) throw HsiError("LulcClassifier::unsupervisedKMeans: k must be >= 2.");

    // Step 1: identify valid (non-background) pixels.
    // Background/NoData = all bands near zero (black border from the diagonal Hyperion swath).
    // Including these in k-means creates a spurious "all-zero" cluster that dominates the
    // result and gets mislabeled as "likely Water" (NDWI=0/0=0, which is > -∞).
    const float kBackgroundThreshold = 1e-6f;  // reflectance < 1e-6 = no signal
    std::vector<int> validPixelIndices;
    validPixelIndices.reserve(cube.pixelCount());
    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            bool isBackground = true;
            for (int b = 0; b < std::min(cube.bands, 10); ++b) {  // check first 10 bands
                if (std::abs(cube.at(b, row, col)) > kBackgroundThreshold) { isBackground = false; break; }
            }
            if (!isBackground) validPixelIndices.push_back(row * cube.width + col);
        }
    }

    if (validPixelIndices.empty())
        throw HsiError("LulcClassifier::unsupervisedKMeans: no valid (non-background) pixels found in the cube.");

    // Step 2: build sample matrix from valid pixels only and normalise (0-mean, unit-variance per band).
    // Un-normalised hyperspectral data gives SWIR bands (reflectance ~0.3) far more weight
    // than VNIR bands (~0.03) — normalisation ensures all bands contribute equally.
    const int nValid = static_cast<int>(validPixelIndices.size());
    cv::Mat samples(nValid, cube.bands, CV_32F);
    for (int i = 0; i < nValid; ++i) {
        int pixIdx = validPixelIndices[i];
        int row = pixIdx / cube.width, col = pixIdx % cube.width;
        for (int b = 0; b < cube.bands; ++b)
            samples.at<float>(i, b) = cube.at(b, row, col);
    }
    // Per-band z-score normalisation. NOTE: cv::meanStdDev(samples, ...) on a
    // single-channel Mat returns ONE overall scalar mean/stddev across every
    // element -- it does NOT give per-column (per-band) statistics the way
    // this code needs. Looping b up to cube.bands-1 into a 1-element Mat
    // read out of bounds and crashed OpenCV's internal assertion in Mat::at.
    // Compute mean/stddev one column (band) at a time instead.
    cv::Mat mean(1, cube.bands, CV_64F), stddev(1, cube.bands, CV_64F);
    for (int b = 0; b < cube.bands; ++b) {
        cv::Scalar m, s;
        cv::meanStdDev(samples.col(b), m, s);
        mean.at<double>(0, b)   = m[0];
        stddev.at<double>(0, b) = s[0];
    }
    for (int b = 0; b < cube.bands; ++b) {
        float sd = static_cast<float>(stddev.at<double>(0, b));
        if (sd < 1e-9f) sd = 1.0f;  // avoid division by zero on constant bands
        for (int i = 0; i < nValid; ++i)
            samples.at<float>(i, b) = (samples.at<float>(i, b) - static_cast<float>(mean.at<double>(0, b))) / sd;
    }

    cv::Mat labels, centers;
    cv::kmeans(samples, k, labels,
               cv::TermCriteria(cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS, 200, 0.1),
               attempts, cv::KMEANS_PP_CENTERS, centers);

    // Step 3: build output raster. Background pixels = 0 (Unclassified/NoData), valid = 1..k
    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform  = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;
    out.hasNoData     = true;
    out.noDataValue   = 0.0f;
    out.bandNames     = { "lulc_unsupervised" };
    // Default all to 0 (background/NoData) — allocate fills with 0.0f already
    for (int i = 0; i < nValid; ++i) {
        int pixIdx = validPixelIndices[i];
        out.data[static_cast<size_t>(pixIdx)] =
            static_cast<float>(labels.at<int>(i, 0) + 1);  // 1-indexed classes
    }

    // De-normalise cluster centres back to real reflectance for label guessing
    for (int c = 0; c < k; ++c)
        for (int b = 0; b < cube.bands; ++b)
            centers.at<float>(c, b) = centers.at<float>(c, b) * static_cast<float>(stddev.at<double>(0, b))
                                    + static_cast<float>(mean.at<double>(0, b));

    if (clusterLabelsOut) {
        clusterLabelsOut->clear();
        if (!cube.bandNumbers.empty()) {
            SpectralIndices::BandSet bd = HyperionBandFinder::autoDetect(cube);
            for (int c = 0; c < k; ++c)
                (*clusterLabelsOut)[c + 1] = guessClusterLabel(centers, c, bd);
        } else {
            // No sensor band numbers to auto-detect NIR/SWIR/etc from
            // (e.g. a cube loaded without that metadata) -- can't guess.
            for (int c = 0; c < k; ++c) (*clusterLabelsOut)[c + 1] = "(no band metadata to guess from)";
        }
    }

    Logger::log("LulcClassifier", "Unsupervised k-means LULC complete, k=" + std::to_string(k) + ".");
    if (clusterLabelsOut) {
        std::string summary;
        for (auto& [id, label] : *clusterLabelsOut) summary += std::to_string(id) + "=" + label + "  ";
        Logger::log("LulcClassifier", "Cluster guesses: " + summary);
    }
    return out;
}

LulcClassifier::SupervisedResult LulcClassifier::supervised(const RasterCube& cube,
                                                              const std::vector<std::vector<float>>& trainFeatures,
                                                              const std::vector<int>& trainLabels,
                                                              const SvmModel::Options& svmOptions) {
    SupervisedResult result;
    result.model.train(trainFeatures, trainLabels, svmOptions);
    result.classified = result.model.classifyCube(cube);
    result.classified.bandNames = { "lulc_supervised" };

    Logger::log("LulcClassifier", "Supervised SVM LULC complete (" +
                std::to_string(trainFeatures.size()) + " training samples).");
    return result;
}

} // namespace hsi
