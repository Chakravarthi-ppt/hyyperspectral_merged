#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QProgressBar>

class HsiMainWindow;

class PcaStackDialog : public QDialog {
    Q_OBJECT
public:
    explicit PcaStackDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

private slots:
    void runPca();
    void resetFields();

private:
    HsiMainWindow* mw_;

    QSpinBox*  pcaStartSpin_;
    QSpinBox*  pcaEndSpin_;
    QSpinBox*  pcaComponentsSpin_;
    QProgressBar* progressBar_;

};
