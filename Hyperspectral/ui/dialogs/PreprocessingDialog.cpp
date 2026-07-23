//#include "PreprocessingDialog.h"
//#include "../MainWindow.h"
//#include "../Utils.h"
//#include "hsi/Database.h"
//#include "hsi/RasterIO.h"
//#include "hsi/SpectralIndices.h"
//#include "UI/MainWindow/mainwindow.h"
//#include "DialogStyle.h"

//#include <QFormLayout>
//#include <QVBoxLayout>
//#include <QHBoxLayout>
//#include <QPushButton>
//#include <QFileDialog>
//#include <QMessageBox>
//#include <QLabel>
//#include <QGroupBox>
//#include <QApplication>
//#include <QFileInfo>
//#include <QRegularExpression>
//#include <algorithm>
//#include <cmath>

//#include "gdal_priv.h"
//#include "ogr_spatialref.h"

//namespace {
//// Parses the EO-1 Hyperion scene ID embedded in the file name, e.g.
//// "EO1H1400262016057110K1_...": EO1 <sensor H> <path 3><row 3><year 4>
//// <day-of-year 3><station+version>. Returns true and fills year/dayOfYear
//// if the standard EO-1 naming convention is found.
//bool parseHyperionSceneId(const std::string& path, int& year, int& dayOfYear) {
//    QString base = QFileInfo(QString::fromStdString(path)).fileName();
//    QRegularExpression re("EO1H\\d{6}(\\d{4})(\\d{3})");
//    auto m = re.match(base);
//    if (!m.hasMatch()) return false;
//    year = m.captured(1).toInt();
//    dayOfYear = m.captured(2).toInt();
//    return (dayOfYear >= 1 && dayOfYear <= 366);
//}

//// Standard solar-position estimate (Cooper 1969 declination formula +
//// hour-angle from local solar time) for the scene's center latitude and
//// acquisition day-of-year, evaluated at Hyperion's nominal ~10:01 AM
//// local-solar-time equator-crossing (EO-1's sun-synchronous orbit design
//// value). Hyperion L1 GeoTIFFs don't carry an embedded sun-angle metadata
//// tag the way a Landsat MTL file does, so this is a physically grounded
//// ESTIMATE from the date + geometry, not a literal metadata read -- it's
//// logged clearly as such wherever it's used.
//double estimateSolarZenithDeg(double latDeg, int dayOfYear, double localSolarHour = 10.017) {
//    const double pi = 3.14159265358979323846;
//    double decl = 23.45 * std::sin(pi / 180.0 * (360.0 / 365.0) * (dayOfYear - 81));
//    double hourAngle = (localSolarHour - 12.0) * 15.0; // degrees
//    double latRad = latDeg * pi / 180.0;
//    double declRad = decl * pi / 180.0;
//    double haRad = hourAngle * pi / 180.0;
//    double cosZ = std::sin(latRad) * std::sin(declRad)
//                + std::cos(latRad) * std::cos(declRad) * std::cos(haRad);
//    cosZ = std::max(-1.0, std::min(1.0, cosZ));
//    return std::acos(cosZ) * 180.0 / pi;
//}

//// Opens the raster just far enough to read its geotransform/projection
//// (cheap -- GDAL doesn't load pixel data for this) and reprojects the
//// scene-center pixel to WGS84 lat/lon, needed for the solar-zenith
//// estimate above. Returns false if the file can't be opened or has no
//// usable georeferencing.
//bool sceneCenterLatLon(const std::string& path, double& lat, double& lon) {
//    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
//    if (!ds) return false;

//    double gt[6];
//    bool haveGt = (ds->GetGeoTransform(gt) == CE_None);
//    const char* wkt = ds->GetProjectionRef();
//    int w = ds->GetRasterXSize(), h = ds->GetRasterYSize();
//    if (!haveGt || !wkt || wkt[0] == '\0' || w <= 0 || h <= 0) {
//        GDALClose(ds);
//        return false;
//    }

//    double cx = gt[0] + gt[1] * (w / 2.0) + gt[2] * (h / 2.0);
//    double cy = gt[3] + gt[4] * (w / 2.0) + gt[5] * (h / 2.0);

//    OGRSpatialReference srcSrs;
//    srcSrs.importFromWkt(wkt);
//    OGRSpatialReference dstSrs;
//    dstSrs.SetWellKnownGeogCS("WGS84");
//#if GDAL_VERSION_MAJOR >= 3
//    srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//    dstSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
//#endif
//    OGRCoordinateTransformation* xf = OGRCreateCoordinateTransformation(&srcSrs, &dstSrs);
//    bool ok = false;
//    if (xf) {
//        ok = xf->Transform(1, &cx, &cy);
//        OGRCoordinateTransformation::DestroyCT(xf);
//    }
//    GDALClose(ds);
//    if (!ok) return false;
//    lon = cx;
//    lat = cy;
//    return true;
//}
//}

//PreprocessingDialog::PreprocessingDialog(HsiMainWindow* mainWindow, QWidget* parent)
//    : QDialog(parent), mw_(mainWindow) {
//    setWindowTitle("Pre-Processing");
//    setMinimumWidth(720);
//    setStyleSheet(indigisDialogStyleSheet());

//    inputPathEdit_ = new QLineEdit(this);
//    inputPathEdit_->setPlaceholderText("No file selected");
//    if (!mw_->appState().hyperionInputPath.empty())
//        inputPathEdit_->setText(QString::fromStdString(mw_->appState().hyperionInputPath));
//    auto* browseInputBtn = new QPushButton("Browse", this);

//    // Solar zenith angle and day of year are auto-detected in run() (see
//    // the header comment and run()'s own comments for exactly how), so
//    // these are read-only display fields -- not editable inputs -- shown
//    // once a run has computed them.
//    solarZenithSpin_ = new QDoubleSpinBox(this);
//    solarZenithSpin_->setRange(0.0, 89.0);
//    solarZenithSpin_->setValue(30.0);
//    solarZenithSpin_->setSuffix(" deg");
//    solarZenithSpin_->setReadOnly(true);
//    solarZenithSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
//    solarZenithSpin_->setToolTip("Auto-detected from scene metadata");

