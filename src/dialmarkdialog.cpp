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
#include <QVector>
#include <QSet>
#include <QImageWriter>
#include <iostream>
#include <cmath>



AnnotationLabel::AnnotationLabel(QWidget *parent)
    : QLabel(parent)
    , m_currentColor(Qt::red)
    , m_currentFontSize(12)
    , m_currentFontFamily("黑体")
    , m_currentBold(false)
    , m_currentItalic(false)
    , m_selectedAnnotation(-1)
    , m_isDraggingAnnotation(false)
    , m_scaleFactor(1.0)
    , m_imageOffset(0, 0)
    , m_isDraggingImage(false)
{
    setMinimumSize(400, 300);
    setAlignment(Qt::AlignCenter);
    setFocusPolicy(Qt::StrongFocus);  // 允许接收键盘焦点
    setAttribute(Qt::WA_AcceptTouchEvents, true);  // 支持触摸事件（包括手势）
    setStyleSheet("border: 1px solid gray;");
    setCursor(Qt::ArrowCursor);
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void AnnotationLabel::setImage(const QPixmap &pixmap)
{
    m_originalPixmap = pixmap;
    // 重置缩放和偏移
    m_scaleFactor = 1.0;
    m_imageOffset = QPoint(0, 0);
    
    // 如果图片太大，自动缩小到合适尺寸
    if (!pixmap.isNull()) {
        QSize labelSize = size();
        QSize pixmapSize = pixmap.size();
        
        if (pixmapSize.width() > labelSize.width() || pixmapSize.height() > labelSize.height()) {
            double scaleX = (double)labelSize.width() / pixmapSize.width();
            double scaleY = (double)labelSize.height() / pixmapSize.height();
            m_scaleFactor = qMin(scaleX, scaleY) * 0.9; // 留一些边距
        }
    }
    
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
        int scaledFontSize = qMax(1, (int)(annotation.fontSize / m_scaleFactor));
        font.setPointSize(scaledFontSize);
        font.setBold(annotation.isBold);
        font.setItalic(annotation.isItalic);
        painter.setFont(font);
        painter.setPen(annotation.color);
        
        // 标注位置是相对于原始图片的坐标，直接使用
        painter.drawText(annotation.position, annotation.text);
    }
    
    return result;
}

void AnnotationLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    if (m_originalPixmap.isNull()) return;
    
    // 计算图片显示位置
    QSize scaledSize(m_originalPixmap.width() * m_scaleFactor, 
                    m_originalPixmap.height() * m_scaleFactor);
    QRect imageRect;
    imageRect.setSize(scaledSize);
    
    // 居中显示并应用偏移
    imageRect.moveCenter(rect().center() + m_imageOffset);
    
    // 绘制图片
    QPixmap scaledPixmap = m_originalPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap(imageRect, scaledPixmap);
    
    // 绘制所有标注
    for (int i = 0; i < m_annotations.size(); ++i) {
        const TextAnnotation &annotation = m_annotations[i];
        
        QFont font(annotation.fontFamily);
        font.setPointSize(annotation.fontSize);
        font.setBold(annotation.isBold);
        font.setItalic(annotation.isItalic);
        painter.setFont(font);
        painter.setPen(annotation.color);
        
        // 标注位置是相对于原始图片的坐标，需要映射到当前显示位置
        QPoint displayPos = imageRect.topLeft() + QPoint(
            annotation.position.x() * m_scaleFactor,
            annotation.position.y() * m_scaleFactor
        );
        
        // 如果是选中的标注，添加背景
        if (i == m_selectedAnnotation) {
            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect(annotation.text);
            textRect.moveTopLeft(displayPos - QPoint(0, textRect.height()));
            painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(255, 255, 0, 100));
        }
        
        painter.drawText(displayPos, annotation.text);
    }
}

void AnnotationLabel::mousePressEvent(QMouseEvent *event)
{
    // 确保获得键盘焦点以接收快捷键
    setFocus();
    
    if (event->button() == Qt::LeftButton) {
        QPoint clickPos = event->pos();
        
        // 计算图片显示区域
        QSize scaledSize(m_originalPixmap.width() * m_scaleFactor, 
                        m_originalPixmap.height() * m_scaleFactor);
        QRect imageRect;
        imageRect.setSize(scaledSize);
        imageRect.moveCenter(rect().center() + m_imageOffset);
        
        // 检查是否点击了现有标注
        int annotationIndex = findAnnotationAt(clickPos, imageRect);
        if (annotationIndex >= 0) {
            // 取消之前的选择
            if (m_selectedAnnotation >= 0) {
                m_annotations[m_selectedAnnotation].isSelected = false;
            }
            
            m_selectedAnnotation = annotationIndex;
            m_annotations[annotationIndex].isSelected = true;
            
            // 准备拖动标注
            m_isDraggingAnnotation = true;
            m_lastMousePos = clickPos;
            
            updateDisplay();
            emit annotationClicked(annotationIndex);
        } else if (imageRect.contains(clickPos)) {
            // 点击在图片上，准备拖动图片
            m_isDraggingImage = true;
            m_lastPanPoint = clickPos;
            setCursor(Qt::ClosedHandCursor);
            
            // 取消标注选择
            if (m_selectedAnnotation >= 0) {
                m_annotations[m_selectedAnnotation].isSelected = false;
                m_selectedAnnotation = -1;
                updateDisplay();
            }
        } else {
            // 点击空白区域，取消选择
            if (m_selectedAnnotation >= 0) {
                m_annotations[m_selectedAnnotation].isSelected = false;
                m_selectedAnnotation = -1;
                updateDisplay();
            }
        }
    }
    
    QLabel::mousePressEvent(event);
}

void AnnotationLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDraggingAnnotation && m_selectedAnnotation >= 0) {
        // 拖动选中的标注
        QPoint delta = event->pos() - m_lastMousePos;
        // 将像素位移转换为原始图片坐标的位移
        QPoint deltaInOriginal = QPoint(delta.x() / m_scaleFactor, delta.y() / m_scaleFactor);
        m_annotations[m_selectedAnnotation].position += deltaInOriginal;
        updateBoundingRect(m_annotations[m_selectedAnnotation]);
        m_lastMousePos = event->pos();
        updateDisplay();
    } else if (m_isDraggingImage) {
        // 拖动图片
        QPoint delta = event->pos() - m_lastPanPoint;
        m_imageOffset += delta;
        m_lastPanPoint = event->pos();
        updateDisplay();
    }
    
    QLabel::mouseMoveEvent(event);
}

void AnnotationLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_isDraggingAnnotation) {
            m_isDraggingAnnotation = false;
        }
        if (m_isDraggingImage) {
            m_isDraggingImage = false;
            setCursor(Qt::ArrowCursor);
        }
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
    QPoint clickPos = event->pos();
    
    // 计算图片显示区域
    QSize scaledSize(m_originalPixmap.width() * m_scaleFactor, 
                    m_originalPixmap.height() * m_scaleFactor);
    QRect imageRect;
    imageRect.setSize(scaledSize);
    imageRect.moveCenter(rect().center() + m_imageOffset);
    
    int annotationIndex = findAnnotationAt(clickPos, imageRect);
    
    QMenu menu(this);
    
    if (annotationIndex >= 0) {
        // 右键点击已有标注
        QAction *editAction = menu.addAction("编辑标注");
        QAction *deleteAction = menu.addAction("删除标注");
        
        QAction *selectedAction = menu.exec(event->globalPos());
        
        if (selectedAction == editAction) {
            emit annotationRightClicked(annotationIndex, event->globalPos());
        } else if (selectedAction == deleteAction) {
            removeAnnotationAt(annotationIndex);
            emit annotationAdded(clickPos); // 通知更新列表
        }
    } else if (imageRect.contains(clickPos)) {
        // 右键点击图片区域
        QAction *createAction = menu.addAction("创建新标注");
        
        QAction *selectedAction = menu.exec(event->globalPos());
        
        if (selectedAction == createAction) {
            QString text = showTextInputDialog();
            if (!text.isEmpty()) {
                // 将显示坐标转换为原始图片坐标
                QPoint imagePos = QPoint(
                    (clickPos.x() - imageRect.left()) / m_scaleFactor,
                    (clickPos.y() - imageRect.top()) / m_scaleFactor
                );
                addTextAnnotation(imagePos, text, m_currentColor, 
                                m_currentFontSize, m_currentFontFamily, 
                                m_currentBold, m_currentItalic);
                // 通知主窗口更新列表
                emit annotationAdded(clickPos);
            }
        }
    }
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
        // 计算图片显示区域
        QSize scaledSize(m_originalPixmap.width() * m_scaleFactor, 
                        m_originalPixmap.height() * m_scaleFactor);
        QRect imageRect;
        imageRect.setSize(scaledSize);
        imageRect.moveCenter(rect().center() + m_imageOffset);
        
        int annotationIndex = findAnnotationAt(event->pos(), imageRect);
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
        QSize scaledSize(m_originalPixmap.width() * m_scaleFactor, 
                        m_originalPixmap.height() * m_scaleFactor);
        setPixmap(m_originalPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    update();
}

