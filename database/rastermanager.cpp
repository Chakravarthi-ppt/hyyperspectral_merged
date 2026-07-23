#include "rastermanager.h"

#include <QDebug>

#include <vector>

#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>

RasterManager::RasterManager()
{
    GDALAllRegister();
}

bool RasterManager::connectDatabase()
{
    QSqlDatabase db;

    if(QSqlDatabase::contains("RasterConnection"))
    {
        db = QSqlDatabase::database("RasterConnection");

        if(db.isOpen())
            return true;
    }
    else
    {
        db = QSqlDatabase::addDatabase(
                    "QPSQL",
                    "RasterConnection");

        db.setHostName("localhost");
        db.setPort(5432);
        db.setDatabaseName("postgres");
        db.setUserName("postgres");
        db.setPassword("postgres");
    }

    if(!db.open())
    {
        qDebug() << db.lastError().text();
        return false;
    }

    qDebug() << "Database Connected";

    return true;
}

QString RasterManager::getRasterPath()
{
    QSqlQuery query(QSqlDatabase::database("RasterConnection"));

    if(!query.exec(
                "SELECT image_path "
                "FROM raster_files "
                "LIMIT 1"))
    {
        qDebug()<<query.lastError().text();
        return "";
    }

    if(query.next())
        return query.value(0).toString();

    return "";
}

QImage RasterManager::loadRaster()
{
    if(!connectDatabase())
        return QImage();

    QString path=getRasterPath();

    return readRaster(path);
}

QImage RasterManager::readRaster(const QString &path)
{
    GDALDataset *dataset =
        (GDALDataset*)GDALOpen(
            path.toStdString().c_str(),
            GA_ReadOnly);

    if(!dataset)
    {
        qDebug() << "Cannot Open Raster";
        return QImage();
    }

    QFileInfo fileInfo(path);

    QString rasterName = fileInfo.fileName();

    //-----------------------------------------
    // Store Raster Information
    //-----------------------------------------
    if(!rasterInfoExists(rasterName))
    {
        qDebug() << "Raster information not found.";

        storeRasterInfo(
            dataset,
            rasterName);
    }
    else
    {
        qDebug() << "Raster information already exists.";
    }

    //-----------------------------------------
    // Store Band Metadata
    //-----------------------------------------
    if(!metadataExists(rasterName))
    {
        qDebug() << "Band metadata not found.";

        storeBandMetadata(
            dataset,
            rasterName);
    }
    else
    {
        qDebug() << "Band metadata already exists.";
    }

    //-----------------------------------------
    // Read Raster Preview
    //-----------------------------------------

    GDALRasterBand *band =
        dataset->GetRasterBand(1);

    int width =
        dataset->GetRasterXSize();

    int height =
        dataset->GetRasterYSize();

    qDebug() << "Raster Size:"
             << width
             << height;

    int previewWidth = 1024;
    int previewHeight = 1024;

    std::vector<uint16_t> buffer(
        previewWidth * previewHeight);

    CPLErr err =
        band->RasterIO(
            GF_Read,
            0,
            0,
            width,
            height,
            buffer.data(),
            previewWidth,
            previewHeight,
            GDT_UInt16,
            0,
            0);

    if(err != CE_None)
    {
        qDebug() << "RasterIO failed";

        GDALClose(dataset);

        return QImage();
    }

    uint16_t minVal = buffer[0];
    uint16_t maxVal = buffer[0];

    for(auto value : buffer)
    {
        if(value < minVal)
            minVal = value;

        if(value > maxVal)
            maxVal = value;
    }

    QImage image(
        previewWidth,
        previewHeight,
        QImage::Format_Grayscale8);

    for(int y = 0; y < previewHeight; y++)
    {
        uchar *line = image.scanLine(y);

        for(int x = 0; x < previewWidth; x++)
        {
            int index =
                y * previewWidth + x;

            float pixel =
                float(buffer[index] - minVal) /
                float(maxVal - minVal + 1);

            line[x] =
                static_cast<uchar>(
                    pixel * 255);
        }
    }

    qDebug() << "Preview Image:"
             << image.width()
             << image.height();

    GDALClose(dataset);

    return image;
}

