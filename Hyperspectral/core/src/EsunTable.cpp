#include "hsi/EsunTable.h"
#include "hsi/Logger.h"
#include "hsi/Types.h"

#include <fstream>
#include <sstream>

namespace hsi {

std::vector<double> EsunTable::loadCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw HsiError("EsunTable::loadCsv: cannot open '" + path + "'.");

    std::vector<double> values;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first && (line[0] < '0' || line[0] > '9')) { first = false; continue; } // skip header row
        first = false;
        std::stringstream ss(line);
        std::string bandTok, esunTok;
        std::getline(ss, bandTok, ',');
        std::getline(ss, esunTok, ',');
        if (!esunTok.empty()) values.push_back(std::stod(esunTok));
    }
    Logger::log("EsunTable", "Loaded " + std::to_string(values.size()) + " ESUN value(s) from '" + path + "'.");
    return values;
}

std::vector<double> EsunTable::flatFallback(int numBands, double value) {
    Logger::log("EsunTable", "WARNING: using a flat placeholder ESUN value (" + std::to_string(value) +
                " W/m^2/um) for all " + std::to_string(numBands) +
                " bands. Replace with a real Thuillier-derived per-band table before trusting reflectance outputs.");
    return std::vector<double>(numBands, value);
}

} // namespace hsi
