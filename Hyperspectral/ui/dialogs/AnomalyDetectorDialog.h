#pragma once
#include <QDialog>
#include <memory>
#include "hsi/RxDetector.h"   // full RxResult needed for unique_ptr destructor (MOC)

QT_BEGIN_NAMESPACE
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QStackedWidget;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

class HsiMainWindow;

/**
 * AnomalyDetectorDialog
 *
 * UI for the Reed-Xiaoli (RX) anomaly detector.
 * Detection runs in a PipelineWorker background thread — the UI never blocks.
 *
 * Input:  Step 2 surface-reflectance cube (AppState::surfaceReflectance).
 * Output: score map + binary mask shown in preview; optionally saved to GeoTIFF.
 *
 * Access: Tools menu → "Anomaly Detector (RX)…"
 */
class AnomalyDetectorDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnomalyDetectorDialog(HsiMainWindow* mw, QWidget* parent = nullptr);

private slots:
    void onModeChanged(int index);
    void onRun();
    void onSaveScore();
    void onSaveMask();

private:
    void buildUi();
    void showResults(const hsi::RxResult& r);

    HsiMainWindow* m_mw;

    QComboBox*      m_modeCombo    = nullptr;
    QStackedWidget* m_paramStack   = nullptr;
    QSpinBox*       m_outerR       = nullptr;
    QSpinBox*       m_innerR       = nullptr;
    QSpinBox*       m_nPc          = nullptr;
    QDoubleSpinBox* m_pfa          = nullptr;
    QPushButton*    m_runBtn       = nullptr;
    QPushButton*    m_saveScoreBtn = nullptr;
    QPushButton*    m_saveMaskBtn  = nullptr;
    QLabel*         m_statusLabel  = nullptr;

    std::unique_ptr<hsi::RxResult> m_result;
};
