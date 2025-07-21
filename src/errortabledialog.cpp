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
    setWindowTitle("压力表误差检测");
    resize(1400, 700);  // 调整窗口尺寸：更宽一些，但高度减少
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);  // 减少组件间距
    m_mainLayout->setContentsMargins(10, 10, 10, 10);  // 减少边距
    
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
    
    // 创建上部分的水平布局，将配置和检测点区域放在一行
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);
    topLayout->addWidget(m_configGroup, 2);  // 配置区域占更多空间
    topLayout->addWidget(m_detectionPointsGroup, 1);  // 检测点区域较小
    
    m_mainLayout->addLayout(topLayout);
    m_mainLayout->addWidget(m_dataGroup, 1);  // 数据表格占主要空间
    
    // 创建下部分的水平布局，将分析结果和按钮并排
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);
    bottomLayout->addWidget(m_analysisGroup, 1);
    
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addStretch();
    rightLayout->addLayout(m_buttonLayout);
    rightLayout->addStretch();
    
    bottomLayout->addLayout(rightLayout);
    m_mainLayout->addLayout(bottomLayout);
    
    qDebug() << "UI创建完成";
}

void ErrorTableDialog::setupConfigArea()
{
    m_configGroup = new QGroupBox("压力表配置", this);
    QGridLayout *layout = new QGridLayout(m_configGroup);
    layout->setSpacing(6);  // 减少间距
    layout->setContentsMargins(8, 8, 8, 8);  // 减少边距
    
    // 产品信息
    layout->addWidget(new QLabel("产品型号:"), 0, 0);
    m_productModelEdit = new QLineEdit();
    m_productModelEdit->setMaximumWidth(150);
    layout->addWidget(m_productModelEdit, 0, 1);
    
    layout->addWidget(new QLabel("产品名称:"), 0, 2);
    m_productNameEdit = new QLineEdit();
    m_productNameEdit->setMaximumWidth(150);
    layout->addWidget(m_productNameEdit, 0, 3);
    
    layout->addWidget(new QLabel("刻度盘图号:"), 1, 0);
    m_dialDrawingNoEdit = new QLineEdit();
    m_dialDrawingNoEdit->setMaximumWidth(150);
    layout->addWidget(m_dialDrawingNoEdit, 1, 1);
    
    layout->addWidget(new QLabel("支组编号:"), 1, 2);
    m_groupNoEdit = new QLineEdit();
    m_groupNoEdit->setMaximumWidth(150);
    layout->addWidget(m_groupNoEdit, 1, 3);
    
    // 技术参数
    layout->addWidget(new QLabel("满量程压力(MPa):"), 2, 0);
    m_maxPressureSpin = new QDoubleSpinBox();
    m_maxPressureSpin->setRange(0.1, 100.0);
    m_maxPressureSpin->setDecimals(1);
    m_maxPressureSpin->setSingleStep(0.1);
    m_maxPressureSpin->setMaximumWidth(100);
    layout->addWidget(m_maxPressureSpin, 2, 1);
    
    layout->addWidget(new QLabel("满量程角度(°):"), 2, 2);
    m_maxAngleSpin = new QDoubleSpinBox();
    m_maxAngleSpin->setRange(180.0, 360.0);
    m_maxAngleSpin->setDecimals(0);
    m_maxAngleSpin->setSingleStep(1.0);
    m_maxAngleSpin->setMaximumWidth(100);
    layout->addWidget(m_maxAngleSpin, 2, 3);
    
    layout->addWidget(new QLabel("基本误差限值(MPa):"), 3, 0);
    m_basicErrorLimitSpin = new QDoubleSpinBox();
    m_basicErrorLimitSpin->setRange(0.01, 1.0);
    m_basicErrorLimitSpin->setDecimals(3);
    m_basicErrorLimitSpin->setSingleStep(0.001);
    m_basicErrorLimitSpin->setMaximumWidth(100);
    layout->addWidget(m_basicErrorLimitSpin, 3, 1);
    
    layout->addWidget(new QLabel("迟滞误差限值(MPa):"), 3, 2);
    m_hysteresisErrorLimitSpin = new QDoubleSpinBox();
    m_hysteresisErrorLimitSpin->setRange(0.01, 1.0);
    m_hysteresisErrorLimitSpin->setDecimals(3);
    m_hysteresisErrorLimitSpin->setSingleStep(0.001);
    m_hysteresisErrorLimitSpin->setMaximumWidth(100);
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
    layout->setSpacing(6);  // 减少间距
    layout->setContentsMargins(8, 8, 8, 8);  // 减少边距
    
    // 检测点表格
    m_detectionPointsTable = new QTableWidget(0, 1);
    m_detectionPointsTable->setHorizontalHeaderLabels(QStringList() << "压力值(MPa)");
    m_detectionPointsTable->horizontalHeader()->setStretchLastSection(true);
    m_detectionPointsTable->setMaximumHeight(150);  // 减少高度
    m_detectionPointsTable->setAlternatingRowColors(true);
    m_detectionPointsTable->verticalHeader()->setDefaultSectionSize(22);  // 紧凑的行高
    layout->addWidget(m_detectionPointsTable);
    
    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);  // 减少间距
    m_addPointBtn = new QPushButton("添加");
    m_removePointBtn = new QPushButton("删除");
    m_addPointBtn->setMaximumWidth(60);
    m_removePointBtn->setMaximumWidth(60);
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
    layout->setSpacing(6);  // 减少间距
    layout->setContentsMargins(8, 8, 8, 8);  // 减少边距
    
    // 当前测量状态
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(8);  // 减少间距
    m_currentPointLabel = new QLabel("当前检测点: 0.0 MPa");
    m_currentPointLabel->setStyleSheet("font-weight: bold; color: #2E86C1;");
    m_directionCombo = new QComboBox();
    m_directionCombo->addItem("正行程");
    m_directionCombo->addItem("反行程");
    m_directionCombo->setMinimumWidth(80);
    
    statusLayout->addWidget(m_currentPointLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(new QLabel("测量方向:"));
    statusLayout->addWidget(m_directionCombo);
    layout->addLayout(statusLayout);
    
    // 数据表格
    m_dataTable = new QTableWidget(0, 10);
    QStringList headers;
    headers << "检测点(MPa)" << "理论角度(°)" << "正行程角度(°)" << "正行程角度误差(°)" << "正行程误差(MPa)"
            << "反行程角度(°)" << "反行程角度误差(°)" << "反行程误差(MPa)" << "迟滞误差角度(°)" << "迟滞误差(MPa)";
    m_dataTable->setHorizontalHeaderLabels(headers);
    
    // 设置列宽 - 第一列加宽，其他列适当调整
    m_dataTable->setColumnWidth(0, 120);  // 检测点列加宽
    m_dataTable->setColumnWidth(1, 100);  // 理论角度
    m_dataTable->setColumnWidth(2, 120);  // 正行程角度
    m_dataTable->setColumnWidth(3, 120);  // 正行程角度误差
    m_dataTable->setColumnWidth(4, 120);  // 正行程误差(MPa)
    m_dataTable->setColumnWidth(5, 120);  // 反行程角度
    m_dataTable->setColumnWidth(6, 120);  // 反行程角度误差
    m_dataTable->setColumnWidth(7, 120);  // 反行程误差(MPa)
    m_dataTable->setColumnWidth(8, 120);  // 迟滞误差角度
    m_dataTable->setColumnWidth(9, 120);  // 迟滞误差(MPa)
    
    // 设置表格属性
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_dataTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置紧凑的行高
    m_dataTable->verticalHeader()->setDefaultSectionSize(25);
    m_dataTable->verticalHeader()->setMinimumSectionSize(25);
    
    layout->addWidget(m_dataTable);
    
    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        m_isForwardDirection = (index == 0);
    });
    connect(m_dataTable, &QTableWidget::cellClicked, this, &ErrorTableDialog::onTableCellClicked);
    // cellChanged信号连接将在updateDataTable中处理，避免初始化时的问题
}

