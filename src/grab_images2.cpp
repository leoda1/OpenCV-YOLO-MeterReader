/*
    该程序使用Basler Pylon SDK实现以下功能：
    1. 初始化 (initializeCamera)

    2. 触发时拍摄并保存图像
    3. 可以连续触发
    4. 修改相机的超参数配置
    5. 可以触发关闭相机
    6. 当相机连接失败时，删除相机设备并重新连接
*/
#include "grab_images2.h"
#include <QDateTime>    // 包含 Qt 的时间日期库
#include <QDebug>       // 包含 Qt 的调试库
#include <QString>

SBaslerCameraControl::SBaslerCameraControl(QObject *parent) : QObject(parent)  // 构造函数
{
}

SBaslerCameraControl::~SBaslerCameraControl()  // 析构函数
{
}

void SBaslerCameraControl::initSome()  // 初始化摄像头控制
{
    PylonInitialize();  // 初始化 Pylon 库
    qDebug() << "Pylon Initialized!";
    CTlFactory &TlFactory = CTlFactory::GetInstance();  // 获取 TlFactory 实例
    TlInfoList_t lstInfo;  // 摄像头信息列表
    int n = TlFactory.EnumerateTls(lstInfo);  // 枚举所有连接的传输层设备

    TlInfoList_t::const_iterator it;
    for ( it = lstInfo.begin(); it != lstInfo.end(); ++it ) {  // 遍历所有设备
        qDebug() << "FriendlyName: " << it->GetFriendlyName() << "FullName: " << it->GetFullName();  // 打印设备的友好名称和完整名称
        qDebug() << "VendorName: " << it->GetVendorName() << "DeviceClass: " << it->GetDeviceClass();  // 打印供应商名称和设备类型
    }
    UpdateCameraList();  // 更新摄像头列表
    emit sigCameraCount(n);  // 发送信号，通知摄像头数量
    qDebug() << "Basler Camera Count: " << n;
}

void SBaslerCameraControl::deleteAll()  // 删除所有摄像头控制
{
    if(m_isOpenAcquire) {  // 如果摄像头正在采集
        StopAcquire();  // 停止采集
    }
    CloseCamera();  // 关闭摄像头
    qDebug() << "SBaslerCameraControl deleteAll: PylonTerminate" ;
    PylonTerminate();  // 终止 Pylon 库
    qDebug() << "SBaslerCameraControl deleteAll: Close" ;
}

QStringList SBaslerCameraControl::cameras()  // 获取摄像头列表
{
    return m_cameralist;  // 返回摄像头列表
}

void SBaslerCameraControl::UpdateCameraList()  // 更新摄像头列表
{
    m_cameralist.clear(); // 清空列表，避免累积
    CTlFactory& TLFactory = CTlFactory::GetInstance();  // 获取传输层工厂实例
    std::unique_ptr<ITransportLayer> pTl(TLFactory.CreateTl("BaslerUsb"));  // 创建传输层实例
    DeviceInfoList_t devices;  // 存储设备信息的列表
    int n = pTl->EnumerateDevices(devices);  // 枚举设备
    
    CInstantCameraArray cameraArray(devices.size());  // 创建摄像头数组
    if(n == 0) {  // 如果没有找到摄像头
        qDebug() << "Cannot find Any camera!";  // 打印错误信息
        return;
    }
    for (int i=0 ; i < cameraArray.GetSize(); i++) {  // 遍历所有设备
        cameraArray[i].Attach(TLFactory.CreateDevice(devices[i]));
        string sn = cameraArray[i].GetDeviceInfo().GetSerialNumber();
        qDebug() << "Serial Number: " << QString::fromStdString(sn);
        m_cameralist << QString::fromStdString(sn);
    }
    emit sigCameraUpdate(m_cameralist);  // 发送摄像头更新信号
}

