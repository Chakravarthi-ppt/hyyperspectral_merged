#include "es04processor.h"
#include <QRegularExpression>
#include "gdal_priv.h"
#include "cpl_conv.h"
#include <cmath>
#include <QElapsedTimer>
#include <QThread>

#include "../Common/threadmanager.h"

es04processor::es04processor(QObject *parent)
    : ProcessorBase(parent)
{
}

bool es04processor::setZipFile(const QString &zipPath)
{
    if(zipPath.isEmpty())
        return false;

    mZipFile = zipPath;

    return true;
}

bool es04processor::extractZip(const QString &destinationFolder)
{
    if (mZipFile.isEmpty())
    {
        qDebug() << "ZIP file not selected.";
        return false;
    }

    if (destinationFolder.isEmpty())
    {
        qDebug() << "Destination folder not selected.";
        return false;
    }

    mWorkingFolder = destinationFolder;
    mExtractFolder = destinationFolder;

    qDebug() << "ZIP File      :" << mZipFile;
    qDebug() << "Destination   :" << mExtractFolder;

    QProcess unzipProcess;

    QStringList arguments;
    arguments << "-o"
              << mZipFile
              << "-d"
              << mExtractFolder;

    unzipProcess.start("unzip", arguments);

    if (!unzipProcess.waitForStarted())
    {
        qDebug() << "Failed to start unzip process.";
        return false;
    }

    if (!unzipProcess.waitForFinished(-1))
    {
        qDebug() << "Extraction timed out.";
        return false;
    }

    if (unzipProcess.exitCode() != 0)
    {
        qDebug() << unzipProcess.readAllStandardError();
        return false;
    }

    qDebug() << "ZIP extracted successfully.";

    return true;
}

bool es04processor::findRootFolder()
{
    if(mExtractFolder.isEmpty())
    {
        qDebug() << "Extraction folder is empty.";
        return false;
    }

    QFileInfo zipInfo(mZipFile);

    // Remove only the .zip extension
    QString folderName = zipInfo.completeBaseName();

    QString rootFolder =
            QDir(mExtractFolder).filePath(folderName);

    if(!QDir(rootFolder).exists())
    {
        qDebug() << "EOS Root Folder Not Found.";
        qDebug() << "Expected :" << rootFolder;
        return false;
    }

    mRootFolder = rootFolder;

    qDebug() << "EOS Root Folder Found :";
    qDebug() << mRootFolder;

    return true;
}

bool es04processor::createPreProcessedFolder()
{
    //----------------------------------------------------
    // Main Folder
    //----------------------------------------------------

    mPreProcessedFolder =
            mWorkingFolder + "/PreProcessed";

    //----------------------------------------------------
    // Sub Folders
    //----------------------------------------------------

    mCalibrationFolder =
            mPreProcessedFolder + "/Calibration";

    mFilterFolder =
            mPreProcessedFolder + "/Filter";

    mRenamedFolder =
            mPreProcessedFolder + "/Auxillary";

    //----------------------------------------------------
    // Create Folders
    //----------------------------------------------------

    QDir dir;

    const QStringList foldersToCreate =
    {
        mPreProcessedFolder,
        mRenamedFolder,
        mFilterFolder,
        mCalibrationFolder
    };

    for(const QString &folder : foldersToCreate)
    {
        if(!dir.exists(folder))
        {
            if(!dir.mkpath(folder))
            {
                qDebug() << "Failed to create folder :" << folder;
                return false;
            }

            qDebug() << "Created folder :" << folder;
        }
    }
    //----------------------------------------------------
    // Debug
    //----------------------------------------------------

    qDebug() << "PreProcessed Path :" << mPreProcessedFolder;
    qDebug() << "Auxillary Path      :" << mRenamedFolder;
    qDebug() << "Filter Path      :" << mFilterFolder;
    qDebug() << "Calibration Path :" << mCalibrationFolder;

    return true;
}

QString es04processor::getRootFolder() const
{
    return mRootFolder;
}

