#include "layermanager.h"

#include "Layer/layerproperties.h"
#include <QMenu>
#include <QFileInfo>
#include <QDebug>

#include <QDir>
#include <QFileInfoList>
#include <QDirIterator>
#include <QMessageBox>

#include "database/rastermanager.h"

LayerManager::LayerManager(QObject *parent)
    : QObject(parent)
{
    mWatcher = new QFileSystemWatcher(this);

    connect(mWatcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            &LayerManager::onProjectFolderChanged);

}

void LayerManager::initialize(MapCanvas *canvas,
                              QTreeWidget *tree)
{
    mCanvas = canvas;
    mTreeWidget = tree;

    qDebug() << "Tree =" << mTreeWidget;
}

bool LayerManager::addRasterLayer(const QString &filePath)
{
    GDALAllRegister();

    GDALDataset *ds =
            (GDALDataset*)GDALOpen(
                filePath.toStdString().c_str(),
                GA_ReadOnly);

    if(!ds)
        return false;

    int width = ds->GetRasterXSize();
    int height = ds->GetRasterYSize();

    int bandCount = ds->GetRasterCount();

    if(width <= 0 || height <= 0)
    {
        GDALClose(ds);
        return false;
    }

    QImage displayImage;

    const int DISPLAY_SIZE = 8192;

    int outWidth =
            std::min(width, DISPLAY_SIZE);

    int outHeight =
            std::min(height, DISPLAY_SIZE);

    if(bandCount >= 3)
    {
        GDALRasterBand *redBand   = ds->GetRasterBand(1);
        GDALRasterBand *greenBand = ds->GetRasterBand(2);
        GDALRasterBand *blueBand  = ds->GetRasterBand(3);

        std::vector<float> red(outWidth*outHeight);
        std::vector<float> green(outWidth*outHeight);
        std::vector<float> blue(outWidth*outHeight);

        //----------------------------------------
        // Get NoData values
        //----------------------------------------

        int hasNoDataR = FALSE;
        int hasNoDataG = FALSE;
        int hasNoDataB = FALSE;

        double noDataR = redBand->GetNoDataValue(&hasNoDataR);
        double noDataG = greenBand->GetNoDataValue(&hasNoDataG);
        double noDataB = blueBand->GetNoDataValue(&hasNoDataB);

        qDebug() << "Red NoData   :" << noDataR << hasNoDataR;
        qDebug() << "Green NoData :" << noDataG << hasNoDataG;
        qDebug() << "Blue NoData  :" << noDataB << hasNoDataB;

        if(redBand->RasterIO(GF_Read,0,0,width,height,
                             red.data(),outWidth,outHeight,
                             GDT_Float32,0,0))
        {
            GDALClose(ds);
            return false;
        }

        if(greenBand->RasterIO(GF_Read,0,0,width,height,
                               green.data(),outWidth,outHeight,
                               GDT_Float32,0,0))
        {
            GDALClose(ds);
            return false;
        }


        if(blueBand->RasterIO(GF_Read,0,0,width,height,
                              blue.data(),outWidth,outHeight,
                              GDT_Float32,0,0))
        {
            GDALClose(ds);
            return false;
        }

        double mmR[2], mmG[2], mmB[2];

        double min,max;

        if(redBand->ComputeStatistics(FALSE,
                                      &min,
                                      &max,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      nullptr) == CE_None)
        {
            mmR[0]=min;
            mmR[1]=max;
        }

        if(greenBand->GetStatistics(TRUE,TRUE,&min,&max,nullptr,nullptr)== CE_None)
        {
        mmG[0]=min;
        mmG[1]=max;
        }

        if(blueBand->GetStatistics(TRUE,TRUE,&min,&max,nullptr,nullptr)== CE_None)
        {
        mmB[0]=min;
        mmB[1]=max;
        }

        displayImage = QImage(outWidth,
                              outHeight,
                              QImage::Format_ARGB32);

//        int noDataPixels = 0;
        for(int y=0;y<outHeight;y++)
        {
//            uchar *line = displayImage.scanLine(y);

            QRgb *line = reinterpret_cast<QRgb *>(displayImage.scanLine(y));


            for(int x=0;x<outWidth;x++)
            {
                int i = y * outWidth + x;

                const float eps = 1e-6f;

//                if(hasNoDataR &&
//                        hasNoDataG &&
//                        hasNoDataB &&
//                        std::fabs(red[i]   - noDataR) < eps &&
//                        std::fabs(green[i] - noDataG) < eps &&
//                        std::fabs(blue[i]  - noDataB) < eps)
//                {
//                    line[x*3+0]=0;
//                    line[x*3+1]=0;
//                    line[x*3+2]=0;

//                     noDataPixels++;
//                    continue;

//                }

                if(hasNoDataR &&
                   hasNoDataG &&
                   hasNoDataB &&
                   fabs(red[i]   - noDataR) < eps &&
                   fabs(green[i] - noDataG) < eps &&
                   fabs(blue[i]  - noDataB) < eps)
                {
                    line[x] = qRgba(0, 0, 0, 0);   // Transparent
                    continue;
                }

//                qDebug() << "NoData Pixels =" << noDataPixels;
                double rangeR = mmR[1]-mmR[0];
                double rangeG = mmG[1]-mmG[0];
                double rangeB = mmB[1]-mmB[0];

                if(rangeR <= 0) rangeR = 1;
                if(rangeG <= 0) rangeG = 1;
                if(rangeB <= 0) rangeB = 1;

                int r = qBound(0,
                               int((red[i]-mmR[0]) * 255.0 / rangeR),
                               255);

                int g = qBound(0,
                               int((green[i]-mmG[0]) * 255.0 / rangeG),
                               255);

                int b = qBound(0,
                               int((blue[i]-mmB[0]) * 255.0 / rangeB),
                               255);

                line[x] = qRgba(r, g, b, 255);
            }
        }
    }
    else
    {
        GDALRasterBand *band = ds->GetRasterBand(1);

        int hasNoData = FALSE;
        double noData = band->GetNoDataValue(&hasNoData);

        std::vector<float> raster(outWidth*outHeight);

        band->RasterIO(GF_Read,
                       0,
                       0,
                       width,
                       height,
                       raster.data(),
                       outWidth,
                       outHeight,
                       GDT_Float32,
                       0,
                       0);

        double displayMin, displayMax;
        double mean, stddev;

        CPLErr statErr =
                band->GetStatistics(TRUE,
                                    TRUE,
                                    &displayMin,
                                    &displayMax,
                                    &mean,
                                    &stddev);

        if(statErr != CE_None)
        {
            band->ComputeStatistics(FALSE,
                                    &displayMin,
                                    &displayMax,
                                    &mean,
                                    &stddev,
                                    nullptr,
                                    nullptr);
        }

        QString name = QFileInfo(filePath).fileName();

        double low;
        double high;

        if(name.contains("Gamma",Qt::CaseInsensitive) ||
                name.contains("Sigma",Qt::CaseInsensitive))
        {
            low = qMax(displayMin,
                       mean-2*stddev);

            high = qMin(displayMax,
                        mean+2*stddev);
        }
        else
        {
            low = mean-3*stddev;
            high = mean+3*stddev;
        }

        if(high<=low)
            high = low+1;

        displayImage =
                QImage(outWidth,
                       outHeight,
                       QImage::Format_Grayscale8);

        for(int y=0;y<outHeight;y++)
        {
            uchar *line = displayImage.scanLine(y);

            for(int x=0;x<outWidth;x++)
            {
                int index = y*outWidth+x;

                float pixel = raster[index];

                if(std::isnan(pixel))
                {
                    line[x]=0;
                    continue;
                }

                if(hasNoData &&
                        std::fabs(pixel-noData)<0.0001f)
                {
                    line[x]=0;
                    continue;
                }

                double value =
                        (pixel-low)/(high-low);

                value = qBound(0.0,
                               value,
                               1.0);

                if(name.contains("Gamma",Qt::CaseInsensitive) ||
                        name.contains("Sigma",Qt::CaseInsensitive))
                {
                    value = std::sqrt(value);
                }

                line[x] =
                        uchar(value*255);
            }
        }
    }

    //-----------------------------------------
    // Create Pixmap
    //-----------------------------------------

    QGraphicsPixmapItem *pixmap =
            new QGraphicsPixmapItem(
                QPixmap::fromImage(displayImage));

    pixmap->setOffset(0, 0);
    pixmap->setTransformationMode(Qt::FastTransformation);
    //-----------------------------------------
    // Read GeoTransform
    //-----------------------------------------

    double gt[6];

    if(ds->GetGeoTransform(gt) != CE_None)
    {
        GDALClose(ds);
        return false;
    }

    qDebug() << "GeoTransform:";
    for(int i=0;i<6;i++)
        qDebug() << "GT[" << i << "] =" << gt[i];

    //-----------------------------------------
    // Pixel -> Map Coordinates
    //-----------------------------------------

    auto pixelToGeo =
            [&](double col,
            double row,
            double &x,
            double &y)
    {
        x = gt[0] + col * gt[1] + row * gt[2];
        y = gt[3] + col * gt[4] + row * gt[5];
    };

    double ulX, ulY;
    double urX, urY;
    double llX, llY;
    double lrX, lrY;

    pixelToGeo(0,0,ulX,ulY);
    pixelToGeo(width,0,urX,urY);
    pixelToGeo(0,height,llX,llY);
    pixelToGeo(width,height,lrX,lrY);

    //-----------------------------------------
    // Transform to WGS84
    //-----------------------------------------

    const char *proj = ds->GetProjectionRef();

    if(proj == nullptr || strlen(proj) == 0)
    {
        qDebug() << "No projection found.";
        GDALClose(ds);
        return false;
    }

    OGRSpatialReference srcSRS;
    srcSRS.importFromWkt(ds->GetProjectionRef());
    srcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference dstSRS;
    dstSRS.importFromEPSG(4326);
    dstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation *transform =
            OGRCreateCoordinateTransformation(
                &srcSRS,
                &dstSRS);

    if(!transform)
    {
        GDALClose(ds);
        return false;
    }

    if(!transform->Transform(1, &ulX, &ulY) ||
            !transform->Transform(1, &urX, &urY) ||
            !transform->Transform(1, &llX, &llY) ||
            !transform->Transform(1, &lrX, &lrY))
    {
        qDebug() << "Coordinate transformation failed.";

        OCTDestroyCoordinateTransformation(transform);
        GDALClose(ds);

        return false;
    }

    OCTDestroyCoordinateTransformation(transform);

    //-----------------------------------------
    // World Coordinates -> Scene Coordinates
    //-----------------------------------------

    double worldGT[6];
    mCanvas->geoTransform(worldGT);

    auto geoToScene =
            [&](double lon,
            double lat,
            double &sx,
            double &sy)
    {
        sx = (lon - worldGT[0]) / worldGT[1];
        sy = (lat - worldGT[3]) / worldGT[5];
    };

    double sx0, sy0;
    double sxX, syX;
    double sxY, syY;

    geoToScene(ulX, ulY, sx0, sy0);
    geoToScene(urX, urY, sxX, syX);
    geoToScene(llX, llY, sxY, syY);

    double dispWidth  = displayImage.width();
    double dispHeight = displayImage.height();

    double dxCol = (sxX - sx0) / dispWidth;
    double dyCol = (syX - sy0) / dispWidth;

    double dxRow = (sxY - sx0) / dispHeight;
    double dyRow = (syY - sy0) / dispHeight;

    qDebug() << "Width :" << width;
    qDebug() << "Height:" << height;

    qDebug() << "Display Width :" << dispWidth;
    qDebug() << "Display Height:" << dispHeight;

    QTransform affine(
                dxCol,
                dyCol,
                dxRow,
                dyRow,
                sx0,
                sy0
                );

    pixmap->setTransform(affine, false);
    pixmap->setZValue(100);

    //-----------------------------------------
    // Add to Scene
    //-----------------------------------------

    for(RasterLayer *layer : mLayers)
    {
        if(layer->filePath == filePath)
        {
            layer->pixmapItem = pixmap;
            break;
        }
    }

    mCanvas->scene()->addItem(pixmap);
    pixmap->update();
    pixmap->setVisible(true);

    qDebug() << "UL :" << ulX << ulY;
    qDebug() << "UR :" << urX << urY;
    qDebug() << "LL :" << llX << llY;

    qDebug() << "Scene UL :" << sx0 << sy0;
    qDebug() << "Scene UR :" << sxX << syX;
    qDebug() << "Scene LL :" << sxY << syY;

    qDebug() << "Display Size :" << dispWidth << dispHeight;
    GDALClose(ds);

    return true;
}


