#include "successdialog.h"
#include "ui_successdialog.h"

#include <QDesktopServices>
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QUrl>

SuccessDialog::SuccessDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SuccessDialog)
{
    ui->setupUi(this);

    setWindowTitle("Pre-Processing Complete");
}

SuccessDialog::~SuccessDialog()
{
    delete ui;
}

void SuccessDialog::setWorkingFolder(const QString &folder)
{
    mWorkingFolder = folder;
}

void SuccessDialog::on_btnProceed_clicked()
{
     accept();
}


void SuccessDialog::on_btnShowFolder_clicked()
{
    QString folder =
                mWorkingFolder + "/PreProcessed";

        QDesktopServices::openUrl(
                    QUrl::fromLocalFile(folder));
}

void SuccessDialog::addStep(
        const QString &title,
        const QString &description)
{
    QLabel *label = new QLabel(this);

    label->setWordWrap(true);

    label->setStyleSheet(
                "padding:8px;"
                "font-size:11pt;"
                "border:1px solid #dcdcdc;"
                "border-radius:6px;"
                "background:white;");

    label->setText(
                "✅ <b>" + title +
                "</b><br><font color='gray'>" +
                description +
                "</font>");

    ui->verticalLayoutSteps->addWidget(label);
}