int AnnotationLabel::findAnnotationAt(const QPoint &pos, const QRect &imageRect)
{
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        // 计算标注在当前缩放和偏移下的显示位置
        QPoint displayPos = imageRect.topLeft() + QPoint(
            m_annotations[i].position.x() * m_scaleFactor,
            m_annotations[i].position.y() * m_scaleFactor
        );
        
        // 计算文本边界框
        QFont font(m_annotations[i].fontFamily);
        font.setPointSize(m_annotations[i].fontSize);
        font.setBold(m_annotations[i].isBold);
        font.setItalic(m_annotations[i].isItalic);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(m_annotations[i].text);
        textRect.moveTopLeft(displayPos - QPoint(0, textRect.height()));
        textRect = textRect.adjusted(-2, -2, 2, 2);
        
        if (textRect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

void AnnotationLabel::setSelectedAnnotation(int index)
{
    // 取消之前的选择
    if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
        m_annotations[m_selectedAnnotation].isSelected = false;
    }
    
    // 设置新的选择
    if (index >= 0 && index < m_annotations.size()) {
        m_selectedAnnotation = index;
        m_annotations[index].isSelected = true;
    } else {
        m_selectedAnnotation = -1;
    }
    
    updateDisplay();
}

// 添加鼠标滚轮缩放功能
void AnnotationLabel::wheelEvent(QWheelEvent *event)
{
    if (m_originalPixmap.isNull()) return;
    
    // 获取滚轮方向
    int delta = event->angleDelta().y();
    double scaleFactor = 1.15;
    
    if (delta > 0) {
        // 放大
        m_scaleFactor *= scaleFactor;
    } else {
        // 缩小
        m_scaleFactor /= scaleFactor;
    }
    
    // 限制缩放范围
    m_scaleFactor = qMax(0.1, qMin(5.0, m_scaleFactor));
    
    updateDisplay();
    event->accept();
}

void AnnotationLabel::keyPressEvent(QKeyEvent *event)
{
    if (m_originalPixmap.isNull()) {
        QLabel::keyPressEvent(event);
        return;
    }
    
    // 处理缩放快捷键
    if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) {
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
            zoomIn();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Minus) {
            zoomOut();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_0) {
            resetZoom();
            event->accept();
            return;
        }
    }
    
    QLabel::keyPressEvent(event);
}

void AnnotationLabel::zoomIn()
{
    if (m_originalPixmap.isNull()) return;
    
    double scaleFactor = 1.15;
    m_scaleFactor *= scaleFactor;
    m_scaleFactor = qMin(5.0, m_scaleFactor);  // 限制最大缩放
    updateDisplay();
}

void AnnotationLabel::zoomOut()
{
    if (m_originalPixmap.isNull()) return;
    
    double scaleFactor = 1.15;
    m_scaleFactor /= scaleFactor;
    m_scaleFactor = qMax(0.1, m_scaleFactor);  // 限制最小缩放
    updateDisplay();
}

void AnnotationLabel::resetZoom()
{
    if (m_originalPixmap.isNull()) return;
    
    m_scaleFactor = 1.0;
    m_imageOffset = QPoint(0, 0);
    updateDisplay();
}

