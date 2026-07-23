#include "BuiltUpClassificationDialog.h"
#include "../MainWindow.h"
#include "../RasterPreviewWidget.h"
#include "../Utils.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <sstream>

using namespace hsi;

BuiltUpClassificationDialog::BuiltUpClassificationDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Built-up Classification (SAM + SVM \u2192 1 band)");
    setMinimumWidth(560);

    samplesCsvEdit_ = new QLineEdit();
    auto* browseBtn = new QPushButton("Browse...");
    builtUpClassNamesEdit_ = new QLineEdit("built_up");
    samAngleSpin_ = new QDoubleSpinBox();
    samAngleSpin_->setRange(0.01, 1.5);
    samAngleSpin_->setDecimals(3);
    samAngleSpin_->setSingleStep(0.01);
    samAngleSpin_->setValue(0.15);
    samAngleSpin_->setSuffix(" rad");

    fusionCombo_ = new QComboBox();
    fusionCombo_->addItem("AND (both SAM and SVM agree)");
    fusionCombo_->addItem("OR (either SAM or SVM)");

    auto* samplesRow = new QHBoxLayout();
    samplesRow->addWidget(samplesCsvEdit_);
    samplesRow->addWidget(browseBtn);

    auto* form = new QFormLayout();
    form->addRow("Samples CSV (class_name,row,col):", samplesRow);
    form->addRow("Class name(s) = \"built-up\" (comma-separated):", builtUpClassNamesEdit_);
    form->addRow("SAM angle threshold:", samAngleSpin_);
    form->addRow("SAM + SVM fusion rule:", fusionCombo_);

    auto* infoLabel = new QLabel(
        "Builds a spectral signature library from the labeled samples, runs SAM against it, "
        "trains an SVM on the same samples, then fuses both into one 0/1 built-up band on the "
        "206-band stack.", this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #555;");

    auto* runBtn = new QPushButton("Classify Built-up Area");
    runBtn->setDefault(true);
    auto* exportBtn = new QPushButton("Export Built-up Mask to Vector...");

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(infoLabel);
    layout->addLayout(form);
    layout->addWidget(runBtn);
    layout->addWidget(exportBtn);

    connect(browseBtn, &QPushButton::clicked, this, &BuiltUpClassificationDialog::browseSamplesCsv);
    connect(runBtn, &QPushButton::clicked, this, &BuiltUpClassificationDialog::run);
    connect(exportBtn, &QPushButton::clicked, this, &BuiltUpClassificationDialog::exportVector);
}

void BuiltUpClassificationDialog::browseSamplesCsv() {
    QString path = QFileDialog::getOpenFileName(this, "Select samples CSV", QString(), "CSV files (*.csv);;All files (*)");
    if (!path.isEmpty()) samplesCsvEdit_->setText(path);
}