bool es04processor::process(const QString &zipFile,
                            const QString &destinationFolder)
{
    qDebug() << "========================================";
    qDebug() << "EOS-04 Processing Started";
    qDebug() << "========================================";

    emit progressChanged(0, "Starting...");

    if(!setZipFile(zipFile))
        return false;

    if(!extractZip(destinationFolder))
        return false;

    emit progressChanged(10, "ZIP Extracted");

    if(!findRootFolder())
        return false;

    emit progressChanged(20, "Root Folder Found");

    if(!findBandMeta())
        return false;

    emit progressChanged(30, "BAND_META Found");

    if(!readCalibrationConstants())
        return false;

    emit progressChanged(40, "Calibration Constants Read");

    if(!createPreProcessedFolder())
        return false;

    emit progressChanged(50, "PreProcessed Folder Created");

    if(!extractRequiredFiles())
        return false;

    emit progressChanged(60, "Files Extracted");

    QDir dir(mPreProcessedFolder);

    qDebug() << "Folder Exists :" << dir.exists();
    qDebug() << "Folder Path   :" << mPreProcessedFolder;

    if(!enhancedLeeFilter())
        return false;

    emit progressChanged(80, "Enhanced Lee Completed");

    if(!gammaCalibration())
        return false;

    emit progressChanged(88, "Gamma Calibration Completed");

    if(!betaCalibration())
        return false;

    emit progressChanged(94, "Beta Calibration Completed");

    if(!sigmaCalibration())
        return false;

    emit progressChanged(100, "Sigma Calibration Completed");

            QStringList files;

            files << mHHFile
                  << mHVFile
                  << mAreaFile
                  << mLIAFile
                  << mEnhancedHHFile
                  << mEnhancedHVFile
                  << mGammaHHFile
                  << mGammaHVFile
                  << mBetaHHFile
                  << mBetaHVFile
                  << mSigmaHHFile
                  << mSigmaHVFile;

            for(const QString &file : files)
            {
                if(file.isEmpty())
                    continue;

                if(!mRasterManager.rasterPathExists(file))
                {
                    mRasterManager.insertRasterPath(file);

                    mRasterManager.loadRaster(file);

                    qDebug()
                        << "Stored into database:"
                        << file;
                }
            }

    qDebug() << "========================================";
    qDebug() << "EOS-04 Processing Completed Successfully";
    qDebug() << "========================================";

    qDebug() << "Processor =" << this;
    qDebug() << "Emitting processingFinished:" << mPreProcessedFolder;
    emit processingFinished(mPreProcessedFolder);

    return true;
}

bool es04processor::findBandMeta()
{
    if(mRootFolder.isEmpty())
    {
        qDebug() << "Root folder is empty.";

        return false;
    }

    QDir rootDir(mRootFolder);

    QStringList metaFiles =
            rootDir.entryList(QStringList() << "BAND_META.txt",
                              QDir::Files,
                              QDir::Name);

    if(metaFiles.isEmpty())
    {
        qDebug() << "BAND_META.txt not found.";

        return false;
    }

    mBandMetaFile =
            rootDir.absoluteFilePath(metaFiles.first());

    qDebug() << "BAND_META Found :";

    qDebug() << mBandMetaFile;

    return true;
}

