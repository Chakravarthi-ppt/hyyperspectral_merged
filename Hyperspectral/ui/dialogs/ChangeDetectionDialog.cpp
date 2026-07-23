#include "ChangeDetectionDialog.h"
#include "../MainWindow.h"
#include "DialogStyle.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QHeaderView>
#include <QColor>
#include <QPainter>
#include <algorithm>

using namespace hsi;

ChangeTimelineWidget::ChangeTimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(220);
}

void ChangeTimelineWidget::setData(const QStringList& classNames,
                                    const QVector<double>& areaA,
                                    const QVector<double>& areaB,
                                    const QVector<QColor>& colors,
                                    const QString& areaUnit) {
    classNames_ = classNames;
    areaA_ = areaA;
    areaB_ = areaB;
    colors_ = colors;
    areaUnit_ = areaUnit;
    hasData_ = !classNames_.isEmpty();
    update();
}

void ChangeTimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);

    if (!hasData_) {
        p.setPen(QColor(150, 150, 150));
        p.drawText(rect(), Qt::AlignCenter,
                   "Timeline will appear here after \u201cGenerate Change Matrix\u201d.");
        return;
    }

    const int marginL = 70, marginR = 140, marginT = 24, marginB = 36;
    const int plotW = width() - marginL - marginR;
    const int plotH = height() - marginT - marginB;
    const int xA = marginL;
    const int xB = marginL + plotW;

    double maxVal = 1.0;
    for (double v : areaA_) maxVal = std::max(maxVal, v);
    for (double v : areaB_) maxVal = std::max(maxVal, v);

    // Axes
    p.setPen(QColor(120, 120, 120));
    p.drawLine(marginL, marginT, marginL, marginT + plotH);
    p.drawLine(marginL, marginT + plotH, marginL + plotW, marginT + plotH);

    // Y-axis gridlines/labels (0%, 50%, 100% of max)
    p.setPen(QColor(220, 220, 220));
    for (int i = 0; i <= 4; ++i) {
        int y = marginT + plotH - i * plotH / 4;
        p.drawLine(marginL, y, marginL + plotW, y);
    }
    p.setPen(QColor(100, 100, 100));
    QFont small = p.font(); small.setPointSize(8); p.setFont(small);
    for (int i = 0; i <= 4; ++i) {
        int y = marginT + plotH - i * plotH / 4;
        double val = maxVal * i / 4.0;
        p.drawText(QRect(0, y - 8, marginL - 8, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(val, 'f', 0));
    }

    // X-axis labels
    QFont bold = p.font(); bold.setBold(true); p.setFont(bold);
    p.drawText(QRect(xA - 40, marginT + plotH + 8, 80, 20), Qt::AlignCenter, "Date A");
    p.drawText(QRect(xB - 40, marginT + plotH + 8, 80, 20), Qt::AlignCenter, "Date B");
    p.setFont(small);

    // One trend line per class, plus a small legend on the right.
    int legendY = marginT;
    for (int i = 0; i < classNames_.size(); ++i) {
        QColor c = (i < colors_.size()) ? colors_[i] : QColor(100, 100, 100);
        double a = (i < areaA_.size()) ? areaA_[i] : 0.0;
        double b = (i < areaB_.size()) ? areaB_[i] : 0.0;
        int yA = marginT + plotH - static_cast<int>(plotH * (a / maxVal));
        int yB = marginT + plotH - static_cast<int>(plotH * (b / maxVal));

        QPen pen(c, 2.2);
        p.setPen(pen);
        p.drawLine(xA, yA, xB, yB);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(xA, yA), 4, 4);
        p.drawEllipse(QPoint(xB, yB), 4, 4);

        // Legend entry: swatch + name + up/down arrow showing the trend.
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRect(marginL + plotW + 12, legendY + 3, 10, 10);
        p.setPen(QColor(60, 60, 60));
        QString arrow = (b > a) ? "\u2191" : (b < a ? "\u2193" : "\u2192");
        QString name = classNames_[i];
        if (name.size() > 14) name = name.left(13) + "\u2026";
        p.drawText(marginL + plotW + 28, legendY + 12, QString("%1 %2").arg(name, arrow));
        legendY += 18;
    }

    p.setPen(QColor(120, 120, 120));
    p.drawText(QRect(marginL + plotW + 12, marginT + plotH - 14, marginR - 12, 14),
               Qt::AlignLeft, areaUnit_);
}

