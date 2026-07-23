#include "ThematicDialog.h"
#include "../MainWindow.h"
#include "../RasterPreviewWidget.h"
#include "hsi/SpectralIndices.h"
#include "hsi/RasterIO.h"
#include "hsi/RasterToVector.h"
#include "DialogStyle.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QDir>
#include <algorithm>
#include <cmath>

#include "../../../UI/MainWindow/mainwindow.h"   // real WESEE window, for addRasterLayerToMap()

using namespace hsi;

namespace {
// Class labels written into the output raster's single band.
enum ThematicClass { kUnclassified = 0, kVegetationForest = 1, kWater = 2, kBuiltUp = 3 };

// Picks whichever cube is available, preferring the fused/stacked product
// (same preference order used by the Classification step).
const RasterCube* requireCube(HsiMainWindow* mw) {
    if (mw->appState().stackFused) return &(*mw->appState().stackFused);
    if (mw->appState().surfaceReflectance) return &(*mw->appState().surfaceReflectance);
    return nullptr;
}

// Builds one legend row as "[color swatch] [checkbox]" and hands back the
// checkbox so the caller can wire it up / read its state.
QCheckBox* legendRow(QVBoxLayout* into, const QString& colorHex, const QString& text, bool checkedByDefault) {
    auto* row = new QHBoxLayout();
    auto* swatch = new QLabel();
    swatch->setFixedSize(14, 14);
    swatch->setStyleSheet(QString("background:%1;border:1px solid #999;").arg(colorHex));
    auto* box = new QCheckBox(text);
    box->setChecked(checkedByDefault);
    row->addWidget(swatch);
    row->addWidget(box);
    row->addStretch();
    into->addLayout(row);
    return box;
}

// Builds a 3-band 0-255 RGB composite from a single-band integer-label
// raster, using the exact legend colors. The real map canvas (LayerManager)
// always reads bands 1/2/3 of whatever file it's given straight as R/G/B --
// it has no notion of categorical class palettes, so a single-band label
// raster shows up as flat grayscale there. Pushing this 3-band version
// instead is what actually makes the map show green/blue/orange/white.
RasterCube colorizeLabels(const RasterCube& labels, const RasterCube& source) {
    struct RGB { unsigned char r, g, b; };
    static const std::map<int, RGB> palette = {
        { kUnclassified,     {255, 255, 255} },
        { kVegetationForest, { 46, 204, 113} },
        { kWater,            { 41, 128, 185} },
        { kBuiltUp,          {230, 126,  34} },
    };
    // Distinct from every real color value (0-255) and from the 0/255
    // anchor pixels below, so it can never collide with either.
    const float kNoDataSentinel = -9999.0f;

    RasterCube rgb;
    rgb.allocate(labels.width, labels.height, 3);
    rgb.geoTransform  = labels.geoTransform;
    rgb.projectionWkt = labels.projectionWkt;
    rgb.bandNames = { "Red", "Green", "Blue" };
    rgb.hasNoData   = true;
    rgb.noDataValue = kNoDataSentinel;

    size_t n = labels.pixelCount();
    size_t srcN = source.pixelCount();
    for (size_t i = 0; i < n; ++i) {
        // True background/NoData in the source cube (outside the actual
        // swath, e.g. the padded bounding box around a diagonal Hyperion
        // strip) -- as opposed to a pixel that's genuinely inside the
        // scene but just didn't match any class range. The former should
        // render transparent like the rest of the app's black/NoData
        // canvas convention; the latter stays white ("Unclassified").
        bool isBackground = true;
        if (i < srcN) {
            for (int b = 0; b < std::min(source.bands, 10); ++b)
                if (std::abs(source.data[static_cast<size_t>(b) * srcN + i]) > 1e-6f) { isBackground = false; break; }
        } else {
            isBackground = false;
        }

        if (isBackground) {
            rgb.data[i] = rgb.data[n + i] = rgb.data[2 * n + i] = kNoDataSentinel;
            continue;
        }

        int cls = static_cast<int>(labels.data[i]);
        auto it = palette.find(cls);
        RGB c = (it != palette.end()) ? it->second : RGB{255, 255, 255};
        rgb.data[i]         = static_cast<float>(c.r);
        rgb.data[n + i]     = static_cast<float>(c.g);
        rgb.data[2 * n + i] = static_cast<float>(c.b);
    }
    // The real map canvas (LayerManager::addRasterLayer) stretches each
    // band independently using ITS OWN min/max, rather than treating 0-255
    // as an absolute display range. With only a handful of fixed class
    // colors present, that stretch can crush a real color down to near-
    // black (e.g. if only Water and white/Unclassified pixels exist, Water
    // -- (41,128,185) -- gets rescaled so its per-band minimum lands at 0,
    // turning it solid black on the map). Pin one corner pixel to true
    // black and the next to true white in every band, guaranteeing each
    // band's min/max is always 0/255 so the "stretch" becomes a no-op and
    // every other pixel's real color survives untouched. These two pixels
    // are ordinary values (0 / 255), not the NoData sentinel above, so
    // they're never treated as transparent.
    if (rgb.width >= 2 && rgb.height >= 1) {
        rgb.data[0] = 0;     rgb.data[n] = 0;     rgb.data[2 * n] = 0;         // (0,0) = black
        rgb.data[1] = 255;   rgb.data[n + 1] = 255; rgb.data[2 * n + 1] = 255; // (1,0) = white
    }
    return rgb;
}

// Percentile summary of one index's valid (non-background, non-degenerate)
// pixel values. Min/max alone are useless for normalized-difference indices
// -- they hit exactly -1.0/+1.0 whenever one of the two bands is ~0 (a
// near-zero-denominator edge pixel, not real data) -- so percentiles are
// used both for the diagnostics popup and for auto-picking thresholds.
struct IndexStats {
    bool valid = false;
    double p10 = 0, p25 = 0, p50 = 0, p75 = 0, p90 = 0;
    size_t n = 0;
};

IndexStats percentileStats(const RasterCube& cube, const RasterCube& idx) {
    IndexStats s;
    size_t pxN = cube.pixelCount();
    std::vector<double> vals;
    vals.reserve(pxN);
    for (size_t i = 0; i < pxN; ++i) {
        bool isBackground = true;
        for (int b = 0; b < std::min(cube.bands, 10); ++b)
            if (std::abs(cube.data[static_cast<size_t>(b) * pxN + i]) > 1e-6f) { isBackground = false; break; }
        if (isBackground) continue;
        double v = idx.data[i];
        if (std::abs(v) > 0.999) continue;  // degenerate near-zero-denominator edge pixel
        vals.push_back(v);
    }
    if (vals.empty()) return s;
    std::sort(vals.begin(), vals.end());
    auto pct = [&](double p) { return vals[static_cast<size_t>(p * (vals.size() - 1))]; };
    s.valid = true;
    s.p10 = pct(0.10); s.p25 = pct(0.25); s.p50 = pct(0.50);
    s.p75 = pct(0.75); s.p90 = pct(0.90);
    s.n = vals.size();
    return s;
}

QString formatStats(const IndexStats& s, const char* name) {
    if (!s.valid) return QString("%1: no valid pixels found.\n").arg(name);
    return QString("%1: p10=%2  p25=%3  median=%4  p75=%5  p90=%6  (n=%7)\n")
               .arg(name)
               .arg(s.p10, 0, 'f', 3).arg(s.p25, 0, 'f', 3)
               .arg(s.p50, 0, 'f', 3).arg(s.p75, 0, 'f', 3)
               .arg(s.p90, 0, 'f', 3).arg(s.n);
}
}