// DialMarkDialog 实现---初始化的函数
DialMarkDialog::DialMarkDialog(QWidget *parent, const QString &dialType)
    : QDialog(parent)
    , ui(nullptr)
    , m_currentColor(Qt::red)
    , m_dialType(dialType)
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
    m_fontComboBox->setCurrentText("黑体"); // 默认黑体
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
    
    m_removeButton = new QPushButton("删除选中标注", buttonGroup);
    m_clearButton = new QPushButton("清除所有标注", buttonGroup);
    m_saveButton = new QPushButton("保存标注", buttonGroup);
    m_loadButton = new QPushButton("加载标注", buttonGroup);
    m_exportButton = new QPushButton("导出图片", buttonGroup);
    
    // 设置按钮样式和字体
    QList<QPushButton*> buttons = {m_removeButton, m_clearButton, m_saveButton, m_loadButton, m_exportButton};
    for (QPushButton* button : buttons) {
        button->setFont(panelFont);
        button->setMinimumHeight(32);
        button->setStyleSheet("QPushButton { padding: 5px 10px; border: 1px solid #ccc; border-radius: 3px; } QPushButton:hover { background-color: #e0e0e0; }");
    }
    
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_exportButton);
    
    controlLayout->addWidget(buttonGroup);
    
    // 在控制面板中添加表盘生成按钮
    QGroupBox *dialGroup = new QGroupBox("表盘生成", controlPanel);
    dialGroup->setFont(panelFont);
    QVBoxLayout *dialLayout = new QVBoxLayout(dialGroup);
    dialLayout->setSpacing(8);
    dialLayout->setContentsMargins(12, 12, 12, 12);
    
    m_generateButton = new QPushButton("生成新表盘", dialGroup);
    m_generateButton->setFont(panelFont);
    m_generateButton->setMinimumHeight(32);
    dialLayout->addWidget(m_generateButton);
    
    // 添加表盘配置控件
    QLabel *maxPressureLabel = new QLabel("最大压力(MPa):");
    maxPressureLabel->setFont(panelFont);
    dialLayout->addWidget(maxPressureLabel);
    
    m_maxPressureSpin = new QDoubleSpinBox(dialGroup);
    m_maxPressureSpin->setRange(0.1, 100.0);  // 支持小数，最小值0.1
    m_maxPressureSpin->setDecimals(1);        // 显示1位小数
    m_maxPressureSpin->setSingleStep(0.1);    // 每次调整0.1
    
    // 根据表盘类型设置默认值
    if (m_dialType == "YYQY-13") {
        m_maxPressureSpin->setValue(6.3);     // YYQY表盘默认6.3MPa
    } else {
        m_maxPressureSpin->setValue(25.0);    // BYQ表盘默认25.0MPa
    }
    
    m_maxPressureSpin->setFont(panelFont);
    dialLayout->addWidget(m_maxPressureSpin);
    
    // 为所有表盘类型添加角度配置
    QLabel *angleLabel = new QLabel("表盘角度(度):");
    angleLabel->setFont(panelFont);
    dialLayout->addWidget(angleLabel);
    
    m_dialAngleSpin = new QDoubleSpinBox(dialGroup);
    if (m_dialType == "YYQY-13") {
        m_dialAngleSpin->setRange(180.0, 360.0);  // YYQY支持更大角度范围
        m_dialAngleSpin->setValue(280.0);         // 默认280度（YYQY标准角度）
    } else {
        m_dialAngleSpin->setRange(60.0, 180.0);   // BYQ支持较小角度范围
        m_dialAngleSpin->setValue(100.0);         // 默认100度（BYQ标准角度）
    }
    m_dialAngleSpin->setDecimals(1);              // 显示1位小数
    m_dialAngleSpin->setSingleStep(0.5);          // 每次调整0.5度
    m_dialAngleSpin->setFont(panelFont);
    dialLayout->addWidget(m_dialAngleSpin);
    
    // 添加保存表盘按钮
    QPushButton *saveDialButton = new QPushButton("保存生成表盘", dialGroup);
    saveDialButton->setFont(panelFont);
    saveDialButton->setMinimumHeight(32);
    dialLayout->addWidget(saveDialButton);
    
    // 添加"从误差表格导入数据"按钮
    QPushButton *importFromErrorTableButton = new QPushButton("从误差表格导入", dialGroup);
    importFromErrorTableButton->setFont(panelFont);
    importFromErrorTableButton->setMinimumHeight(32);
    importFromErrorTableButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; } QPushButton:hover { background-color: #45a049; }");
    dialLayout->addWidget(importFromErrorTableButton);
    
    // 连接导入按钮信号
    connect(importFromErrorTableButton, &QPushButton::clicked, this, [this]() {
        if (!m_errorTableDialog) {
            QMessageBox::information(this, "提示", "请先打开误差检测表格");
            return;
        }
        
        // 调用数据导入方法
        applyFinalDataFromErrorTable();
        
        // 更新界面显示的参数值
        if (m_dialType == "YYQY-13") {
            m_maxPressureSpin->setValue(m_yyqyConfig.maxPressure);
            m_dialAngleSpin->setValue(m_yyqyConfig.totalAngle);
        } else {
            m_maxPressureSpin->setValue(m_byqConfig.maxPressure);
            m_dialAngleSpin->setValue(m_byqConfig.totalAngle);
        }
        
        QMessageBox::information(this, "成功", "已从误差表格导入数据并重新生成表盘");
    });
    
    // 连接信号
    connect(m_generateButton, &QPushButton::clicked, this, [this]() {
        // 更新配置
        if (m_dialType == "YYQY-13") {
            // 更新YYQY配置
            m_yyqyConfig.totalAngle = m_dialAngleSpin->value();
            m_yyqyConfig.maxPressure = m_maxPressureSpin->value();
        } else {
            // 更新BYQ配置
            m_byqConfig.totalAngle = m_dialAngleSpin->value();
            m_byqConfig.maxPressure = m_maxPressureSpin->value();
        }
        
        QPixmap newDial = QPixmap::fromImage(generateDialImage());
        if (!newDial.isNull()) {
            m_imageLabel->setImage(newDial);
            qDebug() << "成功生成新表盘, 类型:" << m_dialType << "角度:" << m_dialAngleSpin->value() << "压力:" << m_maxPressureSpin->value();
        } else {
            QMessageBox::warning(this, "错误", "生成表盘失败");
        }
    });
    
    connect(saveDialButton, &QPushButton::clicked, this, &DialMarkDialog::saveGeneratedDial);
    
    // 将表盘生成组添加到控制布局
    controlLayout->addWidget(dialGroup);
    
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
    connect(m_imageLabel, &AnnotationLabel::annotationRightClicked,
            this, &DialMarkDialog::onAnnotationRightClicked);
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
    
    // 连接角度变化信号（所有表盘类型）
    connect(m_dialAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (m_dialType == "YYQY-13") {
                    m_yyqyConfig.totalAngle = value;
                } else {
                    m_byqConfig.totalAngle = value;
                }
                qDebug() << "角度已更新为：" << value;
            });
    
    // 确保字体设置为默认黑体
    m_fontComboBox->setCurrentText("黑体");
    m_imageLabel->setCurrentFontFamily("黑体");
    m_imageLabel->setCurrentFontSize(12);
    m_imageLabel->setCurrentColor(m_currentColor);
    
    // 添加缩放快捷键
    setupZoomShortcuts();
}

void DialMarkDialog::setupZoomShortcuts()
{
    // 创建缩放快捷键
    QShortcut *zoomInShortcut1 = new QShortcut(QKeySequence("Ctrl++"), this);
    QShortcut *zoomInShortcut2 = new QShortcut(QKeySequence("Ctrl+="), this);
    QShortcut *zoomOutShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    QShortcut *resetZoomShortcut = new QShortcut(QKeySequence("Ctrl+0"), this);
    
    // Mac快捷键支持
    QShortcut *zoomInShortcutMac1 = new QShortcut(QKeySequence("Meta++"), this);
    QShortcut *zoomInShortcutMac2 = new QShortcut(QKeySequence("Meta+="), this);
    QShortcut *zoomOutShortcutMac = new QShortcut(QKeySequence("Meta+-"), this);
    QShortcut *resetZoomShortcutMac = new QShortcut(QKeySequence("Meta+0"), this);
    
    // 连接信号
    connect(zoomInShortcut1, &QShortcut::activated, m_imageLabel, &AnnotationLabel::zoomIn);
    connect(zoomInShortcut2, &QShortcut::activated, m_imageLabel, &AnnotationLabel::zoomIn);
    connect(zoomOutShortcut, &QShortcut::activated, m_imageLabel, &AnnotationLabel::zoomOut);
    connect(resetZoomShortcut, &QShortcut::activated, m_imageLabel, &AnnotationLabel::resetZoom);
    
    connect(zoomInShortcutMac1, &QShortcut::activated, m_imageLabel, &AnnotationLabel::zoomIn);
    connect(zoomInShortcutMac2, &QShortcut::activated, m_imageLabel, &AnnotationLabel::zoomIn);
    connect(zoomOutShortcutMac, &QShortcut::activated, m_imageLabel, &AnnotationLabel::zoomOut);
    connect(resetZoomShortcutMac, &QShortcut::activated, m_imageLabel, &AnnotationLabel::resetZoom);
}

void DialMarkDialog::setupToolbar()
{
    // 可以在这里添加更多工具栏功能
}

void DialMarkDialog::loadDialImageFromFile()
{
    // 询问用户是否要生成新表盘
    QMessageBox::StandardButton reply = QMessageBox::question(this, "选择操作", 
        "是否要生成新的表盘图片？\n选择\"是\"生成新表盘，选择\"否\"加载现有图片。",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // 生成新表盘
        QPixmap newDial = QPixmap::fromImage(generateDialImage());// 生成表盘图片
        if (!newDial.isNull()) {
            m_imageLabel->setImage(newDial);
            qDebug() << "成功生成新表盘";
        } else {
            QMessageBox::warning(this, "错误", "生成表盘失败");
        }
    } else {
        // 加载现有图片（原有逻辑）
        QString imagePath = QApplication::applicationDirPath() + "/images/70-01.tif";//可以自己改路径
        
        if (!QFile::exists(imagePath)) {
            imagePath = "images/70-01.tif";
        }
        
        if (!QFile::exists(imagePath)) {
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
}

void DialMarkDialog::chooseColor()
{
    QColor color = QColorDialog::getColor(m_currentColor, this, "选择文本颜色");
    if (color.isValid()) {
        m_currentColor = color;
        m_imageLabel->setCurrentColor(color);
        updateColorButton();
        
        // 如果有选中的标注，实时更新
        int selected = m_imageLabel->getSelectedAnnotation();
        if (selected >= 0) {
            m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), color, 
                                                  m_fontSizeSpinBox->value(), m_fontComboBox->currentText(),
                                                  m_boldCheckBox->isChecked(), m_italicCheckBox->isChecked());
            updateAnnotationList();
        }
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
    // 如果有选中的标注，实时更新
    int selected = m_imageLabel->getSelectedAnnotation();
    if (selected >= 0) {
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, 
                                              size, m_fontComboBox->currentText(),
                                              m_boldCheckBox->isChecked(), m_italicCheckBox->isChecked());
        updateAnnotationList();
    }
}

