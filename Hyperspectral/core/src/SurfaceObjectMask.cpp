#include "hsi/SurfaceObjectMask.h"
#include "hsi/Logger.h"

#include <opencv2/core.hpp>
#include <limits>
#include <algorithm>

namespace hsi {

// Otsu's method: given a histogram of `bins` counts spanning [lo, hi],
// returns the bin-boundary value that maximizes between-class variance.
// Standard, unremarkable implementation -- O(bins), not O(pixels²).
static double otsuThresholdFromHistogram(const std::vector<long>& hist, double lo, double hi) {
    int bins = static_cast<int>(hist.size());
    long total = 0;
    double sumAll = 0.0;
    for (int i = 0; i < bins; ++i) {
        total += hist[i];
        double binCenter = lo + (hi - lo) * (i + 0.5) / bins;
        sumAll += binCenter * hist[i];
    }
    if (total == 0) return (lo + hi) / 2.0;

    long wB = 0;
    double sumB = 0.0;
    double bestVar = -1.0;
    int bestBin = bins / 2;
    for (int i = 0; i < bins; ++i) {
        wB += hist[i];
        if (wB == 0) continue;
        long wF = total - wB;
        if (wF == 0) break;
        double binCenter = lo + (hi - lo) * (i + 0.5) / bins;
        sumB += binCenter * hist[i];
        double mB = sumB / wB;
        double mF = (sumAll - sumB) / wF;
        double between = static_cast<double>(wB) * static_cast<double>(wF) * (mB - mF) * (mB - mF);
        if (between > bestVar) { bestVar = between; bestBin = i; }
    }
    return lo + (hi - lo) * (bestBin + 1.0) / bins;
}

static RasterCube computeThresholdMask(const RasterCube& cube, const SurfaceObjectMask::ThresholdParams& p) {
    if (p.nirBandIndex < 0 || p.nirBandIndex >= cube.bands ||
        p.swirBandIndex < 0 || p.swirBandIndex >= cube.bands) {
        throw HsiError("SurfaceObjectMask: threshold method requires valid nirBandIndex/swirBandIndex.");
    }

    // Compute the NDBI-style index once per pixel and keep it around --
    // needed both for the auto-threshold histogram pass and the final
    // per-pixel comparison, so we don't recompute (swir-nir)/(swir+nir)
    // three times over the whole scene like the diagnostic build did.
    std::vector<float> index(cube.pixelCount());
    float idxMin = std::numeric_limits<float>::max();
    float idxMax = std::numeric_limits<float>::lowest();
    double idxSum = 0.0;
    long idxN = 0;
    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            double nir = cube.at(p.nirBandIndex, row, col);
            double swir = cube.at(p.swirBandIndex, row, col);
            double denom = swir + nir;
            float v = std::abs(denom) < 1e-9 ? 0.0f : static_cast<float>((swir - nir) / denom);
            index[static_cast<size_t>(row) * cube.width + col] = v;
            if (denom != 0.0) {
                idxMin = std::min(idxMin, v);
                idxMax = std::max(idxMax, v);
                idxSum += v;
                ++idxN;
            }
        }
    }
    if (idxN == 0) { idxMin = idxMax = 0.0f; }
    if (idxMax <= idxMin) idxMax = idxMin + 1e-6f; // avoid a zero-width histogram range

    double threshold = p.thresholdValue;
    bool isAuto = (p.thresholdValue == SurfaceObjectMask::kAutoThreshold);
    if (isAuto) {
        // Build histogram over REAL data pixels only — exclude the black border
        // (NIR=SWIR=0 → denom=0) which has NDBI=0 by construction.  Including
        // those ~50% of the raster extent that are off-scene biases Otsu to
        // split "border" vs "everything else" and flags the entire scene as
        // "object" (77%+ of pixels).  We detect border pixels by the same
        // `denom != 0` condition already used for the stats pass above.
        const int kBins = 256;
        std::vector<long> hist(kBins, 0);
        for (int row = 0; row < cube.height; ++row) {
            for (int col = 0; col < cube.width; ++col) {
                double nir  = cube.at(p.nirBandIndex,  row, col);
                double swir = cube.at(p.swirBandIndex, row, col);
                if (std::abs(nir + swir) < 1e-9) continue;  // skip border/no-data
                float v = index[static_cast<size_t>(row) * cube.width + col];
                int bin = static_cast<int>((v - idxMin) / (idxMax - idxMin) * (kBins - 1));
                bin = std::max(0, std::min(kBins - 1, bin));
                ++hist[bin];
            }
        }
        threshold = otsuThresholdFromHistogram(hist, idxMin, idxMax);
    }
    if (p.resolvedThresholdOut) *p.resolvedThresholdOut = threshold;

    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;
    out.bandNames = { "surface_object_mask" };

    long objectCount = 0;
    for (size_t i = 0; i < index.size(); ++i) {
        bool isObject = index[i] > threshold;
        out.data[i] = isObject ? 1.0f : 0.0f;
        if (isObject) ++objectCount;
    }

    Logger::log("SurfaceObjectMask",
        "Threshold mask: NDBI range [" + std::to_string(idxMin) + ", " + std::to_string(idxMax) +
        "] mean=" + std::to_string(idxN > 0 ? idxSum/idxN : 0.0) +
        " threshold=" + std::to_string(threshold) + (isAuto ? " (auto/Otsu)" : " (manual)") +
        " -> " + std::to_string(objectCount) + " object pixels. " +
        (objectCount == 0 ? "0 objects: try Auto (Otsu), or lower the threshold below the mean." : ""));
    return out;
}

