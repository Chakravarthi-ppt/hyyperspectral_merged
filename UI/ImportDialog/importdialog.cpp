#include "importdialog.h"
#include "ui_importdialog.h"
#include "Processing/E0S-04/es04processor.h"
#include "../UI/SuccessDialog/successdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDir>


namespace
{
    const QString kOrgName     = "CAIR";
    const QString kAppName     = "OpticalFusion";
    const QString kSettingsKey = "WorkingFolder";

    const QString kCalibrationSubPath = "/PreProcessed/Calibration";
    const QString kOpticalSubPath     = "/PreProcessed/Optical";
}


importdialog::importdialog(es04processor *processor,
                           QWidget *parent)
    : QWidget(parent),
      ui(new Ui::importdialog),
      mProcessor(processor)
{
    ui->setupUi(this);

    setWindowTitle("Pre-Processing");

    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(240,240,240));
    setPalette(pal);


    if(mProcessor == nullptr)
    {
        qDebug() << "WARNING: es04processor pointer is null! "
                    "SAR progress signal will not be connected.";
    }
    else
    {
        connect(mProcessor,
                &es04processor::progressChanged,
                this,
                &importdialog::updateSARProgress);
    }

    connect(&mSentinelProcessor,
            &sentinel2processor::progressChanged,
            this,
            &importdialog::updateOpticalProgress);

    ui->progressSAR->setMinimum(0);
    ui->progressSAR->setMaximum(100);
    ui->progressSAR->setValue(0);

    ui->progressOptical->setMinimum(0);
    ui->progressOptical->setMaximum(100);
    ui->progressOptical->setValue(0);
}

importdialog::~importdialog()
{
    delete ui;
}


//----------------------------------------------------------------
// Settings Persistence
//----------------------------------------------------------------

void importdialog::saveWorkingFolder(const QString &folder)
{
    QSettings settings(kOrgName, kAppName);
    settings.setValue(kSettingsKey, folder);
    settings.sync();
}

QString importdialog::loadWorkingFolder()
{
    QSettings settings(kOrgName, kAppName);
    QString folder = settings.value(kSettingsKey).toString();

    if(folder.isEmpty())
    {
        return QString();
    }

    QDir dir(folder);

    // Working folder deleted, or project no longer valid
    if(!dir.exists() || !dir.exists("PreProcessed"))
    {
        settings.remove(kSettingsKey);
        return QString();
    }

    return folder;
}

//----------------------------------------------------------------
// Progress Bar Helpers
//----------------------------------------------------------------

void importdialog::updateSARProgress(int value,
                                     const QString &message)
{
    qDebug() << "SAR Progress:" << value << message;

    ui->progressSAR->setValue(value);

    ui->progressSAR->setFormat(
                QString("%1% - %2")
                .arg(value)
                .arg(message));

    qApp->processEvents();
}

void importdialog::updateOpticalProgress(int value, const QString &message)
{
    ui->progressOptical->setValue(value);
    ui->progressOptical->setFormat(QString("%1% - %2").arg(value).arg(message));
    qApp->processEvents();
}

//----------------------------------------------------------------
// Reusable UI Helpers
//----------------------------------------------------------------

