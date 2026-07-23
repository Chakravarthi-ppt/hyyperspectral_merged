#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>

class HsiMainWindow;

class LandCoverDialog : public QDialog {
    Q_OBJECT
public:
    explicit LandCoverDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

private slots:
    void runIndices();
    void runBuiltinSam();
    void saveResult();

private:
    HsiMainWindow* mw_;
    QTabWidget* tabs_;

    // Spectral indices tab
    QSpinBox*       redIdxSpin_;
    QSpinBox*       nirIdxSpin_;
    QSpinBox*       greenIdxSpin_;
    QSpinBox*       blueIdxSpin_;
    QSpinBox*       swirIdxSpin_;
    QComboBox*      presetCombo_;
    QDoubleSpinBox* ndviThreshSpin_;
    QDoubleSpinBox* ndwwThreshSpin_;
    QDoubleSpinBox* ndbiThreshSpin_;

    // SAM tab
    QDoubleSpinBox* samAngleSpin_;
};
