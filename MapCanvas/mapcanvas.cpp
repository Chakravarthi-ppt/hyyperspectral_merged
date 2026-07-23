//#include "mapcanvas.h"

//#include <QImage>
//#include <QPixmap>
//#include <QDebug>

//#include <QMouseEvent>
//#include <QWheelEvent>
//#include <QResizeEvent>
//#include <QScrollBar>
//#include <algorithm>
//#include <QFileInfo>
//#include <QTimer>

//#include "TilesFile/tileprocessor.h"
//#include "cpl_conv.h"

//MapCanvas::MapCanvas(QWidget *parent)
//    : QGraphicsView(parent),
//      mScene(new QGraphicsScene(this)),
////      mPanning(false),
//      mRasterWidth(0),
//      mRasterHeight(0),
////      mCurrentScale(1.0),
//      mMinScale(1.0),
////      mRotationAngle(0.0),
//      mWorldWidth(0),
//      mWorldHeight(0)
//{
//    setScene(mScene);

//    setMouseTracking(true);
//    viewport()->setMouseTracking(true);
//    setInteractive(true);
//    setDragMode(QGraphicsView::NoDrag);

//    setAlignment(Qt::AlignCenter);
//    setFrameShape(QFrame::NoFrame);
//    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
//    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

//    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

//    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
//    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
//    setRenderHint(QPainter::Antialiasing, true);
//    setRenderHint(QPainter::SmoothPixmapTransform, true);
//    setRenderHint(QPainter::TextAntialiasing, true);

//    //----------------------------------
//    // North Arrow
//    //----------------------------------

//    mNorthArrow = new NorthArrow(viewport());

//    mNorthArrow->resize(70,70);

//    mNorthArrow->move(viewport()->width()-80,20);

//    mNorthArrow->show();

//    connect(mNorthArrow,
//            &NorthArrow::resetNorth,
//            this,
//            &MapCanvas::resetNorthUp);
//}

//MapCanvas::~MapCanvas()
//{
//    mTileProcessor.close();
//}

//bool MapCanvas::loadBaseMap(const QString &fileName)
//{
//    mCurrentFile = fileName;

//    if (!mTileProcessor.openInput(fileName))
//    {
//        qDebug() << "Cannot open :" << fileName;
//        return false;
//    }

//    //-------------------------------------
//    // Raster Information
//    //-------------------------------------
//    mRasterWidth  = mTileProcessor.cols();
//    mRasterHeight = mTileProcessor.rows();

//    memcpy(mGeoTransform, mTileProcessor.geoTransform(), sizeof(double) * 6);

//    qDebug() << "GeoTransform:"
//             << mGeoTransform[0] << mGeoTransform[1] << mGeoTransform[2]
//             << mGeoTransform[3] << mGeoTransform[4] << mGeoTransform[5];

//    mTotalTilesX = (mRasterWidth + TileProcessor::TILE_SIZE - 1) / TileProcessor::TILE_SIZE;
//    mTotalTilesY = (mRasterHeight + TileProcessor::TILE_SIZE - 1) / TileProcessor::TILE_SIZE;

//    mWorldWidth = mRasterWidth;
//    mWorldHeight = mRasterHeight;

//    //-------------------------------------
//    // Clear Scene
//    //-------------------------------------
//    mScene->clear();
//    mVisibleTiles.clear();

//    if(mNorthArrow)
//    {
//        mNorthArrow->raise();
//        mNorthArrow->show();
//    }

//    //-------------------------------------
//    // Scene – allow three copies for seamless wrapping
//    //-------------------------------------
//    mScene->setSceneRect(
//        0,
//        0,
//        mRasterWidth,
//        mRasterHeight);

//    //-------------------------------------
//    // View – stretch to fill viewport
//    //-------------------------------------
//    resetTransform();

//    fitInView(
//        mScene->sceneRect(),
//        Qt::KeepAspectRatio);

//    mMinScale = transform().m11();

//    mCurrentScale = 1.0;

//    mRotationAngle = 0.0;

//    mBaseTransform = transform();

//    centerOn(
//        mRasterWidth/2.0,
//        mRasterHeight/2.0);

//    updateVisibleTiles();

//    emit layerLoaded(QFileInfo(fileName).fileName());
//    emit scaleChanged(mCurrentScale);
//    emit rotationChanged(0);
//    updateExtent();

