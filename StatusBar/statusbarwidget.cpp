//#include "statusbarwidget.h"
//#include "ui_statusbarwidget.h"

//#include <QDateTime>
//#include <QTimer>

//StatusBarWidget::StatusBarWidget(QWidget *parent)
//    : QWidget(parent),
//      ui(new Ui::statusbarwidget)
//{
//    ui->setupUi(this);

//    initializeUI();


//    connect(ui->rotationSpinBox,
//            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
//            this,
//            &StatusBarWidget::onRotationValueChanged);

//    connect(ui->scaleComboBox,
//            &QComboBox::currentTextChanged,
//            this,
//            &StatusBarWidget::onScaleValueChanged);

//    QTimer *timer = new QTimer(this);

//    connect(timer,
//            &QTimer::timeout,
//            this,
//            &StatusBarWidget::updateDateTime);

//    timer->start(1000);

//    updateDateTime();
//}

//StatusBarWidget::~StatusBarWidget()
//{
//    delete ui;
//}

//void StatusBarWidget::initializeUI()
//{
//    ui->projectionLineEdit->setText("WGS84");

//    ui->viewSizeLineEdit->setText("0 x 0 km");

//    ui->rotationSpinBox->setValue(0);

//    ui->scaleComboBox->setCurrentText("1:1000000");

//    ui->lonLatLineEdit->setToolTip("Longitude / Latitude");

//    ui->cursorLineEdit->setToolTip("UTM Coordinates");

//    ui->gridLineEdit->setToolTip("Grid Reference");

//    ui->projectionLineEdit->setToolTip("Projection");

//    ui->viewSizeLineEdit->setToolTip("Current View");

//    ui->scaleComboBox->setToolTip("Scale");

//    ui->rotationSpinBox->setToolTip("Rotation");


//       ui->cursorLineEdit->setText("0.00 , 0.00");   // <-- temporary default
//        ui->lonLatLineEdit->setText("-- , --");
//        ui->gridLineEdit->setText("--");

//}

//void StatusBarWidget::updateDateTime()
//{
//    ui->dateTimeLineEdit->setText(
//                QDateTime::currentDateTime()
//                .toString("dd MMM yyyy hh:mm:ss"));
//}

//void StatusBarWidget::setLonLat(const QString &text)
//{
//    ui->lonLatLineEdit->setText(text);
//}

//void StatusBarWidget::setGridReference(const QString &text)
//{
//    ui->gridLineEdit->setText(text);
//}

//void StatusBarWidget::setCursorPosition(const QString &text)
//{
//    ui->cursorLineEdit->setText(text);
//}

//void StatusBarWidget::setProjection(const QString &text)
//{
//    ui->projectionLineEdit->setText(text);
//}

//void StatusBarWidget::setViewSize(const QString &text)
//{
//    ui->viewSizeLineEdit->setText(text);
//}

//void StatusBarWidget::setCurrentScale(const QString &text)
//{
//    ui->scaleComboBox->blockSignals(true);

//    ui->scaleComboBox->setCurrentText(text);

//    ui->scaleComboBox->blockSignals(false);
//}

//void StatusBarWidget::setRotation(double angle)
//{
//    ui->rotationSpinBox->blockSignals(true);

//    ui->rotationSpinBox->setValue(qRound(angle * 10.0) / 10.0);

//    ui->rotationSpinBox->blockSignals(false);
//}

//void StatusBarWidget::onRotationValueChanged(double value)
//{
//    emit rotationChanged(value);
//}

//void StatusBarWidget::onScaleValueChanged(const QString &value)
//{
//    emit scaleChanged(value);
//}


#include "statusbarwidget.h"
#include "ui_statusbarwidget.h"

#include <QDateTime>
#include <QTimer>

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::statusbarwidget)
{
    ui->setupUi(this);

    initializeUI();


    connect(ui->rotationSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            &StatusBarWidget::onRotationValueChanged);

    connect(ui->scaleComboBox,
            &QComboBox::currentTextChanged,
            this,
            &StatusBarWidget::onScaleValueChanged);

    QTimer *timer = new QTimer(this);

    connect(timer,
            &QTimer::timeout,
            this,
            &StatusBarWidget::updateDateTime);

    timer->start(1000);

    updateDateTime();
}

StatusBarWidget::~StatusBarWidget()
{
    delete ui;
}

void StatusBarWidget::initializeUI()
{
    ui->projectionLineEdit->setText("WGS84");

    ui->viewSizeLineEdit->setText("0 x 0 km");

    ui->rotationSpinBox->setValue(0);

    ui->scaleComboBox->setCurrentText("1:1000000");

    ui->lonLatLineEdit->setToolTip("Longitude / Latitude");

    ui->cursorLineEdit->setToolTip("UTM Coordinates");

    ui->gridLineEdit->setToolTip("Grid Reference");

    ui->projectionLineEdit->setToolTip("Projection");

    ui->viewSizeLineEdit->setToolTip("Current View");

    ui->scaleComboBox->setToolTip("Scale");

    ui->rotationSpinBox->setToolTip("Rotation");

    // Grid and Scale are not shown in the status bar.
    ui->labelGrid->setVisible(false);
    ui->gridLineEdit->setVisible(false);

    ui->labelScale->setVisible(false);
    ui->scaleComboBox->setVisible(false);


       ui->cursorLineEdit->setText("0.00 , 0.00");   // <-- temporary default
        ui->lonLatLineEdit->setText("-- , --");
        ui->gridLineEdit->setText("--");

}

void StatusBarWidget::updateDateTime()
{
    ui->dateTimeLineEdit->setText(
                QDateTime::currentDateTime()
                .toString("dd MMM yyyy hh:mm:ss"));
}

void StatusBarWidget::setLonLat(const QString &text)
{
    ui->lonLatLineEdit->setText(text);
}

void StatusBarWidget::setGridReference(const QString &text)
{
    ui->gridLineEdit->setText(text);
}

void StatusBarWidget::setCursorPosition(const QString &text)
{
    ui->cursorLineEdit->setText(text);
}

void StatusBarWidget::setProjection(const QString &text)
{
    ui->projectionLineEdit->setText(text);
}

void StatusBarWidget::setViewSize(const QString &text)
{
    ui->viewSizeLineEdit->setText(text);
}

void StatusBarWidget::setCurrentScale(const QString &text)
{
    ui->scaleComboBox->blockSignals(true);

    ui->scaleComboBox->setCurrentText(text);

    ui->scaleComboBox->blockSignals(false);
}

void StatusBarWidget::setRotation(double angle)
{
    ui->rotationSpinBox->blockSignals(true);

    ui->rotationSpinBox->setValue(qRound(angle * 10.0) / 10.0);

    ui->rotationSpinBox->blockSignals(false);
}

void StatusBarWidget::onRotationValueChanged(double value)
{
    emit rotationChanged(value);
}

void StatusBarWidget::onScaleValueChanged(const QString &value)
{
    emit scaleChanged(value);
}
