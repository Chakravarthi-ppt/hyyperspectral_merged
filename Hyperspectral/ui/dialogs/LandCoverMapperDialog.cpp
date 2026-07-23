#include "LandCoverMapperDialog.h"
#include "../MainWindow.h"
#include "UI/MainWindow/mainwindow.h"   // real WESEE window, for addRasterLayerToMap()

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QHeaderView>
#include <QComboBox>
#include <QFrame>
#include <QScrollArea>
#include <QColor>
#include <QDir>
#include <cmath>
#include <algorithm>

using namespace hsi;

// ─── colour legend label ─────────────────────────────────────────────────
static QLabel* legendLabel() {
    auto* lbl = new QLabel(
        "<table cellspacing='4'>"
        "<tr><td><span style='background:#e74c3c;padding:2px 8px;border-radius:3px;color:white;'>■</span></td>"
            "<td><b>1 Built-up</b> (urban, structures)</td></tr>"
        "<tr><td><span style='background:#27ae60;padding:2px 8px;border-radius:3px;color:white;'>■</span></td>"
            "<td><b>2 Forest</b> (dense tree cover)</td></tr>"
        "<tr><td><span style='background:#2ecc71;padding:2px 8px;border-radius:3px;color:white;'>■</span></td>"
            "<td><b>3 Vegetation</b> (grass, crops, low cover)</td></tr>"
        "<tr><td><span style='background:#2980b9;padding:2px 8px;border-radius:3px;color:white;'>■</span></td>"
            "<td><b>4 Water / River</b></td></tr>"
        "<tr><td><span style='background:#e67e22;padding:2px 8px;border-radius:3px;color:white;'>■</span></td>"
            "<td><b>5 Bare Soil / Sand</b></td></tr>"
        "<tr><td><span style='background:#7f8c8d;padding:2px 8px;border-radius:3px;color:white;'>■</span></td>"
            "<td><b>0 Unclassified</b></td></tr>"
        "</table>");
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet("background:#ecf0f1;padding:8px;border-radius:5px;");
    return lbl;
}

// ─── Tab 1: Spectral Indices ─────────────────────────────────────────────
void LandCoverMapperDialog::buildIndexTab(QWidget* tab) {
    auto mkSpin = [](double val, double lo, double hi) {
        auto* s = new QDoubleSpinBox();
        s->setRange(lo, hi); s->setSingleStep(0.05);
        s->setDecimals(2); s->setValue(val);
        return s;
    };
    thrForest_   = mkSpin(0.40, -1, 1);
    thrVeg_      = mkSpin(0.20, -1, 1);
    thrWater_    = mkSpin(0.10, -1, 1);
    thrBuiltUp_  = mkSpin(0.00, -1, 1);
    thrBareSoil_ = mkSpin(0.00, -1, 1);

    auto* infoLbl = new QLabel(
        "<b>Zero training required.</b> Uses pure spectral-index formulas on the "
        "reflectance cube. Band positions are auto-detected from Hyperion's "
        "sensor-band numbers.<br><br>"
        "<b>NDVI</b> (NIR−Red)/(NIR+Red) → Forest / Vegetation<br>"
        "<b>NDWI</b> (Green−NIR)/(Green+NIR) → Water / River<br>"
        "<b>NDBI</b> (SWIR−NIR)/(SWIR+NIR) → Built-up<br>"
        "<b>BSI</b>  ((SWIR+Red)−(NIR+Blue))/((SWIR+Red)+(NIR+Blue)) → Bare Soil");
    infoLbl->setWordWrap(true);
    infoLbl->setTextFormat(Qt::RichText);
    infoLbl->setStyleSheet("background:#dfe6e9;padding:8px;border-radius:4px;font-size:11px;");

    auto* form = new QFormLayout();
    form->addRow("NDVI threshold → Forest (above):",     thrForest_);
    form->addRow("NDVI threshold → Vegetation (above):", thrVeg_);
    form->addRow("NDWI threshold → Water (above):",      thrWater_);
    form->addRow("NDBI threshold → Built-up (above):",   thrBuiltUp_);
    form->addRow("BSI  threshold → Bare Soil (above):",  thrBareSoil_);

    auto* runBtn = new QPushButton("▶  Classify (Index-based)");
    runBtn->setStyleSheet("QPushButton{background:#27ae60;color:white;font-weight:bold;"
                           "border:none;border-radius:4px;padding:8px;}"
                           "QPushButton:hover{background:#2ecc71;}");

    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(infoLbl);
    layout->addSpacing(6);
    layout->addLayout(form);
    layout->addWidget(legendLabel());
    layout->addWidget(runBtn);
    layout->addStretch();

    connect(runBtn, &QPushButton::clicked, this, &LandCoverMapperDialog::runIndexBased);
}

