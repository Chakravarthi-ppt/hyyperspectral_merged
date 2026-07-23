#ifndef SENTINEL2PROCESSOR_H
#define SENTINEL2PROCESSOR_H

#include <QString>
#include <QObject>

#include "Common/processorbase.h"

class sentinel2processor : public ProcessorBase
{
    Q_OBJECT

public:

    explicit sentinel2processor(QObject *parent = nullptr);

    bool process(const QString &zipFile,
                 const QString &workingFolder);

signals:

    void progressChanged(int progress,
                         const QString &message);

private:

    //----------------------------------------------------
    // STEP-1
    //----------------------------------------------------

    QString mZipFile;

    QString mWorkingFolder;

    bool extractZip();

    QString mExtractFolder;

    //----------------------------------------------------
    // STEP-2
    //----------------------------------------------------

    bool findSafeFolder();

    QString mSafeFolder;

    //----------------------------------------------------
    // STEP-3
    //----------------------------------------------------

    bool findR10mFolder();

    QString mR10mFolder;

    //----------------------------------------------------
    // STEP-4
    //----------------------------------------------------

    bool findBands();

    QString findBand(const QString &bandName);

    QString mB02File;
    QString mB03File;
    QString mB04File;
    QString mB08File;

    //----------------------------------------------------
    // STEP-5
    //----------------------------------------------------

    bool createOpticalFolder();

    QString mPreProcessedFolder;

    QString mOpticalFolder;

    // JP2 Input Files
    QString mBand02JP2;
    QString mBand03JP2;
    QString mBand04JP2;
    QString mBand08JP2;

    // TIFF Output Files
    QString mBand02TIF;
    QString mBand03TIF;
    QString mBand04TIF;
    QString mBand08TIF;

    bool convertBandsToTiff();

    bool convertJP2ToTiff(const QString &inputJP2,
                          const QString &outputTIF);
};

#endif
