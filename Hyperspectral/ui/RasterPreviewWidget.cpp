#include "RasterPreviewWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWheelEvent>
#include <algorithm>
#include <vector>

namespace {
constexpr qreal kMinZoom = 0.05;
constexpr qreal kMaxZoom = 16.0;
constexpr qreal kZoomStep = 1.25;
}

RasterPreviewWidget::RasterPreviewWidget(QWidget* parent) : QWidget(parent) {
    imageLabel_ = new QLabel(this);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setText("No raster loaded yet.");
    imageLabel_->setMinimumSize(200, 200);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidget(imageLabel_);
    // false (not the default): we size imageLabel_ ourselves to the actual
    // zoomed pixmap size, so it can exceed the viewport and the scroll area
    // shows real scrollbars to pan it -- setWidgetResizable(true) would
    // instead force imageLabel_ to always match the viewport, defeating zoom.
    scrollArea_->setWidgetResizable(false);
    scrollArea_->setAlignment(Qt::AlignCenter);
    scrollArea_->viewport()->installEventFilter(this);

    auto* zoomOutBtn = new QPushButton("\u2212", this);
    auto* zoomInBtn  = new QPushButton("+", this);
    auto* fitBtn     = new QPushButton("Fit", this);
    auto* actualBtn  = new QPushButton("100%", this);
    for (auto* b : { zoomOutBtn, zoomInBtn, fitBtn, actualBtn }) b->setFixedWidth(44);
    zoomOutBtn->setToolTip("Zoom out (or Ctrl+scroll down)");
    zoomInBtn->setToolTip("Zoom in (or Ctrl+scroll up)");
    fitBtn->setToolTip("Fit image to window");
    actualBtn->setToolTip("Zoom to 100% (1 image pixel = 1 screen pixel)");

    zoomPercentLabel_ = new QLabel("100%", this);
    zoomPercentLabel_->setAlignment(Qt::AlignCenter);
    zoomPercentLabel_->setMinimumWidth(50);

    auto* toolbar = new QHBoxLayout();
    toolbar->addWidget(zoomOutBtn);
    toolbar->addWidget(zoomPercentLabel_);
    toolbar->addWidget(zoomInBtn);
    toolbar->addSpacing(8);
    toolbar->addWidget(fitBtn);
    toolbar->addWidget(actualBtn);
    toolbar->addStretch();

    legendBar_ = new QWidget(this);
    legendLayout_ = new QHBoxLayout(legendBar_);
    legendLayout_->setContentsMargins(6, 2, 6, 2);
    legendLayout_->setSpacing(14);
    legendBar_->setVisible(false); // shown only while a categorical result is displayed

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(legendBar_);
    layout->addWidget(scrollArea_);

    connect(zoomInBtn,  &QPushButton::clicked, this, &RasterPreviewWidget::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &RasterPreviewWidget::zoomOut);
    connect(fitBtn,     &QPushButton::clicked, this, &RasterPreviewWidget::zoomFit);
    connect(actualBtn,  &QPushButton::clicked, this, &RasterPreviewWidget::zoomActual);
}

bool RasterPreviewWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == scrollArea_->viewport() && event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            if (wheelEvent->angleDelta().y() > 0) zoomIn(); else zoomOut();
            return true; // consume -- don't also let the scroll area pan on the same gesture
        }
    }
    return QWidget::eventFilter(watched, event);
}

QVector<float> RasterPreviewWidget::stretchBand(const hsi::RasterCube& cube, int band, float& lo, float& hi) {
    size_t n = cube.pixelCount();
    size_t base = static_cast<size_t>(band) * n;
    std::vector<float> sorted(cube.data.begin() + base, cube.data.begin() + base + n);
    std::sort(sorted.begin(), sorted.end());
    size_t loIdx = static_cast<size_t>(0.02 * (n - 1));
    size_t hiIdx = static_cast<size_t>(0.98 * (n - 1));
    lo = sorted[loIdx];
    hi = sorted[hiIdx];
    if (hi <= lo) hi = lo + 1.0f;

    QVector<float> out(static_cast<int>(n));
    for (size_t i = 0; i < n; ++i) out[static_cast<int>(i)] = cube.data[base + i];
    return out;
}

