#pragma once
#include "hsi/Types.h"

namespace hsi {

// Implements the band-selection rule from the workbook:
//   VNIR calibrated:  bands 8-57   (kept, divide by 40 upstream)
//   overlap zone:     bands 58-76  (omitted)
//   SWIR calibrated:  bands 77-224 (kept, divide by 80 upstream)
//   SWIR tail:        bands 225-242 (omitted, uncalibrated)
class BandSelector {
public:
    struct Rule {
        int vnirStartBand = 8,  vnirEndBand = 57;
        int swirStartBand = 77, swirEndBand = 224;

        // Even within the calibrated VNIR/SWIR ranges above, Hyperion has a
        // well-documented set of noisy / water-vapor-absorption bands that
        // are normally dropped before PCA or classification (they're mostly
        // atmospheric noise, not ground signal, and their high variance can
        // dominate PCA components or throw off k-means/SAM/SVM). Sensor band
        // numbers, per published Hyperion band-quality references:
        //   120-132, 165-182, 221-224  (water vapor absorption / low SNR)
        bool excludeKnownBadBands = true;
    };

    // Requires `fullCube.bandNumbers` to hold the original sensor band index
    // per stored band. Returns a new cube containing only the kept bands, in
    // ascending sensor-band order.
    static RasterCube selectCalibratedBands(const RasterCube& fullCube, const Rule& rule);
    static RasterCube selectCalibratedBands(const RasterCube& fullCube); // uses default Rule

    // True if `sensorBand` falls in one of the known noisy/water-absorption
    // ranges above. Exposed separately so callers (e.g. a "bands dropped"
    // UI list) can explain *why* a band is missing, not just that it is.
    static bool isKnownBadBand(int sensorBand);
};

} // namespace hsi
