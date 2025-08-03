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

    // 为重要的UI元素设置更大的字体
    QFont bigFont = this->font();
    bigFont.setPointSize(16);
    
    // 设置预览区域标题字体
    ui->srcHeader->setFont(bigFont);
    
    // 设置状态标签字体
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
    
    // 按钮样式已在UI文件中设置，这里只需要确保字体一致性
    // UI文件中已经设置了按钮的样式和字体

    connect(ui->actionPreview, &QAction::triggered, this, &MainWindow::startPreview);
    connect(ui->actionRefresh, &QAction::triggered, this, &MainWindow::refresh);
    connect(ui->actionSetting, &QAction::triggered, this, &MainWindow::setting);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);
    connect(ui->actionCollectSingle, &QAction::triggered, this, &MainWindow::singleGrab);
    connect(ui->actionCollectMulti, &QAction::triggered, this, &MainWindow::multiGrab);
    connect(ui->actionSpaceAlgo, &QAction::triggered, this, &MainWindow::showDialMarkDialog);
    connect(ui->actionErrorTable, &QAction::triggered, this, &MainWindow::showErrorTableDialog);
    connect(ui->pushResetZero, &QPushButton::clicked, this, &MainWindow::onResetZero);
    connect(ui->pushCaptureZero, &QPushButton::clicked, this, &MainWindow::onCaptureZero);

    QToolBar *mytoolbar = new QToolBar(this);
    mytoolbar->addAction(ui->actionCloseAlgo);
    mytoolbar->setIconSize(QSize(48, 48));
    addToolBar(Qt::RightToolBarArea,mytoolbar);
    connect(ui->actionCloseAlgo, &QAction::triggered, this, &MainWindow::algoArea);

    // 设置表盘类型选择器
    setupDialTypeSelector();

    smallSize = QSize(750,this->height());
    bigSize = this->size();
    
    // 程序启动时隐藏角度检测区域
    ui->angleControlWidget->setVisible(false);
    algoArea();
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
    
    ui->actionPreview->setEnabled(false);

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
    ui->actionPreview->setEnabled(!inPreview);
    ui->actionCollectSingle->setEnabled(true);
    ui->actionCollectMulti->setEnabled(true);

    //    ui->actionSpaceAlgo->setEnabled(!inPreview);

}

