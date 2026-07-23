#include "sentinel2processor.h"

#include <QDebug>
#include <QProcess>
#include <QDirIterator>
#include <QElapsedTimer>

// ==================================================
// GDAL INCLUDES
// ==================================================
#include <gdal_priv.h>
#include <cpl_conv.h>
#include <ogr_spatialref.h>

sentinel2processor::sentinel2processor(QObject *parent)
    : ProcessorBase(parent)
{
    // Register GDAL drivers once per instance instead of once per band conversion
    GDALAllRegister();
}

bool sentinel2processor::process(const QString &zipFile,
                                 const QString &workingFolder)
{
    qDebug() << "========================================";
    qDebug() << "Sentinel-2 Processing Started";
    qDebug() << "========================================";

    mZipFile = zipFile;
    mWorkingFolder = workingFolder;

    emit progressChanged(10, "Extracting ZIP...");
    if(!extractZip())
        return false;

    emit progressChanged(20, "Finding SAFE Folder...");
    if(!findSafeFolder())
        return false;

    emit progressChanged(30, "Finding R10m Folder...");
    if(!findR10mFolder())
        return false;

    emit progressChanged(40, "Finding Bands...");
    if(!findBands())
        return false;

    emit progressChanged(50, "Creating Optical Folder...");
    if(!createOpticalFolder())
        return false;

    emit progressChanged(60, "Converting Bands...");
    if(!convertBandsToTiff())
        return false;

    emit progressChanged(100, "Sentinel-2 Processing Completed.");

    qDebug() << "========================================";
    qDebug() << "Sentinel-2 Processing Completed";
    qDebug() << "========================================";

    return true;
}

bool sentinel2processor::extractZip()
{
    qDebug() << "ZIP File     :" << mZipFile;
    qDebug() << "Destination  :" << mWorkingFolder;

    QProcess process;

    QStringList arguments;
    arguments << "-o" << mZipFile << "-d" << mWorkingFolder;

    QElapsedTimer timer;
    timer.start();

    process.start("unzip", arguments);
    process.waitForFinished(-1);

    qDebug() << "ZIP Extraction Time =" << timer.elapsed() << "ms";

    if(process.exitStatus() != QProcess::NormalExit ||
            process.exitCode() != 0)
    {
        qDebug() << "ZIP Extraction Failed.";
        qDebug() << process.readAllStandardError();
        return false;
    }

    qDebug() << "ZIP Extracted Successfully.";

    mExtractFolder = mWorkingFolder;

    return true;
}

bool sentinel2processor::findSafeFolder()
{
    QDirIterator iterator(
                mWorkingFolder,
                QStringList() << "*.SAFE",
                QDir::Dirs,
                QDirIterator::Subdirectories);

    if(iterator.hasNext())
    {
        mSafeFolder = iterator.next();

        qDebug() << "SAFE Folder Found:" << mSafeFolder;

        return true;
    }

    qDebug() << "SAFE Folder Not Found.";

    return false;
}

bool sentinel2processor::findR10mFolder()
{
    QDirIterator iterator(
                mSafeFolder,
                QStringList() << "R10m",
                QDir::Dirs,
                QDirIterator::Subdirectories);

    if(iterator.hasNext())
    {
        mR10mFolder = iterator.next();

        qDebug() << "R10m Folder Found:" << mR10mFolder;

        return true;
    }

    qDebug() << "R10m Folder Not Found.";

    return false;
}

QString sentinel2processor::findBand(const QString &bandName)
{
    QDirIterator iterator(
                mR10mFolder,
                QStringList() << ("*" + bandName + "_10m.jp2"),
                QDir::Files,
                QDirIterator::Subdirectories);

    if(iterator.hasNext())
    {
        return iterator.next();
    }

    return QString();
}

bool sentinel2processor::findBands()
{
    mB02File = findBand("B02");
    mB03File = findBand("B03");
    mB04File = findBand("B04");
    mB08File = findBand("B08");

    if(mB02File.isEmpty() ||
            mB03File.isEmpty() ||
            mB04File.isEmpty() ||
            mB08File.isEmpty())
    {
        qDebug() << "One or more required bands are missing.";
        return false;
    }

    // Store JP2 paths
    mBand02JP2 = mB02File;
    mBand03JP2 = mB03File;
    mBand04JP2 = mB04File;
    mBand08JP2 = mB08File;

    qDebug() << "B02 :" << mBand02JP2;
    qDebug() << "B03 :" << mBand03JP2;
    qDebug() << "B04 :" << mBand04JP2;
    qDebug() << "B08 :" << mBand08JP2;

    return true;
}

