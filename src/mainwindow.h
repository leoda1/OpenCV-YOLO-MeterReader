#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbCamera.h>
#include <QMainWindow>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

#include <QEvent>
#include <QDebug>
#include <QtGui>
#include <QtCore>
#include <QScreen>
#include <memory>

#include "settings.h"

namespace Ui {
class MainWindow;
}

enum ConvolutionType {
/* Return the full convolution, including border */
  CONVOLUTION_FULL,

/* Return only the part that corresponds to the original image */
  CONVOLUTION_SAME,

/* Return only the submatrix containing elements that were not influenced by the border */
  CONVOLUTION_VALID
};

using namespace cv;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void init();
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    Pylon::CInstantCamera m_camera;
    Pylon::CPylonUsbTLParams usbCameraParam;
    GenApi::INodeMap *m_nodemap;
    QString FullNameOfSelectedDevice;
    // Settings *saveSettings = new Settings();
    std::unique_ptr<Settings> saveSettings;
    QJsonObject json;
    int imageSaved = 0;
    bool isAlgoAreaOpened = false;

    QSize smallSize;
    QSize bigSize;

    void setButtons(bool inPreview);
    void setNoCamera();
    void readJson();
    void temporal_LSI();
    Mat spatial_LSI(Mat speckle,int m);

    // angel
    bool   m_hasZero   = false;
    double m_zeroAngle = 0.0;
    Mat m_lastRgb;

    bool grabOneFrame(cv::Mat &outBar);
    void runAlgoOnce();
    static void conv2(const Mat &img, const Mat& kernel, ConvolutionType type, Mat& dest);
    

private slots:
    void startPreview();
    void refresh();
    void setting();
    void about();
    void singleGrab();
    void multiGrab();
    void algoArea();
    void spatial_LSI_Matlab();
    void onCaptureZero();
    void onResetZero();

};

class highPreciseDetector {
private:
    cv::Mat m_image;
    cv::Mat m_visual;
    std::vector<cv::Vec3f> m_circles;
    std::vector<cv::Vec4i> m_lines;
    double m_angle;
    
public:
    explicit highPreciseDetector(const cv::Mat& image);
    ~highPreciseDetector() = default;

    const std::vector<cv::Vec3f>& getCircles() const { return m_circles; }
    const std::vector<cv::Vec4i>& getLine() const { return m_lines; }
    double getAngle() const { return m_angle; }
    void showScale1Result();
    cv::Mat visual() const { return m_visual; }
    
private:
    void detectCircles();
    void detectLines();
    void calculateAngle();
};

#endif // MAINWINDOW_H
