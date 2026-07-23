#ifndef RASTERMANAGER_H
#define RASTERMANAGER_H

#include <QString>
#include <QImage>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <gdal_priv.h>


struct RasterInfo
{
    QString rasterName;

    int width;
    int height;

    int bandCount;

    QString projection;

    double originX;
    double originY;

    double pixelSizeX;
    double pixelSizeY;

    QString driver;
    QString importedAt;
};

struct BandMetadata
{
    QString rasterName;
    int bandNumber;
    QString dataType;
    double minValue;
    double maxValue;
    QString noDataValue;
    QString colorInterp;
    int blockWidth;
    int blockHeight;
};


class RasterManager
{
public:
    RasterManager();

    QImage loadRaster();

    QImage loadRaster(const QString &path);

    QString getRasterPath(const QString &imageName);

    QStringList getRasterList();

    QList<BandMetadata> getBandMetadata(
            const QString &rasterName);

    RasterInfo getRasterInfo(
            const QString &rasterName);

    bool deleteRaster(const QString &rasterName);

    bool insertRasterPath(const QString &path);

    bool rasterPathExists(const QString &path);

private:
    bool connectDatabase();

    QString getRasterPath();

    QImage readRaster(const QString& path);

    //QImage loadRaster(const QString &path);

    bool metadataExists(const QString &rasterName);

    bool storeBandMetadata(
            GDALDataset *dataset,
            const QString &rasterName);

    bool storeRasterInfo(
               GDALDataset *dataset,
               const QString &rasterName);

    bool rasterInfoExists(
               const QString &rasterName);
};

#endif // RASTERMANAGER_H
