#include "MainWindow.h"
#include "RasterPreviewWidget.h"
#include "Utils.h"
#include "dialogs/PreprocessingDialog.h"
#include "dialogs/SurfaceObjectMaskDialog.h"
#include "dialogs/PcaStackDialog.h"
#include "dialogs/BuiltUpClassificationDialog.h"
#include "dialogs/LulcDialog.h"
#include "dialogs/ChangeDetectionDialog.h"
#include "dialogs/LandCoverMapperDialog.h"
#include "dialogs/AnomalyDetectorDialog.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QStatusBar>
#include <QStandardPaths>
#include <QSplitter>
#include <QTabWidget>
#include <QFont>
#include <QSizePolicy>
#include <QSettings>
#include <QFileInfo>

// ── colour constants used by step status indicators ──────────────────────
static const char* CLR_DONE    = "background:#27ae60;color:white;border-radius:3px;padding:2px 6px;font-size:11px;";
static const char* CLR_READY   = "background:#2980b9;color:white;border-radius:3px;padding:2px 6px;font-size:11px;";
static const char* CLR_WAITING = "background:#7f8c8d;color:white;border-radius:3px;padding:2px 6px;font-size:11px;";

// ── helper: build one step row ────────────────────────────────────────────
StepWidget HsiMainWindow::makeStep(int num, const QString& title, const QString& tooltip) {
    StepWidget sw;
    sw.frame = new QFrame(this);
    sw.frame->setFrameShape(QFrame::StyledPanel);
    sw.frame->setToolTip(tooltip);
    sw.frame->setStyleSheet("QFrame{background:#2c3e50;border-radius:6px;margin:2px;}");

    sw.number = new QLabel(QString::number(num), sw.frame);
    sw.number->setAlignment(Qt::AlignCenter);
    sw.number->setFixedSize(28, 28);
    sw.number->setStyleSheet("background:#1a252f;color:#ecf0f1;border-radius:14px;"
                              "font-weight:bold;font-size:13px;");

    sw.title = new QLabel(title, sw.frame);
    sw.title->setWordWrap(true);
    sw.title->setStyleSheet("color:#ecf0f1;font-size:12px;font-weight:bold;");

    sw.status = new QLabel("Waiting", sw.frame);
    sw.status->setStyleSheet(CLR_WAITING);
    sw.status->setFixedHeight(20);
    sw.status->setAlignment(Qt::AlignCenter);

    sw.btn = new QPushButton("Run", sw.frame);
    sw.btn->setFixedWidth(54);
    sw.btn->setStyleSheet(
        "QPushButton{background:#2980b9;color:white;border:none;border-radius:4px;"
        "padding:4px 6px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#3498db;}"
        "QPushButton:disabled{background:#555;color:#888;}");

    auto* top = new QHBoxLayout();
    top->addWidget(sw.number);
    top->addWidget(sw.title, 1);
    top->addWidget(sw.btn);

    auto* vl = new QVBoxLayout(sw.frame);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(4);
    vl->addLayout(top);
    vl->addWidget(sw.status);
    return sw;
}

