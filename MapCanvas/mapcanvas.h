//#ifndef MAPCANVAS_H
//#define MAPCANVAS_H

//#include <QGraphicsView>
//#include <QGraphicsScene>
//#include <QGraphicsPixmapItem>
//#include <QMouseEvent>
//#include <QWheelEvent>
//#include <QResizeEvent>
////#include <QTimer>
////#include <QElapsedTimer>
//#include <QMap>
//#include <QPair>
//#include <QPropertyAnimation>
////#include <QVariantAnimation>

//#include "TilesFile/tileprocessor.h"
//#include "MapCanvas/northarrow.h"

//class MapCanvas : public QGraphicsView
//{
//    Q_OBJECT

//    Q_PROPERTY(QTransform viewTransform
//               READ transform
//               WRITE setTransform)

//public:
//    explicit MapCanvas(QWidget *parent = nullptr);
//    ~MapCanvas();

//    //----------------------------------------
//    // Base Map
//    //----------------------------------------
//    bool loadBaseMap(const QString &fileName);

//    //----------------------------------------
//    // Navigation
//    //----------------------------------------
//    void zoomIn();
//    void zoomOut();
//    void rotateMap(double angle);
//    void resetNorthUp();

//    void animateToRect(const QRectF &rect);
//    void animateZoomToRect(const QRectF &rect);
//    void animateView(const QRectF &targetRect,
//                     int duration = 500);
//    void stopAnimation();

//    void setLayerVisible(const QString &filePath,
//                         bool visible);

//    void setScalePreset(const QString &scaleText);
//    void setScaleFactor(double scale);

//    //----------------------------------------
//    // Information
//    //----------------------------------------
//    QRectF currentViewExtent() const;

//    double currentScale() const;

//    double rotationAngle() const;

//    void geoTransform(double gt[6]) const;

//    //----------------------------------------
//    // Tile Management
//    //----------------------------------------
//    void updateVisibleTiles();

//    void clearInvisibleTiles();

//signals:
//    void mouseCoordinateChanged(const QPointF &scenePos);

//    void mouseGeoCoordinateChanged(const QPointF &geoPos);

//    void scaleChanged(double scale);

//    void rotationChanged(double angle);

//    void extentChanged(const QRectF &extent);

//    void layerLoaded(const QString &layerName);

//protected:
//    void wheelEvent(QWheelEvent *event) override;

//    void mousePressEvent(QMouseEvent *event) override;

//    void mouseMoveEvent(QMouseEvent *event) override;

//    void mouseReleaseEvent(QMouseEvent *event) override;

//    void resizeEvent(QResizeEvent *event) override;

//private:
//    void updateExtent();

//private:
//    //----------------------------------------
//    // Scene
//    //----------------------------------------

//    QGraphicsScene *mScene = nullptr;

//    QGraphicsPixmapItem *mLeftWorld = nullptr;

//    QGraphicsPixmapItem *mRightWorld = nullptr;

//    //----------------------------------------
//    // Visible Tiles
//    //----------------------------------------

//    QMap<QPair<int,int>, QGraphicsPixmapItem*> mVisibleTiles;

//    //----------------------------------------
//    // Raster Information
//    //----------------------------------------

//    int mRasterWidth = 0;

//    int mRasterHeight = 0;

//    double mGeoTransform[6] = {0};

//    //----------------------------------------
//    // Navigation
//    //----------------------------------------

//    bool mPanning = false;

//    QPoint mLastMousePos;

//    QPointF mViewCenter;

//    QPointF mCurrentCenter;

//    QPointF mDefaultCenter;

//    double mCurrentScale = 1.0;

//    double mDefaultScale = 1.0;

//    double mMinScale = 1.0;

//    double mMaxScale = 20.0;

//    double mRotationAngle = 0.0;

//    //----------------------------------------
//    // World
//    //----------------------------------------

//    int mWorldWidth = 0;

//    int mWorldHeight = 0;

//    int mTotalTilesX = 0;

//    int mTotalTilesY = 0;

//    //----------------------------------------
//    // Base Map
//    //----------------------------------------

//    QString mCurrentFile;

//    bool mBaseMapVisible = true;

//    QTransform mBaseTransform;

//    QTransform mStartTransform;

//    QTransform mEndTransform;

//    //----------------------------------------
//    // Animation
//    //----------------------------------------

//    QPropertyAnimation *mZoomAnimation = nullptr;

//    //----------------------------------------
//    // Navigation Animation
//    //----------------------------------------

//    QRectF mAnimationStartRect;

//    QRectF mAnimationEndRect;

//    QTimer *mAnimationTimer = nullptr;

