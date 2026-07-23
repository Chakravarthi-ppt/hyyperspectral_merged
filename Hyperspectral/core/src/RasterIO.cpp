#include "hsi/RasterIO.h"
#include "hsi/Logger.h"

#include <gdal_priv.h>
#include <gdalwarper.h>
#include <cpl_conv.h>
#include <cpl_vsi.h>
#include <cmath>
#include <cstring>
#include <string>

namespace hsi {

void RasterIO::init() {
    static bool done = false;
    if (!done) {
        GDALAllRegister();
        done = true;
    }
}

RasterCube RasterIO::loadCube(const std::string& path, const std::vector<int>& bandSubset) {
    init();

    // Use GDAL's VSI-aware stat instead of the plain POSIX stat(). The
    // POSIX call only understands real filesystem paths, so it always
    // fails (falsely reporting "does not exist") for GDAL virtual paths
    // like /vsizip/archive.zip/inner_file.tif, /vsicurl/..., etc. --
    // even though the file is perfectly readable through GDAL's own VSI
    // layer. That false negative is what broke loading per-band files
    // straight out of a zip (e.g. EO-1 Hyperion L1T archives): every
    // single /vsizip/ band path was rejected here before GDALOpen ever
    // got a chance to try it. VSIStatL() dispatches to the right handler
    // (disk, zip, curl, ...) based on the path prefix, so it works for
    // both plain paths and virtual ones.
    VSIStatBufL st;
    if (VSIStatL(path.c_str(), &st) != 0) {
        throw HsiError("RasterIO: '" + path + "' does not exist or is not accessible.");
    }

    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!ds) {
        // File exists (checked above) but GDAL still couldn't open it --
        // most likely an unsupported/unrecognized format or a corrupted
        // file, as distinct from a missing path.
        throw HsiError("RasterIO: '" + path + "' exists but could not be opened by GDAL -- "
                                              "unsupported or corrupted raster format.");
    }

    int totalBands = ds->GetRasterCount();
    int rw = ds->GetRasterXSize();
    int rh = ds->GetRasterYSize();
    if (rw <= 0 || rh <= 0 || totalBands <= 0) {
        GDALClose(ds);
        throw HsiError("RasterIO: '" + path + "' is empty or unreadable (" + std::to_string(rw) + "x" +
                       std::to_string(rh) + ", " + std::to_string(totalBands) + " bands).");
    }
    std::vector<int> bandsToRead = bandSubset;
    if (bandsToRead.empty()) {
        bandsToRead.resize(totalBands);
        for (int i = 0; i < totalBands; ++i) bandsToRead[i] = i + 1;
    }
    for (int b : bandsToRead) {
        if (b < 1 || b > totalBands) {
            GDALClose(ds);
            throw HsiError("RasterIO: requested band " + std::to_string(b) + " out of range for '" + path +
                           "' (file has " + std::to_string(totalBands) + " bands).");
        }
    }

    RasterCube cube;
    int w = ds->GetRasterXSize();
    int h = ds->GetRasterYSize();
    cube.allocate(w, h, static_cast<int>(bandsToRead.size()));

    double gt[6];
    if (ds->GetGeoTransform(gt) == CE_None) {
        for (int i = 0; i < 6; ++i) cube.geoTransform[i] = gt[i];
    }
    const char* wkt = ds->GetProjectionRef();
    if (wkt) cube.projectionWkt = wkt;