// ── main layout ───────────────────────────────────────────────────────────
void HsiMainWindow::buildLayout() {
    // ── left workflow panel ───────────────────────────────────────────────
    auto* workflowWidget = new QWidget(this);
    workflowWidget->setStyleSheet("background:#1a252f;");
    workflowWidget->setMinimumWidth(240);
    workflowWidget->setMaximumWidth(280);

    auto* wfLayout = new QVBoxLayout(workflowWidget);
    wfLayout->setContentsMargins(6, 8, 6, 6);
    wfLayout->setSpacing(4);

    // Header
    auto* headerLbl = new QLabel("Hyperspectral\nPipeline", workflowWidget);
    headerLbl->setAlignment(Qt::AlignCenter);
    headerLbl->setStyleSheet("color:#3498db;font-size:15px;font-weight:bold;"
                              "padding:8px 0;border-bottom:1px solid #34495e;");
    wfLayout->addWidget(headerLbl);

    // Band layout legend
    auto* bandLbl = new QLabel(
        "<span style='color:#aaa;font-size:10px;'>"
        "Stack layout after Step 5:<br>"
        "<b style='color:#3498db;'>0 – 197</b>&nbsp; Hyperion (this module)<br>"
        "<b style='color:#2ecc71;'>198 – end</b>&nbsp; Fused data<br>"
        "<span style='color:#888;font-size:9px;'>"
        "S2 B2/B3/B4/B8 + EOS-04 VV/VH<br>"
        "+ NDVI + NDWI (fusion team TIFF)</span>"
        "</span>", workflowWidget);
    bandLbl->setWordWrap(true);
    bandLbl->setStyleSheet("background:#22303f;padding:6px;border-radius:4px;margin:4px 0;");
    wfLayout->addWidget(bandLbl);

    // Steps
    steps_[0] = makeStep(1, "Load + Orthorectify",
        "Load Hyperion scene. If already orthorectified → skip. Otherwise warp using embedded GCPs/RPCs.");
    steps_[1] = makeStep(2, "DN → Surface Reflectance",
        "Band selection (VNIR 8-57, SWIR 77-224), radiometric calibration (÷40/÷80), "
        "TOA reflectance, Dark Object Subtraction surface reflectance.");
    steps_[2] = makeStep(3, "Surface / Object Mask",
        "Binary mask: 0=bare surface, 1=object/structure.\n"
        "Three methods: threshold index, k-means, trained SVM.\n"
        "Validate against EOS-04 and Sentinel-2 reference inputs from other teams.");
    steps_[3] = makeStep(4, "Land Cover Map\n(Forest/Water/Built-up)",
        "Three offline methods: spectral indices (NDVI/NDWI/NDBI/BSI), "
        "SAM against a bundled library, or click-to-train SVM. No internet needed.");
    steps_[4] = makeStep(5, "PCA + Stack",
        "PCA over SWIR bands (70-224) → 1 component.\n"
        "Then stack: 0-197 Hyperion + 198..end Fused data (S2+EOS-04+NDVI+NDWI).");
    steps_[5] = makeStep(6, "Built-up Classification\n(SAM + SVM → 1 band)",
        "Spectral library from labeled samples.\n"
        "SAM + SVM classification fused into one 0/1 built-up band.");
    steps_[6] = makeStep(7, "Raster → Vector",
        "Polygonize the built-up mask into GeoJSON/Shapefile using GDALPolygonize.");
    steps_[7] = makeStep(8, "LULC Classification",
        "Supervised SVM + Unsupervised k-means on the fused stack.\n"
        "Classify a second date to prepare the change matrix.");
    steps_[8] = makeStep(9, "Change Detection Matrix",
        "From-class × to-class pixel count matrix across two dates.\n"
        "Exported as CSV. Automated alert for changed areas.");

    for (auto& s : steps_) wfLayout->addWidget(s.frame);

    // Run all button
    auto* runAllBtn = new QPushButton("▶  Run Full Pipeline", workflowWidget);
    runAllBtn->setStyleSheet(
        "QPushButton{background:#8e44ad;color:white;border:none;border-radius:5px;"
        "padding:8px;font-size:12px;font-weight:bold;margin-top:6px;}"
        "QPushButton:hover{background:#9b59b6;}");
    wfLayout->addWidget(runAllBtn);
    wfLayout->addStretch();

    // Wire step buttons
    connect(steps_[0].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep1_LoadOrtho);
    connect(steps_[1].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep2_Preprocess);
    connect(steps_[2].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep3_SurfaceObjectMask);
    connect(steps_[3].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep5b_LandCoverMapper);
    connect(steps_[4].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep4_PcaAndStack);
    connect(steps_[5].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep5_BuiltUp);
    connect(steps_[6].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep6_RasterToVector);
    connect(steps_[7].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep7_Lulc);
    connect(steps_[8].btn, &QPushButton::clicked, this, &HsiMainWindow::onStep8_ChangeDetection);
    connect(runAllBtn,      &QPushButton::clicked, this, &HsiMainWindow::onRunAll);

    // ── centre: raster preview ────────────────────────────────────────────
    preview_ = new RasterPreviewWidget(this);

    // ── splitter: workflow panel | preview ────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(workflowWidget);
    splitter->addWidget(preview_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setHandleWidth(3);

    // ── image comparison tab ──────────────────────────────────────────────
    comparisonView_ = new ImageComparisonWidget(this);
    connect(comparisonView_, &ImageComparisonWidget::viewRequested,
            this, &HsiMainWindow::onComparisonViewRequested);

    tabs_ = new QTabWidget(this);
    tabs_->setStyleSheet(
        "QTabWidget::pane{border:none;}"
        "QTabBar::tab{background:#22303c;color:#bdc3c7;padding:8px 16px;}"
        "QTabBar::tab:selected{background:#2c3e50;color:#ecf0f1;font-weight:bold;}");
    tabs_->addTab(splitter, "Processing Pipeline");
    tabs_->addTab(comparisonView_, "Image Comparison");
    setCentralWidget(tabs_);

    // ── log console (bottom dock) ─────────────────────────────────────────
    logConsole_ = new QPlainTextEdit(this);
    logConsole_->setReadOnly(true);
    logConsole_->setMaximumBlockCount(4000);
    logConsole_->setStyleSheet(
        "QPlainTextEdit{background:#1e272e;color:#dfe6e9;"
        "font-family:monospace;font-size:11px;border:none;}");
    auto* dock = new QDockWidget("Pipeline Log", this);
    dock->setWidget(logConsole_);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    resizeDocks({dock}, {160}, Qt::Vertical);
}

void HsiMainWindow::buildMenu() {
    auto* hsMenu = menuBar()->addMenu("&Hyperspectral");
    hsMenu->addAction("1. Load + Orthorectify…",         this, &HsiMainWindow::onStep1_LoadOrtho);
    hsMenu->addAction("2. DN → Surface Reflectance…",    this, &HsiMainWindow::onStep2_Preprocess);
    hsMenu->addAction("3. Surface / Object Mask…",       this, &HsiMainWindow::onStep3_SurfaceObjectMask);
    hsMenu->addAction("4. Land Cover Map (Forest/Water/Built-up)…", this, &HsiMainWindow::onStep5b_LandCoverMapper);
    hsMenu->addAction("5. PCA + Stack…",        this, &HsiMainWindow::onStep4_PcaAndStack);
    hsMenu->addAction("6. Built-up (SAM + SVM → 1 band)…", this, &HsiMainWindow::onStep5_BuiltUp);
    hsMenu->addAction("7. Raster → Vector…",             this, &HsiMainWindow::onStep6_RasterToVector);
    hsMenu->addAction("8. LULC Classification…",         this, &HsiMainWindow::onStep7_Lulc);
    hsMenu->addAction("9. Change Detection Matrix…",     this, &HsiMainWindow::onStep8_ChangeDetection);
    hsMenu->addSeparator();
    hsMenu->addAction("▶  Run Full Pipeline",            this, &HsiMainWindow::onRunAll);
    hsMenu->addSeparator();
    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("Anomaly Detector (RX)…", this, &HsiMainWindow::onToolAnomalyDetector);
    menuBar()->addSeparator();
    hsMenu->addAction("About…",                          this, &HsiMainWindow::onAbout);
}

// ── step status refresh ───────────────────────────────────────────────────
void HsiMainWindow::refreshStepStatus() {
    auto set = [](StepWidget& sw, bool done, bool ready) {
        if (done) {
            sw.status->setText("✔  Done");
            sw.status->setStyleSheet(CLR_DONE);
            sw.btn->setEnabled(true);
        } else if (ready) {
            sw.status->setText("Ready");
            sw.status->setStyleSheet(CLR_READY);
            sw.btn->setEnabled(true);
        } else {
            sw.status->setText("Waiting");
            sw.status->setStyleSheet(CLR_WAITING);
            sw.btn->setEnabled(false);
        }
    };

    bool haveInput       = !state_.hyperionInputPath.empty();
    bool haveRefl        = state_.surfaceReflectance.has_value();
    bool haveMask        = state_.surfaceObjectMask.has_value();
    bool haveStack       = state_.stackFused.has_value();
    bool haveBuiltUp     = state_.builtUpMask.has_value();
    bool haveLulcA       = state_.lulcSupervisedA.has_value();
    bool haveLulcB       = state_.lulcSupervisedB.has_value();

    set(steps_[0], haveInput,   true);           // step 1: always ready to start
    set(steps_[1], haveRefl,    haveInput);
    set(steps_[2], haveMask,    haveRefl);
    set(steps_[3], false, haveRefl);          // Land cover: always re-runnable
    set(steps_[4], haveStack,   haveRefl);       // PCA+Stack only needs reflectance
    set(steps_[5], haveBuiltUp, haveRefl);  // standalone: only needs reflectance
    set(steps_[6], false,       haveBuiltUp);    // vector export: re-run any time
    set(steps_[7], haveLulcA,   haveRefl);   // standalone: falls back to reflectance
    set(steps_[8], haveLulcB && state_.changeMatrix.has_value(), haveLulcA);

    if (comparisonView_) comparisonView_->refresh(state_);
}

// ── Image Comparison tab: "View full size" ───────────────────────────────
void HsiMainWindow::onComparisonViewRequested(int idx) {
    tabs_->setCurrentIndex(0); // back to Processing Pipeline, where preview_ lives
    using CS = RasterPreviewWidget::CategoryStyle;

    switch (idx) {
    case 0: // Load + Orthorectify — nothing rasterised to preview
        log("Preview", QString("Step 1 has no image preview — scene: %1")
            .arg(QString::fromStdString(state_.hyperionInputPath)));
        break;
    case 1:
        if (state_.surfaceReflectance) {
            if (state_.surfaceReflectance->bands >= 23)
                preview_->showRgb(*state_.surfaceReflectance, 22, 20, 12);
            else
                preview_->showSingleBand(*state_.surfaceReflectance, 0);
        }
        break;
    case 2:
        if (state_.surfaceObjectMask) {
            preview_->showCategorical(*state_.surfaceObjectMask, 0,
                { {0, CS{QColor(30,30,30), "Surface"}}, {1, CS{QColor(255,140,0), "Object"}} });
        }
        break;
    case 3: // Land Cover Map — not persisted; nothing to re-show
        log("Preview", "Step 4's result isn't cached — reopen the Land Cover Map dialog to view it.");
        break;
    case 4:
        if (state_.stackFused) preview_->showSingleBand(*state_.stackFused, 0);
        break;
    case 5:
        if (state_.builtUpMask) {
            preview_->showCategorical(*state_.builtUpMask, 0,
                { {0, CS{QColor(30,30,30), "Non built-up"}}, {1, CS{QColor(255,50,50), "Built-up"}} });
        }
        break;
    case 6: {
        const hsi::RasterCube* mask = nullptr;
        QString src;
        if (state_.builtUpMask)          { mask = &(*state_.builtUpMask);        src = "Built-up mask (Step 6)"; }
        else if (state_.surfaceObjectMask) { mask = &(*state_.surfaceObjectMask); src = "Surface/Object mask (Step 3)"; }
        if (mask) preview_->showCategorical(*mask, 0, { {1, CS{QColor(255,50,50), src}} });
        break;
    }
    case 7: {
        std::map<int, CS> pal;
        static const QColor kColors[] = {
            {220,50,50},{34,100,34},{100,200,80},{60,120,220},
            {210,160,80},{160,50,180},{50,190,190},{240,140,30}
        };
        if (state_.lulcSupervisedA) {
            if (state_.lulcClassToLabel) {
                int ci = 0;
                for (const auto& kv : *state_.lulcClassToLabel)
                    pal[kv.second] = CS{kColors[(ci++) % 8], QString::fromStdString(kv.first)};
            }
            pal[0] = CS{QColor(30,30,30), "Unclassified"};
            preview_->showCategorical(*state_.lulcSupervisedA, 0, pal);
        } else if (state_.lulcUnsupervised) {
            for (int i = 1; i <= 12; ++i) pal[i] = CS{kColors[(i - 1) % 8], QString("Cluster %1").arg(i)};
            pal[0] = CS{QColor(30,30,30), "Unclassified"};
            preview_->showCategorical(*state_.lulcUnsupervised, 0, pal);
        }
        break;
    }
    case 8: // Change Detection Matrix — numeric only, no raster to preview here
        if (state_.changeMatrix) {
            log("Preview", QString("Change matrix: %1 classes, %2 total pixels compared.")
                .arg(state_.changeMatrix->classIds.size())
                .arg([&]{ long t=0; for (auto& r : state_.changeMatrix->matrix) for (long v : r) t+=v; return t; }()));
        }
        break;
    default:
        break;
    }
}

// ── constructor ───────────────────────────────────────────────────────────
HsiMainWindow::HsiMainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("INDIGIS — Hyperspectral Analytics Module");
    setMinimumSize(1024, 700);
    setStyleSheet("QMainWindow{background:#1a252f;}"
                  "QMenuBar{background:#2c3e50;color:#ecf0f1;}"
                  "QMenuBar::item:selected{background:#34495e;}"
                  "QMenu{background:#2c3e50;color:#ecf0f1;border:1px solid #34495e;}"
                  "QMenu::item:selected{background:#3498db;}"
                  "QStatusBar{background:#2c3e50;color:#bdc3c7;font-size:11px;}");
    buildLayout();
    buildMenu();
    statusBar()->showMessage("Ready  —  Open the Hyperspectral menu or click a step button to begin.");
    refreshStepStatus();
    log("System", "INDIGIS Hyperspectral Analytics Module started. "
                  "EOS-04 team and Sentinel-2 team outputs will be merged at Step 4.");

    // Try to start PostgreSQL if it is not already running.
    // The server lives at /usr/local/pgsql (manual source build) with no
    // systemd unit, so we attempt pg_ctl start ourselves on launch.
    // Errors are non-fatal: the app runs in local-files-only mode if the DB
    // is unavailable.
    auto tryStartPostgres = [this]() {
        static const QString pgCtl  = "/usr/local/pgsql/bin/pg_ctl";
        static const QString pgData = "/usr/local/pgsql/data";
        if (!QFile::exists(pgCtl)) return; // not installed via source build
        // Quick status check -- returns 0 if server is running
        QProcess status;
        status.start(pgCtl, {"-D", pgData, "status"});
        if (status.waitForFinished(3000) && status.exitCode() == 0) return; // already up
        // Not running -- start it
        QProcess starter;
        starter.start(pgCtl, {"-D", pgData, "-l", pgData + "/logfile", "start"});
        if (!starter.waitForFinished(8000)) return;
        // Give it a moment to accept connections
        QThread::msleep(800);
    };
    tryStartPostgres();

    if (db_.connect()) {
        log("Database", "Connected — first load of any file still reads it locally; "
                         "repeat loads of the same file are served from the cache automatically.");
    } else {
        log("Database", QString("Could not connect (%1). Continuing in local-files-only mode; "
                                 "results will not be cached.").arg(QString::fromStdString(db_.lastError())));
    }
}

