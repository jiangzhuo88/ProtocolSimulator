#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Windows"));
//    QString appSkinPath = "./Black/CPFBase_Black.qss";
//    QFile file(appSkinPath);
//    QString appSkin;
//    if(file.open(QIODevice::ReadOnly))
//    {
//        //qDebug()<<"111111111"<<endl;
//        appSkin = QString(file.readAll().data());
//    }

    MainWindow w;
//    w.setStyleSheet(appSkin);
    w.show();


    return a.exec();
}
