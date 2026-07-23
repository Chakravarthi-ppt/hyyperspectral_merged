#include "UI/MainWindow/mainwindow.h"

#include <QApplication>
#include "gdal.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GDALAllRegister();
    MainWindow w;
    w.show();
    return a.exec();
}
