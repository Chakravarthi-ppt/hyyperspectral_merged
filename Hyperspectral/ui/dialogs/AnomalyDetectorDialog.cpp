#include "AnomalyDetectorDialog.h"
#include "MainWindow.h"
#include "RasterPreviewWidget.h"
#include "ProgressDialog.h"
#include "hsi/RxDetector.h"
#include "hsi/RasterIO.h"
#include "hsi/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QMetaObject>
#include <map>
#include <memory>

AnomalyDetectorDialog::AnomalyDetectorDialog(HsiMainWindow* mw, QWidget* parent)
    : QDialog(parent), m_mw(mw)
{
    setWindowTitle("Anomaly Detector — RX (Reed–Xiaoli)");
    setMinimumWidth(460);
    buildUi();
}

void AnomalyDetectorDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    // ── Info ────────────────────────────────────────────────────────────────
    auto* info = new QLabel(
        "<b>RX Anomaly Detector</b><br>"
        "Flags pixels whose spectral signature differs significantly from their "
        "background using the Mahalanobis distance and a chi-squared false-alarm "
        "threshold. Detection runs in a background thread — the UI stays responsive.<br>"
        "<i>Input:</i> Step 2 surface-reflectance cube (198 bands).");
    info->setWordWrap(true);
    root->addWidget(info);

    // ── Mode ────────────────────────────────────────────────────────────────
    auto* modeGroup = new QGroupBox("Detection mode");
    auto* modeForm  = new QFormLayout(modeGroup);
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("Global RX  (fast · single pass over entire scene)");
    m_modeCombo->addItem("Local RX   (CFAR · PCA-whitened · adapts per-pixel)");
    modeForm->addRow("Mode:", m_modeCombo);
    root->addWidget(modeGroup);

    // ── Parameter pages ─────────────────────────────────────────────────────
    m_paramStack = new QStackedWidget;

    // Page 0 — Global RX
    {
        auto* pg  = new QWidget;
        auto* lbl = new QLabel(
            "Global RX uses the full scene mean and covariance.\n"
            "Best when background is spectrally homogeneous.\n"
            "Scoring is parallelised across all CPU cores.");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setWordWrap(true);
        (new QVBoxLayout(pg))->addWidget(lbl);
        m_paramStack->addWidget(pg);
    }

    // Page 1 — Local RX
    {
        auto* pg   = new QWidget;
        auto* form = new QFormLayout(pg);

        m_outerR = new QSpinBox; m_outerR->setRange(3, 60);  m_outerR->setValue(15);
        m_innerR = new QSpinBox; m_innerR->setRange(1, 20);  m_innerR->setValue(3);
        m_nPc    = new QSpinBox; m_nPc->setRange(3,  30);    m_nPc->setValue(10);

        form->addRow("Outer window half-size (px):", m_outerR);
        form->addRow("Guard region half-size  (px):", m_innerR);
        form->addRow("PCA components to retain:", m_nPc);

        auto* note = new QLabel(
            "<i><b>How it works:</b> 198 bands are projected to N whitened PCA components "
            "first, then CFAR local RX runs in that reduced space. "
            "This makes Local RX ~400× faster with negligible accuracy loss. "
            "10 components is a good default; try 5–15 to trade speed vs sensitivity.<br>"
            "Scoring is parallelised across all CPU cores.</i>");
        note->setWordWrap(true);
        form->addRow(note);
        m_paramStack->addWidget(pg);
    }

    root->addWidget(m_paramStack);

    // ── PFA ─────────────────────────────────────────────────────────────────
    auto* thrGroup = new QGroupBox("Detection threshold");
    auto* thrForm  = new QFormLayout(thrGroup);
    m_pfa = new QDoubleSpinBox;
    m_pfa->setRange(1e-7, 0.1);
    m_pfa->setDecimals(6);
    m_pfa->setSingleStep(1e-5);
    m_pfa->setValue(1e-4);
    thrForm->addRow("Probability of false alarm (PFA):", m_pfa);
    auto* pfaNote = new QLabel(
        "<i>Lower PFA = fewer detections, higher confidence. "
        "Threshold = χ²(df, 1−PFA); df = bands (Global) or PCA components (Local).</i>");
    pfaNote->setWordWrap(true);
    thrForm->addRow(pfaNote);
    root->addWidget(thrGroup);

    // ── Status ──────────────────────────────────────────────────────────────
    m_statusLabel = new QLabel("Ready.");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_statusLabel);

    // ── Buttons ─────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    m_runBtn       = new QPushButton("▶  Run");
    m_saveScoreBtn = new QPushButton("Save Score Map…");
    m_saveMaskBtn  = new QPushButton("Save Binary Mask…");
    m_saveScoreBtn->setEnabled(false);
    m_saveMaskBtn->setEnabled(false);
    btnRow->addWidget(m_runBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_saveScoreBtn);
    btnRow->addWidget(m_saveMaskBtn);
    root->addLayout(btnRow);

    auto* closeBtn = new QPushButton("Close");
    root->addWidget(closeBtn);

    connect(m_modeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnomalyDetectorDialog::onModeChanged);
    connect(m_runBtn,       &QPushButton::clicked, this, &AnomalyDetectorDialog::onRun);
    connect(m_saveScoreBtn, &QPushButton::clicked, this, &AnomalyDetectorDialog::onSaveScore);
    connect(m_saveMaskBtn,  &QPushButton::clicked, this, &AnomalyDetectorDialog::onSaveMask);
    connect(closeBtn,       &QPushButton::clicked, this, &QDialog::accept);
}

void AnomalyDetectorDialog::onModeChanged(int index) {
    m_paramStack->setCurrentIndex(index);
}