void DialMarkDialog::onFontFamilyChanged(const QString &family)
{
    m_imageLabel->setCurrentFontFamily(family);
    // 如果有选中的标注，实时更新
    int selected = m_imageLabel->getSelectedAnnotation();
    if (selected >= 0) {
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, 
                                              m_fontSizeSpinBox->value(), family,
                                              m_boldCheckBox->isChecked(), m_italicCheckBox->isChecked());
        updateAnnotationList();
    }
}

void DialMarkDialog::onBoldChanged(bool bold)
{
    m_imageLabel->setCurrentBold(bold);
    // 如果有选中的标注，实时更新
    int selected = m_imageLabel->getSelectedAnnotation();
    if (selected >= 0) {
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, 
                                              m_fontSizeSpinBox->value(), m_fontComboBox->currentText(),
                                              bold, m_italicCheckBox->isChecked());
        updateAnnotationList();
    }
}

void DialMarkDialog::onItalicChanged(bool italic)
{
    m_imageLabel->setCurrentItalic(italic);
    // 如果有选中的标注，实时更新
    int selected = m_imageLabel->getSelectedAnnotation();
    if (selected >= 0) {
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, 
                                              m_fontSizeSpinBox->value(), m_fontComboBox->currentText(),
                                              m_boldCheckBox->isChecked(), italic);
        updateAnnotationList();
    }
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
            
            // 在图像上选中对应的标注
            m_imageLabel->setSelectedAnnotation(index);
        }
    }
}

void DialMarkDialog::onAnnotationAdded(const QPoint &pos)
{
    updateAnnotationList();
}

void DialMarkDialog::onAnnotationRightClicked(int index, const QPoint &globalPos)
{
    showAnnotationProperties(index);
}

void DialMarkDialog::removeSelectedAnnotation()
{
    int selectedRow = m_annotationList->currentRow();
    if (selectedRow >= 0) {
        m_imageLabel->removeAnnotationAt(selectedRow);
        updateAnnotationList();
    }
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
                if (fontFamily.isEmpty()) fontFamily = "黑体"; // 默认黑体
                
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
    int selected = m_imageLabel->getSelectedAnnotation();
    if (selected >= 0) {
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, 
                                              m_fontSizeSpinBox->value(), m_fontComboBox->currentText(),
                                              m_boldCheckBox->isChecked(), m_italicCheckBox->isChecked());
        updateAnnotationList();
    }
}

void DialMarkDialog::exportImage()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出标注图片", "annotated_dial.tiff", 
        "TIFF 图片 (*.tiff);;PNG 图片 (*.png);;JPEG 图片 (*.jpg);;BMP 图片 (*.bmp);;所有图片 (*.png *.jpg *.jpeg *.bmp *.tiff)");
    
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

static inline int deg16(double deg){ return int(std::round(deg*16.0)); }
static inline QPointF pol2(const QPointF& c, double angDeg, double r){
    const double a = qDegreesToRadians(angDeg);
    return QPointF(c.x() + r*std::cos(a), c.y() - r*std::sin(a)); // y 轴向下
}


//注意一下    v2ang 也需要在中间节点调用
static inline double v2ang(double v, double vmax, double startDeg, double totalAngle,QVector<double> points,QVector<double> pointsAngle){
    //  确保两端点完整（若已经在列表里则不会重复添加）
    QVector<double> allP = points;
    QVector<double> allA = pointsAngle;

    if (allP.isEmpty() || allP.last()  != vmax) allP.append(vmax);
    if (allA.isEmpty() || allA.last()  != totalAngle) allA.append(totalAngle);

    //  参数合法性检查（超出范围则 clamp）
    v = std::clamp(v, 0.0, vmax);

    // 找到 v 所在的主段
    int idx = 0;
    for (int i = 0; i < allP.size() - 1; ++i) {
        if (v >= allP[i] && v <= allP[i + 1]) {
            idx = i;
            break;
        }
    }

    double pCurr = allP[idx];
    double pNext = allP[idx + 1];
    double aCurr = allA[idx];
    double aNext = allA[idx + 1];

    // 最后一段之前均分 5 份，最后一份单独处理
    if(idx == allP.size() - 2){
        // 最后一段
        const double lastStep = 1.0; // 最后一段单独处理，步长为1.0
        double angleStep = (aNext - aCurr) / ((pNext - pCurr) / lastStep);
        return startDeg - (aCurr + (v - pCurr) * angleStep); // 注意这里是减去，因为顺时针方向角度减小
    }else{
        const double subDiv = 5.0;
        double angleStep = (aNext - aCurr) / subDiv;
        return startDeg - (aCurr + (v - pCurr) * angleStep);
        }
}


static inline int dpi_to_dpm(double dpi) { return qRound(dpi / 0.0254); } // = dpi*39.370079
// ======= 主入口：生成表盘图 =======
QImage DialMarkDialog::generateDialImage()
{
    if (m_dialType == "YYQY-13") {
        return generateYYQYDialImage();
    } else {
        return generateBYQDialImage();
    }
}

// ======= BYQ表盘生成 =======
QImage DialMarkDialog::generateBYQDialImage()
{
    
    // 使用优化后的配置参数
    const int OUT_W = 2778;         // 图片宽度
    const int OUT_H = 2363;         // 图片高度
    const double OUT_DPI = 2400;    // 图片DPI

    QImage img(OUT_W, OUT_H, QImage::Format_RGBA64);
    img.fill(Qt::white);

    // 2) 写入 DPI 元数据（TIFF 会映射为 X/YResolution）
    const int dpm = dpi_to_dpm(OUT_DPI);
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // 3) 可选：嵌入 sRGB 色彩空间（印前/跨软件显示更一致）
    // img.setColorSpace(QColorSpace::SRgb);
#endif

    // 4) 开始绘制——根据用户输入的角度和压力值
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 圆心位置
    const QPointF C(OUT_W/2.0, OUT_H * 0.54);
    
    // 表盘半径
    const double Rpx = std::min(OUT_W, OUT_H) * 0.4;

    // 使用用户输入的角度和压力值-------- 改用户输入为自动保存
    const double totalAngle = m_byqConfig.totalAngle;  // 用户输入的角度
    const double vmax = m_byqConfig.maxPressure;       // 用户输入的最大压力
    const QVector<double>& points = m_byqConfig.points;           // 保存的分段点
    const QVector<double>& pointsAngle = m_byqConfig.pointsAngle; // 保存的分段角度

    // 角度系统：根据用户输入的角度-----使用自动保存的起始角度
    const double startDeg = 90.0 + totalAngle/2.0;     // 起始角度（左上）
    const double spanDeg = -totalAngle;                // 角度跨度（顺时针）

    // 主刻度间隔：根据最大压力自动计算  
    double majorStep = 5.0;  // 默认5MPa间隔
    if (vmax <= 10.0) majorStep = 2.0;
    else if (vmax <= 25.0) majorStep = 5.0;
    else if (vmax <= 50.0) majorStep = 10.0;
    else majorStep = 20.0;

    // ① 先绘制刻度与数字
    drawBYQTicksAndNumbers(p, C, Rpx, startDeg, totalAngle, spanDeg, vmax, majorStep, points, pointsAngle);
    // ② 然后绘制彩色带
    drawBYQColorBands(p, C, Rpx, startDeg, totalAngle, spanDeg, vmax, points, pointsAngle);
    // ③ 最后绘制单位
    drawBYQUnitMPa(p, C, Rpx);

    p.end();
    return img;
}

