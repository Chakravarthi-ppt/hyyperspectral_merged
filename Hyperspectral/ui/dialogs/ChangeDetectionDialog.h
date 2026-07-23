#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QWidget>
#include <QColor>
#include <QVector>
#include <QStringList>
#include "hsi/Types.h"

class HsiMainWindow;

// Small self-painted widget (no QtCharts dependency) showing each class's
// area at Date A vs Date B as a two-point trend line -- a minimal
// "timeline series" view of how much of each class grew/shrank between
// the two dates, sitting below the numeric change matrix.
class ChangeTimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChangeTimelineWidget(QWidget* parent = nullptr);
    void setData(const QStringList& classNames,
                 const QVector<double>& areaA,
                 const QVector<double>& areaB,
                 const QVector<QColor>& colors,
                 const QString& areaUnit);
    QSize minimumSizeHint() const override { return QSize(400, 220); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QStringList classNames_;
    QVector<double> areaA_, areaB_;
    QVector<QColor> colors_;
    QString areaUnit_;
    bool hasData_ = false;
};

class ChangeDetectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChangeDetectionDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

private slots:
    void run();
    void generateChangeDetection();
    void toggleTimeline();
    void browseDateA();
    void browseDateB();

private:
    HsiMainWindow* mw_;
    QDoubleSpinBox* pixelAreaSpin_;
    QTableWidget* table_;
    QLabel* summaryLabel_;
    QLabel* timelineLabel_;
    QPushButton* timelineBtn_;
    QLineEdit* dateAEdit_;
    QLineEdit* dateBEdit_;
    ChangeTimelineWidget* timelineChart_;
    hsi::ChangeMatrixResult lastResult_;
    bool hasResult_ = false;
};
