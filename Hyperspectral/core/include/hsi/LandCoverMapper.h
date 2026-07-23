#pragma once
#include "hsi/Types.h"
#include "hsi/SpectralIndices.h"
#include "hsi/SpectralLibrary.h"
#include "hsi/SvmModel.h"
#include <map>

namespace hsi {

enum class LandClass : int {
    Unclassified = 0,
    BuiltUp      = 1,
    Forest       = 2,
    Vegetation   = 3,
    Water        = 4,
    BareSoil     = 5,
    Other        = 6
};

inline const char* landClassName(LandClass c) {
    switch (c) {
        case LandClass::BuiltUp:    return "Built-up";
        case LandClass::Forest:     return "Forest";
        case LandClass::Vegetation: return "Vegetation";
        case LandClass::Water:      return "Water/River";
        case LandClass::BareSoil:   return "Bare Soil";
        case LandClass::Other:      return "Other";
        default:                    return "Unclassified";
    }
}

// Moved outside LandCoverMapper to avoid GCC nested-class
// default-member-initializer issue with default function arguments.
struct LandCoverIndexThresholds {
    float ndviForest      =  0.40f;
    float ndviVegetation  =  0.20f;
    float ndwi            =  0.10f;
    float ndbi            =  0.00f;
    float bsi             =  0.00f;
};

class LandCoverMapper {
public:
    using IndexThresholds = LandCoverIndexThresholds;

    // ── Mode 1: index-based (zero training) ──────────────────────────────
    static RasterCube classifyByIndices(const RasterCube& cube,
                                         SpectralIndices::BandSet bands,
                                         const IndexThresholds& thresholds,
                                         bool autoDetect = true);

    // Convenience overload with default thresholds.
    static RasterCube classifyByIndices(const RasterCube& cube);

    // ── Mode 2: SAM library (zero training) ──────────────────────────────
    static RasterCube classifyByLibrary(const RasterCube& cube,
                                         const SpectralLibrary& library,
                                         double angleThresholdRad = 0.20);

    // ── Mode 3: SVM click-to-train (offline) ─────────────────────────────
    struct SvmResult {
        RasterCube classified;
        SvmModel   model;
    };
    static SvmResult classifyBySvm(const RasterCube& cube,
                                    const std::map<LandClass, std::vector<std::pair<int,int>>>& classSamples,
                                    const SvmModel::Options& svmOpts);

    static SvmResult classifyBySvm(const RasterCube& cube,
                                    const std::map<LandClass, std::vector<std::pair<int,int>>>& classSamples);

    // ── Combine two results ───────────────────────────────────────────────
    static RasterCube combine(const RasterCube& primary, const RasterCube& fallback);
};

} // namespace hsi