bool es04processor::readCalibrationConstants()
{
    QFile file(mBandMetaFile);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open BAND_META.txt";
        return false;
    }

    QTextStream in(&file);

    while(!in.atEnd())
    {
        QString line = in.readLine().trimmed();

        //-------------------------------------------------
        // Product Name
        //-------------------------------------------------

        if(line.startsWith("OTSProductID="))
        {
            mProduct.productName = extractValue(line);

            qDebug() << "Product Name :" << mProduct.productName;
        }

        //-------------------------------------------------
        // Sigma Constants
        //-------------------------------------------------

        else if(line.startsWith("Calibration_Constant_HH="))
        {
            mCalibration.sigmaHH =
                    extractValue(line).toDouble();
        }

        else if(line.startsWith("Calibration_Constant_HV="))
        {
            mCalibration.sigmaHV =
                    extractValue(line).toDouble();
        }

        //-------------------------------------------------
        // Beta Constants
        //-------------------------------------------------

        else if(line.startsWith("Calibration_Constant_Beta0_HH="))
        {
            mCalibration.betaHH =
                    extractValue(line).toDouble();
        }

        else if(line.startsWith("Calibration_Constant_Beta0_HV="))
        {
            mCalibration.betaHV =
                    extractValue(line).toDouble();
        }
    }

    file.close();

    //-------------------------------------------------
    // If Product Name not found
    //-------------------------------------------------

    if(mProduct.productName.isEmpty())
    {
        mProduct.productName = getProductName();
    }

    //-------------------------------------------------
    // Debug
    //-------------------------------------------------

    qDebug() << "------------------------------------";
    qDebug() << "Product Name :" << mProduct.productName;
    qDebug() << "Sigma HH :" << mCalibration.sigmaHH;
    qDebug() << "Sigma HV :" << mCalibration.sigmaHV;
    qDebug() << "Beta HH  :" << mCalibration.betaHH;
    qDebug() << "Beta HV  :" << mCalibration.betaHV;
    qDebug() << "------------------------------------";

    return true;
}

QString es04processor::extractValue(const QString &line)
{
    int index = line.indexOf('=');

    if (index == -1)
        return QString();

    return line.mid(index + 1).trimmed();
}

bool es04processor::extractRequiredFiles()
{
    //-------------------------------------------------------
    // Find Original Files
    //-------------------------------------------------------

    QString sourceHH   = findFile("imagery_HH.tif");
    QString sourceHV   = findFile("imagery_HV.tif");
    QString sourceArea = findFile("_area.tif");
    QString sourceLIA  = findFile("_lia.tif");

    if(sourceHH.isEmpty() ||
       sourceHV.isEmpty() ||
       sourceArea.isEmpty() ||
       sourceLIA.isEmpty())
    {
        qDebug() << "Required files not found.";
        return false;
    }

    //-------------------------------------------------------
    // Product Name
    //-------------------------------------------------------

    QString product = getProductName();

    if(product.isEmpty())
    {
        qDebug() << "Unable to read Product Name.";
        return false;
    }

    qDebug() << "Product Name :" << product;

    //-------------------------------------------------------
    // Destination Files (Renamed Folder)
    //-------------------------------------------------------

    mHHFile =
            mRenamedFolder + "/" +
            product + "_HH.tif";

    mHVFile =
            mRenamedFolder + "/" +
            product + "_HV.tif";

    mAreaFile =
            mRenamedFolder + "/" +
            product + "_area.tif";

    mLIAFile =
            mRenamedFolder + "/" +
            product + "_lia.tif";

    //-------------------------------------------------------
    // Remove old files if they already exist
    //-------------------------------------------------------

    QFile::remove(mHHFile);
    QFile::remove(mHVFile);
    QFile::remove(mAreaFile);
    QFile::remove(mLIAFile);

    //-------------------------------------------------------
    // Copy Files
    //-------------------------------------------------------

    if(!QFile::copy(sourceHH, mHHFile))
    {
        qDebug() << "Failed to copy HH.";
        return false;
    }

    if(!QFile::copy(sourceHV, mHVFile))
    {
        qDebug() << "Failed to copy HV.";
        return false;
    }

    if(!QFile::copy(sourceArea, mAreaFile))
    {
        qDebug() << "Failed to copy Area.";
        return false;
    }

    if(!QFile::copy(sourceLIA, mLIAFile))
    {
        qDebug() << "Failed to copy LIA.";
        return false;
    }

    //-------------------------------------------------------
    // Debug
    //-------------------------------------------------------

    qDebug() << "HH   :" << mHHFile;
    qDebug() << "HV   :" << mHVFile;
    qDebug() << "Area :" << mAreaFile;
    qDebug() << "LIA  :" << mLIAFile;

    return true;
}