// ─── Tab 2: Spectral Library (SAM) ───────────────────────────────────────
void LandCoverMapperDialog::buildLibraryTab(QWidget* tab) {
    libPathEdit_ = new QLineEdit();
    libPathEdit_->setPlaceholderText(
        "CSV spectral library (class_name, band_values…). "
        "Class names must contain: Forest / Water / River / Built / Vegetation / Soil");
    auto* browseBtn = new QPushButton("Browse…");
    auto* libRow = new QHBoxLayout();
    libRow->addWidget(libPathEdit_); libRow->addWidget(browseBtn);

    samAngleSpin_ = new QDoubleSpinBox();
    samAngleSpin_->setRange(0.01, 1.5); samAngleSpin_->setDecimals(3);
    samAngleSpin_->setSingleStep(0.01); samAngleSpin_->setValue(0.20);
    samAngleSpin_->setSuffix(" rad");

    auto* infoLbl = new QLabel(
        "<b>Zero training required.</b> SAM matches each pixel to the nearest "
        "material signature in the library by spectral angle.<br><br>"
        "You can use a pre-built USGS Spectral Library export, or build one "
        "with the Built-up Classification dialog's spectral library tool. "
        "Download <b>USGS Spectral Library Version 7</b> once and bundle it "
        "offline — no internet needed at runtime.");
    infoLbl->setWordWrap(true);
    infoLbl->setTextFormat(Qt::RichText);
    infoLbl->setStyleSheet("background:#dfe6e9;padding:8px;border-radius:4px;font-size:11px;");

    auto* form = new QFormLayout();
    form->addRow("Spectral library CSV:", libRow);
    form->addRow("SAM angle threshold:", samAngleSpin_);

    auto* runBtn = new QPushButton("▶  Classify (SAM Library)");
    runBtn->setStyleSheet("QPushButton{background:#2980b9;color:white;font-weight:bold;"
                           "border:none;border-radius:4px;padding:8px;}"
                           "QPushButton:hover{background:#3498db;}");

    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(infoLbl);
    layout->addSpacing(6);
    layout->addLayout(form);
    layout->addWidget(legendLabel());
    layout->addWidget(runBtn);
    layout->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &LandCoverMapperDialog::browseLibrary);
    connect(runBtn,    &QPushButton::clicked, this, &LandCoverMapperDialog::runLibraryBased);
}

// ─── Tab 3: Click-to-train SVM ───────────────────────────────────────────
void LandCoverMapperDialog::buildSvmTab(QWidget* tab) {
    auto* infoLbl = new QLabel(
        "<b>Light training — fully offline.</b> Mark 20–30 pixels per class "
        "(row, col coordinates from the raster). The SVM trains on-device in "
        "seconds — 198 Hyperion bands give it enormous discriminating power.<br><br>"
        "For each pixel you know the type of, note its row/col from the raster "
        "preview (hover to see coordinates), then add it to the table below.");
    infoLbl->setWordWrap(true);
    infoLbl->setTextFormat(Qt::RichText);
    infoLbl->setStyleSheet("background:#dfe6e9;padding:8px;border-radius:4px;font-size:11px;");

    sampleTable_ = new QTableWidget(0, 3, this);
    sampleTable_->setHorizontalHeaderLabels({"Class", "Row", "Col"});
    sampleTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    sampleTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    sampleTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    sampleTable_->setMinimumHeight(200);

    auto* addBtn = new QPushButton("+ Add row");
    addBtn->setStyleSheet("QPushButton{background:#7f8c8d;color:white;border:none;"
                           "border-radius:3px;padding:4px 10px;}"
                           "QPushButton:hover{background:#95a5a6;}");
    auto* removeBtn = new QPushButton("− Remove selected");
    removeBtn->setStyleSheet(addBtn->styleSheet());

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(addBtn); btnRow->addWidget(removeBtn); btnRow->addStretch();

    auto* hintLbl = new QLabel(
        "Classes: <b>1</b>=Built-up  <b>2</b>=Forest  <b>3</b>=Vegetation  "
        "<b>4</b>=Water/River  <b>5</b>=Bare Soil");
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setStyleSheet("font-size:11px;color:#555;");

    auto* runBtn = new QPushButton("▶  Train SVM + Classify");
    runBtn->setStyleSheet("QPushButton{background:#8e44ad;color:white;font-weight:bold;"
                           "border:none;border-radius:4px;padding:8px;}"
                           "QPushButton:hover{background:#9b59b6;}");

    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(infoLbl);
    layout->addSpacing(4);
    layout->addWidget(hintLbl);
    layout->addWidget(sampleTable_);
    layout->addLayout(btnRow);
    layout->addWidget(legendLabel());
    layout->addWidget(runBtn);
    layout->addStretch();

    connect(addBtn,    &QPushButton::clicked, this, &LandCoverMapperDialog::addSampleRow);
    connect(removeBtn, &QPushButton::clicked, this, &LandCoverMapperDialog::removeSampleRow);
    connect(runBtn,    &QPushButton::clicked, this, &LandCoverMapperDialog::runSvm);
}