// ── logging ───────────────────────────────────────────────────────────────
void HsiMainWindow::log(const QString& stage, const QString& message) {
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    logConsole_->appendPlainText(QString("[%1] [%2]  %3").arg(ts, stage, message));
    statusBar()->showMessage(QString("[%1]  %2").arg(stage, message), 6000);
}

// ── Step 1 — Load + Orthorectify ──────────────────────────────────────────
void HsiMainWindow::onStep1_LoadOrtho() {
    // Prefer the directory the user actually picked from last time
    // (QSettings) over the fixed ~/Documents/Hyperspectral_Data/ heuristic --
    // falls back to that heuristic, then to no default, if nothing's been
    // remembered yet.
    QSettings settings("INDIGIS", "HyperspectralAnalyticsModule");
    QString defaultDir = settings.value("lastDir/hyperionInput").toString();
    if (defaultDir.isEmpty() || !QDir(defaultDir).exists()) {
        QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (docs.isEmpty()) docs = QDir::homePath() + "/Documents";
        QString hsiDir = docs + "/Hyperspectral_Data";
        defaultDir = QDir(hsiDir).exists() ? hsiDir : QString();
    }
    QString path = QFileDialog::getOpenFileName(this, "Select Hyperion scene or merged stack",
        defaultDir,
        "Raster / zip archive (*.tif *.tiff *.img *.hdr *.bil *.zip);;All files (*)");
    if (path.isEmpty()) return;
    settings.setValue("lastDir/hyperionInput", QFileInfo(path).absolutePath());

    try {
        std::string inputPath = path.toStdString();

        std::string resolved = ui_util::resolveRasterPath(inputPath);
        if (resolved != inputPath) {
            log("Archive", QString("Zip detected — extracted and using '%1'.")
                                .arg(QString::fromStdString(resolved)));
            inputPath = resolved;
        }

        bool already = hsi::RasterIO::looksOrthorectified(inputPath);

        if (already) {
            state_.hyperionInputPath = inputPath;
            state_.wasAlreadyOrthorectified = true;
            log("Orthorectifier", "Scene already has a valid geotransform/SRS — no warp needed.");
            QMessageBox::information(this, "Orthorectification",
                "Scene is already orthorectified.\nWill be used as-is for preprocessing.");
        } else {
            std::string warpedPath = inputPath + ".orthorectified.tif";
            log("Orthorectifier", "Raw GCPs detected — warping to map grid…");
            auto outcome = hsi::Orthorectifier::ensureOrthorectified(inputPath, warpedPath);
            state_.hyperionInputPath = outcome.outputPath;
            state_.wasAlreadyOrthorectified = false;
            log("Orthorectifier", QString("Warped → %1").arg(QString::fromStdString(outcome.outputPath)));
            QMessageBox::information(this, "Orthorectification complete",
                QString("Scene was NOT orthorectified.\nWarped using embedded GCPs → %1")
                    .arg(QString::fromStdString(outcome.outputPath)));
        }
        refreshStepStatus();
    } catch (const std::exception& e) {
        log("Orthorectifier", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Orthorectification failed", e.what());
    }
}

// ── Step 2 — DN → Surface Reflectance ────────────────────────────────────
void HsiMainWindow::onStep2_Preprocess() {
    PreprocessingDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshStepStatus();
        if (state_.surfaceReflectance && state_.surfaceReflectance->bands >= 21) {
            // False-colour RGB: idx20≈660nm(R), idx12≈549nm(G), idx3≈467nm(B)
            previewWidget()->showRgb(*state_.surfaceReflectance, 22, 20, 12);
            log("Preview", "Showing FCIR composite (R=NIR~901nm / G=Red~660nm / B=Green~549nm). Vegetation=red, Water=black, Built-up=cyan.");
        }
    }
}

