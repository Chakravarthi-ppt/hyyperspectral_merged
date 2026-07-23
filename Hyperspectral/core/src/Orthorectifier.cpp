//#include "hsi/Orthorectifier.h"
//#include "hsi/RasterIO.h"
//#include "hsi/Logger.h"

//#include <gdal_priv.h>
//#include <gdalwarper.h>
//#include <ogr_spatialref.h>
//#include <cpl_conv.h>
//#include <cpl_string.h>

//namespace hsi {

//static GDALResampleAlg resampleAlgFromString(const std::string& s) {
//    if (s == "nearest") return GRA_NearestNeighbour;
//    if (s == "cubic") return GRA_Cubic;
//    return GRA_Bilinear;
//}

//Orthorectifier::Outcome Orthorectifier::ensureOrthorectified(const std::string& inputPath,
//                                                               const std::string& outputPath) {
//    return ensureOrthorectified(inputPath, outputPath, Options());
//}

//Orthorectifier::Outcome Orthorectifier::ensureOrthorectified(const std::string& inputPath,
//                                                               const std::string& outputPath,
//                                                               const Options& opts) {
//    RasterIO::init();
//    Outcome outcome;

//    if (RasterIO::looksOrthorectified(inputPath)) {
//        Logger::log("Orthorectifier", "'" + inputPath + "' already has a valid geotransform/SRS and no GCPs "
//                                       "-- treating as already orthorectified, skipping warp.");
//        outcome.wasAlreadyOrthorectified = true;
//        outcome.outputPath = inputPath;
//        return outcome;
//    }

//    Logger::log("Orthorectifier", "'" + inputPath + "' is not orthorectified (raw GCPs or missing SRS) -- warping.");

//    GDALDataset* src = static_cast<GDALDataset*>(GDALOpen(inputPath.c_str(), GA_ReadOnly));
//    if (!src) throw HsiError("Orthorectifier: failed to open '" + inputPath + "'");

//    std::string dstWkt = opts.targetSrsWkt;
//    if (dstWkt.empty()) {
//        // Fall back to the dataset's own projection if it has one, else WGS84.
//        const char* wkt = src->GetProjectionRef();
//        if (wkt && std::string(wkt).size() > 0) {
//            dstWkt = wkt;
//        } else {
//            OGRSpatialReference srs;
//            srs.SetWellKnownGeogCS("WGS84");
//            char* wktOut = nullptr;
//            srs.exportToWkt(&wktOut);
//            dstWkt = wktOut;
//            CPLFree(wktOut);
//        }
//    } else {
//        // An explicit target CRS was given -- compare it against the
//        // source's own CRS so a caller who passed the wrong target (e.g.
//        // copy-pasted from a different scene) finds out from the log
//        // instead of getting a silently-reprojected/misaligned output.
//        const char* srcWktC = src->GetProjectionRef();
//        if (srcWktC && std::string(srcWktC).size() > 0) {
//            OGRSpatialReference srcSrs, dstSrs;
//            char* srcWktMutable = const_cast<char*>(srcWktC);
//            srcSrs.importFromWkt(&srcWktMutable);
//            std::string dstWktCopy = opts.targetSrsWkt;
//            char* dstWktMutable = const_cast<char*>(dstWktCopy.c_str());
//            dstSrs.importFromWkt(&dstWktMutable);
//            if (!srcSrs.IsSame(&dstSrs)) {
//                Logger::log("Orthorectifier", "'" + inputPath + "' source CRS differs from the requested "
//                            "target CRS -- this will reproject, not just resample. If that wasn't intended, "
//                            "double-check the target CRS passed in.");
//            }
//        }
//    }

//    char* dstWktC = const_cast<char*>(dstWkt.c_str());

//    void* transformerArg = GDALCreateGenImgProjTransformer(src, nullptr, nullptr, dstWktC, TRUE, 0, 1);
//    if (!transformerArg) {
//        GDALClose(src);
//        throw HsiError("Orthorectifier: could not build a georeferencing transformer for '" + inputPath +
//                        "' (no GCPs/RPCs found). Provide a pre-orthorectified product instead.");
//    }

