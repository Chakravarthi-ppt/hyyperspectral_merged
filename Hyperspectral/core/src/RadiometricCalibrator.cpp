#include "hsi/RadiometricCalibrator.h"
#include "hsi/Logger.h"
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>

namespace hsi {

RasterCube RadiometricCalibrator::dnToRadiance(const RasterCube& dnCube) {
    return dnToRadiance(dnCube, Options());
}

RasterCube RadiometricCalibrator::dnToRadiance(const RasterCube& dnCube, const Options& opt) {
    if (dnCube.bandNumbers.empty() || static_cast<int>(dnCube.bandNumbers.size()) != dnCube.bands) {
        throw HsiError("RadiometricCalibrator: cube has no per-band sensor band numbers; "
                        "load it with RasterIO and keep bandNumbers populated.");
    }

    RasterCube out;
    out.allocate(dnCube.width, dnCube.height, dnCube.bands);
    out.geoTransform = dnCube.geoTransform;
    out.projectionWkt = dnCube.projectionWkt;
    out.bandNumbers = dnCube.bandNumbers;
    out.bandNames = dnCube.bandNames;
    out.hasNoData = dnCube.hasNoData;
    out.noDataValue = dnCube.noDataValue;

    std::atomic<int> passthroughCount{0};
    const int    B       = dnCube.bands;
    const size_t pxCount = dnCube.pixelCount();
    const int nThreads   = std::max(1, std::min(B,
        static_cast<int>(std::thread::hardware_concurrency())));
    std::atomic<int> nextBand{0};

    auto processBlock = [&]() {
        while (true) {
            int b = nextBand.fetch_add(1);
            if (b >= B) break;
            int sensorBand = dnCube.bandNumbers[b];
            double scale;
            if (sensorBand >= opt.vnirStartBand && sensorBand <= opt.vnirEndBand) {
                scale = opt.vnirScaleFactor;
            } else if (sensorBand >= opt.swirStartBand && sensorBand <= opt.swirEndBand) {
                scale = opt.swirScaleFactor;
            } else {
                scale = 1.0;
                ++passthroughCount;
            }
            size_t base = static_cast<size_t>(b) * pxCount;
            for (size_t i = 0; i < pxCount; ++i) {
                float radiance = static_cast<float>(dnCube.data[base + i] / scale);
                // A negative DN (sensor artifact / corrupted pixel) would
                // otherwise propagate as negative radiance into every later
                // stage (reflectance, indices, classifiers) as a physically
                // meaningless value. Radiance can't be negative; clamp it.
                out.data[base + i] = std::max(0.0f, radiance);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(nThreads);
    for (int t = 0; t < nThreads; ++t) pool.emplace_back(processBlock);
    for (auto& th : pool) th.join();

    if (passthroughCount > 0) {
        Logger::log("RadiometricCalibrator", std::to_string(passthroughCount) +
                    " band(s) fell outside both VNIR/SWIR calibrated ranges and were passed through "
                    "unscaled -- run BandSelector first to drop overlap/uncalibrated bands.");
    }
    Logger::log("RadiometricCalibrator", "DN to radiance conversion complete for " +
                std::to_string(dnCube.bands) + " bands.");
    return out;
}

} // namespace hsi