// ── Step 3 — Surface / Object Mask ───────────────────────────────────────
void HsiMainWindow::onStep3_SurfaceObjectMask() {
    SurfaceObjectMaskDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshStepStatus();
        if (state_.surfaceObjectMask) {
            using CS = RasterPreviewWidget::CategoryStyle;
            previewWidget()->showCategorical(*state_.surfaceObjectMask, 0,
                { {0, CS{QColor(30,30,30), "Surface"}}, {1, CS{QColor(255,140,0), "Object"}} });
        }
    }
}

// ── Step 4b — Land Cover Mapper
void HsiMainWindow::onStep5b_LandCoverMapper() {
    // Show current reflectance RGB as context before dialog opens
    if (state_.surfaceReflectance && state_.surfaceReflectance->bands >= 21)
        previewWidget()->showRgb(*state_.surfaceReflectance, 22, 20, 12);
    LandCoverMapperDialog dlg(this, this);
    dlg.exec();
    // No post-dialog preview override — the dialog already shows the
    // categorical land cover map while it is open, and the user sees
    // it behind the results popup.  Overwriting with RGB here would
    // erase the colour map as soon as the dialog closes.
}

// ── Step 4 — PCA + Stack (was step 4) ────────────────────────────────────────
void HsiMainWindow::onStep4_PcaAndStack() {
    PcaStackDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshStepStatus();
        if (state_.stackFused && state_.stackFused->bands >= 21) {
            previewWidget()->showRgb(*state_.stackFused, 22, 20, 12);
            log("Preview", QString("Stack: %1 bands total. Showing FCIR false-colour (R=NIR/G=Red/B=Green).")
                .arg(state_.stackFused->bands));
        }
    }
}

