#pragma once
#include "hsi/SpectralLibrary.h"

namespace hsi {

// A compact representative spectral signature table built into the binary.
// Reflectance values are average USGS/ASTER library means sampled at
// Hyperion-equivalent band centres (357nm-2576nm, 10nm steps, 198 calibrated bands).
//
// These let SAM run with ZERO user labelling for a first-pass land-cover map.
// The user can then add scene-specific sample pixels on top (which override
// the built-in entries for that class) using SpectralLibrary::buildFromSamples.
//
// Classes provided:
//   dense_vegetation  — healthy broadleaf forest / mangrove
//   sparse_vegetation — dry grass / coastal scrub
//   turbid_water      — coastal / harbour water (moderate sediment)
//   clear_water       — open ocean / clean river
//   urban_concrete    — concrete roads, building rooftops
//   urban_metal       — metal roofs, ship decks (maritime relevance)
//   bare_soil_dry     — dry sandy / loam soil, beach
//   bare_soil_wet     — wet sediment, mudflat
//
// Usage:
//   SpectralLibrary lib = BuiltinSignatures::library();
//   RasterCube result   = SamClassifier::classify(cube, lib, 0.15);
class BuiltinSignatures {
public:
    // Returns a SpectralLibrary populated with the 8 built-in class signatures.
    // The signatures have 198 bands matching a preprocessed Hyperion reflectance cube.
    static SpectralLibrary library();
};

} // namespace hsi
