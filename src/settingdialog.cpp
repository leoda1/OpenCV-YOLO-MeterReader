#include "settingdialog.h"
#include "ui_settingdialog.h"

SettingDialog::SettingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingDialog),
    saveSettings(std::make_unique<Settings>())
{
    ui->setupUi(this);
    setWindowTitle("软件设置");
    setFixedSize(this->width(), this->height());
    connect(ui->selectDir, &QPushButton::clicked, this, &SettingDialog::setDir);
}

SettingDialog::~SettingDialog()
{
    delete ui;
}

void SettingDialog::setSettings(const Settings* settings) {
    if (!settings) {
        qDebug() << "警告: 传入的settings指针为空";
        return;
    }
    
    // 复制设置到本地对象
    *saveSettings = *settings;
    
    // 更新UI显示
    ui->exposureTime->setText(settings->exposureTime.c_str());
    ui->fps->setText(settings->acquisitionFrameRate.c_str());
    ui->filePrefix->setText(settings->FilePrefix);
    ui->filePath->setText(settings->FilePath);
    ui->image2save->setText(QString::number(settings->image2save));
}

const Settings* SettingDialog::getSaveSettings() const {
    return saveSettings.get();
}

void SettingDialog::setDir() {
    QString dir = QFileDialog::getExistingDirectory(
        this, 
        tr("打开目录"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        ui->filePath->setText(dir);
    }
}

void SettingDialog::on_buttonBox_accepted() {
    qDebug() << "设置对话框被接受";
    
    // 验证输入
    if (ui->filePrefix->text().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "文件前缀不能为空");
        return;
    }
    
    if (ui->filePath->text().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "文件路径不能为空");
        return;
    }
    
    // 更新设置
    saveSettings->FilePrefix = ui->filePrefix->text();
    saveSettings->acquisitionFrameRate = ui->fps->text().toUtf8().constData();
    saveSettings->exposureTime = ui->exposureTime->text().toUtf8().constData();
    saveSettings->FilePath = ui->filePath->text();
    
    bool ok;
    int imageCount = ui->image2save->text().toInt(&ok);
    if (ok && imageCount > 0) {
        saveSettings->image2save = imageCount;
    } else {
        QMessageBox::warning(this, "输入错误", "图像数量必须是正整数");
        return;
    }

    qDebug() << "分辨率索引:" << ui->resolution->currentIndex();

    // 设置分辨率
    GenICam_3_1_Basler_pylon_v3::gcstring width;
    GenICam_3_1_Basler_pylon_v3::gcstring height;
    
    switch (ui->resolution->currentIndex()) {
        case 0:
            width = "658";
            height = "492";
            break;
        case 1:
            width = "600";
            height = "400";
            break;
        default:
            width = "300";
            height = "200";
            break;
    }
    
    // 如果用户手动输入了尺寸，使用用户输入的值
    if (!ui->cameraWidth->text().isEmpty()) {
        width = ui->cameraWidth->text().toUtf8().constData();
    }
    if (!ui->cameraHeight->text().isEmpty()) {
        height = ui->cameraHeight->text().toUtf8().constData();
    }

    saveSettings->width = width;
    saveSettings->height = height;

    // 设置图像格式
    if (ui->imageFormat->currentIndex() == 0) {
        saveSettings->format = Pylon::ImageFileFormat_Png;
    } else if (ui->imageFormat->currentIndex() == 1) {
        saveSettings->format = Pylon::ImageFileFormat_Tiff;
    }

    // 设置自定义属性
    if (!ui->customAttr->text().isEmpty() && !ui->customValue->text().isEmpty()) {
        saveSettings->myattr = ui->customAttr->text();
        saveSettings->myvalue = ui->customValue->text();
        saveSettings->type = ui->customType->currentIndex();
    }
}