//    double geoTransformOut[6];
//    int xSize = 0, ySize = 0;
//    CPLErr gerr = GDALSuggestedWarpOutput(src, GDALGenImgProjTransform, transformerArg, geoTransformOut, &xSize, &ySize);
//    GDALDestroyGenImgProjTransformer(transformerArg);
//    if (gerr != CE_None || xSize <= 0 || ySize <= 0) {
//        GDALClose(src);
//        throw HsiError("Orthorectifier: failed to compute warped output extent for '" + inputPath + "'");
//    }

//    // Snap to the requested output pixel size while keeping the same origin/extent.
//    double xRes = opts.pixelSizeX, yRes = opts.pixelSizeY;
//    int snappedX = static_cast<int>((xSize * geoTransformOut[1]) / xRes + 0.5);
//    int snappedY = static_cast<int>((ySize * std::fabs(geoTransformOut[5])) / yRes + 0.5);
//    if (snappedX > 0) xSize = snappedX;
//    if (snappedY > 0) ySize = snappedY;
//    geoTransformOut[1] = xRes;
//    geoTransformOut[5] = -yRes;

//    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
//    char** createOptions = nullptr;
//    createOptions = CSLSetNameValue(createOptions, "COMPRESS", "LZW");
//    GDALDataset* dst = driver->Create(outputPath.c_str(), xSize, ySize, src->GetRasterCount(), GDT_Float32, createOptions);
//    CSLDestroy(createOptions);
//    if (!dst) {
//        GDALClose(src);
//        throw HsiError("Orthorectifier: failed to create warped output '" + outputPath + "'");
//    }
//    dst->SetGeoTransform(geoTransformOut);
//    dst->SetProjection(dstWkt.c_str());

//    GDALWarpOptions* warpOpts = GDALCreateWarpOptions();
//    warpOpts->hSrcDS = src;
//    warpOpts->hDstDS = dst;
//    warpOpts->nBandCount = src->GetRasterCount();
//    warpOpts->panSrcBands = static_cast<int*>(CPLMalloc(sizeof(int) * warpOpts->nBandCount));
//    warpOpts->panDstBands = static_cast<int*>(CPLMalloc(sizeof(int) * warpOpts->nBandCount));
//    for (int i = 0; i < warpOpts->nBandCount; ++i) {
//        warpOpts->panSrcBands[i] = i + 1;
//        warpOpts->panDstBands[i] = i + 1;
//    }
//    warpOpts->eResampleAlg = resampleAlgFromString(opts.resampleAlg);
//    warpOpts->pTransformerArg = GDALCreateGenImgProjTransformer(src, nullptr, dst, nullptr, TRUE, 0, 1);
//    if (!warpOpts->pTransformerArg) {
//        GDALDestroyWarpOptions(warpOpts);
//        GDALClose(dst);
//        GDALClose(src);
//        throw HsiError("Orthorectifier: could not build the source->destination warp transformer for '" + inputPath + "'.");
//    }
//    warpOpts->pfnTransformer = GDALGenImgProjTransform;

//    GDALWarpOperation warper;
//    if (warper.Initialize(warpOpts) != CE_None || warper.ChunkAndWarpImage(0, 0, xSize, ySize) != CE_None) {
//        GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
//        GDALDestroyWarpOptions(warpOpts);
//        GDALClose(dst);
//        GDALClose(src);
//        throw HsiError("Orthorectifier: warp operation failed for '" + inputPath + "'");
//    }

//    GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
//    GDALDestroyWarpOptions(warpOpts);
//    GDALClose(dst);
//    GDALClose(src);

//    Logger::log("Orthorectifier", "Warped output written to '" + outputPath + "' (" +
//                                   std::to_string(xSize) + "x" + std::to_string(ySize) + ")");

//    outcome.wasAlreadyOrthorectified = false;
//    outcome.outputPath = outputPath;
//    return outcome;
//}

