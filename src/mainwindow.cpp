#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <pylon/PylonIncludes.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <pylon/usb/BaslerUsbInstantCamera.h>
#include <stdio.h>


#include <stdlib.h>
#include "settingdialog.h"
// #include "Opencv_hp.h"

using namespace Pylon;
using namespace GenApi;
using namespace std;
using namespace cv;

#define APP_NAME "CameraView"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    saveSettings(std::make_unique<Settings>())
{
    ui->setupUi(this);
    ui->mainToolBar->setIconSize(QSize(48, 48));
    ui->centralWidget->installEventFilter(this);

    QFont bigFont = this->font();
    bigFont.setPointSize(16);
    
    ui->srcHeader->setFont(bigFont);
    
    QFont statusFont = this->font();
    statusFont.setPointSize(15);
    ui->sizeLabel->setFont(statusFont);
    ui->scaleLabel->setFont(statusFont);
    ui->collectionLabel->setFont(statusFont);
    ui->fpsLabel->setFont(statusFont);
    ui->sizeValue->setFont(statusFont);
    ui->scaleValue->setFont(statusFont);
    ui->collectionValue->setFont(statusFont);
    ui->fpsValue->setFont(statusFont);
    ui->labelAngle->setFont(statusFont);

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::onSaveData);
    connect(ui->actionRefresh, &QAction::triggered, this, &MainWindow::refresh);
    connect(ui->actionSetting, &QAction::triggered, this, &MainWindow::setting);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);
    connect(ui->actionResetZero, &QAction::triggered, this, &MainWindow::onResetZero);
    connect(ui->actionCapture, &QAction::triggered, this, &MainWindow::onCaptureZero);
    connect(ui->actionConfirm, &QAction::triggered, this, &MainWindow::onConfirmData);
    connect(ui->actionSpaceAlgo, &QAction::triggered, this, &MainWindow::showDialMarkDialog);
    connect(ui->actionErrorTable, &QAction::triggered, this, &MainWindow::showErrorTableDialog);
    connect(ui->pushResetZero, &QPushButton::clicked, this, &MainWindow::onResetZero);
    connect(ui->pushCaptureZero, &QPushButton::clicked, this, &MainWindow::onCaptureZero);
    connect(ui->pushConfirm, &QPushButton::clicked, this, &MainWindow::onConfirmData);
    connect(ui->pushSave, &QPushButton::clicked, this, &MainWindow::onSaveData);

    QToolBar *mytoolbar = new QToolBar(this);
    mytoolbar->addAction(ui->actionCloseAlgo);
    mytoolbar->setIconSize(QSize(48, 48));
    addToolBar(Qt::RightToolBarArea,mytoolbar);
    
    // 隐藏切换按钮，因为我们默认展开识别角度区域
    mytoolbar->setVisible(false);
    
    // 如果还想保留切换功能，可以注释掉上面这行，启用下面这行
    // connect(ui->actionCloseAlgo, &QAction::triggered, this, &MainWindow::algoArea);

    setupDialTypeSelector();   // 设置表盘类型选择器
    initPointerConfigs();      // 初始化指针识别配置
    initializeDataArrays();    // 初始化数据数组
    initializeRoundsData();    // 初始化5轮数据结构

    smallSize = QSize(750,this->height());
    bigSize = this->size();
    
    // 默认展开识别角度区域，使用布局管理器自动管理
    ui->destDisplay->setVisible(true);
    
    qDebug() << "UI initialized with default expanded layout";
}

MainWindow::~MainWindow()
{
    // 确保相机资源正确释放
    try {
        if (m_camera.IsGrabbing()) {
            m_camera.StopGrabbing();
        }
        if (m_camera.IsOpen()) {
            m_camera.Close();
        }
        // m_camera.DetachDevice(); // 在Close()之后会自动detach
    } catch (const std::exception& e) {
        qDebug() << "析构函数中关闭相机时发生异常:" << e.what();
    } catch (...) {
        qDebug() << "析构函数中关闭相机时发生未知异常";
    }
    
    delete ui;
    // saveSettings会被智能指针自动释放，不需要手动delete
}

void MainWindow::init(){
ui->sizeValue->setText("等待连接...");
    ui->scaleValue->setText("等待连接...");
    currentCapturedCount = 0;  // 初始化采集计数
    updateCollectionDisplay();
    ui->fpsValue->setText("0");
    ui->labelAngle->setText("等待检测...");
    
    ui->actionSave->setEnabled(false);

    ui->srcDisplay->setText("请连接相机并点击预览");
    ui->srcDisplay->setAlignment(Qt::AlignCenter);
    ui->destDisplay->setText("算法检测结果将显示在这里");
    ui->destDisplay->setAlignment(Qt::AlignCenter);

    refresh();
}

void MainWindow::refresh(){
    
    // 重置采集计数器
    currentCapturedCount = 0;
    updateCollectionDisplay();

    if(FullNameOfSelectedDevice.length() > 0){
        m_camera.Close();
        m_camera.DetachDevice();
        FullNameOfSelectedDevice = "";
    }

    DeviceInfoList_t dList;
    CTlFactory::GetInstance().EnumerateDevices(dList,true);
     if ( dList.size() == 0 )
     {
         QMessageBox::warning(this,APP_NAME,"请插入Basler相机",QMessageBox::Close,QMessageBox::Accepted);
         setNoCamera();
     }
     else
     {
         try{
             CDeviceInfo info;
             info.SetDeviceClass( CBaslerUsbInstantCamera::DeviceClass());
             FullNameOfSelectedDevice = QString(info.GetFriendlyName());
             m_camera.Attach(CTlFactory::GetInstance().CreateFirstDevice(info));
             m_camera.Open();
             startPreview();
         }
         catch(GenICam::GenericException &e)
         {
             QMessageBox::warning(this,APP_NAME,e.GetDescription(),QMessageBox::Close,QMessageBox::Accepted);
             setNoCamera();
         }
     }
}


void MainWindow::startPreview(){
    setButtons(true);
    if(m_camera.IsGrabbing()){
        m_camera.StopGrabbing();
    }

    try
    {
        INodeMap& nodemap = m_camera.GetNodeMap();

        // 先获取和设置基本参数
        CFloatPtr Rate(nodemap.GetNode("AcquisitionFrameRate"));
        CBooleanPtr AcquisitionFrameRateEnable(nodemap.GetNode("AcquisitionFrameRateEnable"));
        AcquisitionFrameRateEnable->FromString("true");
        CFloatPtr ExposureTime(nodemap.GetNode("ExposureTime"));
        ExposureTime->FromString(saveSettings->exposureTime);
        Rate->FromString(saveSettings->acquisitionFrameRate);

        // 获取传感器的完整分辨率
        CIntegerPtr WidthMax(nodemap.GetNode("WidthMax"));
        CIntegerPtr HeightMax(nodemap.GetNode("HeightMax"));
        int64_t sensorWidth = WidthMax->GetValue();
        int64_t sensorHeight = HeightMax->GetValue();

        // 确认用户设置的宽高是否合法
        int64_t cropWidth = atoi(saveSettings->width.c_str());
        int64_t cropHeight = atoi(saveSettings->height.c_str());
        
        // 确保尺寸不超过传感器范围
        if (cropWidth > sensorWidth) cropWidth = sensorWidth;
        if (cropHeight > sensorHeight) cropHeight = sensorHeight;

        // 根据相机规格计算有效的偏移量
        CIntegerPtr OffsetX(nodemap.GetNode("OffsetX"));
        CIntegerPtr OffsetY(nodemap.GetNode("OffsetY"));
        
        // 获取偏移量的增量和最大值
        int64_t offsetXInc = OffsetX->GetInc();
        int64_t offsetYInc = OffsetY->GetInc();
        int64_t offsetXMax = OffsetX->GetMax();
        int64_t offsetYMax = OffsetY->GetMax();
        
        // 计算偏移量，确保在相机允许的范围内
        int64_t offsetX = ((sensorWidth - cropWidth) / 2) / offsetXInc * offsetXInc; // 向下取整到增量的倍数
        int64_t offsetY = ((sensorHeight - cropHeight) / 2) / offsetYInc * offsetYInc;
        
        // 确保偏移量不超过最大值
        if (offsetX > offsetXMax) offsetX = offsetXMax;
        if (offsetY > offsetYMax) offsetY = offsetYMax;

        // 按照Basler相机的推荐顺序设置参数 - 先宽高再偏移
        CIntegerPtr Width(nodemap.GetNode("Width"));
        CIntegerPtr Height(nodemap.GetNode("Height"));
        
        // 先设置宽高
        Width->SetValue(cropWidth);
        Height->SetValue(cropHeight);
        
        // 再设置偏移量
        OffsetX->SetValue(offsetX);
        OffsetY->SetValue(offsetY);
        
        // 输出实际设置的值，用于Debug
        qDebug() << "设置ROI: " << cropWidth << "x" << cropHeight << " @ " << offsetX << "," << offsetY;
        qDebug() << "最大允许值: Width=" << Width->GetMax() << " Height=" << Height->GetMax() 
                 << " OffsetX=" << OffsetX->GetMax() << " OffsetY=" << OffsetY->GetMax();

        // 设置用户自定义参数
        if(!saveSettings->myattr.isEmpty()){
            if(saveSettings->type == 0){
                CIntegerPtr Attr(nodemap.GetNode(saveSettings->myattr.toUtf8().constData()));
                Attr->FromString(saveSettings->myvalue.toUtf8().constData());
            }else if(saveSettings->type == 1){
                CFloatPtr Attr(nodemap.GetNode(saveSettings->myattr.toUtf8().constData()));
                Attr->FromString(saveSettings->myvalue.toUtf8().constData());
            }else if(saveSettings->type == 2){
                CBooleanPtr Attr(nodemap.GetNode(saveSettings->myattr.toUtf8().constData()));
                Attr->FromString(saveSettings->myvalue.toUtf8().constData());
            }else if(saveSettings->type == 3){
                CStringPtr Attr(nodemap.GetNode(saveSettings->myattr.toUtf8().constData()));
                Attr->FromString(saveSettings->myvalue.toUtf8().constData());
            }
        }

        // 图像处理设置
        CImageFormatConverter fc;
        fc.OutputPixelFormat = PixelType_BGR8packed;
        CPylonImage image;

        // This smart pointer will receive the grab result data.
        CGrabResultPtr ptrGrabResult;

        m_camera.StartGrabbing(GrabStrategy_LatestImageOnly);

        cv::Mat openCvImage;
        cv::Mat openCvGrayImage;
        int64_t i = 0;

        while(m_camera.IsGrabbing())
        {
            m_camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException);

            if (ptrGrabResult->GrabSucceeded())
            {
                fc.Convert(image, ptrGrabResult);
                openCvImage = cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t *) image.GetBuffer());
                cvtColor(openCvImage, openCvImage, cv::COLOR_BGR2RGB);
                m_lastRgb = openCvImage.clone();

                // 显示原始图像
                QImage qtImage(openCvImage.data, openCvImage.cols, openCvImage.rows, openCvImage.step, QImage::Format_RGB888);
                ui->srcDisplay->setPixmap(QPixmap::fromImage(qtImage));
                ui->srcDisplay->update();
                // 在预览模式下显示当前已采集数量/设置的总数量
                updateCollectionDisplay();
                float wScale = roundf(ui->srcDisplay->width()*100.0/Width->GetValue())/100.0;
                float hScale = roundf(ui->srcDisplay->height()*100.0/Height->GetValue())/100.0;

                ui->scaleValue->setText(QString("W:%1 H:%2").arg(wScale).arg(hScale));
                ui->sizeValue->setText(QString("W:%1 H:%2").arg(ui->srcDisplay->width()).arg(ui->srcDisplay->height()));
                ui->fpsValue->setText(QString("%1").arg(round(Rate->GetValue(true))));

                // if(!ui->destDisplay->isHidden()){
                //     cvtColor(openCvImage, openCvGrayImage, cv::COLOR_RGB2GRAY);
                //     openCvGrayImage = spatial_LSI(openCvImage,5);
                //     QImage destImage(openCvGrayImage.data, openCvGrayImage.cols, openCvGrayImage.rows, openCvGrayImage.step, QImage::Format_RGB32);
                //     ui->destDisplay->setPixmap(QPixmap::fromImage(destImage.scaled(ui->destDisplay->width(), ui->destDisplay->height())));
                //     ui->destDisplay->update();
                // }
            }
            else
            {
                cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl;
            }
            qApp->processEvents();
        }
    }
    catch (GenICam::GenericException &e){
        QMessageBox::warning(this, "", e.GetDescription(), QMessageBox::Cancel, QMessageBox::Accepted);
        qDebug() << "错误: " << e.GetDescription();
    }
}


