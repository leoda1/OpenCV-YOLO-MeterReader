#include "dialmarkdialog.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QInputDialog>

// AnnotationLabel 实现
AnnotationLabel::AnnotationLabel(QWidget *parent)
    : QLabel(parent)
    , m_currentColor(Qt::red)
    , m_currentFontSize(12)
    , m_currentFontFamily("SimHei")
    , m_currentBold(false)
    , m_currentItalic(false)
    , m_annotationMode(true)
    , m_dragMode(false)
    , m_selectedAnnotation(-1)
    , m_isDragging(false)
    , m_rubberBand(nullptr)
{
    setMinimumSize(400, 300);
    setAlignment(Qt::AlignCenter);
    setStyleSheet("border: 1px solid gray;");
    setCursor(Qt::CrossCursor);
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void AnnotationLabel::setImage(const QPixmap &pixmap)
{
    m_originalPixmap = pixmap;
    updateDisplay();
}

void AnnotationLabel::addTextAnnotation(const QPoint &pos, const QString &text, const QColor &color, int fontSize, const QString &fontFamily, bool isBold, bool isItalic)
{
    TextAnnotation annotation;
    annotation.position = pos;
    annotation.text = text;
    annotation.color = color;
    annotation.fontSize = fontSize;
    annotation.fontFamily = fontFamily;
    annotation.isBold = isBold;
    annotation.isItalic = isItalic;
    annotation.isSelected = false;
    
    updateBoundingRect(annotation);
    
    m_annotations.append(annotation);
    updateDisplay();
}

void AnnotationLabel::clearAnnotations()
{
    m_annotations.clear();
    m_selectedAnnotation = -1;
    updateDisplay();
}

void AnnotationLabel::removeSelectedAnnotation()
{
    if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
        m_annotations.removeAt(m_selectedAnnotation);
        m_selectedAnnotation = -1;
        updateDisplay();
    }
}

void AnnotationLabel::updateSelectedAnnotation(const QString &text, const QColor &color, int fontSize, const QString &fontFamily, bool isBold, bool isItalic)
{
    if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
        TextAnnotation &annotation = m_annotations[m_selectedAnnotation];
        annotation.text = text;
        annotation.color = color;
        annotation.fontSize = fontSize;
        annotation.fontFamily = fontFamily;
        annotation.isBold = isBold;
        annotation.isItalic = isItalic;
        
        updateBoundingRect(annotation);
        updateDisplay();
    }
}

// 添加新的工具函数
void AnnotationLabel::updateBoundingRect(TextAnnotation &annotation)
{
    QFont font(annotation.fontFamily);
    font.setPointSize(annotation.fontSize);
    font.setBold(annotation.isBold);
    font.setItalic(annotation.isItalic);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(annotation.text);
    // 添加边距确保文本完全可见
    annotation.boundingRect = QRect(annotation.position.x() - 2, 
                                   annotation.position.y() - textRect.height() - 2, 
                                   textRect.width() + 4, textRect.height() + 4);
}

// 添加拖拽矩形标注功能  
void AnnotationLabel::addRectAnnotation(const QRect &rect, const QString &text, const QColor &color, int fontSize, const QString &fontFamily, bool isBold, bool isItalic)
{
    // 在矩形中心添加文本标注
    QPoint center = rect.center();
    addTextAnnotation(center, text, color, fontSize, fontFamily, isBold, isItalic);
}

// 获取带标注的图片
QPixmap AnnotationLabel::getAnnotatedImage() const
{
    if (m_originalPixmap.isNull()) return QPixmap();
    
    QPixmap result = m_originalPixmap;
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制所有标注
    for (const TextAnnotation &annotation : m_annotations) {
        QFont font(annotation.fontFamily);
        font.setPointSize(annotation.fontSize);
        font.setBold(annotation.isBold);
        font.setItalic(annotation.isItalic);
        painter.setFont(font);
        painter.setPen(annotation.color);
        painter.drawText(annotation.position, annotation.text);
    }
    
    return result;
}

void AnnotationLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);
    
    if (m_originalPixmap.isNull()) return;
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制所有标注
    for (int i = 0; i < m_annotations.size(); ++i) {
        const TextAnnotation &annotation = m_annotations[i];
        
        QFont font(annotation.fontFamily);
        font.setPointSize(annotation.fontSize);
        font.setBold(annotation.isBold);
        font.setItalic(annotation.isItalic);
        painter.setFont(font);
        painter.setPen(annotation.color);
        
        // 如果是选中的标注，添加背景
        if (i == m_selectedAnnotation) {
            painter.fillRect(annotation.boundingRect, QColor(255, 255, 0, 100));
        }
        
        painter.drawText(annotation.position, annotation.text);
    }
    
    // 绘制拖拽框
    if (m_isDragging && m_dragMode) {
        QPen pen(Qt::red, 2, Qt::DashLine);
        painter.setPen(pen);
        QRect dragRect(m_dragStartPoint, m_dragEndPoint);
        painter.drawRect(dragRect.normalized());
    }
}

void AnnotationLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint clickPos = event->pos();
        
        if (m_dragMode) {
            // 拖拽模式：开始拖拽
            m_isDragging = true;
            m_dragStartPoint = clickPos;
            m_dragEndPoint = clickPos;
        } else if (m_annotationMode) {
            // 点击模式：检查是否点击了现有标注
            int annotationIndex = findAnnotationAt(clickPos);
            if (annotationIndex >= 0) {
                // 取消之前的选择
                if (m_selectedAnnotation >= 0) {
                    m_annotations[m_selectedAnnotation].isSelected = false;
                }
                
                m_selectedAnnotation = annotationIndex;
                m_annotations[annotationIndex].isSelected = true;
                updateDisplay();
                emit annotationClicked(annotationIndex);
            } else {
                // 点击空白区域，弹出输入对话框添加新标注
                if (m_selectedAnnotation >= 0) {
                    m_annotations[m_selectedAnnotation].isSelected = false;
                    m_selectedAnnotation = -1;
                    updateDisplay();
                }
                
                QString text = showTextInputDialog();
                if (!text.isEmpty()) {
                    addTextAnnotation(clickPos, text, m_currentColor, 
                                    m_currentFontSize, m_currentFontFamily, 
                                    m_currentBold, m_currentItalic);
                    // 通知主窗口更新列表
                    emit annotationAdded(clickPos);
                }
            }
        }
    }
    
    QLabel::mousePressEvent(event);
}

void AnnotationLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && m_dragMode) {
        m_dragEndPoint = event->pos();
        update(); // 重绘以显示拖拽框
    }
    
    QLabel::mouseMoveEvent(event);
}

void AnnotationLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDragging && m_dragMode) {
        m_isDragging = false;
        
        QRect dragRect(m_dragStartPoint, m_dragEndPoint);
        dragRect = dragRect.normalized();
        
        // 只有当拖拽区域足够大时才创建标注
        if (dragRect.width() > 10 && dragRect.height() > 10) {
            // 弹出文本输入对话框
            QString text = showTextInputDialog();
            if (!text.isEmpty()) {
                addTextAnnotation(dragRect.center(), text, m_currentColor, 
                                m_currentFontSize, m_currentFontFamily, 
                                m_currentBold, m_currentItalic);
                // 通知主窗口更新列表
                emit rectAnnotationAdded(dragRect);
            }
        }
        
        update(); // 清除拖拽框
    }
    
    QLabel::mouseReleaseEvent(event);
}

// 添加文本输入对话框
QString AnnotationLabel::showTextInputDialog(const QString &currentText)
{
    bool ok;
    QString text = QInputDialog::getText(this, "输入标注文本", 
                                        "请输入文本内容:", QLineEdit::Normal,
                                        currentText, &ok);
    return ok ? text : QString();
}

