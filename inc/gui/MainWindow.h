// gui/include/MainWindow.h
#pragma once

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartAcquisition();
    void onImageProcessed();
    void onAnalysisCompleted();
    void onDataProcessed();
    void onDialRendered();

private:
    Ui::MainWindow *ui;
};
