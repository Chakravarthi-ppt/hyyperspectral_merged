#pragma once
#include "hsi/Types.h"
#include <map>

namespace hsi {

class SpectralLibrary {
public:
    std::vector<SpectralSignature> signatures;

    // classSamplePixels: class name -> list of (row, col) training pixels.
    // Each signature is the mean spectrum (all bands of `cube`) over its pixels.
    static SpectralLibrary buildFromSamples(const RasterCube& cube,
                                             const std::map<std::string, std::vector<std::pair<int, int>>>& classSamplePixels);

    void saveCsv(const std::string& path) const;
    static SpectralLibrary loadCsv(const std::string& path);
};

} // namespace hsi
