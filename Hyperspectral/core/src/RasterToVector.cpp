#include "hsi/RasterToVector.h"
#include <cstdint>
#include <vector>
#include "hsi/Logger.h"

#include <gdal_priv.h>
#include <gdal_alg.h>
#include <ogrsf_frmts.h>

namespace hsi {

void RasterToVector::polygonize(const RasterCube& maskCube,
                                 const std::string& outVectorPath,
                                 const std::string& driverName,
                                 const std::string& attributeFieldName) {
    if (maskCube.bands < 1) throw HsiError("RasterToVector: mask cube has no bands.");

    GDALAllRegister();

    // Build an in-memory raster for band 0 so we don't need a temp file on disk.
    GDALDriver* memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    GDALDataset* memDs = memDriver->Create("", maskCube.width, maskCube.height, 1, GDT_Float32, nullptr);
    memDs->SetGeoTransform(const_cast<double*>(maskCube.geoTransform.data()));
    if (!maskCube.projectionWkt.empty()) memDs->SetProjection(maskCube.projectionWkt.c_str());

    GDALRasterBand* srcBand = memDs->GetRasterBand(1);
    std::vector<float> rowBuf(maskCube.width);
    for (int row = 0; row < maskCube.height; ++row) {
        for (int col = 0; col < maskCube.width; ++col) rowBuf[col] = maskCube.at(0, row, col);
        srcBand->RasterIO(GF_Write, 0, row, maskCube.width, 1, rowBuf.data(), maskCube.width, 1, GDT_Float32, 0, 0);
    }

    GDALDriver* vecDriver = GetGDALDriverManager()->GetDriverByName(driverName.c_str());
    if (!vecDriver) {
        GDALClose(memDs);
        throw HsiError("RasterToVector: unknown vector driver '" + driverName + "'.");
    }
    GDALDataset* vecDs = vecDriver->Create(outVectorPath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!vecDs) {
        GDALClose(memDs);
        throw HsiError("RasterToVector: failed to create vector output '" + outVectorPath + "'.");
    }

    OGRSpatialReference srs;
    if (!maskCube.projectionWkt.empty()) {
        char* wkt = const_cast<char*>(maskCube.projectionWkt.c_str());
        srs.importFromWkt(&wkt);
    }
    OGRLayer* layer = vecDs->CreateLayer("built_up", maskCube.projectionWkt.empty() ? nullptr : &srs, wkbPolygon, nullptr);
    if (!layer) {
        GDALClose(vecDs);
        GDALClose(memDs);
        throw HsiError("RasterToVector: failed to create output layer.");
    }

    OGRFieldDefn fieldDefn(attributeFieldName.c_str(), OFTInteger);
    layer->CreateField(&fieldDefn);
    int fieldIndex = layer->GetLayerDefn()->GetFieldIndex(attributeFieldName.c_str());

    // BUG10: Create a mask so GDALPolygonize skips NoData/background (label=0).
    // Without this, the entire off-scene border generates invalid polygons.
    GDALDataset* maskDs = nullptr;
    GDALRasterBand* maskBand = nullptr;
    {
        GDALDriver* memDrv = GetGDALDriverManager()->GetDriverByName("MEM");
        if (memDrv) {
            int w = maskCube.width, h = maskCube.height;
            maskDs = memDrv->Create("", w, h, 1, GDT_Byte, nullptr);
            if (maskDs) {
                GDALRasterBand* mb = maskDs->GetRasterBand(1);
                std::vector<float>   lineF(w);
                std::vector<uint8_t> lineM(w);
                for (int row = 0; row < h; ++row) {
                    srcBand->RasterIO(GF_Read, 0, row, w, 1, lineF.data(), w, 1, GDT_Float32, 0, 0);
                    for (int c = 0; c < w; ++c) lineM[c] = (lineF[c] > 0.5f) ? 255 : 0;
                    mb->RasterIO(GF_Write, 0, row, w, 1, lineM.data(), w, 1, GDT_Byte, 0, 0);
                }
                maskBand = mb;
            }
        }
    }
    CPLErr err = GDALPolygonize(srcBand, maskBand, layer, fieldIndex, nullptr, nullptr, nullptr);
    if (maskDs) GDALClose(maskDs);

    GDALClose(vecDs);
    GDALClose(memDs);

    if (err != CE_None) throw HsiError("RasterToVector: GDALPolygonize failed for '" + outVectorPath + "'.");

    Logger::log("RasterToVector", "Polygonized mask to '" + outVectorPath + "' using driver " + driverName + ".");
}

} // namespace hsi