// 添加右键菜单
void AnnotationLabel::contextMenuEvent(QContextMenuEvent *event)
{
    int annotationIndex = findAnnotationAt(event->pos());
    if (annotationIndex >= 0) {
        emit annotationRightClicked(annotationIndex, event->globalPos());
    }
    
    QLabel::contextMenuEvent(event);
}

// 添加删除指定索引标注的功能
void AnnotationLabel::removeAnnotationAt(int index)
{
    if (index >= 0 && index < m_annotations.size()) {
        m_annotations.removeAt(index);
        if (m_selectedAnnotation == index) {
            m_selectedAnnotation = -1;
        } else if (m_selectedAnnotation > index) {
            m_selectedAnnotation--;
        }
        updateDisplay();
    }
}

void AnnotationLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int annotationIndex = findAnnotationAt(event->pos());
        if (annotationIndex >= 0) {
            // 双击编辑标注
            bool ok;
            QString newText = QInputDialog::getText(this, "编辑标注", 
                                                   "文本:", QLineEdit::Normal,
                                                   m_annotations[annotationIndex].text, &ok);
            if (ok && !newText.isEmpty()) {
                m_annotations[annotationIndex].text = newText;
                
                // 重新计算边界框
                updateBoundingRect(m_annotations[annotationIndex]);
                
                updateDisplay();
            }
        }
    }
    
    QLabel::mouseDoubleClickEvent(event);
}