void LayerManager::removeLayer()
{
    QTreeWidgetItem *item = mTreeWidget->currentItem();

    if(!item)
        return;

    for(int i = 0; i < mLayers.size(); ++i)
    {
        RasterLayer *layer = mLayers[i];

        if(layer->treeItem == item)
        {
            if(layer->pixmapItem)
            {
                mCanvas->scene()->removeItem(layer->pixmapItem);
                delete layer->pixmapItem;
            }

            delete layer->treeItem;

            mLayers.removeAt(i);

            delete layer;

            break;
        }
    }
}

void LayerManager::addLayer(const QString &filePath,
                            QTreeWidgetItem *parentItem)
{
    QFileInfo info(filePath);

    qDebug() << "Adding Layer:" << filePath;

    QString layerName = info.fileName();

    //---------------------------------------
    // Check duplicate
    //---------------------------------------

    for(RasterLayer *layer : mLayers)
    {
        if(layer->filePath == filePath)
            return;
    }
    //---------------------------------------
    // Create Raster Layer
    //---------------------------------------

    RasterLayer *layer = new RasterLayer;

    layer->filePath = filePath;
    layer->layerName = layerName;
    layer->visible = false;
    layer->pixmapItem = nullptr;

    //---------------------------------------
    // Create Tree Item
    //---------------------------------------

    QTreeWidgetItem *item;

    if(parentItem)
    {
        item = new QTreeWidgetItem(parentItem);
    }
    else
    {
        item = new QTreeWidgetItem(mTreeWidget);
    }

    item->setText(0, layerName);

    if(layerName.compare("World.tif", Qt::CaseInsensitive) == 0)
    {
        // World map: No checkbox
        item->setFlags(Qt::ItemIsEnabled |
                       Qt::ItemIsSelectable);

        layer->visible = true;
    }
    else
    {
        // Other layers: Show checkbox
        item->setFlags(Qt::ItemIsEnabled |
                       Qt::ItemIsSelectable |
                       Qt::ItemIsUserCheckable);

        layer->visible = false;
        item->setCheckState(0, Qt::Unchecked);
    }

    layer->treeItem = item;

    qDebug() << "Stored Tree Item:"
             << layer->layerName
             << layer->treeItem;

    //Tooltip

    QString fileName = QFileInfo(filePath).fileName();
    item->setText(0, fileName);
    item->setToolTip(0, fileName);

    //---------------------------------------
    // Store Layer
    //---------------------------------------

    mLayers.append(layer);

    mTreeWidget->expandAll();

    qDebug() << "Raster Layers =" << mLayers.size();
    qDebug() << "Top Level Items =" << mTreeWidget->topLevelItemCount();
}

