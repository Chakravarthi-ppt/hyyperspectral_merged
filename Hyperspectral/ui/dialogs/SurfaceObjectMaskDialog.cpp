#include "SurfaceObjectMaskDialog.h"
#include "../MainWindow.h"
#include "../Utils.h"
#include "../RasterPreviewWidget.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>
#include <QWidget>

using namespace hsi;

SurfaceObjectMaskDialog::SurfaceObjectMaskDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Surface (0) vs Object (1) Mask");
    setMinimumWidth(560);

    int maxBand = mw_->appState().surfaceReflectance ? mw_->appState().surfaceReflectance->bands - 1 : 0;

    methodCombo_ = new QComboBox(this);
    methodCombo_->addItem("Fixed spectral threshold (e.g. NDBI-style index)");
    methodCombo_->addItem("Unsupervised k-means (k=2)");
    methodCombo_->addItem("Trained SVM (needs labeled samples)");

    // --- Threshold page ---
    nirBandSpin_ = new QSpinBox();
    nirBandSpin_->setRange(0, std::max(0, maxBand));
    nirBandSpin_->setValue(std::min(22, maxBand)); // sensor band ~30 ≈ 901nm NIR
    nirBandSpin_->setToolTip("0-based band index for NIR (~900nm). Default 22 = Hyperion sensor band 30.");

    swirBandSpin_ = new QSpinBox();
    swirBandSpin_->setRange(0, std::max(0, maxBand));
    swirBandSpin_->setValue(std::min(63, maxBand)); // sensor band ~90 ≈ 1093nm SWIR
    swirBandSpin_->setToolTip("0-based band index for SWIR (~1100nm). Default 63 = Hyperion sensor band 90.");

    thresholdSpin_ = new QDoubleSpinBox();
    thresholdSpin_->setRange(-1.0, 1.0);
    thresholdSpin_->setDecimals(3);
    thresholdSpin_->setSingleStep(0.05);
    thresholdSpin_->setValue(0.0);
    thresholdSpin_->setToolTip("NDBI = (SWIR-NIR)/(SWIR+NIR). Values > 0 are typically built-up/bare."
                               "Adjust based on the scene stats shown in the pipeline log after Step 4.");
    auto* thresholdForm = new QFormLayout();
    thresholdForm->addRow("NIR band index (0-based):", nirBandSpin_);
    thresholdForm->addRow("SWIR band index (0-based):", swirBandSpin_);
    autoThresholdCheck_ = new QCheckBox("Auto (Otsu) — pick the split point from this scene's own NDBI histogram");
    autoThresholdCheck_->setChecked(true); // safest default: a fixed 0.0 finds 0 objects on heavily vegetated scenes
    thresholdForm->addRow(autoThresholdCheck_);
    thresholdForm->addRow("Threshold (NDBI > value \u2192 object):", thresholdSpin_);
    thresholdSpin_->setEnabled(false);
    connect(autoThresholdCheck_, &QCheckBox::toggled, thresholdSpin_, &QDoubleSpinBox::setDisabled);
    thresholdForm->addRow(new QLabel(
        "<i>NDBI = (SWIR\u2212NIR)/(SWIR+NIR). "
        "Defaults: NIR=idx 22 (~901nm), SWIR=idx 63 (~1093nm).<br>"
        "A fixed threshold of 0.0 finds ~0 objects on scenes where NDBI is negative everywhere "
        "(e.g. dense forest) \u2014 leave Auto checked unless you have a specific value to test.</i>"));
    auto* thresholdPage = new QWidget(); thresholdPage->setLayout(thresholdForm);

    // --- K-means page ---
    attemptsSpin_ = new QSpinBox(); attemptsSpin_->setRange(1, 20); attemptsSpin_->setValue(3);
    auto* kmeansForm = new QFormLayout();
    kmeansForm->addRow("Restarts (attempts):", attemptsSpin_);
    kmeansForm->addRow(new QLabel("Splits all bands into 2 clusters; the brighter cluster is labeled \"object\"."));
    auto* kmeansPage = new QWidget(); kmeansPage->setLayout(kmeansForm);

    // --- Trained SVM page ---
    samplesCsvEdit_ = new QLineEdit();
    auto* browseSamplesBtn = new QPushButton("Browse...");
    objectClassNameEdit_ = new QLineEdit("built_up");
    auto* svmForm = new QFormLayout();
    auto* samplesRow = new QHBoxLayout();
    samplesRow->addWidget(samplesCsvEdit_);
    samplesRow->addWidget(browseSamplesBtn);
    svmForm->addRow("Samples CSV (class_name,row,col):", samplesRow);
    svmForm->addRow("Class name treated as \"object\":", objectClassNameEdit_);
    svmForm->addRow(new QLabel("All other class names in the CSV are treated as \"surface\"."));
    auto* svmPage = new QWidget(); svmPage->setLayout(svmForm);

    methodStack_ = new QStackedWidget(this);
    methodStack_->addWidget(thresholdPage);
    methodStack_->addWidget(kmeansPage);
    methodStack_->addWidget(svmPage);
    connect(methodCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), methodStack_, &QStackedWidget::setCurrentIndex);

    // --- Validation (shared) ---
    referencePathEdit_ = new QLineEdit();
    auto* browseRefBtn = new QPushButton("Browse...");
    auto* refRow = new QHBoxLayout();
    refRow->addWidget(referencePathEdit_);
    refRow->addWidget(browseRefBtn);
    auto* valForm = new QFormLayout();
    valForm->addRow("Reference mask (optional, e.g. EOS-04/optical-derived):", refRow);
    auto* valGroup = new QGroupBox("Validation");
    valGroup->setLayout(valForm);

    auto* runBtn = new QPushButton("Compute Mask");
    runBtn->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Method:"));
    layout->addWidget(methodCombo_);
    layout->addWidget(methodStack_);
    layout->addWidget(valGroup);
    layout->addWidget(runBtn);

    connect(browseSamplesBtn, &QPushButton::clicked, this, &SurfaceObjectMaskDialog::browseSamplesCsv);
    connect(browseRefBtn, &QPushButton::clicked, this, &SurfaceObjectMaskDialog::browseReference);
    connect(runBtn, &QPushButton::clicked, this, &SurfaceObjectMaskDialog::run);
}

