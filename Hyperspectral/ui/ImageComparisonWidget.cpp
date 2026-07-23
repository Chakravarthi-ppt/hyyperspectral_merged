#include "ImageComparisonWidget.h"
#include "MainWindow.h"          // AppState definition
#include "RasterPreviewWidget.h"

#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPixmap>
#include <algorithm>

namespace {

// Same badge colours used by the Pipeline tab's step list (see
// MainWindow.cpp's CLR_DONE / CLR_READY / CLR_WAITING), kept in sync by hand
// since they're just short constant style strings.
const char* kDoneStyle    = "color:#2ecc71; font-weight:bold;";
const char* kNotDoneStyle = "color:#7f8c8d; font-style:italic;";

const int kThumbW = 220;
const int kThumbH = 150;

QPixmap placeholderPixmap(const QString& message) {
    QPixmap pm(kThumbW, kThumbH);
    pm.fill(QColor(26, 37, 47));
    QPainter p(&pm);
    p.setPen(QColor(120, 130, 140));
    QFont f = p.font();
    f.setPointSize(9);
    p.setFont(f);
    QRect r(8, 8, kThumbW - 16, kThumbH - 16);
    p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, message);
    p.setPen(QColor(70, 80, 90));
    p.drawRect(0, 0, kThumbW - 1, kThumbH - 1);
    return pm;
}

// Small painted heatmap of a from->to change matrix: one square per cell,
// shaded by pixel count relative to the largest cell (the diagonal --
// "unchanged" -- is usually the largest, so off-diagonal cells show up as
// faint squares next to it, which is exactly what you want to eyeball).
QPixmap changeMatrixPixmap(const hsi::ChangeMatrixResult& cm) {
    QPixmap pm(kThumbW, kThumbH);
    pm.fill(QColor(26, 37, 47));
    if (cm.matrix.empty()) return pm;

    QPainter p(&pm);
    int n = static_cast<int>(cm.matrix.size());
    long maxVal = 1;
    for (const auto& row : cm.matrix)
        for (long v : row) maxVal = std::max(maxVal, v);

    int margin = 6;
    int gridSize = std::min(kThumbW, kThumbH) - 2 * margin;
    float cell = static_cast<float>(gridSize) / std::max(1, n);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n && c < static_cast<int>(cm.matrix[r].size()); ++c) {
            long v = cm.matrix[r][c];
            float t = static_cast<float>(v) / static_cast<float>(maxVal);
            QColor col = (r == c) ? QColor(60, 140, 220) : QColor(220, 90, 60);
            col.setAlphaF(std::clamp(0.12f + 0.88f * t, 0.0f, 1.0f));
            QRectF rect(margin + c * cell, margin + r * cell, cell - 1, cell - 1);
            p.fillRect(rect, col);
        }
    }
    p.setPen(QColor(70, 80, 90));
    p.drawRect(0, 0, kThumbW - 1, kThumbH - 1);
    return pm;
}

} // namespace

ImageComparisonWidget::Card ImageComparisonWidget::buildCard(int stageNumber, const QString& title) {
    Card c;
    c.frame = new QFrame(this);
    c.frame->setStyleSheet(
        "QFrame{background:#22303c; border:1px solid #34495e; border-radius:6px;}");
    auto* v = new QVBoxLayout(c.frame);
    v->setContentsMargins(10, 8, 10, 10);
    v->setSpacing(4);

    auto* header = new QHBoxLayout();
    c.number = new QLabel(QString("%1").arg(stageNumber), c.frame);
    c.number->setStyleSheet(
        "background:#3498db; color:white; border-radius:10px; font-weight:bold;"
        "min-width:20px; max-width:20px; min-height:20px; max-height:20px;");
    c.number->setAlignment(Qt::AlignCenter);
    c.title = new QLabel(title, c.frame);
    c.title->setStyleSheet("color:#ecf0f1; font-weight:bold;");
    c.title->setWordWrap(true);
    header->addWidget(c.number);
    header->addWidget(c.title, 1);
    v->addLayout(header);

    c.status = new QLabel("Waiting", c.frame);
    c.status->setStyleSheet(kNotDoneStyle);
    v->addWidget(c.status);

    c.thumb = new QLabel(c.frame);
    c.thumb->setFixedSize(kThumbW, kThumbH);
    c.thumb->setAlignment(Qt::AlignCenter);
    c.thumb->setPixmap(placeholderPixmap("Not run yet"));
    v->addWidget(c.thumb, 0, Qt::AlignHCenter);

    c.caption = new QLabel(c.frame);
    c.caption->setStyleSheet("color:#95a5a6; font-size:11px;");
    c.caption->setWordWrap(true);
    c.caption->setFixedWidth(kThumbW);
    v->addWidget(c.caption);

    c.viewBtn = new QPushButton("View full size", c.frame);
    c.viewBtn->setEnabled(false);
    connect(c.viewBtn, &QPushButton::clicked, this, [this, stageNumber]() {
        emit viewRequested(stageNumber - 1);
    });
    v->addWidget(c.viewBtn);

    return c;
}

