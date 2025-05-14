#pragma once
#ifndef __GRAB_IMAGES2_HPP__
#define __GRAB_IMAGES2_HPP__

#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbCamera.h>
#include <opencv2/opencv.hpp>

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QString>
#include <limits>

class SBaslerCameraControl : public QObject
{
    Q_OBJECT
public:
    explicit SBaslerCameraControl(QObject *parent = nullptr);
    ~SBaslerCameraControl() override;

    enum SBaslerCameraControl_Type{
        Type_Basler_Freerun,                //设置相机的内触发
        Type_Basler_Line1,                  //设置相机的外触发
        Type_Basler_ExposureTimeAbs,        //设置相机的曝光时间
        Type_Basler_GainRaw,                //设置相机的增益
        Type_Basler_AcquisitionFrameRateAbs,//设置相机的频率
        Type_Basler_Width,                  //图片的宽度
        Type_Basler_Height,                 //图片的高度
        Type_Basler_LineSource,             //灯的触发信号
    };
    /* 基本接口 */
    void initSome();
    void deleteAll();
    QStringList cameras();
    int  OpenCamera(const QString& cameraSN);
    int CloseCamera();

    /* 参数接口 */
    void setExposureTime(double time);     // 设置曝光时间
    double getExposureTime();              // 获取曝光时间
    double getExposureTimeMin() {
        return 0.0;
    };                                     // 最小曝光时间
    double getExposureTimeMax() {
        return std::numeric_limits<double>::max();
    };                                     // 最大曝光时间

    void setFeatureTriggerSourceType(const QString& type); // 设置种类
    QString getFeatureTriggerSourceType();          // 获取种类：软触发、外触发等等
    void setFeatureTriggerModeType(bool on);        // 设置模式触发
    bool getFeatureTriggerModeType();               // 获取模式触发

    /* 采集接口 */
    long StartAcquire();                            // 开始采集
    long StopAcquire();                             // 结束采集
    long GrabImage(QImage& image,int timeout = 2000);
    
signals:
    void sigCameraUpdate(QStringList list);
    void sigSizeChange(QSize size);
    void sigCameraCount(int count);
    void sigCurrentImage(QImage img);

    private:
    void UpdateCameraList();
    void CopyToImage(Pylon::CGrabResultPtr pInBuffer, QImage &OutImage);
    void SetCamera(SBaslerCameraControl::SBaslerCameraControl_Type index, double tmpValue = 0.0); // 设置各种参数
    double GetCamera(SBaslerCameraControl::SBaslerCameraControl_Type index); // 获取各种参数

private slots:
    void onTimerGrabImage();

private:    
    Pylon::CInstantCamera m_basler;
    QStringList           m_cameralist;
    QString               m_currentMode   = "Freerun"; // 当前模式
    bool                  m_isOpenAcquire =  false;    // 是否开始采集
    bool                  m_isOpen        =  false;    // 是否打开摄像头
    QSize                 m_size;
};

#endif // __GRAB_IMAGES2_HPP__