// ─── Constructor ─────────────────────────────────────────────────────────
LandCoverMapperDialog::LandCoverMapperDialog(HsiMainWindow* mw, QWidget* parent)
    : QDialog(parent), mw_(mw) {
    setWindowTitle("Land Cover Mapper  —  Built-up / Forest / Water / Soil");
    setMinimumSize(620, 580);

    tabs_ = new QTabWidget(this);

    auto* t1 = new QWidget(); buildIndexTab(t1);   tabs_->addTab(t1, "1. Spectral Indices\n(no training)");
    auto* t2 = new QWidget(); buildLibraryTab(t2); tabs_->addTab(t2, "2. Spectral Library\n(SAM, no training)");
    auto* t3 = new QWidget(); buildSvmTab(t3);     tabs_->addTab(t3, "3. Click-to-train\n(SVM, 20–30 px/class)");

    auto* exportRasterBtn = new QPushButton("Save result raster (GeoTIFF)…");
    auto* exportVecBtn    = new QPushButton("Save result as vector (GeoJSON)…");
    exportRasterBtn->setStyleSheet("QPushButton{background:#2c3e50;color:white;border:none;"
                                    "border-radius:4px;padding:6px 12px;}"
                                    "QPushButton:hover{background:#34495e;}");
    exportVecBtn->setStyleSheet(exportRasterBtn->styleSheet());

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(exportRasterBtn);
    bottomRow->addWidget(exportVecBtn);
    bottomRow->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs_);
    layout->addLayout(bottomRow);

    connect(exportRasterBtn, &QPushButton::clicked, this, &LandCoverMapperDialog::exportResult);
    connect(exportVecBtn,    &QPushButton::clicked, this, &LandCoverMapperDialog::exportVector);
}

// ─── Slot helpers ─────────────────────────────────────────────────────────
static const RasterCube* requireCube(HsiMainWindow* mw) {
    if (mw->appState().stackFused) return &(*mw->appState().stackFused);
    if (mw->appState().surfaceReflectance) return &(*mw->appState().surfaceReflectance);
    return nullptr;
}

// Builds a 3-band 0-255 RGB composite from a single-band integer-label
// raster using the given palette. The real map canvas (LayerManager) always
// reads bands 1/2/3 of whatever file it's given straight as R/G/B -- it has
// no notion of categorical class palettes, so pushing the raw single-band
// label raster onto the map shows up as flat grayscale there. This is what
// actually makes the map display the legend's colors.
static RasterCube colorizeLabels(const RasterCube& labels,
                                  const std::map<int, RasterPreviewWidget::CategoryStyle>& palette) {
    RasterCube rgb;
    rgb.allocate(labels.width, labels.height, 3);
    rgb.geoTransform  = labels.geoTransform;
    rgb.projectionWkt = labels.projectionWkt;
    rgb.bandNames = { "Red", "Green", "Blue" };
    size_t n = labels.pixelCount();
    for (size_t i = 0; i < n; ++i) {
        int cls = static_cast<int>(labels.data[i]);
        auto it = palette.find(cls);
        QColor c = (it != palette.end()) ? it->second.color : QColor("#ffffff");
        rgb.data[i]         = static_cast<float>(c.red());
        rgb.data[n + i]     = static_cast<float>(c.green());
        rgb.data[2 * n + i] = static_cast<float>(c.blue());
    }
    // See ThematicDialog.cpp's colorizeLabels() for why this is needed:
    // the map canvas stretches each band independently by its own min/max,
    // which can crush a real class color to near-black when only a couple
    // of classes are actually present in the scene. Anchor one corner
    // pixel to true black and the next to true white so every band's
    // min/max is always 0/255 and the stretch becomes a no-op.
    if (rgb.width >= 2 && rgb.height >= 1) {
        rgb.data[0] = 0;     rgb.data[n] = 0;     rgb.data[2 * n] = 0;
        rgb.data[1] = 255;   rgb.data[n + 1] = 255; rgb.data[2 * n + 1] = 255;
    }
    return rgb;
}

