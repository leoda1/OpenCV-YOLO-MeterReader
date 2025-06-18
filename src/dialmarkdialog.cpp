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
    , m_annotationMode(true)
    , m_selectedAnnotation(-1)
{
    setMinimumSize(400, 300);
    setAlignment(Qt::AlignCenter);
    setStyleSheet("border: 1px solid gray;");
    setCursor(Qt::CrossCursor);
}

void AnnotationLabel::setImage(const QPixmap &pixmap)
{
    m_originalPixmap = pixmap;
    updateDisplay();
}

void AnnotationLabel::addTextAnnotation(const QPoint &pos, const QString &text, const QColor &color, int fontSize)
{
    TextAnnotation annotation;
    annotation.position = pos;
    annotation.text = text;
    annotation.color = color;
    annotation.fontSize = fontSize;
    annotation.isSelected = false;
    
    // 计算文本边界框
    QFont font;
    font.setPointSize(fontSize);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(text);
    annotation.boundingRect = QRect(pos.x(), pos.y() - textRect.height(), 
                                   textRect.width(), textRect.height());
    
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

void AnnotationLabel::updateSelectedAnnotation(const QString &text, const QColor &color, int fontSize)
{
    if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
        TextAnnotation &annotation = m_annotations[m_selectedAnnotation];
        annotation.text = text;
        annotation.color = color;
        annotation.fontSize = fontSize;
        
        // 重新计算边界框
        QFont font;
        font.setPointSize(fontSize);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(text);
        annotation.boundingRect = QRect(annotation.position.x(), 
                                       annotation.position.y() - textRect.height(), 
                                       textRect.width(), textRect.height());
        
        updateDisplay();
    }
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
        
        QFont font;
        font.setPointSize(annotation.fontSize);
        painter.setFont(font);
        painter.setPen(annotation.color);
        
        // 如果是选中的标注，添加背景
        if (i == m_selectedAnnotation) {
            painter.fillRect(annotation.boundingRect, QColor(255, 255, 0, 100));
        }
        
        painter.drawText(annotation.position, annotation.text);
    }
}

void AnnotationLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_annotationMode) {
        QPoint clickPos = event->pos();
        
        // 首先检查是否点击了现有标注
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
            // 点击空白区域，取消选择并添加新标注
            if (m_selectedAnnotation >= 0) {
                m_annotations[m_selectedAnnotation].isSelected = false;
                m_selectedAnnotation = -1;
                updateDisplay();
            }
            emit annotationAdded(clickPos);
        }
    }
    
    QLabel::mousePressEvent(event);
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
                QFont font;
                font.setPointSize(m_annotations[annotationIndex].fontSize);
                QFontMetrics fm(font);
                QRect textRect = fm.boundingRect(newText);
                m_annotations[annotationIndex].boundingRect = 
                    QRect(m_annotations[annotationIndex].position.x(), 
                         m_annotations[annotationIndex].position.y() - textRect.height(), 
                         textRect.width(), textRect.height());
                
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
    setMinimumSize(800, 600);
    resize(1000, 700);
    
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
    
    // 右侧：控制面板
    QWidget *controlPanel = new QWidget(this);
    controlPanel->setFixedWidth(250);
    controlPanel->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
    
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);
    
    // 文本输入组
    QGroupBox *textGroup = new QGroupBox("文本标注", controlPanel);
    QVBoxLayout *textLayout = new QVBoxLayout(textGroup);
    
    m_textEdit = new QLineEdit(textGroup);
    m_textEdit->setPlaceholderText("请输入标注文本...");
    textLayout->addWidget(m_textEdit);
    
    // 颜色选择
    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->addWidget(new QLabel("颜色:"));
    m_colorButton = new QPushButton("选择颜色", textGroup);
    m_colorButton->setFixedHeight(30);
    updateColorButton();
    colorLayout->addWidget(m_colorButton);
    textLayout->addLayout(colorLayout);
    
    // 字体大小
    QHBoxLayout *fontLayout = new QHBoxLayout();
    fontLayout->addWidget(new QLabel("字体大小:"));
    m_fontSizeSpinBox = new QSpinBox(textGroup);
    m_fontSizeSpinBox->setRange(8, 48);
    m_fontSizeSpinBox->setValue(12);
    fontLayout->addWidget(m_fontSizeSpinBox);
    textLayout->addLayout(fontLayout);
    
    controlLayout->addWidget(textGroup);
    
    // 标注列表组
    QGroupBox *listGroup = new QGroupBox("标注列表", controlPanel);
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);
    
    m_annotationList = new QListWidget(listGroup);
    m_annotationList->setMaximumHeight(150);
    listLayout->addWidget(m_annotationList);
    
    controlLayout->addWidget(listGroup);
    
    // 操作按钮组
    QGroupBox *buttonGroup = new QGroupBox("操作", controlPanel);
    QVBoxLayout *buttonLayout = new QVBoxLayout(buttonGroup);
    
    m_removeButton = new QPushButton("删除选中标注", buttonGroup);
    m_clearButton = new QPushButton("清除所有标注", buttonGroup);
    m_saveButton = new QPushButton("保存标注", buttonGroup);
    m_loadButton = new QPushButton("加载标注", buttonGroup);
    
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_loadButton);
    
    controlLayout->addWidget(buttonGroup);
    
    controlLayout->addStretch();
    
    // 添加到主布局
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(controlPanel, 0);
    
    // 连接信号槽
    connect(m_colorButton, &QPushButton::clicked, this, &DialMarkDialog::chooseColor);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &DialMarkDialog::onFontSizeChanged);
    connect(m_imageLabel, &AnnotationLabel::annotationClicked, 
            this, &DialMarkDialog::onAnnotationClicked);
    connect(m_imageLabel, &AnnotationLabel::annotationAdded, 
            this, &DialMarkDialog::onAnnotationAdded);
    connect(m_removeButton, &QPushButton::clicked, this, &DialMarkDialog::removeAnnotation);
    connect(m_clearButton, &QPushButton::clicked, this, &DialMarkDialog::clearAllAnnotations);
    connect(m_saveButton, &QPushButton::clicked, this, &DialMarkDialog::saveAnnotations);
    connect(m_loadButton, &QPushButton::clicked, this, &DialMarkDialog::loadAnnotations);
    connect(m_textEdit, &QLineEdit::textChanged, this, &DialMarkDialog::onTextChanged);
    connect(m_annotationList, &QListWidget::currentRowChanged, 
            this, &DialMarkDialog::onAnnotationClicked);
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
    QString styleSheet = QString("QPushButton { background-color: %1; color: %2; border: 1px solid #999; }")
                        .arg(m_currentColor.name())
                        .arg(m_currentColor.lightness() > 128 ? "black" : "white");
    m_colorButton->setStyleSheet(styleSheet);
    m_colorButton->setText(m_currentColor.name());
}

void DialMarkDialog::onFontSizeChanged(int size)
{
    m_imageLabel->setCurrentFontSize(size);
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
            updateColorButton();
            
            // 更新列表选择
            m_annotationList->setCurrentRow(index);
        }
    }
}

void DialMarkDialog::onAnnotationAdded(const QPoint &pos)
{
    QString text = m_textEdit->text().trimmed();
    if (text.isEmpty()) {
        text = QString("标注%1").arg(m_imageLabel->getAnnotations().size() + 1);
    }
    
    m_imageLabel->addTextAnnotation(pos, text, m_currentColor, m_fontSizeSpinBox->value());
    updateAnnotationList();
}

void DialMarkDialog::removeAnnotation()
{
    m_imageLabel->removeSelectedAnnotation();
    updateAnnotationList();
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
                
                m_imageLabel->addTextAnnotation(pos, text, color, fontSize);
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
        m_imageLabel->updateSelectedAnnotation(m_textEdit->text(), m_currentColor, m_fontSizeSpinBox->value());
        updateAnnotationList();
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