//    return true;

//}

//void MapCanvas::resizeEvent(QResizeEvent *event)
//{
//    QGraphicsView::resizeEvent(event);

//    resetTransform();

//    fitInView(sceneRect(), Qt::KeepAspectRatio);

//    mMinScale = transform().m11();

//    mBaseTransform = transform();

//    setScaleFactor(mCurrentScale);

//    updateVisibleTiles();

//    updateExtent();

//    viewport()->update();

//    if(mNorthArrow)
//    {
//        mNorthArrow->move(
//                    viewport()->width()-80,
//                    20);
//    }
//}

//void MapCanvas::mouseMoveEvent(QMouseEvent *event)
//{
//    //------------------------------------------
//    // Scene Coordinate
//    //------------------------------------------

//    QPointF scenePos = mapToScene(event->pos());

//    emit mouseCoordinateChanged(scenePos);

//    //------------------------------------------
//    // Wrap X Coordinate
//    //------------------------------------------

//    double x = scenePos.x();
//    double y = scenePos.y();

//    while (x < 0)
//        x += mRasterWidth;

//    while (x >= mRasterWidth)
//        x -= mRasterWidth;

//    //------------------------------------------
//    // Clamp Y
//    //------------------------------------------

//    if (y < 0)
//        y = 0;

//    if (y > mRasterHeight)
//        y = mRasterHeight;

//    //------------------------------------------
//    // Pixel -> Longitude Latitude
//    //------------------------------------------

//    double lon =
//            (x / static_cast<double>(mRasterWidth))
//            * 360.0 - 180.0;

//    double lat =
//            90.0 -
//            (y / static_cast<double>(mRasterHeight))
//            * 180.0;

//    emit mouseGeoCoordinateChanged(QPointF(lon, lat));

//    //------------------------------------------
//    // Panning
//    //------------------------------------------

//    if (mPanning)
//    {
//        QPointF center =
//                mapToScene(viewport()->rect().center());

//        QPointF delta =
//                mapToScene(mLastMousePos) -
//                mapToScene(event->pos());

//        center += delta;

//        mLastMousePos = event->pos();

//        QRectF viewRect =
//                mapToScene(viewport()->rect()).boundingRect();

//        double halfW = viewRect.width() / 2.0;
//        double halfH = viewRect.height() / 2.0;

//        center.setX(qBound(halfW,
//                           center.x(),
//                           mRasterWidth - halfW));

//        center.setY(qBound(halfH,
//                           center.y(),
//                           mRasterHeight - halfH));

//        centerOn(center);

//        updateVisibleTiles();
//        updateExtent();

//        viewport()->update();

//        event->accept();
//        return;
//    }

//    QGraphicsView::mouseMoveEvent(event);
//}

//QRectF MapCanvas::currentViewExtent() const
//{
//    return mapToScene(viewport()->rect()).boundingRect();
//}

//void MapCanvas::rotateMap(double angle)
//{
//    mRotationAngle = angle;

//    setScaleFactor(mCurrentScale);

//    if(mNorthArrow)
//        mNorthArrow->setRotationAngle(angle);

//    emit rotationChanged(angle);
//}

//void MapCanvas::updateExtent()
//{
//    QRectF rect = currentViewExtent();
//    emit extentChanged(rect);
//}

//void MapCanvas::wheelEvent(QWheelEvent *event)
//{
//    const double zoomFactor = 1.20;

//    if(event->angleDelta().y() > 0)
//    {
//        setScaleFactor(mCurrentScale * zoomFactor);
//    }
//    else
//    {
//        setScaleFactor(mCurrentScale / zoomFactor);
//    }

//    event->accept();
//}

//void MapCanvas::mousePressEvent(QMouseEvent *event)
//{
//    if (event->button() == Qt::LeftButton)
//    {
//        mPanning = true;
//        mLastMousePos = event->pos();
//        setCursor(Qt::ClosedHandCursor);
//        event->accept();
//        return;
//    }
//    QGraphicsView::mousePressEvent(event);
//}

//void MapCanvas::mouseReleaseEvent(QMouseEvent *event)
//{
//    if (event->button() == Qt::LeftButton)
//    {
//        mPanning = false;
//        setCursor(Qt::ArrowCursor);
//        updateExtent();
//        event->accept();
//        return;
//    }
//    QGraphicsView::mouseReleaseEvent(event);
//}

