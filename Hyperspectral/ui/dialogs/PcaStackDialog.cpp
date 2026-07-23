#include "PcaStackDialog.h"
#include "../MainWindow.h"
#include "../ProgressDialog.h"
#include "DialogStyle.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>

using namespace hsi;

PcaStackDialog::PcaStackDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Principal Component Analysis (PCA)");
    setMinimumWidth(720);
    setStyleSheet(indigisDialogStyleSheet());

    int maxBand = mw_->appState().surfaceReflectance
                      ? mw_->appState().surfaceReflectance->bands - 1 : 197;

    // ── PCA ──────────────────────────────────────────────────────────────
    // No pre-filled defaults: spin boxes start blank (sentinel = min-1 with
    // an empty special-value text) so the user must explicitly enter values.
    pcaStartSpin_ = new QSpinBox();
    pcaStartSpin_->setRange(-1, maxBand);
    pcaStartSpin_->setSpecialValueText(" ");
    pcaStartSpin_->setValue(-1);

    pcaEndSpin_ = new QSpinBox();
    pcaEndSpin_->setRange(-1, maxBand);
    pcaEndSpin_->setSpecialValueText(" ");
    pcaEndSpin_->setValue(-1);

    pcaComponentsSpin_ = new QSpinBox();
    pcaComponentsSpin_->setRange(0, 10);
    pcaComponentsSpin_->setSpecialValueText(" ");
    pcaComponentsSpin_->setValue(0);

    auto* pcaForm = new QFormLayout();
    pcaForm->addRow("Band Range Start", pcaStartSpin_);
    pcaForm->addRow("Band Range End", pcaEndSpin_);
    pcaForm->addRow("No. of PC Components", pcaComponentsSpin_);

    QPushButton *pcaRunBtn, *pcaResetBtn, *pcaCancelBtn;
    auto* pcaBtnRow = buildRunResetCancelRow(this, progressBar_, pcaRunBtn, pcaResetBtn, pcaCancelBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addSpacing(12);
    layout->addLayout(pcaForm);
    layout->addSpacing(12);
    layout->addLayout(pcaBtnRow);
    setLayout(layout);

    connect(pcaRunBtn,      &QPushButton::clicked, this, &PcaStackDialog::runPca);
    connect(pcaResetBtn,    &QPushButton::clicked, this, &PcaStackDialog::resetFields);
    connect(pcaCancelBtn,   &QPushButton::clicked, this, &QDialog::reject);
}

void PcaStackDialog::resetFields() {
    pcaStartSpin_->setValue(-1);
    pcaEndSpin_->setValue(-1);
    pcaComponentsSpin_->setValue(0);
    progressBar_->setValue(0);
}


void PcaStackDialog::runPca() {
    if (!mw_->appState().surfaceReflectance) {
        QMessageBox::warning(this, "Run Step 1 first",
            "Run Step 1 (DN → Surface Reflectance) first.");
        return;
    }
    if (pcaStartSpin_->value() < 0 || pcaEndSpin_->value() < 0 ||
        pcaComponentsSpin_->value() < 1) {
        QMessageBox::warning(this, "Missing input",
            "Please enter the band range start, band range end, and the "
            "number of PC components before running PCA.");
        return;
    }
    if (pcaStartSpin_->value() > pcaEndSpin_->value()) {
        QMessageBox::warning(this, "Invalid range",
            "Band range start must be less than or equal to band range end.");
        return;
    }
    try {
        auto result = PcaReducer::reduce(
            *mw_->appState().surfaceReflectance,
            pcaStartSpin_->value(), pcaEndSpin_->value(),
            pcaComponentsSpin_->value());
        mw_->appState().pcaResult = result;
        mw_->log("PCA", QString("PC1 explains %1% of variance.")
                     .arg(result.explainedVarianceRatio[0] * 100.0, 0, 'f', 2));
        mw_->previewWidget()->showSingleBand(result.components, 0);
        progressBar_->setValue(100);
        QMessageBox::information(this, "PCA complete",
            QString("%1 component(s). PC1 variance: %2%.")
                .arg(result.components.bands)
                .arg(result.explainedVarianceRatio[0] * 100.0, 0, 'f', 2));
    } catch (const std::exception& e) {
        progressBar_->setValue(0);
        mw_->log("PCA", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "PCA failed", e.what());
    }
}