void LayerManager::addAndDisplayLayer(const QString &filePath)
{
    // No-op if filePath is already registered -- addLayer() itself
    // detects the duplicate and returns without touching mLayers/tree.
    addLayer(filePath);

    for (RasterLayer *layer : mLayers)
    {
        if (layer->filePath != filePath)
            continue;

        // zoomToLayer() operates on mTreeWidget->currentItem(), so select
        // this layer first. It then: loads the pixmap via addRasterLayer()
        // if not already loaded, ticks the checkbox, and zooms the canvas
        // to the raster's extent -- exactly what happens today when a
        // user imports a file and double-clicks it in the Layers panel.
        mTreeWidget->setCurrentItem(layer->treeItem);
        zoomToLayer();
        return;
    }

    qWarning() << "addAndDisplayLayer: layer not found after addLayer() for" << filePath;
}

void LayerManager::onLayerItemChanged(QTreeWidgetItem *item,
                                      int column)
{
    qDebug() << "Item Changed :" << item->text(0)
             << item->checkState(0);

    qDebug() << "Clicked Item :" << item;

    if(column != 0)
        return;

    for(RasterLayer *layer : mLayers)
    {
        if(layer->treeItem != item)
            continue;

        bool visible =
                (item->checkState(0) == Qt::Checked);

        layer->visible = visible;

        if(visible)
        {
            if(layer->pixmapItem == nullptr)
            {
                addRasterLayer(layer->filePath);
            }
            else
            {
                layer->pixmapItem->setVisible(true);
            }
        }
        else
        {
            if(layer->pixmapItem)
                layer->pixmapItem->setVisible(false);
        }

        qDebug() << "Checking Layer :"
                 << layer->layerName
                 << layer->treeItem;

        break;

    }
}