void AnnotationLabel::updateDisplay()
{
    if (!m_originalPixmap.isNull()) {
        setPixmap(m_originalPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    update();
}

int AnnotationLabel::findAnnotationAt(const QPoint &pos)
{
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        if (m_annotations[i].boundingRect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

// DialMarkDialog 实现
DialMarkDialog::DialMarkDialog(QWidget *parent)
    : QDialog(parent)
    , ui(nullptr)
    , m_currentColor(Qt::red)
{
    setWindowTitle("表盘标注工具");
    setMinimumSize(950, 650);  // 增加最小尺寸以适应更宽的控制面板
    resize(1100, 750);  // 增加默认窗口大小
    
    setupUI();
    setupToolbar();
    loadDialImageFromFile();
}

DialMarkDialog::~DialMarkDialog()
{
    if (ui) delete ui;
}

void DialMarkDialog::setupUI()
{
    // 主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    // 左侧：图片显示区域
    m_imageLabel = new AnnotationLabel(this);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMinimumSize(600, 400);
    
    // 右侧：控制面板 - 增加宽度以防止文字被遮挡
    QWidget *controlPanel = new QWidget(this);
    controlPanel->setFixedWidth(300);
    controlPanel->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
    
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setSpacing(15);  // 增加组件间距
    controlLayout->setContentsMargins(15, 15, 15, 15);  // 增加边距
    
    // 设置控制面板的全局字体
    QFont panelFont;
    panelFont.setPointSize(10);
    controlPanel->setFont(panelFont);
    
    // 文本输入组
    QGroupBox *textGroup = new QGroupBox("文本标注", controlPanel);
    textGroup->setFont(panelFont);
    QVBoxLayout *textLayout = new QVBoxLayout(textGroup);
    textLayout->setSpacing(8);
    textLayout->setContentsMargins(12, 12, 12, 12);
    
    m_textEdit = new QLineEdit(textGroup);
    m_textEdit->setPlaceholderText("请输入标注文本...");
    m_textEdit->setFont(panelFont);
    m_textEdit->setMinimumHeight(28);
    textLayout->addWidget(m_textEdit);
    
    // 颜色选择
    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->setSpacing(8);
    QLabel *colorLabel = new QLabel("颜色:");
    colorLabel->setFont(panelFont);
    colorLabel->setMinimumWidth(70);
    colorLayout->addWidget(colorLabel);
    m_colorButton = new QPushButton("选择颜色", textGroup);
    m_colorButton->setFixedHeight(32);
    m_colorButton->setFont(panelFont);
    updateColorButton();
    colorLayout->addWidget(m_colorButton);
    textLayout->addLayout(colorLayout);
    
    // 字体选择
    QHBoxLayout *fontFamilyLayout = new QHBoxLayout();
    fontFamilyLayout->setSpacing(8);
    QLabel *fontFamilyLabel = new QLabel("字体:");
    fontFamilyLabel->setFont(panelFont);
    fontFamilyLabel->setMinimumWidth(70);
    fontFamilyLayout->addWidget(fontFamilyLabel);
    m_fontComboBox = new QFontComboBox(textGroup);
    m_fontComboBox->setFont(panelFont);
    m_fontComboBox->setMinimumHeight(28);
    m_fontComboBox->setCurrentText("SimHei"); // 默认黑体
    fontFamilyLayout->addWidget(m_fontComboBox);
    textLayout->addLayout(fontFamilyLayout);
    
    // 字体大小
    QHBoxLayout *fontSizeLayout = new QHBoxLayout();
    fontSizeLayout->setSpacing(8);
    QLabel *fontSizeLabel = new QLabel("字体大小:");
    fontSizeLabel->setFont(panelFont);
    fontSizeLabel->setMinimumWidth(70);
    fontSizeLayout->addWidget(fontSizeLabel);
    m_fontSizeSpinBox = new QSpinBox(textGroup);
    m_fontSizeSpinBox->setRange(8, 48);
    m_fontSizeSpinBox->setValue(12);
    m_fontSizeSpinBox->setFont(panelFont);
    m_fontSizeSpinBox->setMinimumHeight(28);
    fontSizeLayout->addWidget(m_fontSizeSpinBox);
    textLayout->addLayout(fontSizeLayout);
    
    // 字体样式
    QHBoxLayout *styleLayout = new QHBoxLayout();
    styleLayout->setSpacing(8);
    m_boldCheckBox = new QCheckBox("加粗", textGroup);
    m_boldCheckBox->setFont(panelFont);
    m_italicCheckBox = new QCheckBox("斜体", textGroup);
    m_italicCheckBox->setFont(panelFont);
    styleLayout->addWidget(m_boldCheckBox);
    styleLayout->addWidget(m_italicCheckBox);
    textLayout->addLayout(styleLayout);
    
    controlLayout->addWidget(textGroup);
    
    // 标注列表组
    QGroupBox *listGroup = new QGroupBox("标注列表", controlPanel);
    listGroup->setFont(panelFont);
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);
    listLayout->setSpacing(8);
    listLayout->setContentsMargins(12, 12, 12, 12);
    
    m_annotationList = new QListWidget(listGroup);
    m_annotationList->setMaximumHeight(160);
    m_annotationList->setFont(panelFont);
    listLayout->addWidget(m_annotationList);
    
    controlLayout->addWidget(listGroup);
    
    // 操作按钮组
    QGroupBox *buttonGroup = new QGroupBox("操作", controlPanel);
    buttonGroup->setFont(panelFont);
    QVBoxLayout *buttonLayout = new QVBoxLayout(buttonGroup);
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(12, 12, 12, 12);
    
    m_dragModeButton = new QPushButton("拖拽模式", buttonGroup);
    m_removeButton = new QPushButton("删除选中标注", buttonGroup);
    m_clearButton = new QPushButton("清除所有标注", buttonGroup);
    m_saveButton = new QPushButton("保存标注", buttonGroup);
    m_loadButton = new QPushButton("加载标注", buttonGroup);
    m_exportButton = new QPushButton("导出图片", buttonGroup);
    
    // 设置按钮样式和字体
    QList<QPushButton*> buttons = {m_dragModeButton, m_removeButton, m_clearButton, m_saveButton, m_loadButton, m_exportButton};
    for (QPushButton* button : buttons) {
        button->setFont(panelFont);
        button->setMinimumHeight(32);
        button->setStyleSheet("QPushButton { padding: 5px 10px; border: 1px solid #ccc; border-radius: 3px; } QPushButton:hover { background-color: #e0e0e0; }");
    }
    
    // 拖拽模式按钮可以切换状态
    m_dragModeButton->setCheckable(true);
    m_dragModeButton->setStyleSheet("QPushButton { padding: 5px 10px; border: 1px solid #ccc; border-radius: 3px; } QPushButton:hover { background-color: #e0e0e0; } QPushButton:checked { background-color: #007acc; color: white; }");
    
    buttonLayout->addWidget(m_dragModeButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_exportButton);
    
    controlLayout->addWidget(buttonGroup);
    
    controlLayout->addStretch();
    
    // 添加到主布局
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(controlPanel, 0);
    
    // 连接信号槽
    connect(m_colorButton, &QPushButton::clicked, this, &DialMarkDialog::chooseColor);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &DialMarkDialog::onFontSizeChanged);
    connect(m_fontComboBox, &QFontComboBox::currentTextChanged,
            this, &DialMarkDialog::onFontFamilyChanged);
    connect(m_boldCheckBox, &QCheckBox::toggled, this, &DialMarkDialog::onBoldChanged);
    connect(m_italicCheckBox, &QCheckBox::toggled, this, &DialMarkDialog::onItalicChanged);
    connect(m_imageLabel, &AnnotationLabel::annotationClicked, 
            this, &DialMarkDialog::onAnnotationClicked);
    connect(m_imageLabel, &AnnotationLabel::annotationAdded, 
            this, &DialMarkDialog::onAnnotationAdded);
    connect(m_imageLabel, &AnnotationLabel::rectAnnotationAdded,
            this, &DialMarkDialog::onRectAnnotationAdded);
    connect(m_imageLabel, &AnnotationLabel::annotationRightClicked,
            this, &DialMarkDialog::onAnnotationRightClicked);
    connect(m_dragModeButton, &QPushButton::toggled, this, &DialMarkDialog::toggleDragMode);
    connect(m_removeButton, &QPushButton::clicked, this, &DialMarkDialog::removeSelectedAnnotation);
    connect(m_clearButton, &QPushButton::clicked, this, &DialMarkDialog::clearAllAnnotations);
    connect(m_saveButton, &QPushButton::clicked, this, &DialMarkDialog::saveAnnotations);
    connect(m_loadButton, &QPushButton::clicked, this, &DialMarkDialog::loadAnnotations);
    connect(m_exportButton, &QPushButton::clicked, this, &DialMarkDialog::exportImage);
    connect(m_textEdit, &QLineEdit::textChanged, this, &DialMarkDialog::onTextChanged);
    connect(m_annotationList, &QListWidget::currentRowChanged, 
            this, &DialMarkDialog::onAnnotationClicked);
    connect(m_annotationList, &QListWidget::itemDoubleClicked,
            [this](QListWidgetItem *item) {
                int index = m_annotationList->row(item);
                showAnnotationProperties(index);
            });
}

void DialMarkDialog::setupToolbar()
{
    // 可以在这里添加更多工具栏功能
}

void DialMarkDialog::loadDialImageFromFile()
{
    QString imagePath = QApplication::applicationDirPath() + "/images/70-01.tif";
    
    // 首先尝试从应用程序目录加载
    if (!QFile::exists(imagePath)) {
        // 如果应用程序目录没有，尝试相对路径
        imagePath = "images/70-01.tif";
    }
    
    if (!QFile::exists(imagePath)) {
        // 如果还是找不到，让用户选择文件
        imagePath = QFileDialog::getOpenFileName(this,
            "选择表盘图片", "", "图片文件 (*.png *.jpg *.jpeg *.tif *.tiff *.bmp)");
    }
    
    if (!imagePath.isEmpty()) {
        QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            m_imageLabel->setImage(pixmap);
            qDebug() << "成功加载表盘图片:" << imagePath;
        } else {
            QMessageBox::warning(this, "错误", "无法加载图片文件: " + imagePath);
        }
    } else {
        QMessageBox::information(this, "提示", "未选择图片文件");
    }
}

void DialMarkDialog::chooseColor()
{
    QColor color = QColorDialog::getColor(m_currentColor, this, "选择文本颜色");
    if (color.isValid()) {
        m_currentColor = color;
        m_imageLabel->setCurrentColor(color);
        updateColorButton();
    }
}

void DialMarkDialog::updateColorButton()
{
    QString styleSheet = QString("QPushButton { background-color: %1; color: %2; border: 1px solid #999; padding: 5px 8px; font-weight: bold; }")
                        .arg(m_currentColor.name())
                        .arg(m_currentColor.lightness() > 128 ? "black" : "white");
    m_colorButton->setStyleSheet(styleSheet);
    m_colorButton->setText(m_currentColor.name());
    m_colorButton->setMinimumWidth(120);  // 确保有足够宽度显示颜色名称
}

void DialMarkDialog::onFontSizeChanged(int size)
{
    m_imageLabel->setCurrentFontSize(size);
}

void DialMarkDialog::onFontFamilyChanged(const QString &family)
{
    m_imageLabel->setCurrentFontFamily(family);
}

void DialMarkDialog::onBoldChanged(bool bold)
{
    m_imageLabel->setCurrentBold(bold);
}

void DialMarkDialog::onItalicChanged(bool italic)
{
    m_imageLabel->setCurrentItalic(italic);
}

void DialMarkDialog::onAnnotationClicked(int index)
{
    if (index >= 0) {
        const QList<TextAnnotation>& annotations = m_imageLabel->getAnnotations();
        if (index < annotations.size()) {
            const TextAnnotation& annotation = annotations[index];
            m_textEdit->setText(annotation.text);
            m_currentColor = annotation.color;
            m_fontSizeSpinBox->setValue(annotation.fontSize);
            m_fontComboBox->setCurrentText(annotation.fontFamily);
            m_boldCheckBox->setChecked(annotation.isBold);
            m_italicCheckBox->setChecked(annotation.isItalic);
            updateColorButton();
            
            // 更新列表选择
            m_annotationList->setCurrentRow(index);
        }
    }
}

void DialMarkDialog::onAnnotationAdded(const QPoint &pos)
{
    // 这个函数现在已经不使用了，标注添加直接在AnnotationLabel中处理
    updateAnnotationList();
}

void DialMarkDialog::onRectAnnotationAdded(const QRect &rect)
{
    // 这个函数现在已经不使用了，拖拽标注添加直接在AnnotationLabel中处理
    updateAnnotationList();
}

void DialMarkDialog::onAnnotationRightClicked(int index, const QPoint &globalPos)
{
    showAnnotationProperties(index);
}

void DialMarkDialog::removeAnnotation()
{
    int currentRow = m_annotationList->currentRow();
    if (currentRow >= 0) {
        m_imageLabel->removeAnnotationAt(currentRow);
        updateAnnotationList();
    }
}

void DialMarkDialog::removeSelectedAnnotation()
{
    m_imageLabel->removeSelectedAnnotation();
    updateAnnotationList();
}

void DialMarkDialog::showAnnotationProperties(int index)
{
    const QList<TextAnnotation>& annotations = m_imageLabel->getAnnotations();
    if (index >= 0 && index < annotations.size()) {
        AnnotationPropertiesDialog dialog(annotations[index], this);
        if (dialog.exec() == QDialog::Accepted) {
            TextAnnotation newAnnotation = dialog.getAnnotation();
            // 保持位置和选择状态
            newAnnotation.position = annotations[index].position;
            newAnnotation.isSelected = annotations[index].isSelected;
            
            // 更新标注
            m_imageLabel->updateSelectedAnnotation(newAnnotation.text, newAnnotation.color, 
                                                  newAnnotation.fontSize, newAnnotation.fontFamily,
                                                  newAnnotation.isBold, newAnnotation.isItalic);
            updateAnnotationList();
            
            // 同步UI控件
            m_textEdit->setText(newAnnotation.text);
            m_currentColor = newAnnotation.color;
            m_fontSizeSpinBox->setValue(newAnnotation.fontSize);
            m_fontComboBox->setCurrentText(newAnnotation.fontFamily);
            m_boldCheckBox->setChecked(newAnnotation.isBold);
            m_italicCheckBox->setChecked(newAnnotation.isItalic);
            updateColorButton();
        }
    }
}

void DialMarkDialog::clearAllAnnotations()
{
    int ret = QMessageBox::question(this, "确认", "确定要清除所有标注吗？",
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_imageLabel->clearAnnotations();
        updateAnnotationList();
    }
}

void DialMarkDialog::saveAnnotations()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "保存标注文件", "dial_annotations.json", "JSON 文件 (*.json)");
    
    if (!fileName.isEmpty()) {
        QJsonObject jsonObj;
        QJsonArray annotationsArray;
        
        const QList<TextAnnotation>& annotations = m_imageLabel->getAnnotations();
        for (const TextAnnotation& annotation : annotations) {
            QJsonObject annotationObj;
            annotationObj["x"] = annotation.position.x();
            annotationObj["y"] = annotation.position.y();
            annotationObj["text"] = annotation.text;
            annotationObj["color"] = annotation.color.name();
            annotationObj["fontSize"] = annotation.fontSize;
            annotationObj["fontFamily"] = annotation.fontFamily;
            annotationObj["isBold"] = annotation.isBold;
            annotationObj["isItalic"] = annotation.isItalic;
            annotationsArray.append(annotationObj);
        }
        
        jsonObj["annotations"] = annotationsArray;
        
        QJsonDocument doc(jsonObj);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            QMessageBox::information(this, "成功", "标注已保存到: " + fileName);
        } else {
            QMessageBox::warning(this, "错误", "无法保存文件: " + fileName);
        }
    }
}

