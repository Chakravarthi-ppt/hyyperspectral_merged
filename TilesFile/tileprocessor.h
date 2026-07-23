#ifndef TILEPROCESSOR_H
#define TILEPROCESSOR_H

#include <QString>
#include <opencv2/opencv.hpp>
#include <gdal_priv.h>

#include <QMap>
#include <QImage>

class TileProcessor
{
public:

    TileProcessor();
    ~TileProcessor();

    //-----------------------------
    // File Handling
    //-----------------------------

    static const int TILE_SIZE;

    bool openInput(const QString &fileName);

    bool createOutput(const QString &fileName,
                      int bands,
                      GDALDataType dataType);

    bool readRGBTile(int xOffset,
                     int yOffset,
                     int tileWidth,
                     int tileHeight,
                     QImage &image);

    QImage getTile(int tileX,
                   int tileY,
                   int zoom);

    QImage getWrappedTile(int tileX,
                          int tileY,
                          int zoom);

    void close();

    //-----------------------------
    // Tile Operations
    //-----------------------------

    bool readTile(int xOffset,
                  int yOffset,
                  int tileWidth,
                  int tileHeight,
                  int bandNumber,
                  cv::Mat &tile);

    bool writeTile(int xOffset,
                   int yOffset,
                   int bandNumber,
                   const cv::Mat &tile);

    //-----------------------------
    // Raster Information
    //-----------------------------

    int rows() const;
    int cols() const;
    int bandCount() const;

    QString projection() const;

    double *geoTransform();

public:

    void setRGBBands(int red,
                     int green,
                     int blue);

private:

    GDALDataset *mInputDataset;
    GDALDataset *mOutputDataset;

    int mRows;
    int mCols;
    int mBandCount;

    double mGeoTransform[6];

    QString mProjection;

     QMap<QString, QImage> mTileCache;

private:

    int mRedBand = 1;
    int mGreenBand = 2;
    int mBlueBand = 3;
};

#endif // TILEPROCESSOR_H