void SurfaceObjectMaskDialog::browseSamplesCsv() {
    QString path = QFileDialog::getOpenFileName(this, "Select samples CSV", QString(), "CSV files (*.csv);;All files (*)");
    if (!path.isEmpty()) samplesCsvEdit_->setText(path);
}

void SurfaceObjectMaskDialog::browseReference() {
    QString path = QFileDialog::getOpenFileName(this, "Select reference mask raster", QString(),
                                                  "Raster files (*.tif *.tiff);;All files (*)");
    if (!path.isEmpty()) referencePathEdit_->setText(path);
}

void SurfaceObjectMaskDialog::run() {
    if (!mw_->appState().surfaceReflectance) {
        QMessageBox::warning(this, "Run preprocessing first",
                              "Run Hyperspectral \u2192 Preprocessing first to produce a surface reflectance cube.");
        return;
    }
    const RasterCube& cube = *mw_->appState().surfaceReflectance;

    try {
        SurfaceObjectMask::Options opt;
        SvmModel maskSvm; // kept alive for the duration of this function if TrainedSvm is used
        double resolvedThreshold = 0.0;

        int idx = methodCombo_->currentIndex();
        if (idx == 0) {
            if (nirBandSpin_->value() == swirBandSpin_->value()) {
                QMessageBox::warning(this, "NIR and SWIR bands are the same",
                    "NIR band index and SWIR band index are both set to the same band.\n\n"
                    "The index (SWIR-NIR)/(SWIR+NIR) is then 0 at every pixel, so nothing "
                    "will be classified as \"object\" no matter what threshold you use.\n\n"
                    "Pick two different bands.");
                return;
            }
            opt.method = MaskMethod::Threshold;
            opt.threshold.nirBandIndex = nirBandSpin_->value();
            opt.threshold.swirBandIndex = swirBandSpin_->value();
            opt.threshold.thresholdValue = autoThresholdCheck_->isChecked()
                                                ? SurfaceObjectMask::kAutoThreshold
                                                : thresholdSpin_->value();
            opt.threshold.resolvedThresholdOut = &resolvedThreshold;
        } else if (idx == 1) {
            opt.method = MaskMethod::KMeansUnsupervised;
            opt.kmeans.attempts = attemptsSpin_->value();
        } else {
            if (samplesCsvEdit_->text().isEmpty()) {
                QMessageBox::warning(this, "Missing samples", "Choose a samples CSV for the trained-SVM method.");
                return;
            }
            auto samples = ui_util::loadSampleCsv(samplesCsvEdit_->text().toStdString());
            std::string objectClass = objectClassNameEdit_->text().toStdString();

            std::vector<std::vector<float>> features;
            std::vector<int> labels;
            for (const auto& kv : samples) {
                int label = (kv.first == objectClass) ? 1 : 0;
                for (auto rc : kv.second) {
                    if (rc.first < 0 || rc.first >= cube.height || rc.second < 0 || rc.second >= cube.width) continue;
                    features.push_back(cube.pixelSpectrum(rc.first, rc.second));
                    labels.push_back(label);
                }
            }
            if (features.empty()) {
                QMessageBox::warning(this, "No usable samples", "No sample pixels fell inside the cube's extent.");
                return;
            }
            maskSvm.train(features, labels);
            opt.method = MaskMethod::TrainedSvm;
            opt.trainedSvm = &maskSvm;
        }

        mw_->log("SurfaceObjectMask", "Computing mask...");
        RasterCube mask = SurfaceObjectMask::computeMask(cube, opt);
        mw_->appState().surfaceObjectMask = mask;

        long objectPixels = 0;
        for (float v : mask.data) if (v > 0.5f) ++objectPixels;
        mw_->log("SurfaceObjectMask", QString("Done. %1 of %2 pixels classified as object (1).")
                      .arg(objectPixels).arg(static_cast<qlonglong>(mask.pixelCount())));
        // Show as categorical so 0=Surface (dark) and 1=Object (coloured)
        // are distinguishable -- showSingleBand renders binary 0/1 data
        // as near-identical near-black pixels after percentile stretch.
        using CS = RasterPreviewWidget::CategoryStyle;
        std::map<int, CS> maskStyles = {
            { 0, CS{ QColor(30,  30,  30),  "Surface (0)" } },
            { 1, CS{ QColor(255, 140,  0),  "Object  (1)" } }
        };
        mw_->previewWidget()->showCategorical(mask, 0, maskStyles);

        QString summary = QString("Object pixels: %1 / %2 (%3%)")
                               .arg(objectPixels)
                               .arg(static_cast<qlonglong>(mask.pixelCount()))
                               .arg(100.0 * objectPixels / mask.pixelCount(), 0, 'f', 1);
        if (idx == 0 && autoThresholdCheck_->isChecked()) {
            summary += QString("\nAuto (Otsu) threshold used: %1").arg(resolvedThreshold, 0, 'f', 4);
        }

        if (!referencePathEdit_->text().isEmpty()) {
            RasterCube reference = RasterIO::loadCube(referencePathEdit_->text().toStdString());
            auto val = SurfaceObjectMask::validateAgainstReference(mask, reference);
            QString valMsg = QString("\n\nValidation vs reference:\nAgreement: %1%\nIoU: %2\nPrecision: %3\nRecall: %4")
                                  .arg(val.overallAgreement() * 100.0, 0, 'f', 2)
                                  .arg(val.iou(), 0, 'f', 3)
                                  .arg(val.precision(), 0, 'f', 3)
                                  .arg(val.recall(), 0, 'f', 3);
            summary += valMsg;
            mw_->log("SurfaceObjectMask", QString("Validation agreement=%1%% IoU=%2")
                          .arg(val.overallAgreement() * 100.0, 0, 'f', 2).arg(val.iou(), 0, 'f', 3));
        }

        QMessageBox::information(this, "Mask computed", summary);
        accept();
    } catch (const std::exception& e) {
        mw_->log("SurfaceObjectMask", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Mask computation failed", e.what());
    }
}