ThematicDialog::ThematicDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Thematic - Spectral Index Classification");
    setMinimumWidth(720);
    setStyleSheet(indigisDialogStyleSheet());

    auto mkSpin = [](double val) {
        auto* s = new QDoubleSpinBox();
        s->setRange(-1.0, 1.0);
        s->setSingleStep(0.05);
        s->setDecimals(2);
        s->setValue(val);
        return s;
    };

    // Defaults per project spec (Hyperion, Level-T1):
    //   NDVI  -> Vegetation : 0.30 - 0.80
    //   NDWI  -> Water      : 0.20 - 1.00
    //   NDBI  -> Built-Up   : 0.10 - 0.50
    // NDWI/NDBI ranges weren't independently confirmed the way NDVI was --
    // they're editable here so they can be tuned against real scenes.

    ndviMin_ = mkSpin(0.00);
    ndviMin_->setSpecialValueText(" ");
    ndviMax_ = mkSpin(0.00);
    ndviMax_->setSpecialValueText(" ");
    ndwiMin_ = mkSpin(0.00);
    ndwiMin_->setSpecialValueText(" ");
    ndwiMax_ = mkSpin(0.00);
    ndwiMax_->setSpecialValueText(" ");
    ndbiMin_ = mkSpin(0.00);
    ndbiMin_->setSpecialValueText(" ");
    ndbiMax_ = mkSpin(0.00);
    ndbiMax_->setSpecialValueText(" ");

    auto* form = new QFormLayout();
    auto* ndviRow = new QHBoxLayout(); ndviRow->addWidget(ndviMin_); ndviRow->addWidget(new QLabel("to")); ndviRow->addWidget(ndviMax_);
    auto* ndwiRow = new QHBoxLayout(); ndwiRow->addWidget(ndwiMin_); ndwiRow->addWidget(new QLabel("to")); ndwiRow->addWidget(ndwiMax_);
    auto* ndbiRow = new QHBoxLayout(); ndbiRow->addWidget(ndbiMin_); ndbiRow->addWidget(new QLabel("to")); ndbiRow->addWidget(ndbiMax_);
    form->addRow("NDVI Range \u2192 Vegetation", ndviRow);
    form->addRow("NDWI Range \u2192 Water",      ndwiRow);
    form->addRow("NDBI Range \u2192 Built-Up",   ndbiRow);

    auto* legendLayout = new QVBoxLayout();
    legendLayout->addWidget(new QLabel("<b>Show Classes</b>"));
    showVeg_      = legendRow(legendLayout, "#2ecc71", "Vegetation (NDVI)", true);
    showWater_    = legendRow(legendLayout, "#2980b9", "Water (NDWI)", true);
    showBuiltUp_  = legendRow(legendLayout, "#e67e22", "Built-Up (NDBI)", true);

    auto* runBtn = new QPushButton("Classify (Index-Based)");
    runBtn->setDefault(true);
    runBtn->setToolTip("Classifies this scene -- thresholds above are auto-adjusted to the "
                        "scene's real NDVI/NDWI/NDBI range each time before classifying.");

    auto* saveRasterBtn = new QPushButton("Save as Raster (GeoTIFF)");
    auto* saveVectorBtn = new QPushButton("Save as Vector (GeoJSON)");
    auto* saveRow = new QHBoxLayout();
    saveRow->addWidget(runBtn);
    saveRow->addWidget(saveRasterBtn);
    saveRow->addWidget(saveVectorBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addSpacing(12);
    layout->addLayout(form);
    layout->addLayout(legendLayout);
    layout->addSpacing(12);
    layout->addLayout(saveRow);

    connect(runBtn,        &QPushButton::clicked, this, &ThematicDialog::run);
    connect(saveRasterBtn, &QPushButton::clicked, this, &ThematicDialog::saveRaster);
    connect(saveVectorBtn, &QPushButton::clicked, this, &ThematicDialog::saveVector);
    connect(showVeg_,     &QCheckBox::toggled, this, &ThematicDialog::onVisibilityChanged);
    connect(showWater_,   &QCheckBox::toggled, this, &ThematicDialog::onVisibilityChanged);
    connect(showBuiltUp_, &QCheckBox::toggled, this, &ThematicDialog::onVisibilityChanged);
}

