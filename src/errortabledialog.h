#ifndef ERRORTABLEDIALOG_H
#define ERRORTABLEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QComboBox>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCheckBox>
#include <QTimer>

// 检测点数据结构
struct DetectionPoint {
    double pressure;           // 压力值(MPa)
    double forwardAngle;      // 正行程角度
    double backwardAngle;     // 反行程角度
    bool hasForward;          // 是否有正行程数据
    bool hasBackward;         // 是否有反行程数据
    
    DetectionPoint() : pressure(0.0), forwardAngle(0.0), backwardAngle(0.0), 
                      hasForward(false), hasBackward(false) {}
};

// 压力表配置参数
struct PressureGaugeConfig {
    QString productModel;      // 产品型号
    QString productName;       // 产品名称
    QString dialDrawingNo;     // 刻度盘图号
    QString groupNo;           // 支组编号
    
    double maxPressure;        // 满量程压力(MPa)
    double maxAngle;           // 满量程角度(度)
    double basicErrorLimit;    // 基本误差限值(MPa)
    double hysteresisErrorLimit; // 迟滞误差限值(MPa)
    
    QList<double> detectionPoints; // 检测点压力值列表
    
    PressureGaugeConfig() : maxPressure(3.0), maxAngle(270.0), 
                           basicErrorLimit(0.1), hysteresisErrorLimit(0.15) {
        // 默认检测点 (0, 1, 2, 3 MPa)
        detectionPoints << 0.0 << 1.0 << 2.0 << 3.0;
    }
};

class ErrorTableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ErrorTableDialog(QWidget *parent = nullptr);
    ~ErrorTableDialog();
    
    // 添加角度数据（从主窗口调用）
    void addAngleData(double angle, bool isForward = true);
    
    // 设置当前测量的压力点
    void setCurrentPressurePoint(double pressure);

private slots:
    void onConfigChanged();
    void onDetectionPointsChanged();
    void addDetectionPoint();
    void removeDetectionPoint();
    void clearAllData();
    void calculateErrors();
    void exportToExcel();
    void exportToText();
    void saveConfig();
    void loadConfig();
    void onTableCellClicked(int row, int column);
    void onDataTableCellChanged(int row, int column);
    void validateAndCheckErrors();

private:
    // UI组件
    QVBoxLayout *m_mainLayout;
    
    // 配置区域
    QGroupBox *m_configGroup;
    QLineEdit *m_productModelEdit;
    QLineEdit *m_productNameEdit;
    QLineEdit *m_dialDrawingNoEdit;
    QLineEdit *m_groupNoEdit;
    
    QDoubleSpinBox *m_maxPressureSpin;
    QDoubleSpinBox *m_maxAngleSpin;
    QDoubleSpinBox *m_basicErrorLimitSpin;
    QDoubleSpinBox *m_hysteresisErrorLimitSpin;
    
    // 检测点配置
    QGroupBox *m_detectionPointsGroup;
    QTableWidget *m_detectionPointsTable;
    QPushButton *m_addPointBtn;
    QPushButton *m_removePointBtn;
    
    // 数据表格
    QGroupBox *m_dataGroup;
    QTableWidget *m_dataTable;
    QLabel *m_currentPointLabel;
    QComboBox *m_directionCombo;
    
    // 误差分析结果
    QGroupBox *m_analysisGroup;
    QTextEdit *m_analysisText;
    
    // 操作按钮
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_clearBtn;
    QPushButton *m_calculateBtn;
    QPushButton *m_exportExcelBtn;
    QPushButton *m_exportTextBtn;
    QPushButton *m_saveConfigBtn;
    QPushButton *m_loadConfigBtn;
    QPushButton *m_closeBtn;
    
    // 数据
    PressureGaugeConfig m_config;
    QList<DetectionPoint> m_detectionData;
    int m_currentPressureIndex;
    bool m_isForwardDirection;
    
    // 私有方法
    void setupUI();
    void setupConfigArea();
    void setupDetectionPointsArea();
    void setupDataArea();
    void setupAnalysisArea();
    void setupButtons();
    
    void updateConfigFromUI();
    void updateUIFromConfig();
    void updateDetectionPointsTable();
    void updateDataTable();
    void updateAnalysisText();
    
    // 计算相关
    double pressureToAngle(double pressure) const;
    double angleToPressure(double angle) const;
    double calculateAngleError(double actualAngle, double expectedAngle) const;
    double calculatePressureError(double angleError) const;
    
    QString formatAnalysisResult();
    QString generateExportData();
    
    void saveConfigToFile(const QString &fileName);
    void loadConfigFromFile(const QString &fileName);
};

#endif // ERRORTABLEDIALOG_H 