ChangeDetectionDialog::ChangeDetectionDialog(HsiMainWindow* mainWindow, QWidget* parent)
    : QDialog(parent), mw_(mainWindow) {
    setWindowTitle("Land-Use Change Matrix");
    setMinimumSize(900, 780);
    setStyleSheet(indigisDialogStyleSheet());

    // Kept as a plain (non-shown) widget so run()'s area math is unchanged --
    // just no longer surfaced in the dialog. Default 900 sq.m/pixel (30x30 m,
    // Hyperion's native resolution) stays in effect.
    pixelAreaSpin_ = new QDoubleSpinBox(this);
    pixelAreaSpin_->setRange(1.0, 1000000.0);
    pixelAreaSpin_->setValue(900.0);
    pixelAreaSpin_->setSuffix(" sq.m per pixel");
    pixelAreaSpin_->setVisible(false);

    table_ = new QTableWidget(this);

    timelineChart_ = new ChangeTimelineWidget(this);
    timelineLabel_ = new QLabel("<b>Timeline (area per class, Date A \u2192 Date B):</b>");
    timelineChart_->setVisible(false);
    timelineLabel_->setVisible(false);

    summaryLabel_ = new QLabel(this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setStyleSheet("font-weight:bold;padding:4px;");

    // "Load First Date" / "Load Second Date": browse for a classified LULC
    // GeoTIFF directly (e.g. from a previous session's Save result raster),
    // loading it straight into appState().lulcSupervisedA/B -- so this
    // dialog can be used standalone without needing Step 8 (LULC
    // Classification) to have been run earlier in the same session.
    dateAEdit_ = new QLineEdit(this);
    dateAEdit_->setPlaceholderText("No file selected");
    dateAEdit_->setReadOnly(true);
    auto* browseA = new QPushButton("Browse", this);

    dateBEdit_ = new QLineEdit(this);
    dateBEdit_->setPlaceholderText("No file selected");
    dateBEdit_->setReadOnly(true);
    auto* browseB = new QPushButton("Browse", this);

    auto* dateAForm = new QHBoxLayout();
    dateAForm->addWidget(dateAEdit_);
    dateAForm->addWidget(browseA);
    auto* dateBForm = new QHBoxLayout();
    dateBForm->addWidget(dateBEdit_);
    dateBForm->addWidget(browseB);

    auto* datesForm = new QFormLayout();
    datesForm->addRow("Load First Date", dateAForm);
    datesForm->addRow("Load Second Date", dateBForm);

    auto* generateDetectionBtn = new QPushButton("Generate Change Detection");
    generateDetectionBtn->setToolTip("Shows WHERE pixels changed class (white) vs stayed the same (black), "
                                      "pixel-for-pixel -- the visual counterpart to the numeric matrix below.");
    auto* generateMatrixBtn = new QPushButton("Generate Change Matrix");
    timelineBtn_ = new QPushButton("Generate Time Series");
    timelineBtn_->setToolTip("Shows/hides the area-per-class trend from Date A to Date B. "
                              "Run Generate Change Matrix first.");
    timelineBtn_->setCheckable(true);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(generateDetectionBtn);
    bottomRow->addWidget(generateMatrixBtn);
    bottomRow->addWidget(timelineBtn_);

    auto* layout = new QVBoxLayout(this);
    layout->addSpacing(12);
    layout->addWidget(summaryLabel_);
    layout->addWidget(table_);
    layout->addWidget(timelineLabel_);
    layout->addWidget(timelineChart_);
    layout->addSpacing(12);
    layout->addLayout(datesForm);
    layout->addSpacing(12);
    layout->addLayout(bottomRow);

    connect(generateMatrixBtn, &QPushButton::clicked, this, &ChangeDetectionDialog::run);
    connect(generateDetectionBtn, &QPushButton::clicked, this, &ChangeDetectionDialog::generateChangeDetection);
    connect(timelineBtn_, &QPushButton::clicked, this, &ChangeDetectionDialog::toggleTimeline);
    connect(browseA, &QPushButton::clicked, this, &ChangeDetectionDialog::browseDateA);
    connect(browseB, &QPushButton::clicked, this, &ChangeDetectionDialog::browseDateB);
}

void ChangeDetectionDialog::browseDateA() {
    QString path = QFileDialog::getOpenFileName(this, "Load classified raster \u2014 Date A",
                                                  QString(), "GeoTIFF (*.tif *.tiff)");
    if (path.isEmpty()) return;
    try {
        mw_->appState().lulcSupervisedA = hsi::RasterIO::loadCube(path.toStdString());
        dateAEdit_->setText(path);
        mw_->log("ChangeDetector", QString("Date A loaded \u2190 %1").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load failed", e.what());
    }
}

void ChangeDetectionDialog::browseDateB() {
    QString path = QFileDialog::getOpenFileName(this, "Load classified raster \u2014 Date B",
                                                  QString(), "GeoTIFF (*.tif *.tiff)");
    if (path.isEmpty()) return;
    try {
        mw_->appState().lulcSupervisedB = hsi::RasterIO::loadCube(path.toStdString());
        dateBEdit_->setText(path);
        mw_->log("ChangeDetector", QString("Date B loaded \u2190 %1").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load failed", e.what());
    }
}

void ChangeDetectionDialog::generateChangeDetection() {
    if (!mw_->appState().lulcSupervisedA || !mw_->appState().lulcSupervisedB) {
        QMessageBox::warning(this, "Missing classified rasters",
            "Run Hyperspectral \u2192 LULC Classification for both date A and date B first.");
        return;
    }
    try {
        hsi::RasterCube lulcA = *mw_->appState().lulcSupervisedA;
        hsi::RasterCube lulcB = *mw_->appState().lulcSupervisedB;
        if (!lulcA.sameGridAs(lulcB))
            lulcB = hsi::RasterIO::resampleToGrid(lulcB, lulcA);
        RasterCube changeMap = ChangeDetector::computeChangeMap(lulcA, lulcB);
        using CS = RasterPreviewWidget::CategoryStyle;
        mw_->previewWidget()->showCategorical(changeMap, 0, {
            {0, CS{QColor(30,30,30), "Unchanged"}},
            {1, CS{QColor(255,80,0), "Changed"  }}
        });
        mw_->log("ChangeDetector", "Displaying change map (white = changed pixel, black = unchanged).");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Change map failed", e.what());
    }
}

void ChangeDetectionDialog::run() {
    if (!mw_->appState().lulcSupervisedA || !mw_->appState().lulcSupervisedB) {
        QMessageBox::warning(this, "Missing classified rasters",
            "Run Hyperspectral \u2192 LULC Classification for both date A (supervised) and date B "
            "(classify second date) first.");
        return;
    }
    try {
        // Safety net: resample dateB to dateA grid if they differ.
        // This should already be done in classifyDateB(), but handles the case
        // where dateA and dateB were classified in different sessions or from
        // different source scenes without the resampling step.
        hsi::RasterCube lulcA = *mw_->appState().lulcSupervisedA;
        hsi::RasterCube lulcB = *mw_->appState().lulcSupervisedB;
        if (!lulcA.sameGridAs(lulcB)) {
            mw_->log("ChangeDetector",
                QString("Grid mismatch detected: A=%1x%2, B=%3x%4. Resampling B to A grid.")
                    .arg(lulcA.width).arg(lulcA.height)
                    .arg(lulcB.width).arg(lulcB.height));
            lulcB = hsi::RasterIO::resampleToGrid(lulcB, lulcA);
        }
        lastResult_ = ChangeDetector::computeChangeMatrix(lulcA, lulcB, pixelAreaSpin_->value());
        hasResult_ = true;
        mw_->appState().changeMatrix = lastResult_;

        int n = static_cast<int>(lastResult_.classIds.size());
        table_->setRowCount(n);
        table_->setColumnCount(n + 1);

        // Build id→name map from stored legend; fall back to "class N"
        std::map<int,QString> idToName;
        idToName[0] = "Unclassified";
        if (mw_->appState().lulcClassToLabel.has_value()) {
            for (const auto& kv : *mw_->appState().lulcClassToLabel)
                idToName[kv.second] = QString::fromStdString(kv.first);
        }
        auto className = [&](int id) -> QString {
            auto it = idToName.find(id);
            return (it != idToName.end()) ? it->second : QString("class %1").arg(id);
        };

        QStringList headers;
        headers << "From \\ To";
        for (int id : lastResult_.classIds) headers << className(id);
        table_->setHorizontalHeaderLabels(headers);

        long totalPixels = 0, changedPixels = 0;
        for (int i = 0; i < n; ++i) {
            table_->setItem(i, 0, new QTableWidgetItem(className(lastResult_.classIds[i])));
            for (int j = 0; j < n; ++j) {
                long count = lastResult_.matrix[i][j];
                totalPixels += count;
                auto* item = new QTableWidgetItem(QString::number(count));
                if (i == j) {
                    item->setBackground(QColor(200, 240, 200)); // unchanged -- diagonal
                } else {
                    changedPixels += count;
                    if (count > 0) item->setBackground(QColor(250, 220, 170)); // changed -- off-diagonal
                }
                table_->setItem(i, j + 1, item);
            }
        }
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        double changedPct = totalPixels > 0 ? (100.0 * changedPixels / totalPixels) : 0.0;
        double changedAreaSqm = changedPixels * pixelAreaSpin_->value();
        summaryLabel_->setText(QString(
            "%1 of %2 pixels changed class between date A and date B "
            "(%3%, \u2248 %4 sq. m). Green diagonal cells = unchanged; amber cells = changed "
            "(row = date-A class, column = date-B class it became).")
            .arg(changedPixels).arg(totalPixels)
            .arg(changedPct, 0, 'f', 1)
            .arg(changedAreaSqm, 0, 'f', 0));

        mw_->log("ChangeDetector", QString("Change matrix computed over %1 classes; %2%% of pixels changed.")
                                        .arg(n).arg(changedPct, 0, 'f', 1));

        // Timeline: each class's total area at Date A (row sum) vs Date B
        // (column sum), in the same units already used for the matrix
        // summary above -- converted to sq. km once the numbers get large,
        // purely so the chart's axis labels stay readable.
        double pixelArea = pixelAreaSpin_->value();
        bool useKm2 = (totalPixels * pixelArea) >= 1.0e6;
        double unitScale = useKm2 ? 1.0e6 : 1.0;
        QString areaUnit = useKm2 ? "sq. km" : "sq. m";

        QStringList classNames;
        QVector<double> areaA, areaB;
        QVector<QColor> colors;
        static const QColor palette[] = {
            QColor("#2ecc71"), QColor("#2980b9"), QColor("#e67e22"), QColor("#e74c3c"),
            QColor("#9b59b6"), QColor("#f1c40f"), QColor("#1abc9c"), QColor("#34495e"),
        };
        for (int i = 0; i < n; ++i) {
            long rowSum = 0, colSum = 0;
            for (int j = 0; j < n; ++j) rowSum += lastResult_.matrix[i][j];  // Date-A total for class i
            for (int j = 0; j < n; ++j) colSum += lastResult_.matrix[j][i]; // Date-B total for class i
            classNames << className(lastResult_.classIds[i]);
            areaA << (rowSum * pixelArea / unitScale);
            areaB << (colSum * pixelArea / unitScale);
            colors << palette[i % (sizeof(palette) / sizeof(palette[0]))];
        }
        timelineChart_->setData(classNames, areaA, areaB, colors, areaUnit);

        QMessageBox::information(this, "Change matrix computed",
            QString("%1 classes found across both dates.\n%2%% of the area changed class.")
                .arg(n).arg(changedPct, 0, 'f', 1));
    } catch (const std::exception& e) {
        mw_->log("ChangeDetector", QString("ERROR: %1").arg(e.what()));
        QMessageBox::critical(this, "Change detection failed", e.what());
    }
}

void ChangeDetectionDialog::toggleTimeline() {
    if (!hasResult_) {
        QMessageBox::warning(this, "Not available", "Run Generate Change Matrix first.");
        timelineBtn_->setChecked(false);
        return;
    }
    bool show = timelineBtn_->isChecked();
    timelineChart_->setVisible(show);
    timelineLabel_->setVisible(show);
}
