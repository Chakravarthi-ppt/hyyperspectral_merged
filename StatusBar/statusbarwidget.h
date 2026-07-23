#ifndef STATUSBARWIDGET_H
#define STATUSBARWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
class statusbarwidget;
}
QT_END_NAMESPACE

class StatusBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBarWidget(QWidget *parent = nullptr);
    ~StatusBarWidget();

    // Display Functions
    void setLonLat(const QString &text);
    void setGridReference(const QString &text);
    void setCursorPosition(const QString &text);
    void setProjection(const QString &text);
    void setViewSize(const QString &text);
    void setCurrentScale(const QString &text);
    void setRotation(double angle);

signals:

    void rotationChanged(double angle);

    void scaleChanged(const QString &scale);

private slots:

    void updateDateTime();

    void onRotationValueChanged(double value);

    void onScaleValueChanged(const QString &value);

private:

    void initializeUI();

private:

    Ui::statusbarwidget *ui;
};

#endif
