#ifndef ES04PROCESSOR_H
#define ES04PROCESSOR_H

#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcess>
#include <QDebug>
#include <QObject>

#include <vector>
#include <cmath>

#include "gdal_priv.h"
#include "gdal_utils.h"
#include "cpl_conv.h"

#include "database/rastermanager.h"
#include "Common/processorbase.h"

class es04processor : public ProcessorBase
{
    Q_OBJECT
public:

    explicit es04processor(QObject *parent = nullptr);

    bool process(const QString &zipFile,
                 const QString &destinationFolder);

    bool setZipFile(const QString &zipFile);

    QString getRootFolder() const;


signals:
    void progressChanged(int value,
                         const QString &message);

    void processingFinished(const QString &preProcessedFolder);

private:

    bool extractZip(const QString &destinationFolder);

    bool findRootFolder();

    bool findBandMeta();

    bool readCalibrationConstants();

    QString extractValue(const QString &line);

    QString getProductName();

    bool createPreProcessedFolder();

    bool extractRequiredFiles();

    QString findFile(const QString &keyword);

    bool copyAndRename(const QString &sourceFile,
                       const QString &suffix);

    bool enhancedLeeFilter();

    bool applyEnhancedLee(const QString &inputFile,
                          const QString &outputFile);

    bool gammaCalibration();

    bool calibrateGamma(const QString &inputFile,
                        const QString &outputFile,
                        double calibrationConstant);

    bool betaCalibration();

    bool calibrateBeta(const QString &gammaFile,
                       const QString &areaFile,
                       const QString &outputFile);


    bool sigmaCalibration();

    bool calibrateSigma(const QString &betaFile,
                        const QString &liaFile,
                        const QString &outputFile);

private:

    //----------------------------------------------------
    // Raster Structure
    //----------------------------------------------------

    struct Raster
    {
        int width = 0;

        int height = 0;

        std::vector<float> pixels;

        double geoTransform[6];

        QString projection;

        float noData = 0.0f;
    };

    Raster readRaster(const QString &file);

    bool writeRaster(const QString &file,
                     const Raster &raster);

    float getPixel(const Raster &raster,
                   int row,
                   int col);

    Raster applyEnhancedLeeFilter(const Raster &input,
                                  int kernelSize,
                                  int numberOfLooks,
                                  float dampingFactor);

private:

    //----------------------------------------------------
    // Product Information
    //----------------------------------------------------

    struct ProductInfo
    {
        QString productName;

        QString otsProductId;

        QString polarization1;

        QString polarization2;
    };

    ProductInfo mProduct;

    //----------------------------------------------------
    // Calibration Constants
    //----------------------------------------------------

    struct CalibrationConstants
    {
        double sigmaHH = 0.0;

        double sigmaHV = 0.0;

        double betaHH = 0.0;

        double betaHV = 0.0;

        double gammaHH = 0.0;

        double gammaHV = 0.0;
    };

    CalibrationConstants mCalibration;

private:

    //----------------------------------------------------
    // Paths
    //----------------------------------------------------

    QString mWorkingFolder;

    QString mZipFile;

    QString mExtractFolder;

    QString mRootFolder;

    QString mBandMetaFile;

    QString mPreProcessedFolder;

    RasterManager mRasterManager;

    //----------------------------------------------------
    // Original Files
    //----------------------------------------------------

    QString mHHFile;

    QString mHVFile;

    QString mAreaFile;

    QString mLIAFile;

    //----------------------------------------------------
    // Filtered Files
    //----------------------------------------------------

    QString mEnhancedHHFile;

    QString mEnhancedHVFile;


    //----------------------------------------------------
    // Gamma Files
    //----------------------------------------------------

    QString mGammaHHFile;

    QString mGammaHVFile;

    //----------------------------------------------------
    // Beta Files
    //----------------------------------------------------

    QString mBetaHHFile;

    QString mBetaHVFile;

    //----------------------------------------------------
    // Sigma Files
    //----------------------------------------------------

    QString mSigmaHHFile;

    QString mSigmaHVFile;


    //----------------------------------------------------
    // Particular Folder
    //----------------------------------------------------

    QString mFilterFolder;

    QString mCalibrationFolder;

    QString mRenamedFolder;

};
#endif