void ErrorTableDialog::setupAnalysisArea()
{
    m_analysisGroup = new QGroupBox("误差分析结果", this);
    QVBoxLayout *layout = new QVBoxLayout(m_analysisGroup);
    layout->setSpacing(4);  // 减少间距
    layout->setContentsMargins(8, 8, 8, 8);  // 减少边距
    
    m_analysisText = new QTextEdit();
    m_analysisText->setReadOnly(true);
    m_analysisText->setMaximumHeight(120);  // 限制高度
    // 使用系统默认字体，避免字体不存在的问题
    QFont font = m_analysisText->font();
    font.setPointSize(9);  // 略小的字体
    m_analysisText->setFont(font);
    layout->addWidget(m_analysisText);
}

void ErrorTableDialog::setupButtons()
{
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(8);  // 减少间距
    
    m_clearBtn = new QPushButton("清空");
    m_calculateBtn = new QPushButton("计算");
    m_exportExcelBtn = new QPushButton("导出Excel");
    m_exportTextBtn = new QPushButton("导出文本");
    m_saveConfigBtn = new QPushButton("保存");
    m_loadConfigBtn = new QPushButton("加载");
    m_closeBtn = new QPushButton("关闭");
    
    // 设置按钮的最大宽度让界面更紧凑
    m_clearBtn->setMaximumWidth(60);
    m_calculateBtn->setMaximumWidth(60);
    m_exportExcelBtn->setMaximumWidth(80);
    m_exportTextBtn->setMaximumWidth(80);
    m_saveConfigBtn->setMaximumWidth(60);
    m_loadConfigBtn->setMaximumWidth(60);
    m_closeBtn->setMaximumWidth(60);
    
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
    
        qDebug() << "设置表格编辑权限";
        // 临时断开cellChanged信号，避免在设置权限时触发信号
        disconnect(m_dataTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDataTableCellChanged);
        
        // 设置表格编辑权限：正行程角度(列2)和反行程角度(列5)可编辑，其他只读
        for (int i = 0; i < m_dataTable->rowCount(); ++i) {
            for (int j = 0; j < m_dataTable->columnCount(); ++j) {
                QTableWidgetItem *item = m_dataTable->item(i, j);
                if (item) {
                    if (j == 2 || j == 5) {  // 正行程角度(列2)和反行程角度(列5)可编辑
                        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
                        item->setBackground(QColor(230, 255, 230));  // 淡绿色背景表示可编辑
                    } else {
                        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);  // 只读
                        item->setBackground(QColor(245, 245, 245));  // 淡灰色背景表示只读
                    }
                }
            }
        }
        
        // 使用QTimer::singleShot延迟连接信号，确保表格完全初始化后再连接
        QTimer::singleShot(0, this, [this]() {
            // 确保没有重复连接
            disconnect(m_dataTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDataTableCellChanged);
            connect(m_dataTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDataTableCellChanged);
            qDebug() << "cellChanged信号已连接";
        });
        
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
    
    // 写入产品信息 - 按照新格式
    out << QString("产品型号：,%1,,产品名称：,%2,,,\n").arg(m_config.productModel, m_config.productName);
    out << QString("刻度盘图号：,%1,,支组编号：,%2,,,\n").arg(m_config.dialDrawingNo, m_config.groupNo);
    out << "\n"; // 空行
    
    // 收集所有检测点数据
    QStringList detectionPoints, theoreticalAngles, forwardAngles, forwardAngleErrors, forwardPressureErrors;
    QStringList backwardAngles, backwardAngleErrors, backwardPressureErrors, hysteresisAngles, hysteresisPressureErrors;
    
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        double expectedAngle = pressureToAngle(point.pressure);
        
        // 检测点
        detectionPoints << QString::number(point.pressure, 'f', 1);
        
        // 理论角度
        theoreticalAngles << QString::number(expectedAngle, 'f', 2);
        
        // 正行程数据
        if (point.hasForward) {
            forwardAngles << QString::number(point.forwardAngle, 'f', 2);
            double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
            forwardAngleErrors << QString::number(angleError, 'f', 2);
            double pressureError = calculatePressureError(angleError);
            forwardPressureErrors << QString::number(pressureError, 'f', 3);
        } else {
            forwardAngles << "--";
            forwardAngleErrors << "--";
            forwardPressureErrors << "--";
        }
        
        // 反行程数据
        if (point.hasBackward) {
            backwardAngles << QString::number(point.backwardAngle, 'f', 2);
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            backwardAngleErrors << QString::number(angleError, 'f', 2);
            double pressureError = calculatePressureError(angleError);
            backwardPressureErrors << QString::number(pressureError, 'f', 3);
        } else {
            backwardAngles << "--";
            backwardAngleErrors << "--";
            backwardPressureErrors << "--";
        }
        
        // 迟滞误差数据
        if (point.hasForward && point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            hysteresisAngles << QString::number(angleDiff, 'f', 2);
            double hysteresisPressureError = angleToPressure(angleDiff);
            hysteresisPressureErrors << QString::number(hysteresisPressureError, 'f', 3);
        } else {
            hysteresisAngles << "--";
            hysteresisPressureErrors << "--";
        }
    }
    
    // 按照垂直格式写入数据
    out << QString("检测点,%1,,,\n").arg(detectionPoints.join(","));
    out << QString("检测点对应的刻度盘角度,%1,,,\n").arg(theoreticalAngles.join(","));
    out << QString(",%1,,,\n").arg(theoreticalAngles.join(",")); // 第二行（重复）
    out << QString("正行程角度,%1,,,\n").arg(forwardAngles.join(","));
    out << QString("正行程角度误差,%1,,,\n").arg(forwardAngleErrors.join(","));
    out << QString("正行程误差（MPa）,%1,,,\n").arg(forwardPressureErrors.join(","));
    out << QString(",%1,,,\n").arg(forwardPressureErrors.join(",")); // 第二行（重复）
    out << QString("反行程角度,%1,,,\n").arg(backwardAngles.join(","));
    out << QString("反行程角度误差,%1,,,\n").arg(backwardAngleErrors.join(","));
    out << QString("反行程误差（MPa）,%1,,,\n").arg(backwardPressureErrors.join(","));
    out << QString(",%1,,,\n").arg(backwardPressureErrors.join(",")); // 第二行（重复）
    out << QString("迟滞误差角度,%1,,,\n").arg(hysteresisAngles.join(","));
    out << QString("迟滞误差（MPa）,%1,,,\n").arg(hysteresisPressureErrors.join(","));
    
    QMessageBox::information(this, "成功", "数据已导出到CSV文件，按照指定格式排列，可用Excel打开");
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