bool sentinel2processor::createOpticalFolder()
{
    mPreProcessedFolder = mWorkingFolder + "/PreProcessed";
    mOpticalFolder      = mPreProcessedFolder + "/Optical";

    QDir dir;

    //----------------------------------------------------
    // Check PreProcessed Folder
    //----------------------------------------------------

    if(!dir.exists(mPreProcessedFolder))
    {
        if(!dir.mkpath(mPreProcessedFolder))
        {
            qDebug() << "Failed to create PreProcessed folder.";
            return false;
        }

        qDebug() << "PreProcessed folder created.";
    }

    //----------------------------------------------------
    // Check Optical Folder
    //----------------------------------------------------

    if(!dir.exists(mOpticalFolder))
    {
        if(!dir.mkpath(mOpticalFolder))
        {
            qDebug() << "Failed to create Optical folder.";
            return false;
        }

        qDebug() << "Optical folder created.";
    }

    qDebug() << "Optical Path :" << mOpticalFolder;

    QDir opticalDir(mOpticalFolder);

    mBand02TIF = opticalDir.filePath("B02.tif");
    mBand03TIF = opticalDir.filePath("B03.tif");
    mBand04TIF = opticalDir.filePath("B04.tif");
    mBand08TIF = opticalDir.filePath("B08.tif");

    return true;
}

bool sentinel2processor::convertBandsToTiff()
{
    qDebug() << "========================================";
    qDebug() << "Converting Bands to TIFF";
    qDebug() << "========================================";

    // Log GDAL environment info once, not once per band
    qDebug() << "GDAL Version :" << GDALVersionInfo("--version");
    qDebug() << "GDAL_DATA :" << CPLGetConfigOption("GDAL_DATA", "Not Set");
    qDebug() << "GDAL_DRIVER_PATH :" << CPLGetConfigOption("GDAL_DRIVER_PATH", "Not Set");

    struct BandJob { const QString &input; const QString &output; };

    const BandJob jobs[] =
    {
        { mB02File, mBand02TIF },
        { mB03File, mBand03TIF },
        { mB04File, mBand04TIF },
        { mB08File, mBand08TIF }
    };

    for(const auto &job : jobs)
    {
        if(!convertJP2ToTiff(job.input, job.output))
        {
            return false;
        }
    }

    qDebug() << "All Bands Converted Successfully.";

    return true;
}

bool sentinel2processor::convertJP2ToTiff(
        const QString &inputJP2,
        const QString &outputTIF)
{
    GDALDataset *inputDataset =
            (GDALDataset*)GDALOpen(
                inputJP2.toStdString().c_str(),
                GA_ReadOnly);

    if(inputDataset == nullptr)
    {
        qDebug() << "Failed to open JP2 :" << inputJP2;
        return false;
    }

    GDALRasterBand *inputBand = inputDataset->GetRasterBand(1);

    int width  = inputBand->GetXSize();
    int height = inputBand->GetYSize();

    GDALDataType dataType = inputBand->GetRasterDataType();

    GDALDriver *driver =
            GetGDALDriverManager()->GetDriverByName("GTiff");

    if(driver == nullptr)
    {
        qDebug() << "Failed to get GTiff driver.";
        GDALClose(inputDataset);
        return false;
    }

    GDALDataset *outputDataset =
            driver->Create(
                outputTIF.toStdString().c_str(),
                width,
                height,
                1,
                dataType,
                nullptr);

    if(outputDataset == nullptr)
    {
        qDebug() << "Failed to create output TIFF :" << outputTIF;
        GDALClose(inputDataset);
        return false;
    }

    double geoTransform[6];

    if(inputDataset->GetGeoTransform(geoTransform) == CE_None)
    {
        outputDataset->SetGeoTransform(geoTransform);
    }

    outputDataset->SetProjection(inputDataset->GetProjectionRef());

    GDALRasterBand *outputBand = outputDataset->GetRasterBand(1);

    std::vector<uint16_t> buffer(width);
    bool success = true;

    for(int row = 0; row < height; row++)
    {
        if(inputBand->RasterIO(GF_Read, 0, row, width, 1,
                               buffer.data(), width, 1,
                               GDT_UInt16, 0, 0) != CE_None)
        {
            qDebug() << "Failed to read row" << row << "from" << inputJP2;
            success = false;
            break;
        }

        if(outputBand->RasterIO(GF_Write, 0, row, width, 1,
                                buffer.data(), width, 1,
                                GDT_UInt16, 0, 0) != CE_None)
        {
            qDebug() << "Failed to write row" << row << "to" << outputTIF;
            success = false;
            break;
        }
    }

    if(success)
    {
        qDebug() << "Converted :" << inputJP2 << "->" << outputTIF
                 << "(" << width << "x" << height << ","
                 << GDALGetDataTypeName(dataType) << ")";
    }

    GDALClose(outputDataset);
    GDALClose(inputDataset);

    return success;
}
