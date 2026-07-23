#pragma once
#include <string>
#include <map>
#include <vector>
#include <utility>

namespace ui_util {

// Reads lines of "class_name,row,col" (one sample pixel per line, blank
// lines and a possible header skipped) into a class -> pixel-list map,
// the format SpectralLibrary::buildFromSamples / SVM training expect.
std::map<std::string, std::vector<std::pair<int, int>>> loadSampleCsv(const std::string& path);

// If `path` points to a .zip archive (a Hyperion/Sentinel-2/EOS-04 delivery
// is almost always shipped as one), resolves it to a raster GDAL can open
// directly, and returns that path -- so the rest of the app can hand it
// straight to RasterIO as normal. If `path` is not a .zip, returns it
// unchanged -- safe to call on every path a "Browse..." dialog hands back.
//
// Performance note: for per-band Hyperion archives (one TIFF per band),
// this reads the needed bands directly out of the zip in parallel via
// GDAL's /vsizip/ virtual filesystem -- no `unzip` subprocess, no full
// extraction to disk. For single-file archives, it falls back to a real
// extraction (some GDAL drivers need on-disk sidecar files).
//
// Throws hsi::HsiError if the zip can't be read, extraction fails (e.g.
// 'unzip' isn't installed, for the single-file fallback path), or no
// recognizable raster file is found inside it.
std::string resolveRasterPath(const std::string& path);

} // namespace ui_util
