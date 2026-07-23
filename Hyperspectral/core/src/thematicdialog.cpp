#include "ThematicDialog.h"
#include "../MainWindow.h"
#include "../RasterPreviewWidget.h"
#include "hsi/ThematicClusterer.h"
#include "hsi/RasterIO.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>

#include "UI/MainWindow/mainwindow.h"   // real WESEE window, for addRasterLayerToMap()

using namespace hsi;

namespace {
// Distinct, high-contrast colors for up to 20 clusters (no attempt to imply
// meaning -- these are just spectral groupings, not named land classes).
const QColor kClusterPalette[] = {
    QColor("#e6194b"), QColor("#3cb44b"), QColor("#ffe119"), QColor("#4363d8"),
    QColor("#f58231"), QColor("#911eb4"), QColor("#46f0f0"), QColor("#f032e6"),
    QColor("#bcf60c"), QColor("#fabebe"), QColor("#008080"), QColor("#e6beff"),
    QColor("#9a6324"), QColor("#fffac8"), QColor("#800000"), QColor("#aaffc3"),
    QColor("#808000"), QColor("#ffd8b1"), QColor("#000075"), QColor("#808080"),
};
}

ThematicDialog::ThematicDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Thematic — Spectral Clusters");
    setMinimumWidth(480);

    sourceCombo_ = new QComboBox(this);
    if (mw_->appState().pcaResult)
        sourceCombo_->addItem("PCA result (recommended)", "pca");
    if (mw_->appState().stackFused)
        sourceCombo_->addItem("Stacked (PCA/Hyperion + fused)", "stack");
    if (mw_->appState().surfaceReflectance)
        sourceCombo_->addItem("Surface reflectance", "reflectance");

    kSpin_ = new QSpinBox(this);
    kSpin_->setRange(2, 20);
    kSpin_->setValue(5);
    kSpin_->setToolTip("Number of spectral clusters to show.");

    auto* form = new QFormLayout();
    form->addRow("Source:", sourceCombo_);
    form->addRow("Number of clusters (k):", kSpin_);
    form->addRow(new QLabel(
        "<i>Groups pixels by spectral similarity into k clusters -- a quick "
        "thematic view of the scene, not a named land-cover classification "
        "(use the Classification step for that).</i>"));

    auto* runBtn = new QPushButton("Run Clustering");
    runBtn->setDefault(true);
    auto* saveBtn = new QPushButton("Save Raster…");

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(runBtn);
    btnRow->addWidget(saveBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(btnRow);

    connect(runBtn,  &QPushButton::clicked, this, &ThematicDialog::run);
    connect(saveBtn, &QPushButton::clicked, this, &ThematicDialog::save);
}

void ThematicDialog::run() {
    if (sourceCombo_->count() == 0) {
        QMessageBox::warning(this, "No data available",
            "Run Preprocessing (and, optionally, PCA) first to produce a cube to cluster.");
        return;
    }

    const RasterCube* cube = nullptr;
    QString which = sourceCombo_->currentData().toString();
    if (which == "pca" && mw_->appState().pcaResult)
        cube = &mw_->appState().pcaResult->components;
    else if (which == "stack" && mw_->appState().stackFused)
        cube = &(*mw_->appState().stackFused);
    else if (mw_->appState().surfaceReflectance)
        cube = &(*mw_->appState().surfaceReflectance);

    if (!cube) {
        QMessageBox::warning(this, "No data available",
            "Run Preprocessing first to produce a cube to cluster.");
        return;
    }

    try {
        ThematicClusterer::Options opt;
        opt.k = kSpin_->value();
        mw_->log("ThematicClusterer", QString("Clustering into k=%1 groups…").arg(opt.k));
        auto result = ThematicClusterer::cluster(*cube, opt);
        mw_->appState().thematicClusters = result.clusters;

        std::map<int, RasterPreviewWidget::CategoryStyle> styles;
        int k = opt.k;
        for (int c = 0; c < k; ++c) {
            QColor color = kClusterPalette[c % (sizeof(kClusterPalette) / sizeof(kClusterPalette[0]))];
            styles[c] = { color, QString("Cluster %1").arg(c) };
        }
        mw_->previewWidget()->showCategorical(result.clusters, 0, styles);
        mw_->log("ThematicClusterer", "Thematic clustering complete.");
        QMessageBox::information(this, "Clustering complete",
            QString("Scene grouped into %1 spectral clusters.").arg(k));
    } catch (const std::exception& e) {
        mw_->log("ThematicClusterer", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Clustering failed", e.what());
    }
}

void ThematicDialog::save() {
    if (!mw_->appState().thematicClusters) {
        QMessageBox::warning(this, "Nothing to save", "Run clustering first.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Save thematic cluster raster",
        QString(), "GeoTIFF (*.tif)");
    if (path.isEmpty()) return;

    try {
        RasterIO::saveCube(*mw_->appState().thematicClusters, path.toStdString());
        mw_->log("ThematicClusterer", QString("Raster saved → %1").arg(path));
        if (mapWindow_) mapWindow_->addRasterLayerToMap(path);
        QMessageBox::information(this, "Saved",
            QString("Thematic cluster raster saved to:\n%1").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}