    for (size_t i = 0; i < bandsToRead.size(); ++i) {
        GDALRasterBand* band = ds->GetRasterBand(bandsToRead[i]);
        if (!band) {
            GDALClose(ds);
            throw HsiError("RasterIO: band " + std::to_string(bandsToRead[i]) + " not found in '" + path + "'");
        }
        // Single bulk read straight into this band's contiguous slice of
        // cube.data (band-major layout: band*w*h + row*w + col) instead of
        // one GDAL RasterIO call per row. For a w x h scene that's one call
        // instead of h calls -- the per-row version was the dominant cost
        // when this ran across ~198 separate single-band files during a
        // per-band-archive merge (each file paid its own h-row overhead).
        float* dst = &cube.data[static_cast<size_t>(i) * w * h];
        CPLErr err = band->RasterIO(GF_Read, 0, 0, w, h, dst, w, h, GDT_Float32, 0, 0);
        if (err != CE_None) {
            GDALClose(ds);
            throw HsiError("RasterIO: read error in '" + path + "' band " + std::to_string(bandsToRead[i]));
        }
        cube.bandNumbers[i] = bandsToRead[i];
        int hasNd = 0;
        double nd = band->GetNoDataValue(&hasNd);
        if (hasNd) { cube.hasNoData = true; cube.noDataValue = static_cast<float>(nd); }

        const char* desc = band->GetDescription();
        if (desc && desc[0] != '\0') {
            std::string descStr(desc);
            cube.bandNames[i] = descStr;
            if (descStr.rfind(kSensorBandTag, 0) == 0) {
                try {
                    cube.bandNumbers[i] = std::stoi(descStr.substr(std::strlen(kSensorBandTag)));
                } catch (...) {
                    // Malformed tag value -- keep the positional index set above.
                }
            }
        }
    }

    GDALClose(ds);
    Logger::log("RasterIO", "Loaded '" + path + "' (" + std::to_string(w) + "x" + std::to_string(h) +
                ", " + std::to_string(cube.bands) + " bands)");
    return cube;
}

void RasterIO::saveCube(const RasterCube& cube, const std::string& path, const std::string& driverName) {
    init();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.c_str());
    if (!driver) {
        throw HsiError("RasterIO: unknown GDAL driver '" + driverName + "'");
    }

    char** createOptions = nullptr;
    if (driverName == "GTiff") {
        createOptions = CSLSetNameValue(createOptions, "COMPRESS", "LZW");
        createOptions = CSLSetNameValue(createOptions, "TILED", "YES");
        // A ~200-band Hyperion stack at float32 blows past the classic TIFF
        // 4GB file-size limit (width * height * bands * 4 bytes). Without
        // this, GDAL fails partway through writing -- "TIFFAppendToStrip:
        // Maximum TIFF file size exceeded" -- silently corrupting/
        // truncating the file (later bands end up missing or garbage)
        // instead of raising a clean, catchable error up front. IF_SAFER
        // picks classic TIFF when it safely fits and BigTIFF only when
        // needed, so small single/3-band outputs (previews, FCC composites)
        // stay maximally compatible.
        createOptions = CSLSetNameValue(createOptions, "BIGTIFF", "IF_SAFER");
    }

    GDALDataset* ds = driver->Create(path.c_str(), cube.width, cube.height, cube.bands, GDT_Float32, createOptions);
    CSLDestroy(createOptions);
    if (!ds) {
        throw HsiError("RasterIO: failed to create '" + path + "'");
    }

    ds->SetGeoTransform(const_cast<double*>(cube.geoTransform.data()));
    if (!cube.projectionWkt.empty()) ds->SetProjection(cube.projectionWkt.c_str());

    for (int b = 0; b < cube.bands; ++b) {
        GDALRasterBand* band = ds->GetRasterBand(b + 1);
        if (cube.hasNoData) band->SetNoDataValue(cube.noDataValue);
        if (!cube.bandNames.empty() && !cube.bandNames[b].empty()) band->SetDescription(cube.bandNames[b].c_str());
        // Single bulk write straight from this band's contiguous slice of
        // cube.data (band-major layout) instead of copying through a
        // per-row buffer with one GDAL call per row. One call instead of
        // `height` calls per band -- this was the other half of the
        // per-band-merge slowdown (198 bands each paying full per-row
        // write overhead when the merged stack was saved to disk).
        const float* src = &cube.data[static_cast<size_t>(b) * cube.width * cube.height];
        CPLErr err = band->RasterIO(GF_Write, 0, 0, cube.width, cube.height,
                                    const_cast<float*>(src), cube.width, cube.height,
                                    GDT_Float32, 0, 0);
        if (err != CE_None) {
            GDALClose(ds);
            throw HsiError("RasterIO: write error in '" + path + "'");
        }
    }

    GDALClose(ds);
    Logger::log("RasterIO", "Saved '" + path + "' (" + std::to_string(cube.width) + "x" +
                std::to_string(cube.height) + ", " + std::to_string(cube.bands) + " bands)");
}