void LayerManager::showLayerProperties()
{
    QTreeWidgetItem *item = mTreeWidget->currentItem();

    if(!item)
        return;

    for(RasterLayer *layer : mLayers)
    {
        if(layer->treeItem == item)
        {
            LayerProperties dlg(layer->filePath);
            dlg.exec();
            return;
        }
    }
}

void LayerManager::zoomToLayer()
{
    QTreeWidgetItem *item = mTreeWidget->currentItem();

    if(!item)
        return;

    for(RasterLayer *layer : mLayers)
    {
        if(layer->treeItem != item)
            continue;

        //---------------------------------------
        // Load raster if not already loaded
        //---------------------------------------

        if(layer->pixmapItem == nullptr)
        {
            if(!addRasterLayer(layer->filePath))
            {
                qDebug() << "Unable to load layer:"
                         << layer->filePath;
                return;
            }

            // Keep checkbox checked
            layer->treeItem->setCheckState(0, Qt::Checked);
        }

        if(layer->pixmapItem == nullptr)
            return;

        //---------------------------------------
        // Get raster extent
        //---------------------------------------

        QRectF rect = layer->pixmapItem->sceneBoundingRect();

        qDebug() << "Zooming To :" << layer->layerName;
        qDebug() << "Scene Rect :" << rect;

        //---------------------------------------
        // Zoom to raster
        //---------------------------------------

        mCanvas->animateZoomToRect(rect);

        mCanvas->viewport()->update();

        return;
    }
}