void SBaslerCameraControl::CopyToImage(CGrabResultPtr pInBuffer, QImage &OutImage)  // 将图像数据复制到 QImage 对象
{
    try {
        uchar* buff = (uchar*)pInBuffer->GetBuffer();  // 获取图像数据缓冲区
        int nHeight = pInBuffer->GetHeight();  // 获取图像高度
        int nWidth = pInBuffer->GetWidth();  // 获取图像宽度
        if(m_size != QSize(nWidth, nHeight)) {  // 如果图像尺寸发生变化
            m_size = QSize(nWidth, nHeight);  // 更新图像尺寸
            emit sigSizeChange(m_size);  // 发送尺寸变化信号
        }
        QImage imgBuff(buff, nWidth, nHeight, QImage::Format_Indexed8);  // 创建 QImage 对象
        OutImage = imgBuff;  // 赋值给输出图像
        if(pInBuffer->GetPixelType() == PixelType_Mono8) {  // 如果图像类型是单通道灰度图像
            uchar* pCursor = OutImage.bits();  // 获取 QImage 的数据指针
            if ( OutImage.bytesPerLine() != nWidth ) {  // 如果行字节数不等于图像宽度
                for ( int y=0; y<nHeight; ++y ) {  // 遍历每一行
                    pCursor = OutImage.scanLine( y );  // 获取当前行指针
                    for ( int x=0; x<nWidth; ++x ) {  // 遍历每一列
                        *pCursor =* buff;  // 将缓冲区的图像数据复制到 QImage 中
                        ++pCursor;
                        ++buff;
                    }
                }
            } else {
                memcpy( OutImage.bits(), buff, nWidth * nHeight );  // 直接复制图像数据
            }
        }
    } catch (GenICam::GenericException &e) {  // 捕获异常
        qDebug() << "CopyToImage Error:" << QString(e.GetDescription());  // 打印错误信息
    }
}

void SBaslerCameraControl::onTimerGrabImage()  // 定时从摄像头抓取图像
{
    if(m_isOpenAcquire) {  // 如果摄像头正在采集
        QImage image;  // 创建图像对象
        GrabImage(image, 5);  // 从摄像头获取图像
        if(!image.isNull()) {  // 如果图像有效
            emit sigCurrentImage(image);  // 发送当前图像信号
        }
        QTimer::singleShot(5, this, SLOT(onTimerGrabImage()));  // 5ms 后再次执行抓图
    }
}

int SBaslerCameraControl::OpenCamera(QString cameraSN)  // 打开指定序列号的摄像头
{
    try {
        CDeviceInfo cInfo;  // 创建设备信息对象
        String_t str = String_t(cameraSN.toStdString().c_str());  // 转换序列号为字符串
        cInfo.SetSerialNumber(str);  // 设置序列号
        m_basler.Attach(CTlFactory::GetInstance().CreateDevice(cInfo));  // 创建并附加摄像头设备
        m_basler.Open();  // 打开摄像头
        getFeatureTriggerSourceType();  // 获取触发源类型
        m_isOpen = true;  // 设置为已打开状态
    } catch (GenICam::GenericException &e) {  // 捕获异常
        qDebug() << "OpenCamera Error" << QString(e.GetDescription());  // 打印错误信息
        m_isOpen = false;  // 设置为未打开状态
        return -2;  // 返回错误代码
    }
    return 0;  // 返回成功
}

int SBaslerCameraControl::CloseCamera()  // 关闭摄像头
{
    if(!m_isOpen) {  // 如果摄像头未打开
        return -1;  // 返回错误代码
    }
    try {
        if(m_basler.IsOpen()) {  // 如果摄像头已打开
            m_basler.DetachDevice();  // 分离设备
            m_basler.Close();  // 关闭设备
        }
    } catch (GenICam::GenericException &e) {  // 捕获异常
        qDebug() << "CloseCamera Error:" << QString(e.GetDescription());  // 打印错误信息
        return -2;  // 返回错误代码
    }
    return 0;  // 返回成功
}

void SBaslerCameraControl::setExposureTime(double time)  // 设置曝光时间
{
    SetCamera(Type_Basler_ExposureTimeAbs, time);  // 设置摄像头曝光时间
}

int SBaslerCameraControl::getExposureTime()  // 获取曝光时间
{
    return QString::number(GetCamera(Type_Basler_ExposureTimeAbs)).toInt();  // 获取并返回摄像头的曝光时间
}

int SBaslerCameraControl::getExposureTimeMin()  // 获取最小曝光时间
{
    return DOUBLE_MIN;  // 返回最小曝光时间
}

int SBaslerCameraControl::getExposureTimeMax()  // 获取最大曝光时间
{
    return DOUBLE_MAX;  // 返回最大曝光时间
}

void SBaslerCameraControl::setFeatureTriggerSourceType(QString type)  // 设置触发源类型
{
    if(m_isOpenAcquire) {  // 如果摄像头正在采集
        StopAcquire();  // 停止采集
    }
    if(type == "Freerun") {  // 如果类型是自由运行
        SetCamera(Type_Basler_Freerun);  // 设置为自由运行模式
    } else if(type == "Line1") {  // 如果类型是 Line1
        SetCamera(Type_Basler_Line1);  // 设置为 Line1 触发模式
    }
}

