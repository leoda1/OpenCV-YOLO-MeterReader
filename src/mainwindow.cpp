#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>

namespace {
constexpr auto kStartText  = u8"开始采集";
constexpr auto kStopText   = u8"停止采集";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_control(std::make_unique<SBaslerCameraControl>(this))
{
    ui->setupUi(this);
    ui->labelPreview->setScaledContents(true);

    initConnections();
    m_control->initSome();

    const QStringList cams = m_control->cameras();
    if (cams.isEmpty()) {
        QMessageBox::critical(this, tr("错误"), tr("未检测到 Basler 相机！"));
        return;
    }

    if (m_control->OpenCamera(cams.first()) != 0) {
        QMessageBox::critical(this, tr("错误"), tr("打开相机失败！"));
        return;
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initConnections()
{
    using namespace std::placeholders;
    connect(m_control.get(), &SBaslerCameraControl::sigCurrentImage,
            this, &MainWindow::slotUpdateCurrentImage, Qt::QueuedConnection);

    connect(m_control.get(), &SBaslerCameraControl::sigSizeChange,
            this, &MainWindow::slotCameraSizeChanged, Qt::QueuedConnection);
}

void MainWindow::slotUpdateCurrentImage(const QImage& img)
{
    if (img.isNull()) {
        qWarning() << "slotUpdateCurrentImage: received null image";
        return;
    }
    QPixmap pix = QPixmap::fromImage(img);
    ui->labelPreview->setPixmap(pix);
    ui->labelPreview->setFixedSize(pix.size());        // 解决只显示左上角的问题
}

void MainWindow::slotCameraSizeChanged(const QSize& size)
{
    ui->label_size->setText(QStringLiteral("尺寸：%1 x %2")
                            .arg(size.width()).arg(size.height()));
    this->setMaximumSize(size + QSize(60, 60));        // 预留边框空间
}

void MainWindow::on_pushButton_getExTime_clicked()
{
    ui->label_exTime->setText(QString::number(m_control->getExposureTime()));
}

void MainWindow::on_pushButton_SetExTime_clicked()
{
    m_control->setExposureTime(ui->lineEdit_exTime->text().toDouble());
}

void MainWindow::on_pushButton_SetMode_clicked()
{
    m_control->setFeatureTriggerSourceType(ui->lineEdit_SetMode->text());
}

void MainWindow::on_pushButton_GetMode_clicked()
{
    ui->label_Mode->setText(m_control->getFeatureTriggerSourceType());
}

void MainWindow::on_pushButton_CFMode_clicked()
{
    ui->label_CFMode->setText(m_control->getFeatureTriggerModeType()?"Open":"Close");
}

void MainWindow::on_comboBox_CFMode_activated(int index)
{
    m_control->setFeatureTriggerModeType(index == 0);
}

void MainWindow::on_pushButton_Start_clicked()
{
    if (!m_acquiring) {
        if (m_control->StartAcquire() == 0) {
            m_acquiring = true;
            ui->pushButton_Start->setText(kStopText);
        }
    } else {
        m_control->StopAcquire();
        m_acquiring = false;
        ui->pushButton_Start->setText(kStartText);
    }
}

