#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include <QWidget>

#include "Processing/E0S-04/es04processor.h"
#include "Processing/Sentinel2/sentinel2processor.h"

namespace Ui {
class importdialog;
}

class importdialog : public QWidget
{
    Q_OBJECT

public:
    explicit importdialog(es04processor *processor,
                          QWidget *parent = nullptr);
    ~importdialog();



private slots:
    void on_btnSARRun_clicked();

    void on_btnSARBrowse_clicked();

    void updateProgress(int value,
                        const QString &message);

    void on_btnOpticalBrowse_clicked();

    void on_btnOpticalRun_clicked();

    void updateSARProgress(int value,
                        const QString &message);

    void updateOpticalProgress(int value,
                        const QString &message);

    QString getWorkingFolder();

    void saveWorkingFolder(const QString &folder);

    void on_btnRunAll_clicked();

    void on_btnClose_clicked();

    void on_btnClear_clicked();

private:
    Ui::importdialog *ui;

    QString mES04ZipPath;
    QString mOpticalZipPath;

//    es04processor mES04Processor;

    es04processor *mProcessor = nullptr;


    QString mWorkingFolder;

    bool mProjectCreated = false;

    sentinel2processor mSentinelProcessor;

    QString loadWorkingFolder();

private:
    bool confirmIfExists(const QString &path, const QString &title, const QString &itemLabel);
    bool requireZipPath(const QString &path, const QString &label);
    void showSuccess(const QString &message);
    void showFailure(const QString &message);
};

#endif // IMPORTDIALOG_H
