#pragma once
#include "hsi/Types.h"
#include <functional>

namespace hsi {

/**
 * Reed-Xiaoli (RX) Anomaly Detector
 *
 * Two variants:
 *
 *  Global RX — Mahalanobis distance from scene-wide mean/covariance.
 *    d²(x) = (x-μ)ᵀ Σ⁻¹ (x-μ)
 *    Multithreaded pixel scoring. Fast even on large 198-band scenes.
 *    Threshold: chi-squared inverse CDF at (1 − pfa) with B degrees of freedom.
 *
 *  Local RX (CFAR) — PCA-whitened then dual-window CFAR.
 *    1. Project 198 bands → nPcComponents whitened principal components.
 *    2. For each pixel, estimate local mean+covariance from an annular
 *       window (outer_r excluding inner guard inner_r) in PC space.
 *    3. Mahalanobis score in PC space; threshold is chi-sq with nPcComponents df.
 *
 *    PCA whitening is the critical optimisation: it reduces the per-pixel
 *    covariance inversion from 198×198 to k×k (k≈10), making Local RX
 *    ~400× faster with negligible loss of anomaly separability.
 *    (Kwon & Nasrabadi 2005, "Kernel RX-algorithm", IEEE TGRS)
 *
 * Both modes run pixel scoring on all available hardware threads.
 * onProgress is called from the worker thread — connect UI updates via
 * Qt::QueuedConnection or use ProgressDialog::runTask().
 */

struct RxOptions {
    enum class Mode { Global, Local };
    Mode   mode          = Mode::Global;
    int    outerR        = 15;   ///< Local RX: outer window half-size (pixels)
    int    innerR        = 3;    ///< Local RX: guard region half-size (pixels)
    int    nPcComponents = 10;   ///< Local RX: PCA components to retain before CFAR
    double pfa           = 1e-4; ///< Probability of false alarm

    /// Progress callback: fraction in [0,1]. Called from worker thread.
    std::function<void(double)> onProgress;
};

struct RxResult {
    RasterCube scoreMap;    ///< 1-band float32: Mahalanobis distance² per pixel
    RasterCube binaryMask;  ///< 1-band float32: 1=anomaly, 0=background
    double     threshold    = 0.0;
    int        anomalyCount = 0;
};

class RxDetector {
public:
    static RxResult detect(const RasterCube& cube, const RxOptions& opt = {});

private:
    static RxResult detectGlobal(const RasterCube& cube, const RxOptions& opt);
    static RxResult detectLocal (const RasterCube& cube, const RxOptions& opt);
};

} // namespace hsi
