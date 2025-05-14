#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "grab_images2.h"
#include <QtGui>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pushButton_getExTime_clicked();
    void on_pushButton_SetExTime_clicked();
    void on_pushButton_SetMode_clicked();
    void on_pushButton_GetMode_clicked();
    void on_pushButton_CFMode_clicked();
    void on_comboBox_CFMode_activated(int index);
    void on_pushButton_Start_clicked();

    void slotUpdateCurrentImage(const QImage& img);
    void slotCameraSizeChanged(const QSize& size);
private:
    void initConnections();
    Ui::MainWindow *ui = nullptr;
    std::unique_ptr<SBaslerCameraControl> m_control; // 相机控制
    bool m_acquiring = false;                        // 是否正在采集
};

#endif // MAINWINDOW_H
