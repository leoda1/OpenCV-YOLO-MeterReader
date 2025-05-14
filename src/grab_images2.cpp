#include "grab_images2.h"
#include <QDateTime>
#include <QDebug>

SBaslerCameraControl::SBaslerCameraControl(QObject *parent)
    : QObject(parent)
{
}

SBaslerCameraControl::~SBaslerCameraControl()
{
    deleteAll();
}

void SBaslerCameraControl::initSome() {
    Pylon::PylonInitialize();
    qDebug() << "Pylon Initialized";

    UpdateCameraList();
    emit sigCameraCount(static_cast<int>(m_cameralist.size()));
}

void SBaslerCameraControl::deleteAll()
{
    if (m_isOpenAcquire) StopAcquire();
    CloseCamera();
    Pylon::PylonTerminate();
}

QStringList SBaslerCameraControl::cameras()    // 获取摄像头列表
{
    return m_cameralist;
}

void SBaslerCameraControl::UpdateCameraList()  // 更新摄像头列表
{
     m_cameralist.clear();
    Pylon::CTlFactory& TLFactory = Pylon::CTlFactory::GetInstance();
    Pylon::DeviceInfoList_t devices;

    if (TLFactory.EnumerateDevices(devices) == 0) {
        qWarning() << "No Basler camera detected";
        return;
    }

    for (const auto& devInfo : devices) {
        m_cameralist << QString::fromStdString(devInfo.GetSerialNumber().c_str());
    }
    emit sigCameraUpdate(m_cameralist);
}

int SBaslerCameraControl::OpenCamera(const QString& cameraSN)  // 打开指定序列号的摄像头
{
    try {
        Pylon::CDeviceInfo info;
        info.SetSerialNumber(Pylon::String_t(cameraSN.toStdString().c_str()));
        m_basler.Attach(Pylon::CTlFactory::GetInstance().CreateDevice(info));
        m_basler.Open();

        /* 复位 ROI，确保取整幅图像而不是左上角 */
        GenApi::INodeMap& n = m_basler.GetNodeMap();
        auto offsetX  = GenApi::CIntegerPtr(n.GetNode("OffsetX"));
        auto offsetY  = GenApi::CIntegerPtr(n.GetNode("OffsetY"));
        auto width    = GenApi::CIntegerPtr(n.GetNode("Width"));
        auto height   = GenApi::CIntegerPtr(n.GetNode("Height"));
        auto widthMax = GenApi::CIntegerPtr(n.GetNode("WidthMax"));
        auto heightMax= GenApi::CIntegerPtr(n.GetNode("HeightMax"));

        if (GenApi::IsWritable(offsetX))  offsetX->SetValue(0);
        if (GenApi::IsWritable(offsetY))  offsetY->SetValue(0);
        if (GenApi::IsWritable(width))    width->SetValue(widthMax->GetValue());
        if (GenApi::IsWritable(height))   height->SetValue(heightMax->GetValue());

        /* 统一输出像素格式为 BGR8packed，后面用 Qt 直接显示 */
        GenApi::CEnumerationPtr pixelFormat(n.GetNode("PixelFormat"));
        if (GenApi::IsWritable(pixelFormat))
            pixelFormat->FromString("BGR8");

        m_isOpen = true;
        m_currentMode = "Freerun";
    }
    catch (GenICam::GenericException& e) {
        qCritical() << "OpenCamera Error:" << e.GetDescription();
        m_isOpen = false;
        return -2;
    }
    return 0;
}

int SBaslerCameraControl::CloseCamera()
{
    if (!m_isOpen) return -1;

    try {
        if (m_basler.IsOpen())
            m_basler.Close();
    }
    catch (GenICam::GenericException& e) {
        qCritical() << "CloseCamera Error:" << e.GetDescription();
        return -2;
    }
    m_isOpen = false;
    return 0;
}

void SBaslerCameraControl::setExposureTime(double time)
{
    SetCamera(Type_Basler_ExposureTimeAbs, time);
}

double SBaslerCameraControl::getExposureTime()
{
    return GetCamera(Type_Basler_ExposureTimeAbs);
}

void SBaslerCameraControl::setFeatureTriggerSourceType(const QString& type)
{
    if (m_isOpenAcquire) StopAcquire();

    if (type.compare("Freerun", Qt::CaseInsensitive) == 0)
        SetCamera(Type_Basler_Freerun);
    else if (type.compare("Line1", Qt::CaseInsensitive) == 0)
        SetCamera(Type_Basler_Line1);
}

QString SBaslerCameraControl::getFeatureTriggerSourceType()
{
    return m_currentMode;
}

void SBaslerCameraControl::setFeatureTriggerModeType(bool on)
{
    GenApi::INodeMap& n = m_basler.GetNodeMap();
    GenApi::CEnumerationPtr triggerSel(n.GetNode("TriggerSelector"));
    triggerSel->FromString("FrameStart");
    GenApi::CEnumerationPtr triggerMode(n.GetNode("TriggerMode"));
    triggerMode->SetIntValue(on ? 1 : 0);
}

bool SBaslerCameraControl::getFeatureTriggerModeType()
{
    GenApi::INodeMap& n = m_basler.GetNodeMap();
    GenApi::CEnumerationPtr triggerSel(n.GetNode("TriggerSelector"));
    triggerSel->FromString("FrameStart");
    GenApi::CEnumerationPtr triggerMode(n.GetNode("TriggerMode"));
    return triggerMode->GetIntValue() == 1;
}

