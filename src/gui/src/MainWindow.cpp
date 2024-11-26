// gui/src/MainWindow.cpp
#include "../../inc/gui/MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 连接信号和槽
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartAcquisition);
    // 其他连接...
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onStartAcquisition()
{
    // 调用数据采集模块的函数
}

void MainWindow::onImageProcessed()
{
    // 更新界面显示
}

// 其他槽函数...
