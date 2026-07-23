#include "northarrow.h"

#include <QMouseEvent>
#include <QPixmap>
#include <QTransform>

NorthArrow::NorthArrow(QWidget *parent)
    : QLabel(parent),
      mAngle(0.0)
{
    mOriginalPixmap.load(":/file/North_Arrow.png");

    setPixmap(mOriginalPixmap);

    setScaledContents(true);

    resize(70,70);

    setStyleSheet("background:transparent;");
}

void NorthArrow::setRotationAngle(double angle)
{
    mAngle = angle;

    QTransform t;
    t.rotate(angle);

    setPixmap(
        mOriginalPixmap.transformed(
            t,
            Qt::SmoothTransformation));
}

void NorthArrow::mouseDoubleClickEvent(QMouseEvent *)
{
    emit resetNorth();
}

double NorthArrow::rotationAngle() const
{
    return mAngle;
}