void LandCoverMapperDialog::showResult(const RasterCube& result) {
    lastResult_ = result;
    hasResult_  = true;

    // Matches the hex colors already used in legendLabel()'s HTML table
    // above, so the on-image legend and the text legend agree.
    static const std::map<int, RasterPreviewWidget::CategoryStyle> kLandCoverPalette = {
        { static_cast<int>(LandClass::Unclassified), { QColor("#7f8c8d"), "Unclassified" } },
        { static_cast<int>(LandClass::BuiltUp),      { QColor("#e74c3c"), "Built-up" } },
        { static_cast<int>(LandClass::Forest),       { QColor("#27ae60"), "Forest" } },
        { static_cast<int>(LandClass::Vegetation),   { QColor("#2ecc71"), "Vegetation" } },
        { static_cast<int>(LandClass::Water),        { QColor("#2980b9"), "Water/River" } },
        { static_cast<int>(LandClass::BareSoil),     { QColor("#e67e22"), "Bare Soil" } },
        { static_cast<int>(LandClass::Other),        { QColor("#9b59b6"), "Other" } }, // not in legendLabel()'s table; rarely produced
    };
    mw_->previewWidget()->showCategorical(result, 0, kLandCoverPalette);

    // Push a colorized (real R/G/B) version onto the actual georeferenced
    // map canvas -- see colorizeLabels() above for why this is needed
    // instead of just handing the map the raw single-band label raster.
    if (mapWindow_) {
        try {
            RasterCube rgb = colorizeLabels(result, kLandCoverPalette);
            QString previewPath = QDir::tempPath() + "/wesee_landcover_preview.tif";
            RasterIO::saveCube(rgb, previewPath.toStdString());
            mapWindow_->addRasterLayerToMap(previewPath);
        } catch (const std::exception& e) {
            mw_->log("LandCoverMapper", QString("Note: couldn't push colored preview to map (%1).").arg(e.what()));
        }
    }

    // Count pixels per class
    long cnt[7] = {};
    for (float v : result.data) {
        int c = static_cast<int>(v);
        if (c >= 0 && c <= 6) ++cnt[c];
    }

    // Percentages against the raw pixel count include the black background
    // border outside the (diagonal) Hyperion swath -- that inflates
    // "Unclassified" and dilutes every real class's percentage. Detect
    // background the same way LulcClassifier does (all-near-zero across the
    // first few bands of the *source* cube) and report percentages against
    // the valid-pixel count instead.
    long backgroundCount = 0;
    if (const RasterCube* src = requireCube(mw_)) {
        if (src->sameGridAs(result)) {
            const int kBandsToCheck = std::min(src->bands, 10);
            const float kBackgroundThreshold = 1e-6f;
            for (int row = 0; row < src->height; ++row) {
                for (int col = 0; col < src->width; ++col) {
                    bool isBackground = true;
                    for (int b = 0; b < kBandsToCheck; ++b) {
                        if (std::abs(src->at(b, row, col)) > kBackgroundThreshold) { isBackground = false; break; }
                    }
                    if (isBackground) ++backgroundCount;
                }
            }
        }
    }
    long trueUnclassified = std::max(0L, cnt[0] - backgroundCount);
    long total = static_cast<long>(result.pixelCount()) - backgroundCount;
    auto pct   = [&](int c) { return total ? 100.0 * cnt[c] / total : 0.0; };

    QString summary =
        QString("Results (%1 × %2 pixels, %15 background pixels outside the imaged swath excluded):\n\n"
                "Built-up    : %3  (%4%)\n"
                "Forest      : %5  (%6%)\n"
                "Vegetation  : %7  (%8%)\n"
                "Water/River : %9  (%10%)\n"
                "Bare Soil   : %11 (%12%)\n"
                "Unclassified: %13 (%14%)\n\n"
                "Use 'Save result raster' to export the integer-label GeoTIFF,\n"
                "or 'Save result as vector' for polygon layer.")
            .arg(result.width).arg(result.height)
            .arg(cnt[1]).arg(pct(1), 0, 'f', 1)
            .arg(cnt[2]).arg(pct(2), 0, 'f', 1)
            .arg(cnt[3]).arg(pct(3), 0, 'f', 1)
            .arg(cnt[4]).arg(pct(4), 0, 'f', 1)
            .arg(cnt[5]).arg(pct(5), 0, 'f', 1)
            .arg(trueUnclassified).arg(total ? 100.0 * trueUnclassified / total : 0.0, 0, 'f', 1)
            .arg(backgroundCount);

    QMessageBox::information(this, "Land cover classification complete", summary);
    mw_->log("LandCoverMapper", QString("Done. Built-up=%1 Forest=%2 Water=%3 Veg=%4 Soil=%5 Unclassified=%6 (background excluded=%7)")
                 .arg(cnt[1]).arg(cnt[2]).arg(cnt[4]).arg(cnt[3]).arg(cnt[5]).arg(trueUnclassified).arg(backgroundCount));
}