ImageComparisonWidget::ImageComparisonWidget(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);

    auto* heading = new QLabel(
        "Image Comparison — one thumbnail per pipeline stage. "
        "Use this to sanity-check all 9 outputs at a glance before trusting the run; "
        "click \"View full size\" to inspect a stage in the Pipeline tab.", this);
    heading->setStyleSheet("color:#bdc3c7; padding:4px 2px;");
    heading->setWordWrap(true);
    outer->addWidget(heading);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none; background:#1a252f;}");
    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    static const char* kTitles[N_STAGES] = {
        "Load + Orthorectify",
        "DN → Surface Reflectance",
        "Surface / Object Mask",
        "Land Cover Map\n(Forest/Water/Built-up)",
        "PCA + Stack",
        "Built-up Classification\n(SAM + SVM)",
        "Raster → Vector",
        "LULC Classification",
        "Change Detection Matrix",
    };

    for (int i = 0; i < N_STAGES; ++i) {
        cards_[i] = buildCard(i + 1, kTitles[i]);
        grid->addWidget(cards_[i].frame, i / 3, i % 3);
    }

    auto* gridHost = new QWidget();
    gridHost->setLayout(grid);
    gridHost->setStyleSheet("background:#1a252f;");
    scroll->setWidget(gridHost);
    outer->addWidget(scroll, 1);
}