QString SBaslerCameraControl::getFeatureTriggerSourceType()  // 获取当前触发源类型
{
    INodeMap &cameraNodeMap = m_basler.GetNodeMap();  // 获取摄像头的节点映射
    CEnumerationPtr  ptrTriggerSel = cameraNodeMap.GetNode("TriggerSelector");  // 获取触发选择节点
    ptrTriggerSel->FromString("FrameStart");  // 设置为帧开始触发
    CEnumerationPtr  ptrTrigger = cameraNodeMap.GetNode("TriggerMode");  // 获取触发模式节点
    ptrTrigger->SetIntValue(1);  // 设置触发模式为启用
    CEnumerationPtr  ptrTriggerSource = cameraNodeMap.GetNode("TriggerSource");  // 获取触发源节点

    String_t str = ptrTriggerSource->ToString();  // 获取当前触发源的字符串表示
    m_currentMode = QString::fromLocal8Bit(str.c_str());  // 保存当前模式
    return m_currentMode;  // 返回当前触发源类型
}

void SBaslerCameraControl::setFeatureTriggerModeType(bool on)  // 设置触发模式类型
{
    INodeMap &cameraNodeMap = m_basler.GetNodeMap();  // 获取摄像头的节点映射
    CEnumerationPtr  ptrTriggerSel = cameraNodeMap.GetNode("TriggerSelector");  // 获取触发选择节点
    ptrTriggerSel->FromString("FrameStart");  // 设置为帧开始触发
    CEnumerationPtr  ptrTrigger = cameraNodeMap.GetNode("TriggerMode");  // 获取触发模式节点
    ptrTrigger->SetIntValue(on ? 1 : 0);  // 根据传入的参数设置触发模式
}

bool SBaslerCameraControl::getFeatureTriggerModeType()  // 获取当前触发模式类型
{
    INodeMap &cameraNodeMap = m_basler.GetNodeMap();  // 获取摄像头的节点映射
    CEnumerationPtr  ptrTriggerSel = cameraNodeMap.GetNode("TriggerSelector");  // 获取触发选择节点
    ptrTriggerSel->FromString("FrameStart");  // 设置为帧开始触发
    CEnumerationPtr  ptrTrigger = cameraNodeMap.GetNode("TriggerMode");  // 获取触发模式节点
    return ptrTrigger->GetIntValue() == 1;  // 返回当前触发模式是否为启用
}

