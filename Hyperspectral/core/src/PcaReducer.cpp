#include "hsi/PcaReducer.h"
#include "hsi/Logger.h"

#include <Eigen/Dense>
#include <algorithm>

namespace hsi {

PcaReducer::Result PcaReducer::reduce(const RasterCube& cube, int startBandIndex, int endBandIndex, int numComponents) {
    if (startBandIndex < 0 || endBandIndex >= cube.bands || startBandIndex > endBandIndex) {
        throw HsiError("PcaReducer: invalid band range [" + std::to_string(startBandIndex) + ", " +
                        std::to_string(endBandIndex) + "] for a cube with " + std::to_string(cube.bands) + " bands.");
    }
    int nBands = endBandIndex - startBandIndex + 1;
    if (numComponents < 1 || numComponents > nBands) {
        throw HsiError("PcaReducer: numComponents must be between 1 and " + std::to_string(nBands) + ".");
    }

    long nPixels = static_cast<long>(cube.pixelCount());

    // Build pixels x bands matrix.
    Eigen::MatrixXd X(nPixels, nBands);
    for (int b = 0; b < nBands; ++b) {
        int srcBand = startBandIndex + b;
        size_t base = static_cast<size_t>(srcBand) * cube.width * cube.height;
        for (long i = 0; i < nPixels; ++i) X(i, b) = static_cast<double>(cube.data[base + i]);
    }

    // Mean-center each band.
    Eigen::RowVectorXd mean = X.colwise().mean();
    X.rowwise() -= mean;

    // Covariance matrix (bands x bands).
    Eigen::MatrixXd cov = (X.transpose() * X) / static_cast<double>(nPixels - 1);

    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(cov);
    if (solver.info() != Eigen::Success) {
        throw HsiError("PcaReducer: eigen decomposition of the band covariance matrix failed.");
    }

    // Eigen returns ascending eigenvalues; we want descending (most variance first).
    Eigen::VectorXd eigenvalues = solver.eigenvalues().reverse();
    Eigen::MatrixXd eigenvectors = solver.eigenvectors().rowwise().reverse();

    double totalVariance = eigenvalues.sum();

    Eigen::MatrixXd loadings = eigenvectors.leftCols(numComponents); // bands x numComponents
    Eigen::MatrixXd scores = X * loadings;                            // pixels x numComponents

    Result result;
    result.components.allocate(cube.width, cube.height, numComponents);
    result.components.geoTransform = cube.geoTransform;
    result.components.projectionWkt = cube.projectionWkt;
    for (int c = 0; c < numComponents; ++c) {
        result.components.bandNames[c] = "pca_" + std::to_string(c + 1);
        size_t base = static_cast<size_t>(c) * cube.width * cube.height;
        for (long i = 0; i < nPixels; ++i) result.components.data[base + i] = static_cast<float>(scores(i, c));
        result.explainedVarianceRatio.push_back(totalVariance > 0 ? eigenvalues(c) / totalVariance : 0.0);
    }

    Logger::log("PcaReducer", "PCA over bands [" + std::to_string(startBandIndex) + ", " +
                std::to_string(endBandIndex) + "] -> " + std::to_string(numComponents) +
                " component(s), PC1 explains " +
                std::to_string(result.explainedVarianceRatio.empty() ? 0.0 : result.explainedVarianceRatio[0] * 100.0) +
                "% of variance.");
    return result;
}

} // namespace hsi
