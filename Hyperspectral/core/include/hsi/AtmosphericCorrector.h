#pragma once
#include "hsi/Types.h"

namespace hsi {

// Step 2 from the workflow: radiance -> TOA reflectance -> surface reflectance.
//
//   rho_lambda = (pi * L_lambda * d^2) / (ESUN_lambda * cos(theta_s))
//
//   d = 1.0 - 0.01672 * cos( 2*pi*(DOY - 4) / 365.25 )     [astronomical units]
//
// Surface reflectance can then be approximated two ways:
//   - DOS (Dark Object Subtraction): a simplified stand-in for a full
//     QUAC/ELM atmospheric model -- subtract the darkest-pixel reflectance
//     per band (assumed residual path radiance) from every pixel, clamped
//     at zero. No ground truth needed, but a rougher approximation.
//   - ELM (Empirical Line Method): fit rho_surface = m*rho_TOA + b per
//     band from two calibration targets whose true ground reflectance is
//     already known (e.g. deep water ~0.01, a concrete runway/sand patch
//     ~0.6), then apply that line to every pixel. More accurate when good
//     calibration targets are available in the scene.
class AtmosphericCorrector {
public:
    struct SolarGeometry {
        double solarZenithDeg = 30.0; // theta_s, scene-center solar zenith angle
        int dayOfYear = 1;            // 1-366, used for the earth-sun distance
    };

    enum class CorrectionMethod { DOS, ELM };

    // One ELM calibration target: the mean TOA reflectance spectrum
    // measured over a handful of sample pixels (e.g. from
    // SpectralLibrary::buildFromSamples), paired with the true ground
    // reflectance you already know that target has.
    struct CalibrationTarget {
        std::vector<double> meanToaPerBand; // one value per band of the TOA cube being corrected
        double knownReflectance = 0.0;       // e.g. ~0.01 for deep water, ~0.6 for concrete/sand
    };

    static double earthSunDistanceAU(int dayOfYear);

    // esunPerBand must be parallel to radianceCube's bands (W/m^2/um). If a
    // shorter list is supplied it is reused cyclically with a logged warning.
    static RasterCube radianceToToaReflectance(const RasterCube& radianceCube,
                                                const SolarGeometry& geom,
                                                const std::vector<double>& esunPerBand);

    // darkObjectPercentile: which low percentile per band is treated as the
    // "dark object" floor to subtract (typical DOS uses the 0.1-1st percentile).
    static RasterCube toaToSurfaceReflectanceDOS(const RasterCube& toaReflectance,
                                                  double darkObjectPercentile = 0.5);

    // Empirical Line Method: fits rho_surface = m*rho_TOA + b per band from
    // darkTarget/brightTarget and applies it to every pixel, clamped to
    // [0,1]. Both targets' meanToaPerBand must have one entry per band of
    // toaReflectance. If a band's dark/bright TOA readings are (almost)
    // identical -- can't fit a line through two near-coincident points --
    // that band is passed through unscaled rather than dividing by ~zero,
    // and a warning is logged naming how many bands this happened to.
    static RasterCube toaToSurfaceReflectanceELM(const RasterCube& toaReflectance,
                                                  const CalibrationTarget& darkTarget,
                                                  const CalibrationTarget& brightTarget);
};

} // namespace hsi
