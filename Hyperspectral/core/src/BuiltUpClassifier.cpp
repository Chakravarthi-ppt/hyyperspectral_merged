#include "hsi/BuiltUpClassifier.h"
#include "hsi/SamClassifier.h"
#include "hsi/Logger.h"

#include <set>

namespace hsi {

BuiltUpClassifier::Outcome BuiltUpClassifier::classify(const RasterCube& cube,
                                                         const SpectralLibrary& library,
                                                         const SvmModel& svm,
                                                         const Options& opt) {
    std::map<int, std::string> samLegend;
    RasterCube samClasses = SamClassifier::classify(cube, library, opt.samAngleThresholdRad, &samLegend);

    std::set<int> builtUpSamIndices;
    for (const auto& kv : samLegend)
        if (std::find(opt.builtUpClassNames.begin(), opt.builtUpClassNames.end(), kv.second) != opt.builtUpClassNames.end())
            builtUpSamIndices.insert(kv.first);

    RasterCube svmClasses = svm.classifyCube(cube);
    // Convention: SVM class label 1 == "built-up", 0 == "non-built-up".
    // (The caller trains the SvmModel with that label scheme.)

    RasterCube fused;
    fused.allocate(cube.width, cube.height, 1);
    fused.geoTransform = cube.geoTransform;
    fused.projectionWkt = cube.projectionWkt;
    fused.bandNames = { "built_up_mask" };

    long agreeCount = 0, samCount = 0, svmCount = 0;
    for (size_t i = 0; i < cube.pixelCount(); ++i) {
        bool samSaysBuiltUp = builtUpSamIndices.count(static_cast<int>(samClasses.data[i])) > 0;
        bool svmSaysBuiltUp = svmClasses.data[i] > 0.5f;
        if (samSaysBuiltUp) ++samCount;
        if (svmSaysBuiltUp) ++svmCount;

        bool result;
        switch (opt.fusion) {
            case FusionRule::And:           result = samSaysBuiltUp && svmSaysBuiltUp; break;
            case FusionRule::Or:            result = samSaysBuiltUp || svmSaysBuiltUp; break;
            case FusionRule::MajorityOfTwo: result = samSaysBuiltUp && svmSaysBuiltUp; break;
            default:                        result = false;
        }
        fused.data[i] = result ? 1.0f : 0.0f;
        if (samSaysBuiltUp == svmSaysBuiltUp) ++agreeCount;
    }

    const size_t N = cube.pixelCount();
    Logger::log("BuiltUpClassifier",
        "SAM  : " + std::to_string(samCount) + " built-up pixels (" +
        std::to_string(100.0 * samCount / N) + "%)  angle_threshold=" +
        std::to_string(opt.samAngleThresholdRad) + " rad");
    Logger::log("BuiltUpClassifier",
        "SVM  : " + std::to_string(svmCount) + " built-up pixels (" +
        std::to_string(100.0 * svmCount / N) + "%)");
    Logger::log("BuiltUpClassifier",
        "Fusion (AND/OR): agreement=" + std::to_string(100.0 * agreeCount / N) + "%.  " +
        "If SAM=0 → increase angle threshold (try 0.30-0.45).  " +
        "If SVM=0 → check sample CSV coords are within scene bounds and classes exist in reflectance.");

    Outcome out;
    out.builtUpMask = std::move(fused);
    out.samClassRaster = std::move(samClasses);
    out.svmClassRaster = std::move(svmClasses);
    return out;
}

} // namespace hsi
