//#include "hsi/AtmosphericCorrector.h"
//#include "hsi/Logger.h"

//#include <cmath>
//#include <algorithm>
//#include <thread>
//#include <vector>
//#include <atomic>

//namespace hsi {

//double AtmosphericCorrector::earthSunDistanceAU(int dayOfYear) {
//    constexpr double pi = 3.1415926535897932;
//    return 1.0 - 0.01672 * std::cos((2.0 * pi * (dayOfYear - 4)) / 365.25);
//}

//RasterCube AtmosphericCorrector::radianceToToaReflectance(const RasterCube& radianceCube,
//                                                            const SolarGeometry& geom,
//                                                            const std::vector<double>& esunPerBand) {
//    if (esunPerBand.empty()) {
//        throw HsiError("AtmosphericCorrector: ESUN (solar exoatmospheric irradiance) table is empty. "
//                        "Load a Thuillier-spectrum-derived per-band ESUN table first.");
//    }

//    constexpr double pi = 3.1415926535897932;
//    double d = earthSunDistanceAU(geom.dayOfYear);
//    double cosTheta = std::cos(geom.solarZenithDeg * pi / 180.0);
//    if (cosTheta <= 1e-6) {
//        throw HsiError("AtmosphericCorrector: solar zenith angle too close to 90 degrees (cos ~ 0).");
//    }

//    if (static_cast<int>(esunPerBand.size()) != radianceCube.bands) {
//        Logger::log("AtmosphericCorrector", "ESUN table length (" + std::to_string(esunPerBand.size()) +
//                    ") does not match band count (" + std::to_string(radianceCube.bands) +
//                    "); reusing it cyclically.");
//    }

//    RasterCube out;
//    out.allocate(radianceCube.width, radianceCube.height, radianceCube.bands);
//    out.geoTransform = radianceCube.geoTransform;
//    out.projectionWkt = radianceCube.projectionWkt;
//    out.bandNumbers = radianceCube.bandNumbers;
//    out.bandNames = radianceCube.bandNames;

//    for (int b = 0; b < radianceCube.bands; ++b) {
//        double esun = esunPerBand[b % esunPerBand.size()];
//        double denom = esun * cosTheta;
//        if (std::abs(denom) < 1e-9) {
//            Logger::log("AtmosphericCorrector", "BUG20: ESUN or cos(theta) near zero for band " +
//                        std::to_string(b) + " — skipping TOA conversion for this band.");
//            continue;
//        }
//        double factor = (pi * d * d) / denom;
//        size_t base = static_cast<size_t>(b) * radianceCube.width * radianceCube.height;
//        for (size_t i = 0; i < radianceCube.pixelCount(); ++i) {
//            out.data[base + i] = static_cast<float>(radianceCube.data[base + i] * factor);
//        }
//    }

//    Logger::log("AtmosphericCorrector", "TOA reflectance computed (d=" + std::to_string(d) +
//                " AU, solar zenith=" + std::to_string(geom.solarZenithDeg) + " deg).");
//    return out;
//}

//RasterCube AtmosphericCorrector::toaToSurfaceReflectanceDOS(const RasterCube& toaReflectance,
//                                                              double darkObjectPercentile) {
//    RasterCube out;
//    out.allocate(toaReflectance.width, toaReflectance.height, toaReflectance.bands);
//    out.geoTransform = toaReflectance.geoTransform;
//    out.projectionWkt = toaReflectance.projectionWkt;
//    out.bandNumbers = toaReflectance.bandNumbers;
//    out.bandNames = toaReflectance.bandNames;

//    const size_t pixelCount = toaReflectance.pixelCount();
//    const int    B           = toaReflectance.bands;

//    // Parallelise across bands — each thread processes an independent band
//    // slice with its own sort buffer.  This is the dominant cost in Step 2
//    // (~103 min on a 198-band 861x2961 scene was entirely this serial loop).
//    const int nThreads = std::max(1, std::min(B,
//        static_cast<int>(std::thread::hardware_concurrency())));
//    std::atomic<int> nextBand{0};