//void MapCanvas::zoomIn()
//{
//    setScaleFactor(mCurrentScale * 1.15);
//}

//void MapCanvas::zoomOut()
//{
//    if(mCurrentScale / 1.15 >= mMinScale)
//        setScaleFactor(mCurrentScale / 1.15);
//}

//double MapCanvas::currentScale() const
//{
//    return mCurrentScale;
//}

//void MapCanvas::updateVisibleTiles()
//{

//   // qDebug() << "updateVisibleTiles() Called";

//    if(!mBaseMapVisible)
//        return;

//    QRectF visible = mapToScene(viewport()->rect()).boundingRect();
//    int margin = TileProcessor::TILE_SIZE * 2;
//    QRectF expandedVisible = visible.adjusted(-margin, -margin, margin, margin);

//    int firstTileX = floor(expandedVisible.left() / TileProcessor::TILE_SIZE);
//    int lastTileX  = floor(expandedVisible.right() / TileProcessor::TILE_SIZE);
//    int firstTileY = floor(expandedVisible.top() / TileProcessor::TILE_SIZE);
//    int lastTileY  = floor(expandedVisible.bottom() / TileProcessor::TILE_SIZE);

//    firstTileY = std::max(0, firstTileY);
//    lastTileY  = std::min(mTotalTilesY - 1, lastTileY);

//    for (int ty = firstTileY; ty <= lastTileY; ++ty)
//    {
//        for (int tx = firstTileX; tx <= lastTileX; ++tx)
//        {
//            QPair<int,int> key(tx, ty);
//            if (mVisibleTiles.contains(key))
//                continue;

//            QImage tile = mTileProcessor.getWrappedTile(tx, ty, 0);
//            if (tile.isNull())
//                continue;

//            int wrapped = ((tx % mTotalTilesX) + mTotalTilesX) % mTotalTilesX;
//            int leftInRaster = wrapped * TileProcessor::TILE_SIZE;
//            int posX = tx * TileProcessor::TILE_SIZE;

//            while (posX < -mRasterWidth)
//                posX += mRasterWidth;
//            while (posX > 2 * mRasterWidth)
//                posX -= mRasterWidth;

//            if (posX != leftInRaster)
//            {
//                int diff = posX - leftInRaster;
//                int k = (diff >= 0) ? (diff + mRasterWidth/2) / mRasterWidth
//                                    : (diff - mRasterWidth/2) / mRasterWidth;
//                posX = leftInRaster + k * mRasterWidth;
//            }

//            QGraphicsPixmapItem *item = mScene->addPixmap(QPixmap::fromImage(tile));
//            item->setPos(posX, ty * TileProcessor::TILE_SIZE);
//            mVisibleTiles.insert(key, item);
//        }
//    }

//    clearInvisibleTiles();
//}

//void MapCanvas::clearInvisibleTiles()
//{
//    QRectF visible = mapToScene(viewport()->rect()).boundingRect();
//    int margin = TileProcessor::TILE_SIZE * 2;
//    visible.adjust(-margin, -margin, margin, margin);

//    auto it = mVisibleTiles.begin();
//    while (it != mVisibleTiles.end())
//    {
//        QRectF itemRect = it.value()->sceneBoundingRect();
//        bool isVisible = false;

//        if (visible.intersects(itemRect))
//            isVisible = true;
//        else
//        {
//            QRectF shifted = itemRect;
//            shifted.moveLeft(itemRect.left() + mRasterWidth);
//            if (visible.intersects(shifted))
//                isVisible = true;
//            else
//            {
//                shifted.moveLeft(itemRect.left() - mRasterWidth);
//                if (visible.intersects(shifted))
//                    isVisible = true;
//            }
//        }

//        if (!isVisible)
//        {
//            mScene->removeItem(it.value());
//            delete it.value();
//            it = mVisibleTiles.erase(it);
//        }
//        else
//        {
//            ++it;
//        }
//    }
//}

//void MapCanvas::setScalePreset(const QString &scaleText)
//{
//    double factor = 1.0;

//    if(scaleText == "1:1000")
//        factor = 8.0;
//    else if(scaleText == "1:2500")
//        factor = 4.0;
//    else if(scaleText == "1:5000")
//        factor = 2.5;
//    else if(scaleText == "1:10000")
//        factor = 1.5;
//    else if(scaleText == "1:25000")
//        factor = 1.0;
//    else if(scaleText == "1:50000")
//        factor = 0.75;
//    else if(scaleText == "1:100000")
//        factor = 0.5;