void MainWindow::setButtons(bool inPreview){
    ui->actionSave->setEnabled(true);
    ui->actionResetZero->setEnabled(true);
    ui->actionCapture->setEnabled(true);
    ui->actionConfirm->setEnabled(true);

    //    ui->actionSpaceAlgo->setEnabled(!inPreview);

}

void MainWindow::setNoCamera(){
    ui->actionSave->setEnabled(false);
    ui->actionResetZero->setEnabled(false);
    ui->actionCapture->setEnabled(false);
    ui->actionConfirm->setEnabled(false);
}

void MainWindow::updateCollectionDisplay() {
    // 显示当前已采集数量/设置的总数量
    ui->collectionValue->setText(QString("%1/%2").arg(currentCapturedCount).arg(saveSettings->image2save));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event){
    if (event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        if(obj == ui->centralWidget){
                qDebug() << ui->srcVerticalLayout->sizeHint().width() << "高度" << ui->srcVerticalLayout->sizeHint().width();
                ui->srcVerticalLayout->setSizeConstraint(QLayout::SetMaximumSize);
                ui->srcVerticalLayout->setGeometry(QRect(0,0,(resizeEvent->size().width() - 30)/2,(resizeEvent->size().height()) - 24));
        }
     }
     return QWidget::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton resBtn = QMessageBox::question( this, APP_NAME,
                                                                tr("确认要退出吗?\n"),
                                                                QMessageBox::No | QMessageBox::Yes,
                                                                QMessageBox::Yes);
    if (resBtn != QMessageBox::Yes) {
        event->ignore();
    } else {
        if(FullNameOfSelectedDevice.length() > 0){
            if(m_camera.IsGrabbing()){
                m_camera.StopGrabbing();
            }
            m_camera.Close();
            event->accept();
            exit(0);
        }
    }

}



void MainWindow::setting(){
    if (FullNameOfSelectedDevice.length() <= 0) {
        QMessageBox::warning(this, APP_NAME, "请插入Basler相机", QMessageBox::Cancel, QMessageBox::Accepted);
        return;
    }
    
    qDebug() << "启动设置对话框";
    
    auto dialog = std::make_unique<SettingDialog>(this);
    dialog->setSettings(saveSettings.get());  // 传递指针
    
    int result = dialog->exec();
    if (result == QDialog::Accepted) {
        // 复制设置
        *saveSettings = *(dialog->getSaveSettings());
        startPreview();
    }
}

void MainWindow::about(){
    // AboutDialog *dialog = new AboutDialog(this);
    // dialog->exec();
}

void MainWindow::singleGrab(){
    if(m_camera.IsGrabbing()){
        m_camera.StopGrabbing();
    }

    CGrabResultPtr ptrGrabResult;
    CImageFormatConverter fc;
    fc.OutputPixelFormat = PixelType_BGR8packed;
    CPylonImage image;
    m_camera.StartGrabbing(1,GrabStrategy_LatestImageOnly);
    m_camera.RetrieveResult( 5000, ptrGrabResult, TimeoutHandling_Return);
    if (ptrGrabResult->GrabSucceeded())
    {

       fc.Convert(image, ptrGrabResult);
       cv::Mat openCvImage= cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t *) image.GetBuffer());
       cvtColor(openCvImage, openCvImage, cv::COLOR_BGR2RGB);

       QImage qtImage(openCvImage.data,openCvImage.cols,openCvImage.rows,openCvImage.step,QImage::Format_RGB888);
       ui->srcDisplay->setPixmap(QPixmap::fromImage(qtImage));
       ui->srcDisplay->update();
    }
    else
    {
        cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl;
    }

    m_camera.StopGrabbing();
    setButtons(false);


    QDir saveDir(saveSettings->FilePath);
    saveDir.mkdir("single");
    bool flag = true;
    int i = 0;
    while(flag){
        QString filePath = saveSettings->FilePath + "/single/" + saveSettings->FilePrefix + QString::number(i);
        if(saveSettings->format == ImageFileFormat_Tiff){
            filePath += ".tiff";
        }else if(saveSettings->format == ImageFileFormat_Png){
            filePath += ".png";
        }
        if(QFile::exists(filePath)){
            i++;
        }else{
            flag = false;
            ui->srcDisplay->pixmap(Qt::ReturnByValue).save(filePath);
            // 单张采集成功后增加计数并更新显示
            currentCapturedCount++;
            updateCollectionDisplay();
        }
    }

}


void MainWindow::multiGrab(){
    auto dialog = std::make_unique<SettingDialog>(this);
    dialog->setSettings(saveSettings.get());
    
    int result = dialog->exec();
    if (result == QDialog::Accepted) {
        *saveSettings = *(dialog->getSaveSettings());
    }
    try {
        INodeMap& nodemap = m_camera.GetNodeMap();

        CIntegerPtr Width (nodemap.GetNode("Width"));
        CIntegerPtr Height (nodemap.GetNode("Height"));
        CFloatPtr Rate( nodemap.GetNode("AcquisitionFrameRate") );
        //Acquisition frame rate of the camera in frames per second.
        CBooleanPtr AcquisitionFrameRateEnable( nodemap.GetNode("AcquisitionFrameRateEnable") );
        AcquisitionFrameRateEnable->FromString("true");
        CFloatPtr ExposureTime( nodemap.GetNode("ExposureTime") );


        Width->FromString(saveSettings->width);
        Height->FromString(saveSettings->height);
        ExposureTime->FromString(saveSettings->exposureTime);
        Rate->FromString(saveSettings->acquisitionFrameRate);
        CGrabResultPtr ptrGrabResult;

        m_camera.StartGrabbing(GrabStrategy_LatestImageOnly);

        QDir saveDir(saveSettings->FilePath);
        saveDir.mkdir("multi");

        QProgressDialog dialog;
        connect(&dialog, &QProgressDialog::canceled, this, &MainWindow::startPreview);
        dialog.setRange(0,saveSettings->image2save);
        dialog.setLabelText(QString("保存第 %1 张照片").arg(imageSaved));
        dialog.show();

        // 重置连续采集的计数
        int multiCaptureCount = 0;

        while(m_camera.IsGrabbing()) {
            m_camera.RetrieveResult( 5000, ptrGrabResult, TimeoutHandling_ThrowException);
            if (ptrGrabResult->GrabSucceeded()) {
                QString filePath = saveSettings->FilePath + "/multi/" + saveSettings->FilePrefix + QString::number(imageSaved);

                if(saveSettings->format == ImageFileFormat_Tiff){
                    filePath += ".tiff";
                } else if (saveSettings->format == ImageFileFormat_Png) {
                    filePath += ".png";
                }

                CImagePersistence::Save( saveSettings->format, filePath.toUtf8().constData(), ptrGrabResult);
                imageSaved++;
                multiCaptureCount++;
                
                // 更新进度对话框和采集数量显示
                dialog.setLabelText(QString("保存第 %1 张照片").arg(imageSaved));
                dialog.setValue(imageSaved);
                
                // 更新主界面的采集数量显示
                currentCapturedCount++;
                updateCollectionDisplay();
                
                if (imageSaved >= saveSettings->image2save) {
                    m_camera.StopGrabbing();
                    imageSaved = 0;
                }
            } else {
                cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl;
            }
            qApp->processEvents();
        }
    }
    catch (GenICam::GenericException &e) {
        QMessageBox::warning(this, "", e.GetDescription(), QMessageBox::Cancel, QMessageBox::Accepted);
    }
    startPreview();
}

void MainWindow::temporal_LSI(){

}

Mat MainWindow::spatial_LSI(Mat speckle, int m){
    Mat enhanced;
    
    // 转换为灰度图（如果不是）
    if(speckle.channels() == 3) {
        cvtColor(speckle, enhanced, COLOR_BGR2GRAY);
    } else {
        enhanced = speckle.clone();
    }
    
    // 1. 直方图均衡化增强对比度
    equalizeHist(enhanced, enhanced);
    
    // 2. 高斯滤波去噪
    GaussianBlur(enhanced, enhanced, Size(3, 3), 0);
    
    // 3. 锐化处理，突出边缘
    Mat kernel = (Mat_<float>(3,3) << 
                  0, -1, 0,
                  -1, 5, -1,
                  0, -1, 0);
    filter2D(enhanced, enhanced, -1, kernel);
    
    // 4. 对比度拉伸
    Mat enhanced_norm;
    normalize(enhanced, enhanced_norm, 0, 255, NORM_MINMAX);
    
    // 5. 轻微的边缘检测增强
    Mat edges;
    Canny(enhanced_norm, edges, 30, 90);
    
    // 6. 将边缘信息融合回原图
    Mat result;
    addWeighted(enhanced_norm, 0.8, edges, 0.2, 0, result);
    
    // 转换为3通道以便显示
    Mat result_bgr;
    cvtColor(result, result_bgr, COLOR_GRAY2BGR);
    
    return result_bgr;
}