//    dayOfYearSpin_ = new QSpinBox(this);
//    dayOfYearSpin_->setRange(1, 366);
//    dayOfYearSpin_->setValue(171);
//    dayOfYearSpin_->setReadOnly(true);
//    dayOfYearSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
//    dayOfYearSpin_->setToolTip("Auto-detected from TIFF acquisition date");

//    pixelSizeSpin_ = new QDoubleSpinBox(this);
//    pixelSizeSpin_->setRange(1.0, 1000.0);
//    pixelSizeSpin_->setValue(30.0);
//    pixelSizeSpin_->setSuffix(" mm");

//    auto* inputRow = new QHBoxLayout();
//    inputRow->addWidget(inputPathEdit_);
//    inputRow->addWidget(browseInputBtn);

//    // "Dark Object Subtraction (DOS)" is the only surface-reflectance
//    // method offered (ELM removed per project spec) -- shown as a
//    // read-only field in the same style as every other row, not plain
//    // text, so it doesn't look out of place next to the real inputs.
//    auto* methodEdit = new QLineEdit("Dark Object Subtraction (DOS)", this);
//    methodEdit->setReadOnly(true);

//    auto* form = new QFormLayout();
//    form->addRow("Hyperion Scene", inputRow);
//    form->addRow("Solar Zenith Angle (Auto)", solarZenithSpin_);
//    form->addRow("Day of Year (Auto)", dayOfYearSpin_);
//    form->addRow("Ortho Output Pixel Size", pixelSizeSpin_);
//    form->addRow("Surface Reflectance Method", methodEdit);

//    statusValueLabel_ = new QLabel("Idle", this);
//    statusValueLabel_->setStyleSheet("font-weight: normal; color: #333;");
//    form->addRow("Status:", statusValueLabel_);

//    QPushButton *runBtn, *resetBtn, *cancelBtn;
//    auto* bottomRow = buildRunResetCancelRow(this, progressBar_, runBtn, resetBtn, cancelBtn);

//    auto* mainLayout = new QVBoxLayout(this);
//    mainLayout->addSpacing(12);
//    mainLayout->addLayout(form);
//    mainLayout->addSpacing(12);
//    mainLayout->addLayout(bottomRow);

//    connect(browseInputBtn, &QPushButton::clicked, this, &PreprocessingDialog::browseInput);
//    connect(runBtn, &QPushButton::clicked, this, &PreprocessingDialog::run);
//    connect(resetBtn, &QPushButton::clicked, this, &PreprocessingDialog::resetFields);
//    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
//}

//void PreprocessingDialog::resetFields() {
//    inputPathEdit_->clear();
//    solarZenithSpin_->setValue(30.0);
//    dayOfYearSpin_->setValue(171);
//    pixelSizeSpin_->setValue(30.0);
//    progressBar_->setValue(0);
//    statusValueLabel_->setText("Idle");
//}

//void PreprocessingDialog::browseInput() {
//    QString path = QFileDialog::getOpenFileName(this, "Select Hyperion scene", QString(),
//                                                  "Raster / zip archive (*.tif *.tiff *.img *.hdr *.bil *.zip);;All files (*)");
//    if (path.isEmpty()) return;

//    // Immediately show *something* -- for a zip of per-band Hyperion TIFFs,
//    // resolveRasterPath() below can take a while (it merges ~198 band files
//    // synchronously on this thread before returning). Without this, the
//    // field just sits blank with zero feedback until that finishes, which
//    // looks exactly like the selection didn't register at all. Show the
//    // raw path right away, force a repaint, and put up a wait cursor so
//    // it's clear something is happening in the background.
//    inputPathEdit_->setText(path + "  (resolving…)");
//    inputPathEdit_->repaint();
//    QApplication::setOverrideCursor(Qt::WaitCursor);
//    mw_->log("Archive", QString("Reading '%1' -- this can take a little while for a "
//                                 "per-band Hyperion zip (merging bands)…").arg(path));
//    QApplication::processEvents();

//    try {
//        std::string resolved = ui_util::resolveRasterPath(path.toStdString());
//        inputPathEdit_->setText(QString::fromStdString(resolved));
//        if (resolved != path.toStdString()) {
//            mw_->log("Archive", QString("Zip detected — extracted and using '%1'.")
//                                     .arg(QString::fromStdString(resolved)));
//        }
//    } catch (const std::exception& e) {
//        inputPathEdit_->clear();
//        QApplication::restoreOverrideCursor();
//        QMessageBox::critical(this, "Could not extract archive", e.what());
//        return;
//    }
//    QApplication::restoreOverrideCursor();
//}

//void PreprocessingDialog::run() {
//    if (inputPathEdit_->text().isEmpty()) {
//        QMessageBox::warning(this, "Missing input", "Please choose a Hyperion scene first.");
//        return;
//    }

//    try {
//        hsi::Pipeline::PreprocessConfig cfg;
//        cfg.inputPath = inputPathEdit_->text().toStdString();
//        cfg.orthoOutputPath = cfg.inputPath + ".orthorectified.tif";
//        cfg.orthoOptions.pixelSizeX = pixelSizeSpin_->value();
//        cfg.orthoOptions.pixelSizeY = pixelSizeSpin_->value();

//        // --- Auto-derive day-of-year and solar zenith angle -------------
//        int year = 0, dayOfYear = 171;
//        bool haveSceneId = parseHyperionSceneId(cfg.inputPath, year, dayOfYear);

//        double lat = 0.0, lon = 0.0;
//        bool haveCenter = sceneCenterLatLon(cfg.inputPath, lat, lon);
//        double solarZenithDeg = 30.0;
//        if (haveCenter)
//            solarZenithDeg = estimateSolarZenithDeg(lat, dayOfYear);

