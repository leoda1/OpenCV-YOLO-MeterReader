#include "errortabledialog.h"
#include <QSplitter>
#include <QScrollArea>
#include <QInputDialog>
#include <QTimer>
#include <cmath>

ErrorTableDialog::ErrorTableDialog(QWidget *parent)
    : QDialog(parent)
    , m_currentPressureIndex(0)
    , m_isForwardDirection(true)
{
    qDebug() << "开始创建ErrorTableDialog";
    
    setWindowTitle("压力表误差检测表格");
    // 修改为非模态窗口，避免阻塞主窗口
    setWindowModality(Qt::NonModal);
    resize(1200, 800);
    
    // 初始化配置数据
    m_config = PressureGaugeConfig();
    
    try {
        qDebug() << "开始setupUI";
        setupUI();
        qDebug() << "setupUI完成";
        
        // 延迟初始化数据
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "开始延迟初始化";
            try {
                qDebug() << "调用updateUIFromConfig";
                updateUIFromConfig();
                qDebug() << "updateUIFromConfig完成";
                
                qDebug() << "调用updateDetectionPointsTable";
                updateDetectionPointsTable();
                qDebug() << "updateDetectionPointsTable完成";
                
                qDebug() << "调用updateDataTable";
                updateDataTable();
                qDebug() << "updateDataTable完成";
                
                qDebug() << "连接信号";
                // 在所有UI初始化完成后再连接信号，避免递归问题
                connect(m_detectionPointsTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDetectionPointsChanged);
                
                qDebug() << "延迟初始化完成";
            } catch (const std::exception& e) {
                qDebug() << "延迟初始化异常:" << e.what();
            } catch (...) {
                qDebug() << "延迟初始化未知异常";
            }
        });
        
        qDebug() << "ErrorTableDialog构造完成";
    } catch (const std::exception& e) {
        qDebug() << "ErrorTableDialog 构造函数异常:" << e.what();
    } catch (...) {
        qDebug() << "ErrorTableDialog 构造函数未知异常";
    }
}

ErrorTableDialog::~ErrorTableDialog()
{
}

void ErrorTableDialog::setupUI()
{
    qDebug() << "创建主布局";
    m_mainLayout = new QVBoxLayout(this);
    
    qDebug() << "创建配置区域";
    setupConfigArea();
    
    qDebug() << "创建检测点区域";
    setupDetectionPointsArea();
    
    qDebug() << "创建数据区域";
    setupDataArea();
    
    qDebug() << "创建分析区域";
    setupAnalysisArea();
    
    qDebug() << "创建按钮区域";
    setupButtons();
    
    // 简化布局，不使用复杂的分割器
    m_mainLayout->addWidget(m_configGroup);
    m_mainLayout->addWidget(m_detectionPointsGroup);
    m_mainLayout->addWidget(m_dataGroup);
    m_mainLayout->addWidget(m_analysisGroup);
    m_mainLayout->addLayout(m_buttonLayout);
    
    qDebug() << "UI创建完成";
}