//    setScaleFactor(factor);
//}

//void MapCanvas::setScaleFactor(double factor)
//{
//    if(factor < 1.0)
//        factor = 1.0;

//    mCurrentScale = factor;

//    QPointF center =
//            mapToScene(viewport()->rect().center());

//    QTransform t = mBaseTransform;

//    t.rotate(mRotationAngle);
//    t.scale(mCurrentScale, mCurrentScale);

//    setTransform(t);

//    centerOn(center);

//    emit scaleChanged(mCurrentScale);

//    updateVisibleTiles();
//    updateExtent();

//    viewport()->update();
//}

//void MapCanvas::setLayerVisible(const QString &filePath,
//                                bool visible)
//{
//    if(filePath != mCurrentFile)
//        return;

//    mBaseMapVisible = visible;

//    if(!visible)
//    {
//        qDeleteAll(mVisibleTiles);
//        mVisibleTiles.clear();
//    }
//    else
//    {
//        updateVisibleTiles();
//    }

//    viewport()->update();
//}

//void MapCanvas::geoTransform(double gt[6]) const
//{
//    memcpy(gt,
//           mGeoTransform,
//           sizeof(double)*6);

//    qDebug() << "World GT :"
//             << gt[0]
//             << gt[1]
//             << gt[2]
//             << gt[3]
//             << gt[4]
//             << gt[5];
//}

//void MapCanvas::animateToRect(const QRectF &rect)
//{
//    //---------------------------------------
//    // Save current transform
//    //---------------------------------------

//    QTransform startTransform = transform();

//    //---------------------------------------
//    // Calculate target transform
//    //---------------------------------------

//    fitInView(rect, Qt::KeepAspectRatio);

//    QTransform endTransform = transform();

//    //---------------------------------------
//    // Restore original transform
//    //---------------------------------------

//    setTransform(startTransform);

//    //---------------------------------------
//    // Center on target
//    //---------------------------------------

//    centerOn(rect.center());

//    //---------------------------------------
//    // Animate
//    //---------------------------------------

//    QPropertyAnimation *animation =
//            new QPropertyAnimation(this,
//                                   "viewTransform");

//    animation->setDuration(700);

//    animation->setStartValue(startTransform);

//    animation->setEndValue(endTransform);

//    animation->setEasingCurve(QEasingCurve::InOutCubic);

//    connect(animation,
//            &QPropertyAnimation::finished,
//            this,
//            [=]()
//    {
//        setTransform(endTransform);
//        centerOn(rect.center());
//    });

//    animation->start(QAbstractAnimation::DeleteWhenStopped);
//}

//void MapCanvas::resetNorthUp()
//{
//    rotateMap(0);
//}

//double MapCanvas::rotationAngle() const
//{
//    return mRotationAngle;
//}

#include "mapcanvas.h"

#include <QImage>
#include <QPixmap>
#include <QDebug>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <algorithm>
#include <QFileInfo>
#include <QTimer>
#include <QApplication>
//#include <QVariantAnimation>
#include <QEasingCurve>
#include <QTransform>

#include "TilesFile/tileprocessor.h"
#include "cpl_conv.h"

MapCanvas::MapCanvas(QWidget *parent)
    : QGraphicsView(parent),
      mScene(new QGraphicsScene(this)),
      mRasterWidth(0),
      mRasterHeight(0),
      mMinScale(1.0),
      mWorldWidth(0),
      mWorldHeight(0)
{
    setScene(mScene);

    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setInteractive(true);
    setDragMode(QGraphicsView::NoDrag);

    setAlignment(Qt::AlignCenter);
    setFrameShape(QFrame::NoFrame);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setRenderHint(QPainter::TextAntialiasing, true);

    //----------------------------------
    // North Arrow
    //----------------------------------

    mNorthArrow = new NorthArrow(viewport());

    mNorthArrow->resize(70,70);

    mNorthArrow->move(viewport()->width()-80,20);

    mNorthArrow->show();

    connect(mNorthArrow,
            &NorthArrow::resetNorth,
            this,
            &MapCanvas::resetNorthUp);

    //----------------------------------------
    // Navigation Animation
    //----------------------------------------

}