void DialMarkDialog::loadAnnotations()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "加载标注文件", "", "JSON 文件 (*.json)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject jsonObj = doc.object();
            
            m_imageLabel->clearAnnotations();
            
            QJsonArray annotationsArray = jsonObj["annotations"].toArray();
            for (const QJsonValue& value : annotationsArray) {
                QJsonObject annotationObj = value.toObject();
                QPoint pos(annotationObj["x"].toInt(), annotationObj["y"].toInt());
                QString text = annotationObj["text"].toString();
                QColor color(annotationObj["color"].toString());
                int fontSize = annotationObj["fontSize"].toInt();
                QString fontFamily = annotationObj["fontFamily"].toString();
                bool isBold = annotationObj["isBold"].toBool();
                bool isItalic = annotationObj["isItalic"].toBool();
                if (fontFamily.isEmpty()) fontFamily = "SimHei"; // 默认黑体
                
                m_imageLabel->addTextAnnotation(pos, text, color, fontSize, fontFamily, isBold, isItalic);
            }
            
            updateAnnotationList();
            QMessageBox::information(this, "成功", "标注已加载: " + fileName);
        } else {
            QMessageBox::warning(this, "错误", "无法读取文件: " + fileName);
        }
    }
}

void DialMarkDialog::onTextChanged()
{
    // 当文本框内容改变时，如果有选中的标注，更新它
    const QList<TextAnnotation>& annotations = m_imageLabel->getAnnotations();
    int selected = m_annotationList->currentRow();
    if (selected >= 0 && selected < annotations.size()) {
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, 
                                              m_fontSizeSpinBox->value(), m_fontComboBox->currentText(),
                                              m_boldCheckBox->isChecked(), m_italicCheckBox->isChecked());
        updateAnnotationList();
    }
}

