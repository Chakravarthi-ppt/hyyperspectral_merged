#pragma once
#include <QWidget>

class QPushButton;
class QLabel;
class HsiMainWindow;
class RasterPreviewWidget;
class MainWindow;   // the real WESEE window (UI/MainWindow/mainwindow.h)

// Dedicated Hyperspectral UI panel embedded inside WESEE's own docking
// layout (right dock). This does NOT reuse HsiMainWindow's window chrome;
// it only keeps one hidden HsiMainWindow instance around as the backing
// "controller" object (appState / db / preview / log) that the existing
// HSI step dialogs already expect, and drives those dialogs directly.
class HyperspectralPanel : public QWidget {
    Q_OBJECT
public:
    explicit HyperspectralPanel(QWidget *parent = nullptr);
    ~HyperspectralPanel() override;

private slots:
    // The 5-step flow: Preprocessing -> PCA -> Classification -> Thematic -> Change Detection.
    // Load/Ortho and Surface Object Mask are folded silently into Preprocessing
    // (no separate buttons); Built-Up / Land Cover Mapper / LULC are all
    // reached from the Classification step's chooser.
    void onStep1_Preprocessing();
    void onStep2_Pca();
    void onStep3_Classification();
    void onStep4_Thematic();
    void onStep5_ChangeDetection();
    void onClassifyBuiltUp();
    void onClassifyLandCover();
    void onClassifyLulc();
    void onAbout();

private:
    QPushButton *makeStepButton(const QString &text, const QString &tooltip);

    // Hidden controller instance. Never shown -- provides appState(),
    // database(), previewWidget(), log() to the step dialogs, exactly the
    // way HsiMainWindow itself would if it were visible.
    HsiMainWindow *controller_ = nullptr;

    // The real WESEE window (this panel's parent). Handed to dialogs so
    // that after they save a result raster they can drop it onto the
    // actual georeferenced MapCanvas via mw->addRasterLayerToMap().
    MainWindow *mapWindow_ = nullptr;

    QLabel *statusLabel_ = nullptr;
};
