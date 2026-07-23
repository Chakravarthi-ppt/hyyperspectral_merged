#include "HyperspectralPanel.h"
#include "MainWindow.h"
#include "RasterPreviewWidget.h"

#include "dialogs/PreprocessingDialog.h"
#include "dialogs/PcaStackDialog.h"
#include "dialogs/BuiltUpClassificationDialog.h"
#include "dialogs/LandCoverMapperDialog.h"
#include "dialogs/LulcDialog.h"
#include "dialogs/ThematicDialog.h"
#include "dialogs/ChangeDetectionDialog.h"

// The real, already-existing WESEE window that owns the actual
// georeferenced MapCanvas + LayerManager. HyperspectralPanel is always
// constructed with this as its parent (see MainWindow::onOpenHyperspectral),
// so we just grab it here -- no separate hyperspectral main window needed.
#include "UI/MainWindow/mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenu>
#include <QCursor>

HyperspectralPanel::HyperspectralPanel(QWidget *parent)
    : QWidget(parent)
{
    // Backing controller only -- never shown as its own window. The
    // existing HSI dialogs (PreprocessingDialog, LulcDialog, etc.) all
    // take a HsiMainWindow* to read/write appState(), database() and
    // previewWidget(), so we keep exactly one hidden instance around
    // instead of duplicating that state here.
    controller_ = new HsiMainWindow(nullptr);
    controller_->setAttribute(Qt::WA_DontShowOnScreen, true);

    // Grab the real MainWindow (our parent) so it can be handed to dialogs
    // that need to publish a saved result raster onto the actual map.
    mapWindow_ = qobject_cast<MainWindow *>(parent);

    setObjectName("hyperspectralPanel");
    setStyleSheet(
                "QWidget#hyperspectralPanel { background:#f5f5f5; }"
                "QLabel { color:#000000; }"
                "QPushButton {"
                "   background:#0B4F6C; color:white; font-weight:bold;"
                "   border-radius:3px; padding:8px 10px; text-align:left;"
                "}"
                "QPushButton:hover { background:#1565C0; }"
                );

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *title = new QLabel("Hyperspectral Analytics", this);
    title->setStyleSheet("font-size:14px; font-weight:bold; padding:4px 0; color:#000000; color:#000000;");
    root->addWidget(title);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color:#999999;");
    root->addWidget(line);

    // 1. Preprocessing -- Load/Ortho and Surface Object Mask are folded in
    // silently here (no separate buttons); Preprocessing already handles
    // ortho internally, and the surface/object split is no longer a
    // user-facing step.
    root->addWidget(makeStepButton("1. Preprocessing",
                                    "Orthorectify, calibrate and atmospherically correct the scene"));
    connect(qobject_cast<QPushButton *>(root->itemAt(root->count() - 1)->widget()),
            &QPushButton::clicked, this, &HyperspectralPanel::onStep1_Preprocessing);

    // 2. PCA
    root->addWidget(makeStepButton("2. PCA",
                                    "Principal component reduction (optionally stack with fused data)"));
    connect(qobject_cast<QPushButton *>(root->itemAt(root->count() - 1)->widget()),
            &QPushButton::clicked, this, &HyperspectralPanel::onStep2_Pca);

    // 3. Classification -- opens a chooser for Built-up / Soil / Water / LULC / Forest & Vegetation
    root->addWidget(makeStepButton("3. Classification",
                                    "Built-up, Soil, Water, LULC, Forest & Vegetation"));
    connect(qobject_cast<QPushButton *>(root->itemAt(root->count() - 1)->widget()),
            &QPushButton::clicked, this, &HyperspectralPanel::onStep3_Classification);

    // 4. Thematic -- unsupervised spectral clustering
    root->addWidget(makeStepButton("4. Thematic",
                                    "Unsupervised spectral clustering"));
    connect(qobject_cast<QPushButton *>(root->itemAt(root->count() - 1)->widget()),
            &QPushButton::clicked, this, &HyperspectralPanel::onStep4_Thematic);

    // 5. Change Detection
    root->addWidget(makeStepButton("5. Change Detection",
                                    "Compare two dates and export changed regions"));
    connect(qobject_cast<QPushButton *>(root->itemAt(root->count() - 1)->widget()),
            &QPushButton::clicked, this, &HyperspectralPanel::onStep5_ChangeDetection);

    // The small preview strip is intentionally NOT shown here anymore.
    // Every step's result now goes onto the real, georeferenced map
    // (see MainWindow::addRasterLayerToMap / LayerManager::
    // addAndDisplayLayer) instead of this disconnected side thumbnail.
    // previewWidget() itself still exists inside the hidden controller_
    // (WA_DontShowOnScreen), so nothing else needs to change -- it's just
    // never parented into this panel's visible layout.
    root->addStretch(1);

    statusLabel_ = new QLabel("Ready.", this);
    statusLabel_->setStyleSheet("color:#333333; font-style:italic;");
    root->addWidget(statusLabel_);

    auto *aboutBtn = makeStepButton("About Hyperspectral Module", "");
    root->addWidget(aboutBtn);
    connect(aboutBtn, &QPushButton::clicked, this, &HyperspectralPanel::onAbout);
}