//BYQ刻度绘制
void DialMarkDialog::drawBYQTicksAndNumbers(QPainter& p, const QPointF& C, double outerR,
    double startDeg, double totalAngle, double spanDeg, double vmax, double majorStep, QVector<double> points, QVector<double> pointsAngle)
{
    const double r17_1 = outerR;                        // R17.1：最外圆
    const double r16_1 = outerR * (16.1 / 17.1);       // R16.1：彩色带内圈
    const double r15_1 = outerR * (15.1 / 17.1);       // R15.1：小刻度线外沿
    const double r14_6 = outerR * (14.6 / 17.1);       // R14.6：大刻度线内沿
    const double r13_0 = outerR * (13.0 / 17.1);       // R14.0：数字位置
    // 刻度线宽：根据实际半径调整，保持合理比例
    const double lineScale = outerR / 300.0;  // 基准缩放
    QPen penMinor(Qt::black, std::max(1.0, 0.2 * lineScale * 10), Qt::SolidLine, Qt::RoundCap);    // 小刻度：平直端点
    QPen penMajor(Qt::black, std::max(2.0, 0.4 * lineScale * 10), Qt::SolidLine, Qt::RoundCap);   // 大刻度：圆形端点

    // 次刻度（1 MPa间隔）
    p.setPen(penMinor);
    const double minorStep = 1.0;
    for (double v = 0; v <= vmax + 1e-6; v += minorStep) {
        const double ang = v2ang(v, vmax, startDeg, totalAngle, points, pointsAngle);
        p.drawLine(pol2(C, ang, r16_1), pol2(C, ang, r15_1));
    }

    // 主刻度（5 MPa间隔）————需要改进
    p.setPen(penMajor);
    const double majorTickStep = 5.0;
    for (double v = 0; v <= vmax + 1e-6; v += majorTickStep) {
        const double ang = v2ang(v, vmax, startDeg, totalAngle, points, pointsAngle);
        p.drawLine(pol2(C, ang, r16_1), pol2(C, ang, r14_6));
    }

    // 数字字体：直接使用合理的固定字体大小，不要基于比例计算
    const int fontNumberPx = 3;  // 固定合理字体大小
    QFont numFont("Arial", fontNumberPx, QFont::Bold);
    p.setFont(numFont);
    p.setPen(QPen(Qt::black, 1));

    // 数字：0,10,20,30,40,50（每10MPa一个数字）
    for (double v = 0; v <= vmax + 1e-6; v += majorStep) {
        const double ang = v2ang(v, vmax, startDeg, totalAngle, points, pointsAngle);
        const QPointF pos = pol2(C, ang, r13_0);
        const QString t = QString::number(int(v));
        QRectF br = p.fontMetrics().boundingRect(t);
        br.moveCenter(pos);
        p.drawText(br, Qt::AlignCenter, t);
    }
}

// void DialMarkDialog::drawColorBandsOverTicks(QPainter& p, const QPointF& C, double outerR,
//     double startDeg, double spanDeg, double vmax)
// {
//     // ① 彩带半径 = R16.1 ~ R15.1（图纸要求）
//     const double r17_1 = outerR;   // 彩带外沿
//     const double r16_1 = outerR * (16.1 / 17.1);   // 彩带内沿
//     const double r_mid = 0.5 * (r16_1 + r17_1);    // 以中径+粗笔画
//     const double bandWidth = (r17_1 - r16_1);

//     QRectF arcRect(C.x() - r_mid, C.y() - r_mid, 2*r_mid, 2*r_mid);

//     // ② 与刻度一致的“主刻度线宽” —— 与 drawTicksAndNumbers 里的 penMajor 保持一致
//     const double lineScale = outerR / 300.0;
//     const double w_major   = std::max(2.0, 0.4 * lineScale * 10); // 你的主刻度宽逻辑
//     const double deltaDeg  = qRadiansToDegrees(std::atan((w_major * 0.5) / r_mid));
//     const bool   clockwise = (spanDeg < 0);

//     // ③ 颜色分段
//     const QColor Y07("#D8B64C"), G02("#2E5E36"), R03("#6A2A2A");
//     struct Seg { double v0, v1; QColor c; };
//     const QVector<Seg> segs = {
//         { 0.0,  5.9, Y07},
//         { 5.9, 21.0, G02},
//         {21.0, vmax, R03}
//     };

//     QPen pen; pen.setWidthF(bandWidth); pen.setCapStyle(Qt::FlatCap);

//     // ④ 只在两端做角度补偿；内部边界不补偿（避免断开）
//     const double eps = 1e-9;
//     for (const auto& s : segs) {
//         double a0 = v2ang(s.v0, vmax, startDeg, spanDeg);
//         double a1 = v2ang(s.v1, vmax, startDeg, spanDeg);

//         if (std::abs(s.v0 - 0.0) < eps) {
//             // 左端（0MPa）：让彩带端头贴住 0 的主刻度“外边缘”
//             a0 -= (clockwise ? -deltaDeg : +deltaDeg);
//         }
//         if (std::abs(s.v1 - vmax) < eps) {
//             // 右端（Vmax）：让彩带端头贴住 Vmax 主刻度“外边缘”
//             a1 -= (clockwise ? +deltaDeg : -deltaDeg);
//         }

//         pen.setColor(s.c);
//         p.setPen(pen);
//         p.setBrush(Qt::NoBrush);
//         p.drawArc(arcRect, deg16(a0), deg16(a1 - a0));
//     }
// }

// ======= BYQ表盘绘制函数 =======
void DialMarkDialog::drawBYQColorBands(QPainter& p, const QPointF& C, double outerR,
    double startDeg,double totalAngle, double spanDeg, double vmax,QVector<double> points,QVector<double> pointsAngle)
{
    const double r17_1 = outerR;   // 彩带外沿
    const double r16_1 = outerR * (16.1 / 17.1);   // 彩带内沿
    const double r_mid = 0.5 * (r16_1 + r17_1);
    const double bandWidth = (r17_1 - r16_1);

    QRectF arcRect(C.x() - r_mid, C.y() - r_mid, 2*r_mid, 2*r_mid);

    // 与刻度一致的"主刻度线宽"
    const double lineScale = outerR / 300.0;
    const double w_major = std::max(2.0, 0.4 * lineScale * 10);
    const double deltaDeg = qRadiansToDegrees(std::atan((w_major * 0.5) / r_mid));
    const bool clockwise = (spanDeg < 0);

    // ③ 颜色分段
    const QColor Y07("#D8B64C"), G02("#2E5E36"), R03("#FF0000");
    struct Seg { double v0, v1; QColor c; };
    const QVector<Seg> segs = {
        { 0.0,  5.9, Y07},
        { 5.9, 21.0, G02},
        {21.0, vmax, R03}
    };

    QPen pen;
    pen.setWidthF(bandWidth);
    pen.setCapStyle(Qt::FlatCap);

    // 只在两端做角度补偿
    const double eps = 1e-9;
    for (const auto& s : segs) {
        
        double a0 = v2ang(s.v0, vmax, startDeg, totalAngle, points, pointsAngle);
        double a1 = v2ang(s.v1, vmax, startDeg, totalAngle, points, pointsAngle);
        if (std::abs(s.v0 - 0.0) < eps) {
            a0 -= (clockwise ? -deltaDeg : +deltaDeg);
        }
        if (std::abs(s.v1 - vmax) < eps) {
            a1 -= (clockwise ? +deltaDeg : -deltaDeg);
        }

        pen.setColor(s.c);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawArc(arcRect, deg16(a0), deg16(a1 - a0));
    }
}