//    auto processBlock = [&]() {
//        std::vector<float> sortBuf(pixelCount);
//        while (true) {
//            int b = nextBand.fetch_add(1);
//            if (b >= B) break;
//            size_t base = static_cast<size_t>(b) * pixelCount;
//            std::copy(toaReflectance.data.begin() + base,
//                      toaReflectance.data.begin() + base + pixelCount,
//                      sortBuf.begin());
//            std::sort(sortBuf.begin(), sortBuf.end());
//            size_t idx = static_cast<size_t>((darkObjectPercentile / 100.0) * (pixelCount - 1));
//            float darkValue = sortBuf[std::min(idx, pixelCount - 1)];
//            for (size_t i = 0; i < pixelCount; ++i) {
//                float v = toaReflectance.data[base + i] - darkValue;
//                out.data[base + i] = std::clamp(v, 0.0f, 1.0f);  // BUG21: clamp to valid [0,1]
//            }
//        }
//    };

//    std::vector<std::thread> pool;
//    pool.reserve(nThreads);
//    for (int t = 0; t < nThreads; ++t) pool.emplace_back(processBlock);
//    for (auto& th : pool) th.join();

//    Logger::log("AtmosphericCorrector", "Dark Object Subtraction (simplified QUAC-style) surface "
//                "reflectance complete, percentile=" + std::to_string(darkObjectPercentile));
//    return out;
//}

//RasterCube AtmosphericCorrector::toaToSurfaceReflectanceELM(const RasterCube& toaReflectance,
//                                                              const CalibrationTarget& darkTarget,
//                                                              const CalibrationTarget& brightTarget) {
//    if (static_cast<int>(darkTarget.meanToaPerBand.size()) != toaReflectance.bands ||
//        static_cast<int>(brightTarget.meanToaPerBand.size()) != toaReflectance.bands) {
//        throw HsiError("AtmosphericCorrector: ELM calibration target spectra must have one value per "
//                        "band of the TOA cube (" + std::to_string(toaReflectance.bands) + " bands), got " +
//                        std::to_string(darkTarget.meanToaPerBand.size()) + " (dark) / " +
//                        std::to_string(brightTarget.meanToaPerBand.size()) + " (bright).");
//    }

//    RasterCube out;
//    out.allocate(toaReflectance.width, toaReflectance.height, toaReflectance.bands);
//    out.geoTransform = toaReflectance.geoTransform;
//    out.projectionWkt = toaReflectance.projectionWkt;
//    out.bandNumbers = toaReflectance.bandNumbers;
//    out.bandNames = toaReflectance.bandNames;

//    size_t pixelCount = toaReflectance.pixelCount();
//    int degenerateBands = 0;

//    for (int b = 0; b < toaReflectance.bands; ++b) {
//        double toaDark   = darkTarget.meanToaPerBand[b];
//        double toaBright = brightTarget.meanToaPerBand[b];
//        double denom = toaBright - toaDark;

//        double m, c;
//        if (std::abs(denom) < 1e-9) {
//            // Dark and bright targets read almost identically in this band --
//            // can't fit a line through two near-coincident points, so pass
//            // the band through unscaled rather than dividing by ~zero.
//            m = 1.0;
//            c = 0.0;
//            ++degenerateBands;
//        } else {
//            m = (brightTarget.knownReflectance - darkTarget.knownReflectance) / denom;
//            c = darkTarget.knownReflectance - m * toaDark;
//        }

//        size_t base = static_cast<size_t>(b) * toaReflectance.width * toaReflectance.height;
//        for (size_t i = 0; i < pixelCount; ++i) {
//            double v = m * toaReflectance.data[base + i] + c;
//            if (v < 0.0) v = 0.0;
//            if (v > 1.0) v = 1.0;
//            out.data[base + i] = static_cast<float>(v);
//        }
//    }

//    if (degenerateBands > 0) {
//        Logger::log("AtmosphericCorrector", "ELM: " + std::to_string(degenerateBands) +
//                    " band(s) had near-identical dark/bright TOA readings and were passed through unscaled "
//                    "-- the chosen calibration pixels may not be well separated in those bands.");
//    }

//    Logger::log("AtmosphericCorrector", "Empirical Line Method surface reflectance complete "
//                "(dark target rho=" + std::to_string(darkTarget.knownReflectance) +
//                ", bright target rho=" + std::to_string(brightTarget.knownReflectance) + ").");
//    return out;
//}

//} // namespace hsi






#include "hsi/AtmosphericCorrector.h"
#include "hsi/Logger.h"

#include <cmath>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>

