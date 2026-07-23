#include "tileprocessor.h"

#include <algorithm>
#include <QDebug>

const int TileProcessor::TILE_SIZE = 512;

TileProcessor::TileProcessor()
{
    GDALAllRegister();

    mInputDataset = nullptr;
    mOutputDataset = nullptr;

    mRows = 0;
    mCols = 0;
    mBandCount = 0;

    for(int i = 0; i < 6; i++)
        mGeoTransform[i] = 0.0;
}

TileProcessor::~TileProcessor()
{
    close();
}

bool TileProcessor::openInput(const QString &fileName)
{
    close();


    mInputDataset =
            (GDALDataset *)GDALOpen(
                fileName.toStdString().c_str(),
                GA_ReadOnly);

    if(!mInputDataset)
    {
        qDebug() << "Unable to open input:"
                 << fileName;

        return false;
    }
    mTileCache.clear();

    mCols = mInputDataset->GetRasterXSize();
    mRows = mInputDataset->GetRasterYSize();
    mBandCount = mInputDataset->GetRasterCount();

    if(mBandCount >= 3)
    {
        mRedBand   = 1;
        mGreenBand = 2;
        mBlueBand  = 3;
    }
    else
    {
        mRedBand   = 1;
        mGreenBand = 1;
        mBlueBand  = 1;
    }

    mInputDataset->GetGeoTransform(mGeoTransform);

    const char *projection =
            mInputDataset->GetProjectionRef();


    if(projection)
        mProjection = QString::fromUtf8(projection);

    qDebug() << "Input Image Loaded";
    qDebug() << "Rows :" << mRows;
    qDebug() << "Cols :" << mCols;
    qDebug() << "Bands:" << mBandCount;

    qDebug() << "Opened Raster :" << fileName;

    return true;


}

bool TileProcessor::createOutput(const QString &fileName,
                                 int bands,
                                 GDALDataType dataType)
{
    GDALDriver *driver =
            GetGDALDriverManager()->GetDriverByName("GTiff");

    if(!driver)
    {
        qDebug() << "GTiff Driver Not Found";
        return false;
    }

    mOutputDataset =
            driver->Create(fileName.toStdString().c_str(),
                           mCols,
                           mRows,
                           bands,
                           dataType,
                           nullptr);

    if(!mOutputDataset)
    {
        qDebug() << "Unable to create output";

        return false;
    }

    mOutputDataset->SetGeoTransform(mGeoTransform);

    mOutputDataset->SetProjection(
                mProjection.toStdString().c_str());

    qDebug() << "Output Created";


    return true;
}

bool TileProcessor::readTile(int xOffset,
                             int yOffset,
                             int tileWidth,
                             int tileHeight,
                             int bandNumber,
                             cv::Mat &tile)
{
    if(!mInputDataset)
        return false;

    tileWidth =
            std::min(tileWidth,
                     mCols - xOffset);

    tileHeight =
            std::min(tileHeight,
                     mRows - yOffset);

    tile =
            cv::Mat(tileHeight,
                    tileWidth,
                    CV_32FC1);

    GDALRasterBand *band =
            mInputDataset->GetRasterBand(bandNumber);

    if(!band)
        return false;

    CPLErr error =
            band->RasterIO(GF_Read,
                           xOffset,
                           yOffset,
                           tileWidth,
                           tileHeight,
                           tile.data,
                           tileWidth,
                           tileHeight,
                           GDT_Float32,
                           0,
                           0);

    return error == CE_None;


}

bool TileProcessor::writeTile(int xOffset,
                              int yOffset,
                              int bandNumber,
                              const cv::Mat &tile)
{
    if(!mOutputDataset)
        return false;

    GDALRasterBand *band =
            mOutputDataset->GetRasterBand(bandNumber);

    if(!band)
        return false;

    CPLErr error =
            band->RasterIO(GF_Write,
                           xOffset,
                           yOffset,
                           tile.cols,
                           tile.rows,
                           (void *)tile.data,
                           tile.cols,
                           tile.rows,
                           GDT_Float32,
                           0,
                           0);

    return error == CE_None;
}

int TileProcessor::rows() const
{
    return mRows;
}

int TileProcessor::cols() const
{
    return mCols;
}

int TileProcessor::bandCount() const
{
    return mBandCount;
}

QString TileProcessor::projection() const
{
    return mProjection;
}

double *TileProcessor::geoTransform()
{
    return mGeoTransform;
}

void TileProcessor::close()
{
    if(mInputDataset)
    {
        GDALClose(mInputDataset);
        mInputDataset = nullptr;
    }

    if(mOutputDataset)
    {
        GDALClose(mOutputDataset);
        mOutputDataset = nullptr;
    }
}