void DialMarkDialog::exportImage()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出标注图片", "annotated_dial.png", 
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff)");
    
    if (!fileName.isEmpty()) {
        QPixmap annotatedImage = m_imageLabel->getAnnotatedImage();
        if (!annotatedImage.isNull()) {
            if (annotatedImage.save(fileName)) {
                QMessageBox::information(this, "成功", "图片已导出到: " + fileName);
            } else {
                QMessageBox::warning(this, "错误", "导出图片失败: " + fileName);
            }
        } else {
            QMessageBox::warning(this, "错误", "没有可导出的图片");
        }
    }
}

void DialMarkDialog::toggleDragMode()
{
    bool dragMode = m_dragModeButton->isChecked();
    m_imageLabel->setDragMode(dragMode);
    m_imageLabel->setAnnotationMode(!dragMode);
    
    // 更新按钮文本
    if (dragMode) {
        m_dragModeButton->setText("点击模式");
        m_imageLabel->setCursor(Qt::CrossCursor);
    } else {
        m_dragModeButton->setText("拖拽模式");
        m_imageLabel->setCursor(Qt::CrossCursor);
    }
}

void DialMarkDialog::updateAnnotationList()
{
    m_annotationList->clear();
    const QList<TextAnnotation>& annotations = m_imageLabel->getAnnotations();
    
    for (int i = 0; i < annotations.size(); ++i) {
        const TextAnnotation& annotation = annotations[i];
        QString itemText = QString("%1: %2").arg(i + 1).arg(annotation.text);
        m_annotationList->addItem(itemText);
    }
}