void ErrorTableDialog::onDataTableCellChanged(int row, int column)
{
    if (row < 0 || row >= m_detectionData.size()) return;
    
    QTableWidgetItem *item = m_dataTable->item(row, column);
    if (!item) return;
    
    bool ok;
    double value = item->text().toDouble(&ok);
    if (!ok) return;
    
    DetectionPoint &point = m_detectionData[row];
    
    // 临时断开cellChanged信号，避免递归调用
    disconnect(m_dataTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDataTableCellChanged);
    
    // 根据列判断修改哪个数据
    if (column == 2) { // 正行程角度
        point.forwardAngle = value;
        point.hasForward = true;
        
        // 只更新相关的计算列，不重新生成整个表格
        double expectedAngle = pressureToAngle(point.pressure);
        double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
        double pressureError = calculatePressureError(angleError);
        
        m_dataTable->item(row, 3)->setText(QString::number(angleError, 'f', 2));
        m_dataTable->item(row, 4)->setText(QString::number(pressureError, 'f', 3));
        
        // 更新迟滞误差（如果有反行程数据）
        if (point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            double hysteresisPressureError = angleToPressure(angleDiff);
            m_dataTable->item(row, 8)->setText(QString::number(angleDiff, 'f', 2));
            m_dataTable->item(row, 9)->setText(QString::number(hysteresisPressureError, 'f', 3));
        }
        
    } else if (column == 5) { // 反行程角度
        point.backwardAngle = value;
        point.hasBackward = true;
        
        // 只更新相关的计算列
        double expectedAngle = pressureToAngle(point.pressure);
        double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
        double pressureError = calculatePressureError(angleError);
        
        m_dataTable->item(row, 6)->setText(QString::number(angleError, 'f', 2));
        m_dataTable->item(row, 7)->setText(QString::number(pressureError, 'f', 3));
        
        // 更新迟滞误差（如果有正行程数据）
        if (point.hasForward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            double hysteresisPressureError = angleToPressure(angleDiff);
            m_dataTable->item(row, 8)->setText(QString::number(angleDiff, 'f', 2));
            m_dataTable->item(row, 9)->setText(QString::number(hysteresisPressureError, 'f', 3));
        }
    }
    
    // 重新连接信号
    connect(m_dataTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDataTableCellChanged);
    
    // 更新分析结果
    validateAndCheckErrors();
}

 