// Reads the scene's real NDVI/NDWI/NDBI distribution and updates the three
// threshold fields to match it before classifying, every time Classify is
// clicked. Every scene has a different real range (e.g. a snow-covered
// scene sits nowhere near the "textbook" 0.30-0.80 NDVI window the fields
// start out showing), so each class's lower bound is pinned to that
// index's own percentile within *this* scene: NDVI/NDBI use their 75th
// percentile (top quarter of the scene), NDWI uses its 90th (top tenth --
// NDWI tends to over-match, so a tighter cut avoids the whole scene
// collapsing into "Water"). Classification still checks Vegetation ->
// Water -> Built-up in that order, so this naturally partitions the scene
// into three visible groups instead of one class swallowing everything.
void ThematicDialog::run() {
    const RasterCube* cube = requireCube(mw_);
    if (!cube) {
        QMessageBox::warning(this, "No data available",
            "Run Preprocessing first to produce a cube to classify.");
        return;
    }

    try {
        SpectralIndices::BandSet bands = HyperionBandFinder::autoDetect(*cube);

        // Diagnostics: which actual band index (and, if known, sensor band
        // number) got picked for each target wavelength, and the real
        // min/max/mean of NDVI/NDWI/NDBI across the scene (background/
        // NoData pixels excluded). Shown directly in the completion popup
        // below -- not just logged -- since the log console isn't visible
        // in every layout this dialog runs in. If, say, NDVI's real max
        // across the scene is 0.05, no threshold starting at 0.10+ will
        // ever match it -- that's a data/band issue, not a "wrong
        // threshold" issue, and this makes that visible at a glance.
        auto bandInfo = [&](int idx) -> QString {
            if (idx < 0) return "NOT FOUND (-1)";
            int sensorBand = (idx < static_cast<int>(cube->bandNumbers.size())) ? cube->bandNumbers[idx] : -1;
            return QString("band #%1 (sensor band %2)").arg(idx).arg(sensorBand);
        };
        QString diagnostics = QString(
            "NIR: %1\nRed: %2\nGreen: %3\nSWIR1: %4\n")
            .arg(bandInfo(bands.nir)).arg(bandInfo(bands.red))
            .arg(bandInfo(bands.green)).arg(bandInfo(bands.swir1));

        RasterCube ndvi = SpectralIndices::ndvi(*cube, bands.nir, bands.red);
        RasterCube ndwi = SpectralIndices::ndwi(*cube, bands.green, bands.nir);
        RasterCube ndbi = SpectralIndices::ndbi(*cube, bands.swir1, bands.nir);

        IndexStats ndviStats = percentileStats(*cube, ndvi);
        IndexStats ndwiStats = percentileStats(*cube, ndwi);
        IndexStats ndbiStats = percentileStats(*cube, ndbi);
        diagnostics += formatStats(ndviStats, "NDVI");
        diagnostics += formatStats(ndwiStats, "NDWI");
        diagnostics += formatStats(ndbiStats, "NDBI");
        mw_->log("Thematic", "Diagnostics:\n" + diagnostics);

        // Auto-adjust the threshold fields to this scene's real range
        // before classifying (see the comment above run()). The fields
        // still show the generic starting defaults until you click
        // Classify; after that they reflect what this scene actually used.
        if (ndviStats.valid && ndwiStats.valid && ndbiStats.valid) {
            ndviMin_->setValue(ndviStats.p75); ndviMax_->setValue(1.00);
            ndwiMin_->setValue(ndwiStats.p90); ndwiMax_->setValue(1.00);
            ndbiMin_->setValue(ndbiStats.p75); ndbiMax_->setValue(1.00);
        }

        const double vMin = ndviMin_->value(), vMax = ndviMax_->value();
        const double wMin = ndwiMin_->value(), wMax = ndwiMax_->value();
        const double bMin = ndbiMin_->value(), bMax = ndbiMax_->value();

        RasterCube result;
        result.allocate(cube->width, cube->height, 1);
        result.geoTransform = cube->geoTransform;
        result.projectionWkt = cube->projectionWkt;
        result.hasNoData = cube->hasNoData;
        result.noDataValue = cube->noDataValue;
        result.bandNames[0] = "ThematicClass";

        size_t pixelN = cube->pixelCount();
        for (size_t i = 0; i < pixelN; ++i) {
            double v = ndvi.data[i];
            double w = ndwi.data[i];
            double b = ndbi.data[i];

            int cls;
            if (v >= vMin && v <= vMax)      cls = kVegetationForest;
            else if (w >= wMin && w <= wMax) cls = kWater;
            else if (b >= bMin && b <= bMax) cls = kBuiltUp;
            else                              cls = kUnclassified;

            result.data[i] = static_cast<float>(cls);
        }

        rawResult_ = result;
        hasRawResult_ = true;

        applyVisibilityAndShow();
    } catch (const std::exception& e) {
        mw_->log("Thematic", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Classification failed", e.what());
    }
}

// Re-derives lastResult_ from rawResult_ by folding any unchecked class's
// pixels into Unclassified, then refreshes the preview. Cheap -- just a
// pixel copy/relabel, no re-computation of NDVI/NDWI/NDBI -- so it's safe
// to call on every checkbox toggle.
void ThematicDialog::applyVisibilityAndShow() {
    if (!hasRawResult_) return;

    const bool wantVeg      = showVeg_->isChecked();
    const bool wantWater    = showWater_->isChecked();
    const bool wantBuiltUp  = showBuiltUp_->isChecked();

    RasterCube filtered = rawResult_;
    long cntVeg = 0, cntWater = 0, cntBuiltUp = 0, cntUnclassified = 0;
    for (float& v : filtered.data) {
        int cls = static_cast<int>(v);
        bool keep = (cls == kVegetationForest && wantVeg)
                 || (cls == kWater            && wantWater)
                 || (cls == kBuiltUp          && wantBuiltUp);
        if (!keep) { v = static_cast<float>(kUnclassified); cls = kUnclassified; }

        switch (cls) {
            case kVegetationForest: ++cntVeg; break;
            case kWater:            ++cntWater; break;
            case kBuiltUp:          ++cntBuiltUp; break;
            default:                ++cntUnclassified; break;
        }
    }

    lastResult_ = filtered;
    hasResult_  = true;

    std::map<int, RasterPreviewWidget::CategoryStyle> styles = {
        { kUnclassified,     { QColor("#ffffff"), "Unclassified" } },
        { kVegetationForest, { QColor("#2ecc71"), "Vegetation / Forest" } },
        { kWater,            { QColor("#2980b9"), "Water" } },
        { kBuiltUp,          { QColor("#e67e22"), "Built-up" } },
    };
    mw_->previewWidget()->showCategorical(filtered, 0, styles);

    // Push a colorized (real R/G/B) version onto the actual georeferenced
    // map canvas -- see colorizeLabels() for why this is needed instead of
    // just handing the map the raw single-band label raster.
    if (mapWindow_) {
        try {
            if (const RasterCube* src = requireCube(mw_)) {
                RasterCube rgb = colorizeLabels(filtered, *src);
                QString previewPath = QDir::tempPath() + "/wesee_thematic_preview.tif";
                RasterIO::saveCube(rgb, previewPath.toStdString());
                mapWindow_->addRasterLayerToMap(previewPath);
            }
        } catch (const std::exception& e) {
            mw_->log("Thematic", QString("Note: couldn't push colored preview to map (%1).").arg(e.what()));
        }
    }

    double total = static_cast<double>(filtered.pixelCount());
    mw_->log("Thematic", QString("Shown: Vegetation/Forest=%1(%2%) Water=%3(%4%) Built-up=%5(%6%) Unclassified=%7(%8%)")
                 .arg(cntVeg).arg(total ? 100.0 * cntVeg / total : 0.0, 0, 'f', 1)
                 .arg(cntWater).arg(total ? 100.0 * cntWater / total : 0.0, 0, 'f', 1)
                 .arg(cntBuiltUp).arg(total ? 100.0 * cntBuiltUp / total : 0.0, 0, 'f', 1)
                 .arg(cntUnclassified).arg(total ? 100.0 * cntUnclassified / total : 0.0, 0, 'f', 1));
}

void ThematicDialog::onVisibilityChanged() {
    applyVisibilityAndShow();
}

void ThematicDialog::saveRaster() {
    if (!hasResult_) { QMessageBox::warning(this, "Nothing to save", "Run a classification first."); return; }
    QString path = QFileDialog::getSaveFileName(this, "Save thematic raster",
        QString(), "GeoTIFF (*.tif)");
    if (path.isEmpty()) return;
    try {
        RasterIO::saveCube(lastResult_, path.toStdString());
        mw_->log("Thematic", QString("Raster saved → %1").arg(path));
        if (mapWindow_)
            mapWindow_->addRasterLayerToMap(path);
        QMessageBox::information(this, "Saved", QString("Thematic raster saved to:\n%1\n\n"
            "Integer labels: 0=Unclassified 1=Vegetation/Forest 2=Water 3=Built-up").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}

void ThematicDialog::saveVector() {
    if (!hasResult_) { QMessageBox::warning(this, "Nothing to save", "Run a classification first."); return; }
    QString path = QFileDialog::getSaveFileName(this, "Save thematic polygons",
        QString(), "GeoJSON (*.geojson);;Shapefile (*.shp);;GeoPackage (*.gpkg)");
    if (path.isEmpty()) return;
    QString driver = "GeoJSON";
    if (path.endsWith(".shp"))  driver = "ESRI Shapefile";
    if (path.endsWith(".gpkg")) driver = "GPKG";
    try {
        RasterToVector::polygonize(lastResult_, path.toStdString(), driver.toStdString(), "thematic_class");
        mw_->log("Thematic", QString("Vector saved → %1").arg(path));
        QMessageBox::information(this, "Saved", QString("Thematic vector saved to:\n%1").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save failed", e.what());
    }
}
