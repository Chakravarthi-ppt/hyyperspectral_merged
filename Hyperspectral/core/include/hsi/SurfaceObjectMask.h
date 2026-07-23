#pragma once
#include "hsi/Types.h"
#include "hsi/SvmModel.h"

namespace hsi {

enum class MaskMethod { Threshold, KMeansUnsupervised, TrainedSvm };

// Produces a single-band raster where 0 = bare surface, 1 = object/structure.
class SurfaceObjectMask {
public:
    // Pass as ThresholdParams::thresholdValue to have computeMask() pick the
    // split point itself (Otsu's method on the scene's own NDBI-style index
    // histogram) instead of using a fixed value. Useful because a threshold
    // of 0.0 only makes sense for scenes with roughly balanced object/surface
    // reflectance -- a heavily vegetated scene can have NDBI negative at
    // every single pixel, in which case ANY fixed threshold >= 0 finds
    // (near-)zero objects no matter which NIR/SWIR bands are chosen.
    static constexpr double kAutoThreshold = -1000.0;

    struct ThresholdParams {
        int nirBandIndex = -1;   // 0-based band index within the cube
        int swirBandIndex = -1;  // 0-based band index within the cube
        // index = (swir - nir) / (swir + nir); pixel is "object" if index > threshold.
        // Set to kAutoThreshold to have it picked automatically (Otsu).
        double thresholdValue = 0.0;
        // Optional: if non-null, computeMask() writes the threshold actually
        // used here (handy for displaying what "auto" resolved to).
        double* resolvedThresholdOut = nullptr;
    };

    struct KMeansParams {
        std::vector<int> bandIndices; // 0-based bands/components used as feature space; empty = all bands
        int attempts = 3;
    };

    struct Options {
        MaskMethod method = MaskMethod::Threshold;
        ThresholdParams threshold;
        KMeansParams kmeans;
        const SvmModel* trainedSvm = nullptr; // required if method == TrainedSvm
    };

    static RasterCube computeMask(const RasterCube& cube, const Options& opt);

    // Compares a predicted 0/1 mask against an independently derived
    // reference mask (e.g. built from EOS-04 SAR backscatter thresholding,
    // or from an optical NDVI/NDBI rule) on the same grid.
    static ValidationResult validateAgainstReference(const RasterCube& predictedMask, const RasterCube& referenceMask);
};

} // namespace hsi
