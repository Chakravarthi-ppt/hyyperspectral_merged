

////////////////////////modified/////////////////////


#ifndef CO_BANDSTACK_H
#define CO_BANDSTACK_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

#include <gdal.h>
#include <ogr_spatialref.h>

namespace Ui {
class co_bandstack;
}

// ---------------------------------------------------------------------------
// Structure to hold pipeline results for reporting
// ---------------------------------------------------------------------------
struct PipelineResult
{
    bool success = false;
    QString errorMessage;
    double rmsError = 0.0;
    int numGCPs = 0;
    QString transformation;
    QString interpolation;
    QString sarOutputPath;      // path to SAR output (Float32)
    QString opticalOutputPath;  // path to optical output (UInt16)
   QString stackedOutputPath;
    int numBandsStacked = 0;    // total bands (for info)
};

// ---------------------------------------------------------------------------
// Main widget class
// ---------------------------------------------------------------------------
class co_bandstack : public QWidget
{
    Q_OBJECT

public:
    explicit co_bandstack(QWidget *parent = nullptr);
    ~co_bandstack();

private slots:
    // UI button callbacks
    void on_master_clicked();
    void on_slave_clicked();
    void on_reset_clicked();
    void on_cancel_clicked();
    void on_Run_clicked();


private:
    // Core pipeline
    PipelineResult runPipeline(const QStringList &masterBands,
                               const QStringList &slaveBands,
                               const QString &outPath);

    // Helper functions
    void reportProgress(int percent, const QString &message);

    // Raster I/O and info
    bool readRasterInfo(const QString &path, QString &projWkt, double gt[6],
                        int &xSize, int &ySize, QString &errMsg);
    bool sameCRS(const QString &wktA, const QString &wktB);
    void pixelSize(const double gt[6], double &xRes, double &yRes);

    // Pre‑processing
    bool reprojectToMatch(const QString &srcPath, const QString &dstSRS,
                          const QString &outPath, QString &errMsg);
    bool resampleToResolution(const QString &srcPath, double xRes, double yRes,
                              const QString &outPath, QString &errMsg);

    // GCP generation with downsampling (maxDim = max image dimension for feature detection)
    bool findGCPs(const QString &masterPath, const QString &slavePath,
                  const double masterGT[6], const double slaveGT[6],
                  QVector<GDAL_GCP> &gcps, double &rms, QString &errMsg,
                  int maxDim = 800);

    // Warping
    bool warpBandWithGCPs(const QString &srcPath, const QVector<GDAL_GCP> &gcps,
                          const QString &dstSRSWkt, int xSize, int ySize,
                          const double masterGT[6], const QString &outPath,
                          QString &errMsg);

    // Write cropped bands with specified data type (Float32 or UInt16)
    bool writeCroppedBands(const std::vector<std::vector<float>> &bandData,
                           const double gt[6], const QString &proj,
                           const QString &outPath,
                           GDALDataType dataType,
                           double noDataValue,
                           int cropXSize, int cropYSize,
                           QString &errMsg);

private:
    Ui::co_bandstack *ui;

    // User‑selected file lists
    QStringList m_masterBandPaths;
    QStringList m_slaveBandPaths;
};

#endif // CO_BANDSTACK_H