void LayerManager::showLayerContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = mTreeWidget->itemAt(pos);

    if(!item)
        return;

    //---------------------------------------
    // Find Raster Layer
    //---------------------------------------

    RasterLayer *layer = nullptr;

    for(RasterLayer *l : mLayers)
    {
        if(l->treeItem == item)
        {
            layer = l;
            break;
        }
    }

    if(!layer)
        return;

    //---------------------------------------
    // Context Menu
    //---------------------------------------

    QMenu menu;

    QAction *zoomAction = nullptr;
    QAction *removeAction = nullptr;
    QAction *rgbAction = nullptr;

    QAction *propertyAction =
            menu.addAction("Properties");

    QString fileName =
            QFileInfo(layer->filePath).fileName();

    if(fileName.compare("World.tif",
                        Qt::CaseInsensitive) != 0)
    {
        menu.addSeparator();

        zoomAction =
                menu.addAction("Zoom To Layer");

        removeAction =
                menu.addAction("Remove Layer");

        //        menu.addSeparator();

        //        rgbAction = menu.addAction("RGB Band...");
    }

    //---------------------------------------
    // Execute
    //---------------------------------------

    QAction *selected =
            menu.exec(
                mTreeWidget->viewport()->mapToGlobal(pos));

    if(selected == propertyAction)
    {
        showLayerProperties();
    }
    else if(selected == zoomAction)
    {
        zoomToLayer();
    }
    else if(selected == removeAction)
    {
        removeLayer();
    }

    //    else if(selected == rgbAction)
    //    {
    //        GDALDataset *dataset =
    //            (GDALDataset*)GDALOpen(layer->filePath.toStdString().c_str(),
    //                                   GA_ReadOnly);

    //        if(!dataset)
    //            return;

    //        int bandCount = dataset->GetRasterCount();

    //        if(bandCount < 3)
    //        {
    //            QMessageBox::information(
    //                    mTreeWidget,
    //                    "RGB Composite",
    //                    "Selected raster contains less than 3 bands.\n"
    //                    "RGB Composite is not available.");


    //            GDALClose(dataset);
    //            return;
    //        }

    //        RGBBandDialog dlg(mTreeWidget);

    //        dlg.setRasterName(QFileInfo(layer->filePath).fileName());

    //        dlg.setBandCount(bandCount);

    //        GDALClose(dataset);

    //        if(dlg.exec() == QDialog::Accepted)
    //        {
    //            int red   = dlg.redBand();
    //            int green = dlg.greenBand();
    //            int blue  = dlg.blueBand();

    //            qDebug() << "RGB Selected:";
    //            qDebug() << "File :" << layer->filePath;
    //            qDebug() << "Red  :" << red;
    //            qDebug() << "Green:" << green;
    //            qDebug() << "Blue :" << blue;

    //            emit rgbBandSelected(layer->filePath,
    //                                 red,
    //                                 green,
    //                                 blue,
    //                                 dlg.stretchContrast());
    //        }
    //    }

}