void AnomalyDetectorDialog::onRun()
{
    if (!m_mw->appState().surfaceReflectance.has_value() ||
        m_mw->appState().surfaceReflectance->bands == 0) {
        QMessageBox::warning(this, "Missing input",
            "Run Step 2 (DN → Surface Reflectance) first.\n"
            "The anomaly detector needs the 198-band reflectance cube.");
        return;
    }

    hsi::RxOptions opt;
    opt.mode          = (m_modeCombo->currentIndex() == 1)
                        ? hsi::RxOptions::Mode::Local
                        : hsi::RxOptions::Mode::Global;
    opt.outerR        = m_outerR->value();
    opt.innerR        = m_innerR->value();
    opt.nPcComponents = m_nPc->value();
    opt.pfa           = m_pfa->value();

    if (opt.mode == hsi::RxOptions::Mode::Local && opt.innerR >= opt.outerR) {
        QMessageBox::warning(this, "Invalid parameters",
            "Inner guard half-size must be smaller than outer window half-size.");
        return;
    }

    // Take a const reference to the cube — it lives in AppState for the
    // duration of the dialog, so the worker thread can read it safely.
    const hsi::RasterCube& cube = *m_mw->appState().surfaceReflectance;

    QString modeName = (opt.mode == hsi::RxOptions::Mode::Local) ? "Local" : "Global";
    hsi::Logger::log("AnomalyDetector",
        QString("%1 RX — %2 bands, %3×%4, PFA=%5%6")
            .arg(modeName)
            .arg(cube.bands).arg(cube.width).arg(cube.height)
            .arg(opt.pfa, 0, 'g', 2)
            .arg(opt.mode == hsi::RxOptions::Mode::Local
                 ? QString(", %1 PCs").arg(opt.nPcComponents) : QString())
            .toStdString());

    m_runBtn->setEnabled(false);
    m_saveScoreBtn->setEnabled(false);
    m_saveMaskBtn->setEnabled(false);
    m_statusLabel->setText("Running in background…");

    // Shared result storage written by worker, read by UI after finish
    auto resultPtr = std::make_shared<hsi::RxResult>();
    bool* ok = new bool(false);  // owned by lambda captures below

    ProgressDialog* prog = new ProgressDialog(
        modeName + " RX Anomaly Detection", this);

    prog->runTask([&cube, opt, resultPtr, prog](PipelineWorker* worker) {
        // Progress callback — posts to UI thread via worker signal
        hsi::RxOptions localOpt = opt;
        localOpt.onProgress = [worker](double f) {
            worker->reportProgress(
                static_cast<int>(f * 100),
                f < 0.45 ? "PCA whitening…"
              : f < 0.90 ? "Scoring pixels…"
              :             "Finishing…");
        };
        *resultPtr = hsi::RxDetector::detect(cube, localOpt);
    });

    if (prog->succeeded()) {
        m_result = std::make_unique<hsi::RxResult>(std::move(*resultPtr));

        double totalPx = static_cast<double>(cube.width) * cube.height;
        double pct     = 100.0 * m_result->anomalyCount / totalPx;

        hsi::Logger::log("AnomalyDetector",
            QString("Done. χ²-threshold=%.2f, anomalies=%1 (%.4f%% of pixels)")
                .arg(m_result->anomalyCount)
                .arg(pct, 0, 'f', 4)
                .toStdString());

        m_statusLabel->setText(
            QString("Done — %1 anomalous pixels (%.4f%%)  |  χ² threshold: %2")
                .arg(m_result->anomalyCount)
                .arg(pct, 0, 'f', 4)
                .arg(m_result->threshold, 0, 'f', 2));

        showResults(*m_result);
        m_saveScoreBtn->setEnabled(true);
        m_saveMaskBtn->setEnabled(true);
    } else {
        hsi::Logger::log("AnomalyDetector",
            "ERROR: " + prog->errorMessage().toStdString());
        QMessageBox::critical(this, "Detection failed", prog->errorMessage());
        m_statusLabel->setText("Error — see pipeline log.");
    }

    delete ok;
    prog->deleteLater();
    m_runBtn->setEnabled(true);
}

void AnomalyDetectorDialog::showResults(const hsi::RxResult& r)
{
    using CS = RasterPreviewWidget::CategoryStyle;
    std::map<int, CS> styles = {
        { 0, CS{ QColor(30,  30,  30),  "Background" } },
        { 1, CS{ QColor(255, 50,  50),  "Anomaly"    } }
    };
    m_mw->previewWidget()->showCategorical(r.binaryMask, 0, styles);
}

void AnomalyDetectorDialog::onSaveScore()
{
    if (!m_result) return;
    QString path = QFileDialog::getSaveFileName(
        this, "Save RX Score Map", QString(), "GeoTIFF (*.tif *.tiff)");
    if (path.isEmpty()) return;
    try {
        hsi::RasterIO::saveCube(m_result->scoreMap, path.toStdString());
        hsi::Logger::log("AnomalyDetector", "Score map saved: " + path.toStdString());
        QMessageBox::information(this, "Saved", "Score map written to:\n" + path);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}

void AnomalyDetectorDialog::onSaveMask()
{
    if (!m_result) return;
    QString path = QFileDialog::getSaveFileName(
        this, "Save Binary Anomaly Mask", QString(), "GeoTIFF (*.tif *.tiff)");
    if (path.isEmpty()) return;
    try {
        hsi::RasterIO::saveCube(m_result->binaryMask, path.toStdString());
        hsi::Logger::log("AnomalyDetector", "Binary mask saved: " + path.toStdString());
        QMessageBox::information(this, "Saved", "Binary mask written to:\n" + path);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}
