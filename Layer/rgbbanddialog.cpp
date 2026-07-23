//#include "rgbbanddialog.h"
//#include "ui_rgbbanddialog.h"

//#include <QDialogButtonBox>
//#include <QPushButton>

//#include "Layer/rgbbanddialog.h"

//RGBBandDialog::RGBBandDialog(QWidget *parent)
//    : QDialog(parent),
//      ui(new Ui::RGBBandDialog)
//{
//    ui->setupUi(this);

//    ui->rasterNameLineEdit->setReadOnly(true);
//    ui->bandCountLineEdit->setReadOnly(true);

//    connect(ui->buttonBox,
//            &QDialogButtonBox::clicked,
//            this,
//            &RGBBandDialog::onApplyClicked);

//    connect(ui->buttonBox->button(QDialogButtonBox::Apply),
//            &QPushButton::clicked,
//            this,
//            &RGBBandDialog::onApplyClicked);

//    connect(ui->buttonBox->button(QDialogButtonBox::Cancel),
//            &QPushButton::clicked,
//            this,
//            &RGBBandDialog::reject);
//}

//RGBBandDialog::~RGBBandDialog()
//{
//    delete ui;
//}

//void RGBBandDialog::setRasterName(const QString &name)
//{
//    ui->rasterNameLineEdit->setText(name);
//}

//void RGBBandDialog::setBandCount(int count)
//{
//    ui->bandCountLineEdit->setText(QString::number(count));

//    ui->redBandComboBox->clear();
//    ui->greenBandComboBox->clear();
//    ui->blueBandComboBox->clear();

//    for(int i=1;i<=count;i++)
//    {
//        QString text = QString("Band %1").arg(i);

//        ui->redBandComboBox->addItem(text,i);
//        ui->greenBandComboBox->addItem(text,i);
//        ui->blueBandComboBox->addItem(text,i);
//    }

//    if(count>=3)
//    {
//        ui->redBandComboBox->setCurrentIndex(0);
//        ui->greenBandComboBox->setCurrentIndex(1);
//        ui->blueBandComboBox->setCurrentIndex(2);
//    }
//    else if(count==2)
//    {
//        ui->redBandComboBox->setCurrentIndex(0);
//        ui->greenBandComboBox->setCurrentIndex(1);
//        ui->blueBandComboBox->setCurrentIndex(1);
//    }
//    else if(count==1)
//    {
//        ui->redBandComboBox->setCurrentIndex(0);
//        ui->greenBandComboBox->setCurrentIndex(0);
//        ui->blueBandComboBox->setCurrentIndex(0);
//    }
//}

//int RGBBandDialog::redBand() const
//{
//    return ui->redBandComboBox->currentData().toInt();
//}

//int RGBBandDialog::greenBand() const
//{
//    return ui->greenBandComboBox->currentData().toInt();
//}

//int RGBBandDialog::blueBand() const
//{
//    return ui->blueBandComboBox->currentData().toInt();
//}

//bool RGBBandDialog::stretchContrast() const
//{
//    return ui->stretchCheckBox->isChecked();
//}

//void RGBBandDialog::onApplyClicked()
//{
//    emit rgbBandsSelected(
//                redBand(),
//                greenBand(),
//                blueBand(),
//                stretchContrast());

//    accept();
//}