QImage ImageComparisonWidget::thumbnailFor(int idx, const AppState& s, QString& outCaption) {
    using RPW = RasterPreviewWidget;
    using CS  = RPW::CategoryStyle;

    switch (idx) {
    case 0: { // Load + Orthorectify — a path, not a cached raster; nothing to thumbnail
        if (s.hyperionInputPath.empty()) { outCaption = "No scene loaded yet."; return QImage(); }
        outCaption = QString("Loaded: %1")
            .arg(QString::fromStdString(s.hyperionInputPath).section('/', -1));
        outCaption += s.wasAlreadyOrthorectified ? "\nAlready orthorectified." : "\nWarped to map grid.";
        return QImage(); // info-only card, no pixel preview available
    }
    case 1: { // DN → Surface Reflectance
        if (!s.surfaceReflectance) { outCaption = "Run Step 2 first."; return QImage(); }
        const auto& cube = *s.surfaceReflectance;
        outCaption = QString("%1 bands, %2×%3 px").arg(cube.bands).arg(cube.width).arg(cube.height);
        if (cube.bands >= 23) return RPW::renderRgb(cube, 22, 20, 12); // same FCIR composite as Step 2's preview
        return RPW::renderSingleBand(cube, 0);
    }
    case 2: { // Surface / Object Mask
        if (!s.surfaceObjectMask) { outCaption = "Run Step 3 first."; return QImage(); }
        outCaption = "Surface (dark) vs Object (orange).";
        return RPW::renderCategorical(*s.surfaceObjectMask, 0,
            { {0, CS{QColor(30,30,30), "Surface"}}, {1, CS{QColor(255,140,0), "Object"}} });
    }
    case 3: { // Land Cover Map — always re-run, nothing persisted in AppState
        outCaption = "Not cached — reopen this step's dialog to view its result.";
        return QImage();
    }
    case 4: { // PCA + Stack
        if (!s.stackFused) { outCaption = "Run Step 5 first."; return QImage(); }
        const auto& cube = *s.stackFused;
        outCaption = QString("%1-band stack, %2×%3 px").arg(cube.bands).arg(cube.width).arg(cube.height);
        return RPW::renderSingleBand(cube, 0);
    }
    case 5: { // Built-up Classification
        if (!s.builtUpMask) { outCaption = "Run Step 6 first."; return QImage(); }
        outCaption = "Non built-up (dark) vs Built-up (red).";
        return RPW::renderCategorical(*s.builtUpMask, 0,
            { {0, CS{QColor(30,30,30), "Non built-up"}}, {1, CS{QColor(255,50,50), "Built-up"}} });
    }
    case 6: { // Raster → Vector — exported straight to disk, no cube kept in AppState
        const hsi::RasterCube* mask = nullptr;
        QString src;
        if (s.builtUpMask)          { mask = &(*s.builtUpMask);        src = "built-up mask"; }
        else if (s.surfaceObjectMask) { mask = &(*s.surfaceObjectMask); src = "surface/object mask"; }
        if (!mask) { outCaption = "Run Step 3 or Step 6 first, then export."; return QImage(); }
        outCaption = QString("Preview of what would be vectorised (%1).\nExport file itself isn't cached in-app.").arg(src);
        return RPW::renderCategorical(*mask, 0, { {1, CS{QColor(255,50,50), src}} });
    }
    case 7: { // LULC Classification
        if (s.lulcSupervisedA) {
            outCaption = "Supervised classification.";
            std::map<int, CS> pal;
            static const QColor kColors[] = {
                {220,50,50},{34,100,34},{100,200,80},{60,120,220},
                {210,160,80},{160,50,180},{50,190,190},{240,140,30}
            };
            if (s.lulcClassToLabel) {
                int ci = 0;
                for (const auto& kv : *s.lulcClassToLabel)
                    pal[kv.second] = CS{kColors[(ci++) % 8], QString::fromStdString(kv.first)};
            }
            pal[0] = CS{QColor(30,30,30), "Unclassified"};
            return RPW::renderCategorical(*s.lulcSupervisedA, 0, pal);
        }
        if (s.lulcUnsupervised) {
            outCaption = "Unsupervised clusters (run supervised for named classes).";
            std::map<int, CS> pal;
            static const QColor kColors[] = {
                {220,50,50},{34,100,34},{100,200,80},{60,120,220},
                {210,160,80},{160,50,180},{50,190,190},{240,140,30},
                {120,80,200},{80,160,80},{200,100,50},{50,120,180}
            };
            // Cluster ids are unknown here (only the classified cube persists),
            // so just cycle a palette wide enough for any reasonable k.
            for (int i = 1; i <= 12; ++i) pal[i] = CS{kColors[(i - 1) % 12], QString("Cluster %1").arg(i)};
            pal[0] = CS{QColor(30,30,30), "Unclassified"};
            return RPW::renderCategorical(*s.lulcUnsupervised, 0, pal);
        }
        outCaption = "Run Step 8 first.";
        return QImage();
    }
    case 8: { // Change Detection Matrix — numeric matrix, not a raster
        if (!s.changeMatrix) { outCaption = "Run Step 9 (needs LULC for both dates) first."; return QImage(); }
        int n = static_cast<int>(s.changeMatrix->classIds.size());
        outCaption = QString("%1×%2 change matrix. Diagonal=unchanged, blue;\noff-diagonal=changed, red.").arg(n).arg(n);
        return changeMatrixPixmap(*s.changeMatrix).toImage();
    }
    default:
        return QImage();
    }
}

void ImageComparisonWidget::refresh(const AppState& s) {
    bool haveInput   = !s.hyperionInputPath.empty();
    bool haveRefl    = s.surfaceReflectance.has_value();
    bool haveMask    = s.surfaceObjectMask.has_value();
    bool haveStack   = s.stackFused.has_value();
    bool haveBuiltUp = s.builtUpMask.has_value();
    bool haveLulcA   = s.lulcSupervisedA.has_value();
    bool haveLulcB   = s.lulcSupervisedB.has_value();

    // Mirrors HsiMainWindow::refreshStepStatus()'s "done" logic exactly, so the
    // two badges never disagree with each other.
    const bool done[N_STAGES] = {
        haveInput, haveRefl, haveMask, false, haveStack,
        haveBuiltUp, false, haveLulcA, haveLulcB && s.changeMatrix.has_value()
    };

    for (int i = 0; i < N_STAGES; ++i) {
        Card& c = cards_[i];
        c.status->setText(done[i] ? "✔  Done" : "—  Not yet available");
        c.status->setStyleSheet(done[i] ? kDoneStyle : kNotDoneStyle);

        QString caption;
        QImage img = thumbnailFor(i, s, caption);
        if (img.isNull()) {
            c.thumb->setPixmap(placeholderPixmap(caption.isEmpty() ? "Not run yet" : caption));
            c.viewBtn->setEnabled(false);
        } else {
            c.thumb->setPixmap(QPixmap::fromImage(
                img.scaled(kThumbW, kThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            c.viewBtn->setEnabled(true);
        }
        c.caption->setText(caption);
    }
}