//        cfg.solarGeometry.solarZenithDeg = solarZenithDeg;
//        cfg.solarGeometry.dayOfYear = dayOfYear;

//        mw_->log("Preprocessing",
//            QString("Auto-derived: day-of-year=%1%2, solar zenith=%3 deg%4 "
//                    "(estimate; see PreprocessingDialog.cpp for the method).")
//                .arg(dayOfYear)
//                .arg(haveSceneId ? QString(" (from scene ID, year %1)").arg(year) : " (fallback default)")
//                .arg(solarZenithDeg, 0, 'f', 1)
//                .arg(haveCenter ? QString(" (scene center %1, %2)").arg(lat, 0, 'f', 3).arg(lon, 0, 'f', 3)
//                                : " (fallback default -- no georeferencing found yet)"));

//        cfg.darkObjectPercentile = 0.5;
//        cfg.correctionMethod = hsi::AtmosphericCorrector::CorrectionMethod::DOS;
//        cfg.esunPerBand = hsi::EsunTable::flatFallback(198, 1000.0);

//        mw_->log("Preprocessing", "Starting (DOS)...");

//        // --- Stage tracking ---
//        int currentStage = 0;
//        const int TOTAL_STAGES = 5;  // Orthorectification -> Radiance -> DOS -> Save -> Preview

//        statusValueLabel_->setText("Checking cache...");
//        progressBar_->setValue(0);

//        std::string sourceKey = hsi::Database::computeSourceKey(cfg.inputPath);
//        const std::string productTag = "hyperion_surface_reflectance_dos";

//        hsi::Pipeline::PreprocessResult result;
//        bool fromCache = mw_->database().tryLoad(sourceKey, productTag, result.surfaceReflectance);
//        if (fromCache) {
//            result.wasAlreadyOrthorectified = true;
//            statusValueLabel_->setText("Loaded from database cache");
//            progressBar_->setValue(100);
//            mw_->log("Preprocessing", "Loaded surface reflectance from database cache — "
//                                       "local file was not reprocessed.");
//        } else {
//            // Pipeline with progress tracking
//            result = hsi::Pipeline::preprocess(cfg, [this, &currentStage](const std::string& msg) {
//                // Update status text
//                statusValueLabel_->setText(QString::fromStdString(msg));

//                // Map status messages to stages and update progress bar
//                QString qmsg = QString::fromStdString(msg);
//                if (qmsg.contains("Orthorectifying", Qt::CaseInsensitive) ||
//                    qmsg.contains("Warping", Qt::CaseInsensitive)) {
//                    currentStage = 1;
//                    progressBar_->setValue(20);  // 20%
//                } else if (qmsg.contains("Radiance", Qt::CaseInsensitive)) {
//                    currentStage = 2;
//                    progressBar_->setValue(40);  // 40%
//                } else if (qmsg.contains("Dark Object", Qt::CaseInsensitive) ||
//                           qmsg.contains("DOS", Qt::CaseInsensitive) ||
//                           qmsg.contains("Surface Reflectance", Qt::CaseInsensitive)) {
//                    currentStage = 3;
//                    progressBar_->setValue(60);  // 60%
//                } else if (qmsg.contains("Saving", Qt::CaseInsensitive) ||
//                           qmsg.contains("Writing", Qt::CaseInsensitive)) {
//                    currentStage = 4;
//                    progressBar_->setValue(80);  // 80%
//                } else {
//                    // If we can't map the message, advance by one stage from where we are
//                    // (but don't exceed the total)
//                    if (currentStage < TOTAL_STAGES - 1) {
//                        currentStage++;
//                        progressBar_->setValue((currentStage * 100) / TOTAL_STAGES);
//                    }
//                }

//                QApplication::processEvents();
//            });

//            // Store in cache (stage 5 - final)
//            progressBar_->setValue(90);
//            statusValueLabel_->setText("Caching result...");
//            QApplication::processEvents();

//            if (!mw_->database().store(sourceKey, productTag, result.surfaceReflectance, cfg.inputPath)) {
//                mw_->log("Preprocessing", QString("Note: result was NOT cached to the database (%1). "
//                    "It will be recomputed from the local file next time.")
//                    .arg(QString::fromStdString(mw_->database().lastError())));
//            }
//        }

//        // Final stage - preview and completion
//        progressBar_->setValue(95);
//        statusValueLabel_->setText("Generating preview...");
//        QApplication::processEvents();

//        mw_->appState().hyperionInputPath = cfg.inputPath;
//        mw_->appState().surfaceReflectance = result.surfaceReflectance;
//        mw_->appState().wasAlreadyOrthorectified = result.wasAlreadyOrthorectified;

//        // Pick the bands closest to the requested FCC recipe
//        int rBand = hsi::HyperionBandFinder::findClosest(result.surfaceReflectance, 840.0);
//        int gBand = hsi::HyperionBandFinder::findClosest(result.surfaceReflectance, 660.0);
//        int bBand = hsi::HyperionBandFinder::findClosest(result.surfaceReflectance, 570.0);
//        bool haveFccBands = (rBand >= 0 && gBand >= 0 && bBand >= 0);

