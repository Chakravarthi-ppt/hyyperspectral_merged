#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>

class HsiMainWindow;

class BuiltUpClassificationDialog : public QDialog {
    Q_OBJECT
public:
    explicit BuiltUpClassificationDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

private slots:
    void browseSamplesCsv();
    void run();
    void exportVector();

private:
    HsiMainWindow* mw_;
    QLineEdit* samplesCsvEdit_;
    QLineEdit* builtUpClassNamesEdit_;
    QDoubleSpinBox* samAngleSpin_;
    QComboBox* fusionCombo_;
};