void MainWindow::setNoCamera(){
    ui->actionPreview->setEnabled(false);
    ui->actionCollectSingle->setEnabled(false);
    ui->actionCollectMulti->setEnabled(false);
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
    const int deskW = QGuiApplication::primaryScreen()->geometry().width();
    
    // 设置窗口更新模式，减少闪动
    this->setUpdatesEnabled(false);
    
    if(isAlgoAreaOpened){
        // 关闭识别角度模式
        isAlgoAreaOpened = false;
        ui->actionCloseAlgo->setText("关闭识别角度");

        int w = bigSize.width();

        // 先调整窗口大小和位置
        setGeometry(
            ceil((deskW - w)/2),
            50,
            w,
            this->height()
        );

        setFixedSize(w, this->height());
        
        // 显示右侧检测结果区域和角度控制区域
        ui->destDisplay->setHidden(false);
        ui->angleControlWidget->setVisible(true);

        // 调整检测结果显示区域的尺寸和位置
        int h = (ui->destDisplay->width() * atoi(saveSettings->height.c_str()) / atoi(saveSettings->width.c_str()));
        ui->destDisplay->setGeometry(ui->destDisplay->geometry().x(), ui->srcDisplay->geometry().y(), ui->destDisplay->width(), h);
        
        // 调整源图像区域
        ui->srcFrame->setGeometry(ui->srcFrame->geometry().x(), ui->srcFrame->geometry().y(), 658, 531);
        ui->srcHeader->resize(658, ui->srcHeader->size().height());
        ui->srcBottomhorizontalLayout->setGeometry(QRect(0, ui->srcBottomhorizontalLayout->geometry().y(), 658, ui->srcHeader->size().height()));

    }else{
        // 打开识别角度模式
        isAlgoAreaOpened = true;
        ui->actionCloseAlgo->setText("识别角度");

        int w = smallSize.width() + 20;
        
        // 先调整窗口大小和位置
        setGeometry(
            ceil((deskW - w)/2),
            50,
            w,
            this->height()
        );

        setFixedSize(w, this->height());
        
        // 隐藏右侧检测结果区域和角度控制区域
        ui->destDisplay->setHidden(true);
        ui->angleControlWidget->setVisible(false);
        
        // 调整源图像区域
        ui->srcFrame->setGeometry(ui->srcFrame->geometry().x(), ui->srcFrame->geometry().y(), 658, 531);
        ui->srcBottomhorizontalLayout->setGeometry(QRect(0, ui->srcBottomhorizontalLayout->geometry().y(), 658, ui->srcHeader->size().height()));
        ui->srcHeader->resize(658, ui->srcHeader->size().height());
    }
    
    // 重新启用窗口更新
    this->setUpdatesEnabled(true);
    this->update();
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

        highPreciseDetector det(frame);
        if (det.getLine().empty()) {
            QMessageBox::warning(this, "提示", "未检测到指针");
            return;
        }
        
        double now = det.getAngle();
        if (now == -999) {
            QMessageBox::warning(this, "错误", "计算角度失败");
            return;
        }

        // 计算角度差
        double delta = now - m_zeroAngle;
        if (delta > 180)  delta -= 360;
        if (delta < -180) delta += 360;

        qDebug() << "零位:" << m_zeroAngle << "当前:" << now << "角度差:" << delta;

        ui->labelAngle->setText(
            QString("零位: %1°\n当前: %2°\n角度差: %3°")
                .arg(m_zeroAngle, 0, 'f', 2)
                .arg(now, 0, 'f', 2)
                .arg(delta, 0, 'f', 2));

        // 右侧显示检测示意图
        det.showScale1Result();
        cv::Mat vis = det.visual();
        cv::cvtColor(vis, vis, cv::COLOR_BGR2RGB);
        QImage q(vis.data, vis.cols, vis.rows, vis.step, QImage::Format_RGB888);
        ui->destDisplay->setPixmap(QPixmap::fromImage(q)
                                   .scaled(ui->destDisplay->size(),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        
    } catch (const std::exception& e) {
        qDebug() << "测量角度时发生异常:" << e.what();
        QMessageBox::critical(this, "错误", QString("测量角度失败: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "测量角度时发生未知异常";
        QMessageBox::critical(this, "错误", "测量角度时发生未知错误");
    }
}

// 改进后的 grabOneFrame 函数
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
        highPreciseDetector det(frame);
        
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
        
        qDebug() << "零位设置成功，角度:" << m_zeroAngle;
        
        // 更新界面显示
        ui->labelAngle->setText(QString("零位已设置: %1°").arg(angle, 0, 'f', 2));
        
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

        highPreciseDetector det(bgr);
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
            QString txt;
            if (m_hasZero) {
                double delta = angle - m_zeroAngle;
                if (delta > 180)  delta -= 360;
                if (delta < -180) delta += 360;
                txt = QString("零位: %1°\n当前: %2°\n角度差: %3°")
                        .arg(m_zeroAngle, 0, 'f', 2)
                        .arg(angle, 0, 'f', 2)
                        .arg(delta, 0, 'f', 2);
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
        // 每次创建新的对话框实例，避免重用导致的问题
        auto dialog = new ErrorTableDialog(this);
        
        // 设置对话框关闭时自动删除
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        
        // 根据当前选择的表盘类型设置默认配置
        if (!m_currentDialType.isEmpty()) {
            dialog->setDialType(m_currentDialType);
        }
        
        // 显示对话框，但不强制置顶
        dialog->show();
        // 不调用 raise() 和 activateWindow()，让用户可以自由切换窗口焦点
        
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

void MainWindow::onDialTypeChanged(const QString &dialType)
{
    m_currentDialType = dialType;
    qDebug() << "表盘类型切换为:" << dialType;
    
    // 可以在这里添加其他需要根据表盘类型变化的逻辑
}

// highPreciseDetector 类的实现 - 添加到 mainwindow.cpp 文件中

highPreciseDetector::highPreciseDetector(const cv::Mat& image) : m_angle(-999) {
    if (image.empty()) {
        qDebug() << "输入图像为空";
        return;
    }
    
    // 复制输入图像
    m_image = image.clone();
    m_visual = image.clone();
    
    try {
        // 检测圆形和直线
        detectCircles();
        detectLines();
        
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
    
    // 使用高斯模糊减少噪声
    cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2, 2);
    
    // 使用HoughCircles检测圆形
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1, gray.rows/16.0, 100, 30, 1, 0);
    
    // 选择最大的圆作为表盘
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
        qDebug() << "检测到表盘: 中心(" << maxCircle[0] << "," << maxCircle[1] << ") 半径:" << maxRadius;
    } else {
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
    
    // 边缘检测
    cv::Canny(gray, edges, 50, 150, 3);
    
    // 使用HoughLinesP检测直线
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, 1, CV_PI/180, 50, 50, 10);
    
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
    
    // 绘制检测到的圆形
    for (const auto& circle : m_circles) {
        cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
        int radius = cvRound(circle[2]);
        // 绘制圆心
        cv::circle(m_visual, center, 3, cv::Scalar(0, 255, 0), -1, 8, 0);
        // 绘制圆周
        cv::circle(m_visual, center, radius, cv::Scalar(255, 0, 0), 2, 8, 0);
    }
    
    // 绘制检测到的直线
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