static RasterCube computeKMeansMask(const RasterCube& cube, const SurfaceObjectMask::KMeansParams& p) {
    std::vector<int> bands = p.bandIndices;
    if (bands.empty()) {
        // Use a small representative band subset rather than all 198 bands.
        // All-band k-means on a 2.5M-pixel scene = ~2GB sample matrix; it
        // either hangs or OOMs. A 5-band selection spanning Blue/Green/Red/
        // NIR/SWIR captures most of the spectral variance needed to separate
        // surface vs built-up/bare classes.
        // Hyperion 0-based indices in the merged 198-band stack:
        //   idx 3  ≈ 467nm  (Blue)
        //   idx 12 ≈ 549nm  (Green)
        //   idx 20 ≈ 660nm  (Red)
        //   idx 22 ≈ 901nm  (NIR)
        //   idx 63 ≈ 1093nm (SWIR-1)
        for (int idx : {3, 12, 20, 22, 63})
            if (idx < cube.bands) bands.push_back(idx);
        if (bands.empty()) { bands = {0}; } // absolute fallback
    }

    int nFeatures = static_cast<int>(bands.size());
    long nSamples = static_cast<long>(cube.pixelCount());
    cv::Mat samples(static_cast<int>(nSamples), nFeatures, CV_32F);

    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            int sampleIdx = row * cube.width + col;
            for (int j = 0; j < nFeatures; ++j) samples.at<float>(sampleIdx, j) = cube.at(bands[j], row, col);
        }
    }

    cv::Mat labels, centers;
    cv::kmeans(samples, 2, labels,
               cv::TermCriteria(cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS, 50, 0.5),
               p.attempts, cv::KMEANS_PP_CENTERS, centers);

    // Object cluster selection: built-up / bare structures have a characteristic
    // spectral shape — higher SWIR relative to NIR (positive NDBI), lower NIR
    // than vegetation.  Using overall brightness ("brighter = object") fails
    // badly for forest-dominated Hyperion scenes where the NIR plateau makes
    // vegetation the brightest class by far.
    //
    // Feature-space layout (5-band subset: B=col0, G=col1, R=col2, NIR=col3, SWIR=col4):
    //   NDBI proxy = SWIR_mean - NIR_mean  (more positive → more built-up/bare)
    // The cluster with the higher (SWIR - NIR) is taken as "object".
    // Falls back to lower-NIR cluster when only < 5 bands are available.
    int objectCluster = 0;
    if (centers.cols >= 5) {
        // NIR = col 3, SWIR = col 4
        double ndbi0 = centers.at<float>(0, 4) - centers.at<float>(0, 3);
        double ndbi1 = centers.at<float>(1, 4) - centers.at<float>(1, 3);
        objectCluster = (ndbi0 > ndbi1) ? 0 : 1;
    } else {
        // Fewer bands: use lower NIR (col closest to NIR) as object proxy
        int nirCol = std::min(3, centers.cols - 1);
        objectCluster = (centers.at<float>(0, nirCol) < centers.at<float>(1, nirCol)) ? 0 : 1;
    }
    Logger::log("SurfaceObjectMask",
        "K-means clusters: C0_NIR=" + std::to_string(centers.cols > 3 ? centers.at<float>(0,3) : 0) +
        " C0_SWIR=" + std::to_string(centers.cols > 4 ? centers.at<float>(0,4) : 0) +
        " | C1_NIR=" + std::to_string(centers.cols > 3 ? centers.at<float>(1,3) : 0) +
        " C1_SWIR=" + std::to_string(centers.cols > 4 ? centers.at<float>(1,4) : 0) +
        " → object=cluster" + std::to_string(objectCluster));

    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;
    out.bandNames = { "surface_object_mask" };

    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col) {
            int sampleIdx = row * cube.width + col;
            int cluster = labels.at<int>(sampleIdx, 0);
            out.at(0, row, col) = (cluster == objectCluster) ? 1.0f : 0.0f;
        }
    }
    Logger::log("SurfaceObjectMask", "K-means (k=2) mask computed over " + std::to_string(nFeatures) + " bands.");
    return out;
}