//} // namespace hsi


#include "hsi/Orthorectifier.h"
#include "hsi/RasterIO.h"
#include "hsi/Logger.h"

#include <gdal_priv.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <cpl_conv.h>
#include <cpl_string.h>

namespace hsi {

static GDALResampleAlg resampleAlgFromString(const std::string& s) {
    if (s == "nearest") return GRA_NearestNeighbour;
    if (s == "cubic") return GRA_Cubic;
    return GRA_Bilinear;
}

Orthorectifier::Outcome Orthorectifier::ensureOrthorectified(const std::string& inputPath,
                                                               const std::string& outputPath) {
    return ensureOrthorectified(inputPath, outputPath, Options());
}

Orthorectifier::Outcome Orthorectifier::ensureOrthorectified(const std::string& inputPath,
                                                               const std::string& outputPath,
                                                               const Options& opts) {
    RasterIO::init();
    Outcome outcome;

    if (RasterIO::looksOrthorectified(inputPath)) {
        Logger::log("Orthorectifier", "'" + inputPath + "' already has a valid geotransform/SRS and no GCPs "
                                       "-- treating as already orthorectified, skipping warp.");
        outcome.wasAlreadyOrthorectified = true;
        outcome.outputPath = inputPath;
        return outcome;
    }

    Logger::log("Orthorectifier", "'" + inputPath + "' is not orthorectified (raw GCPs or missing SRS) -- warping.");

    GDALDataset* src = static_cast<GDALDataset*>(GDALOpen(inputPath.c_str(), GA_ReadOnly));
    if (!src) throw HsiError("Orthorectifier: failed to open '" + inputPath + "'");

    std::string dstWkt = opts.targetSrsWkt;
    if (dstWkt.empty()) {
        // Fall back to the dataset's own projection if it has one, else WGS84.
        const char* wkt = src->GetProjectionRef();
        if (wkt && std::string(wkt).size() > 0) {
            dstWkt = wkt;
        } else {
            OGRSpatialReference srs;
            srs.SetWellKnownGeogCS("WGS84");
            char* wktOut = nullptr;
            srs.exportToWkt(&wktOut);
            dstWkt = wktOut;
            CPLFree(wktOut);
        }
    } else {
        // An explicit target CRS was given -- compare it against the
        // source's own CRS so a caller who passed the wrong target (e.g.
        // copy-pasted from a different scene) finds out from the log
        // instead of getting a silently-reprojected/misaligned output.
        const char* srcWktC = src->GetProjectionRef();
        if (srcWktC && std::string(srcWktC).size() > 0) {
            OGRSpatialReference srcSrs, dstSrs;
            char* srcWktMutable = const_cast<char*>(srcWktC);
            srcSrs.importFromWkt(&srcWktMutable);
            std::string dstWktCopy = opts.targetSrsWkt;
            char* dstWktMutable = const_cast<char*>(dstWktCopy.c_str());
            dstSrs.importFromWkt(&dstWktMutable);
            if (!srcSrs.IsSame(&dstSrs)) {
                Logger::log("Orthorectifier", "'" + inputPath + "' source CRS differs from the requested "
                            "target CRS -- this will reproject, not just resample. If that wasn't intended, "
                            "double-check the target CRS passed in.");
            }
        }
    }

    char* dstWktC = const_cast<char*>(dstWkt.c_str());

    void* transformerArg = GDALCreateGenImgProjTransformer(src, nullptr, nullptr, dstWktC, TRUE, 0, 1);
    if (!transformerArg) {
        GDALClose(src);
        throw HsiError("Orthorectifier: could not build a georeferencing transformer for '" + inputPath +
                        "' (no GCPs/RPCs found). Provide a pre-orthorectified product instead.");
    }

    double geoTransformOut[6];
    int xSize = 0, ySize = 0;
    CPLErr gerr = GDALSuggestedWarpOutput(src, GDALGenImgProjTransform, transformerArg, geoTransformOut, &xSize, &ySize);
    GDALDestroyGenImgProjTransformer(transformerArg);
    if (gerr != CE_None || xSize <= 0 || ySize <= 0) {
        GDALClose(src);
        throw HsiError("Orthorectifier: failed to compute warped output extent for '" + inputPath + "'");
    }

    // Snap to the requested output pixel size while keeping the same origin/extent.
    double xRes = opts.pixelSizeX, yRes = opts.pixelSizeY;
    int snappedX = static_cast<int>((xSize * geoTransformOut[1]) / xRes + 0.5);
    int snappedY = static_cast<int>((ySize * std::fabs(geoTransformOut[5])) / yRes + 0.5);
    if (snappedX > 0) xSize = snappedX;
    if (snappedY > 0) ySize = snappedY;
    geoTransformOut[1] = xRes;
    geoTransformOut[5] = -yRes;

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** createOptions = nullptr;
    createOptions = CSLSetNameValue(createOptions, "COMPRESS", "LZW");
    GDALDataset* dst = driver->Create(outputPath.c_str(), xSize, ySize, src->GetRasterCount(), GDT_Float32, createOptions);
    CSLDestroy(createOptions);
    if (!dst) {
        GDALClose(src);
        throw HsiError("Orthorectifier: failed to create warped output '" + outputPath + "'");
    }
    dst->SetGeoTransform(geoTransformOut);
    dst->SetProjection(dstWkt.c_str());

    GDALWarpOptions* warpOpts = GDALCreateWarpOptions();
    warpOpts->hSrcDS = src;
    warpOpts->hDstDS = dst;
    warpOpts->nBandCount = src->GetRasterCount();
    warpOpts->panSrcBands = static_cast<int*>(CPLMalloc(sizeof(int) * warpOpts->nBandCount));
    warpOpts->panDstBands = static_cast<int*>(CPLMalloc(sizeof(int) * warpOpts->nBandCount));
    for (int i = 0; i < warpOpts->nBandCount; ++i) {
        warpOpts->panSrcBands[i] = i + 1;
        warpOpts->panDstBands[i] = i + 1;
    }
    warpOpts->eResampleAlg = resampleAlgFromString(opts.resampleAlg);

    // The warp was previously fully single-threaded: GDAL's default is one
    // core, so on a large multi-band scene this call alone can dominate
    // Step 1's runtime. ChunkAndWarpImage already processes the raster in
    // tiles internally -- NUM_THREADS just lets it run those tiles across
    // cores instead of one at a time.
    warpOpts->papszWarpOptions = CSLSetNameValue(warpOpts->papszWarpOptions, "NUM_THREADS", "ALL_CPUS");

    warpOpts->pTransformerArg = GDALCreateGenImgProjTransformer(src, nullptr, dst, nullptr, TRUE, 0, 1);
    if (!warpOpts->pTransformerArg) {
        GDALDestroyWarpOptions(warpOpts);
        GDALClose(dst);
        GDALClose(src);
        throw HsiError("Orthorectifier: could not build the source->destination warp transformer for '" + inputPath + "'.");
    }
    warpOpts->pfnTransformer = GDALGenImgProjTransform;

    GDALWarpOperation warper;
    if (warper.Initialize(warpOpts) != CE_None || warper.ChunkAndWarpImage(0, 0, xSize, ySize) != CE_None) {
        GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
        GDALDestroyWarpOptions(warpOpts);
        GDALClose(dst);
        GDALClose(src);
        throw HsiError("Orthorectifier: warp operation failed for '" + inputPath + "'");
    }

    GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
    GDALDestroyWarpOptions(warpOpts);
    GDALClose(dst);
    GDALClose(src);

    Logger::log("Orthorectifier", "Warped output written to '" + outputPath + "' (" +
                                   std::to_string(xSize) + "x" + std::to_string(ySize) + ")");

    outcome.wasAlreadyOrthorectified = false;
    outcome.outputPath = outputPath;
    return outcome;
}

} // namespace hsi