//        {
//            std::string previewPath = cfg.inputPath + ".surface_reflectance.tif";
//            try {
//                if (haveFccBands) {
//                    hsi::RasterCube fcc;
//                    fcc.allocate(result.surfaceReflectance.width, result.surfaceReflectance.height, 3);
//                    fcc.geoTransform = result.surfaceReflectance.geoTransform;
//                    fcc.projectionWkt = result.surfaceReflectance.projectionWkt;
//                    fcc.hasNoData = result.surfaceReflectance.hasNoData;
//                    fcc.noDataValue = result.surfaceReflectance.noDataValue;
//                    size_t n = result.surfaceReflectance.pixelCount();
//                    const int srcBands[3] = { rBand, gBand, bBand };
//                    const char* names[3] = { "Red(660nm)", "Green(570nm)", "Blue(840nm)" };
//                    for (int dst = 0; dst < 3; ++dst) {
//                        size_t srcBase = static_cast<size_t>(srcBands[dst]) * n;
//                        size_t dstBase = static_cast<size_t>(dst) * n;
//                        std::copy(result.surfaceReflectance.data.begin() + srcBase,
//                                  result.surfaceReflectance.data.begin() + srcBase + n,
//                                  fcc.data.begin() + dstBase);
//                        fcc.bandNames[dst] = names[dst];
//                    }
//                    hsi::RasterIO::saveCube(fcc, previewPath);
//                } else {
//                    hsi::RasterIO::saveCube(result.surfaceReflectance, previewPath);
//                }
//                if (mapWindow_)
//                    mapWindow_->addRasterLayerToMap(QString::fromStdString(previewPath));
//            } catch (const std::exception& e) {
//                mw_->log("Preprocessing", QString("Note: couldn't save map preview (%1).").arg(e.what()));
//            }
//        }

//        mw_->log("Preprocessing", QString("Done. %1 bands, %2x%3. Orthorectification was %4.")
//                      .arg(result.surfaceReflectance.bands)
//                      .arg(result.surfaceReflectance.width)
//                      .arg(result.surfaceReflectance.height)
//                      .arg(result.wasAlreadyOrthorectified ? "already present (skipped)" : "performed (warped)"));

//        // Display the preprocessed cube as FCC
//        if (haveFccBands) {
//            mw_->previewWidget()->showRgb(result.surfaceReflectance, rBand, gBand, bBand);
//            mw_->log("Preprocessing", QString("Preview: FCC using bands closest to 660/570/840 nm "
//                                               "(cube indices %1/%2/%3).").arg(rBand).arg(gBand).arg(bBand));
//        } else {
//            int midBand = result.surfaceReflectance.bands / 2;
//            mw_->previewWidget()->showSingleBand(result.surfaceReflectance, midBand);
//        }

//        // All done!
//        progressBar_->setValue(100);
//        statusValueLabel_->setText("Done");
//        QApplication::processEvents();

//        QMessageBox::information(this, "Preprocessing complete",
//            QString("Surface reflectance cube ready: %1 bands, %2x%3 pixels.")
//                .arg(result.surfaceReflectance.bands)
//                .arg(result.surfaceReflectance.width)
//                .arg(result.surfaceReflectance.height));
//        accept();
//    } catch (const std::exception& e) {
//        progressBar_->setValue(0);
//        statusValueLabel_->setText("Failed");
//        mw_->log("Preprocessing", QString("ERROR: %1").arg(e.what()));
//        QMessageBox::critical(this, "Preprocessing failed", e.what());
//    }
//}

//#include "PreprocessingDialog.h"
//#include "../MainWindow.h"
//#include "../Utils.h"
//#include "hsi/Database.h"
//#include "hsi/RasterIO.h"
//#include "UI/MainWindow/mainwindow.h"

//#include <QFormLayout>
//#include <QVBoxLayout>
//#include <QHBoxLayout>
//#include <QPushButton>
//#include <QFileDialog>
//#include <QMessageBox>
//#include <QLabel>
//#include <QGroupBox>

//PreprocessingDialog::PreprocessingDialog(HsiMainWindow* mainWindow, QWidget* parent)
//    : QDialog(parent), mw_(mainWindow) {
//    setWindowTitle("Preprocessing: Orthorectification → Radiance → Surface Reflectance");
//    setMinimumWidth(520);

//    inputPathEdit_ = new QLineEdit(this);
//    if (!mw_->appState().hyperionInputPath.empty())
//        inputPathEdit_->setText(QString::fromStdString(mw_->appState().hyperionInputPath));
//    auto* browseInputBtn = new QPushButton("Browse...", this);

//    solarZenithSpin_ = new QDoubleSpinBox(this);
//    solarZenithSpin_->setRange(0.0, 89.0);
//    solarZenithSpin_->setValue(30.0);
//    solarZenithSpin_->setSuffix(" deg");

//    dayOfYearSpin_ = new QSpinBox(this);
//    dayOfYearSpin_->setRange(1, 366);
//    dayOfYearSpin_->setValue(171);

//    darkObjectPercentileSpin_ = new QDoubleSpinBox(this);
//    darkObjectPercentileSpin_->setRange(0.0, 10.0);
//    darkObjectPercentileSpin_->setDecimals(2);
//    darkObjectPercentileSpin_->setValue(0.5);
//    darkObjectPercentileSpin_->setSuffix(" pct");

//    pixelSizeSpin_ = new QDoubleSpinBox(this);
//    pixelSizeSpin_->setRange(1.0, 1000.0);
//    pixelSizeSpin_->setValue(30.0);
//    pixelSizeSpin_->setSuffix(" m");

//    correctionMethodCombo_ = new QComboBox(this);
//    correctionMethodCombo_->addItem("Dark Object Subtraction (DOS, simplified QUAC-style)");
//    correctionMethodCombo_->addItem("Empirical Line Method (ELM)");

//    auto* inputRow = new QHBoxLayout();
//    inputRow->addWidget(inputPathEdit_);
//    inputRow->addWidget(browseInputBtn);

//    auto* form = new QFormLayout();
//    form->addRow("Hyperion DN scene:", inputRow);
//    form->addRow("Solar zenith angle:", solarZenithSpin_);
//    form->addRow("Day of year:", dayOfYearSpin_);
//    form->addRow("Ortho output pixel size:", pixelSizeSpin_);
//    form->addRow("Surface reflectance method:", correctionMethodCombo_);

//    // --- DOS-specific parameters ---
//    dosGroup_ = new QGroupBox("Dark Object Subtraction parameters", this);
//    auto* dosForm = new QFormLayout();
//    dosForm->addRow("Dark-object percentile:", darkObjectPercentileSpin_);
//    dosGroup_->setLayout(dosForm);