bool RasterIO::looksOrthorectified(const std::string& path) {
    init();
    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!ds) throw HsiError("RasterIO: failed to open '" + path + "'");

    double gt[6];
    bool hasGt = (ds->GetGeoTransform(gt) == CE_None);
    bool identityGt = hasGt && std::fabs(gt[0]) < 1e-9 && std::fabs(gt[1] - 1.0) < 1e-9 &&
            std::fabs(gt[3]) < 1e-9 && std::fabs(gt[5] + 1.0) < 1e-9 &&
            std::fabs(gt[2]) < 1e-9 && std::fabs(gt[4]) < 1e-9;
    const char* wkt = ds->GetProjectionRef();
    bool hasSrs = wkt && std::string(wkt).size() > 0;
    int gcpCount = ds->GetGCPCount();

    GDALClose(ds);
    // Already orthorectified if it carries a real (non-identity) geotransform
    // and a spatial reference, and is NOT just raw GCPs waiting to be warped.
    return hasGt && !identityGt && hasSrs && gcpCount == 0;
}

bool RasterIO::hasGroundControlPoints(const std::string& path) {
    init();
    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!ds) throw HsiError("RasterIO: failed to open '" + path + "'");
    int gcpCount = ds->GetGCPCount();
    GDALClose(ds);
    return gcpCount > 0;
}

void RasterIO::pixelSize(const std::string& path, double& outX, double& outY) {
    init();
    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!ds) throw HsiError("RasterIO: failed to open '" + path + "'");
    double gt[6];
    outX = outY = 0.0;
    if (ds->GetGeoTransform(gt) == CE_None) {
        outX = std::fabs(gt[1]);
        outY = std::fabs(gt[5]);
    }
    GDALClose(ds);
}

namespace {
GDALResampleAlg resampleAlgFromStringRasterIO(const std::string& s) {
    if (s == "nearest") return GRA_NearestNeighbour;
    if (s == "cubic") return GRA_Cubic;
    return GRA_Bilinear;
}
}

