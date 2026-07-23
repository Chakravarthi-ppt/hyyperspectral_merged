#pragma once
#include <string>
#include <vector>

namespace hsi {

// Loads a per-band ESUN (solar exoatmospheric irradiance, W/m^2/um) table
// from a two-column CSV: band_number,esun_value -- typically derived once
// from the Thuillier solar irradiance spectrum for the sensor's band centers
// and reused across scenes. A flat fallback generator is provided for
// quick testing when no real table is available yet.
class EsunTable {
public:
    static std::vector<double> loadCsv(const std::string& path);

    // Generates a flat placeholder table (NOT radiometrically accurate --
    // for pipeline smoke-testing only). Logs a loud warning.
    static std::vector<double> flatFallback(int numBands, double value = 1000.0);
};

} // namespace hsi