// ── Step 5 — Built-up (SAM + SVM → 1 band) ───────────────────────────────
void HsiMainWindow::onStep5_BuiltUp() {
    BuiltUpClassificationDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshStepStatus();
        if (state_.builtUpMask) {
            using CS = RasterPreviewWidget::CategoryStyle;
            previewWidget()->showCategorical(*state_.builtUpMask, 0,
                { {0, CS{QColor(30,30,30), "Non built-up"}}, {1, CS{QColor(255,50,50), "Built-up"}} });
        }
    }
}

// ── Step 6 — Raster → Vector ─────────────────────────────────────────────
void HsiMainWindow::onStep6_RasterToVector() {
    // Use built-up mask if available; fall back to surface/object mask (Step 3)
    const hsi::RasterCube* maskToVectorize = nullptr;
    QString maskSource;
    if (state_.builtUpMask) {
        maskToVectorize = &(*state_.builtUpMask);
        maskSource = "Built-up mask (Step 6)";
    } else if (state_.surfaceObjectMask) {
        maskToVectorize = &(*state_.surfaceObjectMask);
        maskSource = "Surface/Object mask (Step 3)";
    } else {
        QMessageBox::warning(this, "No mask available",
            "Run Step 3 (Surface/Object Mask) or Step 6 (Built-up Classification) first.\n"
            "This step converts any binary mask to vector polygons (GeoJSON/Shapefile/GPKG).");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Save vector — " + maskSource,
        QString(), "GeoJSON (*.geojson);;Shapefile (*.shp);;GeoPackage (*.gpkg)");
    if (path.isEmpty()) return;

    QString driver = "GeoJSON";
    if (path.endsWith(".shp"))  driver = "ESRI Shapefile";
    if (path.endsWith(".gpkg")) driver = "GPKG";

    try {
        log("RasterToVector", "Vectorising: " + maskSource);
        hsi::RasterToVector::polygonize(*maskToVectorize, path.toStdString(), driver.toStdString());
        log("RasterToVector", QString("Vector polygons exported → %1").arg(path));
        // Show the mask that was vectorised so user sees what became polygons
        {
            using CS = RasterPreviewWidget::CategoryStyle;
            previewWidget()->showCategorical(*maskToVectorize, 0, {
                {1, CS{QColor(255,50,50), maskSource}}
            });
        }
        QMessageBox::information(this, "Export complete",
            QString("%1\nPolygons written to:\n%2").arg(maskSource).arg(path));
        refreshStepStatus();
    } catch (const std::exception& e) {
        log("RasterToVector", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Export failed", e.what());
    }
}

// ── Step 7 — LULC Classification ─────────────────────────────────────────
void HsiMainWindow::onStep7_Lulc() {
    LulcDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted) refreshStepStatus();
}