void SBaslerCameraControl::SetCamera(SBaslerCameraControl::SBaslerCameraControl_Type index, double tmpValue)  // 设置摄像头的特定参数
{
    INodeMap &cameraNodeMap = m_basler.GetNodeMap();  // 获取摄像头的节点映射
    switch (index) {  // 根据传入的类型选择设置的参数
    case Type_Basler_Freerun: {
        CEnumerationPtr  ptrTriggerSel = cameraNodeMap.GetNode("TriggerSelector");  // 获取触发选择节点
        ptrTriggerSel->FromString("FrameStart");  // 设置为帧开始触发
        CEnumerationPtr  ptrTrigger = cameraNodeMap.GetNode("TriggerMode");  // 获取触发模式节点
#ifdef Real_Freerun
        ptrTrigger->SetIntValue(0);  // 设置为自由运行模式
#else //Software
        ptrTrigger->SetIntValue(1);  // 设置为软件触发模式
        CEnumerationPtr  ptrTriggerSource = cameraNodeMap.GetNode("TriggerSource");  // 获取触发源节点
        ptrTriggerSource->FromString("Software");  // 设置触发源为软件触发
#endif
    } break;
    case Type_Basler_Line1: {
        CEnumerationPtr  ptrTriggerSel = cameraNodeMap.GetNode("TriggerSelector");  // 获取触发选择节点
        ptrTriggerSel->FromString("FrameStart");  // 设置为帧开始触发
        CEnumerationPtr  ptrTrigger = cameraNodeMap.GetNode("TriggerMode");  // 获取触发模式节点
        ptrTrigger->SetIntValue(1);  // 设置为启用触发
        CEnumerationPtr  ptrTriggerSource = cameraNodeMap.GetNode("TriggerSource");  // 获取触发源节点
        ptrTriggerSource->FromString("Line1");  // 设置触发源为 Line1
    } break;
    case Type_Basler_ExposureTimeAbs: {
        const CFloatPtr exposureTime = cameraNodeMap.GetNode("ExposureTimeAbs");  // 获取曝光时间节点
        exposureTime->SetValue(tmpValue);  // 设置曝光时间
    } break;
    case Type_Basler_GainRaw: {
        const CIntegerPtr cameraGen = cameraNodeMap.GetNode("GainRaw");  // 获取增益节点
        cameraGen->SetValue(tmpValue);  // 设置增益值
    } break;
    case Type_Basler_AcquisitionFrameRateAbs: {
        const CBooleanPtr frameRate = cameraNodeMap.GetNode("AcquisitionFrameRateEnable");  // 获取帧率启用节点
        frameRate->SetValue(TRUE);  // 启用帧率
        const CFloatPtr frameRateABS = cameraNodeMap.GetNode("AcquisitionFrameRateAbs");  // 获取帧率节点
        frameRateABS->SetValue(tmpValue);  // 设置帧率
    } break;
    case Type_Basler_Width: {
        const CIntegerPtr widthPic = cameraNodeMap.GetNode("Width");  // 获取图像宽度节点
        widthPic->SetValue(tmpValue);  // 设置图像宽度
    } break;
    case Type_Basler_Height: {
        const CIntegerPtr heightPic = cameraNodeMap.GetNode("Height");  // 获取图像高度节点
        heightPic->SetValue(tmpValue);  // 设置图像高度
    } break;
    case Type_Basler_LineSource: {
        CEnumerationPtr  ptrLineSource = cameraNodeMap.GetNode("LineSource");  // 获取行源节点
        ptrLineSource->SetIntValue(2);  // 设置为行源 2
    } break;
    default:
        break;  // 默认不进行任何操作
    }
}

double SBaslerCameraControl::GetCamera(SBaslerCameraControl::SBaslerCameraControl_Type index)  // 获取摄像头的特定参数
{
    INodeMap &cameraNodeMap = m_basler.GetNodeMap();  // 获取摄像头的节点映射
    switch (index) {  // 根据传入的类型选择获取的参数
    case Type_Basler_ExposureTimeAbs: {
        const CFloatPtr exposureTime = cameraNodeMap.GetNode("ExposureTimeAbs");  // 获取曝光时间节点
        return exposureTime->GetValue();  // 返回曝光时间
    } break;
    case Type_Basler_GainRaw: {
        const CIntegerPtr cameraGen = cameraNodeMap.GetNode("GainRaw");  // 获取增益节点
        return cameraGen->GetValue();  // 返回增益值
    } break;
    case Type_Basler_AcquisitionFrameRateAbs: {
        const CBooleanPtr frameRate = cameraNodeMap.GetNode("AcquisitionFrameRateEnable");  // 获取帧率启用节点
        frameRate->SetValue(TRUE);  // 启用帧率
        const CFloatPtr frameRateABS = cameraNodeMap.GetNode("AcquisitionFrameRateAbs");  // 获取帧率节点
        return frameRateABS->GetValue();  // 返回帧率
    } break;
    case Type_Basler_Width: {
        const CIntegerPtr widthPic = cameraNodeMap.GetNode("Width");  // 获取图像宽度节点
        return widthPic->GetValue();  // 返回图像宽度
    } break;
    case Type_Basler_Height: {
        const CIntegerPtr heightPic = cameraNodeMap.GetNode("Height");  // 获取图像高度节点
        return heightPic->GetValue();  // 返回图像高度
    } break;
    default:
        return -1;  // 默认返回 -1
        break;
    }
}

