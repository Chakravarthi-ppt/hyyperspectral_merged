//#ifndef LAYERMANAGER_H
//#define LAYERMANAGER_H

//#include <QObject>
//#include <QTreeWidget>
//#include <QGraphicsScene>
//#include <QGraphicsPixmapItem>
//#include "MapCanvas/mapcanvas.h"
//#include "Layer/rgbbanddialog.h"
//#include <QFileSystemWatcher>

//struct RasterLayer
//{
//    QString filePath;
//    QString layerName;

//    bool visible = true;

//    QGraphicsPixmapItem *pixmapItem = nullptr;
//    QTreeWidgetItem *treeItem = nullptr;
//};

//class LayerManager : public QObject
//{
//    Q_OBJECT

//public:
//    explicit LayerManager(QObject *parent = nullptr);
//    void initialize(MapCanvas *canvas,
//                    QTreeWidget *tree);
//    //    void addLayer(const QString &layerName);

//    void addLayer(const QString &filePath,
//                  QTreeWidgetItem *parentItem = nullptr);

//    void onLayerItemChanged(QTreeWidgetItem *item,
//                            int column);
//    void removeLayer();
//    void showLayerProperties();
//    void showLayerContextMenu(const QPoint &pos);

//    void zoomToLayer();

//    void setCurrentFile(const QString &filePath);

//    bool addRasterLayer(const QString &filePath);

//    void loadFolder(const QString &folderPath);

//    void clearLayers();

//    void watchProject(const QString &folderPath);
//    void clearLoadedPixmaps();

//signals:

//    void rgbBandSelected(const QString &filePath,
//                         int redBand,
//                         int greenBand,
//                         int blueBand,
//                         bool stretch);


//private:

//    QList<RasterLayer*> mLayers;
//    MapCanvas *mCanvas = nullptr;
//    QTreeWidget *mTreeWidget = nullptr;

//    LayerManager *mLayerManager = nullptr;

//    QString mCurrentFile;
//    QFileSystemWatcher *mWatcher = nullptr;

//    QString mProjectFolder;


//private slots:

//    void onProjectFolderChanged(const QString &path);



//};

//#endif


#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include <QObject>
#include <QTreeWidget>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "MapCanvas/mapcanvas.h"
#include "Layer/rgbbanddialog.h"
#include <QFileSystemWatcher>

struct RasterLayer
{
    QString filePath;
    QString layerName;

    bool visible = true;

    QGraphicsPixmapItem *pixmapItem = nullptr;
    QTreeWidgetItem *treeItem = nullptr;
};

class LayerManager : public QObject
{
    Q_OBJECT

public:
    explicit LayerManager(QObject *parent = nullptr);
    void initialize(MapCanvas *canvas,
                    QTreeWidget *tree);
    //    void addLayer(const QString &layerName);

    void addLayer(const QString &filePath,
                  QTreeWidgetItem *parentItem = nullptr);

    void onLayerItemChanged(QTreeWidgetItem *item,
                            int column);
    void removeLayer();
    void showLayerProperties();
    void showLayerContextMenu(const QPoint &pos);

    void zoomToLayer();

    void setCurrentFile(const QString &filePath);

    bool addRasterLayer(const QString &filePath);

    // Convenience wrapper for "add a freshly-generated result file and make
    // it appear on the map right now" (e.g. Hyperspectral module outputs).
    // addLayer() alone only registers the file and leaves it unchecked/
    // invisible, same as any manual import -- this also ticks it, draws it
    // (addRasterLayer), and zooms the canvas to its extent, same as if the
    // user had imported it and double-clicked it themselves.
    void addAndDisplayLayer(const QString &filePath);

    void loadFolder(const QString &folderPath);

    void clearLayers();

    void watchProject(const QString &folderPath);
    void clearLoadedPixmaps();

signals:

    void rgbBandSelected(const QString &filePath,
                         int redBand,
                         int greenBand,
                         int blueBand,
                         bool stretch);


private:

    QList<RasterLayer*> mLayers;
    MapCanvas *mCanvas = nullptr;
    QTreeWidget *mTreeWidget = nullptr;

    LayerManager *mLayerManager = nullptr;

    QString mCurrentFile;
    QFileSystemWatcher *mWatcher = nullptr;

    QString mProjectFolder;


private slots:

    void onProjectFolderChanged(const QString &path);



};

#endif