QString es04processor::findFile(const QString &keyword)
{
    QDirIterator iterator(mRootFolder,
                          QDir::Files,
                          QDirIterator::Subdirectories);

    while(iterator.hasNext())
    {
        QString filePath = iterator.next();

        QFileInfo fileInfo(filePath);

        QString fileName =
                fileInfo.fileName().toLower();

        // Ignore PreProcessed folder
        if(filePath.contains("/PreProcessed/",
                             Qt::CaseInsensitive))
            continue;

        // Search only TIFF files
        if(!fileName.endsWith(".tif"))
            continue;

        if(fileName.contains(keyword.toLower()))
        {
            qDebug() << "Found :" << filePath;

            return filePath;
        }
    }

    qDebug() << keyword << "Not Found";

    return QString();
}

QString es04processor::getProductName()
{
    QFile file(mBandMetaFile);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&file);

    while(!in.atEnd())
    {
        QString line = in.readLine();

        if(line.startsWith("OTSProductID="))
        {
            QString product = extractValue(line);

            QRegularExpression regex(
                        "(E04_SAR_MRS_\\d{2}[A-Z]{3}\\d{4}).*(N\\d+_E\\d+)");

            QRegularExpressionMatch match =
                    regex.match(product);

            if(match.hasMatch())
            {
                return match.captured(1) + "_" +
                        match.captured(2);
            }

            return product;
        }
    }

    return QString();
}

bool es04processor::gammaCalibration()
{
    qDebug() << "================================";
    qDebug() << "Starting Gamma Calibration...";
    qDebug() << "================================";

    QElapsedTimer timer;
    timer.start();

    mGammaHHFile =
            QDir(mCalibrationFolder).filePath(
                getProductName() + "_Gamma_HH.tif");

    mGammaHVFile =
            QDir(mCalibrationFolder).filePath(
                getProductName() + "_Gamma_HV.tif");

    bool status = ThreadManager::runParallel({

                                                 [this]()
                                                 {
                                                     return calibrateGamma(
                                                     mEnhancedHHFile,
                                                     mGammaHHFile,
                                                     mCalibration.betaHH);
                                                 },

                                                 [this]()
                                                 {
                                                     return calibrateGamma(
                                                     mEnhancedHVFile,
                                                     mGammaHVFile,
                                                     mCalibration.betaHV);
                                                 }

                                             });

    qDebug() << "Gamma Time =" << timer.elapsed() << "ms";

    if(!status)
    {
        qDebug() << "Gamma Calibration Failed.";
        return false;
    }

    qDebug() << "================================";
    qDebug() << "Gamma Calibration Completed.";
    qDebug() << "================================";

    return true;
}