//    QElapsedTimer mElapsedTimer;

//    int mAnimationDuration = 500;

//    //----------------------------------------
//    // Tile Processor
//    //----------------------------------------

//    TileProcessor mTileProcessor;

//    //----------------------------------------
//    // North Arrow
//    //----------------------------------------

//    NorthArrow *mNorthArrow = nullptr;

//    //---------------------------------------------
//    // Navigation Animation
//    //---------------------------------------------

////    QVariantAnimation *mNavigationAnimation = nullptr;

////   e ebool mAnimationRunning = e

//};

//#endif // MAPCANVAS_H


#ifndef MAPCANVAS_H
#define MAPCANVAS_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QMap>
#include <QPair>
#include <QPropertyAnimation>
//#include <QTimeLine>

#include "TilesFile/tileprocessor.h"
#include "MapCanvas/northarrow.h"

class MapCanvas : public QGraphicsView
{
    Q_OBJECT

    Q_PROPERTY(QTransform viewTransform
               READ transform
               WRITE setTransform)

public:
    explicit MapCanvas(QWidget *parent = nullptr);
    ~MapCanvas();

    //----------------------------------------
    // Base Map
    //----------------------------------------
    bool loadBaseMap(const QString &fileName);

    //----------------------------------------
    // Navigation
    //----------------------------------------
    void resetView();
    void zoomIn();
    void zoomOut();
    void rotateMap(double angle);
    void resetNorthUp();

//    void animateToRect(const QRectF &rect);
    void animateZoomToRect(const QRectF &rect);
//    void animateView(const QRectF &targetRect,
//                     int duration = 500);

    void stopAnimation();

    void setLayerVisible(const QString &filePath,
                         bool visible);

    void setScalePreset(const QString &scaleText);

    void setScaleFactor(double scale);

    //----------------------------------------
    // Information
    //----------------------------------------

    QRectF currentViewExtent() const;

    double currentScale() const;

    double rotationAngle() const;

    void geoTransform(double gt[6]) const;

    //----------------------------------------
    // Tile Management
    //----------------------------------------

    void updateVisibleTiles();

    void clearInvisibleTiles();

signals:

    void mouseCoordinateChanged(const QPointF &scenePos);

    void mouseGeoCoordinateChanged(const QPointF &geoPos);

    void scaleChanged(double scale);

    void rotationChanged(double angle);

    void extentChanged(const QRectF &extent);

    void layerLoaded(const QString &layerName);

protected:

    void wheelEvent(QWheelEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

private:

    void updateExtent();

private:

    //----------------------------------------
    // Scene
    //----------------------------------------

    QGraphicsScene *mScene = nullptr;

    QGraphicsPixmapItem *mLeftWorld = nullptr;

    QGraphicsPixmapItem *mRightWorld = nullptr;

    //----------------------------------------
    // Visible Tiles
    //----------------------------------------

    QMap<QPair<int,int>, QGraphicsPixmapItem*> mVisibleTiles;

    //----------------------------------------
    // Raster Information
    //----------------------------------------

    int mRasterWidth = 0;

    int mRasterHeight = 0;

    double mGeoTransform[6] = {0};

    //----------------------------------------
    // Navigation
    //----------------------------------------

    bool mPanning = false;

    QPoint mLastMousePos;

    QPointF mViewCenter;

    QPointF mCurrentCenter;

    QPointF mDefaultCenter;

    double mDefaultZoom = 1.0;

    QPointF mDefaultCenterScene;

    double mCurrentScale = 1.0;

    double mDefaultScale = 1.0;

    double mMinScale = 1.0;

    double mMaxScale = 20.0;

    double mRotationAngle = 0.0;

    //----------------------------------------
    // World
    //----------------------------------------

    int mWorldWidth = 0;

    int mWorldHeight = 0;

    int mTotalTilesX = 0;

    int mTotalTilesY = 0;

    //----------------------------------------
    // Base Map
    //----------------------------------------

    QString mCurrentFile;

    bool mBaseMapVisible = true;

    QTransform mBaseTransform;

    QTransform mStartTransform;

    QTransform mEndTransform;

    //----------------------------------------
    // Animation
    //----------------------------------------

    QPropertyAnimation *mZoomAnimation = nullptr;

//    QTimeLine *mNavigationTimeLine = nullptr;

    //----------------------------------------
    // Tile Processor
    //----------------------------------------

    TileProcessor mTileProcessor;

    //----------------------------------------
    // North Arrow
    //----------------------------------------

    NorthArrow *mNorthArrow = nullptr;
};

#endif // MAPCANVAS_H