bool TileProcessor::readRGBTile(int xOffset,
                                int yOffset,
                                int outputWidth,
                                int outputHeight,
                                QImage &image)
{
    if (!mInputDataset)
        return false;

    // Validate offsets
    if(xOffset < 0 || yOffset < 0 ||
       xOffset >= mCols || yOffset >= mRows)
        return false;

    // Calculate actual dimensions (clamp to image bounds)
    int width = std::min(outputWidth, mCols - xOffset);
    int height = std::min(outputHeight, mRows - yOffset);

    if(width <= 0 || height <= 0)
        return false;

    image = QImage(width,
                   height,
                   QImage::Format_RGB888);

    if(image.isNull())
        return false;

//    int bandMap[3] = {1, 2, 3};

    int bandMap[3] =
    {
        mRedBand,
        mGreenBand,
        mBlueBand
    };

    CPLErr err =
            mInputDataset->RasterIO(
                GF_Read,
                xOffset,      // Start reading from xOffset
                yOffset,      // Start reading from yOffset
                width,        // Read width pixels
                height,       // Read height pixels
                image.bits(),
                width,        // Output width
                height,       // Output height
                GDT_Byte,
                3,
                bandMap,
                3,
                image.bytesPerLine(),
                1);

    return err == CE_None;
}


QImage TileProcessor::getTile(int tileX,
                              int tileY,
                              int zoom)
{
    Q_UNUSED(zoom)

    if(!mInputDataset)
        return QImage();

    QString key =
            QString("%1_%2")
            .arg(tileX)
            .arg(tileY);

    if(mTileCache.contains(key))
        return mTileCache.value(key);

    int xOffset = tileX * TILE_SIZE;
    int yOffset = tileY * TILE_SIZE;

    if(xOffset < 0 ||
       yOffset < 0 ||
       xOffset >= mCols ||
       yOffset >= mRows)
    {
        return QImage();
    }

    int width =
            std::min(TILE_SIZE,
                     mCols - xOffset);

    int height =
            std::min(TILE_SIZE,
                     mRows - yOffset);

    std::vector<float> red(width * height);
    std::vector<float> green(width * height);
    std::vector<float> blue(width * height);

    GDALRasterBand *redBand =
            mInputDataset->GetRasterBand(mRedBand);

    GDALRasterBand *greenBand =
            mInputDataset->GetRasterBand(mGreenBand);

    GDALRasterBand *blueBand =
            mInputDataset->GetRasterBand(mBlueBand);

//    qDebug() << "Current Raster =" << mInputDataset->GetDescription();

    if(!redBand || !greenBand || !blueBand)
        return QImage();

//    qDebug() << "Bands :" << mBandCount;
//    qDebug() << "RGB :" << mRedBand
//             << mGreenBand
//             << mBlueBand;

    qDebug() << redBand;
    qDebug() << greenBand;
    qDebug() << blueBand;

    if(redBand->RasterIO(GF_Read,
                         xOffset,
                         yOffset,
                         width,
                         height,
                         red.data(),
                         width,
                         height,
                         GDT_Float32,
                         0,
                         0) != CE_None)
        return QImage();

    if(greenBand->RasterIO(GF_Read,
                           xOffset,
                           yOffset,
                           width,
                           height,
                           green.data(),
                           width,
                           height,
                           GDT_Float32,
                           0,
                           0) != CE_None)
        return QImage();

    if(blueBand->RasterIO(GF_Read,
                          xOffset,
                          yOffset,
                          width,
                          height,
                          blue.data(),
                          width,
                          height,
                          GDT_Float32,
                          0,
                          0) != CE_None)
        return QImage();

    double mmR[2];
    double mmG[2];
    double mmB[2];

    redBand->ComputeRasterMinMax(TRUE, mmR);
    greenBand->ComputeRasterMinMax(TRUE, mmG);
    blueBand->ComputeRasterMinMax(TRUE, mmB);

    double minR = mmR[0];
    double maxR = mmR[1];

    double minG = mmG[0];
    double maxG = mmG[1];

    double minB = mmB[0];
    double maxB = mmB[1];

    if(maxR == minR) maxR = minR + 1;
    if(maxG == minG) maxG = minG + 1;
    if(maxB == minB) maxB = minB + 1;

    QImage image(width,
                 height,
                 QImage::Format_RGB888);

    for(int y=0;y<height;y++)
    {
        uchar *scan = image.scanLine(y);

        for(int x=0;x<width;x++)
        {
            int i = y * width + x;

            int r = int((red[i]-minR)*255.0/(maxR-minR));
            int g = int((green[i]-minG)*255.0/(maxG-minG));
            int b = int((blue[i]-minB)*255.0/(maxB-minB));

            scan[x*3+0] = qBound(0,r,255);
            scan[x*3+1] = qBound(0,g,255);
            scan[x*3+2] = qBound(0,b,255);
        }
    }

    mTileCache.insert(key,image);

    return image;
}


QImage TileProcessor::getWrappedTile(int tileX,
                                     int tileY,
                                     int zoom)
{
    Q_UNUSED(zoom)

    int totalTilesX =
            (mCols + TILE_SIZE - 1) / TILE_SIZE;

    int totalTilesY =
            (mRows + TILE_SIZE - 1) / TILE_SIZE;

    //---------------------------------------
    // Wrap only X
    //---------------------------------------

    tileX =
            ((tileX % totalTilesX)
             + totalTilesX)
            % totalTilesX;

    //---------------------------------------
    // Clamp Y
    //---------------------------------------

    if(tileY < 0)
        return QImage();

    if(tileY >= totalTilesY)
        return QImage();

    return getTile(tileX,
                   tileY,
                   0);
}

void TileProcessor::setRGBBands(int red,
                                int green,
                                int blue)
{
    mRedBand = red;
    mGreenBand = green;
    mBlueBand = blue;

    mTileCache.clear();     // Important!
}
