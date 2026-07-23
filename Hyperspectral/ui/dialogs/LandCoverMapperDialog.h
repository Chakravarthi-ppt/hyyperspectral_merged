#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include "hsi/LandCoverMapper.h"

class HsiMainWindow;
class MainWindow;   // real WESEE window (UI/MainWindow/mainwindow.h)

// Three-tab dialog exposing all three offline land-cover approaches:
//   Tab 1 — Spectral Indices  (zero training, pure math)
//   Tab 2 — Spectral Library  (SAM against bundled/user signatures)
//   Tab 3 — Click-to-train    (mark pixels, SVM trains on-device)
class LandCoverMapperDialog : public QDialog {
    Q_OBJECT
public:
    explicit LandCoverMapperDialog(HsiMainWindow* mw, QWidget* parent = nullptr);

    // Optional: give the dialog the real MainWindow so that on export it
    // can drop the saved GeoTIFF onto the actual georeferenced MapCanvas
    // instead of only leaving it on disk / in the small preview.
    void setMapWindow(MainWindow* mw) { mapWindow_ = mw; }

private slots:
    void runIndexBased();
    void runLibraryBased();
    void browseLibrary();
    void runSvm();
    void addSampleRow();
    void removeSampleRow();
    void exportResult();
    void exportVector();

private:
    void buildIndexTab(QWidget* tab);
    void buildLibraryTab(QWidget* tab);
    void buildSvmTab(QWidget* tab);
    void showResult(const hsi::RasterCube& result);

    HsiMainWindow* mw_;
    MainWindow*    mapWindow_ = nullptr;
    QTabWidget* tabs_;

    // Tab 1 — index params
    QDoubleSpinBox* thrForest_;
    QDoubleSpinBox* thrVeg_;
    QDoubleSpinBox* thrWater_;
    QDoubleSpinBox* thrBuiltUp_;
    QDoubleSpinBox* thrBareSoil_;

    // Tab 2 — library
    QLineEdit*      libPathEdit_;
    QDoubleSpinBox* samAngleSpin_;

    // Tab 3 — click-to-train
    QTableWidget*   sampleTable_;   // columns: Class, Row, Col

    // Last result for export
    hsi::RasterCube lastResult_;
    bool            hasResult_ = false;
};
