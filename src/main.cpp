#include "mainwindow.h"
#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    Pylon::PylonAutoInitTerm autoInitTerm;

    QApplication a(argc, argv);
    
    // 设置全局字体大小
    QFont font = a.font();
    font.setPointSize(12);
    a.setFont(font);
    
    MainWindow w;

    w.show();
    w.init();
    return a.exec();
}