namespace hsi {

double AtmosphericCorrector::earthSunDistanceAU(int dayOfYear) {
    constexpr double pi = 3.1415926535897932;
    return 1.0 - 0.01672 * std::cos((2.0 * pi * (dayOfYear - 4)) / 365.25);
}

RasterCube AtmosphericCorrector::radianceToToaReflectance(const RasterCube& radianceCube,
                                                            const SolarGeometry& geom,
                                                            const std::vector<double>& esunPerBand) {
    if (esunPerBand.empty()) {
        throw HsiError("AtmosphericCorrector: ESUN (solar exoatmospheric irradiance) table is empty. "
                        "Load a Thuillier-spectrum-derived per-band ESUN table first.");
    }

    constexpr double pi = 3.1415926535897932;
    double d = earthSunDistanceAU(geom.dayOfYear);
    double cosTheta = std::cos(geom.solarZenithDeg * pi / 180.0);
    if (cosTheta <= 1e-6) {
        throw HsiError("AtmosphericCorrector: solar zenith angle too close to 90 degrees (cos ~ 0).");
    }

    if (static_cast<int>(esunPerBand.size()) != radianceCube.bands) {
        Logger::log("AtmosphericCorrector", "ESUN table length (" + std::to_string(esunPerBand.size()) +
                    ") does not match band count (" + std::to_string(radianceCube.bands) +
                    "); reusing it cyclically.");
    }

    RasterCube out;
    out.allocate(radianceCube.width, radianceCube.height, radianceCube.bands);
    out.geoTransform = radianceCube.geoTransform;
    out.projectionWkt = radianceCube.projectionWkt;
    out.bandNumbers = radianceCube.bandNumbers;
    out.bandNames = radianceCube.bandNames;

    // Parallelise across bands -- each band's TOA conversion is an
    // independent scale-by-constant over that band's pixels, so this is the
    // same embarrassingly-parallel shape as toaToSurfaceReflectanceDOS
    // below. This was the last serial per-band loop left in Step 1.
    const size_t pixelCount = radianceCube.pixelCount();
    const int    B          = radianceCube.bands;
    const int nThreads = std::max(1, std::min(B,
        static_cast<int>(std::thread::hardware_concurrency())));
    std::atomic<int> nextBand{0};

    auto processBlock = [&]() {
        while (true) {
            int b = nextBand.fetch_add(1);
            if (b >= B) break;
            double esun = esunPerBand[b % esunPerBand.size()];
            double denom = esun * cosTheta;
            if (std::abs(denom) < 1e-9) {
                Logger::log("AtmosphericCorrector", "BUG20: ESUN or cos(theta) near zero for band " +
                            std::to_string(b) + " — skipping TOA conversion for this band.");
                continue;
            }
            double factor = (pi * d * d) / denom;
            size_t base = static_cast<size_t>(b) * pixelCount;
            for (size_t i = 0; i < pixelCount; ++i) {
                out.data[base + i] = static_cast<float>(radianceCube.data[base + i] * factor);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(nThreads);
    for (int t = 0; t < nThreads; ++t) pool.emplace_back(processBlock);
    for (auto& th : pool) th.join();

    Logger::log("AtmosphericCorrector", "TOA reflectance computed (d=" + std::to_string(d) +
                " AU, solar zenith=" + std::to_string(geom.solarZenithDeg) + " deg).");
    return out;
}

RasterCube AtmosphericCorrector::toaToSurfaceReflectanceDOS(const RasterCube& toaReflectance,
                                                              double darkObjectPercentile) {
    RasterCube out;
    out.allocate(toaReflectance.width, toaReflectance.height, toaReflectance.bands);
    out.geoTransform = toaReflectance.geoTransform;
    out.projectionWkt = toaReflectance.projectionWkt;
    out.bandNumbers = toaReflectance.bandNumbers;
    out.bandNames = toaReflectance.bandNames;

    const size_t pixelCount = toaReflectance.pixelCount();
    const int    B           = toaReflectance.bands;

    // Parallelise across bands — each thread processes an independent band
    // slice with its own sort buffer.  This is the dominant cost in Step 2
    // (~103 min on a 198-band 861x2961 scene was entirely this serial loop).
    const int nThreads = std::max(1, std::min(B,
        static_cast<int>(std::thread::hardware_concurrency())));
    std::atomic<int> nextBand{0};

    auto processBlock = [&]() {
        std::vector<float> sortBuf(pixelCount);
        while (true) {
            int b = nextBand.fetch_add(1);
            if (b >= B) break;
            size_t base = static_cast<size_t>(b) * pixelCount;
            std::copy(toaReflectance.data.begin() + base,
                      toaReflectance.data.begin() + base + pixelCount,
                      sortBuf.begin());
            std::sort(sortBuf.begin(), sortBuf.end());
            size_t idx = static_cast<size_t>((darkObjectPercentile / 100.0) * (pixelCount - 1));
            float darkValue = sortBuf[std::min(idx, pixelCount - 1)];
            for (size_t i = 0; i < pixelCount; ++i) {
                float v = toaReflectance.data[base + i] - darkValue;
                out.data[base + i] = std::clamp(v, 0.0f, 1.0f);  // BUG21: clamp to valid [0,1]
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(nThreads);
    for (int t = 0; t < nThreads; ++t) pool.emplace_back(processBlock);
    for (auto& th : pool) th.join();

    Logger::log("AtmosphericCorrector", "Dark Object Subtraction (simplified QUAC-style) surface "
                "reflectance complete, percentile=" + std::to_string(darkObjectPercentile));
    return out;
}

RasterCube AtmosphericCorrector::toaToSurfaceReflectanceELM(const RasterCube& toaReflectance,
                                                              const CalibrationTarget& darkTarget,
                                                              const CalibrationTarget& brightTarget) {
    if (static_cast<int>(darkTarget.meanToaPerBand.size()) != toaReflectance.bands ||
        static_cast<int>(brightTarget.meanToaPerBand.size()) != toaReflectance.bands) {
        throw HsiError("AtmosphericCorrector: ELM calibration target spectra must have one value per "
                        "band of the TOA cube (" + std::to_string(toaReflectance.bands) + " bands), got " +
                        std::to_string(darkTarget.meanToaPerBand.size()) + " (dark) / " +
                        std::to_string(brightTarget.meanToaPerBand.size()) + " (bright).");
    }

    RasterCube out;
    out.allocate(toaReflectance.width, toaReflectance.height, toaReflectance.bands);
    out.geoTransform = toaReflectance.geoTransform;
    out.projectionWkt = toaReflectance.projectionWkt;
    out.bandNumbers = toaReflectance.bandNumbers;
    out.bandNames = toaReflectance.bandNames;

    size_t pixelCount = toaReflectance.pixelCount();
    std::atomic<int> degenerateBands{0};

    const int B = toaReflectance.bands;
    const int nThreads = std::max(1, std::min(B,
        static_cast<int>(std::thread::hardware_concurrency())));
    std::atomic<int> nextBand{0};

    auto processBlock = [&]() {
        while (true) {
            int b = nextBand.fetch_add(1);
            if (b >= B) break;
            double toaDark   = darkTarget.meanToaPerBand[b];
            double toaBright = brightTarget.meanToaPerBand[b];
            double denom = toaBright - toaDark;

            double m, c;
            if (std::abs(denom) < 1e-9) {
                // Dark and bright targets read almost identically in this band --
                // can't fit a line through two near-coincident points, so pass
                // the band through unscaled rather than dividing by ~zero.
                m = 1.0;
                c = 0.0;
                ++degenerateBands;
            } else {
                m = (brightTarget.knownReflectance - darkTarget.knownReflectance) / denom;
                c = darkTarget.knownReflectance - m * toaDark;
            }

            size_t base = static_cast<size_t>(b) * pixelCount;
            for (size_t i = 0; i < pixelCount; ++i) {
                double v = m * toaReflectance.data[base + i] + c;
                if (v < 0.0) v = 0.0;
                if (v > 1.0) v = 1.0;
                out.data[base + i] = static_cast<float>(v);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(nThreads);
    for (int t = 0; t < nThreads; ++t) pool.emplace_back(processBlock);
    for (auto& th : pool) th.join();

    if (degenerateBands > 0) {
        Logger::log("AtmosphericCorrector", "ELM: " + std::to_string(degenerateBands) +
                    " band(s) had near-identical dark/bright TOA readings and were passed through unscaled "
                    "-- the chosen calibration pixels may not be well separated in those bands.");
    }

    Logger::log("AtmosphericCorrector", "Empirical Line Method surface reflectance complete "
                "(dark target rho=" + std::to_string(darkTarget.knownReflectance) +
                ", bright target rho=" + std::to_string(brightTarget.knownReflectance) + ").");
    return out;
}

} // namespace hsi
