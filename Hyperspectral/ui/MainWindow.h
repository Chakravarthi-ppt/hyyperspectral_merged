#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <optional>
#include "hsi/Hsi.h"
#include "hsi/Database.h"
#include "hsi/RxDetector.h"
#include "RasterPreviewWidget.h"
#include "ImageComparisonWidget.h"

class QTabWidget;

// ---------------------------------------------------------------------------
// Shared pipeline state — populated step by step as each stage runs.
// The other two teams (EOS-04 team, Sentinel-2 team) hand us file paths
// at the merge step; everything else is produced by this tool.
// ---------------------------------------------------------------------------
struct AppState {
    // Step 1 — raw Hyperion scene path (after ortho check / warp)
    std::string hyperionInputPath;
    bool        wasAlreadyOrthorectified = false;

    // Step 2 — surface reflectance cube (198 calibrated bands)
    std::optional<hsi::RasterCube> surfaceReflectance;

    // Step 3 — binary surface(0)/object(1) mask
    std::optional<hsi::RasterCube> surfaceObjectMask;

    // Step 4 — PCA result (SWIR component bands)
    std::optional<hsi::PcaReducer::Result> pcaResult;

    // Step 5 — stacked cube: 0-197 Hyperion + 198..N fused data (from fusion team)
    //   0-197  : Hyperion surface reflectance   (our team)
    //   198-201: Sentinel-2 L2A bands B2/B3/B4/B8  (Team Sentinel-2)
    //   202-203: EOS-04 VV / VH backscatter         (Team EOS-04)
    std::string fusedDataPath;              // single fused TIFF from fusion team
    std::optional<hsi::RasterCube> stackFused;

    // Step 6 — spectral library, SAM+SVM, built-up 1-band raster
    std::optional<hsi::SpectralLibrary> spectralLibrary;
    std::optional<hsi::SvmModel>        builtUpSvm;
    std::optional<hsi::RasterCube>      builtUpMask;

    // Step 7 — LULC (supervised + unsupervised) + change matrix
    std::optional<hsi::RasterCube>      lulcUnsupervised;
    std::optional<hsi::RasterCube>      lulcSupervisedA;
    std::optional<hsi::SvmModel>        lulcModel;
    std::optional<std::map<std::string,int>> lulcClassToLabel;  // class name → integer label
    std::optional<hsi::RasterCube>      lulcSupervisedB;
    std::optional<hsi::ChangeMatrixResult> changeMatrix;

    // Thematic step — unsupervised spectral cluster map (k-means, raw
    // cluster labels 0..k-1; see ThematicClusterer). Independent of the
    // LULC/BuiltUp/LandCoverMapper classification results above.
    std::optional<hsi::RasterCube>      thematicClusters;

    // Thematic step — single-band vegetation/spectral index result (NDVI,
    // NDWI, NDBI, BSI, EVI — see SpectralIndices), when the user picks an
    // index instead of spectral clustering.
    std::optional<hsi::RasterCube>      thematicIndexResult;
};

// ---------------------------------------------------------------------------
// One workflow step shown in the left panel.
// ---------------------------------------------------------------------------
struct StepWidget {
    QFrame*      frame  = nullptr;
    QLabel*      number = nullptr;
    QLabel*      title  = nullptr;
    QLabel*      status = nullptr;
    QPushButton* btn    = nullptr;
};

// ---------------------------------------------------------------------------
// Main window — single "Hyperspectral" menu, workflow side-panel on the left.
// ---------------------------------------------------------------------------
class HsiMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit HsiMainWindow(QWidget* parent = nullptr);

    AppState&            appState()     { return state_; }
    RasterPreviewWidget* previewWidget(){ return preview_; }
    hsi::Database&       database()     { return db_; }
    void log(const QString& stage, const QString& message);

    // Call after any step completes to refresh all step status indicators.
    void refreshStepStatus();

private slots:
    // Sequential workflow steps
    void onStep1_LoadOrtho();
    void onStep2_Preprocess();
    void onStep3_SurfaceObjectMask();
    void onStep4_PcaAndStack();
    void onStep5_BuiltUp();
    void onStep6_RasterToVector();
    void onStep7_Lulc();
    void onStep5b_LandCoverMapper();
    void onStep8_ChangeDetection();

    void onRunAll();
    void onAbout();
    void onToolAnomalyDetector();
    void onComparisonViewRequested(int idx);

private:
    void buildLayout();
    void buildMenu();
    StepWidget makeStep(int num, const QString& title, const QString& tooltip);

    AppState             state_;
    RasterPreviewWidget* preview_    = nullptr;
    ImageComparisonWidget* comparisonView_ = nullptr;
    QTabWidget*          tabs_       = nullptr;
    QPlainTextEdit*      logConsole_ = nullptr;
    hsi::Database        db_;

    // Eight workflow step widgets in the left panel
    static constexpr int N_STEPS = 9;
    StepWidget steps_[N_STEPS];
};