/* ---------------------------- 拍摄与采集 ---------------------------- */
long SBaslerCameraControl::StartAcquire()
{
    if (!m_isOpen) return -1;
    if (m_isOpenAcquire) return 0;

    try {
        m_basler.StartGrabbing(Pylon::GrabStrategy_LatestImageOnly, Pylon::GrabLoop_ProvidedByInstantCamera);
        m_isOpenAcquire = true;
        onTimerGrabImage();                 // 启动定时抓图
    }
    catch (GenICam::GenericException& e) {
        qCritical() << "StartAcquire Error:" << e.GetDescription();
        return -2;
    }
    return 0;
}

long SBaslerCameraControl::StopAcquire()
{
    m_isOpenAcquire = false;
    try {
        if (m_basler.IsGrabbing())
            m_basler.StopGrabbing();
    }
    catch (GenICam::GenericException& e) {
        qCritical() << "StopAcquire Error:" << e.GetDescription();
        return -2;
    }
    return 0;
}

void SBaslerCameraControl::onTimerGrabImage()
{
    if (!m_isOpenAcquire) return;

    QImage img;
    if (GrabImage(img, 1000) == 0 && !img.isNull())
        emit sigCurrentImage(img);

    // 约 30 fps，调整可改
    QTimer::singleShot(33, this, &SBaslerCameraControl::onTimerGrabImage);
}

long SBaslerCameraControl::GrabImage(QImage &image, int timeout)
{
    if (!m_isOpen) return -1;

    try {
        Pylon::CGrabResultPtr ptr;
        if (!m_basler.RetrieveResult(timeout, ptr, Pylon::TimeoutHandling_Return) || !ptr.IsValid())
            return -3;

        if (ptr->GrabSucceeded())
            CopyToImage(ptr, image);
        else
            return -4;
    }
    catch (GenICam::GenericException& e) {
        qCritical() << "GrabImage Error:" << e.GetDescription();
        return -2;
    }
    return 0;
}

/* ---------------------------- 工具函数 ---------------------------- */
void SBaslerCameraControl::CopyToImage(Pylon::CGrabResultPtr pInBuffer, QImage &OutImage)
{
    try {
        Pylon::CPylonImage pylImg;
        Pylon::CImageFormatConverter conv;
        conv.OutputPixelFormat = Pylon::PixelType_BGR8packed;
        conv.Convert(pylImg, pInBuffer);

        const int w = static_cast<int>(pylImg.GetWidth());
        const int h = static_cast<int>(pylImg.GetHeight());

        if (m_size != QSize(w, h)) {
            m_size = QSize(w, h);
            emit sigSizeChange(m_size);
        }

        OutImage = QImage(static_cast<const uchar*>(pylImg.GetBuffer()),
                          w, h, QImage::Format_BGR888).copy();      // detach
    }
    catch (GenICam::GenericException& e) {
        qCritical() << "CopyToImage Error:" << e.GetDescription();
        OutImage = QImage();
    }
}

/* ---------------------------- Set / Get 通用 ---------------------------- */
void SBaslerCameraControl::SetCamera(SBaslerCameraControl::SBaslerCameraControl_Type index, double val)
{
    using namespace GenApi;
    INodeMap& n = m_basler.GetNodeMap();
    switch (index) {
    case Type_Basler_Freerun:
        CEnumerationPtr(n.GetNode("TriggerSelector"))->FromString("FrameStart");
        CEnumerationPtr(n.GetNode("TriggerMode"))->SetIntValue(0);
        m_currentMode = "Freerun";
        break;

    case Type_Basler_Line1:
        CEnumerationPtr(n.GetNode("TriggerSelector"))->FromString("FrameStart");
        CEnumerationPtr(n.GetNode("TriggerMode"))->SetIntValue(1);
        CEnumerationPtr(n.GetNode("TriggerSource"))->FromString("Line1");
        m_currentMode = "Line1";
        break;

    case Type_Basler_ExposureTimeAbs:
        CFloatPtr(n.GetNode("ExposureTimeAbs"))->SetValue(val);
        break;

    case Type_Basler_GainRaw:
        CIntegerPtr(n.GetNode("GainRaw"))->SetValue(static_cast<int64_t>(val));
        break;

    case Type_Basler_AcquisitionFrameRateAbs:
        CBooleanPtr(n.GetNode("AcquisitionFrameRateEnable"))->SetValue(true);
        CFloatPtr(n.GetNode("AcquisitionFrameRateAbs"))->SetValue(val);
        break;

    case Type_Basler_Width:
        CIntegerPtr(n.GetNode("Width"))->SetValue(static_cast<int64_t>(val));
        break;

    case Type_Basler_Height:
        CIntegerPtr(n.GetNode("Height"))->SetValue(static_cast<int64_t>(val));
        break;

    case Type_Basler_LineSource:
        CEnumerationPtr(n.GetNode("LineSource"))->FromString("ExposureActive");
        break;
    }
}

double SBaslerCameraControl::GetCamera(SBaslerCameraControl::SBaslerCameraControl_Type index)
{
    using namespace GenApi;
    INodeMap& n = m_basler.GetNodeMap();
    switch (index) {
    case Type_Basler_ExposureTimeAbs:
        return CFloatPtr(n.GetNode("ExposureTimeAbs"))->GetValue();
    case Type_Basler_GainRaw:
        return CIntegerPtr(n.GetNode("GainRaw"))->GetValue();
    case Type_Basler_AcquisitionFrameRateAbs:
        return CFloatPtr(n.GetNode("AcquisitionFrameRateAbs"))->GetValue();
    case Type_Basler_Width:
        return CIntegerPtr(n.GetNode("Width"))->GetValue();
    case Type_Basler_Height:
        return CIntegerPtr(n.GetNode("Height"))->GetValue();
    default:
        return -1.0;
    }
}