void LayerManager::loadFolder(const QString &folderPath)
{

    qDebug() << "Loading Folder :" << folderPath;

    QDir preDir(folderPath);

    if(!preDir.exists())
    {
        qDebug() << "Folder not found";
        return;
    }
    //---------------------------------------
    // Create PreProcessed node
    //---------------------------------------

    QTreeWidgetItem *preItem =
            new QTreeWidgetItem(mTreeWidget);

    preItem->setText(0, "PreProcessed");
    preItem->setFlags(Qt::ItemIsEnabled |
                      Qt::ItemIsSelectable);

    //---------------------------------------
    // Get all subfolders
    //---------------------------------------

    qDebug() << "Folders found:";

    QFileInfoList folders =
            preDir.entryInfoList(
                QDir::Dirs |
                QDir::NoDotAndDotDot);

    for(const QFileInfo &folder : folders)
    {
        qDebug() << folder.fileName();

        // Skip folders we don't want shown in the Layers panel.
        if (folder.fileName().compare("Filter", Qt::CaseInsensitive) == 0 ||
                folder.fileName().compare("Renamed", Qt::CaseInsensitive) == 0 ||
                folder.fileName().compare("Auxillary", Qt::CaseInsensitive) == 0)
        {
            continue;
        }

        QTreeWidgetItem *folderItem =
                new QTreeWidgetItem(preItem);

        // folderItem->setText(0, folder.fileName());

        folderItem->setText(0,
                            folder.fileName());

        folderItem->setFlags(Qt::ItemIsEnabled |
                             Qt::ItemIsSelectable);

        //---------------------------------------
        // Read TIFFs
        //---------------------------------------

        QDir dir(folder.absoluteFilePath());

        QFileInfoList tifFiles =
                dir.entryInfoList(
                    QStringList()
                    << "*.tif"
                    << "*.tiff",
                    QDir::Files,
                    QDir::Name);

        //        for(const QFileInfo &file : tifFiles)
        //        {
        //            QString rasterPath = file.absoluteFilePath();

        //            //---------------------------------------
        //            // Store raster automatically in database
        //            //---------------------------------------

        //            RasterManager rasterManager;

        //            if(!rasterManager.rasterPathExists(rasterPath))
        //            {
        //                rasterManager.insertRasterPath(rasterPath);

        //                // This also stores raster_info and band_metadata
        //                rasterManager.loadRaster(rasterPath);
        //            }


        //            //---------------------------------------
        //            // Add layer to Tree
        //            //---------------------------------------

        //            addLayer(rasterPath, folderItem);
        //        }

        for (const QFileInfo &file : tifFiles)
        {
            QString fileName = file.fileName();

            // Apply filter ONLY for Calibration folder
            if (folder.fileName().compare("Calibration", Qt::CaseInsensitive) == 0)
            {
                if (!(fileName.contains("Sigma_HH", Qt::CaseInsensitive) ||
                      fileName.contains("Sigma_HV", Qt::CaseInsensitive)))
                {
                    continue;
                }
            }

            QString rasterPath = file.absoluteFilePath();

            //---------------------------------------
            // Store raster automatically in database
            //---------------------------------------

            RasterManager rasterManager;

            if (!rasterManager.rasterPathExists(rasterPath))
            {
                rasterManager.insertRasterPath(rasterPath);
                rasterManager.loadRaster(rasterPath);
            }

            //---------------------------------------
            // Add layer to Tree
            //---------------------------------------

            addLayer(rasterPath, folderItem);
        }
    }

    mTreeWidget->expandAll();
    mTreeWidget->update();
}

