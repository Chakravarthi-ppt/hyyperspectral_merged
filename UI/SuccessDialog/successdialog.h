#ifndef SUCCESSDIALOG_H
#define SUCCESSDIALOG_H

#include <QDialog>

namespace Ui {
class SuccessDialog;
}

class SuccessDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SuccessDialog(QWidget *parent = nullptr);
    ~SuccessDialog();

    void setWorkingFolder(const QString &folder);

    void addStep(const QString &title,
                     const QString &description);


private slots:
    void on_btnProceed_clicked();

    void on_btnShowFolder_clicked();

private:
    Ui::SuccessDialog *ui;

    QString mWorkingFolder;
};

#endif // SUCCESSDIALOG_H