MapCanvas::~MapCanvas()
{
    mTileProcessor.close();
}

bool MapCanvas::loadBaseMap(const QString &fileName)
{
    mCurrentFile = fileName;

    if (!mTileProcessor.openInput(fileName))
    {
        qDebug() << "Cannot open :" << fileName;
        return false;
    }

    //-------------------------------------
    // Raster Information
    //-------------------------------------
    mRasterWidth  = mTileProcessor.cols();
    mRasterHeight = mTileProcessor.rows();

    memcpy(mGeoTransform, mTileProcessor.geoTransform(), sizeof(double) * 6);

    qDebug() << "GeoTransform:"
             << mGeoTransform[0] << mGeoTransform[1] << mGeoTransform[2]
             << mGeoTransform[3] << mGeoTransform[4] << mGeoTransform[5];

    mTotalTilesX = (mRasterWidth + TileProcessor::TILE_SIZE - 1) / TileProcessor::TILE_SIZE;
    mTotalTilesY = (mRasterHeight + TileProcessor::TILE_SIZE - 1) / TileProcessor::TILE_SIZE;

    mWorldWidth = mRasterWidth;
    mWorldHeight = mRasterHeight;

    //-------------------------------------
    // Clear Scene
    //-------------------------------------
    mScene->clear();
    mVisibleTiles.clear();

    if(mNorthArrow)
    {
        mNorthArrow->raise();
        mNorthArrow->show();
    }

    //-------------------------------------
    // Scene – allow three copies for seamless wrapping
    //-------------------------------------
    mScene->setSceneRect(
                0,
                0,
                mRasterWidth,
                mRasterHeight);

    //-------------------------------------
    // View – stretch to fill viewport
    //-------------------------------------
    resetTransform();

    fitInView(
                mScene->sceneRect(),
                Qt::KeepAspectRatio);

    mMinScale = transform().m11();

    mCurrentScale = 1.0;

    mRotationAngle = 0.0;

    mBaseTransform = transform();

    {
        //--------------------------------------
        // Initial view on India
        //--------------------------------------

        const double indiaLon = 80.0;
        const double indiaLat = 22.0;

        double x = (indiaLon + 180.0) / 360.0 * mRasterWidth;
        double y = (90.0 - indiaLat) / 180.0 * mRasterHeight;

        setScaleFactor(3.9);      // adjust between 4.5 and 5.5 if required
        centerOn(x, y);

        mDefaultZoom = mCurrentScale;
        mDefaultCenterScene = QPointF(x, y);

        //        updateVisibleTiles();

        //        viewport()->update();
    }

    updateVisibleTiles();

    emit layerLoaded(QFileInfo(fileName).fileName());
    emit scaleChanged(mCurrentScale);
    emit rotationChanged(0);
    updateExtent();

    return true;
}

//void MapCanvas::resizeEvent(QResizeEvent *event)
//{
//    QGraphicsView::resizeEvent(event);

//    resetTransform();

//    fitInView(sceneRect(), Qt::KeepAspectRatio);

//    mMinScale = transform().m11();

//    mBaseTransform = transform();

//    setScaleFactor(mCurrentScale);

//    updateVisibleTiles();

//    updateExtent();

//    viewport()->update();

//    if(mNorthArrow)
//    {
//        mNorthArrow->move(
//                    viewport()->width()-80,
//                    20);
//    }
//}

void MapCanvas::resizeEvent(QResizeEvent *event)
{
    // Save current center before resizing
    QPointF currentCenter = mapToScene(viewport()->rect().center());

    QGraphicsView::resizeEvent(event);

    //--------------------------------------
    // Recalculate minimum fit scale
    //--------------------------------------
    resetTransform();

    fitInView(sceneRect(), Qt::KeepAspectRatio);

    mMinScale = transform().m11();

    mBaseTransform = transform();

    //--------------------------------------
    // Restore current zoom
    //--------------------------------------
    QTransform t = mBaseTransform;
    t.rotate(mRotationAngle);
    t.scale(mCurrentScale, mCurrentScale);

    setTransform(t);

    //--------------------------------------
    // Restore previous center
    //--------------------------------------
    centerOn(currentCenter);

    updateVisibleTiles();
    updateExtent();

    viewport()->update();

    //--------------------------------------
    // Keep north arrow at top-right
    //--------------------------------------
    if (mNorthArrow)
    {
        mNorthArrow->move(viewport()->width() - 80, 20);
    }
}

