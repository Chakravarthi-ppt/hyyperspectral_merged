#pragma once
#include "hsi/Types.h"

namespace hsi {

class RasterToVector {
public:
    // Polygonizes band 0 of `maskCube` (expects integer-like 0/1 or class
    // values) into vector features written to `outVectorPath`.
    // driverName: "GeoJSON" (default), "ESRI Shapefile", "GPKG", etc.
    static void polygonize(const RasterCube& maskCube,
                            const std::string& outVectorPath,
                            const std::string& driverName = "GeoJSON",
                            const std::string& attributeFieldName = "class");
};

} // namespace hsi