//    // --- ELM-specific parameters ---
//    elmSampleCsvEdit_ = new QLineEdit(this);
//    elmSampleCsvEdit_->setPlaceholderText("CSV with 'dark,row,col' and 'bright,row,col' rows");
//    auto* browseElmBtn = new QPushButton("Browse...", this);
//    auto* elmCsvRow = new QHBoxLayout();
//    elmCsvRow->addWidget(elmSampleCsvEdit_);
//    elmCsvRow->addWidget(browseElmBtn);

//    elmDarkReflectanceSpin_ = new QDoubleSpinBox(this);
//    elmDarkReflectanceSpin_->setRange(0.0, 1.0);
//    elmDarkReflectanceSpin_->setDecimals(3);
//    elmDarkReflectanceSpin_->setValue(0.01);

//    elmBrightReflectanceSpin_ = new QDoubleSpinBox(this);
//    elmBrightReflectanceSpin_->setRange(0.0, 1.0);
//    elmBrightReflectanceSpin_->setDecimals(3);
//    elmBrightReflectanceSpin_->setValue(0.60);

//    elmGroup_ = new QGroupBox("Empirical Line Method calibration targets", this);
//    auto* elmForm = new QFormLayout();
//    elmForm->addRow("Dark/bright sample CSV:", elmCsvRow);
//    elmForm->addRow("Known dark-target reflectance:", elmDarkReflectanceSpin_);
//    elmForm->addRow("Known bright-target reflectance:", elmBrightReflectanceSpin_);
//    elmGroup_->setLayout(elmForm);
//    elmGroup_->setVisible(false);


//    auto* runBtn = new QPushButton("Run Preprocessing", this);
//    runBtn->setDefault(true);

//    auto* mainLayout = new QVBoxLayout(this);
//    auto* group = new QGroupBox("Parameters", this);
//    group->setLayout(form);
//    mainLayout->addWidget(group);
//    mainLayout->addWidget(dosGroup_);
//    mainLayout->addWidget(elmGroup_);
//    mainLayout->addWidget(runBtn);

//    connect(browseInputBtn, &QPushButton::clicked, this, &PreprocessingDialog::browseInput);
//    connect(browseElmBtn, &QPushButton::clicked, this, &PreprocessingDialog::browseElmSamples);
//    connect(runBtn, &QPushButton::clicked, this, &PreprocessingDialog::run);
//    connect(correctionMethodCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
//        bool isElm = (idx == 1);
//        elmGroup_->setVisible(isElm);
//        dosGroup_->setVisible(!isElm);
//    });
//}

//void PreprocessingDialog::browseInput() {
//    QString path = QFileDialog::getOpenFileName(this, "Select Hyperion scene", QString(),
//                                                  "Raster / zip archive (*.tif *.tiff *.img *.hdr *.bil *.zip);;All files (*)");
//    if (path.isEmpty()) return;
//    try {
//        std::string resolved = ui_util::resolveRasterPath(path.toStdString());
//        inputPathEdit_->setText(QString::fromStdString(resolved));
//        if (resolved != path.toStdString()) {
//            mw_->log("Archive", QString("Zip detected — extracted and using '%1'.")
//                                     .arg(QString::fromStdString(resolved)));
//        }
//    } catch (const std::exception& e) {
//        QMessageBox::critical(this, "Could not extract archive", e.what());
//    }
//}

//void PreprocessingDialog::browseElmSamples() {
//    QString path = QFileDialog::getOpenFileName(this, "Select dark/bright calibration sample CSV",
//                                                  QString(), "CSV files (*.csv);;All files (*)");
//    if (!path.isEmpty()) elmSampleCsvEdit_->setText(path);
//}

//void PreprocessingDialog::run() {
//    if (inputPathEdit_->text().isEmpty()) {
//        QMessageBox::warning(this, "Missing input", "Please choose a Hyperion scene first.");
//        return;
//    }

//    try {
//        hsi::Pipeline::PreprocessConfig cfg;
//        cfg.inputPath = inputPathEdit_->text().toStdString();
//        cfg.orthoOutputPath = cfg.inputPath + ".orthorectified.tif";
//        cfg.orthoOptions.pixelSizeX = pixelSizeSpin_->value();
//        cfg.orthoOptions.pixelSizeY = pixelSizeSpin_->value();
//        cfg.solarGeometry.solarZenithDeg = solarZenithSpin_->value();
//        cfg.solarGeometry.dayOfYear = dayOfYearSpin_->value();
//        cfg.darkObjectPercentile = darkObjectPercentileSpin_->value();

//        bool useElm = (correctionMethodCombo_->currentIndex() == 1);
//        cfg.correctionMethod = useElm ? hsi::AtmosphericCorrector::CorrectionMethod::ELM
//                                       : hsi::AtmosphericCorrector::CorrectionMethod::DOS;
//        if (useElm) {
//            if (elmSampleCsvEdit_->text().isEmpty()) {
//                QMessageBox::warning(this, "Missing calibration samples",
//                    "ELM needs a sample CSV with 'dark,row,col' and 'bright,row,col' rows.");
//                return;
//            }
//            cfg.elmSamplePixels = ui_util::loadSampleCsv(elmSampleCsvEdit_->text().toStdString());
//            cfg.elmDarkKnownReflectance = elmDarkReflectanceSpin_->value();
//            cfg.elmBrightKnownReflectance = elmBrightReflectanceSpin_->value();
//        }

//        cfg.esunPerBand = hsi::EsunTable::flatFallback(198, 1000.0);

//        mw_->log("Preprocessing", QString("Starting (%1)...").arg(useElm ? "ELM" : "DOS"));

//        std::string sourceKey = hsi::Database::computeSourceKey(cfg.inputPath);
//        const std::string productTag = std::string("hyperion_surface_reflectance_") + (useElm ? "elm" : "dos");

