#include "hsi/SvmModel.h"
#include <set>
#include "hsi/Logger.h"

#include <opencv2/core.hpp>
#include <opencv2/ml.hpp>

namespace hsi {

SvmModel::SvmModel() : svm_(cv::ml::SVM::create()) {}
SvmModel::~SvmModel() = default;

void SvmModel::train(const std::vector<std::vector<float>>& features,
                      const std::vector<int>& labels) {
    train(features, labels, Options());
}

void SvmModel::train(const std::vector<std::vector<float>>& features,
                      const std::vector<int>& labels,
                      const Options& opt) {
    if (features.empty() || features.size() != labels.size()) {
        throw HsiError(
            "SvmModel::train: " +
            std::string(features.empty() ? "empty training set — no valid sample pixels found. "
                "Check that your CSV coordinates fall within the scene bounds and not on the black border." :
                "feature/label count mismatch."));
    }
    int nSamples = static_cast<int>(features.size());
    int nFeatures = static_cast<int>(features[0].size());

    cv::Mat trainData(nSamples, nFeatures, CV_32F);
    cv::Mat labelMat(nSamples, 1, CV_32S);
    for (int i = 0; i < nSamples; ++i) {
        if (static_cast<int>(features[i].size()) != nFeatures) {
            throw HsiError("SvmModel::train: ragged feature vectors (sample " + std::to_string(i) + ").");
        }
        for (int j = 0; j < nFeatures; ++j) trainData.at<float>(i, j) = features[i][j];
        labelMat.at<int>(i, 0) = labels[i];
    }

    svm_->setType(cv::ml::SVM::C_SVC);
    svm_->setKernel(opt.kernelType == 0 ? cv::ml::SVM::LINEAR : cv::ml::SVM::RBF);
    svm_->setC(opt.C);
    svm_->setGamma(opt.gamma);
    svm_->setTermCriteria(cv::TermCriteria(cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS,
                                            opt.maxIterations, opt.epsilon));

    // BUG26: Per-band z-score normalisation. Without this, SWIR bands (~0.3)
    // dominate VNIR (~0.03) by 10×, severely degrading SVM accuracy.
    scaleMean_  = std::vector<float>(nFeatures, 0.0f);
    scaleStd_   = std::vector<float>(nFeatures, 1.0f);
    for (int j = 0; j < nFeatures; ++j) {
        double sum = 0, sum2 = 0;
        for (int i = 0; i < nSamples; ++i) { double v = trainData.at<float>(i,j); sum += v; sum2 += v*v; }
        double mean = sum / nSamples;
        double sd   = std::sqrt(std::max(0.0, sum2/nSamples - mean*mean));
        scaleMean_[j] = static_cast<float>(mean);
        scaleStd_[j]  = static_cast<float>(sd < 1e-9 ? 1.0 : sd);
    }
    for (int i = 0; i < nSamples; ++i)
        for (int j = 0; j < nFeatures; ++j)
            trainData.at<float>(i,j) = (trainData.at<float>(i,j) - scaleMean_[j]) / scaleStd_[j];

    // OpenCV SVM requires at least 2 distinct class labels. If all samples are
    // the same class (e.g. all OOB samples of one class got filtered, leaving
    // only one class), training will silently fail or crash.
    {
        std::set<int> uniqueLabels(labels.begin(), labels.end());
        if (uniqueLabels.size() < 2) {
            throw HsiError(
                "SvmModel::train: only one distinct class label found (" +
                std::to_string(*uniqueLabels.begin()) + "). "
                "SVM needs at least 2 classes. Check that your CSV has both "
                "'built_up' AND 'non_built_up' rows with valid coordinates "
                "inside the scene (" + std::to_string(nSamples) + " samples passed bounds check).");
        }
    }

    cv::Ptr<cv::ml::TrainData> td = cv::ml::TrainData::create(trainData, cv::ml::ROW_SAMPLE, labelMat);
    if (!svm_->train(td)) {
        throw HsiError("SvmModel::train: OpenCV SVM training failed to converge. "
                       "Try increasing max iterations or check for degenerate training data.");
    }
    Logger::log("SvmModel", "Trained on " + std::to_string(nSamples) + " samples, " +
                std::to_string(nFeatures) + " features (band-normalised).");
}

int SvmModel::predict(const std::vector<float>& feature) const {
    int n = static_cast<int>(feature.size());
    cv::Mat sample(1, n, CV_32F);
    const bool hasScale = !scaleMean_.empty() && static_cast<int>(scaleMean_.size()) >= n;
    for (int j = 0; j < n; ++j) {
        float v = feature[j];
        if (hasScale) v = (v - scaleMean_[j]) / scaleStd_[j];
        sample.at<float>(0, j) = v;
    }
    return static_cast<int>(svm_->predict(sample));
}

RasterCube SvmModel::classifyCube(const RasterCube& cube) const {
    if (!isTrained()) throw HsiError("SvmModel::classifyCube: model is not trained/loaded.");

    // OpenCV asserts samples.cols == var_count when predict() is called.
    // This fires when the SVM was trained on a different-sized feature vector
    // than the cube passed here (e.g. trained on 198-band reflectance, called
    // on a 199- or 207-band fused stack, or vice versa).
    // Resolution: use only the first min(trainBands, cube.bands) features,
    // matching what the model was trained on.  This is safe because the first
    // N bands of the stack are always the Hyperion reflectance bands in order,
    // so we are not mixing in random extra bands.
    int trainBands = svm_->getVarCount();
    if (trainBands <= 0) throw HsiError("SvmModel::classifyCube: cannot determine training band count from model.");
    int useBands = std::min(trainBands, cube.bands);
    if (useBands < trainBands) {
        Logger::log("SvmModel", "WARNING: cube has " + std::to_string(cube.bands) +
                    " bands but SVM was trained on " + std::to_string(trainBands) +
                    " — classifying using only the first " + std::to_string(useBands) + " bands.");
    }

    RasterCube out;
    out.allocate(cube.width, cube.height, 1);
    out.geoTransform = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;
    out.bandNames = { "svm_class" };

    cv::Mat batch(cube.width, trainBands, CV_32F, cv::Scalar(0.f));
    const bool hasScale = !scaleMean_.empty() && static_cast<int>(scaleMean_.size()) >= useBands;
    for (int row = 0; row < cube.height; ++row) {
        for (int col = 0; col < cube.width; ++col)
            for (int b = 0; b < useBands; ++b) {
                float v = cube.at(b, row, col);
                if (hasScale)
                    v = (v - scaleMean_[b]) / scaleStd_[b];
                batch.at<float>(col, b) = v;
            }

        cv::Mat results;
        svm_->predict(batch, results);
        for (int col = 0; col < cube.width; ++col) out.at(0, row, col) = results.at<float>(col, 0);
    }
    return out;
}

void SvmModel::save(const std::string& path) const {
    if (!isTrained()) throw HsiError("SvmModel::save: model is not trained.");
    svm_->save(path);
    Logger::log("SvmModel", "Saved model to '" + path + "'.");
}

void SvmModel::load(const std::string& path) {
    svm_ = cv::ml::SVM::load(path);
    if (!svm_ || !svm_->isTrained()) throw HsiError("SvmModel::load: failed to load a trained model from '" + path + "'.");
    Logger::log("SvmModel", "Loaded model from '" + path + "'.");
}

bool SvmModel::isTrained() const { return svm_ && svm_->isTrained(); }

} // namespace hsi