QImage RasterPreviewWidget::renderSingleBand(const hsi::RasterCube& cube, int bandIndex) {
    if (bandIndex < 0 || bandIndex >= cube.bands) return QImage();
    float lo, hi;
    auto values = stretchBand(cube, bandIndex, lo, hi);

    QImage img(cube.width, cube.height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    for (int row = 0; row < cube.height; ++row) {
        uchar* line = img.scanLine(row);
        for (int col = 0; col < cube.width; ++col) {
            float raw = values[row * cube.width + col];
            if (std::abs(raw) < 1e-9f) continue; // background/NoData → transparent
            float t = std::clamp((raw - lo) / (hi - lo), 0.0f, 1.0f);
            uchar g = static_cast<uchar>(t * 255.0f);
            int px  = col * 4;
            line[px+0] = g; line[px+1] = g; line[px+2] = g; line[px+3] = 255;
        }
    }
    return img;
}

void RasterPreviewWidget::showSingleBand(const hsi::RasterCube& cube, int bandIndex) {
    QImage img = renderSingleBand(cube, bandIndex);
    if (img.isNull()) { clear(); return; }
    setImage(img);
}

QImage RasterPreviewWidget::renderRgb(const hsi::RasterCube& cube, int rBand, int gBand, int bBand) {
    if (rBand < 0 || gBand < 0 || bBand < 0 || rBand >= cube.bands || gBand >= cube.bands || bBand >= cube.bands) {
        return QImage();
    }
    float rLo, rHi, gLo, gHi, bLo, bHi;
    auto rVals = stretchBand(cube, rBand, rLo, rHi);
    auto gVals = stretchBand(cube, gBand, gLo, gHi);
    auto bVals = stretchBand(cube, bBand, bLo, bHi);

    QImage img(cube.width, cube.height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    for (int row = 0; row < cube.height; ++row) {
        uchar* line = img.scanLine(row);
        for (int col = 0; col < cube.width; ++col) {
            int idx = row * cube.width + col;
            if (std::abs(rVals[idx]) < 1e-9f && std::abs(gVals[idx]) < 1e-9f &&
                std::abs(bVals[idx]) < 1e-9f) continue; // background → transparent
            auto stretch = [](float v, float lo, float hi) {
                float t = (v - lo) / (hi - lo <= 0 ? 1.0f : hi - lo);
                return static_cast<uchar>(std::clamp(t, 0.0f, 1.0f) * 255.0f);
            };
            int px = col * 4;
            line[px+0] = stretch(bVals[idx], bLo, bHi); // B (ARGB little-endian)
            line[px+1] = stretch(gVals[idx], gLo, gHi); // G
            line[px+2] = stretch(rVals[idx], rLo, rHi); // R
            line[px+3] = 255; // fully opaque
        }
    }
    return img;
}

void RasterPreviewWidget::showRgb(const hsi::RasterCube& cube, int rBand, int gBand, int bBand) {
    QImage img = renderRgb(cube, rBand, gBand, bBand);
    if (img.isNull()) { clear(); return; }
    setImage(img);
}

QImage RasterPreviewWidget::renderCategorical(const hsi::RasterCube& cube, int bandIndex,
                                               const std::map<int, CategoryStyle>& styles) {
    if (bandIndex < 0 || bandIndex >= cube.bands) return QImage();

    size_t base = static_cast<size_t>(bandIndex) * cube.pixelCount();

    // Use RGBA so NoData / background pixels (label=0 when not in styles)
    // render as transparent rather than a fallback colour that looks like data.
    QImage img(cube.width, cube.height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);   // transparent black by default
    for (int row = 0; row < cube.height; ++row) {
        uchar* line = img.scanLine(row);  // ARGB: B G R A per pixel (little-endian)
        for (int col = 0; col < cube.width; ++col) {
            float rawVal = cube.data[base + static_cast<size_t>(row) * cube.width + col];
            // Skip NoData pixels: value 0.0 when hasNoData, or all-zero background
            if (cube.hasNoData && std::abs(rawVal - cube.noDataValue) < 0.5f) continue;
            if (std::abs(rawVal) < 1e-6f && !styles.count(0)) continue; // skip unlabelled background
            int label = static_cast<int>(rawVal);
            auto it = styles.find(label);
            if (it == styles.end()) continue;  // truly unlabelled — leave transparent
            const QColor& c = it->second.color;
            int px = col * 4;
            line[px + 0] = static_cast<uchar>(c.blue());
            line[px + 1] = static_cast<uchar>(c.green());
            line[px + 2] = static_cast<uchar>(c.red());
            line[px + 3] = 255;  // fully opaque
        }
    }
    return img;
}

void RasterPreviewWidget::showCategorical(const hsi::RasterCube& cube, int bandIndex,
                                            const std::map<int, CategoryStyle>& styles) {
    QImage img = renderCategorical(cube, bandIndex, styles);
    if (img.isNull()) { clear(); return; }
    setImage(img);
    setLegend(styles);
}

void RasterPreviewWidget::setLegend(const std::map<int, CategoryStyle>& styles) {
    QLayoutItem* item;
    while ((item = legendLayout_->takeAt(0)) != nullptr) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        } else if (QLayout* l = item->layout()) {
            // `l` (the per-entry QHBoxLayout built below) only owns the
            // layout object itself -- the swatch/text QLabels inside it were
            // constructed with `legendBar_` as their parent, not `l`, so
            // deleting just `l` left them alive and visible, stacked at
            // their old position underneath whatever legend got built next
            // (this is why the previous step's legend kept bleeding through
            // the current one). Delete each child widget explicitly first.
            QLayoutItem* child;
            while ((child = l->takeAt(0)) != nullptr) {
                if (QWidget* cw = child->widget()) cw->deleteLater();
                delete child;
            }
            l->deleteLater();
        }
        delete item;
    }

    for (const auto& entry : styles) {
        const CategoryStyle& style = entry.second;

        auto* swatch = new QLabel(legendBar_);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QString("background-color: %1; border: 1px solid #888;").arg(style.color.name()));

        auto* text = new QLabel(style.label, legendBar_);

        auto* pair = new QHBoxLayout();
        pair->setSpacing(4);
        pair->addWidget(swatch);
        pair->addWidget(text);
        legendLayout_->addLayout(pair);
    }
    legendLayout_->addStretch();
    legendBar_->setVisible(!styles.empty());
}

