#pragma once
#include "hsi/Types.h"
#include "hsi/SamClassifier.h"
#include "hsi/SvmModel.h"

namespace hsi {

enum class FusionRule { And, Or, MajorityOfTwo };

// Combines a SAM classification (against built-up material signatures) and
// an SVM classification (trained on labeled built-up/non-built-up samples)
// into one single-band built-up mask, as called for in the workflow:
// "SAM and SVM -> output as 1 band".
class BuiltUpClassifier {
public:
    struct Options {
        std::vector<std::string> builtUpClassNames; // which SpectralLibrary class names count as "built-up"
        double samAngleThresholdRad = 0.15;
        FusionRule fusion = FusionRule::Or;  // OR recommended: AND gives 0 if either classifier struggles
    };

    struct Outcome {
        RasterCube builtUpMask;     // single band, 0/1
        RasterCube samClassRaster;  // intermediate, for inspection/debugging
        RasterCube svmClassRaster;  // intermediate, for inspection/debugging
    };

    static Outcome classify(const RasterCube& cube,
                             const SpectralLibrary& library,
                             const SvmModel& svm,
                             const Options& opt);
};

} // namespace hsi