void MapCanvas::mouseMoveEvent(QMouseEvent *event)
{
    //------------------------------------------
    // Scene Coordinate
    //------------------------------------------

    QPointF scenePos = mapToScene(event->pos());

    emit mouseCoordinateChanged(scenePos);

    //------------------------------------------
    // Wrap X Coordinate
    //------------------------------------------

    double x = scenePos.x();
    double y = scenePos.y();

    while (x < 0)
        x += mRasterWidth;

    while (x >= mRasterWidth)
        x -= mRasterWidth;

    //------------------------------------------
    // Clamp Y
    //------------------------------------------

    if (y < 0)
        y = 0;

    if (y > mRasterHeight)
        y = mRasterHeight;

    //------------------------------------------
    // Pixel -> Longitude Latitude
    //------------------------------------------

    double lon =
            (x / static_cast<double>(mRasterWidth))
            * 360.0 - 180.0;

    double lat =
            90.0 -
            (y / static_cast<double>(mRasterHeight))
            * 180.0;

    emit mouseGeoCoordinateChanged(QPointF(lon, lat));

    //------------------------------------------
    // Panning
    //------------------------------------------

    if (mPanning)
    {
        QPointF center =
                mapToScene(viewport()->rect().center());

        QPointF delta =
                mapToScene(mLastMousePos) -
                mapToScene(event->pos());

        center += delta;

        mLastMousePos = event->pos();

        QRectF viewRect =
                mapToScene(viewport()->rect()).boundingRect();

        double halfW = viewRect.width() / 2.0;
        double halfH = viewRect.height() / 2.0;

        center.setX(qBound(halfW,
                           center.x(),
                           mRasterWidth - halfW));

        center.setY(qBound(halfH,
                           center.y(),
                           mRasterHeight - halfH));

        centerOn(center);

        updateVisibleTiles();
        updateExtent();

        viewport()->update();

        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

QRectF MapCanvas::currentViewExtent() const
{
    return mapToScene(viewport()->rect()).boundingRect();
}

void MapCanvas::rotateMap(double angle)
{
    mRotationAngle = angle;

    setScaleFactor(mCurrentScale);

    if(mNorthArrow)
        mNorthArrow->setRotationAngle(angle);

    emit rotationChanged(angle);
}

void MapCanvas::updateExtent()
{
    QRectF rect = currentViewExtent();
    emit extentChanged(rect);
}

void MapCanvas::wheelEvent(QWheelEvent *event)
{
    const double zoomFactor = 1.20;

    if(event->angleDelta().y() > 0)
    {
        setScaleFactor(mCurrentScale * zoomFactor);
    }
    else
    {
        setScaleFactor(mCurrentScale / zoomFactor);
    }

    event->accept();
}

void MapCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        mPanning = true;
        mLastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        mPanning = false;
        setCursor(Qt::ArrowCursor);
        updateExtent();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MapCanvas::zoomIn()
{
    setScaleFactor(mCurrentScale * 1.15);
}

void MapCanvas::zoomOut()
{
    if(mCurrentScale / 1.15 >= mMinScale)
        setScaleFactor(mCurrentScale / 1.15);
}

double MapCanvas::currentScale() const
{
    return mCurrentScale;
}

void MapCanvas::updateVisibleTiles()
{

    // qDebug() << "updateVisibleTiles() Called";

    if(!mBaseMapVisible)
        return;

    QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    int margin = TileProcessor::TILE_SIZE * 2;
    QRectF expandedVisible = visible.adjusted(-margin, -margin, margin, margin);

    int firstTileX = floor(expandedVisible.left() / TileProcessor::TILE_SIZE);
    int lastTileX  = floor(expandedVisible.right() / TileProcessor::TILE_SIZE);
    int firstTileY = floor(expandedVisible.top() / TileProcessor::TILE_SIZE);
    int lastTileY  = floor(expandedVisible.bottom() / TileProcessor::TILE_SIZE);

    firstTileY = std::max(0, firstTileY);
    lastTileY  = std::min(mTotalTilesY - 1, lastTileY);

    for (int ty = firstTileY; ty <= lastTileY; ++ty)
    {
        for (int tx = firstTileX; tx <= lastTileX; ++tx)
        {
            QPair<int,int> key(tx, ty);
            if (mVisibleTiles.contains(key))
                continue;

            QImage tile = mTileProcessor.getWrappedTile(tx, ty, 0);
            if (tile.isNull())
                continue;

            int wrapped = ((tx % mTotalTilesX) + mTotalTilesX) % mTotalTilesX;
            int leftInRaster = wrapped * TileProcessor::TILE_SIZE;
            int posX = tx * TileProcessor::TILE_SIZE;

            while (posX < -mRasterWidth)
                posX += mRasterWidth;
            while (posX > 2 * mRasterWidth)
                posX -= mRasterWidth;

            if (posX != leftInRaster)
            {
                int diff = posX - leftInRaster;
                int k = (diff >= 0) ? (diff + mRasterWidth/2) / mRasterWidth
                                    : (diff - mRasterWidth/2) / mRasterWidth;
                posX = leftInRaster + k * mRasterWidth;
            }

            QGraphicsPixmapItem *item = mScene->addPixmap(QPixmap::fromImage(tile));
            item->setPos(posX, ty * TileProcessor::TILE_SIZE);
            mVisibleTiles.insert(key, item);
        }
    }

    clearInvisibleTiles();
}

void MapCanvas::clearInvisibleTiles()
{
    QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    int margin = TileProcessor::TILE_SIZE * 2;
    visible.adjust(-margin, -margin, margin, margin);

    auto it = mVisibleTiles.begin();
    while (it != mVisibleTiles.end())
    {
        QRectF itemRect = it.value()->sceneBoundingRect();
        bool isVisible = false;

        if (visible.intersects(itemRect))
            isVisible = true;
        else
        {
            QRectF shifted = itemRect;
            shifted.moveLeft(itemRect.left() + mRasterWidth);
            if (visible.intersects(shifted))
                isVisible = true;
            else
            {
                shifted.moveLeft(itemRect.left() - mRasterWidth);
                if (visible.intersects(shifted))
                    isVisible = true;
            }
        }

        if (!isVisible)
        {
            mScene->removeItem(it.value());
            delete it.value();
            it = mVisibleTiles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void MapCanvas::setScalePreset(const QString &scaleText)
{
    double factor = 1.0;

    if(scaleText == "1:1000")
        factor = 8.0;
    else if(scaleText == "1:2500")
        factor = 4.0;
    else if(scaleText == "1:5000")
        factor = 2.5;
    else if(scaleText == "1:10000")
        factor = 1.5;
    else if(scaleText == "1:25000")
        factor = 1.0;
    else if(scaleText == "1:50000")
        factor = 0.75;
    else if(scaleText == "1:100000")
        factor = 0.5;

    setScaleFactor(factor);
}

void MapCanvas::setScaleFactor(double factor)
{
    if(factor < 1.0)
        factor = 1.0;

    mCurrentScale = factor;

    QPointF center =
            mapToScene(viewport()->rect().center());

    QTransform t = mBaseTransform;

    t.rotate(mRotationAngle);
    t.scale(mCurrentScale, mCurrentScale);

    setTransform(t);

    centerOn(center);

    emit scaleChanged(mCurrentScale);

    updateVisibleTiles();
    updateExtent();

    viewport()->update();
}

void MapCanvas::setLayerVisible(const QString &filePath,
                                bool visible)
{
    if(filePath != mCurrentFile)
        return;

    mBaseMapVisible = visible;

    if(!visible)
    {
        qDeleteAll(mVisibleTiles);
        mVisibleTiles.clear();
    }
    else
    {
        updateVisibleTiles();
    }

    viewport()->update();
}

void MapCanvas::geoTransform(double gt[6]) const
{
    memcpy(gt,
           mGeoTransform,
           sizeof(double)*6);

    qDebug() << "World GT :"
             << gt[0]
             << gt[1]
             << gt[2]
             << gt[3]
             << gt[4]
             << gt[5];

    qDebug() << "Raster Width :" << mRasterWidth;
    qDebug() << "Raster Height:" << mRasterHeight;
}

//void MapCanvas::animateToRect(const QRectF &rect)
//{
//    //---------------------------------------
//    // Save current transform
//    //---------------------------------------

//    QTransform startTransform = transform();

//    //---------------------------------------
//    // Calculate target transform
//    //---------------------------------------

//    fitInView(rect, Qt::KeepAspectRatio);

//    QTransform endTransform = transform();

//    //---------------------------------------
//    // Restore original transform
//    //---------------------------------------

//    setTransform(startTransform);

//    //---------------------------------------
//    // Center on target
//    //---------------------------------------

//    centerOn(rect.center());

//    //---------------------------------------
//    // Animate
//    //---------------------------------------

//    QPropertyAnimation *animation =
//            new QPropertyAnimation(this,
//                                   "viewTransform");

//    animation->setDuration(700);

//    animation->setStartValue(startTransform);

//    animation->setEndValue(endTransform);

//    animation->setEasingCurve(QEasingCurve::InOutCubic);

//    connect(animation,
//            &QPropertyAnimation::finished,
//            this,
//            [=]()
//    {
//        setTransform(endTransform);
//        centerOn(rect.center());
//    });

//    animation->start(QAbstractAnimation::DeleteWhenStopped);
//}

void MapCanvas::resetNorthUp()
{
    rotateMap(0);
}

double MapCanvas::rotationAngle() const
{
    return mRotationAngle;
}

void MapCanvas::animateZoomToRect(const QRectF &rect)
{
    if(rect.isEmpty())
        return;

    //----------------------------------------------------
    // Save current state
    //----------------------------------------------------

    QPointF startCenter =
            mapToScene(viewport()->rect().center());

    QRectF currentView =
            mapToScene(viewport()->rect()).boundingRect();

    double startWidth = currentView.width();

    //----------------------------------------------------
    // Calculate target zoom
    //----------------------------------------------------

    fitInView(rect, Qt::KeepAspectRatio);

    QRectF targetView =
            mapToScene(viewport()->rect()).boundingRect();

    double endWidth = targetView.width();

    QPointF endCenter = rect.center();

    //----------------------------------------------------
    // Restore current view
    //----------------------------------------------------

    resetTransform();

    fitInView(currentView, Qt::KeepAspectRatio);

    //----------------------------------------------------
    // Animation
    //----------------------------------------------------

    QVariantAnimation *animation =
            new QVariantAnimation(this);

    animation->setDuration(900);

    animation->setStartValue(0.0);

    animation->setEndValue(1.0);

    animation->setEasingCurve(QEasingCurve::InOutCubic);

    connect(animation,
            &QVariantAnimation::valueChanged,
            this,
            [=](const QVariant &value)
    {
        double t = value.toDouble();

        //------------------------------------------------
        // Interpolate center
        //------------------------------------------------

        QPointF center;

        center.setX(
                    startCenter.x() +
                    (endCenter.x() - startCenter.x()) * t);

        center.setY(
                    startCenter.y() +
                    (endCenter.y() - startCenter.y()) * t);

        //------------------------------------------------
        // Interpolate zoom
        //------------------------------------------------

        double width =
                startWidth +
                (endWidth - startWidth) * t;

        QRectF viewRect(
                    center.x() - width / 2.0,
                    center.y() - width / 2.0,
                    width,
                    width);

        fitInView(viewRect,
                  Qt::KeepAspectRatio);
    });

    connect(animation,
            &QVariantAnimation::finished,
            this,
            [=]()
    {
        QRectF view =
                mapToScene(viewport()->rect()).boundingRect();

        mCurrentScale = transform().m11() / mBaseTransform.m11();

        updateExtent();

        emit scaleChanged(mCurrentScale);

        animation->deleteLater();
    });

    animation->start();
}


//void MapCanvas::animateView(const QRectF &targetRect,
//                            int duration)
//{
//    qDebug() << "Animation Engine Called";

//    if(targetRect.isEmpty())
//        return;

//    // We'll implement the animation logic in the next step.
//}


void MapCanvas::resetView()
{
    //----------------------------------------
    // Reset transform
    //----------------------------------------

    resetTransform();

    setTransform(mBaseTransform);

    //----------------------------------------
    // Restore startup zoom
    //----------------------------------------

    setScaleFactor(mDefaultZoom);

    //----------------------------------------
    // Restore startup center
    //----------------------------------------

    centerOn(mDefaultCenterScene);

    //----------------------------------------
    // Reset rotation
    //----------------------------------------

    mRotationAngle = 0.0;

    emit scaleChanged(mCurrentScale);
    emit rotationChanged(0);

    updateVisibleTiles();
    updateExtent();

    viewport()->update();
}