RasterCube RasterIO::resampleToGrid(const RasterCube& src, const RasterCube& referenceGrid,
                                    const std::string& resampleAlg) {
    init();

    if (src.projectionWkt.empty() || referenceGrid.projectionWkt.empty()) {
        throw HsiError("RasterIO::resampleToGrid: both rasters need a known projection to resample "
                       "between grids -- got an empty projection on one or both.");
    }

    GDALDriver* memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!memDriver) throw HsiError("RasterIO::resampleToGrid: GDAL's in-memory (MEM) driver is unavailable.");

    // --- Source: an in-memory GDAL dataset built from `src`'s own pixels ---
    GDALDataset* srcDs = memDriver->Create("", src.width, src.height, src.bands, GDT_Float32, nullptr);
    if (!srcDs) throw HsiError("RasterIO::resampleToGrid: could not create the in-memory source dataset.");
    double srcGt[6];
    for (int i = 0; i < 6; ++i) srcGt[i] = src.geoTransform[i];
    srcDs->SetGeoTransform(srcGt);
    srcDs->SetProjection(src.projectionWkt.c_str());
    for (int b = 0; b < src.bands; ++b) {
        size_t base = static_cast<size_t>(b) * src.pixelCount();
        CPLErr err = srcDs->GetRasterBand(b + 1)->RasterIO(
                    GF_Write, 0, 0, src.width, src.height,
                    const_cast<float*>(src.data.data() + base), src.width, src.height, GDT_Float32, 0, 0);
        if (err != CE_None) {
            GDALClose(srcDs);
            throw HsiError("RasterIO::resampleToGrid: failed writing band " + std::to_string(b + 1) +
                           " into the in-memory source dataset.");
        }
    }

    // --- Destination: an in-memory dataset on referenceGrid's exact grid ---
    GDALDataset* dstDs = memDriver->Create("", referenceGrid.width, referenceGrid.height, src.bands, GDT_Float32, nullptr);
    if (!dstDs) {
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleToGrid: could not create the in-memory destination dataset.");
    }
    double dstGt[6];
    for (int i = 0; i < 6; ++i) dstGt[i] = referenceGrid.geoTransform[i];
    dstDs->SetGeoTransform(dstGt);
    dstDs->SetProjection(referenceGrid.projectionWkt.c_str());

    // --- Warp src -> dst ---
    GDALWarpOptions* warpOpts = GDALCreateWarpOptions();
    warpOpts->hSrcDS = srcDs;
    warpOpts->hDstDS = dstDs;
    warpOpts->nBandCount = src.bands;
    warpOpts->panSrcBands = static_cast<int*>(CPLMalloc(sizeof(int) * src.bands));
    warpOpts->panDstBands = static_cast<int*>(CPLMalloc(sizeof(int) * src.bands));
    for (int i = 0; i < src.bands; ++i) {
        warpOpts->panSrcBands[i] = i + 1;
        warpOpts->panDstBands[i] = i + 1;
    }
    warpOpts->eResampleAlg = resampleAlgFromStringRasterIO(resampleAlg);
    warpOpts->pTransformerArg = GDALCreateGenImgProjTransformer(srcDs, nullptr, dstDs, nullptr, TRUE, 0, 1);
    if (!warpOpts->pTransformerArg) {
        GDALDestroyWarpOptions(warpOpts);
        GDALClose(dstDs);
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleToGrid: could not build a warp transformer between the two "
                       "rasters' projections -- check that both have valid, compatible spatial references.");
    }
    warpOpts->pfnTransformer = GDALGenImgProjTransform;

    GDALWarpOperation warper;
    bool ok = (warper.Initialize(warpOpts) == CE_None &&
               warper.ChunkAndWarpImage(0, 0, referenceGrid.width, referenceGrid.height) == CE_None);

    GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
    GDALDestroyWarpOptions(warpOpts);

    if (!ok) {
        GDALClose(dstDs);
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleToGrid: the warp operation itself failed.");
    }

    RasterCube out;
    out.allocate(referenceGrid.width, referenceGrid.height, src.bands);
    out.geoTransform = referenceGrid.geoTransform;
    out.projectionWkt = referenceGrid.projectionWkt;
    out.bandNumbers = src.bandNumbers;
    out.bandNames = src.bandNames;
    for (int b = 0; b < src.bands; ++b) {
        size_t base = static_cast<size_t>(b) * out.pixelCount();
        CPLErr err = dstDs->GetRasterBand(b + 1)->RasterIO(
                    GF_Read, 0, 0, out.width, out.height,
                    out.data.data() + base, out.width, out.height, GDT_Float32, 0, 0);
        if (err != CE_None) {
            GDALClose(dstDs);
            GDALClose(srcDs);
            throw HsiError("RasterIO::resampleToGrid: failed reading band " + std::to_string(b + 1) +
                           " back from the warped destination dataset.");
        }
    }

    GDALClose(dstDs);
    GDALClose(srcDs);

    Logger::log("RasterIO", "Resampled " + std::to_string(src.width) + "x" + std::to_string(src.height) +
                " -> " + std::to_string(referenceGrid.width) + "x" + std::to_string(referenceGrid.height) +
                " to match the reference grid (" + resampleAlg + " resampling).");
    return out;
}

void RasterIO::datasetSize(const std::string& path, int& outWidth, int& outHeight, int& outBands) {
    init();
    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!ds) throw HsiError("RasterIO::datasetSize: failed to open '" + path + "'");
    outWidth = ds->GetRasterXSize();
    outHeight = ds->GetRasterYSize();
    outBands = ds->GetRasterCount();
    GDALClose(ds);
}