void ErrorTableDialog::setupConfigArea()
{
    m_configGroup = new QGroupBox("压力表配置", this);
    QGridLayout *layout = new QGridLayout(m_configGroup);
    
    // 产品信息
    layout->addWidget(new QLabel("产品型号:"), 0, 0);
    m_productModelEdit = new QLineEdit();
    layout->addWidget(m_productModelEdit, 0, 1);
    
    layout->addWidget(new QLabel("产品名称:"), 0, 2);
    m_productNameEdit = new QLineEdit();
    layout->addWidget(m_productNameEdit, 0, 3);
    
    layout->addWidget(new QLabel("刻度盘图号:"), 1, 0);
    m_dialDrawingNoEdit = new QLineEdit();
    layout->addWidget(m_dialDrawingNoEdit, 1, 1);
    
    layout->addWidget(new QLabel("支组编号:"), 1, 2);
    m_groupNoEdit = new QLineEdit();
    layout->addWidget(m_groupNoEdit, 1, 3);
    
    // 技术参数
    layout->addWidget(new QLabel("满量程压力(MPa):"), 2, 0);
    m_maxPressureSpin = new QDoubleSpinBox();
    m_maxPressureSpin->setRange(0.1, 100.0);
    m_maxPressureSpin->setDecimals(1);
    m_maxPressureSpin->setSingleStep(0.1);
    layout->addWidget(m_maxPressureSpin, 2, 1);
    
    layout->addWidget(new QLabel("满量程角度(°):"), 2, 2);
    m_maxAngleSpin = new QDoubleSpinBox();
    m_maxAngleSpin->setRange(180.0, 360.0);
    m_maxAngleSpin->setDecimals(0);
    m_maxAngleSpin->setSingleStep(1.0);
    layout->addWidget(m_maxAngleSpin, 2, 3);
    
    layout->addWidget(new QLabel("基本误差限值(MPa):"), 3, 0);
    m_basicErrorLimitSpin = new QDoubleSpinBox();
    m_basicErrorLimitSpin->setRange(0.01, 1.0);
    m_basicErrorLimitSpin->setDecimals(3);
    m_basicErrorLimitSpin->setSingleStep(0.001);
    layout->addWidget(m_basicErrorLimitSpin, 3, 1);
    
    layout->addWidget(new QLabel("迟滞误差限值(MPa):"), 3, 2);
    m_hysteresisErrorLimitSpin = new QDoubleSpinBox();
    m_hysteresisErrorLimitSpin->setRange(0.01, 1.0);
    m_hysteresisErrorLimitSpin->setDecimals(3);
    m_hysteresisErrorLimitSpin->setSingleStep(0.001);
    layout->addWidget(m_hysteresisErrorLimitSpin, 3, 3);
    
    // 暂时注释掉实时更新的信号连接，避免初始化时的递归问题
    // 用户可以通过"计算误差"按钮手动更新
    /*
    connect(m_productModelEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onConfigChanged);
    connect(m_productNameEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onConfigChanged);
    connect(m_dialDrawingNoEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onConfigChanged);
    connect(m_groupNoEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onConfigChanged);
    connect(m_maxPressureSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
    connect(m_maxAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
    connect(m_basicErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
    connect(m_hysteresisErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
    */
}

void ErrorTableDialog::setupDetectionPointsArea()
{
    m_detectionPointsGroup = new QGroupBox("检测点配置", this);
    QVBoxLayout *layout = new QVBoxLayout(m_detectionPointsGroup);
    
    // 检测点表格
    m_detectionPointsTable = new QTableWidget(0, 1);
    m_detectionPointsTable->setHorizontalHeaderLabels(QStringList() << "压力值(MPa)");
    m_detectionPointsTable->horizontalHeader()->setStretchLastSection(true);
    m_detectionPointsTable->setMaximumHeight(200);
    layout->addWidget(m_detectionPointsTable);
    
    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addPointBtn = new QPushButton("添加检测点");
    m_removePointBtn = new QPushButton("删除检测点");
    btnLayout->addWidget(m_addPointBtn);
    btnLayout->addWidget(m_removePointBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    connect(m_addPointBtn, &QPushButton::clicked, this, &ErrorTableDialog::addDetectionPoint);
    connect(m_removePointBtn, &QPushButton::clicked, this, &ErrorTableDialog::removeDetectionPoint);
    // 延迟连接cellChanged信号，避免初始化时的问题
    // connect(m_detectionPointsTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDetectionPointsChanged);
}

void ErrorTableDialog::setupDataArea()
{
    m_dataGroup = new QGroupBox("采集数据", this);
    QVBoxLayout *layout = new QVBoxLayout(m_dataGroup);
    
    // 当前测量状态
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_currentPointLabel = new QLabel("当前检测点: 0.0 MPa");
    m_directionCombo = new QComboBox();
    m_directionCombo->addItem("正行程");
    m_directionCombo->addItem("反行程");
    m_manualInputBtn = new QPushButton("手动输入角度");
    
    statusLayout->addWidget(m_currentPointLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(new QLabel("测量方向:"));
    statusLayout->addWidget(m_directionCombo);
    statusLayout->addWidget(m_manualInputBtn);
    layout->addLayout(statusLayout);
    
    // 数据表格
    m_dataTable = new QTableWidget(0, 10);
    QStringList headers;
    headers << "检测点(MPa)" << "检测点对应的刻度盘角度(°)" << "正行程角度(°)" << "正行程角度误差(°)" << "正行程误差(MPa)"
            << "反行程角度(°)" << "反行程角度误差(°)" << "反行程误差(MPa)" << "迟滞误差角度(°)" << "迟滞误差(MPa)";
    m_dataTable->setHorizontalHeaderLabels(headers);
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_dataTable);
    
    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        m_isForwardDirection = (index == 0);
    });
    connect(m_manualInputBtn, &QPushButton::clicked, this, &ErrorTableDialog::onManualAngleInput);
    connect(m_dataTable, &QTableWidget::cellClicked, this, &ErrorTableDialog::onTableCellClicked);
}