long SBaslerCameraControl::StartAcquire()  // 开始图像采集
{
    m_isOpenAcquire = true;  // 标记为开始采集
    try {
        qDebug() << "SBaslerCameraControl StartAcquire" << m_currentMode;  // 打印当前的采集模式
        if(m_currentMode == "Freerun") {  // 如果当前模式是自由运行
            m_basler.StartGrabbing(GrabStrategy_LatestImageOnly, GrabLoop_ProvidedByInstantCamera);  // 启动采集，使用最新图像策略
        } else if(m_currentMode == "Software") {  // 如果当前模式是软件触发
            m_basler.StartGrabbing(GrabStrategy_LatestImageOnly);  // 启动采集，使用最新图像策略
            onTimerGrabImage();  // 启动定时器抓取图像
        } else if(m_currentMode == "Line1") {  // 如果当前模式是 Line1 触发
            m_basler.StartGrabbing(GrabStrategy_OneByOne);  // 启动采集，按一张图像一张图像抓取
            onTimerGrabImage();  // 启动定时器抓取图像
        } else if(m_currentMode == "Line2") {  // 如果当前模式是 Line2 触发
            m_basler.StartGrabbing(GrabStrategy_OneByOne);  // 启动采集，按一张图像一张图像抓取
        }
    } catch (GenICam::GenericException &e) {  // 捕获异常
        qDebug() << "StartAcquire Error:" << QString(e.GetDescription());  // 打印错误信息
        return -2;  // 返回错误代码
    }
    return 0;  // 返回成功
}

long SBaslerCameraControl::StopAcquire()  // 停止图像采集
{
    m_isOpenAcquire = false;  // 标记为停止采集
    qDebug() << "SBaslerCameraControl StopAcquire";  // 打印停止采集信息
    try {
        if (m_basler.IsGrabbing()) {  // 如果摄像头正在采集
            m_basler.StopGrabbing();  // 停止采集
        }
    } catch (GenICam::GenericException &e) {  // 捕获异常
        qDebug() << "StopAcquire Error:" << QString(e.GetDescription());  // 打印错误信息
        return -2;  // 返回错误代码
    }
    return 0;  // 返回成功
}

long SBaslerCameraControl::GrabImage(QImage &image, int timeout)  // 从摄像头抓取图像
{
    try {
        if (!m_basler.IsGrabbing()) {  // 如果摄像头没有在抓取图像
            StartAcquire();  // 启动采集
        }
        CGrabResultPtr ptrGrabResult;  // 创建图像抓取结果对象
        if(m_currentMode == "Freerun") {  // 如果当前模式是自由运行
        } else if(m_currentMode == "Software") {  // 如果当前模式是软件触发
            if (m_basler.WaitForFrameTriggerReady(1000, TimeoutHandling_Return)) {  // 等待帧触发准备好
                m_basler.ExecuteSoftwareTrigger();  // 执行软件触发
                m_basler.RetrieveResult(timeout, ptrGrabResult, TimeoutHandling_Return);  // 获取图像抓取结果
            }
        } else if(m_currentMode == "Line1") {  // 如果当前模式是 Line1 触发
            m_basler.RetrieveResult(timeout, ptrGrabResult, TimeoutHandling_Return);  // 获取图像抓取结果
        } else if(m_currentMode == "Line2") {  // 如果当前模式是 Line2 触发
            m_basler.RetrieveResult(timeout, ptrGrabResult, TimeoutHandling_Return);  // 获取图像抓取结果
        }
        if(ptrGrabResult == NULL) {  // 如果抓取结果为空
            return -5;  // 返回错误代码
        }
        if (ptrGrabResult->GrabSucceeded()) {  // 如果抓取成功
            qDebug() << "what: ptrGrabResult GrabSucceeded";  // 打印成功信息
            if (!ptrGrabResult.IsValid()) {  // 如果抓取结果无效
                OutputDebugString("GrabResult not Valid Error\n");  // 打印无效结果错误
                return -1;  // 返回错误代码
            }
            EPixelType pixelType = ptrGrabResult->GetPixelType();  // 获取图像像素类型
            switch (pixelType) {  // 根据像素类型进行处理
            case PixelType_Mono8: {  // 如果是单通道灰度图像
                CopyToImage(ptrGrabResult, image);  // 复制图像数据到 QImage
            } break;
            case PixelType_BayerRG8: {  // 如果是 Bayer 格式图像
                qDebug() << "what: PixelType_BayerRG8";  // 打印信息
            } break;
            default:
                qDebug() << "what: default";  // 默认情况
                break;
            }
        } else {  // 如果抓取失败
            qDebug() << "Grab Error!!!";  // 打印错误信息
            return -3;  // 返回错误代码
        }
    } catch (GenICam::GenericException &e) {  // 捕获异常
        qDebug() << "GrabImage Error:" << QString(e.GetDescription());  // 打印错误信息
        return -2;  // 返回错误代码
    } catch(...) {  // 捕获其他未知异常
        OutputDebugString("ZP 11 Shot GetParam Try 12 No know Error\n");  // 打印错误信息
        return -1;  // 返回错误代码
    }
    return 0;  // 返回成功
}
