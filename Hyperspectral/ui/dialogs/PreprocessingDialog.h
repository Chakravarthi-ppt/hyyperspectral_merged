#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>

class HsiMainWindow;
class MainWindow;   // real WESEE window (UI/MainWindow/mainwindow.h)

// Preprocessing step: Orthorectification -> Radiance -> Surface Reflectance.
// Simplified per project spec:
//   - Only Dark Object Subtraction (DOS) is offered -- ELM (and its sample
//     CSV / calibration target fields) has been removed entirely.
//   - The dark-object percentile is no longer a user field -- it's fixed
//     internally at a sane default (see run()).
//   - Solar zenith angle and day-of-year are no longer typed in by hand --
//     they're auto-derived in run() (day-of-year from the EO-1 Hyperion
//     scene ID embedded in the filename; solar zenith from a standard
//     solar-position estimate using that date and the scene's center
//     latitude -- see run()'s comments for exactly how, and what it falls
//     back to if that derivation isn't possible) and shown read-only here
//     once available, so it's visible without needing to check the log.
class PreprocessingDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreprocessingDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

    // Optional: gives the dialog the real MainWindow so the surface-
    // reflectance preview it computes can be dropped onto the actual
    // georeferenced MapCanvas, not just held in memory.
    void setMapWindow(MainWindow* mw) { mapWindow_ = mw; }

private slots:
    void browseInput();
    void run();
    void resetFields();

private:
    HsiMainWindow* mw_;
    MainWindow*    mapWindow_ = nullptr;
    QLineEdit* inputPathEdit_;
    QDoubleSpinBox* solarZenithSpin_;  // read-only, auto-derived (see run())
    QSpinBox* dayOfYearSpin_;          // read-only, auto-derived (see run())
    QDoubleSpinBox* pixelSizeSpin_;
    QProgressBar* progressBar_;
    QLabel* statusValueLabel_;  // shows the pipeline's current step name (see run())
};
