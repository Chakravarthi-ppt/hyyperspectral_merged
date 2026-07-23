#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include "hsi/Types.h"

class HsiMainWindow;
class MainWindow;

class ThematicDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThematicDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

    // Optional: lets the dialog drop the saved raster onto the real,
    // georeferenced MapCanvas after Save (see other dialogs' setMapWindow).
    void setMapWindow(MainWindow* mw) { mapWindow_ = mw; }

private slots:
    void run();
    void saveRaster();
    void saveVector();
    void onVisibilityChanged();

private:
    void applyVisibilityAndShow();

    HsiMainWindow* mw_;
    MainWindow*    mapWindow_ = nullptr;

    QDoubleSpinBox* ndviMin_;
    QDoubleSpinBox* ndviMax_;
    QDoubleSpinBox* ndwiMin_;
    QDoubleSpinBox* ndwiMax_;
    QDoubleSpinBox* ndbiMin_;
    QDoubleSpinBox* ndbiMax_;

    QCheckBox* showVeg_;
    QCheckBox* showWater_;
    QCheckBox* showBuiltUp_;

    // Unfiltered classification (all 3 classes) from the last run(), kept
    // so toggling a checkbox can re-filter without recomputing the indices.
    hsi::RasterCube rawResult_;
    bool            hasRawResult_ = false;

    hsi::RasterCube lastResult_;   // the filtered raster actually shown/exported
    bool            hasResult_ = false;
};
