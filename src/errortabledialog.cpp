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
    , m_dialTypeSet(false)
    , m_currentRound(0)
    , m_maxMeasurementsPerRound(6)
{
    qDebug() << "开始创建ErrorTableDialog";
    
    try {
        setWindowTitle("压力表误差检测表格");
        // 设置为非模态窗口，允许主窗口获得焦点
        setWindowModality(Qt::NonModal);
        // 设置窗口标志，确保窗口独立且可以正常切换焦点
        setWindowFlags(Qt::Window);
        // 确保窗口不会强制激活
        setAttribute(Qt::WA_ShowWithoutActivating, false);
        // 不设置为总是置顶
        setAttribute(Qt::WA_AlwaysShowToolTips, false);
        resize(1200, 800);
        
        qDebug() << "基本窗口设置完成";
        
        // 初始化配置数据
        m_config = PressureGaugeConfig();
        
        // 初始化轮次数据
        m_maxAngles.resize(5);  // 5轮数据
        
        qDebug() << "配置数据初始化完成";
        
        // 先创建基本的检测点数据，再初始化轮次数据
        for (double pressure : m_config.detectionPoints) {
            DetectionPoint point;
            point.pressure = pressure;
            m_detectionData.append(point);
        }
        
        qDebug() << "检测点数据创建完成，数量:" << m_detectionData.size();
        
        // 现在可以安全地初始化轮次数据
        initializeRoundData();
        
        qDebug() << "轮次数据初始化完成";
        
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
                
                // 界面初始化完成，等待setDialType调用
                
                qDebug() << "连接信号";
                // 在所有UI初始化完成后再连接信号，避免递归问题
                connect(m_detectionPointsTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDetectionPointsChanged);
                
                // 连接配置参数的信号槽，使其能实时更新表格
                connect(m_maxPressureSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
                connect(m_maxAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
                connect(m_basicErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
                connect(m_hysteresisErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
                
                // 现在UI已完全初始化，可以安全调用轮次显示更新
                updateCurrentRoundDisplay();
                        
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
        QMessageBox::critical(nullptr, "错误", QString("创建对话框失败: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "ErrorTableDialog 构造函数未知异常";
        QMessageBox::critical(nullptr, "错误", "创建对话框失败: 未知异常");
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
    
    qDebug() << "创建轮次切换区域";
    setupRoundSwitchArea();
    
    qDebug() << "创建按钮区域";
    setupButtons();
    
    // 创建上部分的水平布局，将配置和检测点区域放在一行
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);
    topLayout->addWidget(m_configGroup, 2);  // 配置区域占更多空间
    topLayout->addWidget(m_detectionPointsGroup, 1);  // 检测点区域较小
    
    // 创建中间布局，包含轮次切换
    QHBoxLayout *middleLayout = new QHBoxLayout();
    middleLayout->setSpacing(10);
    middleLayout->addWidget(m_roundSwitchGroup);
    middleLayout->addStretch();  // 右边留空
    
    m_mainLayout->addLayout(topLayout);
    m_mainLayout->addLayout(middleLayout);
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
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectItems);  // 改为选择单元格而不是整行
    m_dataTable->setSelectionMode(QAbstractItemView::SingleSelection);   // 单选模式
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

void ErrorTableDialog::setupRoundSwitchArea()
{
    m_roundSwitchGroup = new QGroupBox("轮次切换", this);
    QHBoxLayout *layout = new QHBoxLayout(m_roundSwitchGroup);
    layout->setSpacing(10);
    layout->setContentsMargins(8, 8, 8, 8);
    
    // 轮次选择下拉框
    layout->addWidget(new QLabel("当前轮次:"));
    m_roundCombo = new QComboBox();
    for (int i = 1; i <= 5; ++i) {
        m_roundCombo->addItem(QString("第%1轮").arg(i));
    }
    m_roundCombo->setCurrentIndex(0);  // 默认第1轮
    layout->addWidget(m_roundCombo);
    
    // 轮次信息显示
    m_roundInfoLabel = new QLabel("0/0 数据");
    m_roundInfoLabel->setStyleSheet("color: #666; font-size: 12px;");
    layout->addWidget(m_roundInfoLabel);
    
    layout->addStretch();  // 左对齐
    
    // 连接信号槽
    connect(m_roundCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ErrorTableDialog::onRoundChanged);
}

void ErrorTableDialog::setupButtons()
{
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(8);  // 减少间距
    
    m_clearBtn = new QPushButton("清空");
    m_calculateBtn = new QPushButton("计算");
    m_exportExcelBtn = new QPushButton("导出Excel");
    m_saveConfigBtn = new QPushButton("自动保存");
    m_closeBtn = new QPushButton("关闭");
    
    // 设置按钮的最大宽度让界面更紧凑
    m_clearBtn->setMaximumWidth(60);
    m_calculateBtn->setMaximumWidth(60);
    m_exportExcelBtn->setMaximumWidth(80);
    m_saveConfigBtn->setMaximumWidth(80);
    m_closeBtn->setMaximumWidth(60);
    
    m_buttonLayout->addWidget(m_clearBtn);
    m_buttonLayout->addWidget(m_calculateBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_exportExcelBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_saveConfigBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_closeBtn);
    
    connect(m_clearBtn, &QPushButton::clicked, this, &ErrorTableDialog::clearAllData);
    connect(m_calculateBtn, &QPushButton::clicked, this, &ErrorTableDialog::calculateErrors);
    connect(m_exportExcelBtn, &QPushButton::clicked, this, &ErrorTableDialog::exportToExcel);
    connect(m_saveConfigBtn, &QPushButton::clicked, this, &ErrorTableDialog::saveConfig);
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
        
        // 强制重新连接信号（确保信号连接）
        connect(m_detectionPointsTable, &QTableWidget::cellChanged, this, &ErrorTableDialog::onDetectionPointsChanged);
        
        // 只有在检测数据为空或数量不匹配时才重新创建
        if (m_detectionData.size() != m_config.detectionPoints.size()) {
            qDebug() << "检测数据数量不匹配，重新创建检测数据";
            
            // 保存已有的测量数据
            QMap<double, DetectionPoint> oldData;
            for (const auto& point : m_detectionData) {
                oldData[point.pressure] = point;
            }
            
            // 重新创建检测数据
            m_detectionData.clear();
            for (double pressure : m_config.detectionPoints) {
                DetectionPoint point;
                point.pressure = pressure;
                
                // 如果之前有这个检测点的数据，则保留
                if (oldData.contains(pressure)) {
                    point = oldData[pressure];
                }
                
                m_detectionData.append(point);
            }
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
        
        // 向后兼容：更新老的数据结构
        if (isForward) {
            point.forwardAngle = angle;
            point.hasForward = true;
        } else {
            point.backwardAngle = angle;
            point.hasBackward = true;
        }
        
        // 新的多轮次数据结构：将数据添加到当前轮次
        if (m_currentRound >= 0 && m_currentRound < point.roundData.size()) {
            MeasurementData &currentRoundData = point.roundData[m_currentRound];
            
            if (isForward) {
                // 找到第一个空的正行程位置
                for (int i = 0; i < currentRoundData.forwardAngles.size(); ++i) {
                    if (currentRoundData.forwardAngles[i] == 0.0) {
                        currentRoundData.forwardAngles[i] = angle;
                        qDebug() << "添加第" << (m_currentRound + 1) << "轮第" << (i + 1) << "次正行程数据:" << angle;
                        break;
                    }
                }
            } else {
                // 找到第一个空的反行程位置
                for (int i = 0; i < currentRoundData.backwardAngles.size(); ++i) {
                    if (currentRoundData.backwardAngles[i] == 0.0) {
                        currentRoundData.backwardAngles[i] = angle;
                        qDebug() << "添加第" << (m_currentRound + 1) << "轮第" << (i + 1) << "次反行程数据:" << angle;
                        break;
                    }
                }
            }
        }
        
        updateDataTable();
        validateAndCheckErrors();
        updateCurrentRoundDisplay();
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

void ErrorTableDialog::setDialType(const QString &dialType)
{
    qDebug() << "设置表盘类型:" << dialType;
    
    // 标记已经设置了表盘类型，防止自动加载覆盖
    m_dialTypeSet = true;
    
    // 根据表盘类型设置默认配置
    if (dialType == "YYQY-13") {
        m_config.productModel = "YYQY-13";
        m_config.productName = "";
        m_config.detectionPoints.clear();
        // YYQY-13默认6个检测点 (0, 0.6, 1.2, 1.8, 2.4, 3.0 MPa)
        m_config.detectionPoints << 0.0 << 0.6 << 1.2 << 1.8 << 2.4 << 3.0;
        m_config.maxPressure = 3.0;
        m_config.maxAngle = 270.0;
        m_config.basicErrorLimit = 0.1;
        m_config.hysteresisErrorLimit = 0.15;
        m_maxMeasurementsPerRound = 6;  // YYQY每轮6次测量
    } else if (dialType == "BYQ-19") {
        m_config.productModel = "BYQ-19";
        m_config.productName = "标准压力表";
        m_config.detectionPoints.clear();
        // BYQ-19默认5个检测点 (0, 0.75, 1.5, 2.25, 3.0 MPa)
        m_config.detectionPoints << 0.0 << 0.75 << 1.5 << 2.25 << 3.0;
        m_config.maxPressure = 3.0;
        m_config.maxAngle = 270.0;
        m_config.basicErrorLimit = 0.075;  // 更严格的误差限值
        m_config.hysteresisErrorLimit = 0.1;
        m_maxMeasurementsPerRound = 5;  // BYQ每轮5次测量
    }
    
    // 安全地重新创建检测数据
    try {
        m_detectionData.clear();
        for (double pressure : m_config.detectionPoints) {
            DetectionPoint point;
            point.pressure = pressure;
            m_detectionData.append(point);
        }
        
        qDebug() << "检测点数据已重新创建，数量:" << m_detectionData.size();
        
        // 重新初始化轮次数据（必须在创建m_detectionData之后）
        initializeRoundData();
        
    } catch (const std::exception& e) {
        qDebug() << "重新创建检测数据异常:" << e.what();
        return;  // 如果出错，直接返回，不继续执行
    } catch (...) {
        qDebug() << "重新创建检测数据未知异常";
        return;
    }
    
    // 更新界面显示（仅在UI组件已初始化时）
    if (m_productModelEdit) {  // 检查UI是否已初始化
        updateUIFromConfig();
        updateDetectionPointsTable();
        updateDataTable();
        validateAndCheckErrors();
    } else {
        qDebug() << "UI组件还未初始化，跳过界面更新";
    }
    
    qDebug() << "表盘类型设置完成，检测点数量:" << m_config.detectionPoints.size() 
             << "检测数据数量:" << m_detectionData.size()
             << "每轮测量次数:" << m_maxMeasurementsPerRound;
             
    // 设置表盘类型后，检查是否需要自动加载之前的数据
    checkAndLoadPreviousData();
}

void ErrorTableDialog::checkAndLoadPreviousData()
{
    QString fileName = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PressureGauge_AutoSave.json";
    
    if (QFile::exists(fileName)) {
        qDebug() << "发现自动保存文件，检查是否有相同表盘类型的数据:" << fileName;
        try {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonObject config = doc.object();
                
                // 检查保存的表盘类型是否与当前设置的相同
                QString savedDialType = config["productModel"].toString();
                if (savedDialType == m_config.productModel) {
                    // 检查是否有有效的测量数据
                    bool hasValidMeasurementData = config.contains("detectionData") && 
                                                  config["detectionData"].toArray().size() > 0;
                    
                    if (hasValidMeasurementData) {
                        qDebug() << "找到相同表盘类型的测量数据，询问是否加载";
                        int ret = QMessageBox::question(this, "发现历史数据", 
                                                        QString("发现 %1 的历史测量数据，是否加载？").arg(savedDialType),
                                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                        if (ret == QMessageBox::Yes) {
                            loadConfigFromFile(fileName);
                            qDebug() << "用户选择加载历史数据";
                        } else {
                            qDebug() << "用户选择不加载历史数据";
                        }
                    } else {
                        qDebug() << "自动保存文件存在但无有效测量数据";
                    }
                } else {
                    qDebug() << "保存的表盘类型(" << savedDialType << ")与当前类型(" << m_config.productModel << ")不匹配";
                }
            }
        } catch (const std::exception& e) {
            qDebug() << "检查历史数据失败:" << e.what();
        }
    } else {
        qDebug() << "未找到自动保存文件";
    }
}

void ErrorTableDialog::onConfigChanged()
{
    qDebug() << "配置参数发生变化";
    
    // 更新配置
    updateConfigFromUI();
    
    // 重新计算理论角度并更新数据表格
    updateDataTable();
    
    // 重新计算误差分析
    validateAndCheckErrors();
    
    qDebug() << "配置更新完成";
}

void ErrorTableDialog::onDetectionPointsChanged()
{
    qDebug() << "检测点配置发生变化";
    
    // 保存当前的测量数据
    QMap<double, DetectionPoint> oldData;
    for (const auto& point : m_detectionData) {
        oldData[point.pressure] = point;
    }
    
    // 更新检测点配置
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
    
    // 重建检测数据，保留已有的测量数据
    m_detectionData.clear();
    for (double pressure : m_config.detectionPoints) {
        DetectionPoint point;
        point.pressure = pressure;
        
        // 如果之前有这个检测点的数据，则保留
        if (oldData.contains(pressure)) {
            point = oldData[pressure];
        }
        
        m_detectionData.append(point);
    }
    
    // 更新界面
    updateDetectionPointsTable();
    updateDataTable();
    validateAndCheckErrors();
    
    qDebug() << "检测点配置更新完成，当前检测点数量:" << m_config.detectionPoints.size() 
             << "检测数据数量:" << m_detectionData.size();
}

void ErrorTableDialog::addDetectionPoint()
{
    bool ok;
    double pressure = QInputDialog::getDouble(this, "添加检测点", "压力值(MPa):", 1.0, 0.0, 100.0, 1, &ok);
    if (ok) {
        m_config.detectionPoints.append(pressure);
        // 排序检测点
        std::sort(m_config.detectionPoints.begin(), m_config.detectionPoints.end());
        
        // 手动触发检测点变化处理
        onDetectionPointsChanged();
        
        qDebug() << "添加检测点:" << pressure << "当前检测点数量:" << m_config.detectionPoints.size();
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
    QString result = "<h3>误差检测结果</h3>";
    
    // 基本配置信息和轮次信息
    result += QString("<p><b>型号:</b> %1 &nbsp;&nbsp; <b>限值:</b> 基本±%2MPa 迟滞%3MPa</p>")
              .arg(m_config.productModel)
              .arg(m_config.basicErrorLimit, 0, 'f', 3)
              .arg(m_config.hysteresisErrorLimit, 0, 'f', 3);
              
    result += QString("<p><b>当前轮次:</b> 第%1轮/共5轮 &nbsp;&nbsp; <b>每轮测量:</b> %2次正反行程")
              .arg(m_currentRound + 1)
              .arg(m_maxMeasurementsPerRound);
              
    // 最大角度信息
    double avgMaxAngle = calculateAverageMaxAngle();
    if (avgMaxAngle > 0) {
        result += QString(" &nbsp;&nbsp; <b>平均最大角度:</b> %1°").arg(avgMaxAngle, 0, 'f', 2);
    }
    result += "</p>";
    
    bool allPointsValid = true;
    int validPointsCount = 0;
    
    // 统计检测结果
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        
        if (!point.hasForward && !point.hasBackward) {
            continue; // 跳过无数据的点
        }
        
        validPointsCount++;
        double expectedAngle = pressureToAngle(point.pressure);
        
        // 检查基本误差
        if (point.hasForward) {
            double angleError = calculateAngleError(point.forwardAngle, expectedAngle);
            double pressureError = std::abs(calculatePressureError(angleError));
            
            if (pressureError > m_config.basicErrorLimit) {
                allPointsValid = false;
                result += QString("<p style='color: red;'><b>%1 MPa 正行程:</b> 误差 %2 MPa <span style='font-weight: bold;'>超标</span></p>")
                          .arg(point.pressure, 0, 'f', 1)
                          .arg(pressureError, 0, 'f', 3);
            }
        }
        
        if (point.hasBackward) {
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            double pressureError = std::abs(calculatePressureError(angleError));
            
            if (pressureError > m_config.basicErrorLimit) {
                allPointsValid = false;
                result += QString("<p style='color: red;'><b>%1 MPa 反行程:</b> 误差 %2 MPa <span style='font-weight: bold;'>超标</span></p>")
                          .arg(point.pressure, 0, 'f', 1)
                          .arg(pressureError, 0, 'f', 3);
            }
        }
        
        // 检查迟滞误差
        if (point.hasForward && point.hasBackward) {
            double angleDiff = std::abs(point.forwardAngle - point.backwardAngle);
            double hysteresisErrorMPa = angleToPressure(angleDiff);
            
            if (hysteresisErrorMPa > m_config.hysteresisErrorLimit) {
                allPointsValid = false;
                result += QString("<p style='color: red;'><b>%1 MPa 迟滞:</b> 误差 %2 MPa <span style='font-weight: bold;'>超标</span></p>")
                          .arg(point.pressure, 0, 'f', 1)
                          .arg(hysteresisErrorMPa, 0, 'f', 3);
            }
        }
    }
    
    // 总结（简化显示）
    if (validPointsCount == 0) {
        result += "<p style='color: orange; font-size: 16px; font-weight: bold;'>⚠ 暂无测量数据</p>";
    } else if (allPointsValid) {
        result += "<p style='color: green; font-size: 16px; font-weight: bold;'>✓ 检测合格</p>";
        result += QString("<p>已检测 %1 个点，均符合误差要求</p>").arg(validPointsCount);
    } else {
        result += "<p style='color: red; font-size: 16px; font-weight: bold;'>✗ 检测不合格</p>";
        result += "<p style='color: red;'>请调整超标检测点的刻度盘角度</p>";
    }
    
    return result;
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
    out << QString("产品型号：,%1,,产品名称：,%2,,,\n").arg(m_config.productModel, m_config.productName);
    out << QString("刻度盘图号：,%1,,支组编号：,%2,,,\n").arg(m_config.dialDrawingNo, m_config.groupNo);
    out << "\n"; // 空行
    
    // 写入每轮测量次数说明
    out << QString("测量说明：,%1表盘，每轮正反行程各测量%2次，共5轮数据\n")
        .arg(m_config.productModel)
        .arg(m_maxMeasurementsPerRound);
    out << "\n"; // 空行
    
    // 收集检测点数据
    QStringList detectionPoints;
    for (const DetectionPoint &point : m_detectionData) {
        detectionPoints << QString::number(point.pressure, 'f', 1);
    }
    
    // 写入检测点行
    out << QString("检测点,%1\n").arg(detectionPoints.join(","));
    
    // 为每轮数据写入正行程和反行程
    for (int round = 0; round < 5; ++round) {
        out << QString("\n第%1轮测量数据,\n").arg(round + 1);
        
        // 写入正行程数据（每次测量一行）
        for (int measurement = 0; measurement < m_maxMeasurementsPerRound; ++measurement) {
            QStringList forwardData;
            for (const DetectionPoint &point : m_detectionData) {
                if (round < point.roundData.size() && 
                    measurement < point.roundData[round].forwardAngles.size() &&
                    point.roundData[round].forwardAngles[measurement] != 0.0) {
                    forwardData << QString::number(point.roundData[round].forwardAngles[measurement], 'f', 2);
                } else {
                    forwardData << "--";
                }
            }
            out << QString("正行程第%1次,%2\n").arg(measurement + 1).arg(forwardData.join(","));
        }
        
        // 写入反行程数据（每次测量一行）
        for (int measurement = 0; measurement < m_maxMeasurementsPerRound; ++measurement) {
            QStringList backwardData;
            for (const DetectionPoint &point : m_detectionData) {
                if (round < point.roundData.size() && 
                    measurement < point.roundData[round].backwardAngles.size() &&
                    point.roundData[round].backwardAngles[measurement] != 0.0) {
                    backwardData << QString::number(point.roundData[round].backwardAngles[measurement], 'f', 2);
                } else {
                    backwardData << "--";
                }
            }
            out << QString("反行程第%1次,%2\n").arg(measurement + 1).arg(backwardData.join(","));
        }
        
        // 写入该轮的最大角度
        QStringList maxAngles;
        for (const DetectionPoint &point : m_detectionData) {
            if (round < point.roundData.size() && point.roundData[round].maxAngle != 0.0) {
                maxAngles << QString::number(point.roundData[round].maxAngle, 'f', 2);
            } else {
                maxAngles << "--";
            }
        }
        out << QString("最大角度,%1\n").arg(maxAngles.join(","));
    }
    
    // 写入汇总统计
    out << QString("\n汇总统计,\n");
    QStringList avgMaxAngles;
    for (int i = 0; i < m_detectionData.size(); ++i) {
        double avgMaxAngle = calculateAverageMaxAngle();
        avgMaxAngles << QString::number(avgMaxAngle, 'f', 2);
    }
    out << QString("平均最大角度,%1\n").arg(avgMaxAngles.join(","));
    
    QMessageBox::information(this, "成功", 
        QString("5轮测量数据已导出到CSV文件\n文件位置: %1\n包含%2轮数据，每轮正反行程各%3次测量")
        .arg(fileName)
        .arg(5)
        .arg(m_maxMeasurementsPerRound));
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
    // 自动保存到默认位置，不再弹出文件选择对话框
    QString fileName = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PressureGauge_AutoSave.json";
    
    try {
        saveConfigToFile(fileName);
        QMessageBox::information(this, "成功", QString("数据已自动保存到:\n%1").arg(fileName));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("自动保存失败: %1").arg(e.what()));
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
    
    // 保存测量数据（包含多轮次数据）
    QJsonArray detectionData;
    for (const DetectionPoint &point : m_detectionData) {
        QJsonObject pointObj;
        pointObj["pressure"] = point.pressure;
        pointObj["forwardAngle"] = point.forwardAngle;  // 向后兼容
        pointObj["backwardAngle"] = point.backwardAngle;  // 向后兼容
        pointObj["hasForward"] = point.hasForward;  // 向后兼容
        pointObj["hasBackward"] = point.hasBackward;  // 向后兼容
        
        // 保存多轮次数据
        QJsonArray roundDataArray;
        for (const MeasurementData &roundData : point.roundData) {
            QJsonObject roundObj;
            
            QJsonArray forwardAngles;
            for (double angle : roundData.forwardAngles) {
                forwardAngles.append(angle);
            }
            roundObj["forwardAngles"] = forwardAngles;
            
            QJsonArray backwardAngles;
            for (double angle : roundData.backwardAngles) {
                backwardAngles.append(angle);
            }
            roundObj["backwardAngles"] = backwardAngles;
            
            roundObj["maxAngle"] = roundData.maxAngle;
            roundDataArray.append(roundObj);
        }
        pointObj["roundData"] = roundDataArray;
        
        detectionData.append(pointObj);
    }
    config["detectionData"] = detectionData;
    
    // 保存轮次管理数据
    config["currentRound"] = m_currentRound;
    config["maxMeasurementsPerRound"] = m_maxMeasurementsPerRound;
    
    QJsonArray maxAnglesArray;
    for (double maxAngle : m_maxAngles) {
        maxAnglesArray.append(maxAngle);
    }
    config["maxAngles"] = maxAnglesArray;
    
    QJsonDocument doc(config);
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        qDebug() << "数据已保存到:" << fileName;
    } else {
        throw std::runtime_error("无法保存配置文件");
    }
}

void ErrorTableDialog::loadConfigFromFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("无法读取配置文件");
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
    
    // 加载测量数据
    m_detectionData.clear();
    if (config.contains("detectionData")) {
        QJsonArray detectionData = config["detectionData"].toArray();
        for (const QJsonValue &value : detectionData) {
            QJsonObject pointObj = value.toObject();
            DetectionPoint point;
            point.pressure = pointObj["pressure"].toDouble();
            point.forwardAngle = pointObj["forwardAngle"].toDouble();  // 向后兼容
            point.backwardAngle = pointObj["backwardAngle"].toDouble();  // 向后兼容
            point.hasForward = pointObj["hasForward"].toBool();  // 向后兼容
            point.hasBackward = pointObj["hasBackward"].toBool();  // 向后兼容
            
            // 加载多轮次数据
            if (pointObj.contains("roundData")) {
                QJsonArray roundDataArray = pointObj["roundData"].toArray();
                point.roundData.clear();
                point.roundData.resize(5);  // 确保有5轮数据
                
                for (int i = 0; i < roundDataArray.size() && i < 5; ++i) {
                    QJsonObject roundObj = roundDataArray[i].toObject();
                    
                    // 加载正行程数据
                    if (roundObj.contains("forwardAngles")) {
                        QJsonArray forwardAngles = roundObj["forwardAngles"].toArray();
                        point.roundData[i].forwardAngles.clear();
                        for (const QJsonValue &angle : forwardAngles) {
                            point.roundData[i].forwardAngles.append(angle.toDouble());
                        }
                    }
                    
                    // 加载反行程数据
                    if (roundObj.contains("backwardAngles")) {
                        QJsonArray backwardAngles = roundObj["backwardAngles"].toArray();
                        point.roundData[i].backwardAngles.clear();
                        for (const QJsonValue &angle : backwardAngles) {
                            point.roundData[i].backwardAngles.append(angle.toDouble());
                        }
                    }
                    
                    // 加载最大角度
                    point.roundData[i].maxAngle = roundObj["maxAngle"].toDouble();
                }
            } else {
                // 如果没有多轮次数据，初始化空的轮次数据
                point.roundData.clear();
                point.roundData.resize(5);
            }
            
            m_detectionData.append(point);
        }
    } else {
        // 如果没有测量数据，创建空的检测点数据
        for (double pressure : m_config.detectionPoints) {
            DetectionPoint point;
            point.pressure = pressure;
            m_detectionData.append(point);
        }
    }
    
    // 加载轮次管理数据
    if (config.contains("currentRound")) {
        m_currentRound = config["currentRound"].toInt();
    }
    if (config.contains("maxMeasurementsPerRound")) {
        m_maxMeasurementsPerRound = config["maxMeasurementsPerRound"].toInt();
    }
    if (config.contains("maxAngles")) {
        QJsonArray maxAnglesArray = config["maxAngles"].toArray();
        m_maxAngles.clear();
        m_maxAngles.resize(5);
        for (int i = 0; i < maxAnglesArray.size() && i < 5; ++i) {
            m_maxAngles[i] = maxAnglesArray[i].toDouble();
        }
    }
    
    // 确保所有检测点都有完整的轮次数据
    for (DetectionPoint &point : m_detectionData) {
        if (point.roundData.size() != 5) {
            point.roundData.resize(5);
        }
        for (MeasurementData &roundData : point.roundData) {
            if (roundData.forwardAngles.size() != m_maxMeasurementsPerRound) {
                roundData.forwardAngles.resize(m_maxMeasurementsPerRound);
            }
            if (roundData.backwardAngles.size() != m_maxMeasurementsPerRound) {
                roundData.backwardAngles.resize(m_maxMeasurementsPerRound);
            }
        }
    }
    
    updateUIFromConfig();
    updateDetectionPointsTable();
    updateDataTable();
    validateAndCheckErrors();
    
    qDebug() << "配置和数据已从" << fileName << "加载完成";
}

void ErrorTableDialog::autoLoadPreviousData()
{
    // 如果已经通过setDialType设置了表盘类型，则不进行自动加载
    if (m_dialTypeSet) {
        qDebug() << "已设置表盘类型，跳过自动加载";
        return;
    }
    
    QString fileName = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PressureGauge_AutoSave.json";
    
    if (QFile::exists(fileName)) {
        qDebug() << "发现自动保存文件，检查是否有有效数据:" << fileName;
        try {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonObject config = doc.object();
                
                // 检查是否有有效的检测点数据或测量数据
                bool hasValidDetectionPoints = config.contains("detectionPoints") && 
                                             config["detectionPoints"].toArray().size() > 0;
                bool hasValidMeasurementData = config.contains("detectionData") && 
                                              config["detectionData"].toArray().size() > 0;
                
                if (hasValidDetectionPoints || hasValidMeasurementData) {
                    loadConfigFromFile(fileName);
                    qDebug() << "自动加载完成，有有效数据";
                } else {
                    qDebug() << "自动保存文件存在但无有效数据，保持当前默认配置";
                }
            }
        } catch (const std::exception& e) {
            qDebug() << "自动加载失败:" << e.what();
            // 加载失败时使用默认配置，不显示错误信息
        }
    } else {
        qDebug() << "未找到自动保存文件，保持当前默认配置";
    }
}

void ErrorTableDialog::onTableCellClicked(int row, int column)
{
    if (row >= 0 && row < m_detectionData.size()) {
        m_currentPressureIndex = row;
        double pressure = m_detectionData[row].pressure;
        m_currentPointLabel->setText(QString("当前检测点: %1 MPa").arg(pressure, 0, 'f', 1));
        
        // 根据点击的列设置测量方向
        if (column == 2) {  // 正行程角度列
            m_directionCombo->setCurrentIndex(0);  // 正行程
            m_isForwardDirection = true;
        } else if (column == 5) {  // 反行程角度列
            m_directionCombo->setCurrentIndex(1);  // 反行程
            m_isForwardDirection = false;
        }
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

// ================== 轮次管理方法实现 ==================

void ErrorTableDialog::initializeRoundData()
{
    m_currentRound = 0;
    m_maxAngles.fill(0.0);
    
    // 为所有检测点初始化轮次数据
    for (DetectionPoint &point : m_detectionData) {
        point.roundData.clear();
        point.roundData.resize(5);  // 5轮数据
        
        // 为每轮初始化数据
        for (int round = 0; round < 5; ++round) {
            point.roundData[round].forwardAngles.resize(m_maxMeasurementsPerRound);
            point.roundData[round].backwardAngles.resize(m_maxMeasurementsPerRound);
            point.roundData[round].maxAngle = 0.0;
        }
    }
    
    qDebug() << "轮次数据已初始化，最大测量次数:" << m_maxMeasurementsPerRound;
    // 注意：不在这里调用updateCurrentRoundDisplay()，因为UI可能还没有初始化
}

void ErrorTableDialog::resetCurrentRound()
{
    qDebug() << "重置当前轮次(" << m_currentRound + 1 << ")数据";
    
    // 重置当前轮次的所有检测点数据
    for (DetectionPoint &point : m_detectionData) {
        if (m_currentRound < point.roundData.size()) {
            MeasurementData &currentData = point.roundData[m_currentRound];
            currentData.forwardAngles.fill(0.0);
            currentData.backwardAngles.fill(0.0);
            currentData.maxAngle = 0.0;
        }
    }
    
    // 重置当前轮次的最大角度
    if (m_currentRound < m_maxAngles.size()) {
        m_maxAngles[m_currentRound] = 0.0;
    }
    
    // 更新界面显示
    updateDataTable();
    updateCurrentRoundDisplay();
    
    QMessageBox::information(this, "归位", QString("第%1轮数据已重置").arg(m_currentRound + 1));
}

void ErrorTableDialog::saveCurrentRound()
{
    qDebug() << "保存当前轮次(" << m_currentRound + 1 << ")数据";
    
    // 检查当前轮次是否有有效数据
    bool hasValidData = false;
    for (const DetectionPoint &point : m_detectionData) {
        if (m_currentRound < point.roundData.size()) {
            const MeasurementData &currentData = point.roundData[m_currentRound];
            for (double angle : currentData.forwardAngles) {
                if (angle != 0.0) {
                    hasValidData = true;
                    break;
                }
            }
            if (hasValidData) break;
            
            for (double angle : currentData.backwardAngles) {
                if (angle != 0.0) {
                    hasValidData = true;
                    break;
                }
            }
            if (hasValidData) break;
        }
    }
    
    if (!hasValidData) {
        QMessageBox::warning(this, "警告", "当前轮次没有有效的测量数据！");
        return;
    }
    
    // 移动到下一轮
    if (m_currentRound < 4) {
        m_currentRound++;
        updateCurrentRoundDisplay();
        QMessageBox::information(this, "保存", 
            QString("第%1轮数据已保存！\n准备开始第%2轮测量")
            .arg(m_currentRound)
            .arg(m_currentRound + 1));
    } else {
        QMessageBox::information(this, "保存", "第5轮数据已保存！\n所有5轮测量完成");
    }
    
    // 自动保存到文件
    saveConfig();
}

void ErrorTableDialog::addMaxAngleData(double maxAngle)
{
    if (m_currentRound >= 0 && m_currentRound < m_maxAngles.size()) {
        m_maxAngles[m_currentRound] = maxAngle;
        qDebug() << "添加第" << (m_currentRound + 1) << "轮最大角度:" << maxAngle;
        
        // 更新所有检测点的当前轮次最大角度
        for (DetectionPoint &point : m_detectionData) {
            if (m_currentRound < point.roundData.size()) {
                point.roundData[m_currentRound].maxAngle = maxAngle;
            }
        }
        
        updateCurrentRoundDisplay();
    }
}

double ErrorTableDialog::calculateAverageMaxAngle() const
{
    double sum = 0.0;
    int count = 0;
    
    for (int i = 0; i <= m_currentRound && i < m_maxAngles.size(); ++i) {
        if (m_maxAngles[i] != 0.0) {
            sum += m_maxAngles[i];
            count++;
        }
    }
    
    return count > 0 ? sum / count : 0.0;
}

int ErrorTableDialog::getExpectedMeasurements() const
{
    return m_maxMeasurementsPerRound;
}

void ErrorTableDialog::updateCurrentRoundDisplay()
{
    // 检查UI组件是否已初始化
    if (!m_currentPointLabel) {
        qDebug() << "UI组件还未初始化，跳过轮次显示更新";
        return;
    }
    
    try {
        // 更新当前轮次显示标签
        QString currentInfo = m_currentPointLabel->text();
        if (currentInfo.contains("当前检测点:")) {
            // 保持检测点信息，添加轮次信息
            QString roundInfo = QString(" | 第%1轮/共5轮").arg(m_currentRound + 1);
            if (!currentInfo.contains("|")) {
                m_currentPointLabel->setText(currentInfo + roundInfo);
            } else {
                // 替换已有的轮次信息
                QStringList parts = currentInfo.split(" | ");
                if (parts.size() > 0) {
                    m_currentPointLabel->setText(parts[0] + roundInfo);
                }
            }
        } else {
            m_currentPointLabel->setText(QString("第%1轮测量/共5轮").arg(m_currentRound + 1));
        }
        
        // 更新分析文本中的轮次信息（如果分析文本组件已初始化）
        if (m_analysisText) {
            updateAnalysisText();
        }
        
        qDebug() << "当前轮次:" << (m_currentRound + 1) << "/5";
        qDebug() << "平均最大角度:" << calculateAverageMaxAngle();
    } catch (const std::exception& e) {
        qDebug() << "更新轮次显示异常:" << e.what();
    } catch (...) {
        qDebug() << "更新轮次显示未知异常";
    }
}

// 生成多轮次导出数据
QString ErrorTableDialog::generateMultiRoundExportData()
{
    QString result;
    
    // 生成标题行
    QStringList headers;
    headers << "检测点";
    
    // 为每轮添加标题列
    for (int round = 1; round <= 5; ++round) {
        for (int measurement = 1; measurement <= m_maxMeasurementsPerRound; ++measurement) {
            headers << QString("第%1轮第%2次正行程").arg(round).arg(measurement);
        }
        for (int measurement = 1; measurement <= m_maxMeasurementsPerRound; ++measurement) {
            headers << QString("第%1轮第%2次反行程").arg(round).arg(measurement);
        }
    }
    
    result = headers.join(",") + "\n";
    
    // 为每个检测点生成数据行
    for (int pointIndex = 0; pointIndex < m_detectionData.size(); ++pointIndex) {
        const DetectionPoint &point = m_detectionData[pointIndex];
        QStringList rowData;
        
        // 检测点压力值
        rowData << QString::number(point.pressure, 'f', 1);
        
        // 遍历5轮数据
        for (int round = 0; round < 5; ++round) {
            if (round < point.roundData.size()) {
                const MeasurementData &roundData = point.roundData[round];
                
                // 正行程数据
                for (int measurement = 0; measurement < m_maxMeasurementsPerRound; ++measurement) {
                    if (measurement < roundData.forwardAngles.size() && roundData.forwardAngles[measurement] != 0.0) {
                        rowData << QString::number(roundData.forwardAngles[measurement], 'f', 2);
                    } else {
                        rowData << "--";
                    }
                }
                
                // 反行程数据
                for (int measurement = 0; measurement < m_maxMeasurementsPerRound; ++measurement) {
                    if (measurement < roundData.backwardAngles.size() && roundData.backwardAngles[measurement] != 0.0) {
                        rowData << QString::number(roundData.backwardAngles[measurement], 'f', 2);
                    } else {
                        rowData << "--";
                    }
                }
            } else {
                // 如果该轮没有数据，填充空值
                for (int i = 0; i < m_maxMeasurementsPerRound * 2; ++i) {
                    rowData << "--";
                }
            }
        }
        
        result += rowData.join(",") + "\n";
    }
    
    return result;
}

// ================== 轮次切换方法实现 ==================

void ErrorTableDialog::onRoundChanged(int roundIndex)
{
    if (roundIndex >= 0 && roundIndex < 5) {
        m_currentRound = roundIndex;
        
        // 更新数据表格显示当前轮次的数据
        updateDataTable();
        
        // 更新轮次信息显示
        updateRoundInfoDisplay();
        
        // 更新当前轮次显示
        updateCurrentRoundDisplay();
        
        qDebug() << "切换到第" << (roundIndex + 1) << "轮";
    }
}

void ErrorTableDialog::updateRoundInfoDisplay()
{
    if (!m_roundInfoLabel) return;
    
    if (m_currentRound >= 0 && m_currentRound < m_maxAngles.size()) {
        // 统计当前轮次的数据量
        int totalData = 0;
        int maxPossibleData = m_detectionData.size() * m_maxMeasurementsPerRound * 2; // 正反行程
        
        for (const DetectionPoint &point : m_detectionData) {
            if (m_currentRound < point.roundData.size()) {
                const MeasurementData &roundData = point.roundData[m_currentRound];
                
                // 计算正行程数据量
                for (double angle : roundData.forwardAngles) {
                    if (angle != 0.0) totalData++;
                }
                
                // 计算反行程数据量
                for (double angle : roundData.backwardAngles) {
                    if (angle != 0.0) totalData++;
                }
            }
        }
        
        m_roundInfoLabel->setText(QString("%1/%2 数据").arg(totalData).arg(maxPossibleData));
    } else {
        m_roundInfoLabel->setText("0/0 数据");
    }
}

void ErrorTableDialog::setCurrentRound(int round)
{
    if (round >= 0 && round < 5) {
        m_currentRound = round;
        
        // 同步更新UI
        if (m_roundCombo) {
            m_roundCombo->blockSignals(true);  // 阻止信号触发
            m_roundCombo->setCurrentIndex(round);
            m_roundCombo->blockSignals(false);
        }
        
        // 更新显示
        updateDataTable();
        updateRoundInfoDisplay();
        updateCurrentRoundDisplay();
        
        qDebug() << "设置当前轮次为第" << (round + 1) << "轮";
    }
}

int ErrorTableDialog::getCurrentRound() const
{
    return m_currentRound;
}

 