//        hsi::Pipeline::PreprocessResult result;
//        bool fromCache = mw_->database().tryLoad(sourceKey, productTag, result.surfaceReflectance);
//        if (fromCache) {
//            result.wasAlreadyOrthorectified = true;
//            mw_->log("Preprocessing", "Loaded surface reflectance from database cache — "
//                                       "local file was not reprocessed.");
//        } else {
//            result = hsi::Pipeline::preprocess(cfg);
//            if (!mw_->database().store(sourceKey, productTag, result.surfaceReflectance, cfg.inputPath)) {
//                mw_->log("Preprocessing", QString("Note: result was NOT cached to the database (%1). "
//                    "It will be recomputed from the local file next time.")
//                    .arg(QString::fromStdString(mw_->database().lastError())));
//            }
//        }

//        mw_->appState().hyperionInputPath = cfg.inputPath;
//        mw_->appState().surfaceReflectance = result.surfaceReflectance;
//        mw_->appState().wasAlreadyOrthorectified = result.wasAlreadyOrthorectified;

//        {
//            std::string previewPath = cfg.inputPath + ".surface_reflectance.tif";
//            try {
//                hsi::RasterIO::saveCube(result.surfaceReflectance, previewPath);
//                if (mapWindow_)
//                    mapWindow_->addRasterLayerToMap(QString::fromStdString(previewPath));
//            } catch (const std::exception& e) {
//                mw_->log("Preprocessing", QString("Note: couldn't save map preview (%1).").arg(e.what()));
//            }
//        }

//        mw_->log("Preprocessing", QString("Done. %1 bands, %2x%3. Orthorectification was %4.")
//                      .arg(result.surfaceReflectance.bands)
//                      .arg(result.surfaceReflectance.width)
//                      .arg(result.surfaceReflectance.height)
//                      .arg(result.wasAlreadyOrthorectified ? "already present (skipped)" : "performed (warped)"));

//        int midBand = result.surfaceReflectance.bands / 2;
//        mw_->previewWidget()->showSingleBand(result.surfaceReflectance, midBand);

//        QMessageBox::information(this, "Preprocessing complete",
//            QString("Surface reflectance cube ready: %1 bands, %2x%3 pixels.")
//                .arg(result.surfaceReflectance.bands)
//                .arg(result.surfaceReflectance.width)
//                .arg(result.surfaceReflectance.height));
//        accept();
//    } catch (const std::exception& e) {
//        mw_->log("Preprocessing", QString("ERROR: %1").arg(e.what()));
//        QMessageBox::critical(this, "Preprocessing failed", e.what());
//    }
//}


#include "PreprocessingDialog.h"
#include "../MainWindow.h"
#include "../Utils.h"
#include "hsi/Database.h"
#include "hsi/RasterIO.h"
#include "hsi/SpectralIndices.h"
#include "UI/MainWindow/mainwindow.h"
#include "DialogStyle.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>
#include <QApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

#include "gdal_priv.h"
#include "ogr_spatialref.h"

namespace {
// Parses the EO-1 Hyperion scene ID embedded in the file name, e.g.
// "EO1H1400262016057110K1_...": EO1 <sensor H> <path 3><row 3><year 4>
// <day-of-year 3><station+version>. Returns true and fills year/dayOfYear
// if the standard EO-1 naming convention is found.
bool parseHyperionSceneId(const std::string& path, int& year, int& dayOfYear) {
    QString base = QFileInfo(QString::fromStdString(path)).fileName();
    QRegularExpression re("EO1H\\d{6}(\\d{4})(\\d{3})");
    auto m = re.match(base);
    if (!m.hasMatch()) return false;
    year = m.captured(1).toInt();
    dayOfYear = m.captured(2).toInt();
    return (dayOfYear >= 1 && dayOfYear <= 366);
}

// Standard solar-position estimate (Cooper 1969 declination formula +
// hour-angle from local solar time) for the scene's center latitude and
// acquisition day-of-year, evaluated at Hyperion's nominal ~10:01 AM
// local-solar-time equator-crossing (EO-1's sun-synchronous orbit design
// value). Hyperion L1 GeoTIFFs don't carry an embedded sun-angle metadata
// tag the way a Landsat MTL file does, so this is a physically grounded
// ESTIMATE from the date + geometry, not a literal metadata read -- it's
// logged clearly as such wherever it's used.
double estimateSolarZenithDeg(double latDeg, int dayOfYear, double localSolarHour = 10.017) {
    const double pi = 3.14159265358979323846;
    double decl = 23.45 * std::sin(pi / 180.0 * (360.0 / 365.0) * (dayOfYear - 81));
    double hourAngle = (localSolarHour - 12.0) * 15.0; // degrees
    double latRad = latDeg * pi / 180.0;
    double declRad = decl * pi / 180.0;
    double haRad = hourAngle * pi / 180.0;
    double cosZ = std::sin(latRad) * std::sin(declRad)
                + std::cos(latRad) * std::cos(declRad) * std::cos(haRad);
    cosZ = std::max(-1.0, std::min(1.0, cosZ));
    return std::acos(cosZ) * 180.0 / pi;
}


bool sceneCenterLatLon(const std::string& path, double& lat, double& lon) {
    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (!ds) return false;

    double gt[6];
    bool haveGt = (ds->GetGeoTransform(gt) == CE_None);
    const char* wkt = ds->GetProjectionRef();
    int w = ds->GetRasterXSize(), h = ds->GetRasterYSize();
    if (!haveGt || !wkt || wkt[0] == '\0' || w <= 0 || h <= 0) {
        GDALClose(ds);
        return false;
    }

    double cx = gt[0] + gt[1] * (w / 2.0) + gt[2] * (h / 2.0);
    double cy = gt[3] + gt[4] * (w / 2.0) + gt[5] * (h / 2.0);

    OGRSpatialReference srcSrs;
    srcSrs.importFromWkt(wkt);
    OGRSpatialReference dstSrs;
    dstSrs.SetWellKnownGeogCS("WGS84");
#if GDAL_VERSION_MAJOR >= 3
    srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    dstSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
#endif
    OGRCoordinateTransformation* xf = OGRCreateCoordinateTransformation(&srcSrs, &dstSrs);
    bool ok = false;
    if (xf) {
        ok = xf->Transform(1, &cx, &cy);
        OGRCoordinateTransformation::DestroyCT(xf);
    }
    GDALClose(ds);
    if (!ok) return false;
    lon = cx;
    lat = cy;
    return true;
}
}

