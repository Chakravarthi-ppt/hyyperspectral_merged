#pragma once
// ---------------------------------------------------------------------------
// ImageComparisonWidget
//
// Second main tab ("Image Comparison"). Shows all 9 pipeline stages side by
// side as small thumbnail cards -- Load+Ortho, DN->Surface Reflectance,
// Surface/Object Mask, Land Cover Map, PCA+Stack, Built-up Classification,
// Raster->Vector, LULC Classification, Change Detection -- so the whole run
// can be visually sanity-checked ("does step 6 actually look like built-up,
// does step 9's change map look reasonable") without clicking through each
// step one at a time in the single large preview.
//
// This widget only *renders* thumbnails from whatever the pipeline has
// already produced (via RasterPreviewWidget's static render* helpers, so the
// colouring/stretch matches the full-size preview exactly). It does not run
// any processing itself. Clicking "View full size" asks MainWindow (via
// viewRequested) to switch back to the Pipeline tab and push that stage's
// data into the shared RasterPreviewWidget.
// ---------------------------------------------------------------------------
#include <QWidget>
#include <QImage>
#include <array>

class QLabel;
class QPushButton;
class QFrame;
class QGridLayout;
struct AppState;

class ImageComparisonWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageComparisonWidget(QWidget* parent = nullptr);

    static constexpr int N_STAGES = 9;

    // Rebuild every thumbnail + status badge from the current app state.
    // Cheap enough to call after every step completes: each thumbnail is
    // rendered at full stretch quality then scaled down for display.
    void refresh(const AppState& state);

signals:
    // idx is 0-based, same indexing as HsiMainWindow::steps_ (0 = Step 1, ... 8 = Step 9).
    void viewRequested(int idx);

private:
    struct Card {
        QFrame*      frame   = nullptr;
        QLabel*      number  = nullptr;
        QLabel*      title   = nullptr;
        QLabel*      status  = nullptr;
        QLabel*      thumb   = nullptr;
        QLabel*      caption = nullptr;
        QPushButton* viewBtn = nullptr;
    };

    Card buildCard(int stageNumber, const QString& title);
    // Renders the thumbnail image for one stage from whatever's cached in
    // AppState right now. Returns a null QImage (and sets outCaption to an
    // explanatory placeholder message) if that stage hasn't produced
    // anything cacheable yet.
    QImage thumbnailFor(int idx, const AppState& state, QString& outCaption);

    std::array<Card, N_STAGES> cards_;
};