bool importdialog::confirmIfExists(const QString &path,
                                    const QString &title,
                                    const QString &itemLabel)
{
    if(!QDir(path).exists())
    {
        return true; // Nothing to overwrite, safe to continue
    }

    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                title,
                itemLabel + " data already exists in this project.\n\n"
                "Do you want to replace it?",
                QMessageBox::Yes | QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void importdialog::showSuccess(const QString &message)
{
    QMessageBox::information(this, "Processing Completed", message);
}

void importdialog::showFailure(const QString &message)
{
    QMessageBox::critical(this, "Error", message);
}

bool importdialog::requireZipPath(const QString &path, const QString &label)
{
    if(path.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please select a " + label + " file.");
        return false;
    }
    return true;
}

//----------------------------------------------------------------
// File Selection
//----------------------------------------------------------------

void importdialog::on_btnSARBrowse_clicked()
{
    qDebug() << "Browse Button Clicked";

    QString filePath =
            QFileDialog::getOpenFileName(
                this,
                "Select EOS-04 ZIP File",
                QDir::homePath(),
                "ZIP Files (*.zip)");

    qDebug() << "Selected =" << filePath;

    if(filePath.isEmpty())
    {
        qDebug() << "Cancelled";
        return;
    }

    mES04ZipPath = filePath;

    ui->txtSARZipPath->setText(filePath);

    qDebug() << "Stored =" << mES04ZipPath;
}

void importdialog::updateProgress(int value,
                                  const QString &message)
{
    ui->progressSAR->setValue(value);
    qDebug() << message;
    qApp->processEvents();
}

void importdialog::on_btnOpticalBrowse_clicked()
{
    qDebug() << "Optical Browse Button Clicked";

    QString zipPath =
            QFileDialog::getOpenFileName(
                this,
                "Select Sentinel-2 ZIP",
                QDir::homePath(),
                "ZIP Files (*.zip)");

    if(zipPath.isEmpty())
    {
        qDebug() << "No ZIP Selected";

        return;
    }

    mOpticalZipPath = zipPath;

    ui->txtOpticalZipPath->setText(mOpticalZipPath);

    qDebug() << "Selected =" << zipPath;

    qDebug() << "Stored =" << mOpticalZipPath;

}


//void importdialog::on_btnOpticalRun_clicked()
//{

//    qDebug() << "Optical Run Button Clicked";

//    if(mOpticalZipPath.isEmpty())
//    {
//        QMessageBox::warning(
//                    this,
//                    "Warning",
//                    "Please select a Sentinel-2 ZIP file.");

//        return;
//    }

//    QString workingFolder =
//            QFileDialog::getExistingDirectory(
//                this,
//                "Select Working Folder");

//    if(workingFolder.isEmpty())
//    {
//        return;
//    }

//}


//----------------------------------------------------------------
// Working Folder Resolution
//----------------------------------------------------------------

QString importdialog::getWorkingFolder()
{
    if(mWorkingFolder.isEmpty())
    {
        mWorkingFolder = loadWorkingFolder();
    }

    // No previous project — ask for one directly
    if(mWorkingFolder.isEmpty())
    {
        QString folder = QFileDialog::getExistingDirectory(this, "Select Working Folder");

        if(folder.isEmpty())
        {
            return QString();
        }

        mWorkingFolder = folder;
        saveWorkingFolder(mWorkingFolder);
        return mWorkingFolder;
    }

    // Previous project found — confirm reuse
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Previous Project Found",
                "Current Working Folder:\n\n" + mWorkingFolder +
                "\n\nDo you want to continue using this project?",
                QMessageBox::Yes | QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        return mWorkingFolder;
    }

    // User wants a new project folder
    QString folder = QFileDialog::getExistingDirectory(this, "Select New Working Folder");

    if(folder.isEmpty())
    {
        return QString();
    }

    mWorkingFolder = folder;
    saveWorkingFolder(mWorkingFolder);
    return mWorkingFolder;
}

//----------------------------------------------------------------
// EOS-04 (SAR) Processing
//----------------------------------------------------------------

void importdialog::on_btnSARRun_clicked()
{
    updateSARProgress(0, "Starting...");

    if(!requireZipPath(mES04ZipPath, "EOS-04 ZIP"))
    {
        return;
    }

    QString workingFolder = getWorkingFolder();

    if(workingFolder.isEmpty())
    {
        return;
    }

    if(!confirmIfExists(workingFolder + kCalibrationSubPath,
                         "EOS-04 Already Processed",
                         "EOS-04"))
    {
        return;
    }

    if(mProcessor->process(mES04ZipPath, workingFolder))
    {
        updateSARProgress(100, "EOS-04 Completed.");

        showSuccess("EOS-04 processing completed successfully.\n\n"
                     "The processed data has been saved in the PreProcessed folder.");
    }
    else
    {
        showFailure("EOS-04 Processing Failed.");
    }
}

//----------------------------------------------------------------
// Sentinel-2 (Optical) Processing
//----------------------------------------------------------------