// ======= 3) 中央单位与装饰 =======
void DialMarkDialog::drawBYQUnitMPa(QPainter& p, const QPointF& C, double Rpx)
{
    // 直接使用合理的固定字体大小
    double r4_0 = Rpx * (4.0 / 17.1);
    double r2_0 = Rpx * (2.0 / 17.1);
    double fontPixelHeight = (r4_0 - r2_0) * 2;


    QFont font("DIN Rounded");
    font.setPixelSize(std::round(fontPixelHeight)); // 按像素高度设置
    font.setBold(true); // 若图纸要求粗体，可设置
    p.setFont(font);

    p.setPen(QPen(Qt::black, 1));

    const QString MPa = "MPa";
    QRectF textRect = p.fontMetrics().boundingRect(MPa);

    double r3_0 = (r4_0 + r2_0) / 2.0;
    QPointF pos = QPointF(C.x() - textRect.width() / 2.0,
                          C.y() - r3_0 - textRect.height() / 2.0);

    p.drawText(pos, MPa);
}

void DialMarkDialog::saveGeneratedDial()
{
    // 生成表盘图像
    QImage img = generateDialImage();
    
    // 选择保存路径
    QString fileName = QFileDialog::getSaveFileName(this, 
        "保存表盘图片", "", "TIFF Files (*.tiff);;PNG Files (*.png)");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QImageWriter w(fileName, "tiff");
    // 压缩：LZW（无损，兼容性好）；也可改 "deflate"
    w.setCompression(1); // 0=无压缩，1=LZW，2=PackBits，32946=Deflate（按 Qt/平台实现而定）
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt6.5+ 可设置 ICC（如有自定义）
    // w.setColorSpace(QColorSpace::SRgb);
#endif
    if (!w.write(img)) {
        QMessageBox::warning(this, "保存失败", "错误：保存失败: " + w.errorString());
    } else {
        QMessageBox::information(this, "保存成功", "成功：已保存 TIFF: " + fileName);
    }
}

// ======= YYQY表盘生成 =======

QImage DialMarkDialog::generateYYQYDialImage()
{
    // YYQY表盘规格：1890x1890像素，1200x1200分辨率
    const int S = 1890;  
    
    QImage img(S, S, QImage::Format_RGBA64);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    
    const QPointF C(S/2.0, S/2.0);  // 圆心
    const double outerR = 16.3 * S / 42.0;  // 外径半径（缩小表盘，留出边距）
    
    // 使用配置中的角度值
    const double maxPressure = m_yyqyConfig.maxPressure; // 最大压力
    double totalAngle = m_yyqyConfig.totalAngle;
    const QVector<double>& points = m_yyqyConfig.points;           // 保存的分段点
    const QVector<double>& pointsAngle = m_yyqyConfig.pointsAngle; // 保存的分段角度

    qDebug() << "生成YYQY表盘，角度：" << totalAngle;
    
    // 绘制各个组件 - 调整绘制顺序，确保数字不被遮挡
    drawYYQYLogo(p, C, outerR);                        // 绘制商标
    drawYYQYTicks(p, C, outerR, totalAngle, maxPressure, points, pointsAngle);           // 先绘制刻度线
    drawYYQYColorBands(p, C, outerR, totalAngle, maxPressure, points, pointsAngle);      // 然后绘制彩色带
    drawYYQYNumbers(p, C, outerR, totalAngle, maxPressure, points, pointsAngle);         // 再绘制数字（确保在最上层）
    drawYYQYCenterTexts(p, C, outerR);                 // 绘制中心文字
    drawYYQYPositionDot(p, C, outerR, totalAngle);     // 最后绘制定位点
    
    p.end();
    return img;
}

//实在不行就yyqy2ang
static inline double yyqyV2Ang(double v, double vmax, double startDeg, double totalAngle, QVector<double> points, QVector<double> pointsAngle)
{
    // 复制并保证包含端点 0 和 vmax
    QVector<double> allP = points;
    QVector<double> allA = pointsAngle;

    if (allP.isEmpty() || allP.first() != 0.0) {
        allP.prepend(0.0);
        allA.prepend(0.0);
    }
    if (allP.isEmpty() || allP.last() != vmax) {
        allP.append(vmax);
        allA.append(totalAngle);
    }

    // 基本合法性检查：长度、单调性
    if (allP.size() != allA.size()) {
        // 退化为线性映射
        double frac = (vmax > 0.0) ? std::clamp(v / vmax, 0.0, 1.0) : 0.0;
        return startDeg - frac * totalAngle;
    }
    for (int i = 1; i < allP.size(); ++i) {
        if (allP[i] <= allP[i-1] || allA[i] < allA[i-1]) {
            double frac = (vmax > 0.0) ? std::clamp(v / vmax, 0.0, 1.0) : 0.0;
            return startDeg - frac * totalAngle;
        }
    }

    // clamp v
    v = std::clamp(v, 0.0, vmax);

    // 处理边界值，保证 0 与 vmax 精确映射
    if (v <= 0.0) return startDeg;
    if (v >= vmax) return startDeg - totalAngle;

    // 找到包含 v 的段并做线性插值
    int idx = 0;
    for (int i = 0; i < allP.size() - 1; ++i) {
        if (v >= allP[i] && v <= allP[i+1]) {
            idx = i;
            break;
        }
    }

    double p0 = allP[idx], p1 = allP[idx+1];
    double a0 = allA[idx], a1 = allA[idx+1];

    double t = (p1 == p0) ? 0.0 : (v - p0) / (p1 - p0);
    double angSeg = a0 + t * (a1 - a0);

    return startDeg - angSeg;
}


//绘制刻度线I（需要改变）
void DialMarkDialog::drawYYQYTicks(QPainter& p, const QPointF& C, double outerR, double totalAngle,double maxPressure,
    QVector<double> points,QVector<double> pointsAngle)
{
    const double k = outerR / 16.3;  // 缩放系数
    
    const double r_band_outer = 16.3 * k;   // 外圆半径（与彩色带外沿对齐）
    const double r_band_inner = 15.5 * k;   // 彩色带内沿
    const double r_major_outer = r_band_inner;  // 大刻度外径 = 彩色带内沿
    const double r_major_inner = 12.5 * k;  // 大刻度内径
    const double r_minor_outer = r_band_inner;  // 小刻度外径 = 彩色带内沿
    const double r_minor_inner = 13.8 * k;  // 小刻度内径
    const double r_number = 10.0 * k;       // 数字半径，调整到更靠内避免被刻度线遮挡
    
    // 角度设置 - 表盘是左右对称的
    const double startAngle = 90.0 + totalAngle / 2.0;  // 起始角度（从左上开始）
    
    // 计算刻度数量 - 根据配置的最大压力
      // 使用配置的最大压力
    const int totalPositions = (int)(maxPressure * 10) + 1;  // 总刻度位置（每0.1MPa一个位置）
    const double anglePerPosition = totalAngle / (totalPositions - 1);  // 每个位置的角度-----要改成变化的
    
    // 大刻度位置：整数MPa对应的位置索引
    QSet<int> majorPositions;
    for (int i = 0; i <= (int)maxPressure; ++i) {
        majorPositions.insert(i * 10);  // 每整数MPa对应位置索引
    }
    
    QPen minorPen(Qt::black, 0.3 * k, Qt::SolidLine, Qt::RoundCap);  // 小刻度：圆角端点
    QPen majorPen(Qt::black, 0.8 * k, Qt::SolidLine, Qt::RoundCap);  // 大刻度：圆角端点
    
    // 绘制小刻度线 - 在非大刻度位置
    p.setPen(minorPen);
    for (int i = 0; i < totalPositions; ++i) {
        if (!majorPositions.contains(i)) {  // 不是大刻度位置才画小刻度
            double angle = yyqyV2Ang(i * 0.1, maxPressure, startAngle, totalAngle, points, pointsAngle);  // 从左上逆时针
            double rad = qDegreesToRadians(angle);
            QPointF outer(C.x() + r_minor_outer * qCos(rad), 
                         C.y() - r_minor_outer * qSin(rad));
            QPointF inner(C.x() + r_minor_inner * qCos(rad), 
                         C.y() - r_minor_inner * qSin(rad));
            p.drawLine(outer, inner);
        }
    }
    
    // 绘制大刻度线（对应整数MPa）
    p.setPen(majorPen);
    for (int pos : majorPositions) {
        double angle = yyqyV2Ang(static_cast<double>(pos) * 0.1, maxPressure, startAngle, totalAngle, points, pointsAngle);
        double rad = qDegreesToRadians(angle);
        QPointF outer(C.x() + r_major_outer * qCos(rad), 
                     C.y() - r_major_outer * qSin(rad));
        QPointF inner(C.x() + r_major_inner * qCos(rad), 
                     C.y() - r_major_inner * qSin(rad));
        p.drawLine(outer, inner);
    }
}

