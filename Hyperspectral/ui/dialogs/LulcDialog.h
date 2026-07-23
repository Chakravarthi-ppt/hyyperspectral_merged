#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QProgressBar>

class HsiMainWindow;
class MainWindow;   // real WESEE window (UI/MainWindow/mainwindow.h)


class LulcDialog : public QDialog {
    Q_OBJECT
public:
    explicit LulcDialog(HsiMainWindow* mainWindow, QWidget* parent = nullptr);

    // Optional: lets the dialog drop the classified raster onto the real,
    // georeferenced MapCanvas after classifying.
    void setMapWindow(MainWindow* mw) { mapWindow_ = mw; }

private slots:
    void runUnsupervised();
    void resetFields();

private:
    HsiMainWindow* mw_;
    MainWindow*    mapWindow_ = nullptr;
    QSpinBox* kSpin_;
    QProgressBar* progressBar_;
};