void ErrorTableDialog::setupAnalysisArea()
{
    m_analysisGroup = new QGroupBox("误差分析结果", this);
    QVBoxLayout *layout = new QVBoxLayout(m_analysisGroup);
    
    m_analysisText = new QTextEdit();
    m_analysisText->setReadOnly(true);
    // 使用系统默认字体，避免字体不存在的问题
    QFont font = m_analysisText->font();
    font.setPointSize(10);
    m_analysisText->setFont(font);
    layout->addWidget(m_analysisText);
}

void ErrorTableDialog::setupButtons()
{
    m_buttonLayout = new QHBoxLayout();
    
    m_clearBtn = new QPushButton("清空数据");
    m_calculateBtn = new QPushButton("计算误差");
    m_exportExcelBtn = new QPushButton("导出Excel");
    m_exportTextBtn = new QPushButton("导出文本");
    m_saveConfigBtn = new QPushButton("保存配置");
    m_loadConfigBtn = new QPushButton("加载配置");
    m_closeBtn = new QPushButton("关闭");
    
    m_buttonLayout->addWidget(m_clearBtn);
    m_buttonLayout->addWidget(m_calculateBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_exportExcelBtn);
    m_buttonLayout->addWidget(m_exportTextBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_saveConfigBtn);
    m_buttonLayout->addWidget(m_loadConfigBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_closeBtn);
    
    connect(m_clearBtn, &QPushButton::clicked, this, &ErrorTableDialog::clearAllData);
    connect(m_calculateBtn, &QPushButton::clicked, this, &ErrorTableDialog::calculateErrors);
    connect(m_exportExcelBtn, &QPushButton::clicked, this, &ErrorTableDialog::exportToExcel);
    connect(m_exportTextBtn, &QPushButton::clicked, this, &ErrorTableDialog::exportToText);
    connect(m_saveConfigBtn, &QPushButton::clicked, this, &ErrorTableDialog::saveConfig);
    connect(m_loadConfigBtn, &QPushButton::clicked, this, &ErrorTableDialog::loadConfig);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ErrorTableDialog::updateConfigFromUI()
{
    m_config.productModel = m_productModelEdit->text();
    m_config.productName = m_productNameEdit->text();
    m_config.dialDrawingNo = m_dialDrawingNoEdit->text();
    m_config.groupNo = m_groupNoEdit->text();
    m_config.maxPressure = m_maxPressureSpin->value();
    m_config.maxAngle = m_maxAngleSpin->value();
    m_config.basicErrorLimit = m_basicErrorLimitSpin->value();
    m_config.hysteresisErrorLimit = m_hysteresisErrorLimitSpin->value();
}

void ErrorTableDialog::updateUIFromConfig()
{
    qDebug() << "updateUIFromConfig 开始检查UI组件";
    
    // 检查所有UI组件是否已初始化
    if (!m_productModelEdit || !m_productNameEdit || !m_dialDrawingNoEdit || !m_groupNoEdit ||
        !m_maxPressureSpin || !m_maxAngleSpin || !m_basicErrorLimitSpin || !m_hysteresisErrorLimitSpin) {
        qDebug() << "某些UI组件还没有初始化，跳过updateUIFromConfig";
        return;
    }
    
    qDebug() << "开始设置UI组件的值";
    
    try {
        m_productModelEdit->setText(m_config.productModel);
        m_productNameEdit->setText(m_config.productName);
        m_dialDrawingNoEdit->setText(m_config.dialDrawingNo);
        m_groupNoEdit->setText(m_config.groupNo);
        m_maxPressureSpin->setValue(m_config.maxPressure);
        m_maxAngleSpin->setValue(m_config.maxAngle);
        m_basicErrorLimitSpin->setValue(m_config.basicErrorLimit);
        m_hysteresisErrorLimitSpin->setValue(m_config.hysteresisErrorLimit);
        
        qDebug() << "updateUIFromConfig 完成设置";
    } catch (const std::exception& e) {
        qDebug() << "updateUIFromConfig 异常:" << e.what();
    } catch (...) {
        qDebug() << "updateUIFromConfig 未知异常";
    }
}

void ErrorTableDialog::updateDetectionPointsTable()
{
    qDebug() << "updateDetectionPointsTable 开始";
    
    if (!m_detectionPointsTable) {
        qDebug() << "m_detectionPointsTable 还没有初始化";
        return;
    }
    
    try {
        qDebug() << "设置检测点表格行数:" << m_config.detectionPoints.size();
        
        // 安全地断开信号连接，防止无限递归
        bool wasConnected = disconnect(m_detectionPointsTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDetectionPointsChanged);
        
        m_detectionPointsTable->setRowCount(m_config.detectionPoints.size());
        
        for (int i = 0; i < m_config.detectionPoints.size(); ++i) {
            QTableWidgetItem *item = new QTableWidgetItem(QString::number(m_config.detectionPoints[i], 'f', 1));
            m_detectionPointsTable->setItem(i, 0, item);
        }
        
        // 只有之前有连接才重新连接
        if (wasConnected) {
            connect(m_detectionPointsTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDetectionPointsChanged);
        }
        
        qDebug() << "更新检测数据";
        // 更新检测数据
        m_detectionData.clear();
        for (double pressure : m_config.detectionPoints) {
            DetectionPoint point;
            point.pressure = pressure;
            m_detectionData.append(point);
        }
        
        qDebug() << "updateDetectionPointsTable 完成，检测数据数量:" << m_detectionData.size();
        
    } catch (const std::exception& e) {
        qDebug() << "updateDetectionPointsTable 异常:" << e.what();
    } catch (...) {
        qDebug() << "updateDetectionPointsTable 未知异常";
    }
}

void ErrorTableDialog::updateDataTable()
{
    qDebug() << "updateDataTable 开始";
    
    if (!m_dataTable) {
        qDebug() << "m_dataTable 还没有初始化";
        return;
    }
    
    try {
        qDebug() << "设置数据表格行数:" << m_detectionData.size();
        m_dataTable->setRowCount(m_detectionData.size());
    
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        
        // 检测点压力
        m_dataTable->setItem(i, 0, new QTableWidgetItem(QString::number(point.pressure, 'f', 1)));
        
        // 检测点对应的刻度盘角度(理论角度)
        double expectedAngle = pressureToAngle(point.pressure);
        m_dataTable->setItem(i, 1, new QTableWidgetItem(QString::number(expectedAngle, 'f', 2)));
        
        // 正行程角度
        if (point.hasForward) {
            m_dataTable->setItem(i, 2, new QTableWidgetItem(QString::number(point.forwardAngle, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 2, new QTableWidgetItem("--"));
        }
        
        // 正行程角度误差
        if (point.hasForward) {
            double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
            m_dataTable->setItem(i, 3, new QTableWidgetItem(QString::number(angleError, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 3, new QTableWidgetItem("--"));
        }
        
        // 正行程误差(压力)
        if (point.hasForward) {
            double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
            double pressureError = calculatePressureError(angleError);
            m_dataTable->setItem(i, 4, new QTableWidgetItem(QString::number(pressureError, 'f', 3)));
        } else {
            m_dataTable->setItem(i, 4, new QTableWidgetItem("--"));
        }
        
        // 反行程角度
        if (point.hasBackward) {
            m_dataTable->setItem(i, 5, new QTableWidgetItem(QString::number(point.backwardAngle, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 5, new QTableWidgetItem("--"));
        }
        
        // 反行程角度误差
        if (point.hasBackward) {
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            m_dataTable->setItem(i, 6, new QTableWidgetItem(QString::number(angleError, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 6, new QTableWidgetItem("--"));
        }
        
        // 反行程误差(压力)
        if (point.hasBackward) {
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            double pressureError = calculatePressureError(angleError);
            m_dataTable->setItem(i, 7, new QTableWidgetItem(QString::number(pressureError, 'f', 3)));
        } else {
            m_dataTable->setItem(i, 7, new QTableWidgetItem("--"));
        }
        
        // 迟滞误差角度
        if (point.hasForward && point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            m_dataTable->setItem(i, 8, new QTableWidgetItem(QString::number(angleDiff, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 8, new QTableWidgetItem("--"));
        }
        
        // 迟滞误差(压力)
        if (point.hasForward && point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            double hysteresisPressureError = angleToPressure(angleDiff);
            m_dataTable->setItem(i, 9, new QTableWidgetItem(QString::number(hysteresisPressureError, 'f', 3)));
        } else {
            m_dataTable->setItem(i, 9, new QTableWidgetItem("--"));
        }
    }
    
        qDebug() << "设置表格只读";
        // 设置只读
        for (int i = 0; i < m_dataTable->rowCount(); ++i) {
            for (int j = 0; j < m_dataTable->columnCount(); ++j) {
                QTableWidgetItem *item = m_dataTable->item(i, j);
                if (item) {
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                }
            }
        }
        
        qDebug() << "updateDataTable 完成";
        
    } catch (const std::exception& e) {
        qDebug() << "updateDataTable 异常:" << e.what();
    } catch (...) {
        qDebug() << "updateDataTable 未知异常";
    }
}

double ErrorTableDialog::pressureToAngle(double pressure) const
{
    if (m_config.maxPressure <= 0) return 0.0;
    return (pressure / m_config.maxPressure) * m_config.maxAngle;
}

double ErrorTableDialog::angleToPressure(double angle) const
{
    if (m_config.maxAngle <= 0) return 0.0;
    return (angle / m_config.maxAngle) * m_config.maxPressure;
}

double ErrorTableDialog::calculateAngleError(double actualAngle, double expectedAngle) const
{
    return actualAngle - expectedAngle;
}

double ErrorTableDialog::calculatePressureError(double angleError) const
{
    return angleToPressure(angleError);
}

void ErrorTableDialog::addAngleData(double angle, bool isForward)
{
    if (m_currentPressureIndex >= 0 && m_currentPressureIndex < m_detectionData.size()) {
        DetectionPoint &point = m_detectionData[m_currentPressureIndex];
        
        if (isForward) {
            point.forwardAngle = angle;
            point.hasForward = true;
        } else {
            point.backwardAngle = angle;
            point.hasBackward = true;
        }
        
        updateDataTable();
        validateAndCheckErrors();
    }
}

void ErrorTableDialog::setCurrentPressurePoint(double pressure)
{
    for (int i = 0; i < m_detectionData.size(); ++i) {
        if (std::abs(m_detectionData[i].pressure - pressure) < 0.01) {
            m_currentPressureIndex = i;
            m_currentPointLabel->setText(QString("当前检测点: %1 MPa").arg(pressure, 0, 'f', 1));
            break;
        }
    }
}

void ErrorTableDialog::onConfigChanged()
{
    updateConfigFromUI();
    updateDataTable();
}

void ErrorTableDialog::onDetectionPointsChanged()
{
    m_config.detectionPoints.clear();
    
    for (int i = 0; i < m_detectionPointsTable->rowCount(); ++i) {
        QTableWidgetItem *item = m_detectionPointsTable->item(i, 0);
        if (item) {
            bool ok;
            double value = item->text().toDouble(&ok);
            if (ok && value >= 0) {
                m_config.detectionPoints.append(value);
            }
        }
    }
    
    // 排序检测点
    std::sort(m_config.detectionPoints.begin(), m_config.detectionPoints.end());
    
    updateDetectionPointsTable();
}

void ErrorTableDialog::addDetectionPoint()
{
    bool ok;
    double pressure = QInputDialog::getDouble(this, "添加检测点", "压力值(MPa):", 1.0, 0.0, 100.0, 1, &ok);
    if (ok) {
        m_config.detectionPoints.append(pressure);
        updateDetectionPointsTable();
    }
}

void ErrorTableDialog::removeDetectionPoint()
{
    int currentRow = m_detectionPointsTable->currentRow();
    if (currentRow >= 0 && currentRow < m_config.detectionPoints.size()) {
        m_config.detectionPoints.removeAt(currentRow);
        updateDetectionPointsTable();
    }
}

void ErrorTableDialog::clearAllData()
{
    if (QMessageBox::question(this, "确认", "确定要清空所有采集数据吗？") == QMessageBox::Yes) {
        for (DetectionPoint &point : m_detectionData) {
            point.hasForward = false;
            point.hasBackward = false;
            point.forwardAngle = 0.0;
            point.backwardAngle = 0.0;
        }
        updateDataTable();
        updateAnalysisText();
    }
}

void ErrorTableDialog::calculateErrors()
{
    updateDataTable();
    updateAnalysisText();
}

void ErrorTableDialog::validateAndCheckErrors()
{
    updateAnalysisText();
}

void ErrorTableDialog::updateAnalysisText()
{
    QString analysis = formatAnalysisResult();
    m_analysisText->setHtml(analysis);
}

QString ErrorTableDialog::formatAnalysisResult()
{
    QString result = "<h3>误差分析结果</h3>";
    
    // 基本信息
    result += QString("<p><b>产品型号:</b> %1 &nbsp;&nbsp; <b>产品名称:</b> %2</p>").arg(m_config.productModel, m_config.productName);
    result += QString("<p><b>满量程:</b> %1 MPa / %2° &nbsp;&nbsp; <b>基本误差限值:</b> ±%3 MPa &nbsp;&nbsp; <b>迟滞误差限值:</b> %4 MPa</p>")
              .arg(m_config.maxPressure).arg(m_config.maxAngle).arg(m_config.basicErrorLimit).arg(m_config.hysteresisErrorLimit);
    
    // 误差限值转换为角度
    double basicErrorAngle = (m_config.basicErrorLimit / m_config.maxPressure) * m_config.maxAngle;
    double hysteresisErrorAngle = (m_config.hysteresisErrorLimit / m_config.maxPressure) * m_config.maxAngle;
    
    result += QString("<p><b>角度误差限值:</b> 基本误差 ±%1°, 迟滞误差 %2°</p>")
              .arg(basicErrorAngle, 0, 'f', 1).arg(hysteresisErrorAngle, 0, 'f', 1);
    
    result += "<h4>检测结果:</h4>";
    
    bool allPointsValid = true;
    QStringList issues;
    
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        
        result += QString("<p><b>%1 MPa:</b> ").arg(point.pressure, 0, 'f', 1);
        
        if (!point.hasForward && !point.hasBackward) {
            result += "<span style='color: orange;'>无数据</span>";
            continue;
        }
        
        // 检查迟滞误差
        if (point.hasForward && point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            double hysteresisErrorMPa = angleToPressure(angleDiff);
            
            if (angleDiff <= hysteresisErrorAngle) {
                result += QString("<span style='color: green;'>迟滞误差 %1° (%.3f MPa) ✓</span>")
                          .arg(angleDiff, 0, 'f', 2).arg(hysteresisErrorMPa);
            } else {
                result += QString("<span style='color: red;'>迟滞误差 %1° (%.3f MPa) ✗</span>")
                          .arg(angleDiff, 0, 'f', 2).arg(hysteresisErrorMPa);
                allPointsValid = false;
                issues << QString("%1 MPa 迟滞误差超标").arg(point.pressure, 0, 'f', 1);
            }
        }
        
        // 检查基本误差
        double expectedAngle = pressureToAngle(point.pressure);
        
        if (point.hasForward) {
            double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
            double pressureError = calculatePressureError(angleError);
            
            if (std::abs(angleError) <= basicErrorAngle) {
                result += QString(" 正行程 %1° (%.3f MPa) ✓")
                          .arg(angleError, 0, 'f', 2).arg(pressureError);
            } else {
                result += QString(" <span style='color: red;'>正行程 %1° (%.3f MPa) ✗</span>")
                          .arg(angleError, 0, 'f', 2).arg(pressureError);
                allPointsValid = false;
                issues << QString("%1 MPa 正行程基本误差超标").arg(point.pressure, 0, 'f', 1);
            }
        }
        
        if (point.hasBackward) {
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            double pressureError = calculatePressureError(angleError);
            
            if (std::abs(angleError) <= basicErrorAngle) {
                result += QString(" 反行程 %1° (%.3f MPa) ✓")
                          .arg(angleError, 0, 'f', 2).arg(pressureError);
            } else {
                result += QString(" <span style='color: red;'>反行程 %1° (%.3f MPa) ✗</span>")
                          .arg(angleError, 0, 'f', 2).arg(pressureError);
                allPointsValid = false;
                issues << QString("%1 MPa 反行程基本误差超标").arg(point.pressure, 0, 'f', 1);
            }
        }
        
        result += "</p>";
    }
    
    // 总结
    result += "<h4>总结:</h4>";
    if (allPointsValid) {
        result += "<p style='color: green; font-weight: bold;'>✓ 所有检测点均符合误差要求，刻度盘可以制作</p>";
    } else {
        result += "<p style='color: red; font-weight: bold;'>✗ 存在误差超标项目，需要调整:</p>";
        result += "<ul>";
        for (const QString &issue : issues) {
            result += QString("<li style='color: red;'>%1</li>").arg(issue);
        }
        result += "</ul>";
        
        // 建议调整方案
        result += "<h4>建议调整方案:</h4>";
        result += "<p>对于基本误差超标的检测点，可以调整刻度盘角度至正反行程角度的中间值来改善误差。</p>";
    }
    
    return result;
}

void ErrorTableDialog::exportToText()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出文本文件", "", "文本文件 (*.txt)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }
    
    QTextStream out(&file);
    
    QString data = generateExportData();
    out << data;
    
    QMessageBox::information(this, "成功", "数据已导出到文本文件");
}

void ErrorTableDialog::exportToExcel()
{
    // 由于Qt默认不包含Excel导出功能，这里导出为CSV格式
    QString fileName = QFileDialog::getSaveFileName(this, "导出CSV文件", "", "CSV文件 (*.csv)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }
    
    // 写入UTF-8 BOM头，确保Excel能正确显示中文
    file.write("\xEF\xBB\xBF");
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入产品信息
    out << QString("产品型号：,%1,产品名称：,%2\n").arg(m_config.productModel, m_config.productName);
    out << QString("刻度盘图号：,%1,支组编号：,%2\n").arg(m_config.dialDrawingNo, m_config.groupNo);
    out << QString("满量程压力：,%1MPa,满量程角度：,%2°\n").arg(m_config.maxPressure).arg(m_config.maxAngle);
    out << QString("基本误差限值：,±%1MPa,迟滞误差限值：,%2MPa\n").arg(m_config.basicErrorLimit).arg(m_config.hysteresisErrorLimit);
    out << "\n"; // 空行
    
    // 写入表头
    QStringList headers;
    for (int j = 0; j < m_dataTable->columnCount(); ++j) {
        headers << m_dataTable->horizontalHeaderItem(j)->text();
    }
    out << headers.join(",") << "\n";
    
    // 写入数据
    for (int i = 0; i < m_dataTable->rowCount(); ++i) {
        QStringList row;
        for (int j = 0; j < m_dataTable->columnCount(); ++j) {
            QTableWidgetItem *item = m_dataTable->item(i, j);
            row << (item ? item->text() : "");
        }
        out << row.join(",") << "\n";
    }
    
    QMessageBox::information(this, "成功", "数据已导出到CSV文件，包含产品信息，可用Excel打开");
}

QString ErrorTableDialog::generateExportData()
{
    QString data;
    
    data += "压力表误差检测报告\n";
    data += "==================\n\n";
    
    data += QString("产品型号：%1\n").arg(m_config.productModel);
    data += QString("产品名称：%1\n").arg(m_config.productName);
    data += QString("刻度盘图号：%1\n").arg(m_config.dialDrawingNo);
    data += QString("支组编号：%1\n\n").arg(m_config.groupNo);
    
    data += "检测数据\n";
    data += "--------\n";
    data += QString("检测点\t正行程角度\t反行程角度\t角度差\t正行程误差(°)\t反行程误差(°)\t正行程误差(MPa)\t反行程误差(MPa)\n");
    
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        double expectedAngle = pressureToAngle(point.pressure);
        
        data += QString("%1").arg(point.pressure, 0, 'f', 1);
        
        if (point.hasForward) {
            data += QString("\t%1").arg(point.forwardAngle, 0, 'f', 2);
        } else {
            data += "\t--";
        }
        
        if (point.hasBackward) {
            data += QString("\t%1").arg(point.backwardAngle, 0, 'f', 2);
        } else {
            data += "\t--";
        }
        
        if (point.hasForward && point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            data += QString("\t%1").arg(angleDiff, 0, 'f', 2);
        } else {
            data += "\t--";
        }
        
        if (point.hasForward) {
            double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
            double pressureError = calculatePressureError(angleError);
            data += QString("\t%1\t\t%2").arg(angleError, 0, 'f', 2).arg(pressureError, 0, 'f', 3);
        } else {
            data += "\t--\t\t--";
        }
        
        if (point.hasBackward) {
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            double pressureError = calculatePressureError(angleError);
            data += QString("\t%1\t%2").arg(angleError, 0, 'f', 2).arg(pressureError, 0, 'f', 3);
        } else {
            data += "\t--\t--";
        }
        
        data += "\n";
    }
    
    return data;
}

void ErrorTableDialog::saveConfig()
{
    QString fileName = QFileDialog::getSaveFileName(this, "保存配置", "", "配置文件 (*.json)");
    if (!fileName.isEmpty()) {
        saveConfigToFile(fileName);
    }
}

void ErrorTableDialog::loadConfig()
{
    QString fileName = QFileDialog::getOpenFileName(this, "加载配置", "", "配置文件 (*.json)");
    if (!fileName.isEmpty()) {
        loadConfigFromFile(fileName);
    }
}

void ErrorTableDialog::saveConfigToFile(const QString &fileName)
{
    QJsonObject config;
    config["productModel"] = m_config.productModel;
    config["productName"] = m_config.productName;
    config["dialDrawingNo"] = m_config.dialDrawingNo;
    config["groupNo"] = m_config.groupNo;
    config["maxPressure"] = m_config.maxPressure;
    config["maxAngle"] = m_config.maxAngle;
    config["basicErrorLimit"] = m_config.basicErrorLimit;
    config["hysteresisErrorLimit"] = m_config.hysteresisErrorLimit;
    
    QJsonArray points;
    for (double pressure : m_config.detectionPoints) {
        points.append(pressure);
    }
    config["detectionPoints"] = points;
    
    QJsonDocument doc(config);
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        QMessageBox::information(this, "成功", "配置已保存");
    } else {
        QMessageBox::warning(this, "错误", "无法保存配置文件");
    }
}

void ErrorTableDialog::loadConfigFromFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法读取配置文件");
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject config = doc.object();
    
    m_config.productModel = config["productModel"].toString();
    m_config.productName = config["productName"].toString();
    m_config.dialDrawingNo = config["dialDrawingNo"].toString();
    m_config.groupNo = config["groupNo"].toString();
    m_config.maxPressure = config["maxPressure"].toDouble();
    m_config.maxAngle = config["maxAngle"].toDouble();
    m_config.basicErrorLimit = config["basicErrorLimit"].toDouble();
    m_config.hysteresisErrorLimit = config["hysteresisErrorLimit"].toDouble();
    
    m_config.detectionPoints.clear();
    QJsonArray points = config["detectionPoints"].toArray();
    for (const QJsonValue &value : points) {
        m_config.detectionPoints.append(value.toDouble());
    }
    
    updateUIFromConfig();
    updateDetectionPointsTable();
    
    QMessageBox::information(this, "成功", "配置已加载");
}

void ErrorTableDialog::onTableCellClicked(int row, int column)
{
    if (row >= 0 && row < m_detectionData.size()) {
        m_currentPressureIndex = row;
        double pressure = m_detectionData[row].pressure;
        m_currentPointLabel->setText(QString("当前检测点: %1 MPa").arg(pressure, 0, 'f', 1));
    }
}

void ErrorTableDialog::onManualAngleInput()
{
    if (m_currentPressureIndex < 0 || m_currentPressureIndex >= m_detectionData.size()) {
        QMessageBox::warning(this, "提示", "请先选择一个检测点");
        return;
    }
    
    bool ok;
    double angle = QInputDialog::getDouble(this, "手动输入角度", 
                                          QString("检测点 %1 MPa %2 角度值:")
                                          .arg(m_detectionData[m_currentPressureIndex].pressure, 0, 'f', 1)
                                          .arg(m_isForwardDirection ? "正行程" : "反行程"),
                                          0.0, 0.0, 360.0, 2, &ok);
    if (ok) {
        addAngleData(angle, m_isForwardDirection);
    }
} 