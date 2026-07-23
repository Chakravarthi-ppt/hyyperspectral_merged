#ifndef NORTHARROW_H
#define NORTHARROW_H

#include <QLabel>
#include <QPixmap>

class NorthArrow : public QLabel
{
    Q_OBJECT

public:
    explicit NorthArrow(QWidget *parent=nullptr);

    void setRotationAngle(double angle);

    double rotationAngle() const;

signals:
    void resetNorth();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    double mAngle = 0.0;
    QPixmap mOriginalPixmap;
};

#endif