RasterCube RasterIO::resampleFileToGrid(const std::string& path, const RasterCube& referenceGrid,
                                        const std::string& resampleAlg) {
    init();

    if (referenceGrid.projectionWkt.empty()) {
        throw HsiError("RasterIO::resampleFileToGrid: the reference grid needs a known projection to "
                       "resample onto -- got an empty projection.");
    }

    GDALDataset* srcDs = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!srcDs) throw HsiError("RasterIO::resampleFileToGrid: failed to open '" + path + "'");

    const char* srcWkt = srcDs->GetProjectionRef();
    if (!srcWkt || std::string(srcWkt).empty()) {
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleFileToGrid: '" + path + "' has no known projection to "
                                                                  "resample from -- got an empty projection.");
    }

    int nBands = srcDs->GetRasterCount();

    GDALDriver* memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!memDriver) {
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleFileToGrid: GDAL's in-memory (MEM) driver is unavailable.");
    }

    // Destination sized only to referenceGrid -- never to the source file's
    // own (potentially much larger) resolution.
    GDALDataset* dstDs = memDriver->Create("", referenceGrid.width, referenceGrid.height, nBands, GDT_Float32, nullptr);
    if (!dstDs) {
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleFileToGrid: could not create the in-memory destination dataset.");
    }
    double dstGt[6];
    for (int i = 0; i < 6; ++i) dstGt[i] = referenceGrid.geoTransform[i];
    dstDs->SetGeoTransform(dstGt);
    dstDs->SetProjection(referenceGrid.projectionWkt.c_str());

    GDALWarpOptions* warpOpts = GDALCreateWarpOptions();
    warpOpts->hSrcDS = srcDs;
    warpOpts->hDstDS = dstDs;
    warpOpts->nBandCount = nBands;
    warpOpts->panSrcBands = static_cast<int*>(CPLMalloc(sizeof(int) * nBands));
    warpOpts->panDstBands = static_cast<int*>(CPLMalloc(sizeof(int) * nBands));
    for (int i = 0; i < nBands; ++i) {
        warpOpts->panSrcBands[i] = i + 1;
        warpOpts->panDstBands[i] = i + 1;
    }
    warpOpts->eResampleAlg = resampleAlgFromStringRasterIO(resampleAlg);
    warpOpts->pTransformerArg = GDALCreateGenImgProjTransformer(srcDs, nullptr, dstDs, nullptr, TRUE, 0, 1);
    if (!warpOpts->pTransformerArg) {
        GDALDestroyWarpOptions(warpOpts);
        GDALClose(dstDs);
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleFileToGrid: could not build a warp transformer between '" +
                       path + "' and the reference grid -- check that both have valid, compatible "
                              "spatial references.");
    }
    warpOpts->pfnTransformer = GDALGenImgProjTransform;

    GDALWarpOperation warper;
    bool ok = (warper.Initialize(warpOpts) == CE_None &&
               warper.ChunkAndWarpImage(0, 0, referenceGrid.width, referenceGrid.height) == CE_None);

    GDALDestroyGenImgProjTransformer(warpOpts->pTransformerArg);
    GDALDestroyWarpOptions(warpOpts);

    if (!ok) {
        GDALClose(dstDs);
        GDALClose(srcDs);
        throw HsiError("RasterIO::resampleFileToGrid: the warp operation itself failed.");
    }

    RasterCube out;
    out.allocate(referenceGrid.width, referenceGrid.height, nBands);
    out.geoTransform = referenceGrid.geoTransform;
    out.projectionWkt = referenceGrid.projectionWkt;
    for (int b = 0; b < nBands; ++b) {
        size_t base = static_cast<size_t>(b) * out.pixelCount();
        CPLErr err = dstDs->GetRasterBand(b + 1)->RasterIO(
                    GF_Read, 0, 0, out.width, out.height,
                    out.data.data() + base, out.width, out.height, GDT_Float32, 0, 0);
        if (err != CE_None) {
            GDALClose(dstDs);
            GDALClose(srcDs);
            throw HsiError("RasterIO::resampleFileToGrid: failed reading band " + std::to_string(b + 1) +
                           " back from the warped destination dataset.");
        }
    }

    GDALClose(dstDs);
    GDALClose(srcDs);

    Logger::log("RasterIO", "Resampled '" + path + "' -> " + std::to_string(referenceGrid.width) + "x" +
                std::to_string(referenceGrid.height) + " to match the reference grid (" + resampleAlg +
                " resampling), streamed directly from disk to keep memory bounded.");
    return out;
}
}// namespace hsi
