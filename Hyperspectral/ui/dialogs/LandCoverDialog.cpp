#include "LandCoverDialog.h"
#include "../MainWindow.h"
#include "hsi/SpectralIndices.h"
#include "hsi/BuiltinSignatures.h"
#include "hsi/SamClassifier.h"
#include "hsi/RasterIO.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>

using namespace hsi;

static const RasterCube* bestCube(AppState& s) {
    if (s.stackFused) return &(*s.stackFused);
    if (s.surfaceReflectance) return &(*s.surfaceReflectance);
    return nullptr;
}

LandCoverDialog::LandCoverDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Land Cover Mapping — no training required");
    setMinimumWidth(540);

    // ── Tab 1: Spectral Indices ───────────────────────────────────────────
    auto* tab1 = new QWidget();

    auto* presetRow = new QHBoxLayout();
    presetCombo_ = new QComboBox();
    presetCombo_->addItem("Auto-detect (204-band stack, use Sentinel-2 bands 198-201)");
    presetCombo_->addItem("Hyperion reflectance cube (198 bands, Hyperion band centres)");
    presetCombo_->addItem("Manual — enter band indices below");
    presetRow->addWidget(presetCombo_);

    const int maxBand = 210;
    redIdxSpin_   = new QSpinBox(); redIdxSpin_->setRange(0, maxBand);   redIdxSpin_->setValue(200);
    nirIdxSpin_   = new QSpinBox(); nirIdxSpin_->setRange(0, maxBand);   nirIdxSpin_->setValue(201);
    greenIdxSpin_ = new QSpinBox(); greenIdxSpin_->setRange(0, maxBand); greenIdxSpin_->setValue(199);
    blueIdxSpin_  = new QSpinBox(); blueIdxSpin_->setRange(0, maxBand);  blueIdxSpin_->setValue(198);
    swirIdxSpin_  = new QSpinBox(); swirIdxSpin_->setRange(0, maxBand);  swirIdxSpin_->setValue(80);

    ndviThreshSpin_ = new QDoubleSpinBox(); ndviThreshSpin_->setRange(-1, 1); ndviThreshSpin_->setValue(0.3); ndviThreshSpin_->setSingleStep(0.05);
    ndwwThreshSpin_ = new QDoubleSpinBox(); ndwwThreshSpin_->setRange(-1, 1); ndwwThreshSpin_->setValue(0.0); ndwwThreshSpin_->setSingleStep(0.05);
    ndbiThreshSpin_ = new QDoubleSpinBox(); ndbiThreshSpin_->setRange(-1, 1); ndbiThreshSpin_->setValue(0.0); ndbiThreshSpin_->setSingleStep(0.05);

    auto* bandForm = new QFormLayout();
    bandForm->addRow("Band preset:", presetRow);
    bandForm->addRow(new QLabel("— or set manually —"));
    bandForm->addRow("Red band index:",   redIdxSpin_);
    bandForm->addRow("NIR band index:",   nirIdxSpin_);
    bandForm->addRow("Green band index:", greenIdxSpin_);
    bandForm->addRow("Blue band index:",  blueIdxSpin_);
    bandForm->addRow("SWIR band index:",  swirIdxSpin_);
    bandForm->addRow("NDVI threshold (> → vegetation):", ndviThreshSpin_);
    bandForm->addRow("NDWI threshold (> → water):",      ndwwThreshSpin_);
    bandForm->addRow("NDBI threshold (> → built-up):",   ndbiThreshSpin_);

    auto* infoLbl = new QLabel(
        "Output: 1=vegetation/forest  2=water/river  3=built-up  4=bare soil  0=unclassified\n"
        "Formula: NDVI=(NIR-Red)/(NIR+Red)  NDWI=(Green-NIR)/(Green+NIR)  NDBI=(SWIR-NIR)/(SWIR+NIR)",
        tab1);
    infoLbl->setWordWrap(true);
    infoLbl->setStyleSheet("color:#555;font-size:11px;");

    auto* runIndexBtn = new QPushButton("▶  Run Spectral Indices (instant, no training)");
    runIndexBtn->setStyleSheet("QPushButton{background:#27ae60;color:white;font-weight:bold;"
                                "border:none;border-radius:4px;padding:8px;}"
                                "QPushButton:hover{background:#2ecc71;}");

    auto* t1Layout = new QVBoxLayout(tab1);
    t1Layout->addLayout(bandForm);
    t1Layout->addWidget(infoLbl);
    t1Layout->addWidget(runIndexBtn);
    connect(runIndexBtn, &QPushButton::clicked, this, &LandCoverDialog::runIndices);

    // ── Tab 2: Built-in SAM ───────────────────────────────────────────────
    auto* tab2 = new QWidget();
    samAngleSpin_ = new QDoubleSpinBox(); samAngleSpin_->setRange(0.01, 1.5); samAngleSpin_->setValue(0.15);
    samAngleSpin_->setSuffix(" rad"); samAngleSpin_->setSingleStep(0.01);

    auto* samInfoLbl = new QLabel(
        "Uses 8 built-in land-cover spectral signatures derived from USGS/ASTER library values:\n"
        "  • dense_vegetation   (broadleaf forest, mangrove)\n"
        "  • sparse_vegetation  (dry grass, coastal scrub)\n"
        "  • turbid_water       (coastal harbour, sediment-laden)\n"
        "  • clear_water        (open ocean, clean river)\n"
        "  • urban_concrete     (roads, rooftops, paved)\n"
        "  • urban_metal        (ship decks, metal roofs)\n"
        "  • bare_soil_dry      (dry sand, beach)\n"
        "  • bare_soil_wet      (mudflat, wet sediment)\n\n"
        "No download, no training, no internet. Works on any Hyperion reflectance cube (198 bands).\n"
        "Works best on a 198-band Hyperion surface reflectance cube — full spectral richness.",
        tab2);
    samInfoLbl->setWordWrap(true);
    samInfoLbl->setStyleSheet("font-size:11px;");

    auto* samForm = new QFormLayout();
    samForm->addRow("SAM angle threshold:", samAngleSpin_);

    auto* runSamBtn = new QPushButton("▶  Run SAM with Built-in Signatures (no training)");
    runSamBtn->setStyleSheet("QPushButton{background:#2980b9;color:white;font-weight:bold;"
                              "border:none;border-radius:4px;padding:8px;}"
                              "QPushButton:hover{background:#3498db;}");

    auto* t2Layout = new QVBoxLayout(tab2);
    t2Layout->addWidget(samInfoLbl);
    t2Layout->addLayout(samForm);
    t2Layout->addWidget(runSamBtn);
    connect(runSamBtn, &QPushButton::clicked, this, &LandCoverDialog::runBuiltinSam);

    // ── Tab layout ────────────────────────────────────────────────────────
    tabs_ = new QTabWidget(this);
    tabs_->addTab(tab1, "Approach 1: Spectral indices (instant)");
    tabs_->addTab(tab2, "Approach 2: Built-in SAM (no training)");

    auto* saveBtn = new QPushButton("Save last result as GeoTIFF…");
    auto* layout  = new QVBoxLayout(this);
    layout->addWidget(tabs_);
    layout->addWidget(saveBtn);
    connect(saveBtn, &QPushButton::clicked, this, &LandCoverDialog::saveResult);
}

