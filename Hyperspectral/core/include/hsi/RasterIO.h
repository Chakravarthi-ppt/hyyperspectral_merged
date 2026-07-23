#pragma once
#include "hsi/Types.h"

namespace hsi {

// Special band-description tag RasterIO::loadCube recognizes: if a band's
// GDAL description is exactly "HSI_SENSOR_BAND=<n>", bandNumbers[i] is set
// to <n> instead of this file's own positional index within the file.
// Written by code that merges several single-band files back into one
// cube (e.g. a USGS EO-1 Hyperion L1T delivery, which ships one TIFF per
// band rather than a combined multi-band file) so the true sensor band
// number survives a save-to-disk/reload round trip instead of collapsing
// back to a plain 1..N sequence. Chosen to be unique enough that it will
// never collide with descriptions written by other tools (ENVI, etc.),
// so files without this exact tag behave exactly as before.
inline const char* const kSensorBandTag = "HSI_SENSOR_BAND=";

// All GDAL access goes through this class so the rest of the codebase
// never includes <gdal_priv.h> directly.
class RasterIO {
public:
    // Registers GDAL drivers exactly once. Safe to call repeatedly.
    static void init();

    // Loads every raster band of `path` into memory as float32.
    // bandSubset: optional 1-indexed list of bands to load (empty = all).
    static RasterCube loadCube(const std::string& path, const std::vector<int>& bandSubset = {});

    // Writes `cube` to disk. driverName: "GTiff" (default), "ENVI", etc.
    static void saveCube(const RasterCube& cube, const std::string& path, const std::string& driverName = "GTiff");

    // True if the dataset carries a non-trivial geotransform AND a spatial
    // reference -- i.e. it looks like it has already been orthorectified.
    static bool looksOrthorectified(const std::string& path);

    // True if the dataset instead carries ground control points (raw,
    // not yet rectified to a map grid).
    static bool hasGroundControlPoints(const std::string& path);

    // Pixel size in projected units (e.g. metres), 0 if unavailable.
    static void pixelSize(const std::string& path, double& outX, double& outY);

    // Reprojects/resamples `src` onto the *exact* pixel grid of
    // `referenceGrid` (same width, height, geotransform, and projection) --
    // for combining two teams' rasters that cover the same area at
    // different resolutions or projections. Always resamples `src` (not
    // the reference), so pick the lower-resolution raster you actually
    // want to analyze at as the reference: upsampling a hyperspectral cube
    // to match a much higher-resolution optical/SAR product would just
    // interpolate fabricated values into every new pixel, not add real
    // information, while massively inflating memory/compute for no
    // scientific benefit. Both rasters must already have a known
    // projection (throws otherwise -- there is nothing to align without one).
    static RasterCube resampleToGrid(const RasterCube& src, const RasterCube& referenceGrid,
                                      const std::string& resampleAlg = "bilinear");

    // Same result as loadCube(path) + resampleToGrid(...), but for a source
    // file whose full resolution is much larger than referenceGrid (e.g. a
    // fusion team's 10-20m product being matched down to a 30m Hyperion
    // grid). loadCube()+resampleToGrid() allocates the entire source raster
    // twice at full resolution before ever shrinking it -- easily tens of GB
    // for a large fusion product, which can crash the process. This warps
    // straight from the on-disk file into a buffer already sized to
    // referenceGrid instead, so peak memory tracks the (much smaller)
    // output size. Prefer this whenever the source isn't already loaded.
    static RasterCube resampleFileToGrid(const std::string& path, const RasterCube& referenceGrid,
                                          const std::string& resampleAlg = "bilinear");

    // Reads just width/height/band-count from a file's header, without
    // loading any pixel data -- cheap enough to call before deciding whether
    // a full loadCube() is memory-safe.
    static void datasetSize(const std::string& path, int& outWidth, int& outHeight, int& outBands);
};

} // namespace hsi
