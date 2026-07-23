#pragma once
#include "hsi/Types.h"

namespace hsi {

// Concatenates several co-registered RasterCubes band-wise, in the order
// given. Used to build the 204-band stack: 0-197 hyperspectral, 198-201
// Sentinel-2, 202-203 EOS-04 VV/VH.
class BandStacker {
public:
    struct Input {
        const RasterCube* cube;
        std::string label; // e.g. "hyperspectral", "sentinel2", "sar_vv_vh"
    };

    // All inputs must share width/height (resample them to a common grid first).
    static RasterCube stack(const std::vector<Input>& inputs);
};

} // namespace hsi