bool es04processor::calibrateGamma(const QString &inputFile,
                                   const QString &outputFile,
                                   double calibrationConstant)
{
    GDALAllRegister();

    GDALDataset *inputDataset =
            (GDALDataset *)GDALOpen(
                inputFile.toStdString().c_str(),
                GA_ReadOnly);

    if(inputDataset == nullptr)
    {
        qDebug() << "Unable to open input image.";
        return false;
    }

    GDALRasterBand *inputBand =
            inputDataset->GetRasterBand(1);

    int width  = inputBand->GetXSize();
    int height = inputBand->GetYSize();

    //--------------------------------------------------
    // Read Complete Image
    //--------------------------------------------------

    std::vector<float> inputBuffer(width * height);

    if(inputBand->RasterIO(GF_Read,
                           0,
                           0,
                           width,
                           height,
                           inputBuffer.data(),
                           width,
                           height,
                           GDT_Float32,
                           0,
                           0) != CE_None)
    {
        qDebug() << "Unable to read input raster.";

        GDALClose(inputDataset);

        return false;
    }

    //--------------------------------------------------
    // Output Buffer
    //--------------------------------------------------

    std::vector<float> outputBuffer(width * height);

    //--------------------------------------------------
    // Parallel Processing
    //--------------------------------------------------

    ThreadManager::parallelFor(
                height,
                [&](int startRow, int endRow)
    {
        for(int row = startRow; row < endRow; row++)
        {
            int offset = row * width;

            for(int col = 0; col < width; col++)
            {
                float dn = inputBuffer[offset + col];

                if(dn <= 0.0f)
                {
                    outputBuffer[offset + col] = -9999.0f;
                }
                else
                {
                    outputBuffer[offset + col] =
                            static_cast<float>(
                                20.0 * log10(dn)
                                - calibrationConstant);
                }
            }
        }
    });

    //--------------------------------------------------
    // Create Output
    //--------------------------------------------------

    GDALDriver *driver =
            GetGDALDriverManager()->GetDriverByName("GTiff");

    GDALDataset *outputDataset =
            driver->Create(outputFile.toStdString().c_str(),
                           width,
                           height,
                           1,
                           GDT_Float32,
                           nullptr);

    if(outputDataset == nullptr)
    {
        GDALClose(inputDataset);
        return false;
    }

    double geoTransform[6];

    inputDataset->GetGeoTransform(geoTransform);

    outputDataset->SetGeoTransform(geoTransform);

    outputDataset->SetProjection(
                inputDataset->GetProjectionRef());

    GDALRasterBand *outputBand =
            outputDataset->GetRasterBand(1);

    if(outputBand->RasterIO(GF_Write,
                            0,
                            0,
                            width,
                            height,
                            outputBuffer.data(),
                            width,
                            height,
                            GDT_Float32,
                            0,
                            0) != CE_None)
    {
        qDebug() << "Unable to write output raster.";

        GDALClose(outputDataset);
        GDALClose(inputDataset);

        return false;
    }

    GDALClose(outputDataset);
    GDALClose(inputDataset);

    return true;
}
bool es04processor::betaCalibration()
{
    qDebug() << "================================";
    qDebug() << "Starting Beta Calibration...";
    qDebug() << "================================";

    QElapsedTimer timer;
    timer.start();

    mBetaHHFile =
            QDir(mCalibrationFolder).filePath(
                getProductName() + "_Beta_HH.tif");

    mBetaHVFile =
            QDir(mCalibrationFolder).filePath(
                getProductName() + "_Beta_HV.tif");

    bool status = ThreadManager::runParallel({

                                                 [this]()
                                                 {
                                                     return calibrateBeta(
                                                     mGammaHHFile,
                                                     mAreaFile,
                                                     mBetaHHFile);
                                                 },

                                                 [this]()
                                                 {
                                                     return calibrateBeta(
                                                     mGammaHVFile,
                                                     mAreaFile,
                                                     mBetaHVFile);
                                                 }

                                             });

    qDebug() << "Beta Time =" << timer.elapsed() << "ms";

    if(!status)
    {
        qDebug() << "Beta Calibration Failed.";
        return false;
    }

    qDebug() << "================================";
    qDebug() << "Beta Calibration Completed.";
    qDebug() << "================================";

    return true;
}