void DialMarkDialog::drawYYQYNumbers(QPainter& p, const QPointF& C, double outerR, double totalAngle, double maxPressure,
    QVector<double> points, QVector<double> pointsAngle)
{
    const double k = outerR / 16.3;         // 缩放系数
    const double r_number = 10.2 * k;       // 数字半径，调整到更靠内避免被刻度线遮挡
    
    // 角度设置 - 表盘是左右对称的
    const double startAngle = 90.0 + totalAngle / 2.0;  // 起始角度（从左上开始）
    
    // 计算刻度数量 - 根据配置的最大压力
    
    const int totalPositions = (int)(maxPressure * 10) + 1;  // 总刻度位置（每0.1MPa一个位置）
    
    
    // 绘制数字（3号黑体）
    int fontSize = (int)(108 * k);  // 大幅增加字体大小
    fontSize = qMax(fontSize, 72);  // 最小72px
    fontSize = qMin(fontSize, 168);  // 最大168px
    QFont numberFont("SimHei", fontSize, QFont::Normal);  // 数字不加粗
    p.setFont(numberFont);
    p.setPen(Qt::black);
    
    // 绘制数字在对应的大刻度位置
    for (int i = 0; i <= (int)maxPressure; ++i) {
        int pos = i * 10;  // 每整数MPa对应的位置索引
        double angle = yyqyV2Ang(static_cast<double>(pos) * 0.1, maxPressure, startAngle, totalAngle, points, pointsAngle);
        double rad = qDegreesToRadians(angle);
        
        QPointF numberPos(C.x() + r_number * qCos(rad), 
                         C.y() - r_number * qSin(rad));
        
        QString text = QString::number(i);
        
        // 对于数字0，稍微向上偏移避开定位点
        if (i == 0) {
            numberPos.ry() -= k * 1.0;  // 向上偏移
            numberPos.rx() -= k * 1.0;  // 向左偏移一点点
        } else if (i == 1) {
            numberPos.rx() -= k * 0.8;  // 向左偏移一点点
        }
        
        // 计算文本矩形大小
        QFontMetrics fm(numberFont);
        QRect textRect = fm.boundingRect(text);
        QRectF drawRect(numberPos.x() - textRect.width()/2.0, 
                       numberPos.y() - textRect.height()/2.0, 
                       textRect.width(), textRect.height());
        
        p.drawText(drawRect, Qt::AlignCenter, text);
    }
}

void DialMarkDialog::drawYYQYColorBands(QPainter& p, const QPointF& C, double outerR, double totalAngle, double maxPressure,
                                        QVector<double> points, QVector<double> pointsAngle)
{
    const double k = outerR / 16.3;
    const double r_band_outer = 16.3 * k;   // 彩色带外沿
    const double r_band_inner = 15.5 * k;   // 彩色带内沿（与刻度外沿对齐）
    const double bandWidth = r_band_outer - r_band_inner;
    const double r_mid = (r_band_outer + r_band_inner) / 2.0;
    
    QRectF arcRect(C.x() - r_mid, C.y() - r_mid, 2 * r_mid, 2 * r_mid);
    
    // 角度设置 - 与刻度保持一致
    const double startAngle = 90.0 + totalAngle / 2.0;  // 起始角度


    // 计算不同刻度线宽对应的角度补偿
    const double w_major = 0.8 * k;  // 大刻度线宽
    const double w_minor = 0.3 * k;  // 小刻度线宽
    const double deltaDeg_major = qRadiansToDegrees(std::atan((w_major * 0.5) / r_mid));
    const double deltaDeg_minor = qRadiansToDegrees(std::atan((w_minor * 0.5) / r_mid));
    
    QPen pen;
    pen.setWidthF(bandWidth);
    pen.setCapStyle(Qt::FlatCap);
    
    // 0-warningPressure：黑色
    double black_start_angle = startAngle;
    double black_end_angle = yyqyV2Ang(m_yyqyConfig.warningPressure, maxPressure, startAngle, totalAngle, points, pointsAngle);
    
    // 起始端（0MPa）角度补偿：0位置是大刻度，使用大刻度线宽
    black_start_angle += deltaDeg_major;
    
    pen.setColor(Qt::black);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(arcRect, deg16(black_start_angle), deg16(black_end_angle - black_start_angle));
    
    // warningPressure-maxPressure：红色
    double red_start_angle = black_end_angle;  // 从警告压力开始（无补偿，避免断开）
    double red_end_angle = yyqyV2Ang(m_yyqyConfig.maxPressure, maxPressure, startAngle, totalAngle, points, pointsAngle);
    
    // 结束端角度补偿：检查末尾位置是大刻度还是小刻度
    // 末尾位置的压力值
    double endPressureValue = maxPressure;
    bool isEndMajorTick = (std::abs(endPressureValue - std::round(endPressureValue)) < 1e-6);
    
    if (isEndMajorTick) {
        // 如果末尾是大刻度，使用大刻度线宽
        red_end_angle -= deltaDeg_major;
    } else {
        // 如果末尾是小刻度，使用小刻度线宽
        red_end_angle -= deltaDeg_minor;
    }
    
    pen.setColor(Qt::red);
    p.setPen(pen);
    p.drawArc(arcRect, deg16(red_start_angle), deg16(red_end_angle - red_start_angle));
}

void DialMarkDialog::drawYYQYCenterTexts(QPainter& p, const QPointF& C, double outerR)
{
    const double k = outerR / 16.3;
    
    // "MPa"文字在圆心正上方R3位置（2号黑体）- 修复字体大小
    int mpaFontSize = (int)(108 * k);  // 2号字体大幅增加，从18*k改为36*k
    mpaFontSize = qMax(mpaFontSize, 88);  // 最小28px
    mpaFontSize = qMin(mpaFontSize, 88);  // 最大56px
    QFont mpaFont("黑体", mpaFontSize);  // MPa文字加粗
    //mpaFont.setStretch(QFont::Condensed);  // 设置为窄体（高高细细）
    p.setFont(mpaFont);
    p.setPen(Qt::black);
    
    QPointF mpaPos(C.x(), C.y() - 3 * k);
    QFontMetrics mpafm(mpaFont);
    QRect mpaRect = mpafm.boundingRect("MPa");
    QRectF mpaDrawRect(mpaPos.x() - mpaRect.width()/2.0, 
                      mpaPos.y() - mpaRect.height()/2.0, 
                      mpaRect.width(), mpaRect.height());
    p.drawText(mpaDrawRect, Qt::AlignCenter, "MPa");
    
    // "禁油"和"氧气"文字（3号黑体）- 修复字体大小
    int textFontSize = (int)(88 * k);  // 3号字体大幅增加，从20*k改为40*k
    textFontSize = qMax(textFontSize, 88);  // 最小32px
    textFontSize = qMin(textFontSize, 88);  // 最大64px
    QFont textFont("黑体", textFontSize, QFont::Normal);
    //textFont.setStretch(QFont::Condensed);  // 设置为窄体（高高细细）
    p.setFont(textFont);

    double text_r = 4.5 * k;  // R4位置
    double text_y_offset = 1.5 * k;  // 相对圆心往下偏移
    
    // "禁油"文字在圆心左边
    QPointF jinYouPos(C.x() - text_r, C.y() + text_y_offset);  // 添加向下偏移
    QFontMetrics textfm(textFont);
    QRect jinYouRect = textfm.boundingRect("禁油");
    QRectF jinYouDrawRect(jinYouPos.x() - jinYouRect.width()/2.0, 
                         jinYouPos.y() - jinYouRect.height()/2.0, 
                         jinYouRect.width(), jinYouRect.height());
    p.drawText(jinYouDrawRect, Qt::AlignCenter, "禁油");
    
    // "氧气"文字在圆心右边（有蓝色下划线）
    QPointF yangQiPos(C.x() + text_r, C.y() + text_y_offset);  // 添加向下偏移
    QRect yangQiRect = textfm.boundingRect("氧气");
    QRectF yangQiDrawRect(yangQiPos.x() - yangQiRect.width()/2.0, 
                         yangQiPos.y() - yangQiRect.height()/2.0, 
                         yangQiRect.width(), yangQiRect.height());
    p.setPen(Qt::black);
    p.drawText(yangQiDrawRect, Qt::AlignCenter, "氧气");
    
    // 绘制"氧气"的蓝色下划线（酞蓝色PB06，宽0.5，圆角）---改0.1
    QPen underlinePen(QColor("#0066CC"), 0.25 * k, Qt::SolidLine);
    p.setPen(underlinePen);
    double underlineY = yangQiDrawRect.bottom() + k * 0.3;  //   - k * 0.1;  // 更靠近文字
    // 下划线与文字一样宽，不留边距
    p.drawLine(QPointF(yangQiDrawRect.left() + 0.6 * k, underlineY), 
               QPointF(yangQiDrawRect.right() - 0.6 * k, underlineY));
}