void DialMarkDialog::loadDialImage()
{
    loadDialImageFromFile();
}

// AnnotationPropertiesDialog 实现
AnnotationPropertiesDialog::AnnotationPropertiesDialog(const TextAnnotation &annotation, QWidget *parent)
    : QDialog(parent), m_currentColor(annotation.color)
{
    setWindowTitle("标注属性");
    setFixedSize(320, 280);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 文本输入
    mainLayout->addWidget(new QLabel("文本内容:"));
    m_textEdit = new QLineEdit(annotation.text);
    mainLayout->addWidget(m_textEdit);
    
    // 字体选择
    mainLayout->addWidget(new QLabel("字体:"));
    m_fontComboBox = new QFontComboBox();
    m_fontComboBox->setCurrentText(annotation.fontFamily);
    mainLayout->addWidget(m_fontComboBox);
    
    // 字体大小
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel("字体大小:"));
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 48);
    m_fontSizeSpinBox->setValue(annotation.fontSize);
    sizeLayout->addWidget(m_fontSizeSpinBox);
    mainLayout->addLayout(sizeLayout);
    
    // 颜色选择
    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->addWidget(new QLabel("颜色:"));
    m_colorButton = new QPushButton();
    m_colorButton->setFixedHeight(30);
    updateColorButton();
    colorLayout->addWidget(m_colorButton);
    mainLayout->addLayout(colorLayout);
    
    // 样式选项
    m_boldCheckBox = new QCheckBox("加粗");
    m_boldCheckBox->setChecked(annotation.isBold);
    mainLayout->addWidget(m_boldCheckBox);
    
    m_italicCheckBox = new QCheckBox("斜体");
    m_italicCheckBox->setChecked(annotation.isItalic);
    mainLayout->addWidget(m_italicCheckBox);
    
    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定");
    QPushButton *cancelButton = new QPushButton("取消");
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_colorButton, &QPushButton::clicked, this, &AnnotationPropertiesDialog::chooseColor);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

TextAnnotation AnnotationPropertiesDialog::getAnnotation() const
{
    TextAnnotation annotation;
    annotation.text = m_textEdit->text();
    annotation.color = m_currentColor;
    annotation.fontSize = m_fontSizeSpinBox->value();
    annotation.fontFamily = m_fontComboBox->currentText();
    annotation.isBold = m_boldCheckBox->isChecked();
    annotation.isItalic = m_italicCheckBox->isChecked();
    return annotation;
}

void AnnotationPropertiesDialog::updateColorButton()
{
    QString styleSheet = QString("QPushButton { background-color: %1; color: %2; border: 1px solid #999; }")
                        .arg(m_currentColor.name())
                        .arg(m_currentColor.lightness() > 128 ? "black" : "white");
    m_colorButton->setStyleSheet(styleSheet);
    m_colorButton->setText(m_currentColor.name());
}

void AnnotationPropertiesDialog::chooseColor()
{
    QColor color = QColorDialog::getColor(m_currentColor, this, "选择文本颜色");
    if (color.isValid()) {
        m_currentColor = color;
        updateColorButton();
    }
} 