void LayerManager::clearLayers()
{
    for(RasterLayer *layer : mLayers)
    {
        if(layer->pixmapItem)
        {
            mCanvas->scene()->removeItem(layer->pixmapItem);
            delete layer->pixmapItem;
        }

        delete layer;
    }

    mLayers.clear();

    if(mTreeWidget)
        mTreeWidget->clear();
}


void LayerManager::watchProject(const QString &folderPath)
{
    mProjectFolder = folderPath;

    if(!mWatcher)
        return;

    //----------------------------------------------------
    // Remove old watched folders
    //----------------------------------------------------

    mWatcher->removePaths(mWatcher->directories());

    QDir root(folderPath);

    if(!root.exists())
        return;

    //----------------------------------------------------
    // Watch root folder
    //----------------------------------------------------

    mWatcher->addPath(folderPath);

    qDebug() << "Watching :" << folderPath;

    //----------------------------------------------------
    // Watch all subfolders recursively
    //----------------------------------------------------

    QDirIterator iterator(folderPath,
                          QDir::Dirs |
                          QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while(iterator.hasNext())
    {
        QString folder = iterator.next();

        mWatcher->addPath(folder);

        qDebug() << "Watching :" << folder;
    }
}


void LayerManager::onProjectFolderChanged(const QString &path)
{
    qDebug() << "====================================";
    qDebug() << "Project Changed";
    qDebug() << "Changed Folder :" << path;
    qDebug() << "Refreshing Layers...";
    qDebug() << "====================================";

    //----------------------------------------------------
    // Clear current tree and layers
    //----------------------------------------------------

    clearLayers();

    //----------------------------------------------------
    // Watch again because new folders may have been created
    //----------------------------------------------------

    watchProject(mProjectFolder);

    //----------------------------------------------------
    // Reload Project
    //----------------------------------------------------

    loadFolder(mProjectFolder);

    qDebug() << "Refresh Completed.";
}

void LayerManager::clearLoadedPixmaps()
{
    for(RasterLayer *layer : mLayers)
    {
        layer->pixmapItem = nullptr;
    }
}