PreprocessingDialog::PreprocessingDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Pre-Processing");
    setMinimumWidth(720);
    setStyleSheet(indigisDialogStyleSheet());

    inputPathEdit_ = new QLineEdit(this);
    inputPathEdit_->setPlaceholderText("No file selected");
    if (!mw_->appState().hyperionInputPath.empty())
        inputPathEdit_->setText(QString::fromStdString(mw_->appState().hyperionInputPath));
    auto* browseInputBtn = new QPushButton("Browse", this);

    solarZenithSpin_ = new QDoubleSpinBox(this);
    solarZenithSpin_->setRange(0.0, 89.0);
    solarZenithSpin_->setValue(0);
    solarZenithSpin_->setSuffix(" deg");
    solarZenithSpin_->setReadOnly(true);
    solarZenithSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    solarZenithSpin_->setToolTip("Auto-detected from scene metadata");

    dayOfYearSpin_ = new QSpinBox(this);
    dayOfYearSpin_->setRange(1, 366);
    dayOfYearSpin_->setValue(0);
    dayOfYearSpin_->setReadOnly(true);
    dayOfYearSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    dayOfYearSpin_->setToolTip("Auto-detected from TIFF acquisition date");

    pixelSizeSpin_ = new QDoubleSpinBox(this);
    pixelSizeSpin_->setRange(1.0, 1000.0);
    pixelSizeSpin_->setValue(30.0);
    pixelSizeSpin_->setSuffix(" mm");

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(inputPathEdit_);
    inputRow->addWidget(browseInputBtn);


    auto* methodEdit = new QLineEdit("Dark Object Subtraction (DOS)", this);
    methodEdit->setReadOnly(true);

    auto* form = new QFormLayout();
    form->addRow("Hyperion Scene", inputRow);
    form->addRow("Solar Zenith Angle ", solarZenithSpin_);
    form->addRow("Day of Year ", dayOfYearSpin_);
    form->addRow("Ortho Output Pixel Size", pixelSizeSpin_);
    form->addRow("Surface Reflectance Method", methodEdit);

    statusValueLabel_ = new QLabel("Idle", this);
    statusValueLabel_->setStyleSheet("font-weight: normal; color: #333;");
    form->addRow("Status:", statusValueLabel_);

    QPushButton *runBtn, *resetBtn, *cancelBtn;
    auto* bottomRow = buildRunResetCancelRow(this, progressBar_, runBtn, resetBtn, cancelBtn);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(form);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(bottomRow);

    connect(browseInputBtn, &QPushButton::clicked, this, &PreprocessingDialog::browseInput);
    connect(runBtn, &QPushButton::clicked, this, &PreprocessingDialog::run);
    connect(resetBtn, &QPushButton::clicked, this, &PreprocessingDialog::resetFields);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void PreprocessingDialog::resetFields() {
    inputPathEdit_->clear();
    solarZenithSpin_->setValue(30.0);
    dayOfYearSpin_->setValue(171);
    pixelSizeSpin_->setValue(30.0);
    progressBar_->setValue(0);
    statusValueLabel_->setText("Idle");
}

void PreprocessingDialog::browseInput() {
    QString path = QFileDialog::getOpenFileName(this, "Select Hyperion scene", QString(),
                                                  "Raster / zip archive (*.tif *.tiff *.img *.hdr *.bil *.zip);;All files (*)");
    if (path.isEmpty()) return;
    inputPathEdit_->setText(path + "  (resolving…)");
    inputPathEdit_->repaint();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    mw_->log("Archive", QString("Reading '%1' -- this can take a little while for a "
                                 "per-band Hyperion zip (merging bands)…").arg(path));
    QApplication::processEvents();

    try {
        std::string resolved = ui_util::resolveRasterPath(path.toStdString());
        inputPathEdit_->setText(QString::fromStdString(resolved));
        if (resolved != path.toStdString()) {
            mw_->log("Archive", QString("Zip detected — extracted and using '%1'.")
                                     .arg(QString::fromStdString(resolved)));
        }
    } catch (const std::exception& e) {
        inputPathEdit_->clear();
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "Could not extract archive", e.what());
        return;
    }
    QApplication::restoreOverrideCursor();
}