void DialMarkDialog::drawYYQYPositionDot(QPainter& p, const QPointF& C, double outerR, double totalAngle)
{
    const double k = outerR / 16.3;
    
    // 定位点是两个条件的交点：
    // 1. 角度：0刻度线减1度的位置
    // 2. 距离：圆心垂直向下R10
    const double startAngle = 90.0 + totalAngle / 2.0;  // 0刻度线的角度
    double dotAngle = startAngle + 6.0;  // 0刻度减1度（向逆时针偏移）
    
    // 计算在该角度方向上，距离圆心R10的点
    double r_dot = 11.4 * k;
    double rad = qDegreesToRadians(dotAngle);
    QPointF dotPos(C.x() + r_dot * qCos(rad), 
                   C.y() - r_dot * qSin(rad));
    
    p.setPen(Qt::black);
    p.setBrush(Qt::black);
    double dotRadius = 0.3 * k;  // 缩小定位点
    p.drawEllipse(dotPos, dotRadius, dotRadius);
}

void DialMarkDialog::drawYYQYLogo(QPainter& p, const QPointF& C, double outerR)
{
    const double k = outerR / 16.3;  // 缩放系数
    
    // 加载商标图片
    QString logoPath = QApplication::applicationDirPath() + "/images/logo_region.png";
    QPixmap logoPixmap(logoPath);
    
    if (logoPixmap.isNull()) {
        qDebug() << "无法加载商标图片：" << logoPath;
        return;
    }
    
    QImage logoImage = logoPixmap.toImage();
    
    // 商标上边界在圆心向下R19的位置
    double logoTopY = C.y() + 10.0 * k;  // 圆心向下R19
    
    // 商标大小不受刻度盘角度影响，只依赖于表盘缩放
    double logoScale = k * 0.01913824057;  // 固定比例，不受角度影响
    // 计算缩放后的商标尺寸
    int scaledWidth = (int)(logoImage.width() * logoScale);
    int scaledHeight = (int)(logoImage.height() * logoScale);
    
    // 缩放商标图像
    QImage scaledLogo = logoImage.scaled(scaledWidth, scaledHeight, 
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // 计算商标的中心位置（水平居中，上边界在指定位置）
    QPointF logoPos(C.x() - scaledWidth / 2.0, logoTopY);
    
    // 绘制商标
    p.drawImage(logoPos, scaledLogo);
    
    qDebug() << "商标已绘制在位置：" << logoPos << "，尺寸：" << scaledWidth << "x" << scaledHeight;
}

void DialMarkDialog::setErrorTableDialog(ErrorTableDialog *dlg)
{
    m_errorTableDialog = dlg;
}



void DialMarkDialog::addBYQconfig(double maxPressure,double totalAngle,QVector<double> points,QVector<double> pointsAngle) 
{
    m_byqConfig.maxPressure = maxPressure;
    m_byqConfig.totalAngle = totalAngle;
    m_byqConfig.points.clear();
    for (const double &v : points) {
        m_byqConfig.points.append(v);
    }

    m_byqConfig.pointsAngle.clear();
    for (const double &a : pointsAngle) {
        m_byqConfig.pointsAngle.append(a);
    }
}
void DialMarkDialog::addYYQYconfig(double maxPressure,double totalAngle,QVector<double> points,QVector<double> pointsAngle)
{
    m_yyqyConfig.maxPressure = maxPressure;
    m_yyqyConfig.totalAngle = totalAngle;
    m_yyqyConfig.points.clear();
    for (const double &v : points) {
        m_yyqyConfig.points.append(v);
    }

    m_yyqyConfig.pointsAngle.clear();
    for (const double &a : pointsAngle) {
        m_yyqyConfig.pointsAngle.append(a);
    }
}

// 把 final_data 转换并传给 DialMarkDialog，立即刷新预览
void DialMarkDialog::applyFinalDataFromErrorTable()
{
    if (!m_errorTableDialog) {
        qDebug() << "applyFinalDataFromErrorTable: 误差表格对话框未打开";
        return;  // 如果表格窗口没有打开，就不更新
    }
    
    // 区分类型并传入对应数据
    if (m_dialType == "BYQ-19") {
        // 检查数据有效性
        if (m_errorTableDialog->m_byqFinalData.points.isEmpty() || 
            m_errorTableDialog->m_byqFinalData.pointsAngle.isEmpty()) {
            qDebug() << "applyFinalDataFromErrorTable: BYQ数据为空，跳过应用";
            QMessageBox::warning(this, "数据无效", "误差表格中没有有效的BYQ表盘数据，请先在误差表格中完成数据采集");
            return;
        }
        
        // 直接使用 DialMarkDialog 的接口（addBYQconfig 已实现为写入并刷新）
        addBYQconfig(
            m_errorTableDialog->m_byqFinalData.maxPressure,
            m_errorTableDialog->m_byqFinalData.totalAngle,
            m_errorTableDialog->m_byqFinalData.points,
            m_errorTableDialog->m_byqFinalData.pointsAngle
        );
    } else if (m_dialType == "YYQY-13") {
        // 检查数据有效性
        if (m_errorTableDialog->m_yyqyFinalData.points.isEmpty() || 
            m_errorTableDialog->m_yyqyFinalData.pointsAngle.isEmpty()) {
            qDebug() << "applyFinalDataFromErrorTable: YYQY数据为空，跳过应用";
            QMessageBox::warning(this, "数据无效", "误差表格中没有有效的YYQY表盘数据，请先在误差表格中完成数据采集");
            return;
        }
        
        addYYQYconfig(
            m_errorTableDialog->m_yyqyFinalData.maxPressure,
            m_errorTableDialog->m_yyqyFinalData.totalAngle,
            m_errorTableDialog->m_yyqyFinalData.points,
            m_errorTableDialog->m_yyqyFinalData.pointsAngle
        );
    }

    // 关键：立即重新生成并替换显示的图片
    if (m_imageLabel) {
        QImage img = generateDialImage();
        if (!img.isNull()) {
            m_imageLabel->setImage(QPixmap::fromImage(img));
            qDebug() << "applyFinalDataFromErrorTable: 已应用最终数据并刷新显示";
        } else {
            qDebug() << "applyFinalDataFromErrorTable: 生成图像失败";
        }
    }
}