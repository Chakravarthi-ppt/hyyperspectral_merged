//#include "layerproperties.h"
//#include "ui_layerproperties.h"

//#include <QFileInfo>
//#include <QMessageBox>
//#include <QDebug>

//#include "gdal_priv.h"

//#include "database/rastermanager.h"

//LayerProperties::LayerProperties(const QString &filePath,
//                                 QWidget *parent)
//    : QDialog(parent),
//      ui(new Ui::LayerProperties)
//{
//    ui->setupUi(this);

//    GDALAllRegister();

//    GDALDataset *ds =
//            (GDALDataset *)GDALOpen(filePath.toStdString().c_str(),
//                                    GA_ReadOnly);

//    if(ds == nullptr)
//    {
//        QMessageBox::warning(this,
//                             "Layer Properties",
//                             "Unable to open dataset.");
//        return;
//    }


//    QFileInfo info(filePath);

//    qDebug() << "Raster Name =" << info.fileName();

//    RasterManager rasterManager;

//    RasterInfo rasterInfo =
//            rasterManager.getRasterInfo(info.fileName());

//    qDebug() << "Width from DB =" << rasterInfo.width;
//    qDebug() << "Height from DB =" << rasterInfo.height;
//    qDebug() << "Bands from DB =" << rasterInfo.bandCount;



//    qDebug() << "Projection:";
//    qDebug() << ds->GetProjectionRef();

//    double gt[6];
//    CPLErr err = ds->GetGeoTransform(gt);

//    qDebug() << "GeoTransform Status:" << err;

//    if(err == CE_None)
//    {
//        qDebug() << gt[0]
//                 << gt[1]
//                 << gt[2]
//                 << gt[3]
//                 << gt[4]
//                 << gt[5];
//    }

//    //-----------------------------------
//    // Layer Name
//    //-----------------------------------

//    ui->layerNameLineEdit->setText(info.fileName());

//    //-----------------------------------
//    // Source
//    //-----------------------------------

//    ui->sourceLineEdit->setText(filePath);

//    //-----------------------------------
//    // Width
//    //-----------------------------------

//    ui->widthLineEdit->setText(
//        QString::number(rasterInfo.width));


//    //-----------------------------------
//    // Height
//    //-----------------------------------

//    ui->heightLineEdit->setText(
//        QString::number(rasterInfo.height));

//    //-----------------------------------
//    // Bands
//    //-----------------------------------

//    ui->bandsLineEdit->setText(
//        QString::number(rasterInfo.bandCount));

//    //-----------------------------------
//    // Data Type
//    //-----------------------------------

//    GDALRasterBand *band = ds->GetRasterBand(1);

//    if(band)
//    {
//        ui->datatypeLineEdit->setText(
//                    GDALGetDataTypeName(
//                        band->GetRasterDataType()));
//    }

//    //-----------------------------------
//    // Resolution & Extent
//    //-----------------------------------

//    if(ds->GetGeoTransform(gt) != CE_None)
//    {
//        // world.tif has no georeference.
//        // Assume global WGS84.

//        gt[0] = -180.0;                                // Min Longitude
//        gt[1] = 360.0 / ds->GetRasterXSize();          // Pixel Width
//        gt[2] = 0.0;

//        gt[3] = 90.0;                                 // Max Latitude
//        gt[4] = 0.0;
//        gt[5] = -180.0 / ds->GetRasterYSize();         // Pixel Height
//    }

//    ui->resolutionLineEdit->setText(
//                QString("%1 x %2")
//                .arg(gt[1],0,'f',6)
//                .arg(fabs(gt[5]),0,'f',6));

//    double minX = gt[0];
//    double maxY = gt[3];

//    double maxX = gt[0] + ds->GetRasterXSize() * gt[1];
//    double minY = gt[3] + ds->GetRasterYSize() * gt[5];

//    QString extent;

//    extent += QString("Min X : %1\n").arg(minX);
//    extent += QString("Max X : %1\n").arg(maxX);
//    extent += QString("Min Y : %1\n").arg(minY);
//    extent += QString("Max Y : %1").arg(maxY);

//    ui->extentTextEdit->setPlainText(extent);

//    //-----------------------------------
//    // Projection
//    //-----------------------------------

//    QString projection = ds->GetProjectionRef();

//    if(projection.isEmpty())
//    {
//        projection = "WGS84";
//    }