// ─── Tab 1: run ──────────────────────────────────────────────────────────
void LandCoverMapperDialog::runIndexBased() {
    const RasterCube* cube = requireCube(mw_);
    if (!cube) {
        QMessageBox::warning(this, "No data", "Run Step 2 (DN → Surface Reflectance) first.");
        return;
    }
    try {
        LandCoverMapper::IndexThresholds thr;
        thr.ndviForest     = static_cast<float>(thrForest_->value());
        thr.ndviVegetation = static_cast<float>(thrVeg_->value());
        thr.ndwi           = static_cast<float>(thrWater_->value());
        thr.ndbi           = static_cast<float>(thrBuiltUp_->value());
        thr.bsi            = static_cast<float>(thrBareSoil_->value());

        mw_->log("LandCoverMapper", "Running index-based classification (NDVI/NDWI/NDBI/BSI)…");
        SpectralIndices::BandSet bs;   // autoDetect=true fills it inside classifyByIndices
        auto result = LandCoverMapper::classifyByIndices(*cube, bs, thr, true);
        showResult(result);
    } catch (const std::exception& e) {
        mw_->log("LandCoverMapper", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Index classification failed", e.what());
    }
}

// ─── Tab 2: browse + run ─────────────────────────────────────────────────
void LandCoverMapperDialog::browseLibrary() {
    QString p = QFileDialog::getOpenFileName(this, "Select spectral library CSV",
        QString(), "CSV files (*.csv);;All files (*)");
    if (!p.isEmpty()) libPathEdit_->setText(p);
}

void LandCoverMapperDialog::runLibraryBased() {
    const RasterCube* cube = requireCube(mw_);
    if (!cube) {
        QMessageBox::warning(this, "No data", "Run Step 2 (DN → Surface Reflectance) first.");
        return;
    }
    if (libPathEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "No library", "Select a spectral library CSV first.\n\n"
            "You can export one from the Built-up Classification dialog, or\n"
            "download the USGS Spectral Library v7 (free, one-time, works offline).");
        return;
    }
    try {
        mw_->log("LandCoverMapper", "Loading spectral library…");
        SpectralLibrary lib = SpectralLibrary::loadCsv(libPathEdit_->text().toStdString());
        mw_->log("LandCoverMapper", QString("Library loaded: %1 signatures. Running SAM…")
                     .arg(lib.signatures.size()));
        auto result = LandCoverMapper::classifyByLibrary(*cube, lib, samAngleSpin_->value());
        showResult(result);
    } catch (const std::exception& e) {
        mw_->log("LandCoverMapper", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Library classification failed", e.what());
    }
}

// ─── Tab 3: sample table helpers + run ───────────────────────────────────
void LandCoverMapperDialog::addSampleRow() {
    int row = sampleTable_->rowCount();
    sampleTable_->insertRow(row);

    auto* classCombo = new QComboBox();
    classCombo->addItem("1 — Built-up",    1);
    classCombo->addItem("2 — Forest",      2);
    classCombo->addItem("3 — Vegetation",  3);
    classCombo->addItem("4 — Water/River", 4);
    classCombo->addItem("5 — Bare Soil",   5);
    sampleTable_->setCellWidget(row, 0, classCombo);

    sampleTable_->setItem(row, 1, new QTableWidgetItem("0"));
    sampleTable_->setItem(row, 2, new QTableWidgetItem("0"));
}

void LandCoverMapperDialog::removeSampleRow() {
    auto selected = sampleTable_->selectedItems();
    if (selected.isEmpty()) return;
    int row = sampleTable_->row(selected.first());
    sampleTable_->removeRow(row);
}

void LandCoverMapperDialog::runSvm() {
    const RasterCube* cube = requireCube(mw_);
    if (!cube) {
        QMessageBox::warning(this, "No data", "Run Step 2 (DN → Surface Reflectance) first.");
        return;
    }
    if (sampleTable_->rowCount() == 0) {
        QMessageBox::warning(this, "No samples",
            "Add at least a few sample pixels per class using '+ Add row'.\n"
            "Aim for 20–30 pixels per class for good results.");
        return;
    }

    std::map<LandClass, std::vector<std::pair<int,int>>> classSamples;
    for (int r = 0; r < sampleTable_->rowCount(); ++r) {
        auto* combo = qobject_cast<QComboBox*>(sampleTable_->cellWidget(r, 0));
        if (!combo) continue;
        int classId = combo->currentData().toInt();
        int row = sampleTable_->item(r, 1) ? sampleTable_->item(r, 1)->text().toInt() : 0;
        int col = sampleTable_->item(r, 2) ? sampleTable_->item(r, 2)->text().toInt() : 0;
        classSamples[static_cast<LandClass>(classId)].push_back({row, col});
    }

    try {
        mw_->log("LandCoverMapper", QString("Training SVM on %1 sample rows…")
                     .arg(sampleTable_->rowCount()));
        auto result = LandCoverMapper::classifyBySvm(*cube, classSamples);
        showResult(result.classified);
    } catch (const std::exception& e) {
        mw_->log("LandCoverMapper", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "SVM classification failed", e.what());
    }
}

// ─── Export ──────────────────────────────────────────────────────────────
void LandCoverMapperDialog::exportResult() {
    if (!hasResult_) { QMessageBox::warning(this, "Nothing to save", "Run a classification first."); return; }
    QString path = QFileDialog::getSaveFileName(this, "Save land-cover raster",
        QString(), "GeoTIFF (*.tif)");
    if (path.isEmpty()) return;
    try {
        RasterIO::saveCube(lastResult_, path.toStdString());
        mw_->log("LandCoverMapper", QString("Raster saved → %1").arg(path));

        // Drop the classified GeoTIFF onto the real MapCanvas so it shows
        // up over India (or wherever its geotransform places it), not just
        // saved to disk / shown in this dialog's own small preview.
        if (mapWindow_)
            mapWindow_->addRasterLayerToMap(path);

        QMessageBox::information(this, "Saved", QString("Land-cover raster saved to:\n%1\n\n"
            "Integer labels: 0=Unclassified 1=Built-up 2=Forest 3=Vegetation 4=Water 5=Bare Soil").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}

void LandCoverMapperDialog::exportVector() {
    if (!hasResult_) { QMessageBox::warning(this, "Nothing to save", "Run a classification first."); return; }
    QString path = QFileDialog::getSaveFileName(this, "Save land-cover polygons",
        QString(), "GeoJSON (*.geojson);;Shapefile (*.shp);;GeoPackage (*.gpkg)");
    if (path.isEmpty()) return;
    QString driver = "GeoJSON";
    if (path.endsWith(".shp"))  driver = "ESRI Shapefile";
    if (path.endsWith(".gpkg")) driver = "GPKG";
    try {
        RasterToVector::polygonize(lastResult_, path.toStdString(), driver.toStdString(), "land_class");
        mw_->log("LandCoverMapper", QString("Vector saved → %1").arg(path));
        QMessageBox::information(this, "Saved", QString("Land-cover vector saved to:\n%1").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}
