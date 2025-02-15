#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);  // 初始化Qt应用

    MainWindow mainWindow;  // 创建主窗口对象
    mainWindow.show();  // 显示主窗口

    return app.exec();  // 进入Qt的事件循环
}