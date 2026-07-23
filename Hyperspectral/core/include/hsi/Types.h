#pragma once
// Core shared data types used across the hyperspectral pipeline.
// Kept GDAL/OpenCV-free so it can be included anywhere without pulling
// heavy headers into every translation unit.

#include <string>
#include <vector>
#include <map>
#include <array>
#include <stdexcept>
#include <cmath>

namespace hsi {

// Thrown by any core stage on unrecoverable error (bad file, shape mismatch, etc).
class HsiError : public std::runtime_error {
public:
    explicit HsiError(const std::string& msg) : std::runtime_error(msg) {}
};

// A multi-band raster held in memory, band-sequential (BSQ) layout:
// data[b * width * height + row * width + col]
// This is the single data structure passed between every pipeline stage.
struct RasterCube {
    int width = 0;
    int height = 0;
    int bands = 0;

    std::vector<float> data;          // size == bands*width*height
    std::array<double, 6> geoTransform{ {0, 1, 0, 0, 0, 1} };
    std::string projectionWkt;        // OGC WKT, empty if unknown
    std::vector<int> bandNumbers;     // original sensor band index per stored band (1-indexed), optional
    std::vector<std::string> bandNames; // human-readable label per band, optional
    float noDataValue = -9999.0f;
    bool hasNoData = false;

    void allocate(int w, int h, int b) {
        width = w; height = h; bands = b;
        data.assign(static_cast<size_t>(w) * h * b, 0.0f);
        bandNumbers.assign(b, 0);
        bandNames.assign(b, "");
    }

    inline float at(int band, int row, int col) const {
        return data[static_cast<size_t>(band) * width * height + static_cast<size_t>(row) * width + col];
    }
    inline float& at(int band, int row, int col) {
        return data[static_cast<size_t>(band) * width * height + static_cast<size_t>(row) * width + col];
    }

    // Spectrum (all bands) for a single pixel.
    std::vector<float> pixelSpectrum(int row, int col) const {
        std::vector<float> v(bands);
        for (int b = 0; b < bands; ++b) v[b] = at(b, row, col);
        return v;
    }

    // Width/height alone doesn't guarantee two rasters cover the same ground:
    // two scenes can both be 861x2961 and still have different origins or
    // pixel sizes (different geoTransform), in which case stacking them
    // band-for-band silently misaligns every pixel. Compare geoTransform
    // with a small tolerance (float roundtrips through GDAL/warp can differ
    // by a few ULPs even for "the same" grid).
    bool sameGridAs(const RasterCube& other) const {
        if (width != other.width || height != other.height) return false;
        for (int i = 0; i < 6; ++i) {
            if (std::abs(geoTransform[i] - other.geoTransform[i]) > 1e-6) return false;
        }
        return true;
    }

    size_t pixelCount() const { return static_cast<size_t>(width) * height; }
};

// Result of validating a binary surface/object mask against an independent
// reference layer (e.g. derived from EOS-04 SAR or an optical scene).
struct ValidationResult {
    long truePositive = 0;   // both say "object"
    long trueNegative = 0;   // both say "surface"
    long falsePositive = 0;  // predicted object, reference surface
    long falseNegative = 0;  // predicted surface, reference object

    double overallAgreement() const {
        long total = truePositive + trueNegative + falsePositive + falseNegative;
        return total == 0 ? 0.0 : static_cast<double>(truePositive + trueNegative) / total;
    }
    double iou() const {
        long denom = truePositive + falsePositive + falseNegative;
        return denom == 0 ? 0.0 : static_cast<double>(truePositive) / denom;
    }
    double precision() const {
        long denom = truePositive + falsePositive;
        return denom == 0 ? 0.0 : static_cast<double>(truePositive) / denom;
    }
    double recall() const {
        long denom = truePositive + falseNegative;
        return denom == 0 ? 0.0 : static_cast<double>(truePositive) / denom;
    }
};

// One class's mean reflectance signature, used by the spectral library / SAM.
struct SpectralSignature {
    std::string className;
    std::vector<double> meanReflectance; // length == number of bands it was built from
};

struct ChangeMatrixResult {
    std::vector<int> classIds;                 // class label values present in either date
    std::vector<std::string> classNames;        // optional, parallel to classIds
    std::vector<std::vector<long>> matrix;       // matrix[fromIdx][toIdx] = pixel count
    double pixelAreaSqMeters = 900.0;            // default 30m x 30m
};

} // namespace hsi