void MainWindow::conv2(const Mat &img, const Mat& kernel, ConvolutionType type, Mat& dest) {
      Mat source = img;
      if(CONVOLUTION_FULL == type) {
        source = Mat();
        const int additionalRows = kernel.rows-1, additionalCols = kernel.cols-1;
        copyMakeBorder(img, source, (additionalRows+1)/2, additionalRows/2, (additionalCols+1)/2, additionalCols/2, BORDER_CONSTANT, Scalar(0));
      }

      Point anchor(kernel.cols - kernel.cols/2 - 1, kernel.rows - kernel.rows/2 - 1);
      int borderMode = BORDER_CONSTANT;
      Mat result;
      flip(kernel,result,-1);

      filter2D(source, dest, img.depth(), result, anchor, 0, borderMode);

      if(CONVOLUTION_VALID == type) {
        dest = dest.colRange((kernel.cols-1)/2, dest.cols - kernel.cols/2)
                   .rowRange((kernel.rows-1)/2, dest.rows - kernel.rows/2);
      }
}

void MainWindow::spatial_LSI_Matlab(){

    QString filePath = "/tmp/t.png";
    ui->srcDisplay->pixmap(Qt::ReturnByValue).save(filePath);

    Mat speckle;
    speckle = imread(filePath.toUtf8().constData(), cv::IMREAD_GRAYSCALE);
    speckle.convertTo(speckle, CV_32FC1);

    int m = 5;
    int n = m;
    Mat spatial_masker = Mat::ones(m,n,CV_32FC1)/(m*n);
    Mat resultSum;
    conv2(speckle,spatial_masker,ConvolutionType::CONVOLUTION_SAME,resultSum);
    Mat resultSqure;
    Mat speckeSqure;
    cv::pow(speckle,2,speckeSqure);
    conv2(speckeSqure,spatial_masker,ConvolutionType::CONVOLUTION_SAME,resultSqure);
    Mat tmp;
    cv::pow(resultSum,2,tmp);

    Mat trueResult;
    cv::divide((resultSqure - tmp)*1.0,tmp,trueResult);

    namedWindow( "Display window", WINDOW_NORMAL );// Create a window for display.
    imshow( "Display window", trueResult * 10 );                   // Show our image inside it.
    waitKey(0);


}

void MainWindow::algoArea(){
    qDebug() << "algoArea called - layout is now fixed";
}


