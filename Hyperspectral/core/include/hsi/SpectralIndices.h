#pragma once
#include "hsi/Types.h"

namespace hsi {

// Computes standard spectral indices that map directly onto named land-cover
// classes without any training. Each index returns a single-band float raster
// in the range -1..+1 (or wider for EVI). Thresholding these produces
// binary class masks; combining them produces a multi-class map.
class SpectralIndices {
public:
    struct BandSet {
        int blue  = -1;  // ~450-490 nm  Hyperion sensor band ~8-10
        int green = -1;  // ~530-570 nm  Hyperion sensor band ~16-18
        int red   = -1;  // ~630-670 nm  Hyperion sensor band ~25-27
        int nir   = -1;  // ~850-880 nm  Hyperion sensor band ~36-40
        int swir1 = -1;  // ~1550-1650 nm Hyperion sensor band ~95-100
        int swir2 = -1;  // ~2100-2200 nm Hyperion sensor band ~155-160
    };

    // Per-class band pairs used by the Classification step. Each land class
    // uses its own SWIR/NIR (or Red/NIR) pair instead of one shared BandSet,
    // per the target wavelengths supplied by the project spec:
    //   Urban (Built-up):        SWIR ~1590 nm, NIR ~860 nm (+/-5 nm)
    //   Barren land (Bare Soil): SWIR ~1620 nm, NIR ~860 nm (+/-5 nm)
    //   Soil/Forest/Vegetation:  Red  ~660-665 nm, NIR ~815 nm
    struct ClassBandSet {
        int urbanSwir  = -1;
        int urbanNir   = -1;
        int barrenSwir = -1;
        int barrenNir  = -1;
        int vegRed     = -1;
        int vegNir     = -1;
    };

    // NDVI > 0.3 => forest / vegetation
    static RasterCube ndvi(const RasterCube& cube, int nirIdx, int redIdx);
    // NDWI > 0.1 => river / open water
    static RasterCube ndwi(const RasterCube& cube, int greenIdx, int nirIdx);
    // NDBI > 0.0 => built-up
    static RasterCube ndbi(const RasterCube& cube, int swir1Idx, int nirIdx);
    // BSI  > 0.0 => bare soil / sand
    static RasterCube bsi(const RasterCube& cube, int swir1Idx, int redIdx, int nirIdx, int blueIdx);
    // EVI  > 0.3 => dense forest (less saturation than NDVI)
    static RasterCube evi(const RasterCube& cube, int nirIdx, int redIdx, int blueIdx,
                           double G = 2.5, double C1 = 6.0, double C2 = 7.5, double L = 1.0);

    // Compute all available indices from a BandSet; returns a multi-band cube
    // with bandNames set to index names ("NDVI", "NDWI", …).
    static RasterCube computeAll(const RasterCube& cube, const BandSet& bands);

    // Single-band index raster + threshold -> binary 0/1 mask.
    static RasterCube threshold(const RasterCube& indexBand, float thresh, bool aboveIsOne = true);

    // Merge multiple binary class masks into one integer label raster.
    // masks[i].first = integer class label (1=forest, 2=water, 3=built-up, ...)
    // Pixels matching multiple masks get the label of the first match.
    // Unmatched pixels get label 0 ("unclassified").
    static RasterCube mergeClassMasks(
        const std::vector<std::pair<int, RasterCube>>& masks,
        int width, int height,
        const std::array<double, 6>& geoTransform,
        const std::string& projectionWkt);
};

// Finds the 0-based in-cube index of the Hyperion band closest to a target
// wavelength (nm), using bandNumbers[] to do the sensor-band->wavelength
// mapping. Used to auto-fill a BandSet without user input.
class HyperionBandFinder {
public:
    static int findClosest(const RasterCube& cube, double targetNm);
    static SpectralIndices::BandSet autoDetect(const RasterCube& cube);

    // Resolves the per-class band pairs (see SpectralIndices::ClassBandSet)
    // used by the Classification step. Logs a warning (via Logger) for any
    // pair whose closest available sensor band misses its target wavelength
    // by more than toleranceNm, since the caller asked for a specific band
    // (+/-5 nm) and the sensor may not have one that close.
    static SpectralIndices::ClassBandSet autoDetectClassBands(const RasterCube& cube,
                                                               double toleranceNm = 5.0);
};

} // namespace hsi