bool RasterManager::storeBandMetadata(
        GDALDataset *dataset,
        const QString &rasterName)
{
    int bandCount = dataset->GetRasterCount();

    qDebug() << "Raster :" << rasterName;
    qDebug() << "Bands  :" << bandCount;


    for(int i = 1; i <= bandCount; i++)
    {
        GDALRasterBand *band =
                dataset->GetRasterBand(i);

        GDALDataType type =
                band->GetRasterDataType();

        int blockX;
        int blockY;

        band->GetBlockSize(
                    &blockX,
                    &blockY);

        int hasMin = 0;
        int hasMax = 0;

        double minValue =
                band->GetMinimum(&hasMin);

        double maxValue =
                band->GetMaximum(&hasMax);

        if(!hasMin || !hasMax)
        {
            double adfMinMax[2];

            GDALComputeRasterMinMax(
                        band,
                        TRUE,
                        adfMinMax);

            minValue = adfMinMax[0];
            maxValue = adfMinMax[1];

            hasMin = 1;
            hasMax = 1;
        }

        int hasNoData = 0;

        double noData =
                band->GetNoDataValue(&hasNoData);

        qDebug() << "----------------------";
        qDebug() << "Band :" << i;
        qDebug() << "Data Type :"
                 << GDALGetDataTypeName(type);

        if(hasMin)
            qDebug() << "Minimum :" << minValue;
        else
            qDebug() << "Minimum : Not Available";

        if(hasMax)
            qDebug() << "Maximum :" << maxValue;
        else
            qDebug() << "Maximum : Not Available";

        if(hasNoData)
            qDebug() << "NoData :" << noData;
        else
            qDebug() << "NoData : None";

        qDebug() << "Color Interpretation :"
                 << GDALGetColorInterpretationName(
                        band->GetColorInterpretation());

        qDebug() << "Block Size :"
                 << blockX
                 << "x"
                 << blockY;

        QSqlQuery query(
            QSqlDatabase::database("RasterConnection"));

        query.prepare(
            "INSERT INTO raster_band_metadata ("
            "raster_name,"
            "band_number,"
            "data_type,"
            "min_value,"
            "max_value,"
            "nodata_value,"
            "color_interp,"
            "block_width,"
            "block_height)"
            "VALUES("
            ":raster_name,"
            ":band_number,"
            ":data_type,"
            ":min_value,"
            ":max_value,"
            ":nodata_value,"
            ":color_interp,"
            ":block_width,"
            ":block_height)");

        query.bindValue(
            ":raster_name",
            rasterName);

        query.bindValue(
            ":band_number",
            i);

        query.bindValue(
            ":data_type",
            QString(GDALGetDataTypeName(type)));

        query.bindValue(
            ":min_value",
            minValue);

        query.bindValue(
            ":max_value",
            maxValue);

        if(hasNoData)
        {
            query.bindValue(
                ":nodata_value",
                noData);
        }
        else
        {
            query.bindValue(
                ":nodata_value",
                QVariant(QVariant::Double));
        }

        query.bindValue(
            ":color_interp",
            QString(
                GDALGetColorInterpretationName(
                    band->GetColorInterpretation())));

        query.bindValue(
            ":block_width",
            blockX);

        query.bindValue(
            ":block_height",
            blockY);

        if(!query.exec())
        {
            qDebug() << "Insert Failed:"
                     << query.lastError().text();
        }
        else
        {
            qDebug() << "Metadata inserted successfully.";
        }
    }

    return true;
}

QList<BandMetadata> RasterManager::getBandMetadata(
        const QString &rasterName)
{

    qDebug() << "Fetching BandMetadata for:" << rasterName;
    QList<BandMetadata> metadataList;

    QSqlQuery query(QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT "
        "raster_name,"
        "band_number,"
        "data_type,"
        "min_value,"
        "max_value,"
        "nodata_value,"
        "color_interp,"
        "block_width,"
        "block_height "
        "FROM raster_band_metadata "
        "WHERE raster_name = :name "
        "ORDER BY band_number");

    query.bindValue(
            ":name",
            rasterName);

    if(!query.exec())
    {
        qDebug() << query.lastError().text();
        return metadataList;
    }

    while(query.next())
    {
        BandMetadata meta;

        meta.rasterName = query.value(0).toString();
        meta.bandNumber = query.value(1).toInt();
        meta.dataType = query.value(2).toString();
        meta.minValue = query.value(3).toDouble();
        meta.maxValue = query.value(4).toDouble();

        if(query.value(5).isNull())
            meta.noDataValue = "None";
        else
            meta.noDataValue = query.value(5).toString();

        meta.colorInterp = query.value(6).toString();
        meta.blockWidth = query.value(7).toInt();
        meta.blockHeight = query.value(8).toInt();

        metadataList.append(meta);
    }

    return metadataList;
}


