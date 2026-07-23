#include "hsi/RxDetector.h"
#include <Eigen/Dense>
#include <Eigen/Cholesky>
#include <Eigen/SVD>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

namespace hsi {

// ---------------------------------------------------------------------------
// Chi-squared inverse CDF — Wilson-Hilferty approximation (accurate for df≥2)
// ---------------------------------------------------------------------------
static double chiSqThreshold(int df, double p) {
    auto normInv = [](double q) -> double {
        const double a[] = {2.515517, 0.802853, 0.010328};
        const double b[] = {1.432788, 0.189269, 0.001308};
        double t = std::sqrt(-2.0 * std::log(q < 0.5 ? q : 1.0 - q));
        double z = t - (((a[2]*t + a[1])*t + a[0]) /
                        (((b[2]*t + b[1])*t + b[0])*t + 1.0));
        return q < 0.5 ? -z : z;
    };
    double z  = normInv(p);
    double mu = 1.0 - 2.0 / (9.0 * df);
    double sg = std::sqrt(2.0 / (9.0 * df));
    double c  = mu + z * sg;
    return df * c * c * c;
}

static RasterCube makeOneBand(const RasterCube& src, const std::string& name) {
    RasterCube c;
    c.allocate(src.width, src.height, 1);
    c.geoTransform  = src.geoTransform;
    c.projectionWkt = src.projectionWkt;
    c.bandNames[0]  = name;
    c.bandNumbers[0]= 1;
    return c;
}

// ---------------------------------------------------------------------------
// PCA whitening: project `cube` down to `nComponents` whitened PCs.
// Returns a RasterCube with nComponents bands where each band has unit variance.
// This is the key step that makes Local RX tractable on 198-band data:
// reduce to ~10 components first, then the per-pixel covariance inversion
// is 10×10 instead of 198×198 — ~400× fewer operations.
// ---------------------------------------------------------------------------
static RasterCube pcaWhiten(const RasterCube& cube, int nComponents,
                             const std::function<void(double)>& onProgress) {
    const int B = cube.bands;
    const int N = cube.width * cube.height;
    nComponents = std::min(nComponents, B);

    // 1. Mean-center
    Eigen::VectorXd mean = Eigen::VectorXd::Zero(B);
    for (int b = 0; b < B; ++b)
        for (int i = 0; i < N; ++i)
            mean(b) += cube.data[static_cast<size_t>(b)*N + i];
    mean /= N;

    if (onProgress) onProgress(0.05);

    // 2. Covariance (one pass, outer-product accumulation)
    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(B, B);
    Eigen::VectorXd x(B);
    for (int i = 0; i < N; ++i) {
        for (int b = 0; b < B; ++b)
            x(b) = cube.data[static_cast<size_t>(b)*N + i] - mean(b);
        cov.selfadjointView<Eigen::Upper>().rankUpdate(x);
    }
    cov /= (N - 1);

    if (onProgress) onProgress(0.20);

    // 3. Eigen-decomposition (only upper triangle stored, symmetric solver)
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(cov);
    if (eig.info() != Eigen::Success)
        throw HsiError("RxDetector PCA: eigendecomposition failed");

    // Eigenvalues in ascending order — take the last nComponents (largest)
    int total = eig.eigenvalues().size();
    Eigen::MatrixXd W(B, nComponents);          // projection matrix
    Eigen::VectorXd invStd(nComponents);         // 1/sqrt(eigenvalue) for whitening
    for (int k = 0; k < nComponents; ++k) {
        int idx = total - nComponents + k;
        double ev = eig.eigenvalues()(idx);
        W.col(k)      = eig.eigenvectors().col(idx);
        invStd(k)     = (ev > 1e-10) ? 1.0 / std::sqrt(ev) : 0.0;
    }

    if (onProgress) onProgress(0.30);

    // 4. Project + whiten every pixel
    RasterCube out;
    out.allocate(cube.width, cube.height, nComponents);
    out.geoTransform  = cube.geoTransform;
    out.projectionWkt = cube.projectionWkt;

    Eigen::VectorXd proj(nComponents);
    for (int i = 0; i < N; ++i) {
        for (int b = 0; b < B; ++b)
            x(b) = cube.data[static_cast<size_t>(b)*N + i] - mean(b);
        proj = W.transpose() * x;
        for (int k = 0; k < nComponents; ++k)
            out.data[static_cast<size_t>(k)*N + i] =
                static_cast<float>(proj(k) * invStd(k));
    }

    if (onProgress) onProgress(0.40);
    return out;
}

// ---------------------------------------------------------------------------
// Global RX — single-pass, multithreaded scoring
// ---------------------------------------------------------------------------
RxResult RxDetector::detectGlobal(const RasterCube& cube, const RxOptions& opt) {
    const int B = cube.bands;
    const int N = cube.width * cube.height;
    if (B < 2) throw HsiError("RxDetector: need at least 2 bands");
    if (N < B + 1) throw HsiError("RxDetector: too few pixels to estimate covariance");

    // 1. Mean
    Eigen::VectorXd mean = Eigen::VectorXd::Zero(B);
    for (int b = 0; b < B; ++b)
        for (int i = 0; i < N; ++i)
            mean(b) += cube.data[static_cast<size_t>(b)*N + i];
    mean /= N;
    if (opt.onProgress) opt.onProgress(0.10);

    // 2. Covariance (symmetric update)
    Eigen::MatrixXd cov = Eigen::MatrixXd::Zero(B, B);
    Eigen::VectorXd x(B);
    for (int i = 0; i < N; ++i) {
        for (int b = 0; b < B; ++b)
            x(b) = cube.data[static_cast<size_t>(b)*N + i] - mean(b);
        cov.selfadjointView<Eigen::Upper>().rankUpdate(x);
    }
    cov.triangularView<Eigen::Lower>() = cov.transpose();
    cov /= (N - 1);
    if (opt.onProgress) opt.onProgress(0.40);

    // 3. LDLT
    Eigen::LDLT<Eigen::MatrixXd> ldlt(cov);
    if (ldlt.info() != Eigen::Success)
        throw HsiError("RxDetector: covariance is not positive semi-definite");

    double thr = chiSqThreshold(B, 1.0 - opt.pfa);
    RxResult res;
    res.threshold  = thr;
    res.scoreMap   = makeOneBand(cube, "RX_Score");
    res.binaryMask = makeOneBand(cube, "Anomaly");

    if (opt.onProgress) opt.onProgress(0.50);

    // 4. Score pixels — split across hardware threads
    const int nThreads = std::max(1, (int)std::thread::hardware_concurrency());
    std::atomic<int> anomalyCount{0};
    std::atomic<int> doneRows{0};
    const int H = cube.height;
    const int W = cube.width;

    auto scoreBlock = [&](int startRow, int endRow) {
        Eigen::VectorXd xLocal(B);
        for (int row = startRow; row < endRow; ++row) {
            for (int col = 0; col < W; ++col) {
                int i = row * W + col;
                for (int b = 0; b < B; ++b)
                    xLocal(b) = cube.data[static_cast<size_t>(b)*N + i] - mean(b);
                double d2 = xLocal.dot(ldlt.solve(xLocal));
                res.scoreMap.data[i] = static_cast<float>(d2);
                if (d2 > thr) { res.binaryMask.data[i] = 1.0f; ++anomalyCount; }
            }
            ++doneRows;
            if (opt.onProgress)
                opt.onProgress(0.50 + 0.50 * doneRows.load() / H);
        }
    };

    std::vector<std::thread> threads;
    int rowsPerThread = H / nThreads;
    for (int t = 0; t < nThreads; ++t) {
        int r0 = t * rowsPerThread;
        int r1 = (t == nThreads-1) ? H : r0 + rowsPerThread;
        threads.emplace_back(scoreBlock, r0, r1);
    }
    for (auto& th : threads) th.join();

    res.anomalyCount = anomalyCount.load();
    if (opt.onProgress) opt.onProgress(1.0);
    return res;
}

// ---------------------------------------------------------------------------
// Local RX — PCA-whitened then CFAR.
//
// Why PCA-whiten first?
//   198-band local RX needs a 198×198 covariance inversion *per pixel* —
//   O(N × OR² × B²) ≈ billions of operations on a 931×3181 scene.
//   Projecting to nPcComponents (~10) first reduces the inner work to
//   O(N × OR² × k²) — ~400× faster — while preserving anomaly separability
//   in the principal subspace. This is standard practice for hyperspectral RX
//   (Kwon & Nasrabadi 2005, IEEE TGRS).
// ---------------------------------------------------------------------------
RxResult RxDetector::detectLocal(const RasterCube& cube, const RxOptions& opt) {
    const int W  = cube.width;
    const int H  = cube.height;
    if (cube.bands < 2) throw HsiError("RxDetector: need at least 2 bands");
    if (W * H < 2)      throw HsiError("RxDetector: scene too small");

    // Step 1: PCA-whiten to opt.nPcComponents bands
    auto progressWhiten = opt.onProgress
        ? std::function<void(double)>([&](double f){ opt.onProgress(f * 0.45); })
        : std::function<void(double)>(nullptr);

    RasterCube white = pcaWhiten(cube, opt.nPcComponents, progressWhiten);
    const int  k     = white.bands;   // actual components after clamping

    const int OR     = opt.outerR;
    const int IR     = opt.innerR;
    const int minBg  = k + 1;

    double thr = chiSqThreshold(k, 1.0 - opt.pfa);

    RxResult res;
    res.threshold  = thr;
    res.scoreMap   = makeOneBand(cube, "RX_Score");
    res.binaryMask = makeOneBand(cube, "Anomaly");

    if (opt.onProgress) opt.onProgress(0.45);

    // Step 2: Score each pixel against its local annular background,
    //         parallelised row-by-row across hardware threads.
    const int N = W * H;
    std::atomic<int> anomalyCount{0};
    std::atomic<int> doneRows{0};
    const int nThreads = std::max(1, (int)std::thread::hardware_concurrency());

    auto scoreBlock = [&](int startRow, int endRow) {
        Eigen::VectorXd mean(k), xc(k);
        Eigen::MatrixXd cov(k, k);
        Eigen::LDLT<Eigen::MatrixXd> ldlt(k);

        for (int row = startRow; row < endRow; ++row) {
            for (int col = 0; col < W; ++col) {
                mean.setZero(); cov.setZero();
                int n = 0;

                int r0 = std::max(0, row-OR), r1 = std::min(H-1, row+OR);
                int c0 = std::max(0, col-OR), c1 = std::min(W-1, col+OR);

                // Pass 1: mean
                for (int r = r0; r <= r1; ++r)
                    for (int c = c0; c <= c1; ++c) {
                        if (std::abs(r-row) <= IR && std::abs(c-col) <= IR) continue;
                        int idx = r*W + c;
                        for (int ki = 0; ki < k; ++ki)
                            mean(ki) += white.data[static_cast<size_t>(ki)*N + idx];
                        ++n;
                    }

                if (n < minBg) continue;
                mean /= n;

                // Pass 2: covariance
                for (int r = r0; r <= r1; ++r)
                    for (int c = c0; c <= c1; ++c) {
                        if (std::abs(r-row) <= IR && std::abs(c-col) <= IR) continue;
                        int idx = r*W + c;
                        for (int ki = 0; ki < k; ++ki)
                            xc(ki) = white.data[static_cast<size_t>(ki)*N + idx] - mean(ki);
                        cov.selfadjointView<Eigen::Upper>().rankUpdate(xc);
                    }
                cov.triangularView<Eigen::Lower>() = cov.transpose();
                cov /= (n - 1);

                ldlt.compute(cov);
                if (ldlt.info() != Eigen::Success) { mean.setZero(); cov.setZero(); continue; }

                // Score center
                int ci = row*W + col;
                for (int ki = 0; ki < k; ++ki)
                    xc(ki) = white.data[static_cast<size_t>(ki)*N + ci] - mean(ki);
                double d2 = xc.dot(ldlt.solve(xc));
                res.scoreMap.data[ci] = static_cast<float>(d2);
                if (d2 > thr) { res.binaryMask.data[ci] = 1.0f; ++anomalyCount; }

                mean.setZero(); cov.setZero();
            }
            ++doneRows;
            if (opt.onProgress)
                opt.onProgress(0.45 + 0.55 * doneRows.load() / H);
        }
    };

    std::vector<std::thread> threads;
    int rowsPerThread = H / nThreads;
    for (int t = 0; t < nThreads; ++t) {
        int r0 = t * rowsPerThread;
        int r1 = (t == nThreads-1) ? H : r0 + rowsPerThread;
        threads.emplace_back(scoreBlock, r0, r1);
    }
    for (auto& th : threads) th.join();

    res.anomalyCount = anomalyCount.load();
    if (opt.onProgress) opt.onProgress(1.0);
    return res;
}

// ---------------------------------------------------------------------------
RxResult RxDetector::detect(const RasterCube& cube, const RxOptions& opt) {
    return (opt.mode == RxOptions::Mode::Local)
        ? detectLocal(cube, opt)
        : detectGlobal(cube, opt);
}

} // namespace hsi