void RasterPreviewWidget::clear() {
    currentImage_ = QImage();
    imageLabel_->setPixmap(QPixmap());
    imageLabel_->setText("No raster loaded yet.");
    imageLabel_->resize(imageLabel_->minimumSize());
    setLegend({});
}

void RasterPreviewWidget::setImage(const QImage& img) {
    currentImage_ = img;
    setLegend({}); // continuous (non-categorical) content never shows a legend
    zoomFit(); // every newly loaded raster starts fit-to-window, as before
}

void RasterPreviewWidget::applyZoom() {
    if (currentImage_.isNull()) return;

    QSize target = currentImage_.size() * zoomFactor_;
    target.setWidth(std::max(1, target.width()));
    target.setHeight(std::max(1, target.height()));

    Qt::TransformationMode mode = (zoomFactor_ >= 1.0) ? Qt::FastTransformation : Qt::SmoothTransformation;
    QPixmap pix = QPixmap::fromImage(currentImage_).scaled(target, Qt::KeepAspectRatio, mode);

    imageLabel_->setPixmap(pix);
    imageLabel_->setText("");
    imageLabel_->resize(pix.size());
    zoomPercentLabel_->setText(QString::number(static_cast<int>(zoomFactor_ * 100.0 + 0.5)) + "%");
}

void RasterPreviewWidget::zoomIn() {
    zoomFactor_ = std::min(kMaxZoom, zoomFactor_ * kZoomStep);
    applyZoom();
}

void RasterPreviewWidget::zoomOut() {
    zoomFactor_ = std::max(kMinZoom, zoomFactor_ / kZoomStep);
    applyZoom();
}

void RasterPreviewWidget::zoomActual() {
    zoomFactor_ = 1.0;
    applyZoom();
}

void RasterPreviewWidget::zoomFit() {
    if (currentImage_.isNull()) { zoomFactor_ = 1.0; return; }
    QSize avail = scrollArea_->viewport()->size();
    if (avail.width() <= 0 || avail.height() <= 0) avail = QSize(400, 400);
    qreal sx = static_cast<qreal>(avail.width())  / std::max(1, currentImage_.width());
    qreal sy = static_cast<qreal>(avail.height()) / std::max(1, currentImage_.height());
    zoomFactor_ = std::clamp(std::min(sx, sy), kMinZoom, kMaxZoom);
    applyZoom();
}