bool es04processor::calibrateBeta(const QString &gammaFile,
                                  const QString &areaFile,
                                  const QString &outputFile)
{
    GDALAllRegister();

    GDALDataset *gammaDS =
            (GDALDataset *)GDALOpen(gammaFile.toStdString().c_str(),
                                    GA_ReadOnly);

    GDALDataset *areaDS =
            (GDALDataset *)GDALOpen(areaFile.toStdString().c_str(),
                                    GA_ReadOnly);

    if(gammaDS == nullptr || areaDS == nullptr)
    {
        qDebug() << "Unable to open Gamma or Area image.";

        return false;
    }

    GDALRasterBand *gammaBand = gammaDS->GetRasterBand(1);
    GDALRasterBand *areaBand  = areaDS->GetRasterBand(1);

    int width = gammaBand->GetXSize();
    int height = gammaBand->GetYSize();

    if(width != areaBand->GetXSize() ||
            height != areaBand->GetYSize())
    {
        qDebug() << "Gamma and Area dimensions mismatch!";

        GDALClose(gammaDS);
        GDALClose(areaDS);

        return false;
    }

    GDALDriver *driver =
            GetGDALDriverManager()->GetDriverByName("GTiff");

    GDALDataset *outputDS =
            driver->Create(outputFile.toStdString().c_str(),
                           width,
                           height,
                           1,
                           GDT_Float32,
                           nullptr);

    double geoTransform[6];

    gammaDS->GetGeoTransform(geoTransform);

    outputDS->SetGeoTransform(geoTransform);

    outputDS->SetProjection(gammaDS->GetProjectionRef());

    GDALRasterBand *outputBand =
            outputDS->GetRasterBand(1);

    std::vector<float> gammaBuffer(width);
    std::vector<float> areaBuffer(width);
    std::vector<float> outputBuffer(width);

    for(int row = 0; row < height; row++)
    {
        gammaBand->RasterIO(GF_Read,
                            0,
                            row,
                            width,
                            1,
                            gammaBuffer.data(),
                            width,
                            1,
                            GDT_Float32,
                            0,
                            0);

        areaBand->RasterIO(GF_Read,
                           0,
                           row,
                           width,
                           1,
                           areaBuffer.data(),
                           width,
                           1,
                           GDT_Float32,
                           0,
                           0);

        for(int col = 0; col < width; col++)
        {
            if(areaBuffer[col] <= 0)
            {
                outputBuffer[col] = -9999;
            }
            else
            {
                outputBuffer[col] =
                        gammaBuffer[col] +
                        (10.0 * log10(areaBuffer[col]));
            }
        }

        outputBand->RasterIO(GF_Write,
                             0,
                             row,
                             width,
                             1,
                             outputBuffer.data(),
                             width,
                             1,
                             GDT_Float32,
                             0,
                             0);
    }

    GDALClose(outputDS);
    GDALClose(gammaDS);
    GDALClose(areaDS);

    return true;
}


bool es04processor::sigmaCalibration()
{
    qDebug() << "================================";
    qDebug() << "Starting Sigma Calibration...";
    qDebug() << "================================";

    QElapsedTimer timer;
    timer.start();

    mSigmaHHFile =
            QDir(mCalibrationFolder).filePath(
                getProductName() + "_Sigma_HH.tif");

    mSigmaHVFile =
            QDir(mCalibrationFolder).filePath(
                getProductName() + "_Sigma_HV.tif");

    bool status = ThreadManager::runParallel({

                                                 [this]()
                                                 {
                                                     return calibrateSigma(
                                                     mBetaHHFile,
                                                     mAreaFile,
                                                     mSigmaHHFile);
                                                 },

                                                 [this]()
                                                 {
                                                     return calibrateSigma(
                                                     mBetaHVFile,
                                                     mAreaFile,
                                                     mSigmaHVFile);
                                                 }

                                             });

    qDebug() << "Sigma Time =" << timer.elapsed() << "ms";

    if(!status)
    {
        qDebug() << "Sigma Calibration Failed.";
        return false;
    }

    qDebug() << "================================";
    qDebug() << "Sigma Calibration Completed.";
    qDebug() << "================================";

    return true;
}

bool es04processor::calibrateSigma(const QString &betaFile,
                                   const QString &liaFile,
                                   const QString &outputFile)
{
    Raster beta =
            readRaster(betaFile);

    Raster lia =
            readRaster(liaFile);

    if(beta.width != lia.width ||
            beta.height != lia.height)
    {
        qDebug() << "Beta and LIA dimensions mismatch!";

        return false;
    }

    Raster sigma = beta;

    for(int row = 0; row < beta.height; row++)
    {
        for(int col = 0; col < beta.width; col++)
        {
            float betaPixel =
                    beta.pixels[
                    row * beta.width + col];

            float liaPixel =
                    lia.pixels[
                    row * lia.width + col];

            if(liaPixel <= 0.0f)
            {
                sigma.pixels[
                        row * sigma.width + col] = 0.0f;

                continue;
            }

            sigma.pixels[
                    row * sigma.width + col] =
                    betaPixel +
                    10.0f *
                    std::log10(liaPixel);
        }
    }

    return writeRaster(
                outputFile,
                sigma);
}

