#include "mainwindow.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    
    w.show();
    qDebug() << "Qt GUI should be visible now!";
    return a.exec();
}
