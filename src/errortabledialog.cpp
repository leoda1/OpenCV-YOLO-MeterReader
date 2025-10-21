#include "errortabledialog.h"
#include <QSplitter>
#include <QScrollArea>
#include <QToolTip>
#include <QInputDialog>
#include <QTimer>
#include <cmath>
#include <algorithm>

// ======== NEW: 预检阈值计算辅助函数（仅本文件可见） ========
// 按型号获取满量程压力（MPa）：YYQY=6.3，BYQ=25；其他回退到配置值
static inline double modelFullScalePressure(const PressureGaugeConfig& cfg) {
    if (cfg.productModel == "YYQY-13")return 6.3;
    if (cfg.productModel == "BYQ-19") return 25.0;
    return cfg.maxPressure;
}

// 型号级"预检"迟滞限值（MPa）：YYQY-13=0.3，BYQ-19=2.0；其他回退到配置里的迟滞限值
static inline double modelPrecheckHysteresisLimitMPa(const PressureGaugeConfig& cfg) {
    if (cfg.productModel == "YYQY-13") return 0.3;
    if (cfg.productModel == "BYQ-19") return 2.0;
    return cfg.hysteresisErrorLimit;
}

// 根据"本轮最大角度"和"型号预检迟滞限值(MPa)"得到"预检迟滞阈值角度(°)"
static inline double precheckHysteresisAngleDeg(const PressureGaugeConfig& cfg, double roundMaxAngleDeg) {
    const double fsPressure = modelFullScalePressure(cfg);
    if (fsPressure <= 0.0) return 0.0;

    // 基准角度用于夹紧本轮最大角度，防止单位/采集异常
    const double fsDeg = (cfg.maxAngle > 0.0 ? cfg.maxAngle : 270.0);
    double safeRoundMax = roundMaxAngleDeg;
    if (safeRoundMax <= 0.0 || safeRoundMax < 0.5 * fsDeg || safeRoundMax > 1.5 * fsDeg) {
        // 落在合理区间外就回退到当前配置的“满量程角度/平均最大角度”
        safeRoundMax = fsDeg;
    }
    // 迟滞阈值角度 = (迟滞限值MPa / 满量程压力MPa) × 本轮最大角度(°)
    return (modelPrecheckHysteresisLimitMPa(cfg) / fsPressure) * safeRoundMax;
}

// 读取一轮中"首个有效正/反行程角度"，没有数据则返回 false
static inline bool firstValidAnglesOfRound(const MeasurementData& rd, double& fwd, double& bwd) {
    bool hasF = false, hasB = false;
    for (double a : rd.forwardAngles) { if (a != 0.0) { fwd = a; hasF = true; break; } }
    for (double a : rd.backwardAngles){ if (a != 0.0) { bwd = a; hasB = true; break; } }
    return hasF && hasB;
}

// 通用角度->压力换算：压力 = 角度 * (满量程压力 / 满量程角度)
static inline double angleToPressureByFS(double angleErrDeg, double fsAngleDeg, double fsPressureMPa) {
    if (fsAngleDeg <= 0.0) return 0.0;
    return (angleErrDeg / fsAngleDeg) * fsPressureMPa;
}
// ======== END NEW ========