bool RasterManager::metadataExists(const QString &rasterName)
{
    QSqlQuery query(QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT COUNT(*) "
        "FROM raster_band_metadata "
        "WHERE raster_name = :name");

    query.bindValue(":name", rasterName);

    if(!query.exec())
    {
        qDebug() << query.lastError().text();
        return false;
    }

    query.next();

    int count = query.value(0).toInt();

    return count > 0;
}

bool RasterManager::rasterInfoExists(
        const QString &rasterName)
{
    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT COUNT(*) "
        "FROM raster_info "
        "WHERE raster_name = :name");

    query.bindValue(":name", rasterName);

    if(!query.exec())
    {
        qDebug() << query.lastError().text();
        return false;
    }

    query.next();

    return query.value(0).toInt() > 0;
}

bool RasterManager::storeRasterInfo(
        GDALDataset *dataset,
        const QString &rasterName)
{
    if(!dataset)
        return false;

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();
    int bandCount = dataset->GetRasterCount();

    // Projection
    QString projection =
            QString(dataset->GetProjectionRef());

    qDebug() << "Projection Length :" << projection.length();

    qDebug() << "Projection Preview :";

    qDebug() << projection.left(100);

    // GeoTransform
    double geoTransform[6];

    if(dataset->GetGeoTransform(geoTransform) != CE_None)
    {
        qDebug() << "GeoTransform not available.";

        geoTransform[0] = 0.0;
        geoTransform[1] = 0.0;
        geoTransform[2] = 0.0;
        geoTransform[3] = 0.0;
        geoTransform[4] = 0.0;
        geoTransform[5] = 0.0;
    }

    double originX = geoTransform[0];
    double originY = geoTransform[3];

    double pixelSizeX = geoTransform[1];
    double pixelSizeY = geoTransform[5];

    // Driver
    QString driver =
            dataset->GetDriver()->GetDescription();

    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "INSERT INTO raster_info ("
        "raster_name,"
        "width,"
        "height,"
        "band_count,"
        "projection,"
        "origin_x,"
        "origin_y,"
        "pixel_size_x,"
        "pixel_size_y,"
        "driver)"
        " VALUES ("
        ":raster_name,"
        ":width,"
        ":height,"
        ":band_count,"
        ":projection,"
        ":origin_x,"
        ":origin_y,"
        ":pixel_size_x,"
        ":pixel_size_y,"
        ":driver)");

    query.bindValue(":raster_name", rasterName);
    query.bindValue(":width", width);
    query.bindValue(":height", height);
    query.bindValue(":band_count", bandCount);
    query.bindValue(":projection", projection);
    query.bindValue(":origin_x", originX);
    query.bindValue(":origin_y", originY);
    query.bindValue(":pixel_size_x", pixelSizeX);
    query.bindValue(":pixel_size_y", pixelSizeY);
    query.bindValue(":driver", driver);

    if(!query.exec())
    {
        qDebug() << "Raster Info Insert Failed:"
                 << query.lastError().text();
        return false;
    }

    qDebug() << "Raster information inserted successfully.";

    return true;
}

RasterInfo RasterManager::getRasterInfo(
        const QString &rasterName)
{
    qDebug() << "Fetching RasterInfo for:" << rasterName;

    RasterInfo info{};

    if(!connectDatabase())
    {
        qDebug() << "Database connection failed.";
        return info;
    }

    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT "
        "raster_name,"
        "width,"
        "height,"
        "band_count,"
        "projection,"
        "origin_x,"
        "origin_y,"
        "pixel_size_x,"
        "pixel_size_y,"
        "driver,"
        "imported_at "
        "FROM raster_info "
        "WHERE raster_name = :name");

    query.bindValue(
            ":name",
            rasterName);

    if(!query.exec())
    {
        qDebug() << "SQL Error:";
        qDebug() << query.lastError().text();
        qDebug() << query.lastQuery();
    }
    else if(!query.next())
    {
        qDebug() << "No record found for" << rasterName;
    }
    else
    {
        info.rasterName = query.value(0).toString();
        info.width = query.value(1).toInt();
        info.height = query.value(2).toInt();
        info.bandCount = query.value(3).toInt();
        info.projection = query.value(4).toString();
        info.originX = query.value(5).toDouble();
        info.originY = query.value(6).toDouble();
        info.pixelSizeX = query.value(7).toDouble();
        info.pixelSizeY = query.value(8).toDouble();
        info.driver = query.value(9).toString();
        info.importedAt = query.value(10).toString();

        qDebug() << "Raster Name :" << info.rasterName;
        qDebug() << "Driver      :" << info.driver;
        qDebug() << "Projection  :" << info.projection;
        qDebug() << "Imported At :" << info.importedAt;
    }

    return info;
}

