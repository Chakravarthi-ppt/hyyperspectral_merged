#pragma once
#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QColor>
#include <map>
#include "hsi/Types.h"

// Raster viewer: takes a RasterCube and either a single band index
// (rendered as grayscale) or three band indices (rendered as RGB), applies
// a 2nd/98th percentile linear stretch, and displays it zoomable/pannable:
// Zoom In/Out/Fit/100% buttons, Ctrl+mouse-wheel to zoom, and the scroll
// area's own scrollbars (drag them, or click-drag isn't wired -- use the
// scrollbars) once the image is larger than the visible area.
//
// For classification results (a handful of small integer labels, e.g.
// 0=Unclassified..6=Other), use showCategorical() instead of
// showSingleBand(): percentile-stretching a few small integers barely
// shows any contrast, where flat per-class colors plus a legend are
// actually readable.
class RasterPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit RasterPreviewWidget(QWidget* parent = nullptr);

    struct CategoryStyle {
        QColor color;
        QString label;
    };

    void showSingleBand(const hsi::RasterCube& cube, int bandIndex);
    void showRgb(const hsi::RasterCube& cube, int rBand, int gBand, int bBand);
    // bandIndex's pixel values are read as integer class labels (cube
    // stores floats, so each is truncated via static_cast<int>). Any label
    // not present in `styles` is drawn in a neutral dark gray fallback, so
    // an unexpected/out-of-range value is visually obvious rather than
    // silently miscolored. A small color-swatch legend is shown beneath
    // the zoom toolbar for exactly the labels present in `styles`.
    void showCategorical(const hsi::RasterCube& cube, int bandIndex,
                          const std::map<int, CategoryStyle>& styles);
    void clear();

    // ── reusable render helpers ────────────────────────────────────────────
    // Same percentile-stretch / colouring logic as showSingleBand/showRgb/
    // showCategorical above, but returned as a standalone QImage instead of
    // being pushed into this widget's own view. Used by other widgets (e.g.
    // the Image Comparison tab) that need small thumbnails of a stage's
    // output without duplicating the stretch math.
    static QImage renderSingleBand(const hsi::RasterCube& cube, int bandIndex);
    static QImage renderRgb(const hsi::RasterCube& cube, int rBand, int gBand, int bBand);
    static QImage renderCategorical(const hsi::RasterCube& cube, int bandIndex,
                                     const std::map<int, CategoryStyle>& styles);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActual();

private:
    QLabel* imageLabel_;
    QScrollArea* scrollArea_;
    QLabel* zoomPercentLabel_;
    QWidget* legendBar_;
    QHBoxLayout* legendLayout_;
    QImage currentImage_;     // full-resolution source image, never itself scaled
    qreal zoomFactor_ = 1.0;  // 1.0 = 100% (one image pixel per screen pixel)

    static QVector<float> stretchBand(const hsi::RasterCube& cube, int band, float& lo, float& hi);
    void setImage(const QImage& img);
    void applyZoom(); // re-renders imageLabel_'s pixmap at the current zoomFactor_
    void setLegend(const std::map<int, CategoryStyle>& styles); // empty map hides the legend bar
};