ErrorTableDialog::ErrorTableDialog(QWidget *parent)
    : QDialog(parent)
    , m_currentPressureIndex(0)
    , m_isForwardDirection(true)
    , m_dialTypeSet(false)
    , m_currentRound(0)
    , m_totalRounds(5)  // 默认5轮
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
        m_maxAngles.resize(m_totalRounds);  // 动态轮数
        
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
                // 删除未使用的满量程压力和满量程角度信号连接
                connect(m_basicErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
                connect(m_hysteresisErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onConfigChanged);
                
                // 现在UI已完全初始化，可以安全调用轮次显示更新
                updateCurrentRoundDisplay();
                        
                qDebug() << "延迟初始化完成";

                // ===== 新增：构造后自动加载上次保存的数据（若存在），并刷新分析区 =====
                // 若之前通过 setDialType() 显式设置过型号，则 autoLoadPreviousData() 内部会跳过加载
                // 加载函数内部会触发 updateUIFromConfig() / updateDataTable() / validateAndCheckErrors()
                // 从而确保 formatAnalysisResult() 在打开对话框时就是最新的
                autoLoadPreviousData();
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
    
    // 技术参数 - 删除未使用的满量程压力和满量程角度输入框
    
    layout->addWidget(new QLabel("基本误差限值(MPa):"), 3, 0);
    m_basicErrorLimitSpin = new QDoubleSpinBox();
    m_basicErrorLimitSpin->setRange(0.01, 1.0);
    m_basicErrorLimitSpin->setDecimals(3);
    m_basicErrorLimitSpin->setSingleStep(0.001);
    m_basicErrorLimitSpin->setMaximumWidth(100);
    layout->addWidget(m_basicErrorLimitSpin, 3, 1);
    
    layout->addWidget(new QLabel("迟滞误差限值(MPa):"), 3, 2);
    m_hysteresisErrorLimitSpin = new QDoubleSpinBox();
    // 放宽范围以支持不同表盘（BYQ=2.0）
    m_hysteresisErrorLimitSpin->setRange(0.01, 5.0);
    m_hysteresisErrorLimitSpin->setDecimals(3);
    m_hysteresisErrorLimitSpin->setSingleStep(0.001);
    m_hysteresisErrorLimitSpin->setMaximumWidth(100);
    m_hysteresisErrorLimitSpin->setToolTip("由表盘型号自动决定：YYQY=0.3，BYQ=2.0");
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
    
    // 移除添加和删除按钮，只保留检测点表格
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
    
    statusLayout->addWidget(m_currentPointLabel);
    statusLayout->addStretch();
    layout->addLayout(statusLayout);
    
    // 数据表格
    m_dataTable = new QTableWidget(0, 10);
    QStringList headers;
    headers << "检测点(MPa)" << "最终角度(°)" << "正行程角度(°)" << "正行程角度误差(°)" << "正行程误差(MPa)"
            << "反行程角度(°)" << "反行程角度误差(°)" << "反行程误差(MPa)" << "迟滞误差角度(°)" << "迟滞误差(MPa)";
    m_dataTable->setHorizontalHeaderLabels(headers);
    
    // 设置列宽 - 第一列加宽，其他列适当调整
    m_dataTable->setColumnWidth(0, 120);  // 检测点列加宽
    m_dataTable->setColumnWidth(1, 100);  // 最终角度
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
    for (int i = 1; i <= m_totalRounds; ++i) {
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
    // 删除未使用的计算按钮
    // m_calculateBtn = new QPushButton("计算");
    m_exportExcelBtn = new QPushButton("导出Excel");
    m_saveConfigBtn = new QPushButton("自动保存");
    m_closeBtn = new QPushButton("关闭");
    
    // 设置按钮的最大宽度让界面更紧凑
    m_clearBtn->setMaximumWidth(60);
    // 删除未使用的计算按钮宽度设置
    // m_calculateBtn->setMaximumWidth(60);
    m_exportExcelBtn->setMaximumWidth(80);
    m_saveConfigBtn->setMaximumWidth(80);
    m_closeBtn->setMaximumWidth(60);
    
    m_buttonLayout->addWidget(m_clearBtn);
    // 删除未使用的计算按钮添加到布局
    // m_buttonLayout->addWidget(m_calculateBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_exportExcelBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_saveConfigBtn);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_closeBtn);
    
    connect(m_clearBtn, &QPushButton::clicked, this, &ErrorTableDialog::clearAllData);
    // 删除未使用的计算按钮信号连接
    // connect(m_calculateBtn, &QPushButton::clicked, this, &ErrorTableDialog::calculateErrors);
    connect(m_exportExcelBtn, &QPushButton::clicked, this, &ErrorTableDialog::exportToExcel);
    connect(m_saveConfigBtn, &QPushButton::clicked, this, &ErrorTableDialog::saveConfig);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    // 连接输入框的实时更新信号
    connect(m_productModelEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onProductInfoChanged);
    connect(m_productNameEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onProductInfoChanged);
    connect(m_dialDrawingNoEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onProductInfoChanged);
    connect(m_groupNoEdit, &QLineEdit::textChanged, this, &ErrorTableDialog::onProductInfoChanged);
    // 删除未使用的满量程压力和满量程角度信号连接
    // connect(m_maxPressureSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onProductInfoChanged);
    // connect(m_maxAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onProductInfoChanged);
    connect(m_basicErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onProductInfoChanged);
    connect(m_hysteresisErrorLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ErrorTableDialog::onProductInfoChanged);
}

void ErrorTableDialog::updateConfigFromUI()
{
    m_config.productModel = m_productModelEdit->text();
    m_config.productName = m_productNameEdit->text();
    m_config.dialDrawingNo = m_dialDrawingNoEdit->text();
    m_config.groupNo = m_groupNoEdit->text();
    // 删除未使用的满量程压力和满量程角度配置
    // m_config.maxPressure = m_maxPressureSpin->value();
    // m_config.maxAngle = m_maxAngleSpin->value();
    m_config.basicErrorLimit = m_basicErrorLimitSpin->value();
    m_config.hysteresisErrorLimit = m_hysteresisErrorLimitSpin->value();
}

void ErrorTableDialog::updateUIFromConfig()
{
    qDebug() << "updateUIFromConfig 开始检查UI组件";
    
    // 检查所有UI组件是否已初始化
    if (!m_productModelEdit || !m_productNameEdit || !m_dialDrawingNoEdit || !m_groupNoEdit ||
        !m_basicErrorLimitSpin || !m_hysteresisErrorLimitSpin) {
        qDebug() << "某些UI组件还没有初始化，跳过updateUIFromConfig";
        return;
    }
    
    qDebug() << "开始设置UI组件的值";
    
    try {
        m_productModelEdit->setText(m_config.productModel);
        m_productNameEdit->setText(m_config.productName);
        m_dialDrawingNoEdit->setText(m_config.dialDrawingNo);
        m_groupNoEdit->setText(m_config.groupNo);
        // 删除未使用的满量程压力和满量程角度设置
        // m_maxPressureSpin->setValue(m_config.maxPressure);
        // m_maxAngleSpin->setValue(m_config.maxAngle);
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
    qDebug() << "updateDataTable 开始，当前轮次:" << (m_currentRound + 1);
    
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

        // 检测点对应的刻度盘角度（最终角度）= 已完成"正+反"成对数据的实测平均（跨轮）
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
        m_dataTable->setItem(i, 1, new QTableWidgetItem(QString::number(finalAngle, 'f', 2)));
        
        // 获取当前轮次的数据
        double currentForwardAngle = 0.0;
        double currentBackwardAngle = 0.0;
        bool hasForwardData = false;
        bool hasBackwardData = false;
        
        if (m_currentRound < point.roundData.size()) {
            const MeasurementData &currentRoundData = point.roundData[m_currentRound];
            
            // 获取正行程数据（取第一个非零值）
            for (double angle : currentRoundData.forwardAngles) {
                if (angle != 0.0) {
                    currentForwardAngle = angle;
                    hasForwardData = true;
                    break;
                }
            }
            
            // 获取反行程数据（取第一个非零值）
            for (double angle : currentRoundData.backwardAngles) {
                if (angle != 0.0) {
                    currentBackwardAngle = angle;
                    hasBackwardData = true;
                    break;
                }
            }
        }
        
        // 正行程角度
        if (hasForwardData) {
            m_dataTable->setItem(i, 2, new QTableWidgetItem(QString::number(currentForwardAngle, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 2, new QTableWidgetItem("--"));
        }
        
        // 正行程角度误差 - 所有轮次完成后计算
        if (hasForwardData && isAllRoundsCompleted()) {
            // 最终阶段：与"最终角度"(实测对数平均)比较；若无最终角度则退化到理论角度
            double expected = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
            double angleError = calculateAngleError(currentForwardAngle, expected);
            m_dataTable->setItem(i, 3, new QTableWidgetItem(QString::number(angleError, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 3, new QTableWidgetItem("--"));
        }
        
        // 正行程误差(压力) - 所有轮次完成后计算
        if (hasForwardData && isAllRoundsCompleted()) {
            double expected = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
            double angleError = calculateAngleError(currentForwardAngle, expected);
            // 最终阶段：用平均最大角度换算压力误差
            const double fsAngle = calculateAverageMaxAngle();
            const double fsPressure = modelFullScalePressure(m_config);
            double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
            m_dataTable->setItem(i, 4, new QTableWidgetItem(QString::number(pressureError, 'f', 3)));
        } else {
            m_dataTable->setItem(i, 4, new QTableWidgetItem("--"));
        }
        
        // 反行程角度
        if (hasBackwardData) {
            m_dataTable->setItem(i, 5, new QTableWidgetItem(QString::number(currentBackwardAngle, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 5, new QTableWidgetItem("--"));
        }
        
        // 反行程角度误差 - 所有轮次完成后计算
        if (hasBackwardData && isAllRoundsCompleted()) {
            double expected = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
            double angleError = calculateAngleError(currentBackwardAngle, expected);
            m_dataTable->setItem(i, 6, new QTableWidgetItem(QString::number(angleError, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 6, new QTableWidgetItem("--"));
        }
        
        // 反行程误差(压力) - 所有轮次完成后计算
        if (hasBackwardData && isAllRoundsCompleted()) {
            double expected = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
            double angleError = calculateAngleError(currentBackwardAngle, expected);
            // 最终阶段：用平均最大角度换算压力误差
            const double fsAngle = calculateAverageMaxAngle();
            const double fsPressure = modelFullScalePressure(m_config);
            double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
            m_dataTable->setItem(i, 7, new QTableWidgetItem(QString::number(pressureError, 'f', 3)));
        } else {
            m_dataTable->setItem(i, 7, new QTableWidgetItem("--"));
        }
        
        // 迟滞误差角度 - 所有轮次完成后计算
        if (isAllRoundsCompleted()) {
            double hysteresisAngle = calculateHysteresisAngle(i);
            m_dataTable->setItem(i, 8, new QTableWidgetItem(QString::number(hysteresisAngle, 'f', 2)));
        } else {
            m_dataTable->setItem(i, 8, new QTableWidgetItem("--"));
        }
        
        // 迟滞误差(压力) - 固定值
        double fixedHysteresisError = getFixedHysteresisError(i);
        m_dataTable->setItem(i, 9, new QTableWidgetItem(QString::number(fixedHysteresisError, 'f', 3)));
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
    if (isAllRoundsCompleted()) {
        setFinalData();
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
    // 修正：角度->压力换算使用"动态满量程角度"
    // 预检阶段：使用当前轮 m_maxAngles[m_currentRound]
    // 最终阶段：使用所有轮的最大角度平均值 calculateAverageMaxAngle()
    const double fsPressure = modelFullScalePressure(m_config);
    double fsAngle = 0.0;
    if (isAllRoundsCompleted()) {
        fsAngle = calculateAverageMaxAngle();
    } else {
        if (m_currentRound >= 0 && m_currentRound < m_maxAngles.size())
            fsAngle = m_maxAngles[m_currentRound];
    }
    return angleToPressureByFS(angleError, fsAngle, fsPressure);
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
        // YYQY-13默认6个检测点 (0, 1, 2, 3, 4, 5 MPa)
        m_config.detectionPoints << 0.0 << 1.0 << 2.0 << 3.0 << 4.0 << 5.0;
        m_config.maxPressure = 6.3;
        m_config.maxAngle = 270.0;
        m_config.basicErrorLimit = 0.1;
        m_config.hysteresisErrorLimit = 0.3;   // 按型号：YYQY=0.3
        m_maxMeasurementsPerRound = 6;  // YYQY每轮6次测量
    } else if (dialType == "BYQ-19") {
        m_config.productModel = "BYQ-19";
        m_config.productName = "标准压力表";
        m_config.detectionPoints.clear();
        // BYQ-19默认5个检测点 (0, 6, 10, 21, 30 MPa)
        m_config.detectionPoints << 0.0 << 6.0 << 10.0 << 21.0 << 25.0;
        m_config.maxPressure = 25.0;
        m_config.maxAngle = 270.0;
        m_config.basicErrorLimit = 0.075;  // 更严格的误差限值
        m_config.hysteresisErrorLimit = 2.0;   // 按型号：BYQ=2.0
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
    
    // 重新计算最终角度并更新数据表格
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


void ErrorTableDialog::clearAllData()
{
    // 清空所有检测点数据
    for (DetectionPoint &point : m_detectionData) {
        // 清空向后兼容的数据
        point.hasForward = false;
        point.hasBackward = false;
        point.forwardAngle = 0.0;
        point.backwardAngle = 0.0;
        
        // 清空多轮次数据
        for (MeasurementData &roundData : point.roundData) {
            roundData.forwardAngles.fill(0.0);
            roundData.backwardAngles.fill(0.0);
            roundData.maxAngle = 0.0;
            roundData.isCompleted = false;
        }
    }
    
    // 清空最大角度数据
    m_maxAngles.clear();
    m_maxAngles.resize(m_totalRounds);  // 重新分配动态轮数
    
    // 重置当前轮次
    m_currentRound = 0;
    
    updateDataTable();
    updateAnalysisText();
    updateCurrentRoundDisplay();
    updateRoundInfoDisplay();
    
    qDebug() << "误差表格数据已清空";
}

// 删除未使用的计算错误函数
// void ErrorTableDialog::calculateErrors()
// {
//     updateDataTable();
//     updateAnalysisText();
// }

void ErrorTableDialog::validateAndCheckErrors()
{
    updateAnalysisText();
}

void ErrorTableDialog::updateAnalysisText()
{
    QString analysis = formatAnalysisResult();
    m_analysisText->setHtml(analysis);
}


//important
QString ErrorTableDialog::formatAnalysisResult()
{
    QString result = "<h3>误差检测结果</h3>";
    
    // 基本配置信息和轮次信息
    result += QString("<p><b>型号:</b> %1 &nbsp;&nbsp; <b>限值:</b> 基本±%2MPa 迟滞%3MPa</p>")
              .arg(m_config.productModel)
              .arg(m_config.basicErrorLimit, 0, 'f', 3)
              .arg(m_config.hysteresisErrorLimit, 0, 'f', 3);
              
    result += QString("<p><b>当前轮次:</b> 第%1轮/共%2轮 &nbsp;&nbsp; <b>每轮测量:</b> %3次正反行程")
              .arg(m_currentRound + 1)
              .arg(m_totalRounds)
              .arg(m_maxMeasurementsPerRound);
              
    // 最大角度信息
    double avgMaxAngle = calculateAverageMaxAngle();
    if (avgMaxAngle > 0) {
        result += QString(" &nbsp;&nbsp; <b>平均最大角度:</b> %1°").arg(avgMaxAngle, 0, 'f', 2);
    }
    result += "</p>";
    
    bool allPointsValid = true;       // 仅在最终阶段用于综合结论
    bool precheckAllOk = true;        // 预检阶段综合结论
    int validPointsCount = 0;
    
    // 显示每轮每个检测点的详细误差信息
    result += "<hr><h4>详细误差分析</h4>";
    
    // 遍历所有轮次
    for (int round = 0; round < m_totalRounds; ++round) {
        bool roundHasData = false;
        
        // 检查当前轮次是否有数据
        for (const DetectionPoint &point : m_detectionData) {
            if (round < point.roundData.size()) {
                const MeasurementData &roundData = point.roundData[round];
                for (double angle : roundData.forwardAngles) {
                    if (angle != 0.0) { roundHasData = true; break; }
                }
                if (!roundHasData) {
                    for (double angle : roundData.backwardAngles) {
                        if (angle != 0.0) { roundHasData = true; break; }
                    }
                }
            }
            if (roundHasData) break;
        }
        
        if (!roundHasData) continue; // 跳过没有数据的轮次
        
        result += QString("<h5>第%1轮误差分析</h5>").arg(round + 1);
        
        // 遍历所有检测点
        const bool completedAll = isAllRoundsCompleted();
        // 若未完成全部轮次，且该轮已采过最大角度，则计算本轮"预检迟滞阈值角度"
        double precheckThreshDeg = 0.0;
        if (!completedAll && round < m_maxAngles.size() && m_maxAngles[round] > 0.0) {
            precheckThreshDeg = precheckHysteresisAngleDeg(m_config, m_maxAngles[round]);
            result += QString("<p style='color:#888;'>（预检）本轮迟滞阈值角度 ≈ %1°</p>")
                      .arg(precheckThreshDeg, 0, 'f', 2);
        }

        for (int i = 0; i < m_detectionData.size(); ++i) {
            const DetectionPoint &point = m_detectionData[i];
            // "最终角度"：该点跨轮已完成"正+反"的对数平均（若不存在则回退到理论角度）
            double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
            double expectedAngle = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
            
            if (round < point.roundData.size()) {
                const MeasurementData &roundData = point.roundData[round];
                // 预检：如正反首个有效角度差超过阈值，则立即提示
                if (!completedAll && precheckThreshDeg > 0.0) {
                    double fwd=0.0, bwd=0.0;
                    if (firstValidAnglesOfRound(roundData, fwd, bwd)) {
                        double diffDeg = std::abs(fwd - bwd);
                        if (diffDeg > precheckThreshDeg) {
                            result += QString("<p><span style='color:red;font-weight:bold;'>（预检）第%1轮 %2 MPa 正/反差 %3° &gt; 阈值 %4° [超标]</span></p>")
                                      .arg(round + 1).arg(point.pressure, 0, 'f', 1).arg(diffDeg, 0, 'f', 2).arg(precheckThreshDeg, 0, 'f', 2);
                            precheckAllOk = false;
                        }
                    }
                }
                
                // 检查正行程数据
                for (int measurement = 0; measurement < roundData.forwardAngles.size(); ++measurement) {
                    double angle = roundData.forwardAngles[measurement];
                    if (angle != 0.0) {
                        if (completedAll) {
                            // 最终阶段：计算MPa，并按基本误差限值判断
                            double angleError = calculateAngleError(angle, expectedAngle);
                            double fsAngle = calculateAverageMaxAngle();
                            const double fsPressure = modelFullScalePressure(m_config);
                            double pressureError = std::abs(angleToPressureByFS(angleError, fsAngle, fsPressure));
                            const bool over = (pressureError > m_config.basicErrorLimit);
                            result += QString("<p><b>%1 MPa 第%2轮第%3次正行程:</b> 角度 %4° → 误差 %5 MPa")
                                      .arg(point.pressure, 0, 'f', 1).arg(round + 1).arg(measurement + 1)
                                      .arg(angle, 0, 'f', 2).arg(pressureError, 0, 'f', 3);
                            result += over ? QString(" <span style='color: red; font-weight: bold;'>[超标]</span>")
                                           : QString(" <span style='color: green;'>[合格]</span>");
                            result += "</p>";
                            if (over) allPointsValid = false;
                        } else {
                            // 预检阶段：仅按角度偏差与"预检阈值角度"比较，避免虚高的MPa
                            if (precheckThreshDeg > 0.0) {
                                double diffDeg = std::abs(angle - expectedAngle);
                                bool over = (diffDeg > precheckThreshDeg);
                                result += QString("<p><b>%1 MPa 第%2轮第%3次正行程（预检）:</b> 角度 %4° → 偏差 %5° / 阈值 %6°")
                                          .arg(point.pressure, 0, 'f', 1).arg(round + 1).arg(measurement + 1)
                                          .arg(angle, 0, 'f', 2).arg(diffDeg, 0, 'f', 2).arg(precheckThreshDeg, 0, 'f', 2);
                                result += over ? QString(" <span style='color: red; font-weight: bold;'>[超标]</span>")
                                               : QString(" <span style='color: green;'>[合格]</span>");
                                result += "</p>";
                                if (over) precheckAllOk = false;
                            }
                        }
                        validPointsCount++;
                    }
                }
                
                // 检查反行程数据
                for (int measurement = 0; measurement < roundData.backwardAngles.size(); ++measurement) {
                    double angle = roundData.backwardAngles[measurement];
                    if (angle != 0.0) {
                        if (completedAll) {
                            double angleError = calculateAngleError(angle, expectedAngle);
                            double fsAngle = calculateAverageMaxAngle();
                            const double fsPressure = modelFullScalePressure(m_config);
                            double pressureError = std::abs(angleToPressureByFS(angleError, fsAngle, fsPressure));
                            const bool over = (pressureError > m_config.basicErrorLimit);
                            result += QString("<p><b>%1 MPa 第%2轮第%3次反行程:</b> 角度 %4° → 误差 %5 MPa")
                                      .arg(point.pressure, 0, 'f', 1).arg(round + 1).arg(measurement + 1)
                                      .arg(angle, 0, 'f', 2).arg(pressureError, 0, 'f', 3);
                            result += over ? QString(" <span style='color: red; font-weight: bold;'>[超标]</span>")
                                           : QString(" <span style='color: green;'>[合格]</span>");
                            result += "</p>";
                            if (over) allPointsValid = false;
                        } else {
                            if (precheckThreshDeg > 0.0) {
                                double diffDeg = std::abs(angle - expectedAngle);
                                bool over = (diffDeg > precheckThreshDeg);
                                result += QString("<p><b>%1 MPa 第%2轮第%3次反行程（预检）:</b> 角度 %4° → 偏差 %5° / 阈值 %6°")
                                          .arg(point.pressure, 0, 'f', 1).arg(round + 1).arg(measurement + 1)
                                          .arg(angle, 0, 'f', 2).arg(diffDeg, 0, 'f', 2).arg(precheckThreshDeg, 0, 'f', 2);
                                result += over ? QString(" <span style='color: red; font-weight: bold;'>[超标]</span>")
                                               : QString(" <span style='color: green;'>[合格]</span>");
                                result += "</p>";
                                if (over) precheckAllOk = false;
                            }
                        }
                        validPointsCount++;
                    }
                }
            }
        }
        
        if (round < m_maxAngles.size() && m_maxAngles[round] > 0) {
            
            for (int i = 0; i < m_detectionData.size(); ++i) {
                const DetectionPoint &point = m_detectionData[i];
                if (round < point.roundData.size()) {
                    const MeasurementData &roundData = point.roundData[round];
                    
                    // 找到正行程和反行程的有效数据
                    double forwardAngle = 0.0, backwardAngle = 0.0;
                    bool hasForward = false, hasBackward = false;
                    
                    for (double angle : roundData.forwardAngles) {
                        if (angle != 0.0) { forwardAngle = angle; hasForward = true; break; }
                    }
                    for (double angle : roundData.backwardAngles) {
                        if (angle != 0.0) { backwardAngle = angle; hasBackward = true; break; }
                    }
                    
                    if (hasForward && hasBackward) {
                        double fixedHysteresisError = getFixedHysteresisError(i);
                        
                        if (fixedHysteresisError > m_config.hysteresisErrorLimit) {
                            result += QString(" <span style='color: red; font-weight: bold;'>[超标]</span>");
                            allPointsValid = false;
                        } else {
                            // 如果是"预检阶段"，此处只给出固定值提示，不改变预检逻辑
                            if (!completedAll) { /* 保持现有输出格式 */ }
                        }
                    }
                }
            }
            result += "</p>";
        }
        
        result += "<hr>";
    }
    
    // 总结
    if (!isAllRoundsCompleted()) {
        // 预检阶段总结
        if (validPointsCount == 0) {
            result += "<p style='color: orange; font-size: 16px; font-weight: bold;'>⚠ 暂无测量数据</p>";
        } else if (precheckAllOk) {
            result += "<p style='color: #2E86C1; font-size: 16px; font-weight: bold;'>✓ 预检通过</p>";
            result += QString("<p>已预检 %1 个数据点，未发现超过预检阈值</p>").arg(validPointsCount);
        } else {
            result += "<p style='color: red; font-size: 16px; font-weight: bold;'>✗ 预检发现超差</p>";
            result += "<p style='color: red;'>请关注标红项目，可继续完善余下轮次以复核</p>";
        }
    } else if (validPointsCount == 0) {
        result += "<p style='color: orange; font-size: 16px; font-weight: bold;'>⚠ 暂无测量数据</p>";
    } else if (allPointsValid) {
        result += "<p style='color: green; font-size: 16px; font-weight: bold;'>✓ 检测合格</p>";
        result += QString("<p>已检测 %1 个数据点，均符合误差要求</p>").arg(validPointsCount);
    } else {
        result += "<p style='color: red; font-size: 16px; font-weight: bold;'>✗ 检测不合格</p>";
        result += "<p style='color: red;'>请调整超标检测点的刻度盘角度</p>";
    }
    
    return result;
}

void ErrorTableDialog::exportToExcel()
{
    // 在导出之前，先更新配置信息，确保最新的输入值被保存
    updateConfigFromUI();
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出CSV文件", "", "CSV文件 (*.csv)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }
    
    file.write("\xEF\xBB\xBF");
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入产品信息 - 按照新格式
    out << QString("产品型号：,%1,,产品名称：,%2,,,\n").arg(m_config.productModel, m_config.productName);
    out << QString("刻度盘图号：,%1,,支组编号：,%2,,,\n").arg(m_config.dialDrawingNo, m_config.groupNo);
    out << "\n"; // 空行
    
    // 收集所有检测点数据（只显示一次）
    QStringList detectionPoints, theoreticalAngles;
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        // 检测点
        detectionPoints << QString::number(point.pressure, 'f', 1);
        // "检测点对应的刻度盘角度" = 实测最终角度（若无则以理论角度代替）
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
        if (finalAngle > 0.0) {
            theoreticalAngles << QString::number(finalAngle, 'f', 2);
        } else {
            theoreticalAngles << QString::number(pressureToAngle(point.pressure), 'f', 2);
        }
    }
    
    // 写入检测点和最终角度（只显示一次）
    out << QString("检测点,%1,,,\n").arg(detectionPoints.join(","));
    out << QString("检测点对应的刻度盘角度,%1,,,\n").arg(theoreticalAngles.join(","));
    
    // 为每轮数据写入（不显示轮次标题）
    for (int round = 0; round < m_totalRounds; ++round) {
        // 收集当前轮次的所有检测点数据
        QStringList forwardAngles, forwardAngleErrors, forwardPressureErrors;
        QStringList backwardAngles, backwardAngleErrors, backwardPressureErrors;
        
        for (int i = 0; i < m_detectionData.size(); ++i) {
            const DetectionPoint &point = m_detectionData[i];
            // "最终角度"：该点跨轮已完成"正+反"的对数平均（若不存在则回退到理论角度）
            double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
            double expectedAngle = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
            
            // 获取当前轮次的数据
            double currentForwardAngle = 0.0;
            double currentBackwardAngle = 0.0;
            bool hasForwardData = false;
            bool hasBackwardData = false;
            
            if (round < point.roundData.size()) {
                const MeasurementData &currentRoundData = point.roundData[round];
                
                // 获取正行程数据（取第一个非零值）
                for (double angle : currentRoundData.forwardAngles) {
                    if (angle != 0.0) {
                        currentForwardAngle = angle;
                        hasForwardData = true;
                        break;
                    }
                }
                
                // 获取反行程数据（取第一个非零值）
                for (double angle : currentRoundData.backwardAngles) {
                    if (angle != 0.0) {
                        currentBackwardAngle = angle;
                        hasBackwardData = true;
                        break;
                    }
                }
            }
            
            // 正行程数据
            if (hasForwardData) {
                forwardAngles << QString::number(currentForwardAngle, 'f', 2);
                double angleError = calculateAngleError(currentForwardAngle, expectedAngle);
                forwardAngleErrors << QString::number(angleError, 'f', 2);
                // 按轮换算：预检用该轮最大角度，最终用平均最大角度
                double fsAngle = isAllRoundsCompleted()
                                   ? calculateAverageMaxAngle()
                                   : ((round < m_maxAngles.size()) ? m_maxAngles[round] : 0.0);
                const double fsPressure = modelFullScalePressure(m_config);
                double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
                forwardPressureErrors << QString::number(pressureError, 'f', 3);
            } else {
                forwardAngles << "--";
                forwardAngleErrors << "--";
                forwardPressureErrors << "--";
            }
            
            // 反行程数据
            if (hasBackwardData) {
                backwardAngles << QString::number(currentBackwardAngle, 'f', 2);
                double angleError = calculateAngleError(currentBackwardAngle, expectedAngle);
                backwardAngleErrors << QString::number(angleError, 'f', 2);
                // 按轮换算：预检用该轮最大角度，最终用平均最大角度
                double fsAngle = isAllRoundsCompleted()
                                   ? calculateAverageMaxAngle()
                                   : ((round < m_maxAngles.size()) ? m_maxAngles[round] : 0.0);
                const double fsPressure = modelFullScalePressure(m_config);
                double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
                backwardPressureErrors << QString::number(pressureError, 'f', 3);
            } else {
                backwardAngles << "--";
                backwardAngleErrors << "--";
                backwardPressureErrors << "--";
            }
        }
        
        // 按照垂直格式写入当前轮次的数据
        out << QString("正行程角度,%1,,,\n").arg(forwardAngles.join(","));
        out << QString("正行程角度误差,%1,,,\n").arg(forwardAngleErrors.join(","));
        out << QString("正行程误差（MPa）,%1,,,\n").arg(forwardPressureErrors.join(","));
        out << "\n";
        out << QString("反行程角度,%1,,,\n").arg(backwardAngles.join(","));
        out << QString("反行程角度误差,%1,,,\n").arg(backwardAngleErrors.join(","));
        out << QString("反行程误差（MPa）,%1,,,\n").arg(backwardPressureErrors.join(","));
        out << "\n";
    }
    
    // 计算并显示迟滞误差（只显示一次，在最后两行）
    QStringList hysteresisAngles, hysteresisPressureErrors;
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &point = m_detectionData[i];
        // "最终角度"：该点跨轮已完成"正+反"的对数平均（若不存在则回退到理论角度）
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
        double expectedAngle = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
        
        // 计算所有轮次的平均迟滞误差
        double totalAngleDiff = 0.0;
        int validRoundCount = 0;
        
        for (int round = 0; round < point.roundData.size() && round < m_totalRounds; ++round) {
            const MeasurementData &currentRoundData = point.roundData[round];
            
            double currentForwardAngle = 0.0;
            double currentBackwardAngle = 0.0;
            bool hasForwardData = false;
            bool hasBackwardData = false;
            
            // 获取正行程数据
            for (double angle : currentRoundData.forwardAngles) {
                if (angle != 0.0) {
                    currentForwardAngle = angle;
                    hasForwardData = true;
                    break;
                }
            }
            
            // 获取反行程数据
            for (double angle : currentRoundData.backwardAngles) {
                if (angle != 0.0) {
                    currentBackwardAngle = angle;
                    hasBackwardData = true;
                    break;
                }
            }
            
            // 计算迟滞误差
            if (hasForwardData && hasBackwardData) {
                double angleDiff = std::abs(currentForwardAngle - currentBackwardAngle);
                totalAngleDiff += angleDiff;
                validRoundCount++;
            }
        }
        
        if (validRoundCount > 0) {
            double avgAngleDiff = totalAngleDiff / validRoundCount;
            hysteresisAngles << QString::number(avgAngleDiff, 'f', 2);
            // 使用固定迟滞误差值
            double fixedHysteresisError = getFixedHysteresisError(i);
            hysteresisPressureErrors << QString::number(fixedHysteresisError, 'f', 3);
        } else {
            hysteresisAngles << "--";
            hysteresisPressureErrors << "--";
        }
    }
    
    // === 修正：导出迟滞误差角度 = 该点迟滞误差(MPa) / 满量程压力 * 实测平均总角度 ===
    QStringList hysteresisAnglesFixed;
    double avgMaxAngle = calculateAverageMaxAngle(); // 实测平均总角度
    const double fsPressure = modelFullScalePressure(m_config);
    for (int i = 0; i < m_detectionData.size(); ++i) {
        if (avgMaxAngle > 0.0 && fsPressure > 0.0) {
            double angleDeg = (getFixedHysteresisError(i) / fsPressure) * avgMaxAngle;
            hysteresisAnglesFixed << QString::number(angleDeg, 'f', 2);
        } else {
            hysteresisAnglesFixed << "--";
        }
    }
    out << QString("迟滞误差角度,%1,,,\n").arg(hysteresisAnglesFixed.join(","));
    out << QString("迟滞误差（MPa）,%1,,,\n").arg(hysteresisPressureErrors.join(","));
    
    QMessageBox::information(this, "成功", QString("%1轮测量数据已导出到CSV文件，按照指定格式排列，可用Excel打开").arg(m_totalRounds));
}

QString ErrorTableDialog::generateExportData()
{
    // 在生成导出数据之前，先更新配置信息，确保最新的输入值被保存
    updateConfigFromUI();
    
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
        // "最终角度"：该点跨轮已完成"正+反"的对数平均（若不存在则回退到理论角度）
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
        double expectedAngle = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
        
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
            // 单行导出沿用动态满量程角度（预检用当前轮，最终用平均）
            double fsAngle = isAllRoundsCompleted()
                               ? calculateAverageMaxAngle()
                               : ((m_currentRound >=0 && m_currentRound < m_maxAngles.size()) ? m_maxAngles[m_currentRound] : 0.0);
            const double fsPressure = modelFullScalePressure(m_config);
            double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
            data += QString("\t%1\t\t%2").arg(angleError, 0, 'f', 2).arg(pressureError, 0, 'f', 3);
        } else {
            data += "\t--\t\t--";
        }
        
        if (point.hasBackward) {
            double angleError = calculateAngleError(point.backwardAngle, expectedAngle);
            // 同上
            double fsAngle = isAllRoundsCompleted()
                               ? calculateAverageMaxAngle()
                               : ((m_currentRound >=0 && m_currentRound < m_maxAngles.size()) ? m_maxAngles[m_currentRound] : 0.0);
            const double fsPressure = modelFullScalePressure(m_config);
            double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
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
                point.roundData.resize(m_totalRounds);  // 确保有足够的轮次数据
                
                for (int i = 0; i < roundDataArray.size() && i < m_totalRounds; ++i) {
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
        if (point.roundData.size() != m_totalRounds) {
            point.roundData.resize(m_totalRounds);
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
            m_isForwardDirection = true;
        } else if (column == 5) {  // 反行程角度列
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
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(row);
        double expected = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
        double angleError = calculateAngleError(point.forwardAngle, expected);
        // 预检 / 最终阶段动态换算
        double fsAngle = isAllRoundsCompleted()
                           ? calculateAverageMaxAngle()
                           : ((m_currentRound >=0 && m_currentRound < m_maxAngles.size()) ? m_maxAngles[m_currentRound] : 0.0);
        const double fsPressure = modelFullScalePressure(m_config);
        double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
        
        m_dataTable->item(row, 3)->setText(QString::number(angleError, 'f', 2));
        m_dataTable->item(row, 4)->setText(QString::number(pressureError, 'f', 3));
        
        // 更新迟滞误差（如果有反行程数据）
        if (point.hasBackward) {
            // 迟滞误差角度 - 所有轮次完成后计算
            if (isAllRoundsCompleted()) {
                double hysteresisAngle = calculateHysteresisAngle(row);
                m_dataTable->item(row, 8)->setText(QString::number(hysteresisAngle, 'f', 2));
            }
            
            // 迟滞误差(压力) - 固定值
            double fixedHysteresisError = getFixedHysteresisError(row);
            m_dataTable->setItem(row, 9, new QTableWidgetItem(QString::number(fixedHysteresisError, 'f', 3)));
        }
        
    } else if (column == 5) { // 反行程角度
        point.backwardAngle = value;
        point.hasBackward = true;
        
        // 只更新相关的计算列
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(row);
        double expected = (finalAngle > 0.0) ? finalAngle : pressureToAngle(point.pressure);
        double angleError = calculateAngleError(point.backwardAngle, expected);
        // 预检 / 最终阶段动态换算
        double fsAngle = isAllRoundsCompleted()
                           ? calculateAverageMaxAngle()
                           : ((m_currentRound >=0 && m_currentRound < m_maxAngles.size()) ? m_maxAngles[m_currentRound] : 0.0);
        const double fsPressure = modelFullScalePressure(m_config);
        double pressureError = angleToPressureByFS(angleError, fsAngle, fsPressure);
        
        m_dataTable->item(row, 6)->setText(QString::number(angleError, 'f', 2));
        m_dataTable->item(row, 7)->setText(QString::number(pressureError, 'f', 3));
        
        // 更新迟滞误差（如果有正行程数据）
        if (point.hasForward) {
            // 迟滞误差角度 - 所有轮次完成后计算
            if (isAllRoundsCompleted()) {
                double hysteresisAngle = calculateHysteresisAngle(row);
                m_dataTable->item(row, 8)->setText(QString::number(hysteresisAngle, 'f', 2));
            }
            
            // 迟滞误差(压力) - 固定值
            double fixedHysteresisError = getFixedHysteresisError(row);
            m_dataTable->setItem(row, 9, new QTableWidgetItem(QString::number(fixedHysteresisError, 'f', 3)));
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
        point.roundData.resize(m_totalRounds);  // 动态轮数
        
        // 为每轮初始化数据
        for (int round = 0; round < m_totalRounds; ++round) {
            point.roundData[round].forwardAngles.resize(m_maxMeasurementsPerRound);
            point.roundData[round].backwardAngles.resize(m_maxMeasurementsPerRound);
            point.roundData[round].maxAngle = 0.0;
        }
    }
    
    qDebug() << "轮次数据已初始化，最大测量次数:" << m_maxMeasurementsPerRound;
    // 注意：不在这里调用updateCurrentRoundDisplay()，因为UI可能还没有初始化
}

void ErrorTableDialog::setTotalRounds(int rounds)
{
    if (rounds < 1 || rounds > 10) {
        qDebug() << "轮数设置无效:" << rounds << "，使用默认值5";
        rounds = 5;
    }
    
    if (rounds != m_totalRounds) {
        qDebug() << "设置总轮数从" << m_totalRounds << "改为" << rounds;
        m_totalRounds = rounds;
        
        // 重新调整最大角度数组大小
        m_maxAngles.resize(m_totalRounds);
        
        // 重新调整所有检测点的轮次数据
        for (DetectionPoint &point : m_detectionData) {
            point.roundData.resize(m_totalRounds);
            
            // 为新增的轮次初始化数据
            for (int round = 0; round < m_totalRounds; ++round) {
                if (point.roundData[round].forwardAngles.size() != m_maxMeasurementsPerRound) {
                    point.roundData[round].forwardAngles.resize(m_maxMeasurementsPerRound);
                }
                if (point.roundData[round].backwardAngles.size() != m_maxMeasurementsPerRound) {
                    point.roundData[round].backwardAngles.resize(m_maxMeasurementsPerRound);
                }
            }
        }
        
        // 如果当前轮次超出新的总轮数，重置为0
        if (m_currentRound >= m_totalRounds) {
            m_currentRound = 0;
        }
        
        // 更新轮次选择下拉框
        if (m_roundCombo) {
            m_roundCombo->blockSignals(true);
            m_roundCombo->clear();
            for (int i = 1; i <= m_totalRounds; ++i) {
                m_roundCombo->addItem(QString("第%1轮").arg(i));
            }
            m_roundCombo->setCurrentIndex(m_currentRound);
            m_roundCombo->blockSignals(false);
        }
        
        // 更新界面显示
        updateDataTable();
        updateCurrentRoundDisplay();
        updateRoundInfoDisplay();
        


        qDebug() << "总轮数设置完成:" << m_totalRounds << "轮";
    }
}

int ErrorTableDialog::getTotalRounds() const
{
    return m_totalRounds;
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
    if (m_currentRound < m_totalRounds - 1) {
        m_currentRound++;
        updateCurrentRoundDisplay();
        QMessageBox::information(this, "保存", 
            QString("第%1轮数据已保存！\n准备开始第%2轮测量")
            .arg(m_currentRound)
            .arg(m_currentRound + 1));
    } else {
        QMessageBox::information(this, "保存", 
            QString("第%1轮数据已保存！\n所有%1轮测量完成")
            .arg(m_totalRounds));
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
        
        // 计算5轮最大角度的平均值并更新配置
        updateMaxAngleFromRounds();
        
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

// 计算所有轮次最大角度的平均值并更新配置
void ErrorTableDialog::updateMaxAngleFromRounds()
{
    double sum = 0.0;
    int count = 0;
    
    // 统计所有轮次的最大角度
    for (int i = 0; i < m_maxAngles.size(); ++i) {
        if (m_maxAngles[i] != 0.0) {
            sum += m_maxAngles[i];
            count++;
        }
    }
    
    if (count > 0) {
        double avgMaxAngle = sum / count;
        m_config.maxAngle = avgMaxAngle;
        qDebug() << "更新满量程角度为" << m_totalRounds << "轮平均值:" << avgMaxAngle << "度";
        
        // 删除未使用的满量程角度UI更新
        // if (m_maxAngleSpin) {
        //     m_maxAngleSpin->setValue(avgMaxAngle);
        // }
    }
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
            QString roundInfo = QString(" | 第%1轮/共%2轮").arg(m_currentRound + 1).arg(m_totalRounds);
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
            m_currentPointLabel->setText(QString("第%1轮测量/共%2轮").arg(m_currentRound + 1).arg(m_totalRounds));
        }
        
        // 更新分析文本中的轮次信息（如果分析文本组件已初始化）
        if (m_analysisText) {
            updateAnalysisText();
        }
        
        qDebug() << "当前轮次:" << (m_currentRound + 1) << "/" << m_totalRounds;
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
    for (int round = 1; round <= m_totalRounds; ++round) {
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
        
        // 遍历所有轮次数据
        for (int round = 0; round < m_totalRounds; ++round) {
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
    if (roundIndex >= 0 && roundIndex < m_totalRounds) {
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
    if (round >= 0 && round < m_totalRounds) {
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

// 批量设置主界面数据
void ErrorTableDialog::setMainWindowData(const QVector<QVector<double>>& allRoundsForward, 
                                        const QVector<QVector<double>>& allRoundsBackward, 
                                        const QVector<double>& allRoundsMaxAngles)
{
    qDebug() << "开始批量设置主界面数据到误差表格";
    
    // 确保有足够的检测点数据
    if (m_detectionData.isEmpty()) {
        qDebug() << "检测点数据为空，无法设置数据";
        return;
    }
    
    // 根据表盘类型确定检测点数量
    int detectionPointCount = m_detectionData.size();
    qDebug() << "检测点数量:" << detectionPointCount;
    
    // 为每个检测点设置对应的数据
    for (int pointIndex = 0; pointIndex < detectionPointCount; ++pointIndex) {
        DetectionPoint &point = m_detectionData[pointIndex];
        
        // 确保轮次数据结构存在
        if (point.roundData.size() != m_totalRounds) {
            point.roundData.resize(m_totalRounds);
        }
        
        // 设置所有轮次的数据
        for (int round = 0; round < m_totalRounds; ++round) {
            if (round < allRoundsForward.size() && round < allRoundsBackward.size()) {
                // 初始化轮次数据
                if (point.roundData[round].forwardAngles.size() != m_maxMeasurementsPerRound) {
                    point.roundData[round].forwardAngles.resize(m_maxMeasurementsPerRound);
                }
                if (point.roundData[round].backwardAngles.size() != m_maxMeasurementsPerRound) {
                    point.roundData[round].backwardAngles.resize(m_maxMeasurementsPerRound);
                }
                
                // 清空当前轮次数据
                point.roundData[round].forwardAngles.fill(0.0);
                point.roundData[round].backwardAngles.fill(0.0);
                
                // 根据检测点索引获取对应的数据
                // 检测点0对应采集数据1，检测点1对应采集数据2，以此类推
                if (pointIndex < allRoundsForward[round].size()) {
                    point.roundData[round].forwardAngles[0] = allRoundsForward[round][pointIndex];
                }
                
                if (pointIndex < allRoundsBackward[round].size()) {
                    point.roundData[round].backwardAngles[0] = allRoundsBackward[round][pointIndex];
                }
                
                // 设置最大角度（所有检测点共享同一个最大角度）
                if (round < allRoundsMaxAngles.size()) {
                    point.roundData[round].maxAngle = allRoundsMaxAngles[round];
                    if (round < m_maxAngles.size()) {
                        m_maxAngles[round] = allRoundsMaxAngles[round];
                    }
                }
                
                // 设置完成状态
                point.roundData[round].isCompleted = (allRoundsForward[round].size() > pointIndex || allRoundsBackward[round].size() > pointIndex);
                
                qDebug() << "检测点" << (pointIndex + 1) << "第" << (round + 1) << "轮: 正行程" 
                         << point.roundData[round].forwardAngles[0] << ", 反行程" << point.roundData[round].backwardAngles[0];
            }
        }
        
        // 更新显示的数据（向后兼容）
        if (!point.roundData.isEmpty() && m_currentRound < point.roundData.size()) {
            const MeasurementData &currentRoundData = point.roundData[m_currentRound];
            if (!currentRoundData.forwardAngles.isEmpty()) {
                point.forwardAngle = currentRoundData.forwardAngles[0];
                point.hasForward = (point.forwardAngle != 0.0);
            }
            if (!currentRoundData.backwardAngles.isEmpty()) {
                point.backwardAngle = currentRoundData.backwardAngles[0];
                point.hasBackward = (point.backwardAngle != 0.0);
            }
        }
    }
    
    // 更新界面显示
    updateDataTable();
    updateCurrentRoundDisplay();
    updateRoundInfoDisplay();
    
    // 更新误差分析结果
    validateAndCheckErrors();
    qDebug() << "批量数据设置完成";

    //setFinalData();

}

// ================== 新增的计算函数 ==================

// 计算指定检测点所有轮次正反行程角度的平均值 --需要使用
double ErrorTableDialog::calculateAverageAngleForDetectionPoint(int pointIndex) const
{
    if (pointIndex < 0 || pointIndex >= m_detectionData.size()) {
        return 0.0;
    }
    
    const DetectionPoint &point = m_detectionData[pointIndex];
    double totalAngle = 0.0;
    int validCount = 0;
    
    // 遍历所有轮次数据
    for (int round = 0; round < point.roundData.size(); ++round) {
        const MeasurementData &roundData = point.roundData[round];
        
        // 正行程数据
        for (double angle : roundData.forwardAngles) {
            if (angle != 0.0) {
                totalAngle += angle;
                validCount++;
            }
        }
        
        // 反行程数据
        for (double angle : roundData.backwardAngles) {
            if (angle != 0.0) {
                totalAngle += angle;
                validCount++;
            }
        }
    }
    
    return validCount > 0 ? totalAngle / validCount : 0.0;
}

// 新增：计算"最终角度"（该检测点跨轮已完成"正+反"成对数据后的平均）
double ErrorTableDialog::calculateFinalMeasuredAngleForDetectionPoint(int pointIndex) const
{
    if (pointIndex < 0 || pointIndex >= m_detectionData.size()) return 0.0;
    const DetectionPoint &point = m_detectionData[pointIndex];
    double totalPairMean = 0.0;
    int pairCount = 0;

    for (int round = 0; round < point.roundData.size(); ++round) {
        const MeasurementData &rd = point.roundData[round];
        double fwd = 0.0, bwd = 0.0;
        bool hasF = false, hasB = false;
        for (double a : rd.forwardAngles) { if (a != 0.0) { fwd = a; hasF = true; break; } }
        for (double a : rd.backwardAngles){ if (a != 0.0) { bwd = a; hasB = true; break; } }
        if (hasF && hasB) {
            totalPairMean += 0.5 * (fwd + bwd);
            pairCount++;
        }
    }
    return (pairCount > 0) ? (totalPairMean / pairCount) : 0.0;
}

// 检查是否所有轮次都已完成
bool ErrorTableDialog::isAllRoundsCompleted() const
{
    // 检查是否有至少一轮的最大角度数据
    bool hasMaxAngleData = false;
    for (double maxAngle : m_maxAngles) {
        if (maxAngle != 0.0) {
            hasMaxAngleData = true;
            break;
        }
    }
    
    if (!hasMaxAngleData) {
        return false;
    }
    
    // 检查所有检测点是否都有数据
    for (const DetectionPoint &point : m_detectionData) {
        for (int round = 0; round < point.roundData.size(); ++round) {
            const MeasurementData &roundData = point.roundData[round];
            
            // 检查是否有正行程或反行程数据
            bool hasData = false;
            for (double angle : roundData.forwardAngles) {
                if (angle != 0.0) {
                    hasData = true;
                    break;
                }
            }
            if (!hasData) {
                for (double angle : roundData.backwardAngles) {
                    if (angle != 0.0) {
                        hasData = true;
                        break;
                    }
                }
            }
            
            if (!hasData) {
                return false;
            }
        }
    }
    
    return true;
}

// 获取固定迟滞误差值
double ErrorTableDialog::getFixedHysteresisError(int pointIndex) const
{
    if (m_config.productModel == "YYQY-13") {
        // YYQY表盘：第一个检测点0.1，其他都是0.3
        if (pointIndex == 0) {
            return 0.1;
        } else {
            return 0.3;
        }
    } else if (m_config.productModel == "BYQ-19") {
        // BYQ表盘：根据检测点返回固定值
        switch (pointIndex) {
            case 0: return 0.5;  // 检测点0
            case 1: return 1.0;  // 检测点6
            case 2: return 2.0;  // 检测点10
            case 3: return 1.0;  // 检测点21
            case 4: return 2.0;  // 检测点25
            default: return 0.0;
        }
    }
    return 0.0;
}

// 计算迟滞误差角度
double ErrorTableDialog::calculateHysteresisAngle(int pointIndex) const
{
    double fixedHysteresisError = getFixedHysteresisError(pointIndex);
    const double fsPressure = modelFullScalePressure(m_config);
    const double avgMaxDeg  = calculateAverageMaxAngle(); // 实测平均总角度
    if (fixedHysteresisError <= 0.0 || fsPressure <= 0.0 || avgMaxDeg <= 0.0) return 0.0;
    // 统一公式：迟滞误差角度 = 迟滞误差(MPa) / 满量程压力 * 实测平均总角度
    return (fixedHysteresisError / fsPressure) * avgMaxDeg;
}

// 实时更新产品信息配置
void ErrorTableDialog::onProductInfoChanged()
{
    // 当用户修改产品信息时，实时更新配置
    updateConfigFromUI();
    if (m_config.productModel == "YYQY-13") {
        if (m_config.hysteresisErrorLimit != 0.3) {
            m_config.hysteresisErrorLimit = 0.3;
            if (m_hysteresisErrorLimitSpin) m_hysteresisErrorLimitSpin->setValue(0.3);
        }
    } else if (m_config.productModel == "BYQ-19") {
        if (m_config.hysteresisErrorLimit != 2.0) {
            m_config.hysteresisErrorLimit = 2.0;
            if (m_hysteresisErrorLimitSpin) m_hysteresisErrorLimitSpin->setValue(2.0);
        }
    }
    validateAndCheckErrors();  // 确保分析区即时刷新
}

//设置最终数据
// void ErrorTableDialog::setFinalData() {
    
//         if (m_config.productModel == "YYQY-13") m_yyqyFinalData = buildYYQYFinalData();
//         if (m_config.productModel == "BYQ-19") m_byqFinalData = buildBYQFinalData();
    
// }

void ErrorTableDialog::setFinalData() {
    // 1. 检查是否完成所有轮次
    if (!isAllRoundsCompleted()) {
        qDebug() << "未完成所有轮次测量，跳过设置最终数据";
        return;
    }

    // 2. 检查是否有有效的平均最大角度
    double avgMaxAngle = calculateAverageMaxAngle();
    if (avgMaxAngle <= 0.0) {
        qDebug() << "无有效的平均最大角度，跳过设置最终数据";
        return;
    }

    // 3. 检查是否每个检测点都有有效的最终角度
    for (int i = 0; i < m_detectionData.size(); i++) {
        double finalAngle = calculateFinalMeasuredAngleForDetectionPoint(i);
        if(i == 0 && finalAngle >= 3.0)  return;
        if (finalAngle <= 0.0) {
            qDebug() << "检测点" << i << "无有效最终角度，跳过设置最终数据";
            return;
        }
    }

    // 4. 检查产品型号是否有效
    if (m_config.productModel != "YYQY-13" && m_config.productModel != "BYQ-19") {
        qDebug() << "无效的产品型号:" << m_config.productModel;
        return;
    }

    // 5. 设置最终数据
    if (m_config.productModel == "YYQY-13") {
        m_yyqyFinalData = buildYYQYFinalData();
        qDebug() << "YYQY最终数据设置完成 - 压力点数量:" << m_yyqyFinalData.points.size()
                 << "角度数量:" << m_yyqyFinalData.pointsAngle.size();
    }
    if (m_config.productModel == "BYQ-19") {
        m_byqFinalData = buildBYQFinalData();
        qDebug() << "BYQ最终数据设置完成 - 压力点数量:" << m_byqFinalData.points.size()
                 << "角度数量:" << m_byqFinalData.pointsAngle.size();
    }
}

// 构造 BYQ_final_data：收集当前检测点（按 m_detectionData 顺序）并使用实测平均最大角度
BYQ_final_data ErrorTableDialog::buildBYQFinalData() const
{
    BYQ_final_data out;
    out.maxPressure = modelFullScalePressure(m_config);        // 使用型号映射的满量程压力
    out.totalAngle  = calculateAverageMaxAngle();              // 使用所有可用轮次的平均最大角度

    out.points.clear();
    out.pointsAngle.clear();

    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &pt = m_detectionData[i];
        out.points.append(pt.pressure);
        // 最终角度按现有逻辑计算（跨轮正反成对平均）
        double finalAng = calculateFinalMeasuredAngleForDetectionPoint(i);
        if (finalAng <= 0.0) {
            // 回退：使用理论角度（以配置的满量程角为基准）
            finalAng = pressureToAngle(pt.pressure);
        }
        out.pointsAngle.append(finalAng);
    }

    return out;
}

// 构造 YYQY_final_data：与 BYQ 类似，按 YYQY 需要的检测点顺序/数量返回
YYQY_final_data ErrorTableDialog::buildYYQYFinalData() const
{
    YYQY_final_data out;
    out.maxPressure = modelFullScalePressure(m_config);
    out.totalAngle  = calculateAverageMaxAngle();

    out.points.clear();
    out.pointsAngle.clear();

    // 如果是 YYQY 型号，则直接按 m_detectionData 顺序输出
    // 否则也按现有检测点列表输出，调用者可根据型号选择使用哪个函数
    for (int i = 0; i < m_detectionData.size(); ++i) {
        const DetectionPoint &pt = m_detectionData[i];
        out.points.append(pt.pressure);
        double finalAng = calculateFinalMeasuredAngleForDetectionPoint(i);
        if (finalAng <= 0.0) {
            finalAng = pressureToAngle(pt.pressure);
        }
        out.pointsAngle.append(finalAng);
    }
    
    qDebug() << "YYQY points:" << out.points;
    qDebug() << "YYQY pointsAngle:" << out.pointsAngle;

    return out;
}