static RasterCube computeSvmMask(const RasterCube& cube, const SvmModel* model) {
    if (!model) throw HsiError("SurfaceObjectMask: TrainedSvm method requires a trained SvmModel.");
    RasterCube out = model->classifyCube(cube);
    out.bandNames = { "surface_object_mask" };
    Logger::log("SurfaceObjectMask", "Trained-SVM mask computed.");
    return out;
}

RasterCube SurfaceObjectMask::computeMask(const RasterCube& cube, const Options& opt) {
    switch (opt.method) {
        case MaskMethod::Threshold:          return computeThresholdMask(cube, opt.threshold);
        case MaskMethod::KMeansUnsupervised: return computeKMeansMask(cube, opt.kmeans);
        case MaskMethod::TrainedSvm:         return computeSvmMask(cube, opt.trainedSvm);
    }
    throw HsiError("SurfaceObjectMask: unknown method.");
}

ValidationResult SurfaceObjectMask::validateAgainstReference(const RasterCube& predictedMask,
                                                               const RasterCube& referenceMask) {
    if (!predictedMask.sameGridAs(referenceMask)) {
        throw HsiError("SurfaceObjectMask::validateAgainstReference: predicted and reference masks "
                        "are not on the same grid -- resample one to match the other first.");
    }

    ValidationResult result;
    for (size_t i = 0; i < predictedMask.pixelCount(); ++i) {
        bool pred = predictedMask.data[i] > 0.5f;
        bool ref = referenceMask.data[i] > 0.5f;
        if (pred && ref) ++result.truePositive;
        else if (!pred && !ref) ++result.trueNegative;
        else if (pred && !ref) ++result.falsePositive;
        else ++result.falseNegative;
    }

    Logger::log("SurfaceObjectMask", "Validation vs reference: agreement=" +
                std::to_string(result.overallAgreement() * 100.0) + "%, IoU=" +
                std::to_string(result.iou()));
    return result;
}

} // namespace hsi