//    ui->projectionTextEdit->setPlainText(projection);

//    GDALClose(ds);
//}

//LayerProperties::~LayerProperties()
//{
//    delete ui;
//}


#include "layerproperties.h"
#include "ui_layerproperties.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QDebug>

#include "gdal_priv.h"

#include "database/rastermanager.h"

LayerProperties::LayerProperties(const QString &filePath,
                                 QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LayerProperties)
{
    ui->setupUi(this);

    GDALAllRegister();

    GDALDataset *ds =
            (GDALDataset *)GDALOpen(filePath.toStdString().c_str(),
                                    GA_ReadOnly);

    if(ds == nullptr)
    {
        QMessageBox::warning(this,
                             "Layer Properties",
                             "Unable to open dataset.");
        return;
    }


    QFileInfo info(filePath);

    qDebug() << "Raster Name =" << info.fileName();

    int rasterWidth  = ds->GetRasterXSize();
    int rasterHeight = ds->GetRasterYSize();
    int rasterBands  = ds->GetRasterCount();

    qDebug() << "Width from GDAL =" << rasterWidth;
    qDebug() << "Height from GDAL =" << rasterHeight;
    qDebug() << "Bands from GDAL =" << rasterBands;

    qDebug() << "Projection:";
    qDebug() << ds->GetProjectionRef();

    double gt[6];
    CPLErr err = ds->GetGeoTransform(gt);

    qDebug() << "GeoTransform Status:" << err;

    if(err == CE_None)
    {
        qDebug() << gt[0]
                 << gt[1]
                 << gt[2]
                 << gt[3]
                 << gt[4]
                 << gt[5];
    }

    //-----------------------------------
    // Layer Name
    //-----------------------------------

    ui->layerNameLineEdit->setText(info.fileName());

    //-----------------------------------
    // Source
    //-----------------------------------

    ui->sourceLineEdit->setText(filePath);

    //-----------------------------------
    // Width
    //-----------------------------------

    ui->widthLineEdit->setText(
        QString::number(rasterWidth));


    //-----------------------------------
    // Height
    //-----------------------------------

    ui->heightLineEdit->setText(
        QString::number(rasterHeight));

    //-----------------------------------
    // Bands
    //-----------------------------------

    ui->bandsLineEdit->setText(
        QString::number(rasterBands));

    //-----------------------------------
    // Data Type
    //-----------------------------------

    GDALRasterBand *band = ds->GetRasterBand(1);

    if(band)
    {
        ui->datatypeLineEdit->setText(
                    GDALGetDataTypeName(
                        band->GetRasterDataType()));
    }

    //-----------------------------------
    // Resolution & Extent
    //-----------------------------------

    if(ds->GetGeoTransform(gt) != CE_None)
    {
        // world.tif has no georeference.
        // Assume global WGS84.

        gt[0] = -180.0;                                // Min Longitude
        gt[1] = 360.0 / rasterWidth;                   // Pixel Width
        gt[2] = 0.0;

        gt[3] = 90.0;                                 // Max Latitude
        gt[4] = 0.0;
        gt[5] = -180.0 / rasterHeight;                 // Pixel Height
    }

    const QString deg = QString::fromUtf8("\xC2\xB0");

    ui->resolutionLineEdit->setText(
                QString("%1%3 x %2%3")
                .arg(gt[1],0,'f',6)
                .arg(fabs(gt[5]),0,'f',6)
                .arg(deg));

    double minX = gt[0];
    double maxY = gt[3];

    double maxX = gt[0] + rasterWidth * gt[1];
    double minY = gt[3] + rasterHeight * gt[5];

    QString extent;

    extent += QString("Min X : %1%2\n").arg(minX).arg(deg);
    extent += QString("Max X : %1%2\n").arg(maxX).arg(deg);
    extent += QString("Min Y : %1%2\n").arg(minY).arg(deg);
    extent += QString("Max Y : %1%2").arg(maxY).arg(deg);

    ui->extentTextEdit->setPlainText(extent);

    //-----------------------------------
    // Projection
    //-----------------------------------

    QString projection = ds->GetProjectionRef();

    if(projection.isEmpty())
    {
        projection = "WGS84";
    }

    ui->projectionTextEdit->setPlainText(projection);

    GDALClose(ds);
}

LayerProperties::~LayerProperties()
{
    delete ui;
}