void BuiltUpClassificationDialog::run() {
    // Standalone: use surface reflectance if fused stack not yet built.
    const hsi::RasterCube* cubePtr = nullptr;
    if (mw_->appState().stackFused)         cubePtr = &(*mw_->appState().stackFused);
    else if (mw_->appState().surfaceReflectance) cubePtr = &(*mw_->appState().surfaceReflectance);
    if (!cubePtr) {
        QMessageBox::warning(this, "Step 2 required",
            "Run Step 2 (DN \u2192 Surface Reflectance) first.\n\n"
            "For full multi-sensor classification, also run Step 5 (PCA & Stack).\n"
            "Standalone operation on Hyperion-only is also supported.");
        return;
    }
    bool usingStack = mw_->appState().stackFused.has_value();
    mw_->log("BuiltUpClassifier", usingStack
        ? QString("Using %1-band fused stack.").arg(cubePtr->bands)
        : QString("Standalone mode: using %1-band surface reflectance (no fusion team data).").arg(cubePtr->bands));
    if (samplesCsvEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "Missing samples", "Choose a samples CSV.");
        return;
    }

    try {
        const RasterCube& stack = *cubePtr;
        auto samples = ui_util::loadSampleCsv(samplesCsvEdit_->text().toStdString());

        std::vector<std::string> builtUpNames;
        {
            std::stringstream ss(builtUpClassNamesEdit_->text().toStdString());
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
                while (!tok.empty() && tok.back() == ' ') tok.pop_back();
                if (!tok.empty()) builtUpNames.push_back(tok);
            }
        }
        if (builtUpNames.empty()) {
            QMessageBox::warning(this, "Missing class names", "Enter at least one built-up class name.");
            return;
        }

        mw_->log("BuiltUpClassifier", "Building spectral library...");
        SpectralLibrary library = SpectralLibrary::buildFromSamples(stack, samples);
        mw_->appState().spectralLibrary = library;

        std::vector<std::vector<float>> features;
        std::vector<int> labels;
        for (const auto& kv : samples) {
            bool isBuiltUp = std::find(builtUpNames.begin(), builtUpNames.end(), kv.first) != builtUpNames.end();
            for (auto rc : kv.second) {
                if (rc.first < 0 || rc.first >= stack.height || rc.second < 0 || rc.second >= stack.width) continue;
                features.push_back(stack.pixelSpectrum(rc.first, rc.second));
                labels.push_back(isBuiltUp ? 1 : 0);
            }
        }
        if (features.empty()) {
            QMessageBox::warning(this, "No usable samples", "No sample pixels fell inside the stack's extent.");
            return;
        }

        SvmModel svm;
        svm.train(features, labels);
        mw_->appState().builtUpSvm = svm;

        BuiltUpClassifier::Options opt;
        opt.builtUpClassNames = builtUpNames;
        opt.samAngleThresholdRad = samAngleSpin_->value();
        opt.fusion = (fusionCombo_->currentIndex() == 0) ? FusionRule::And : FusionRule::Or;

        mw_->log("BuiltUpClassifier", "Running SAM + SVM fusion...");
        auto outcome = BuiltUpClassifier::classify(stack, library, svm, opt);
        mw_->appState().builtUpMask = outcome.builtUpMask;

        long builtUpPixels = 0;
        for (float v : outcome.builtUpMask.data) if (v > 0.5f) ++builtUpPixels;

        mw_->log("BuiltUpClassifier", QString("Done. %1 of %2 pixels classified as built-up.")
                      .arg(builtUpPixels).arg(static_cast<qlonglong>(outcome.builtUpMask.pixelCount())));
        // Show built-up mask as categorical over the reflectance RGB context
        using CS = RasterPreviewWidget::CategoryStyle;
        mw_->previewWidget()->showCategorical(outcome.builtUpMask, 0, {
            { 0, CS{ QColor(40,  40,  40),  "Non built-up" } },
            { 1, CS{ QColor(255, 50,  50),  "Built-up"     } }
        });

        QMessageBox::information(this, "Built-up classification complete",
            QString("Built-up pixels: %1 / %2 (%3%)")
                .arg(builtUpPixels)
                .arg(static_cast<qlonglong>(outcome.builtUpMask.pixelCount()))
                .arg(100.0 * builtUpPixels / outcome.builtUpMask.pixelCount(), 0, 'f', 1));
    } catch (const std::exception& e) {
        mw_->log("BuiltUpClassifier", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Classification failed", e.what());
    }
}

void BuiltUpClassificationDialog::exportVector() {
    if (!mw_->appState().builtUpMask) {
        QMessageBox::warning(this, "Nothing to export", "Run the classification first.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Save built-up vector", QString(), "GeoJSON (*.geojson)");
    if (path.isEmpty()) return;

    try {
        RasterToVector::polygonize(*mw_->appState().builtUpMask, path.toStdString(), "GeoJSON");
        mw_->log("RasterToVector", QString("Exported to '%1'.").arg(path));
        QMessageBox::information(this, "Exported", QString("Built-up vector saved to:\n%1").arg(path));
    } catch (const std::exception& e) {
        mw_->log("RasterToVector", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Export failed", e.what());
    }
}
