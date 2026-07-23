#pragma once
#include "hsi/Types.h"

namespace hsi {

// Cross-tabulates two single-band classified rasters (same grid, different
// dates) into a from-class x to-class pixel-count matrix -- the land-use
// change matrix referenced in the workflow.
class ChangeDetector {
public:
    static ChangeMatrixResult computeChangeMatrix(const RasterCube& classifiedDateA,
                                                    const RasterCube& classifiedDateB,
                                                    double pixelAreaSqMeters = 900.0);

    static void saveMatrixCsv(const ChangeMatrixResult& result, const std::string& path);

    // Pixel-wise "did this pixel's class change between date A and date B"
    // map: single-band raster, 1 = changed (any from-class != to-class,
    // i.e. any off-diagonal cell in the matrix), 0 = unchanged (diagonal).
    // This is the visual counterpart to the numeric matrix -- the matrix
    // tells you *what* changed into *what*, this shows *where*.
    static RasterCube computeChangeMap(const RasterCube& classifiedDateA,
                                        const RasterCube& classifiedDateB);
};

} // namespace hsi