void LandCoverDialog::runIndices() {
    const RasterCube* cube = bestCube(mw_->appState());
    if (!cube) {
        QMessageBox::warning(this, "No data", "Run at least Step 2 (preprocessing) first.");
        return;
    }
    try {
        SpectralIndices::BandMap bands;
        int preset = presetCombo_->currentIndex();
        if (preset == 0) {
            bands = SpectralIndices::BandMap::fromStack204(swirIdxSpin_->value());
        } else if (preset == 1) {
            bands = SpectralIndices::BandMap::fromHyperionReflectance();
        } else {
            bands.redIdx   = redIdxSpin_->value();
            bands.nirIdx   = nirIdxSpin_->value();
            bands.greenIdx = greenIdxSpin_->value();
            bands.blueIdx  = blueIdxSpin_->value();
            bands.swirIdx  = swirIdxSpin_->value();
        }

        SpectralIndices::Thresholds thresh;
        thresh.ndviVeg   = static_cast<float>(ndviThreshSpin_->value());
        thresh.ndwwWater = static_cast<float>(ndwwThreshSpin_->value());
        thresh.ndbiBuilt = static_cast<float>(ndbiThreshSpin_->value());

        mw_->log("SpectralIndices", "Running NDVI/NDWI/NDBI/BSI…");
        auto result = SpectralIndices::compute(*cube, bands, thresh);

        mw_->appState().lulcUnsupervised = result.classified;
        mw_->log("SpectralIndices", "Done. Output: 1=veg 2=water 3=built-up 4=bare-soil 0=unclassified.");
        mw_->previewWidget()->showSingleBand(result.classified, 0);
        QMessageBox::information(this, "Spectral indices complete",
            "Land cover map generated:\n"
            "1 = Vegetation / forest\n"
            "2 = Water / river\n"
            "3 = Built-up\n"
            "4 = Bare soil / beach\n"
            "0 = Unclassified\n\n"
            "Use 'Save last result' to export as GeoTIFF.");
    } catch (const std::exception& e) {
        mw_->log("SpectralIndices", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Failed", e.what());
    }
}

void LandCoverDialog::runBuiltinSam() {
    if (!mw_->appState().surfaceReflectance) {
        QMessageBox::warning(this, "No data", "Run Step 2 (DN → Surface Reflectance) first.\n"
            "The built-in SAM works best on the 198-band Hyperion reflectance cube.");
        return;
    }
    try {
        SpectralLibrary lib = BuiltinSignatures::library();
        mw_->log("BuiltinSAM", QString("Running SAM against %1 built-in signatures…").arg(lib.signatures.size()));

        std::map<int, std::string> legend;
        RasterCube result = SamClassifier::classify(*mw_->appState().surfaceReflectance, lib,
                                                     samAngleSpin_->value(), &legend);
        mw_->appState().lulcUnsupervised = result;

        QString legendStr;
        for (const auto& kv : legend)
            legendStr += QString("%1 = %2\n").arg(kv.first).arg(QString::fromStdString(kv.second));

        mw_->log("BuiltinSAM", "Classification complete.");
        mw_->previewWidget()->showSingleBand(result, 0);
        QMessageBox::information(this, "Built-in SAM complete",
            "Class legend:\n" + legendStr + "\nUse 'Save last result' to export as GeoTIFF.");
    } catch (const std::exception& e) {
        mw_->log("BuiltinSAM", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Failed", e.what());
    }
}

void LandCoverDialog::saveResult() {
    if (!mw_->appState().lulcUnsupervised) {
        QMessageBox::warning(this, "Nothing to save", "Run one of the approaches first.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Save land cover GeoTIFF",
        QString(), "GeoTIFF (*.tif)");
    if (path.isEmpty()) return;
    try {
        RasterIO::saveCube(*mw_->appState().lulcUnsupervised, path.toStdString());
        mw_->log("LandCover", QString("Saved → %1").arg(path));
        QMessageBox::information(this, "Saved", path);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}
