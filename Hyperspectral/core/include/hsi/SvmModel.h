#pragma once
#include "hsi/Types.h"
#include <memory>

// Forward-declare the OpenCV type so this header stays light for callers
// that only need to pass an SvmModel* around without touching OpenCV.
namespace cv { namespace ml { class SVM; } }

namespace hsi {

// Thin, reusable wrapper around cv::ml::SVM (RBF kernel, C-SVC by default).
// Used wherever the pipeline needs a trained pixel/feature classifier:
// surface-vs-object masking, built-up refinement after SAM, supervised LULC.
class SvmModel {
public:
    SvmModel();
    ~SvmModel();

    struct Options {
        double C = 10.0;
        double gamma = 0.5;
        int kernelType = 1; // 0=linear, 1=RBF (default)
        int maxIterations = 1000;
        double epsilon = 1e-6;
    };

    // features[i] is one sample's vector (e.g. a pixel's band values),
    // labels[i] is its integer class id.
    void train(const std::vector<std::vector<float>>& features,
               const std::vector<int>& labels,
               const Options& opt);

    // Convenience overload using default Options (RBF kernel, C=10, gamma=0.5).
    void train(const std::vector<std::vector<float>>& features,
               const std::vector<int>& labels);

    int predict(const std::vector<float>& feature) const;

    // Classifies every pixel of `cube` (its full spectrum as the feature
    // vector) into a single-band integer label raster.
    RasterCube classifyCube(const RasterCube& cube) const;

    void save(const std::string& path) const;
    void load(const std::string& path);

    bool isTrained() const;

private:
    std::shared_ptr<cv::ml::SVM> svm_;
    std::vector<float> scaleMean_;  // per-band normalisation mean (BUG26)
    std::vector<float> scaleStd_;   // per-band normalisation std-dev (BUG26)
};

} // namespace hsi