HyperspectralPanel::~HyperspectralPanel()
{
    // previewWidget() was never parented onto us (see constructor), so
    // there's nothing to hand back -- it dies with controller_.
    delete controller_;
}

QPushButton *HyperspectralPanel::makeStepButton(const QString &text, const QString &tooltip)
{
    auto *btn = new QPushButton(text, this);
    btn->setToolTip(tooltip);
    return btn;
}

void HyperspectralPanel::onStep1_Preprocessing()
{
    // Loading + orthorectification are already handled inside this dialog
    // (it asks for the input scene and orthorectifies internally if
    // needed); Surface Object Mask is no longer run as a separate
    // user-facing step.
    PreprocessingDialog dlg(controller_, this);
    dlg.setMapWindow(mapWindow_);
    dlg.exec();
    controller_->refreshStepStatus();
    statusLabel_->setText("Preprocessing step finished.");
}

void HyperspectralPanel::onStep2_Pca()
{
    PcaStackDialog dlg(controller_, this);
    dlg.exec();
    controller_->refreshStepStatus();
    statusLabel_->setText("PCA step finished.");
}

void HyperspectralPanel::onStep3_Classification()
{
    // Classification is now a single dialog: a Supervised / Unsupervised
    // combo box (Unsupervised only exposes the K-Cluster count -- no other
    // options for now), replacing the old Built-up / Soil / Water /
    // Forest&Veg / LULC chooser menu.
    onClassifyLulc();
}

void HyperspectralPanel::onClassifyBuiltUp()
{
    BuiltUpClassificationDialog dlg(controller_, this);
    dlg.exec();
    controller_->refreshStepStatus();
    statusLabel_->setText("Built-up classification finished.");
}

void HyperspectralPanel::onClassifyLandCover()
{
    // Soil, Water and Forest & Vegetation are all produced together in one
    // pass by the index-based classifier (NDBI/BSI/NDVI/NDWI computed from
    // the per-class band targets -- see SpectralIndices::ClassBandSet), so
    // all three menu entries open the same dialog; its result already
    // contains all of them with a legend to pick out the one you need.
    LandCoverMapperDialog dlg(controller_, this);
    dlg.setMapWindow(mapWindow_);
    dlg.exec();
    controller_->refreshStepStatus();
    statusLabel_->setText("Land cover classification finished.");
}

void HyperspectralPanel::onClassifyLulc()
{
    LulcDialog dlg(controller_, this);
    dlg.setMapWindow(mapWindow_);
    dlg.exec();
    controller_->refreshStepStatus();
    statusLabel_->setText("LULC classification finished.");
}

void HyperspectralPanel::onStep4_Thematic()
{
    ThematicDialog dlg(controller_, this);
    dlg.setMapWindow(mapWindow_);
    dlg.exec();
    statusLabel_->setText("Thematic classification finished.");
}

void HyperspectralPanel::onStep5_ChangeDetection()
{
    ChangeDetectionDialog dlg(controller_, this);
    dlg.exec();
    controller_->refreshStepStatus();
    statusLabel_->setText("Change detection finished.");
}

void HyperspectralPanel::onAbout()
{
    QMessageBox::information(this, "Hyperspectral Analytics",
                              "INDIGIS Hyperspectral Analytics module, "
                              "embedded inside WESEE.\n\n"
                              "1. Preprocessing -> 2. PCA -> 3. Classification "
                              "(Supervised / Unsupervised) -> "
                              "4. Thematic (spectral clusters / vegetation indices) -> "
                              "5. Change Detection.");
}
