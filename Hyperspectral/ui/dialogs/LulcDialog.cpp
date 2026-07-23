#include "LulcDialog.h"
#include "../MainWindow.h"
#include "../RasterPreviewWidget.h"
#include "../Utils.h"
#include "hsi/RasterIO.h"
#include "UI/MainWindow/mainwindow.h"   // real WESEE window, for addRasterLayerToMap()
#include "DialogStyle.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>
#include <QDir>
#include <map>

using namespace hsi;

LulcDialog::LulcDialog(HsiMainWindow* mainWindow, QWidget* parent) : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Classification");
    setMinimumWidth(720);
    setStyleSheet(indigisDialogStyleSheet());

    // Only Unsupervised (K-Means) is offered — Supervised (SVM) and the
    // two-date change-input section have been removed from the UI.
    kSpin_ = new QSpinBox();
    kSpin_->setRange(0, 20);
    kSpin_->setSpecialValueText(" ");

    auto* form = new QFormLayout();
    auto* classTypeEdit = new QLineEdit("Unsupervised (K-means)", this);
    classTypeEdit->setReadOnly(true);
    form->addRow("Classification Type", classTypeEdit);
    form->addRow("No. of Clusters (K)", kSpin_);

    QPushButton *runBtn, *resetBtn, *cancelBtn;
    auto* btnRow = buildRunResetCancelRow(this, progressBar_, runBtn, resetBtn, cancelBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addSpacing(12);
    layout->addLayout(form);
    layout->addSpacing(12);
    layout->addLayout(btnRow);

    connect(runBtn, &QPushButton::clicked, this, &LulcDialog::runUnsupervised);
    connect(resetBtn, &QPushButton::clicked, this, &LulcDialog::resetFields);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void LulcDialog::resetFields() {
    kSpin_->setValue(0);
    progressBar_->setValue(0);
}

// Returns the best available cube for LULC: fused stack if built, else surface reflectance.
// This enables standalone operation when the fusion team TIFF has not been provided yet.
static const hsi::RasterCube* bestCube(const AppState& s) {
    if (s.stackFused)         return &(*s.stackFused);
    if (s.surfaceReflectance) return &(*s.surfaceReflectance);
    return nullptr;
}

// Builds a 3-band 0-255 RGB composite from a single-band integer-label
// raster using the given palette, then saves it and pushes it onto the
// real georeferenced map canvas. Two things this works around, both in
// LayerManager (outside Hyperspectral, so fixed here instead):
//  1) The map canvas only ever reads bands 1/2/3 as R/G/B -- it has no
//     notion of categorical class palettes, so handing it the raw
//     single-band label raster renders flat grayscale.
//  2) It stretches each band independently by its own min/max rather than
//     treating 0-255 as an absolute range, which can crush a real class
//     color to near-black when only a couple of classes are present in the
//     scene. Anchoring one corner pixel to true black and the next to true
//     white pins every band's min/max to 0/255, making that stretch a
//     no-op so every other pixel's real color survives untouched.
static void pushColoredPreviewToMap(HsiMainWindow* mw, MainWindow* mapWindow,
                                     const hsi::RasterCube& labels,
                                     const std::map<int, RasterPreviewWidget::CategoryStyle>& palette,
                                     const QString& tempFileName) {
    if (!mapWindow) return;
    try {
        hsi::RasterCube rgb;
        rgb.allocate(labels.width, labels.height, 3);
        rgb.geoTransform  = labels.geoTransform;
        rgb.projectionWkt = labels.projectionWkt;
        rgb.bandNames = { "Red", "Green", "Blue" };
        size_t n = labels.pixelCount();
        for (size_t i = 0; i < n; ++i) {
            int cls = static_cast<int>(labels.data[i]);
            auto it = palette.find(cls);
            QColor c = (it != palette.end()) ? it->second.color : QColor("#1e1e1e");
            rgb.data[i]         = static_cast<float>(c.red());
            rgb.data[n + i]     = static_cast<float>(c.green());
            rgb.data[2 * n + i] = static_cast<float>(c.blue());
        }
        if (rgb.width >= 2 && rgb.height >= 1) {
            rgb.data[0] = 0;   rgb.data[n] = 0;   rgb.data[2 * n] = 0;
            rgb.data[1] = 255; rgb.data[n + 1] = 255; rgb.data[2 * n + 1] = 255;
        }
        QString previewPath = QDir::tempPath() + "/" + tempFileName;
        hsi::RasterIO::saveCube(rgb, previewPath.toStdString());
        mapWindow->addRasterLayerToMap(previewPath);
    } catch (const std::exception& e) {
        mw->log("LulcClassifier", QString("Note: couldn't push colored preview to map (%1).").arg(e.what()));
    }
}

void LulcDialog::runUnsupervised() {
    const hsi::RasterCube* cube = bestCube(mw_->appState());
    if (!cube) {
        QMessageBox::warning(this, "Step 2 required", "Run Step 2 (DN \u2192 Surface Reflectance) first.");
        return;
    }
    if (kSpin_->value() < 1) {
        QMessageBox::warning(this, "Missing input", "Please enter the number of clusters (K) first.");
        return;
    }
    bool usingStack = mw_->appState().stackFused.has_value();
    mw_->log("LulcClassifier", usingStack ? "Using fused stack for LULC." : "Standalone mode: using surface reflectance (198 bands) for LULC.");
    try {
        std::map<int, std::string> clusterLabels;
        RasterCube result = LulcClassifier::unsupervisedKMeans(*cube, kSpin_->value(), 5, &clusterLabels);
        mw_->appState().lulcUnsupervised = result;
        mw_->log("LulcClassifier", QString("Unsupervised LULC complete, k=%1.").arg(kSpin_->value()));
        // Show with distinct colours per cluster, each labeled with a guessed
        // land-cover type (from the cluster's centroid NDVI/NDWI/NDBI/BSI) so
        // "Cluster 3" isn't just a meaningless number -- it's a starting point,
        // not a certainty, so it's shown as "likely X" rather than stated flatly.
        QString summaryText = QString("%1 clusters computed:\n\n").arg(kSpin_->value());
        {
            using CS = RasterPreviewWidget::CategoryStyle;
            std::map<int,CS> pal;
            static const QColor kColors[] = {
                {220,50,50},{34,100,34},{100,200,80},{60,120,220},
                {210,160,80},{160,50,180},{50,190,190},{240,140,30},
                {120,80,200},{80,160,80},{200,100,50},{50,120,180}
            };
            for (int i = 0; i < kSpin_->value(); ++i) {
                QString guess = clusterLabels.count(i+1) ? QString::fromStdString(clusterLabels[i+1]) : QString();
                QString label = guess.isEmpty() ? QString("Cluster %1").arg(i+1)
                                                 : QString("Cluster %1 (%2)").arg(i+1).arg(guess);
                pal[i+1] = CS{kColors[i % 12], label};
                summaryText += QString("  Cluster %1: %2\n").arg(i+1).arg(guess.isEmpty() ? "?" : guess);
            }
            pal[0] = CS{QColor(30,30,30), "Unclassified"};
            mw_->previewWidget()->showCategorical(result, 0, pal);
            pushColoredPreviewToMap(mw_, mapWindow_, result, pal, "wesee_lulc_unsupervised_preview.tif");
        }
        summaryText += "\nThese are automatic guesses based on each cluster's average spectral "
                       "signature (NDVI/NDWI/NDBI/BSI) \u2014 spot-check a few pixels from each "
                       "cluster against the image before relying on the label.";
        mw_->log("LulcClassifier", summaryText);
        progressBar_->setValue(100);
    } catch (const std::exception& e) {
        progressBar_->setValue(0);
        mw_->log("LulcClassifier", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Unsupervised classification failed", e.what());
    }
}