es04processor::Raster
es04processor::readRaster(const QString &file)
{
    Raster raster;

    GDALDataset *dataset =
            (GDALDataset *)GDALOpen(
                file.toStdString().c_str(),
                GA_ReadOnly);

    if(!dataset)
    {
        qDebug() << "Unable to open raster :" << file;
        return raster;
    }

    raster.width = dataset->GetRasterXSize();
    raster.height = dataset->GetRasterYSize();

    raster.pixels.resize(
                raster.width *
                raster.height);

    dataset->GetGeoTransform(
                raster.geoTransform);

    raster.projection =
            QString(
                dataset->GetProjectionRef());

    GDALRasterBand *band =
            dataset->GetRasterBand(1);

    int hasNoData = 0;

    raster.noData =
            band->GetNoDataValue(
                &hasNoData);

    band->RasterIO(
                GF_Read,
                0,
                0,
                raster.width,
                raster.height,
                raster.pixels.data(),
                raster.width,
                raster.height,
                GDT_Float32,
                0,
                0);

    GDALClose(dataset);

    return raster;
}



bool es04processor::writeRaster(const QString &file,
                                const Raster &raster)
{
    GDALDriver *driver =
            GetGDALDriverManager()->GetDriverByName("GTiff");

    if(!driver)
    {
        qDebug() << "GTiff Driver Not Found.";
        return false;
    }

    GDALDataset *dataset =
            driver->Create(
                file.toStdString().c_str(),
                raster.width,
                raster.height,
                1,
                GDT_Float32,
                nullptr);

    if(!dataset)
    {
        qDebug() << "Unable to create output raster.";
        return false;
    }

    dataset->SetGeoTransform(
                const_cast<double*>(raster.geoTransform));

    dataset->SetProjection(
                raster.projection.toStdString().c_str());

    GDALRasterBand *band =
            dataset->GetRasterBand(1);

    band->SetNoDataValue(raster.noData);

    band->RasterIO(
                GF_Write,
                0,
                0,
                raster.width,
                raster.height,
                const_cast<float*>(raster.pixels.data()),
                raster.width,
                raster.height,
                GDT_Float32,
                0,
                0);

    GDALClose(dataset);

    return true;
}


float es04processor::getPixel(const Raster &raster,
                              int row,
                              int col)
{
    if(row < 0)
        row = 0;

    if(col < 0)
        col = 0;

    if(row >= raster.height)
        row = raster.height - 1;

    if(col >= raster.width)
        col = raster.width - 1;

    return raster.pixels[row * raster.width + col];
}


