#include "hsi/BandStacker.h"
#include "hsi/Logger.h"
#include <algorithm>

namespace hsi {

RasterCube BandStacker::stack(const std::vector<Input>& inputs) {
    if (inputs.empty()) throw HsiError("BandStacker: no inputs given.");

    const RasterCube* first = inputs[0].cube;
    int totalBands = 0;
    for (const auto& in : inputs) {
        if (!in.cube) throw HsiError("BandStacker: null cube in input '" + in.label + "'.");
        if (!in.cube->sameGridAs(*first)) {
            throw HsiError("BandStacker: '" + in.label + "' (" + std::to_string(in.cube->width) + "x" +
                            std::to_string(in.cube->height) + ") does not match the grid of '" + inputs[0].label +
                            "' (" + std::to_string(first->width) + "x" + std::to_string(first->height) +
                            "). Resample it first.");
        }
        // WKT strings can differ cosmetically for the same CRS (missing
        // AUTHORITY node, different axis order notation, etc.), so this is a
        // warning rather than a hard failure -- but a mismatch here is worth
        // a human's attention, since geoTransform alone can't catch "same
        // pixel grid, different real-world CRS" (e.g. an unprojected vs.
        // projected raster that happen to share a transform numerically).
        if (!in.cube->projectionWkt.empty() && !first->projectionWkt.empty() &&
            in.cube->projectionWkt != first->projectionWkt) {
            Logger::log("BandStacker", "WARNING: '" + in.label + "' has a different CRS/projection string than '" +
                        inputs[0].label + "'. Their pixel grids line up numerically, but double-check they are "
                        "really in the same spatial reference before trusting this stack.");
        }
        totalBands += in.cube->bands;
    }

    RasterCube out;
    out.allocate(first->width, first->height, totalBands);
    out.geoTransform = first->geoTransform;
    out.projectionWkt = first->projectionWkt;

    // Inputs can each have their own (or no) NoData value -- e.g. the
    // Hyperion cube uses 0.0f for its background swath border while a fused
    // SAR/optical layer might use -9999 or nothing at all. Silently dropping
    // this (as before) meant the stacked cube never flagged background
    // pixels for anything downstream that checks hasNoData. Standardize on
    // 0.0f (already the de-facto background value used by LulcClassifier's
    // and LandCoverMapper's own near-zero background heuristics) and flag
    // hasNoData if *any* input had NoData set, rather than requiring all of
    // them to agree on the same literal value.
    bool anyHasNoData = false;
    for (const auto& in : inputs) {
        if (in.cube->hasNoData && in.cube->noDataValue != 0.0f) {
            Logger::log("BandStacker", "'" + in.label + "' uses NoData=" + std::to_string(in.cube->noDataValue) +
                        "; standardizing the stacked output on NoData=0.0.");
        }
        anyHasNoData = anyHasNoData || in.cube->hasNoData;
    }
    out.hasNoData = anyHasNoData;
    out.noDataValue = 0.0f;

    int dstBand = 0;
    for (const auto& in : inputs) {
        for (int b = 0; b < in.cube->bands; ++b, ++dstBand) {
            size_t srcBase = static_cast<size_t>(b) * in.cube->width * in.cube->height;
            size_t dstBase = static_cast<size_t>(dstBand) * out.width * out.height;
            std::copy(in.cube->data.begin() + srcBase, in.cube->data.begin() + srcBase + in.cube->pixelCount(),
                      out.data.begin() + dstBase);
            out.bandNames[dstBand] = in.label + ":" +
                (in.cube->bandNames.size() > static_cast<size_t>(b) && !in.cube->bandNames[b].empty()
                     ? in.cube->bandNames[b] : ("band" + std::to_string(b + 1)));
        }
    }

    Logger::log("BandStacker", "Stacked " + std::to_string(inputs.size()) + " input(s) into " +
                std::to_string(totalBands) + " total bands.");
    return out;
}

} // namespace hsi