// ── Step 8 — Change Detection Matrix ─────────────────────────────────────
void HsiMainWindow::onStep8_ChangeDetection() {
    ChangeDetectionDialog dlg(this, this);
    if (dlg.exec() == QDialog::Accepted) refreshStepStatus();
}

// ── Run Full Pipeline (guided sequential) ────────────────────────────────
void HsiMainWindow::onRunAll() {
    QMessageBox::information(this, "Run Full Pipeline",
        "This will walk through all 9 steps in order.\n\n"
        "Step 5 (PCA & Stack): leave the fused data field blank to run\n"
        "standalone — the pipeline continues on Hyperion-only data.\n\n"
        "Cancel any step dialog to stop the pipeline at that point.");

    // Step 1 — Load + Orthorectify
    onStep1_LoadOrtho();
    if (state_.hyperionInputPath.empty()) return;

    // Step 2 — DN → Surface Reflectance
    { PreprocessingDialog d(this, this);
      if (d.exec() != QDialog::Accepted) return; }
    refreshStepStatus();
    if (state_.surfaceReflectance && state_.surfaceReflectance->bands >= 21)
        previewWidget()->showRgb(*state_.surfaceReflectance, 22, 20, 12);

    // Step 3 — Surface / Object Mask (optional; skip if cancelled)
    { SurfaceObjectMaskDialog d(this, this); d.exec(); }
    refreshStepStatus();

    // Step 4 — Land Cover Map
    { LandCoverMapperDialog d(this, this); d.exec(); }
    refreshStepStatus();

    // Step 5 — PCA + Stack (standalone if fused blank)
    { PcaStackDialog d(this, this);
      if (d.exec() != QDialog::Accepted) return; }
    refreshStepStatus();
    if (state_.stackFused && state_.stackFused->bands >= 21)
        previewWidget()->showRgb(*state_.stackFused, 22, 20, 12);

    // Step 6 — Built-up Classification
    { BuiltUpClassificationDialog d(this, this); d.exec(); }
    refreshStepStatus();
    if (state_.builtUpMask) {
        using CS = RasterPreviewWidget::CategoryStyle;
        previewWidget()->showCategorical(*state_.builtUpMask, 0,
            { {0, CS{QColor(30,30,30),"Non built-up"}}, {1, CS{QColor(255,50,50),"Built-up"}} });
    }

    // Step 7 — Raster → Vector Export
    onStep6_RasterToVector();

    // Step 8 — LULC Classification
    { LulcDialog d(this, this); d.exec(); }
    refreshStepStatus();

    // Step 9 — Change Detection Matrix
    { ChangeDetectionDialog d(this, this); d.exec(); }
    refreshStepStatus();

    log("Pipeline", "Full pipeline run complete.");
}

