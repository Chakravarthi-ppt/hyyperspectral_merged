#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QComboBox>

class HsiMainWindow;
class MainWindow;   // real WESEE window (UI/MainWindow/mainwindow.h)

// Step 4 — Thematic: unsupervised spectral clustering. Shows the scene's
// natural spectral groupings as a k-cluster map -- no class names, just
// clusters (see ThematicClusterer). Deliberately minimal: pick a source
// cube, pick k, run, view.
class ThematicDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThematicDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

    // Optional: lets the dialog drop the saved raster onto the real,
    // georeferenced MapCanvas after Save (see other dialogs' setMapWindow).
    void setMapWindow(MainWindow* mw) { mapWindow_ = mw; }

private slots:
    void run();
    void save();

private:
    HsiMainWindow* mw_;
    MainWindow*    mapWindow_ = nullptr;

    QComboBox* sourceCombo_;   // which cube to cluster
    QSpinBox*  kSpin_;         // number of clusters
};
