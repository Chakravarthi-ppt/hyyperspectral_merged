//#ifndef MAINWINDOW_H
//#define MAINWINDOW_H

//#include <QMainWindow>
//#include <QGraphicsScene>
//#include <QGraphicsPixmapItem>
//#include <QMenu>
//#include <QTreeWidgetItem>
//#include <QDockWidget>

//#include "Layer/layermanager.h"

//#include "MapCanvas/mapcanvas.h"
//#include "Processing/E0S-04/es04processor.h"
//#include "ui_IHSFusion.h"
//#include "Fusion/IHSFusion.h"
//#include "../UI/co_bandstack/co_bandstack.h"


//class StatusBarWidget;
//class QTreeWidgetItem;
//class HyperspectralPanel;


//QT_BEGIN_NAMESPACE
//namespace Ui {
//class MainWindow;
//}
//QT_END_NAMESPACE

//class IHSFusion ;

//class MainWindow : public QMainWindow
//{
//    Q_OBJECT

//public:
//    explicit MainWindow(QWidget *parent = nullptr);
//    ~MainWindow();

//protected:

//    void keyPressEvent(QKeyEvent *event) override;

//    QString convertToUTM(
//            double lon,
//            double lat);

//private slots:

//    void updateGeoCoordinate(const QPointF &geoPos);

//    void updateMouseCoordinate(const QPointF &scenePos);

//    void onOpenClicked();

//    void onRotationChanged(double angle);

//    void onScaleChanged(const QString &scale);

//    void updateScale(double scale);

//    void updateRotation(double angle);

//    void updateExtent(const QRectF &rect);

//    void on_btnOpenImport_clicked();

//    void onOpenCoregistration();

//    void on_btnIHSFusion_clicked();

//    void onOpenHyperspectral();

//private:
//    QDockWidget       *hyperspectralDock = nullptr;
//    HyperspectralPanel *hyperspectralPanel = nullptr;

//private:
//    //-------------------------------
//    // Initialization
//    //-------------------------------

//    void initializeUI();
//    void initializeGraphicsView();
//    void initializeStatusBar();
//    void initializeLayerDock();
//    void connectSignals();

//    //-------------------------------
//    // Basemap
//    //-------------------------------

//    bool loadBaseMap();

//    //-------------------------------
//    // Layer
//    //-------------------------------

//    void addLayer(const QString &layerName);
//    void clearLayers();

//    void onProcessingFinished(const QString &folder);

//private:

//    Ui::MainWindow *ui;

//    //-------------------------------
//    // Widgets
//    //-------------------------------

//    StatusBarWidget *mStatusBarWidget = nullptr;

//    //-------------------------------
//    // Raster Information
//    //-------------------------------

//    double mGeoTransform[6] = {0};

//    int mRasterWidth = 0;
//    int mRasterHeight = 0;

//    int mPreviewWidth = 0;
//    int mPreviewHeight = 0;

//    //-------------------------------
//    // View Information
//    //-------------------------------

//    double mCurrentScale = 1.0;
//    double mMinScale = 1.0;

//    QString mCurrentFile;  // Add this line

//    LayerManager *mLayerManager = nullptr;

//    es04processor *mES04Processor;

////    MapCanvas *mapcanvas;
//};

//#endif // MAINWINDOW_H


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMenu>
#include <QTreeWidgetItem>
#include <QDockWidget>

#include "Layer/layermanager.h"

#include "MapCanvas/mapcanvas.h"
#include "Processing/E0S-04/es04processor.h"
#include "ui_IHSFusion.h"
#include "Fusion/IHSFusion.h"
#include "../UI/co_bandstack/co_bandstack.h"


class StatusBarWidget;
class QTreeWidgetItem;
class HyperspectralPanel;


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class IHSFusion ;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:

    void keyPressEvent(QKeyEvent *event) override;

    QString convertToUTM(
            double lon,
            double lat);

private slots:

    void updateGeoCoordinate(const QPointF &geoPos);

    void updateMouseCoordinate(const QPointF &scenePos);

    void onOpenClicked();

    void onRotationChanged(double angle);

    void onScaleChanged(const QString &scale);

    void updateScale(double scale);

    void updateRotation(double angle);

    void updateExtent(const QRectF &rect);

    void on_btnOpenImport_clicked();

    void onOpenCoregistration();

    void on_btnIHSFusion_clicked();

    void onOpenHyperspectral();

public:
    // Called by the Hyperspectral module (via HsiMainWindow::publishToMap)
    // to drop a finished, georeferenced result raster (GeoTIFF) onto the
    // real map canvas, at whatever extent GDAL reads from the file itself.
    void addRasterLayerToMap(const QString &filePath);

private:
    QDockWidget       *hyperspectralDock = nullptr;
    HyperspectralPanel *hyperspectralPanel = nullptr;

private:
    //-------------------------------
    // Initialization
    //-------------------------------

    void initializeUI();
    void initializeGraphicsView();
    void initializeStatusBar();
    void initializeLayerDock();
    void connectSignals();

    //-------------------------------
    // Basemap
    //-------------------------------

    bool loadBaseMap();

    //-------------------------------
    // Layer
    //-------------------------------

    void addLayer(const QString &layerName);
    void clearLayers();

    void onProcessingFinished(const QString &folder);

private:

    Ui::MainWindow *ui;

    //-------------------------------
    // Widgets
    //-------------------------------

    StatusBarWidget *mStatusBarWidget = nullptr;

    //-------------------------------
    // Raster Information
    //-------------------------------

    double mGeoTransform[6] = {0};

    int mRasterWidth = 0;
    int mRasterHeight = 0;

    int mPreviewWidth = 0;
    int mPreviewHeight = 0;

    //-------------------------------
    // View Information
    //-------------------------------

    double mCurrentScale = 1.0;
    double mMinScale = 1.0;

    QString mCurrentFile;  // Add this line

    LayerManager *mLayerManager = nullptr;

    es04processor *mES04Processor;

//    MapCanvas *mapcanvas;
};

#endif // MAINWINDOW_H