bool RasterManager::insertRasterPath(const QString &path)
{
    if(!connectDatabase())
        return false;

    QFileInfo fileInfo(path);

   QString imageName = fileInfo.fileName();

    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "INSERT INTO raster_files ("
        "image_name,"
        "image_path)"
        " VALUES("
        ":image_name,"
        ":image_path)");

    query.bindValue(
        ":image_name",
        imageName);

    query.bindValue(
        ":image_path",
        path);

    if(!query.exec())
    {
        qDebug() << "Failed to insert raster path:"
                 << query.lastError().text();
        return false;
    }

    qDebug() << "Raster path inserted successfully.";

    return true;
}

bool RasterManager::rasterPathExists(const QString &path)
{
    if(!connectDatabase())
        return false;

    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT COUNT(*) "
        "FROM raster_files "
        "WHERE image_path = :path");

    query.bindValue(":path", path);

    if(!query.exec())
    {
        qDebug() << query.lastError().text();
        return false;
    }

    query.next();

    return query.value(0).toInt() > 0;
}

QImage RasterManager::loadRaster(const QString &path)
{
    if(!connectDatabase())
        return QImage();

    return readRaster(path);
}

QStringList RasterManager::getRasterList()
{
    QStringList rasterList;

    if(!connectDatabase())
        return rasterList;

    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT image_name "
        "FROM raster_files "
        "ORDER BY id");

    if(!query.exec())
    {
        qDebug() << query.lastError().text();
        return rasterList;
    }

    while(query.next())
    {
        rasterList.append(
                    query.value(0).toString());
    }

    return rasterList;
}

QString RasterManager::getRasterPath(
        const QString &imageName)
{
    if(!connectDatabase())
        return "";

    QSqlQuery query(
        QSqlDatabase::database("RasterConnection"));

    query.prepare(
        "SELECT image_path "
        "FROM raster_files "
        "WHERE image_name = :name");

    query.bindValue(":name", imageName);

    if(!query.exec())
    {
        qDebug() << query.lastError().text();
        return "";
    }

    if(query.next())
    {
        return query.value(0).toString();
    }

    return "";
}

//bool RasterManager::deleteRaster(const QString &rasterName)
//{
//    if(!connectDatabase())
//        return false;

//    QSqlDatabase db =
//            QSqlDatabase::database("RasterConnection");

//    db.transaction();

//    QSqlQuery query(db);

//    query.prepare(
//        "DELETE FROM raster_band_metadata "
//        "WHERE raster_name = :name");

//    query.bindValue(":name", rasterName);

//    if(!query.exec())
//    {
//        db.rollback();
//        return false;
//    }

//    query.prepare(
//        "DELETE FROM raster_info "
//        "WHERE raster_name = :name");

//    query.bindValue(":name", rasterName);

//    if(!query.exec())
//    {
//        db.rollback();
//        return false;
//    }

//    query.prepare(
//        "DELETE FROM raster_files "
//        "WHERE image_name = :name");

//    query.bindValue(":name", rasterName);

//    if(!query.exec())
//    {
//        db.rollback();
//        return false;
//    }

//    db.commit();

//    qDebug() << "Deleted :" << rasterName;

//    return true;
//}
bool RasterManager::deleteRaster(const QString &rasterName)
{
    if(!connectDatabase())
        return false;

    QSqlDatabase db =
            QSqlDatabase::database("RasterConnection");

    QSqlQuery query(db);

    db.transaction();

    // Delete Band Metadata
    query.prepare(
        "DELETE FROM raster_band_metadata "
        "WHERE raster_name=:name");

    query.bindValue(":name", rasterName);

    if(!query.exec())
    {
        db.rollback();
        qDebug()<<query.lastError();
        return false;
    }

    // Delete Raster Info
    query.prepare(
        "DELETE FROM raster_info "
        "WHERE raster_name=:name");

    query.bindValue(":name", rasterName);

    if(!query.exec())
    {
        db.rollback();
        qDebug()<<query.lastError();
        return false;
    }

    // Delete Raster Path
    query.prepare(
        "DELETE FROM raster_files "
        "WHERE image_name=:name");

    query.bindValue(":name", rasterName);

    if(!query.exec())
    {
        db.rollback();
        qDebug()<<query.lastError();
        return false;
    }

    db.commit();

    qDebug()<<"Raster Deleted Successfully.";

    return true;
}