// ── Anomaly Detector (Tools menu) ────────────────────────────────────────
void HsiMainWindow::onToolAnomalyDetector() {
    if (!state_.surfaceReflectance.has_value()) {
        QMessageBox::information(this, "Step 2 required",
            "Run Step 2 (DN → Surface Reflectance) first.\n"
            "The anomaly detector needs the 198-band reflectance cube as input.");
        return;
    }
    AnomalyDetectorDialog dlg(this, this);
    dlg.exec();
}

// ── About ─────────────────────────────────────────────────────────────────
void HsiMainWindow::onAbout() {
    QMessageBox::about(this, "About — INDIGIS Hyperspectral Module",
        "<b>INDIGIS Hyperspectral Analytics Module</b><br><br>"
        "Implements the Hyperspectral processing pipeline for the<br>"
        "WESEE PoC (SAR–Optical–Hyperspectral Analytics Framework).<br><br>"
        "<b>This module (our team):</b><br>"
        "Hyperion Level-1T → Orthorectification → DN→Reflectance →<br>"
        "Surface/Object Mask → PCA → 206-band Stack →<br>"
        "SAM+SVM Built-up → Raster→Vector → LULC → Change Matrix<br><br>"
        "<b>Band merge point (Step 4):</b><br>"
        "0–197 &nbsp;Hyperion (this module)<br>"
        "198–end  Fused data TIFF (fusion team)<br>"
        "<br>"
        "Built on Qt 5.15 · GDAL · OpenCV · Eigen3 · C++17");
}