void MainWindow::onCaptureZero()
{
    qDebug() << "开始测量角度差...";
    
    if (!m_hasZero) {
        QMessageBox::information(this, "提示", "请先点击『归位』按钮设定零位");
        return;
    }

    if (m_lastRgb.empty()) {
        QMessageBox::warning(this, "提示", "还没有获取到图像，请确保预览正在运行");
        return;
    }

    try {
        cv::Mat frame;
        cv::cvtColor(m_lastRgb, frame, cv::COLOR_RGB2BGR);

        // 使用多次测量提高精度
        double now = measureAngleMultipleTimes(frame, 3);
        if (now == -999) {
            QMessageBox::warning(this, "错误", "计算角度失败");
            return;
        }

        // 更新指针运动方向
        updatePointerDirection(now);

        // 计算角度差
        double delta = now - m_zeroAngle;
        if (delta > 180)  delta -= 360;
        if (delta < -180) delta += 360;

        qDebug() << "零位:" << m_zeroAngle << "当前:" << now << "角度差:" << delta << "行程:" << getStrokeDirectionString();

        ui->labelAngle->setText(
            QString("零位: %1° | 当前: %2° | 角度差: %3° | 行程: %4")
                .arg(m_zeroAngle, 0, 'f', 2)
                .arg(now, 0, 'f', 2)
                .arg(delta, 0, 'f', 2)
                .arg(getStrokeDirectionString()));

        // 右侧显示检测示意图 - 创建单次检测器用于可视化
        highPreciseDetector visDetector(frame, m_currentConfig);
        visDetector.showScale1Result();
        cv::Mat vis = visDetector.visual();
        cv::cvtColor(vis, vis, cv::COLOR_BGR2RGB);
        QImage q(vis.data, vis.cols, vis.rows, vis.step, QImage::Format_RGB888);
        ui->destDisplay->setPixmap(QPixmap::fromImage(q)
                                   .scaled(ui->destDisplay->size(),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        
        // 更新采集计数
        currentCapturedCount++;
        updateCollectionDisplay();
        
    } catch (const std::exception& e) {
        qDebug() << "测量角度时发生异常:" << e.what();
        QMessageBox::critical(this, "错误", QString("测量角度失败: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "测量角度时发生未知异常";
        QMessageBox::critical(this, "错误", "测量角度时发生未知错误");
    }
}

bool MainWindow::grabOneFrame(cv::Mat& outBgr)
{
    if (m_lastRgb.empty()) {
        qDebug() << "m_lastRgb 为空";
        QMessageBox::warning(this, "提示", "还没取到第一帧，请稍等或重新开始预览");
        return false;
    }
    
    try {
        cv::cvtColor(m_lastRgb, outBgr, cv::COLOR_RGB2BGR);
        qDebug() << "成功获取一帧，尺寸:" << outBgr.cols << "x" << outBgr.rows;
        return true;
    } catch (const std::exception& e) {
        qDebug() << "颜色转换失败:" << e.what();
        return false;
    }
}


// 修复 onResetZero() 函数
void MainWindow::onResetZero()
{
    qDebug() << "开始重置零位...";
    
    // 检查是否有可用的图像
    if (m_lastRgb.empty()) {
        QMessageBox::warning(this, "提示", "还没有获取到图像，请确保预览正在运行");
        return;
    }
    
    try {
        // 转换为BGR格式用于检测
        cv::Mat frame;
        cv::cvtColor(m_lastRgb, frame, cv::COLOR_RGB2BGR);
        
        qDebug() << "图像尺寸:" << frame.cols << "x" << frame.rows;
        
        // 创建检测器
        highPreciseDetector det(frame, m_currentConfig);
        
        // 检查是否检测到必要的元素
        if (det.getCircles().empty() && det.getLine().empty()) {
            QMessageBox::warning(this, "提示", "未检测到表盘或指针，请调整相机位置或光照条件");
            return;
        }
        
        // 获取角度
        double angle = det.getAngle();
        if (angle == -999) {
            QMessageBox::warning(this, "错误", "无法计算角度，请确保指针清晰可见");
            return;
        }
        
        // 设置零位
        m_zeroAngle = angle;
        m_hasZero = true;
        
        // 重置行程跟踪
        resetStrokeTracking();
        
        qDebug() << "零位设置成功，角度:" << m_zeroAngle;
        
        // 归位操作：清空当前轮次所有数据，然后添加零度数据到采集数据1
        if (m_currentRound < m_allRoundsData.size()) {
            RoundData &currentRound = m_allRoundsData[m_currentRound];
            
            // 清空当前轮次的所有数据
            currentRound.forwardAngles.fill(0.0);
            currentRound.backwardAngles.fill(0.0);
            currentRound.maxAngle = 0.0;
            currentRound.isCompleted = false;
            
            // 将归位的0度数据写入采集数据1（第一个正行程位置）
            if (!currentRound.forwardAngles.isEmpty()) {
                currentRound.forwardAngles[0] = 0.0;  // 零度写入采集数据1
            }
            
            qDebug() << "第" << (m_currentRound + 1) << "轮数据已清空，零度已写入采集数据1";
        }
        
        // 同时更新误差表格（如果打开的话）
        updateErrorTableWithAllRounds();
        
        // 更新界面显示
        ui->labelAngle->setText(QString("零位已设置: %1° (第%2轮数据已清空，0°已写入采集数据1)")
            .arg(angle, 0, 'f', 2)
            .arg(m_currentRound + 1));
        
        // 显示检测结果到右侧区域
        det.showScale1Result();
        cv::Mat vis = det.visual();
        cv::cvtColor(vis, vis, cv::COLOR_BGR2RGB);
        QImage q(vis.data, vis.cols, vis.rows, vis.step, QImage::Format_RGB888);
        ui->destDisplay->setPixmap(QPixmap::fromImage(q)
                                   .scaled(ui->destDisplay->size(),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        
        QMessageBox::information(this, "成功", QString("零位设置成功！\n当前角度: %1°").arg(angle, 0, 'f', 2));
        
    } catch (const std::exception& e) {
        qDebug() << "重置零位时发生异常:" << e.what();
        QMessageBox::critical(this, "错误", QString("重置零位失败: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "重置零位时发生未知异常";
        QMessageBox::critical(this, "错误", "重置零位时发生未知错误");
    }
}

void MainWindow::runAlgoOnce()
{
    cv::Mat rgbFrame;                 // 取当前一帧（RGB）
    if (!grabOneFrame(rgbFrame)) {    // 你已有的同步抓帧函数
        ui->labelAngle->setText("取帧失败");
        return;
    }

    try {
        cv::Mat bgr;
        cv::cvtColor(rgbFrame, bgr, cv::COLOR_RGB2BGR);

        highPreciseDetector det(bgr, m_currentConfig);
        if (det.getCircles().empty() || det.getLine().empty())
            throw std::runtime_error("未检测到表盘或指针");

        double angle = det.getAngle();          // 角度
        det.showScale1Result();
        cv::Mat vis = det.visual();             // 可视化结果

        cv::Mat enhanced = spatial_LSI(bgr, 5);
        cv::Mat mix;
        cv::addWeighted(vis, 0.7, enhanced, 0.3, 0, mix);

        cv::cvtColor(mix, mix, cv::COLOR_BGR2RGB);
        QImage img(mix.data, mix.cols, mix.rows, mix.step, QImage::Format_RGB888);
        ui->destDisplay->setPixmap(QPixmap::fromImage(
                img).scaled(ui->destDisplay->size(),
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation));

        // 更新角度标签
        if (angle != -999) {
            // 更新指针运动方向（如果已设置零位）
            if (m_hasZero) {
                updatePointerDirection(angle);
            }
            
            QString txt;
            if (m_hasZero) {
                double delta = angle - m_zeroAngle;
                if (delta > 180)  delta -= 360;
                if (delta < -180) delta += 360;
                txt = QString("零位: %1° | 当前: %2° | 角度差: %3° | 行程: %4")
                        .arg(m_zeroAngle, 0, 'f', 2)
                        .arg(angle, 0, 'f', 2)
                        .arg(delta, 0, 'f', 2)
                        .arg(getStrokeDirectionString());
            } else {
                txt = QString("当前角度: %1°").arg(angle, 0, 'f', 2);
            }
            ui->labelAngle->setText(txt);
        }
    }
    catch (const std::exception &e) {
        qDebug() << "识别错误:" << e.what();
        cv::Mat gray;
        cv::cvtColor(rgbFrame, gray, cv::COLOR_RGB2GRAY);
        cv::Mat lsi = spatial_LSI(gray, 5);
        QImage img(lsi.data, lsi.cols, lsi.rows, lsi.step, QImage::Format_Grayscale8);
        ui->destDisplay->setPixmap(QPixmap::fromImage(
                img).scaled(ui->destDisplay->size(),
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation));
        ui->labelAngle->setText("未检测到表盘或指针");
    }
}

void MainWindow::showDialMarkDialog()
{
    qDebug() << "打开表盘标注对话框，表盘类型:" << m_currentDialType;
    
    auto dialog = std::make_unique<DialMarkDialog>(this, m_currentDialType);
    dialog->exec();
}

void MainWindow::showErrorTableDialog()
{
    qDebug() << "打开误差检测表格";
    
    try {
        // 如果对话框已存在，直接显示并激活
        if (m_errorTableDialog) {
            m_errorTableDialog->show();
            m_errorTableDialog->raise();
            m_errorTableDialog->activateWindow();
            return;
        }
        
        // 创建新的对话框实例
        m_errorTableDialog = new ErrorTableDialog(this);
        
        // 连接关闭信号，在对话框关闭时清空指针
        connect(m_errorTableDialog, &QDialog::finished, this, [this]() {
            m_errorTableDialog = nullptr;
        });
        
        // 根据当前选择的表盘类型设置默认配置
        if (!m_currentDialType.isEmpty()) {
            m_errorTableDialog->setDialType(m_currentDialType);
        }
        
        // 同步所有轮次的数据到误差表格
        updateErrorTableWithAllRounds();
        
        m_errorTableDialog->show();
        
        qDebug() << "误差检测表格创建成功";
    } catch (const std::exception& e) {
        qDebug() << "创建误差检测表格异常:" << e.what();
        QMessageBox::warning(this, "错误", QString("无法打开误差检测表格: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "创建误差检测表格未知异常";
        QMessageBox::warning(this, "错误", "无法打开误差检测表格");
    }
}

void MainWindow::setupDialTypeSelector()
{
    // 创建表盘类型选择标签和下拉框
    m_dialTypeLabel = new QLabel("表盘类型:", this);
    m_dialTypeCombo = new QComboBox(this);
    
    // 添加表盘类型选项
    m_dialTypeCombo->addItem("YYQY-13");
    m_dialTypeCombo->addItem("BYQ-19");
    
    // 设置默认选择
    m_dialTypeCombo->setCurrentText("YYQY-13");
    m_currentDialType = "YYQY-13";
    
    // 设置样式
    QFont font = m_dialTypeLabel->font();
    font.setPointSize(12);
    font.setBold(true);
    m_dialTypeLabel->setFont(font);
    m_dialTypeCombo->setFont(font);
    
    m_dialTypeCombo->setMinimumWidth(120);
    m_dialTypeCombo->setMaximumHeight(30);
    
    // 将控件添加到状态栏
    ui->statusBar->addPermanentWidget(m_dialTypeLabel);
    ui->statusBar->addPermanentWidget(m_dialTypeCombo);
    
    // 连接信号槽
    connect(m_dialTypeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onDialTypeChanged);
}

void MainWindow::initPointerConfigs()
{
    // YYQY表盘配置 - 针对白色指针优化
    m_yyqyConfig.dp = 1.0;
    m_yyqyConfig.minDist = 100;  
    m_yyqyConfig.param1 = 100;
    m_yyqyConfig.param2 = 25;              // 降低以检测更多圆候选
    m_yyqyConfig.minRadius = 150;  
    m_yyqyConfig.maxRadius = 300;
    
    // 白色指针检测参数 - 针对YYQY白色指针优化
    m_yyqyConfig.usePointerFromCenter = true;  // 使用专门的白色指针检测
    m_yyqyConfig.pointerSearchRadius = 0.85;   // 搜索半径比例
    m_yyqyConfig.pointerMinLength = 60;        // 降低最小长度，白色指针可能较短
    m_yyqyConfig.cannyLow = 30;               // 保持低阈值
    m_yyqyConfig.cannyHigh = 100;
    m_yyqyConfig.rho = 1.0;                   // 距离分辨率
    m_yyqyConfig.theta = CV_PI/180;           // 角度分辨率
    m_yyqyConfig.threshold = 35;              // 降低直线检测阈值
    m_yyqyConfig.minLineLength = 45;          // 降低最小线段长度
    m_yyqyConfig.maxLineGap = 12;             // 适当增加间隙
    m_yyqyConfig.silverThresholdLow = 0;      // YYQY不使用银色检测
    m_yyqyConfig.dialType = "YYQY";           // 设置表盘类型
    
    // BYQ表盘配置 - 针对银色指针末端和转轴检测优化
    m_byqConfig.dp = 1.0;
    m_byqConfig.minDist = 100;
    m_byqConfig.param1 = 100;
    m_byqConfig.param2 = 30;
    m_byqConfig.minRadius = 50;
    m_byqConfig.maxRadius = 0;
    
    // BYQ指针检测参数 - 针对细指针末端检测
    m_byqConfig.usePointerFromCenter = true;
    
    // 步骤1-2：掩码参数
    m_byqConfig.pointerMaskRadius = 0.9;        // 表盘掩码半径比例
    m_byqConfig.axisExcludeMultiplier = 1.8;    // 转轴排除区域倍数
    
    // 步骤3：预处理参数
    m_byqConfig.morphKernelWidth = 1;           // 形态学核宽度
    m_byqConfig.morphKernelHeight = 2;          // 形态学核高度
    m_byqConfig.gaussianKernelSize = 3;         // 高斯核大小
    m_byqConfig.gaussianSigma = 0.8;            // 高斯标准差
    
    // 步骤4：边缘检测参数
    m_byqConfig.cannyLowThreshold = 30;         // Canny低阈值
    m_byqConfig.cannyHighThreshold = 100;       // Canny高阈值
    
    // 步骤5：HoughLinesP参数
    m_byqConfig.houghThreshold = 20;            // 直线检测阈值
    m_byqConfig.minLineLengthRatio = 0.12;      // 最小长度比例
    m_byqConfig.maxLineGapRatio = 0.08;         // 最大间隙比例
    m_byqConfig.dialType = "BYQ";             // 设置表盘类型
    
    // 设置默认配置
    m_currentConfig = &m_yyqyConfig;
    
    qDebug() << "指针识别配置初始化完成 - 白色指针优化版本";
}

void MainWindow::switchPointerConfig(const QString& dialType)
{
    if (dialType == "YYQY-13") {
        m_currentConfig = &m_yyqyConfig;
        qDebug() << "切换到YYQY-13指针识别配置";
    } else if (dialType == "BYQ-19") {
        m_currentConfig = &m_byqConfig;
        qDebug() << "切换到BYQ-19指针识别配置";
    }
}

void MainWindow::onDialTypeChanged(const QString &dialType)
{
    m_currentDialType = dialType;
    switchPointerConfig(dialType);  // 切换指针识别配置
    qDebug() << "表盘类型切换为:" << dialType;
    
    // 重新初始化轮次数据
    initializeRoundsData();
    
    // 更新界面显示
    updateDataTable();
}

highPreciseDetector::highPreciseDetector(const cv::Mat& image, const PointerDetectionConfig* config) : m_angle(-999), m_config(config), m_axisCenter(-1, -1), m_axisRadius(-1) {
    if (image.empty()) {
        qDebug() << "输入图像为空";
        return;
    }
    
    // 如果没有提供配置，使用默认配置
    static PointerDetectionConfig defaultConfig;
    if (m_config == nullptr) {
        m_config = &defaultConfig;
    }
    
    // 复制输入图像
    m_image = image.clone();
    m_visual = image.clone();
    
    try {
        // 检测圆形
        detectCircles();
        
        // 根据配置选择指针检测方法
        if (m_config->usePointerFromCenter && !m_circles.empty()) {
            detectPointerFromCenter();
        } else {
            detectLines();
        }
        
        // 计算角度
        if (!m_circles.empty() && !m_lines.empty()) {
            calculateAngle();
        }
    } catch (const std::exception& e) {
        qDebug() << "检测过程中出错:" << e.what();
    }
}

void highPreciseDetector::detectCircles() {
    cv::Mat gray;
    if (m_image.channels() == 3) {
        cv::cvtColor(m_image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = m_image.clone();
    }
    
    // 检查是否为BYQ模式，如果是则使用增强检测
    if (m_config && m_config->dialType == "BYQ") {
        qDebug() << "BYQ模式：使用增强表盘检测";
        
        // BYQ专用的增强预处理
        cv::Mat enhanced;
        cv::equalizeHist(gray, enhanced);
        cv::GaussianBlur(enhanced, enhanced, cv::Size(7, 7), 1.5);
        
        // 两次检测：标准 + 保守
        std::vector<cv::Vec3f> allCircles;
        
        // 第一次：标准参数
        std::vector<cv::Vec3f> circles1;
        cv::HoughCircles(enhanced, circles1, cv::HOUGH_GRADIENT, 
                         m_config->dp, m_config->minDist, 
                         m_config->param1, m_config->param2, 
                         m_config->minRadius, m_config->maxRadius);
        allCircles.insert(allCircles.end(), circles1.begin(), circles1.end());
        
        // 第二次：保守参数
        std::vector<cv::Vec3f> circles2;
        cv::HoughCircles(enhanced, circles2, cv::HOUGH_GRADIENT, 
                         m_config->dp, m_config->minDist, 
                         m_config->param1 * 1.3, m_config->param2 * 1.6, 
                         m_config->minRadius, m_config->maxRadius);
        allCircles.insert(allCircles.end(), circles2.begin(), circles2.end());
        
        if (!allCircles.empty()) {
            // 简化的两次评分
            cv::Vec3f bestCircle = allCircles[0];
            double bestScore = 0;
            
            for (const auto& circle : allCircles) {
                // 第一次评分：基本筛选
                cv::Point2f center(circle[0], circle[1]);
                float radius = circle[2];
                
                if (center.x < radius || center.x > gray.cols - radius ||
                    center.y < radius || center.y > gray.rows - radius) {
                    continue;
                }
                
                // 第二次评分：简化综合评分
                cv::Point2f imageCenter(gray.cols / 2.0f, gray.rows / 2.0f);
                double distToCenter = sqrt(pow(center.x - imageCenter.x, 2) + 
                                         pow(center.y - imageCenter.y, 2));
                
                double score = radius * 0.7 + 50.0 / (distToCenter + 1);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestCircle = circle;
                }
            }
            
            m_circles.push_back(bestCircle);
            qDebug() << "BYQ表盘检测: 中心(" << bestCircle[0] << "," << bestCircle[1] 
                     << ") 半径:" << bestCircle[2];
        }
    } else {
        // YYQY模式：保持原有逻辑
        qDebug() << "YYQY模式：使用标准表盘检测";
        
        cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2, 2);
        
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 
                         m_config->dp, m_config->minDist, 
                         m_config->param1, m_config->param2, 
                         m_config->minRadius, m_config->maxRadius);
        
        if (!circles.empty()) {
            cv::Vec3f maxCircle = circles[0];
            float maxRadius = maxCircle[2];
            
            for (const auto& circle : circles) {
                if (circle[2] > maxRadius) {
                    maxCircle = circle;
                    maxRadius = circle[2];
                }
            }
            
            m_circles.push_back(maxCircle);
            qDebug() << "YYQY表盘检测: 中心(" << maxCircle[0] << "," << maxCircle[1] 
                     << ") 半径:" << maxRadius;
        }
    }
    
    if (m_circles.empty()) {
        qDebug() << "未检测到圆形表盘";
    }
}

void highPreciseDetector::detectLines() {
    cv::Mat gray, edges;
    if (m_image.channels() == 3) {
        cv::cvtColor(m_image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = m_image.clone();
    }
    
    // 使用配置参数进行边缘检测
    cv::Canny(gray, edges, m_config->cannyLow, m_config->cannyHigh, 3);
    
    // 使用配置参数进行HoughLinesP检测
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, 
                    m_config->rho, 
                    m_config->theta, 
                    m_config->threshold, 
                    m_config->minLineLength, 
                    m_config->maxLineGap);
    
    if (!lines.empty()) {
        // 如果检测到表盘，选择距离表盘中心最近的直线作为指针
        if (!m_circles.empty()) {
            cv::Point2f center(m_circles[0][0], m_circles[0][1]);
            cv::Vec4i bestLine;
            double minDist = std::numeric_limits<double>::max();
            
            for (const auto& line : lines) {
                cv::Point2f lineCenter((line[0] + line[2])/2.0f, (line[1] + line[3])/2.0f);
                double dist = cv::norm(center - lineCenter);
                
                if (dist < minDist) {
                    minDist = dist;
                    bestLine = line;
                }
            }
            
            if (minDist < m_circles[0][2]) { // 确保直线在表盘内
                m_lines.push_back(bestLine);
                qDebug() << "检测到指针: (" << bestLine[0] << "," << bestLine[1] << ") 到 (" << bestLine[2] << "," << bestLine[3] << ")";
            }
        } else {
            // 如果没有检测到表盘，选择最长的直线
            cv::Vec4i longestLine;
            double maxLength = 0;
            
            for (const auto& line : lines) {
                double length = sqrt(pow(line[2] - line[0], 2) + pow(line[3] - line[1], 2));
                if (length > maxLength) {
                    maxLength = length;
                    longestLine = line;
                }
            }
            
            if (maxLength > 0) {
                m_lines.push_back(longestLine);
                qDebug() << "检测到最长直线作为指针";
            }
        }
    } else {
        qDebug() << "未检测到直线";
    }
}

void highPreciseDetector::detectPointerFromCenter() {
    if (m_circles.empty()) {
        qDebug() << "没有检测到表盘，无法进行指针检测";
        return;
    }
    
    cv::Mat gray;
    if (m_image.channels() == 3) {
        cv::cvtColor(m_image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = m_image.clone();
    }
    
    // 获取表盘中心和半径
    cv::Point2f center(m_circles[0][0], m_circles[0][1]);
    float radius = m_circles[0][2];
    
    cv::Vec4i bestPointer(-1, -1, -1, -1);
    
    // 根据配置参数选择检测算法
    if (m_config->silverThresholdLow > 0) {
        // BYQ银色指针检测
        qDebug() << "检测BYQ银色指针，表盘中心:(" << center.x << "," << center.y << ") 半径:" << radius;
        bestPointer = detectBYQPointer(gray, center, radius);
    } else {
        // YYQY白色指针检测
        qDebug() << "检测YYQY白色指针，表盘中心:(" << center.x << "," << center.y << ") 半径:" << radius;
        bestPointer = detectWhitePointer(gray, center, radius);
    }
    
    if (bestPointer[0] != -1) {
        m_lines.clear();
        m_lines.push_back(bestPointer);
        double length = sqrt(pow(bestPointer[2] - bestPointer[0], 2) + pow(bestPointer[3] - bestPointer[1], 2));
        qDebug() << "检测到指针: (" << bestPointer[0] << "," << bestPointer[1] 
                 << ") 到 (" << bestPointer[2] << "," << bestPointer[3] << "), 长度:" << length;
    } else {
        qDebug() << "未能检测到指针，回退到传统方法";
        detectLines();
    }
}

cv::Vec4i highPreciseDetector::detectWhitePointer(const cv::Mat& gray, const cv::Point2f& center, float radius) {
    // 确保YYQY模式下不显示转轴中心
    m_axisCenter = cv::Point2f(-1, -1);
    
    // 1. 创建表盘内部的掩码
    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::circle(mask, cv::Point((int)center.x, (int)center.y), (int)(radius * 0.9), cv::Scalar(255), -1);
    
    // 2. 检测白色区域 - 使用阈值分割
    cv::Mat whiteRegions;
    cv::threshold(gray, whiteRegions, 180, 255, cv::THRESH_BINARY);  // 检测亮区域
    whiteRegions.copyTo(whiteRegions, mask);  // 仅在表盘内部
    
    // 3. 形态学操作连接白色区域
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(whiteRegions, whiteRegions, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(whiteRegions, whiteRegions, cv::MORPH_OPEN, kernel);
    
    // 4. 查找白色区域的轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(whiteRegions, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    cv::Vec4i bestPointer(-1, -1, -1, -1);
    double maxScore = 0;
    
    // 5. 分析每个轮廓，找到最可能的指针
    for (const auto& contour : contours) {
        if (contour.size() < 5) continue;  // 轮廓点太少
        
        // 计算轮廓的面积
        double area = cv::contourArea(contour);
        if (area < 100 || area > radius * radius * 0.3) continue;  // 面积过滤
        
        // 拟合椭圆或直线
        cv::RotatedRect ellipse = cv::fitEllipse(contour);
        
        // 检查椭圆的长宽比，指针应该是细长的
        float aspectRatio = ellipse.size.width / ellipse.size.height;
        if (aspectRatio < 1) aspectRatio = 1.0f / aspectRatio;  // 确保>1
        
        if (aspectRatio < 2.0) continue;  // 不够细长，不像指针
        
        // 检查椭圆中心是否接近表盘中心
        cv::Point2f ellipseCenter = ellipse.center;
        double distToDialCenter = cv::norm(ellipseCenter - center);
        if (distToDialCenter > radius * 0.5) continue;  // 中心偏离太远
        
        // 计算指针方向和端点 - 修复角度计算
        double angle = ellipse.angle * CV_PI / 180.0;
        double length = std::max(ellipse.size.width, ellipse.size.height) / 2.0;
        
        // OpenCV的椭圆角度定义：从x轴正方向逆时针测量
        // 但是我们需要考虑长轴方向
        if (ellipse.size.width < ellipse.size.height) {
            // 如果高度>宽度，则长轴是垂直方向，需要调整角度
            angle += CV_PI / 2.0;
        }
        
        cv::Point2f direction(cos(angle), sin(angle));
        cv::Point2f startPoint = ellipseCenter - direction * (float)length;
        cv::Point2f endPoint = ellipseCenter + direction * (float)length;
        
        // 确保指针从表盘中心指向外围
        double dist1 = cv::norm(startPoint - center);
        double dist2 = cv::norm(endPoint - center);
        if (dist1 > dist2) {
            // 如果startPoint离表盘中心更远，说明方向反了
            std::swap(startPoint, endPoint);
        }
        
        // 进一步调整：确保startPoint是表盘中心附近的点
        cv::Point2f vectorToCenter = center - ellipseCenter;
        double distEllipseToCenter = cv::norm(vectorToCenter);
        if (distEllipseToCenter > 10) {  // 椭圆中心不在表盘中心
            // 将起点调整为更接近表盘中心的位置
            cv::Point2f directionToCenter = vectorToCenter / (float)distEllipseToCenter;
            startPoint = ellipseCenter + directionToCenter * std::min(30.0f, (float)distEllipseToCenter);
            
            // 重新计算指针方向（从调整后的起点到椭圆边缘的最远点）
            cv::Point2f pointerDirection = endPoint - startPoint;
            float pointerLength = cv::norm(pointerDirection);
            if (pointerLength > 0) {
                pointerDirection = pointerDirection / pointerLength;
                endPoint = startPoint + pointerDirection * (float)length;
            }
        }
        
        // 计算得分：基于长度、位置和形状
        double lengthScore = std::min(length / (radius * 0.8), 1.0);  // 长度得分
        double positionScore = std::max(0.0, 1.0 - distToDialCenter / (radius * 0.3));  // 位置得分
        double shapeScore = std::min(aspectRatio / 5.0, 1.0);  // 形状得分
        
        double totalScore = lengthScore * 0.4 + positionScore * 0.4 + shapeScore * 0.2;
        
        if (totalScore > maxScore) {
            maxScore = totalScore;
            bestPointer = cv::Vec4i((int)startPoint.x, (int)startPoint.y, 
                                   (int)endPoint.x, (int)endPoint.y);
        }
    }
    
    // 6. 如果基于轮廓的方法失败，尝试基于亮度的射线方法
    if (bestPointer[0] == -1) {
        bestPointer = detectWhitePointerByBrightness(gray, center, radius);
    }
    
    qDebug() << "白色指针检测完成，最高得分:" << maxScore;
    return bestPointer;
}

cv::Vec4i highPreciseDetector::detectWhitePointerByBrightness(const cv::Mat& gray, const cv::Point2f& center, float radius) {
    cv::Vec4i bestPointer(-1, -1, -1, -1);
    double maxScore = 0;
    
    // 在多个角度方向搜索最亮的射线
    for (int angle = 0; angle < 360; angle += 2) {  // 更精细的角度搜索
        double radian = angle * CV_PI / 180.0;
        cv::Point2f direction(cos(radian), sin(radian));
        
        double totalBrightness = 0;
        int validPoints = 0;
        cv::Point2f farthestBrightPoint = center;
        std::vector<cv::Point2f> brightPoints;  // 记录所有亮点
        
        // 从表盘中心附近开始搜索（跳过中心区域，避免干扰）
        for (int step = 15; step < radius * 0.85; step += 2) {
            cv::Point2f currentPoint = center + direction * (float)step;
            
            if (currentPoint.x < 0 || currentPoint.x >= gray.cols ||
                currentPoint.y < 0 || currentPoint.y >= gray.rows) {
                break;
            }
            
            uchar brightness = gray.at<uchar>((int)currentPoint.y, (int)currentPoint.x);
            
            // 检测亮点（白色指针）
            if (brightness > 170) {  // 降低阈值，检测更多亮点
                totalBrightness += brightness;
                validPoints++;
                brightPoints.push_back(currentPoint);
                
                double distFromCenter = cv::norm(currentPoint - center);
                if (distFromCenter > cv::norm(farthestBrightPoint - center)) {
                    farthestBrightPoint = currentPoint;
                }
            }
        }
        
        // 计算这个方向的得分
        if (validPoints > 8) {  // 需要足够多的亮点
            double avgBrightness = totalBrightness / validPoints;
            double pointerLength = cv::norm(farthestBrightPoint - center);
            double continuity = (double)validPoints / (pointerLength / 2.0);  // 连续性得分
            
            // 综合评分：亮度 + 长度 + 连续性
            double score = (avgBrightness - 170) * 0.4 + 
                          std::min(pointerLength / (radius * 0.7), 1.0) * 100 * 0.4 + 
                          std::min(continuity, 1.0) * 100 * 0.2;
            
            if (score > maxScore && pointerLength > m_config->pointerMinLength) {
                maxScore = score;
                
                // 使用更精确的端点：找到亮点的质心作为起点
                cv::Point2f startPoint = center;
                if (!brightPoints.empty()) {
                    cv::Point2f centroid(0, 0);
                    float totalWeight = 0;
                    
                    // 计算亮点的加权质心，距离表盘中心近的点权重更大
                    for (const auto& point : brightPoints) {
                        float weight = 1.0f / (1.0f + cv::norm(point - center) / 50.0f);
                        centroid += point * weight;
                        totalWeight += weight;
                    }
                    
                    if (totalWeight > 0) {
                        centroid = centroid / totalWeight;
                        
                        // 如果质心距离表盘中心合理，使用质心作为起点
                        if (cv::norm(centroid - center) < radius * 0.4) {
                            startPoint = centroid;
                        }
                    }
                }
                
                bestPointer = cv::Vec4i((int)startPoint.x, (int)startPoint.y,
                                       (int)farthestBrightPoint.x, (int)farthestBrightPoint.y);
            }
        }
    }
    
    qDebug() << "基于亮度的白色指针检测完成，最高得分:" << maxScore;
    return bestPointer;
}

void highPreciseDetector::calculateAngle() {
    if (m_lines.empty()) {
        m_angle = -999;
        return;
    }
    
    cv::Vec4i line = m_lines[0];
    
    // 计算直线的角度（相对于水平方向）
    double dx = line[2] - line[0];
    double dy = line[3] - line[1];
    
    // 使用atan2计算角度，结果范围是 -π 到 π
    double angle_rad = atan2(dy, dx);
    
    // 转换为度数，范围 -180 到 180
    double angle_deg = angle_rad * 180.0 / CV_PI;
    
    // 转换为 0 到 360 度范围
    if (angle_deg < 0) {
        angle_deg += 360;
    }
    
    m_angle = angle_deg;
    qDebug() << "计算得到角度:" << m_angle << "度";
}

void highPreciseDetector::showScale1Result() {
    m_visual = m_image.clone();
    
    // 绘制检测到的圆形（表盘）
    for (const auto& circle : m_circles) {
        cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
        int radius = cvRound(circle[2]);
        // 绘制圆心（绿色）
        cv::circle(m_visual, center, 3, cv::Scalar(0, 255, 0), -1, 8, 0);
        // 绘制圆周（蓝色）
        cv::circle(m_visual, center, radius, cv::Scalar(255, 0, 0), 2, 8, 0);
    }
    
    // 绘制BYQ转轴中心（只在BYQ模式下且检测到转轴时显示）
    if (m_config && m_config->dialType == "BYQ" && m_axisCenter.x != -1 && m_axisCenter.y != -1 && m_axisRadius > 0) {
        cv::Point axisPoint(cvRound(m_axisCenter.x), cvRound(m_axisCenter.y));
        int axisRadiusInt = cvRound(m_axisRadius);
        
        // 绘制检测到的转轴圆（绿色圆圈，使用实际检测到的半径）
        cv::circle(m_visual, axisPoint, axisRadiusInt, cv::Scalar(0, 255, 0), 2, 8, 0);
        // 绘制转轴中心点（红色）
        cv::circle(m_visual, axisPoint, 3, cv::Scalar(0, 0, 255), -1, 8, 0);
        
        // 添加标注文字，显示半径信息
        std::string axisText = "Axis(R=" + std::to_string(axisRadiusInt) + ")";
        cv::putText(m_visual, axisText, cv::Point(axisPoint.x + axisRadiusInt + 5, axisPoint.y - 10), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    }
    
    // 绘制检测到的直线（指针）
    for (const auto& line : m_lines) {
        cv::line(m_visual, 
                cv::Point(line[0], line[1]), 
                cv::Point(line[2], line[3]), 
                cv::Scalar(0, 0, 255), 3, cv::LINE_AA);
    }
    
    // 添加角度文本
    if (m_angle != -999) {
        std::string angleText = "Angle: " + std::to_string(m_angle) + "°";
        cv::putText(m_visual, angleText, cv::Point(10, 30), 
                   cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 0), 2);
    }
}

void MainWindow::setupExpandedLayout()
{
    const int deskW = QGuiApplication::primaryScreen()->geometry().width();
    
    // 设置为展开布局（显示右侧检测结果区域和角度控制区域）
    int w = bigSize.width();
    
    // 设置窗口大小和位置
    setGeometry(
        ceil((deskW - w)/2),
        50,
        w,
        this->height()
    );
    
    setFixedSize(w, this->height());
    
    // 显示右侧检测结果区域和角度控制区域
    ui->destDisplay->setVisible(true);
    
    // 调整检测结果显示区域的尺寸和位置
    int h = (ui->destDisplay->width() * atoi(saveSettings->height.c_str()) / atoi(saveSettings->width.c_str()));
    ui->destDisplay->setGeometry(ui->destDisplay->geometry().x(), ui->srcDisplay->geometry().y(), ui->destDisplay->width(), h);
    
    // 调整源图像区域
    ui->srcFrame->setGeometry(ui->srcFrame->geometry().x(), ui->srcFrame->geometry().y(), 658, 531);
    ui->srcHeader->resize(658, ui->srcHeader->size().height());
    ui->srcBottomhorizontalLayout->setGeometry(QRect(0, ui->srcBottomhorizontalLayout->geometry().y(), 658, ui->srcHeader->size().height()));
}

void MainWindow::updatePointerDirection(double currentAngle) {
    if (!m_hasPreviousAngle) {
        // 第一次测量，记录当前角度
        m_previousAngle = currentAngle;
        m_hasPreviousAngle = true;
        m_strokeDirection = 0;  // 未知方向
        qDebug() << "初始化指针位置:" << currentAngle << "度";
        return;
    }
    
    // 计算角度变化，考虑360度跨越的情况
    double angleDelta = currentAngle - m_previousAngle;
    
    // 处理跨越360度边界的情况
    if (angleDelta > 180) {
        angleDelta -= 360;  // 实际是逆时针运动
    } else if (angleDelta < -180) {
        angleDelta += 360;  // 实际是顺时针运动
    }
    
    // 设置最小角度阈值，避免噪声影响
    const double minAngleThreshold = 2.0;  // 2度阈值
    
    if (abs(angleDelta) > minAngleThreshold) {
        if (angleDelta > 0) {
            // 顺时针运动 - 正行程
            m_strokeDirection = 1;
            m_isForwardStroke = true;
            qDebug() << "检测到正行程（顺时针），角度变化:" << angleDelta << "度";
        } else {
            // 逆时针运动 - 反行程
            m_strokeDirection = -1;
            m_isForwardStroke = false;
            qDebug() << "检测到反行程（逆时针），角度变化:" << angleDelta << "度";
        }
        
        // 更新上一次角度
        m_previousAngle = currentAngle;
    }
    // 如果角度变化太小，保持当前方向状态不变
}

QString MainWindow::getStrokeDirectionString() const {
    switch (m_strokeDirection) {
        case 1:
            return "正行程（顺时针）";
        case -1:
            return "反行程（逆时针）";
        case 0:
        default:
            return "待检测";
    }
}

void MainWindow::resetStrokeTracking() {
    m_hasPreviousAngle = false;
    m_previousAngle = 0.0;
    m_strokeDirection = 0;
    m_isForwardStroke = true;
    qDebug() << "重置行程跟踪状态";
}

double MainWindow::measureAngleMultipleTimes(const cv::Mat& frame, int measureCount) {
    std::vector<double> angles;
    
    qDebug() << "开始进行" << measureCount << "次角度测量以提高精度";
    
    // 进行多次测量
    for (int i = 0; i < measureCount; ++i) {
        try {
            highPreciseDetector det(frame, m_currentConfig);
            if (!det.getLine().empty()) {
                double angle = det.getAngle();
                if (angle != -999) {
                    angles.push_back(angle);
                    qDebug() << "第" << (i + 1) << "次测量角度:" << angle;
                } else {
                    qDebug() << "第" << (i + 1) << "次测量失败：角度计算错误";
                }
            } else {
                qDebug() << "第" << (i + 1) << "次测量失败：未检测到指针";
            }
        } catch (const std::exception& e) {
            qDebug() << "第" << (i + 1) << "次测量异常:" << e.what();
        }
    }
    
    if (angles.empty()) {
        qDebug() << "所有测量都失败";
        return -999;
    }
    
    if (angles.size() == 1) {
        qDebug() << "仅有1次有效测量，直接返回:" << angles[0];
        return angles[0];
    }
    
    // 异常值检测和过滤
    std::vector<double> validAngles;
    
    if (angles.size() >= 2) {
        // 计算所有角度的平均值和标准差
        double sum = 0;
        for (double angle : angles) {
            sum += angle;
        }
        double mean = sum / angles.size();
        
        double variance = 0;
        for (double angle : angles) {
            double diff = angle - mean;
            // 处理角度跨越360度边界的情况
            if (diff > 180) diff -= 360;
            if (diff < -180) diff += 360;
            variance += diff * diff;
        }
        double stdDev = sqrt(variance / angles.size());
        
        qDebug() << "初步统计 - 平均值:" << mean << "标准差:" << stdDev;
        
        // 异常值阈值：2倍标准差或最小5度
        double threshold = std::max(2.0 * stdDev, 5.0);
        
        // 过滤异常值
        for (double angle : angles) {
            double diff = angle - mean;
            // 处理角度跨越360度边界的情况
            if (diff > 180) diff -= 360;
            if (diff < -180) diff += 360;
            
            if (abs(diff) <= threshold) {
                validAngles.push_back(angle);
                qDebug() << "保留有效角度:" << angle << "偏差:" << diff;
            } else {
                qDebug() << "过滤异常角度:" << angle << "偏差:" << diff << "超过阈值:" << threshold;
            }
        }
    }
    
    // 如果过滤后没有足够的有效值，使用所有测量值
    if (validAngles.size() < 2) {
        qDebug() << "过滤后有效角度不足，使用所有测量值";
        validAngles = angles;
    }
    
    // 计算最终平均值
    double finalSum = 0;
    for (double angle : validAngles) {
        finalSum += angle;
    }
    double finalAverage = finalSum / validAngles.size();
    
    // 计算最终的精度评估
    double maxDiff = 0;
    for (double angle : validAngles) {
        double diff = abs(angle - finalAverage);
        if (diff > 180) diff = 360 - diff;  // 处理跨越边界的情况
        maxDiff = std::max(maxDiff, diff);
    }
    
    qDebug() << "最终结果 - 平均角度:" << finalAverage 
             << "有效测量次数:" << validAngles.size() 
             << "最大偏差:" << maxDiff << "度";
    
    return finalAverage;
}

// 初始化数据数组
void MainWindow::initializeDataArrays()
{
    m_forwardData.clear();
    m_reverseData.clear();
    m_currentForwardIndex = 0;
    m_currentReverseIndex = 0;
    m_saveCount = 0;
    m_maxAngle = 0.0;
    
    // 初始化显示为5个空数据位
    for (int i = 0; i < 5; ++i) {
        m_forwardData.append(0.0);
        m_reverseData.append(0.0);
    }
    
    updateDataTable();
}

// 更新数据表格显示
void MainWindow::updateDataTable()
{
    // 更新正行程数据显示（显示当前轮次的数据）
    QLabel* forwardLabels[5] = {
        ui->labelForwardData1, ui->labelForwardData2, ui->labelForwardData3,
        ui->labelForwardData4, ui->labelForwardData5
    };
    
    // 获取当前轮次的数据
    if (m_currentRound < m_allRoundsData.size()) {
        const RoundData &currentRound = m_allRoundsData[m_currentRound];
        
        for (int i = 0; i < 5; ++i) {
            if (i < currentRound.forwardAngles.size()) {
                // 第一个位置：显示0.0度（归位数据）或其他数值
                // 其他位置：只有非零数据才显示
                bool shouldShow = (i == 0) || (currentRound.forwardAngles[i] != 0.0);
                
                if (shouldShow) {
                    forwardLabels[i]->setText(QString::number(currentRound.forwardAngles[i], 'f', 2) + "°");
                    forwardLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #d4edda; }");
                } else {
                    forwardLabels[i]->setText("采集数据" + QString::number(i + 1));
                    forwardLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #f8f9fa; }");
                }
            } else {
                forwardLabels[i]->setText("采集数据" + QString::number(i + 1));
                forwardLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #f8f9fa; }");
            }
        }
    } else {
        // 如果当前轮次无效，显示空数据
        for (int i = 0; i < 5; ++i) {
            forwardLabels[i]->setText("采集数据" + QString::number(i + 1));
            forwardLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #f8f9fa; }");
        }
    }
    
    // 更新反行程数据显示（显示当前轮次的数据）
    QLabel* reverseLabels[5] = {
        ui->labelReverseData1, ui->labelReverseData2, ui->labelReverseData3,
        ui->labelReverseData4, ui->labelReverseData5
    };
    
    if (m_currentRound < m_allRoundsData.size()) {
        const RoundData &currentRound = m_allRoundsData[m_currentRound];
        
        for (int i = 0; i < 5; ++i) {
            if (i < currentRound.backwardAngles.size() && currentRound.backwardAngles[i] != 0.0) {
                reverseLabels[i]->setText(QString::number(currentRound.backwardAngles[i], 'f', 2) + "°");
                reverseLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #cce7ff; }");
            } else {
                reverseLabels[i]->setText("采集数据" + QString::number(i + 1));
                reverseLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #f8f9fa; }");
            }
        }
        
        // 更新最大角度显示（使用当前轮次的最大角度）
        if (currentRound.maxAngle != 0.0) {
            ui->labelMaxAngleValue->setText(QString::number(currentRound.maxAngle, 'f', 2) + "°");
        } else {
            ui->labelMaxAngleValue->setText("--");
        }
    } else {
        // 如果当前轮次无效，显示空数据
        for (int i = 0; i < 5; ++i) {
            reverseLabels[i]->setText("采集数据" + QString::number(i + 1));
            reverseLabels[i]->setStyleSheet("QLabel { border: 1px solid #ccc; padding: 3px; background-color: #f8f9fa; }");
        }
        ui->labelMaxAngleValue->setText("--");
    }
    
    // 更新当前轮次显示
    ui->labelSaveCount->setText(QString("当前轮次：第%1轮/共5轮").arg(m_currentRound + 1));
}

// 添加数据到当前行程
void MainWindow::addDataToCurrentStroke(double angle)
{
    if (m_strokeDirection == 1 && m_currentForwardIndex < 5) {
        // 正行程
        m_forwardData[m_currentForwardIndex] = angle;
        m_currentForwardIndex++;
        qDebug() << "添加正行程数据：" << angle << "当前索引：" << m_currentForwardIndex;
    } else if (m_strokeDirection == -1 && m_currentReverseIndex < 5) {
        // 反行程
        m_reverseData[m_currentReverseIndex] = angle;
        m_currentReverseIndex++;
        qDebug() << "添加反行程数据：" << angle << "当前索引：" << m_currentReverseIndex;
    } else {
        qDebug() << "无法添加数据，行程方向：" << m_strokeDirection 
                 << "正行程索引：" << m_currentForwardIndex 
                 << "反行程索引：" << m_currentReverseIndex;
    }
    
    updateDataTable();
}

// 确定按钮点击处理 - 添加当前测量数据
void MainWindow::onConfirmData()
{
    if (!m_hasZero) {
        QMessageBox::warning(this, "警告", "请先进行归位操作！");
        return;
    }
    
    // 获取当前角度差值
    cv::Mat frame;
    if (!grabOneFrame(frame)) {
        QMessageBox::warning(this, "警告", "无法获取图像！");
        return;
    }
    
    // 使用高精度测量
    double currentAngle = measureAngleMultipleTimes(frame);
    double angleDelta = currentAngle - m_zeroAngle;
    
    // 处理角度跨越边界
    if (angleDelta > 180) angleDelta -= 360;
    if (angleDelta < -180) angleDelta += 360;
    
    // 添加到当前轮次数据中
    addAngleToCurrentRound(abs(angleDelta), m_isForwardStroke);
    
    // 同时更新误差表格（如果打开的话）
    updateErrorTableWithAllRounds();
    
    QMessageBox::information(this, "确认", 
        QString("已添加数据（%1°）到第%2轮 %3 第%4个检测点")
        .arg(abs(angleDelta), 0, 'f', 2)
        .arg(m_currentRound + 1)
        .arg(m_isForwardStroke ? "正行程" : "反行程")
        .arg(m_currentDetectionPoint + 1));
}

// 保存按钮点击处理 - 当前轮次+1
void MainWindow::onSaveData()
{
    if (m_currentRound >= 4) {  // 如果已经是第5轮，不能再增加
        QMessageBox::information(this, "提示", "已经是最后一轮（第5轮）！");
        return;
    }
    
    // 标记当前轮次为完成
    if (m_currentRound < m_allRoundsData.size()) {
        m_allRoundsData[m_currentRound].isCompleted = true;
    }
    
    // 进入下一轮
    int oldRound = m_currentRound;
    m_currentRound++;
    
    QMessageBox::information(this, "保存", 
        QString("第%1轮已完成！\n开始第%2轮测量")
        .arg(oldRound + 1)
        .arg(m_currentRound + 1));
    
    // 同时更新误差表格（如果打开的话）
    updateErrorTableWithAllRounds();
    
    // 更新数据显示
    updateDataTable();
    
    qDebug() << "从第" << (oldRound + 1) << "轮推进到第" << (m_currentRound + 1) << "轮";
}

// ================== BYQ指针检测算法实现 ==================
cv::Vec4i highPreciseDetector::detectBYQPointer(const cv::Mat& gray, const cv::Point2f& center, float radius) {
    qDebug() << "开始BYQ指针检测";
    
    // 初始化转轴中心
    m_axisCenter = cv::Point2f(-1, -1);
    
    // 1. 首先检测转轴中心
    cv::Point2f axisCenter = detectBYQAxis(gray, center, radius);
    
    if (axisCenter.x == -1) {
        qDebug() << "未找到BYQ转轴中心，使用表盘中心";
        axisCenter = center;
    } else {
        // 保存转轴中心用于可视化
        m_axisCenter = axisCenter;
    }
    
    // 2. 检测银色指针末端
    cv::Vec4i silverEnd = detectSilverPointerEnd(gray, axisCenter, center, radius);
    
    if (silverEnd[0] != -1) {
        qDebug() << "BYQ指针检测成功";
        return silverEnd;
    }
    
    qDebug() << "BYQ指针检测失败";
    return cv::Vec4i(-1, -1, -1, -1);
}

cv::Point2f highPreciseDetector::detectBYQAxis(const cv::Mat& gray, const cv::Point2f& dialCenter, float dialRadius) {
    qDebug() << "检测BYQ螺旋波登管转轴中心，表盘中心:(" << dialCenter.x << "," << dialCenter.y << ") 半径:" << dialRadius;
    
    // 硬编码转轴中心位置：表盘圆心向下90像素
    cv::Point2f hardcodedAxisCenter(dialCenter.x, dialCenter.y + 90.0f);
    m_axisRadius = 48.0f;  // 硬编码转轴半径
    
    qDebug() << "硬编码转轴中心: (" << hardcodedAxisCenter.x << "," << hardcodedAxisCenter.y << ") 半径:" << m_axisRadius;
    
    return hardcodedAxisCenter;
}

cv::Vec4i highPreciseDetector::detectSilverPointerEnd(const cv::Mat& gray, const cv::Point2f& axisCenter, const cv::Point2f& dialCenter, float dialRadius) {
    qDebug() << "检测银色指针末端 - 简化版";
    
    // 1. 创建环形掩码，排除转轴附近区域
    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::circle(mask, cv::Point((int)dialCenter.x, (int)dialCenter.y), (int)(dialRadius * m_config->pointerMaskRadius), cv::Scalar(255), -1);
    // 排除转轴附近的圆形区域，避免干扰
    cv::circle(mask, cv::Point((int)axisCenter.x, (int)axisCenter.y), (int)(m_axisRadius * m_config->axisExcludeMultiplier), cv::Scalar(0), -1);
    
    // 2. 应用掩码获取感兴趣区域
    cv::Mat roiGray;
    gray.copyTo(roiGray, mask);
    
    // 3. 针对细指针的预处理 - 增强与白色背景的反差
    cv::Mat enhanced;
    
    // 3.1 反转图像，让暗色指针变成亮线（更容易检测）
    cv::bitwise_not(roiGray, enhanced, mask);
    
    // 3.2 形态学操作增强细线
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(m_config->morphKernelWidth, m_config->morphKernelHeight));
    cv::morphologyEx(enhanced, enhanced, cv::MORPH_CLOSE, kernel);
    
    // 3.3 轻微的高斯滤波减少噪声
    cv::GaussianBlur(enhanced, enhanced, cv::Size(m_config->gaussianKernelSize, m_config->gaussianKernelSize), m_config->gaussianSigma);
    
    // 4. 边缘检测 - 使用配置的参数检测细线
    cv::Mat edges;
    cv::Canny(enhanced, edges, m_config->cannyLowThreshold, m_config->cannyHighThreshold);
    
    // 5. HoughLinesP检测 - 使用配置的参数
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, 
                    1.0,                                        // rho: 1像素精度
                    CV_PI/180,                                 // theta: 1度精度
                    m_config->houghThreshold,                  // 使用配置的阈值
                    dialRadius * m_config->minLineLengthRatio, // 使用配置的最小长度比例
                    dialRadius * m_config->maxLineGapRatio);   // 使用配置的最大间隙比例
    
    qDebug() << "HoughLinesP检测找到" << lines.size() << "条直线段";
    
    if (lines.empty()) {
        qDebug() << "未检测到指针直线段";
        return cv::Vec4i(-1, -1, -1, -1);
    }
    
    // 6. 简单逻辑：找最长的直线，连接到转轴中心
    cv::Vec4i bestLine(-1, -1, -1, -1);
    double maxLength = 0;
    
    for (const auto& line : lines) {
        cv::Point2f p1(line[0], line[1]);
        cv::Point2f p2(line[2], line[3]);
        
        double length = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
        
        if (length > maxLength) {
            maxLength = length;
            bestLine = line;
        }
    }
    
    if (bestLine[0] != -1) {
        cv::Point2f p1(bestLine[0], bestLine[1]);
        cv::Point2f p2(bestLine[2], bestLine[3]);
        
        // 找出距离转轴更远的点
        double dist1 = sqrt(pow(p1.x - axisCenter.x, 2) + pow(p1.y - axisCenter.y, 2));
        double dist2 = sqrt(pow(p2.x - axisCenter.x, 2) + pow(p2.y - axisCenter.y, 2));
        
        cv::Point2f farPoint = (dist1 > dist2) ? p1 : p2;
        
        // 直接从转轴中心连接到远点
        bestLine[0] = (int)axisCenter.x;
        bestLine[1] = (int)axisCenter.y;
        bestLine[2] = (int)farPoint.x;
        bestLine[3] = (int)farPoint.y;
        
        qDebug() << "找到最长直线，从转轴中心连接: (" << bestLine[0] << "," << bestLine[1] 
                 << ") 到 (" << bestLine[2] << "," << bestLine[3] << ")";
    }
    
    return bestLine;
}

// 测量并保存最大角度
void MainWindow::measureAndSaveMaxAngle()
{
    if (!m_hasZero) {
        QMessageBox::warning(this, "警告", "请先进行归位操作！");
        return;
    }
    
    // 获取当前图像
    cv::Mat frame;
    if (!grabOneFrame(frame)) {
        QMessageBox::warning(this, "警告", "无法获取图像！");
        return;
    }
    
    // 多次测量取平均值，提高精度
    double currentAngle = measureAngleMultipleTimes(frame, 5);  // 测量5次取平均
    double angleDelta = currentAngle - m_zeroAngle;
    
    // 处理角度跨越边界
    if (angleDelta > 180) angleDelta -= 360;
    if (angleDelta < -180) angleDelta += 360;
    
    // 保存绝对值角度
    double maxAngle = abs(angleDelta);
    m_maxAngle = maxAngle;
    
    // 如果误差检测表格窗口打开，将最大角度数据传递给它
    if (m_errorTableDialog) {
        m_errorTableDialog->addMaxAngleData(maxAngle);
    }
    
    // 更新界面显示
    updateMaxAngleDisplay();
    
    qDebug() << "最大角度测量完成:" << maxAngle << "度";
    
    QMessageBox::information(this, "最大角度测量", 
        QString("最大角度测量完成\n当前角度: %1°\n平均最大角度: %2°")
        .arg(maxAngle, 0, 'f', 2)
        .arg(m_errorTableDialog ? m_errorTableDialog->calculateAverageMaxAngle() : maxAngle, 0, 'f', 2));
}

// 更新最大角度显示
void MainWindow::updateMaxAngleDisplay()
{
    if (m_errorTableDialog) {
        double avgMaxAngle = m_errorTableDialog->calculateAverageMaxAngle();
        // 这里可以更新界面上的最大角度显示标签
        // 例如: ui->labelMaxAngle->setText(QString("平均最大角度: %1°").arg(avgMaxAngle, 0, 'f', 2));
        qDebug() << "当前轮次最大角度:" << m_maxAngle << "平均最大角度:" << avgMaxAngle;
    }
}

// ================== 5轮数据管理方法实现 ==================

void MainWindow::initializeRoundsData()
{
    qDebug() << "初始化5轮数据结构";
    
    // 初始化5轮数据
    m_allRoundsData.clear();
    m_allRoundsData.resize(5);
    
    // 根据当前表盘类型设置测量次数和检测点
    if (m_currentDialType == "YYQY-13") {
        m_maxMeasurementsPerRound = 6;
        m_detectionPoints = {0.0, 0.6, 1.2, 1.8, 2.4, 3.0};
    } else if (m_currentDialType == "BYQ-19") {
        m_maxMeasurementsPerRound = 5;
        m_detectionPoints = {0.0, 0.75, 1.5, 2.25, 3.0};
    } else {
        // 默认配置
        m_maxMeasurementsPerRound = 6;
        m_detectionPoints = {0.0, 1.0, 2.0, 3.0};
    }
    
    // 为每轮初始化数据结构
    for (int round = 0; round < 5; ++round) {
        m_allRoundsData[round].forwardAngles.resize(m_maxMeasurementsPerRound);
        m_allRoundsData[round].backwardAngles.resize(m_maxMeasurementsPerRound);
        m_allRoundsData[round].forwardAngles.fill(0.0);
        m_allRoundsData[round].backwardAngles.fill(0.0);
        m_allRoundsData[round].maxAngle = 0.0;
        m_allRoundsData[round].isCompleted = false;
    }
    
    // 重置状态
    m_currentRound = 0;
    m_currentDetectionPoint = 0;
    
    qDebug() << "5轮数据结构初始化完成，表盘类型:" << m_currentDialType 
             << "每轮测量次数:" << m_maxMeasurementsPerRound
             << "检测点数量:" << m_detectionPoints.size();
}

void MainWindow::addAngleToCurrentRound(double angle, bool isForward)
{
    if (m_currentRound >= m_allRoundsData.size()) {
        qDebug() << "当前轮次超出范围:" << m_currentRound;
        return;
    }
    
    RoundData &currentRound = m_allRoundsData[m_currentRound];
    
    if (isForward) {
        // 找到第一个空的正行程位置
        for (int i = 0; i < currentRound.forwardAngles.size(); ++i) {
            if (currentRound.forwardAngles[i] == 0.0 && i == 0) {
                // 第一个位置可以是0.0（零位）
                currentRound.forwardAngles[i] = angle;
                qDebug() << "添加第" << (m_currentRound + 1) << "轮正行程第" << (i + 1) << "次数据:" << angle;
                break;
            } else if (currentRound.forwardAngles[i] == 0.0 && angle != 0.0) {
                // 其他位置不能是0.0，除非确实是零位
                currentRound.forwardAngles[i] = angle;
                qDebug() << "添加第" << (m_currentRound + 1) << "轮正行程第" << (i + 1) << "次数据:" << angle;
                break;
            }
        }
    } else {
        // 找到第一个空的反行程位置
        for (int i = 0; i < currentRound.backwardAngles.size(); ++i) {
            if (currentRound.backwardAngles[i] == 0.0 && angle != 0.0) {
                currentRound.backwardAngles[i] = angle;
                qDebug() << "添加第" << (m_currentRound + 1) << "轮反行程第" << (i + 1) << "次数据:" << angle;
                break;
            }
        }
    }
    
    // 更新状态显示
    updateDataTable();
}

void MainWindow::updateErrorTableWithAllRounds()
{
    if (!m_errorTableDialog) {
        return;  // 如果表格窗口没有打开，就不更新
    }
    
    qDebug() << "开始同步5轮数据到误差表格";
    
    // 简化同步逻辑：直接传递数据而不是通过addAngleData方法
    // 这需要在ErrorTableDialog中添加一个直接设置多轮数据的方法
    
    // 暂时保持现有的逻辑，但只同步当前主界面的轮次
    m_errorTableDialog->setCurrentRound(m_currentRound);
    
    // 为当前轮次同步数据
    if (m_currentRound < m_allRoundsData.size()) {
        const RoundData &currentRoundData = m_allRoundsData[m_currentRound];
        
        // 简单起见，我们只同步非零数据
        for (int i = 0; i < currentRoundData.forwardAngles.size(); ++i) {
            if (currentRoundData.forwardAngles[i] != 0.0 || (i == 0 && currentRoundData.forwardAngles[i] == 0.0)) {
                // 这里需要设置检测点，但简化逻辑，我们假设只有一个检测点
                if (!m_detectionPoints.isEmpty()) {
                    m_errorTableDialog->setCurrentPressurePoint(m_detectionPoints[0]);
                    m_errorTableDialog->addAngleData(currentRoundData.forwardAngles[i], true);
                }
            }
        }
        
        for (int i = 0; i < currentRoundData.backwardAngles.size(); ++i) {
            if (currentRoundData.backwardAngles[i] != 0.0) {
                if (!m_detectionPoints.isEmpty()) {
                    m_errorTableDialog->setCurrentPressurePoint(m_detectionPoints[0]);
                    m_errorTableDialog->addAngleData(currentRoundData.backwardAngles[i], false);
                }
            }
        }
        
        // 添加最大角度数据
        if (currentRoundData.maxAngle != 0.0) {
            m_errorTableDialog->addMaxAngleData(currentRoundData.maxAngle);
        }
    }
    
    qDebug() << "当前轮次数据同步完成";
}

void MainWindow::setCurrentDetectionPoint(int pointIndex)
{
    if (pointIndex >= 0 && pointIndex < m_detectionPoints.size()) {
        m_currentDetectionPoint = pointIndex;
        qDebug() << "切换到检测点" << (pointIndex + 1) << ":" << m_detectionPoints[pointIndex] << "MPa";
    }
}

QString MainWindow::getCurrentStatusInfo() const
{
    return QString("第%1轮 检测点%2 (%3MPa) %4")
        .arg(m_currentRound + 1)
        .arg(m_currentDetectionPoint + 1)
        .arg(m_detectionPoints.isEmpty() ? 0.0 : m_detectionPoints[m_currentDetectionPoint])
        .arg(m_isForwardStroke ? "正行程" : "反行程");
}