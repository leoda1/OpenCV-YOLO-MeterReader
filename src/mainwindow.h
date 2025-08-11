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
#include <QComboBox>
#include <QLabel>
#include <memory>

#include "settings.h"
#include "dialmarkdialog.h"
#include "errortabledialog.h"

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

// 指针识别配置结构
struct PointerDetectionConfig {
    // 圆形检测参数
    double dp = 1.0;                    // HoughCircles的累加器分辨率
    double minDist = 100;               // 圆心之间的最小距离
    double param1 = 100;                // Canny边缘检测的高阈值
    double param2 = 30;                 // 圆心检测的累加器阈值
    int minRadius = 50;                 // 最小圆半径
    int maxRadius = 0;                  // 最大圆半径（0表示不限制）
    
    // 直线检测参数
    double rho = 1.0;                   // 距离分辨率
    double theta = CV_PI/180;           // 角度分辨率
    int threshold = 50;                 // 累加器阈值
    double minLineLength = 30;          // 最小线段长度
    double maxLineGap = 10;             // 最大线段间隙
    
    // Canny边缘检测参数
    double cannyLow = 50;               // Canny低阈值
    double cannyHigh = 150;             // Canny高阈值
    
    // 指针识别特定参数
    bool usePointerFromCenter = true;   // 是否从圆心开始识别指针
    double pointerSearchRadius = 0.9;   // 指针搜索半径比例（相对于表盘半径）
    int pointerMinLength = 50;          // 指针最小长度
    double angleOffset = 0.0;           // 角度偏移量
};

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
    int currentCapturedCount = 0;
    bool isAlgoAreaOpened = true;  // 默认展开识别角度区域

    QSize smallSize;
    QSize bigSize;
    
    // 表盘类型选择相关
    QComboBox *m_dialTypeCombo;
    QLabel *m_dialTypeLabel;
    QString m_currentDialType;
    
    // 指针识别配置
    PointerDetectionConfig m_yyqyConfig;  // YYQY表盘配置
    PointerDetectionConfig m_byqConfig;   // BYQ表盘配置
    PointerDetectionConfig* m_currentConfig;  // 当前使用的配置指针

    void setButtons(bool inPreview);
    void setNoCamera();
    void readJson();
    void temporal_LSI();
    Mat spatial_LSI(Mat speckle,int m);
    void updateCollectionDisplay();
    void setupDialTypeSelector();
    void setupExpandedLayout();  // 设置展开的布局
    void initPointerConfigs();   // 初始化指针识别配置
    void switchPointerConfig(const QString& dialType);  // 切换指针识别配置

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
    void showDialMarkDialog();
    void showErrorTableDialog();
    void onDialTypeChanged(const QString &dialType);

};

class highPreciseDetector {
private:
    cv::Mat m_image;
    cv::Mat m_visual;
    std::vector<cv::Vec3f> m_circles;
    std::vector<cv::Vec4i> m_lines;
    double m_angle;
    const PointerDetectionConfig* m_config;  // 配置参数指针
    
public:
    explicit highPreciseDetector(const cv::Mat& image, const PointerDetectionConfig* config = nullptr);
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
    void detectPointerFromCenter();  // 从圆心开始检测指针的新方法
};

#endif // MAINWINDOW_H
