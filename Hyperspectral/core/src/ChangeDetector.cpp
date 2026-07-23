#include "hsi/ChangeDetector.h"
#include "hsi/Logger.h"

#include <set>
#include <map>
#include <fstream>
#include <algorithm>

namespace hsi {

ChangeMatrixResult ChangeDetector::computeChangeMatrix(const RasterCube& classifiedDateA,
                                                          const RasterCube& classifiedDateB,
                                                          double pixelAreaSqMeters) {
    if (!classifiedDateA.sameGridAs(classifiedDateB)) {
        throw HsiError("ChangeDetector: the two classified rasters are not on the same grid.");
    }

    std::set<int> classSet;
    for (size_t i = 0; i < classifiedDateA.pixelCount(); ++i) {
        classSet.insert(static_cast<int>(classifiedDateA.data[i]));
        classSet.insert(static_cast<int>(classifiedDateB.data[i]));
    }

    ChangeMatrixResult result;
    result.classIds.assign(classSet.begin(), classSet.end());
    std::sort(result.classIds.begin(), result.classIds.end());
    result.pixelAreaSqMeters = pixelAreaSqMeters;

    int n = static_cast<int>(result.classIds.size());
    result.matrix.assign(n, std::vector<long>(n, 0));

    std::map<int, int> classToIndex;
    for (int i = 0; i < n; ++i) classToIndex[result.classIds[i]] = i;

    for (size_t i = 0; i < classifiedDateA.pixelCount(); ++i) {
        int from = classToIndex[static_cast<int>(classifiedDateA.data[i])];
        int to = classToIndex[static_cast<int>(classifiedDateB.data[i])];
        ++result.matrix[from][to];
    }

    Logger::log("ChangeDetector", "Change matrix computed over " + std::to_string(n) + " classes.");
    return result;
}

void ChangeDetector::saveMatrixCsv(const ChangeMatrixResult& result, const std::string& path) {
    std::ofstream out(path);
    if (!out) throw HsiError("ChangeDetector::saveMatrixCsv: cannot open '" + path + "' for writing.");

    out << "from_class\\to_class";
    for (int id : result.classIds) out << "," << id;
    out << ",row_total_pixels,row_total_area_sqm\n";

    for (size_t i = 0; i < result.classIds.size(); ++i) {
        out << result.classIds[i];
        long rowTotal = 0;
        for (size_t j = 0; j < result.classIds.size(); ++j) {
            out << "," << result.matrix[i][j];
            rowTotal += result.matrix[i][j];
        }
        out << "," << rowTotal << "," << (rowTotal * result.pixelAreaSqMeters) << "\n";
    }

    Logger::log("ChangeDetector", "Saved change matrix CSV to '" + path + "'.");
}

RasterCube ChangeDetector::computeChangeMap(const RasterCube& classifiedDateA,
                                             const RasterCube& classifiedDateB) {
    if (!classifiedDateA.sameGridAs(classifiedDateB)) {
        throw HsiError("ChangeDetector: the two classified rasters are not on the same grid.");
    }
    RasterCube out;
    out.allocate(classifiedDateA.width, classifiedDateA.height, 1);
    out.geoTransform = classifiedDateA.geoTransform;
    out.projectionWkt = classifiedDateA.projectionWkt;
    out.bandNames = { "changed (1) / unchanged (0)" };
    for (size_t i = 0; i < classifiedDateA.pixelCount(); ++i) {
        out.data[i] = (classifiedDateA.data[i] != classifiedDateB.data[i]) ? 1.0f : 0.0f;
    }
    return out;
}

} // namespace hsi