void PreprocessingDialog::run() {
    if (inputPathEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "Missing input", "Please choose a Hyperion scene first.");
        return;
    }

    try {
        hsi::Pipeline::PreprocessConfig cfg;
        cfg.inputPath = inputPathEdit_->text().toStdString();
        cfg.orthoOutputPath = cfg.inputPath + ".orthorectified.tif";
        cfg.orthoOptions.pixelSizeX = pixelSizeSpin_->value();
        cfg.orthoOptions.pixelSizeY = pixelSizeSpin_->value();

        int year = 0, dayOfYear = 171;
        bool haveSceneId = parseHyperionSceneId(cfg.inputPath, year, dayOfYear);

        double lat = 0.0, lon = 0.0;
        bool haveCenter = sceneCenterLatLon(cfg.inputPath, lat, lon);
        double solarZenithDeg = 30.0;
        if (haveCenter)
            solarZenithDeg = estimateSolarZenithDeg(lat, dayOfYear);

        cfg.solarGeometry.solarZenithDeg = solarZenithDeg;
        cfg.solarGeometry.dayOfYear = dayOfYear;

        mw_->log("Preprocessing",
            QString("Auto-derived: day-of-year=%1%2, solar zenith=%3 deg%4 "
                    "(estimate; see PreprocessingDialog.cpp for the method).")
                .arg(dayOfYear)
                .arg(haveSceneId ? QString(" (from scene ID, year %1)").arg(year) : " (fallback default)")
                .arg(solarZenithDeg, 0, 'f', 1)
                .arg(haveCenter ? QString(" (scene center %1, %2)").arg(lat, 0, 'f', 3).arg(lon, 0, 'f', 3)
                                : " (fallback default -- no georeferencing found yet)"));

        cfg.darkObjectPercentile = 0.5;

        cfg.correctionMethod = hsi::AtmosphericCorrector::CorrectionMethod::DOS;

        cfg.esunPerBand = hsi::EsunTable::flatFallback(198, 1000.0);

        mw_->log("Preprocessing", "Starting (DOS)...");

        statusValueLabel_->setText("Checking cache...");
        std::string sourceKey = hsi::Database::computeSourceKey(cfg.inputPath);
        const std::string productTag = "hyperion_surface_reflectance_dos";

        hsi::Pipeline::PreprocessResult result;
        bool fromCache = mw_->database().tryLoad(sourceKey, productTag, result.surfaceReflectance);
        if (fromCache) {
            result.wasAlreadyOrthorectified = true;
            statusValueLabel_->setText("Loaded from database cache");
            mw_->log("Preprocessing", "Loaded surface reflectance from database cache — "
                                       "local file was not reprocessed.");
        } else {
            result = hsi::Pipeline::preprocess(cfg, [this](const std::string& msg, int percent) {
                statusValueLabel_->setText(QString::fromStdString(msg));
                progressBar_->setValue(percent);
                QApplication::processEvents();
            });
            if (!mw_->database().store(sourceKey, productTag, result.surfaceReflectance, cfg.inputPath)) {
                mw_->log("Preprocessing", QString("Note: result was NOT cached to the database (%1). "
                    "It will be recomputed from the local file next time.")
                    .arg(QString::fromStdString(mw_->database().lastError())));
            }
        }
        statusValueLabel_->setText("Done");

        mw_->appState().hyperionInputPath = cfg.inputPath;
        mw_->appState().surfaceReflectance = result.surfaceReflectance;
        mw_->appState().wasAlreadyOrthorectified = result.wasAlreadyOrthorectified;

        int rBand = hsi::HyperionBandFinder::findClosest(result.surfaceReflectance, 840.0);
        int gBand = hsi::HyperionBandFinder::findClosest(result.surfaceReflectance, 660.0);
        int bBand = hsi::HyperionBandFinder::findClosest(result.surfaceReflectance, 570.0);
        bool haveFccBands = (rBand >= 0 && gBand >= 0 && bBand >= 0);

        {
            std::string previewPath = cfg.inputPath + ".surface_reflectance.tif";
            try {
                if (haveFccBands) {
                    hsi::RasterCube fcc;
                    fcc.allocate(result.surfaceReflectance.width, result.surfaceReflectance.height, 3);
                    fcc.geoTransform = result.surfaceReflectance.geoTransform;
                    fcc.projectionWkt = result.surfaceReflectance.projectionWkt;
                    fcc.hasNoData = result.surfaceReflectance.hasNoData;
                    fcc.noDataValue = result.surfaceReflectance.noDataValue;
                    size_t n = result.surfaceReflectance.pixelCount();
                    const int srcBands[3] = { rBand, gBand, bBand };
                    const char* names[3] = { "Red(660nm)", "Green(570nm)", "Blue(840nm)" };
                    for (int dst = 0; dst < 3; ++dst) {
                        size_t srcBase = static_cast<size_t>(srcBands[dst]) * n;
                        size_t dstBase = static_cast<size_t>(dst) * n;
                        std::copy(result.surfaceReflectance.data.begin() + srcBase,
                                  result.surfaceReflectance.data.begin() + srcBase + n,
                                  fcc.data.begin() + dstBase);
                        fcc.bandNames[dst] = names[dst];
                    }
                    hsi::RasterIO::saveCube(fcc, previewPath);
                } else {
                    hsi::RasterIO::saveCube(result.surfaceReflectance, previewPath);
                }
                if (mapWindow_)
                    mapWindow_->addRasterLayerToMap(QString::fromStdString(previewPath));
            } catch (const std::exception& e) {
                mw_->log("Preprocessing", QString("Note: couldn't save map preview (%1).").arg(e.what()));
            }
        }

        mw_->log("Preprocessing", QString("Done. %1 bands, %2x%3. Orthorectification was %4.")
                      .arg(result.surfaceReflectance.bands)
                      .arg(result.surfaceReflectance.width)
                      .arg(result.surfaceReflectance.height)
                      .arg(result.wasAlreadyOrthorectified ? "already present (skipped)" : "performed (warped)"));

        // Display the preprocessed cube as a color False Colour Composite
        // (FCC) instead of a flat grayscale mid-band, in the side-dock
        // preview widget too.
        if (haveFccBands) {
            mw_->previewWidget()->showRgb(result.surfaceReflectance, rBand, gBand, bBand);
            mw_->log("Preprocessing", QString("Preview: FCC using bands closest to 660/570/840 nm "
                                               "(cube indices %1/%2/%3).").arg(rBand).arg(gBand).arg(bBand));
        } else {
            // Fallback if wavelength lookup failed for some reason.
            int midBand = result.surfaceReflectance.bands / 2;
            mw_->previewWidget()->showSingleBand(result.surfaceReflectance, midBand);
        }

        progressBar_->setValue(100);
        QMessageBox::information(this, "Preprocessing complete",
            QString("Surface reflectance cube ready: %1 bands, %2x%3 pixels.")
                .arg(result.surfaceReflectance.bands)
                .arg(result.surfaceReflectance.width)
                .arg(result.surfaceReflectance.height));
        accept();
    } catch (const std::exception& e) {
        progressBar_->setValue(0);
        statusValueLabel_->setText("Failed");
        mw_->log("Preprocessing", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Preprocessing failed", e.what());
    }
}
