#include "hsi/SpectralLibrary.h"
#include "hsi/Logger.h"

#include <fstream>
#include <sstream>

namespace hsi {

SpectralLibrary SpectralLibrary::buildFromSamples(const RasterCube& cube,
                                                    const std::map<std::string, std::vector<std::pair<int, int>>>& classSamplePixels) {
    SpectralLibrary lib;
    int totalSkipped = 0;
    for (const auto& entry : classSamplePixels) {
        const std::string& className = entry.first;
        const auto& pixels = entry.second;
        if (pixels.empty()) continue;

        SpectralSignature sig;
        sig.className = className;
        sig.meanReflectance.assign(cube.bands, 0.0);
        int validCount = 0;
        for (const auto& rc : pixels) {
            int row = rc.first, col = rc.second;
            // BUG6: validate bounds before accessing pixel data
            if (row < 0 || row >= cube.height || col < 0 || col >= cube.width) {
                ++totalSkipped;
                Logger::log("SpectralLibrary",
                    "WARNING: sample for class '" + className + "' at (" +
                    std::to_string(row) + "," + std::to_string(col) +
                    ") is outside scene (" + std::to_string(cube.width) + "x" +
                    std::to_string(cube.height) + ") — skipped. "
                    "Update your CSV coordinates to match this scene.");
                continue;
            }
            // Skip background/NoData pixels (all-zero = off-scene border)
            bool isBackground = true;
            for (int b = 0; b < std::min(cube.bands, 5); ++b)
                if (std::abs(cube.at(b, row, col)) > 1e-6f) { isBackground = false; break; }
            if (isBackground) { ++totalSkipped; continue; }

            auto spectrum = cube.pixelSpectrum(row, col);
            for (int b = 0; b < cube.bands; ++b) sig.meanReflectance[b] += spectrum[b];
            ++validCount;
        }
        if (validCount == 0) {
            Logger::log("SpectralLibrary",
                "WARNING: class '" + className + "' has 0 valid pixels after bounds/background filtering. "
                "Check that your CSV row,col values are within the scene and not on the black border.");
            continue;  // don't add a zero-vector signature — SAM angle against all-zeros is undefined
        }
        for (double& v : sig.meanReflectance) v /= static_cast<double>(validCount);
        lib.signatures.push_back(std::move(sig));
    }

    if (totalSkipped > 0)
        Logger::log("SpectralLibrary", std::to_string(totalSkipped) +
                    " sample pixel(s) skipped (out of bounds or background). "
                    "Scene size is " + std::to_string(cube.width) + "x" + std::to_string(cube.height) + ".");
    Logger::log("SpectralLibrary", "Built " + std::to_string(lib.signatures.size()) +
                " class signature(s) from labeled samples.");
    return lib;
}

void SpectralLibrary::saveCsv(const std::string& path) const {
    std::ofstream out(path);
    if (!out) throw HsiError("SpectralLibrary::saveCsv: cannot open '" + path + "' for writing.");
    out << "class_name,band_values...\n";
    for (const auto& sig : signatures) {
        out << sig.className;
        for (double v : sig.meanReflectance) out << "," << v;
        out << "\n";
    }
    Logger::log("SpectralLibrary", "Saved " + std::to_string(signatures.size()) + " signature(s) to '" + path + "'.");
}

SpectralLibrary SpectralLibrary::loadCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw HsiError("SpectralLibrary::loadCsv: cannot open '" + path + "'.");

    SpectralLibrary lib;
    std::string line;
    // Skip lines until we find the data header (first non-comment, non-blank line).
    // This tolerates comment lines starting with # (as in the sample CSVs), blank
    // lines, and the "class_name,band_values..." header row itself.
    while (std::getline(in, line)) {
        // trim leading whitespace
        auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;   // blank
        if (line[first] == '#') continue;           // comment
        // This is the header row — stop here and start reading data below.
        break;
    }
    int skippedBadRows = 0;
    while (std::getline(in, line)) {
        // trim leading whitespace
        auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        if (line[first] == '#') continue;  // comment lines inside data section
        std::stringstream ss(line);
        std::string token;
        SpectralSignature sig;
        std::getline(ss, sig.className, ',');
        // trim class name
        while (!sig.className.empty() && (sig.className.back() == '\r' || sig.className.back() == ' '))
            sig.className.pop_back();
        if (sig.className.empty()) continue;
        while (std::getline(ss, token, ',')) {
            // skip empty tokens (trailing commas) and comment-only tokens
            if (token.empty() || token[0] == '#') continue;
            try { sig.meanReflectance.push_back(std::stod(token)); }
            catch (...) { /* skip non-numeric tokens (e.g. column headers) */ }
        }
        if (sig.meanReflectance.empty()) { ++skippedBadRows; continue; }
        lib.signatures.push_back(std::move(sig));
    }
    if (skippedBadRows > 0)
        Logger::log("SpectralLibrary", "Skipped " + std::to_string(skippedBadRows) +
                    " row(s) with no numeric band values (comment lines / header rows).");
    Logger::log("SpectralLibrary", "Loaded " + std::to_string(lib.signatures.size()) +
                " signature(s) from '" + path + "'.");
    return lib;
}

} // namespace hsi
