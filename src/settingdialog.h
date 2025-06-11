#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QtWidgets>
#include <memory>
#include "settings.h"

namespace Ui {
class SettingDialog;
}

class SettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingDialog(QWidget *parent = nullptr);
    ~SettingDialog();
    
    // 返回const指针，防止外部意外修改
    const Settings* getSaveSettings() const;
    void setSettings(const Settings* settings);

private:
    Ui::SettingDialog *ui;
    // 使用智能指针管理Settings对象
    std::unique_ptr<Settings> saveSettings;

private slots:
    void on_buttonBox_accepted();
    void setDir();
};

#endif // SETTINGDIALOG_H