void importdialog::on_btnOpticalRun_clicked()
{
    if(!requireZipPath(mOpticalZipPath, "Sentinel-2 ZIP"))
    {
        return;
    }

    QString workingFolder = getWorkingFolder();

    if(workingFolder.isEmpty())
    {
        return;
    }

    if(!confirmIfExists(workingFolder + kOpticalSubPath,
                         "Sentinel-2 Already Processed",
                         "Sentinel-2"))
    {
        return;
    }

    if(mSentinelProcessor.process(mOpticalZipPath, workingFolder))
    {
        updateOpticalProgress(100, "Sentinel-2 Completed.");

        showSuccess("Sentinel-2 processing completed successfully.\n\n"
                     "The processed data has been saved in the PreProcessed folder.");
    }
    else
    {
        showFailure("Sentinel-2 Processing Failed.");
    }
}

//----------------------------------------------------------------
// Run All (EOS-04 + Sentinel-2)
//----------------------------------------------------------------

void importdialog::on_btnRunAll_clicked()
{
    if(!requireZipPath(mES04ZipPath, "EOS-04 ZIP"))
    {
        return;
    }

    if(!requireZipPath(mOpticalZipPath, "Sentinel-2 ZIP"))
    {
        return;
    }

    QString workingFolder = getWorkingFolder();

    if(workingFolder.isEmpty())
    {
        return;
    }

    //----------------------------------------------------
    // Combined Overwrite Check
    //----------------------------------------------------

    bool sarExists =
            QDir(workingFolder + kCalibrationSubPath).exists();

    bool opticalExists =
            QDir(workingFolder + kOpticalSubPath).exists();

    if(sarExists || opticalExists)
    {
        QString message =
                "This project already contains:\n\n";

        if(sarExists)
        {
            message += "✓ EOS-04 Processed Data\n";
        }

        if(opticalExists)
        {
            message += "✓ Sentinel-2 Processed Data\n";
        }

        message +=
                "\nRunning again will replace the existing processed files.\n\n"
                "Do you want to continue?";

        QMessageBox::StandardButton reply =
                QMessageBox::question(
                    this,
                    "Existing Project",
                    message,
                    QMessageBox::Yes | QMessageBox::No);

        if(reply == QMessageBox::No)
        {
            return;
        }
    }

    //----------------------------------------------------
    // Process EOS-04
    //----------------------------------------------------

    updateSARProgress(0, "Starting EOS-04...");

    if(!mProcessor->process(
                mES04ZipPath,
                workingFolder))
    {
        showFailure("EOS-04 Processing Failed.");
        return;
    }

    updateSARProgress(
                100,
                "EOS-04 Completed.");

    //----------------------------------------------------
    // Process Sentinel-2
    //----------------------------------------------------

    updateOpticalProgress(
                0,
                "Starting Sentinel-2...");

    if(!mSentinelProcessor.process(
                mOpticalZipPath,
                workingFolder))
    {
        showFailure("Sentinel-2 Processing Failed.");
        return;
    }

    updateOpticalProgress(
                100,
                "Sentinel-2 Completed.");

    //----------------------------------------------------
    // Success Dialog
    //----------------------------------------------------
    //----------------------------------------------------
    // Success Dialog
    //----------------------------------------------------

    SuccessDialog dialog(this);

    dialog.setWorkingFolder(workingFolder);

    QString preProcessedPath =
            workingFolder + "/PreProcessed";

    QString calibrationPath =
            preProcessedPath + "/Calibration";

    QString opticalPath =
            preProcessedPath + "/Optical";

    QString filteredPath =
            calibrationPath + "/Filtered";

    dialog.addStep(
                "SAR-Optical Fusion folder created",
                preProcessedPath);

    dialog.addStep(
                "EOS-04 polarization files renamed",
                calibrationPath);

    dialog.addStep(
                "Filter & Calibration of SAR completed",
                filteredPath);

    dialog.addStep(
                "Sentinel-2 bands extracted and renamed",
                opticalPath);

    dialog.exec();
}


void importdialog::on_btnClose_clicked()
{
   this->close();
}

void importdialog::on_btnClear_clicked()
{
    mES04ZipPath.clear();
    mOpticalZipPath.clear();

    ui->txtSARZipPath->clear();
    ui->txtOpticalZipPath->clear();

    ui->progressSAR->setValue(0);
    ui->progressOptical->setValue(0);

    ui->progressSAR->setValue(0);
    ui->progressSAR->setFormat("Ready");

    ui->progressOptical->setValue(0);
    ui->progressOptical->setFormat("Ready");

}