es04processor::Raster
es04processor::applyEnhancedLeeFilter(const Raster &input,
                                      int kernelSize,
                                      int numberOfLooks,
                                      float dampingFactor)
{/*
    Q_UNUSED(kernelSize)
    Q_UNUSED(numberOfLooks)
    Q_UNUSED(dampingFactor)

    // Fixed parameters
    kernelSize = 3;
    numberOfLooks = 2;
    dampingFactor = 1.0f;*/

    Raster output = input;

    const int width  = input.width;
    const int height = input.height;

    const std::vector<float> &src = input.pixels;
    std::vector<float> &dst = output.pixels;

    const int radius = kernelSize / 2;

    const double Cu =
            1.0 / std::sqrt((double)numberOfLooks);

    const double Cmax =
            std::sqrt(1.0 +
                      (2.0 / (double)numberOfLooks));

    ThreadManager::parallelFor(
                height,
                [&](int startRow, int endRow)
    {
        for(int row = startRow; row < endRow; row++)
        {
            for (int col = 0; col < width; col++)
            {
                //-----------------------------------------
                // Local Mean & Variance
                //-----------------------------------------

                double sum = 0.0;
                double sumSquare = 0.0;
                int count = 0;

                int yStart = std::max(0, row - radius);
                int yEnd   = std::min(height - 1, row + radius);

                int xStart = std::max(0, col - radius);
                int xEnd   = std::min(width - 1, col + radius);

                for (int y = yStart; y <= yEnd; y++)
                {
                    for (int x = xStart; x <= xEnd; x++)
                    {
                        float pixel = src[y * width + x];

                        sum += pixel;
                        sumSquare += pixel * pixel;

                        count++;
                    }
                }

                if (count == 0)
                {
                    dst[row * width + col] =
                            src[row * width + col];
                    continue;
                }

                double mean = sum / count;

                double variance =
                        (sumSquare / count) -
                        (mean * mean);

                if (variance < 0.0)
                    variance = 0.0;

                float centerPixel =
                        src[row * width + col];

                if (mean <= 0.0)
                {
                    dst[row * width + col] =
                            centerPixel;
                    continue;
                }

                //-----------------------------------------
                // Coefficient of Variation
                //-----------------------------------------

                double Ci =
                        variance / mean;

                double filteredPixel;

                //-----------------------------------------
                // Enhanced Lee
                //-----------------------------------------

                if (Ci <= Cu)
                {
                    filteredPixel = mean;
                }
                else if (Ci >= Cmax)
                {
                    filteredPixel = centerPixel;
                }
                else
                {
                    double weight =
                            std::exp(
                                -dampingFactor *
                                ((Ci - Cu) /
                                 (Cmax - Ci)));

                    filteredPixel =
                            mean * weight +
                            centerPixel * (1.0 - weight);
                }

                dst[row * width + col] =
                        static_cast<float>(filteredPixel);
            }
        }
    });

    return output;
}

bool es04processor::applyEnhancedLee(const QString &inputFile,
                                     const QString &outputFile)
{
    qDebug() << "Filtering :" << inputFile;

    //--------------------------------------------------
    // Read Input Raster
    //--------------------------------------------------

    Raster inputRaster =
            readRaster(inputFile);

    if(inputRaster.pixels.empty())
    {
        qDebug() << "Unable to read raster.";

        return false;
    }

    //--------------------------------------------------
    // Apply Enhanced Lee Filter
    //--------------------------------------------------

    Raster outputRaster =
            applyEnhancedLeeFilter(
                inputRaster,
                5,
                2,
                1.0f);     // Damping Factor

    //--------------------------------------------------
    // Write Output Raster
    //--------------------------------------------------

    if(!writeRaster(outputFile,
                    outputRaster))
    {
        qDebug() << "Unable to write filtered raster.";

        return false;
    }

    qDebug() << "Enhanced Lee Output :" << outputFile;

    return true;
}



bool es04processor::enhancedLeeFilter()
{
    qDebug() << "================================";
    qDebug() << "Enhanced Lee Filtering Started";
    qDebug() << "================================";

    mEnhancedHHFile =
            QDir(mFilterFolder).filePath(
                getProductName() +
                "_HH_EnhancedLee.tif");

    mEnhancedHVFile =
            QDir(mFilterFolder).filePath(
                getProductName() +
                "_HV_EnhancedLee.tif");

    QElapsedTimer timer;
    timer.start();

    bool status = ThreadManager::runParallel(

                [this]()
    {
        //        return applyEnhancedLee(
        //                    mResampledHHFile,
        //                    mEnhancedHHFile);

        return applyEnhancedLee(
                    mHHFile,
                    mEnhancedHHFile);
    },

    [this]()
    {
        //        return applyEnhancedLee(
        //                    mResampledHVFile,
        //                    mEnhancedHVFile);

        return applyEnhancedLee(
                    mHVFile,
                    mEnhancedHVFile);
    }

    );

    qDebug() << "Enhanced Lee Time =" << timer.elapsed() << "ms";

    if(!status)
    {
        qDebug() << "Enhanced Lee Filtering Failed.";
        return false;
    }

    qDebug() << "================================";
    qDebug() << "Enhanced Lee Filtering Completed";
    qDebug() << "================================";

    return true;
}
