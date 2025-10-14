#ifndef DIALMARKDIALOG_H
#define DIALMARKDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QLineEdit>
#include <QColorDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTextEdit>
#include <opencv2/opencv.hpp>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QListWidget>
#include <QFontComboBox>
#include <QRubberBand>
#include <QMenu>
#include <QCheckBox>
#include <QInputDialog>
#include <QShortcut>
#include <QKeyEvent>

namespace Ui {
class DialMarkDialog;
}

// 文本标注项
struct TextAnnotation {
    QPoint position;
    QString text;
    QColor color;
    int fontSize;
    QString fontFamily;
    bool isBold;
    bool isItalic;
    bool isSelected;
    QRect boundingRect;
    
    TextAnnotation() : color(Qt::red), fontSize(12), fontFamily("黑体"), 
                      isBold(false), isItalic(false), isSelected(false) {}
};

// BYQ表盘配置
struct BYQDialConfig {
    double maxPressure;      // 最大压力值 (MPa)
    double totalAngle;       // 表盘总角度 (度)
    double majorStep;        // 主刻度步长 (MPa)
    double minorStep;        // 次刻度步长 (MPa)
    QVector<double> points;  // 中间节点的压力值
    QVector<double> pointsAngle;  // 中间节点的角度值

    BYQDialConfig() : maxPressure(25.0), totalAngle(138.0), majorStep(5.0), minorStep(1.0), points({0.0, 5.0, 10.0, 15.0, 20.0}), pointsAngle({0.0, 20.0, 50.0, 80.0, 125.0}) {}
};

// YYQY表盘配置
struct YYQYDialConfig {
    double maxPressure;      // 最大压力值 (MPa)
    double totalAngle;       // 表盘总角度 (度)
    double warningPressure;  // 警告压力值 (MPa) - 黑色到红色的分界点
    QVector<double> points;  // 中间节点的压力值
    QVector<double> pointsAngle;  // 中间节点的角度值    
    YYQYDialConfig() : maxPressure(6.3), totalAngle(280.0), warningPressure(4.0) ,points({0.0, 1.0, 2.0, 3.0, 4.0, 5.0}), pointsAngle({0.0, 45.0, 90.0, 135.0, 185.0, 222.5}) {}
};

// 自定义图片显示标签类，支持鼠标交互
class AnnotationLabel : public QLabel
{
    Q_OBJECT

public:
    explicit AnnotationLabel(QWidget *parent = nullptr);
    void setImage(const QPixmap &pixmap);
    void addTextAnnotation(const QPoint &pos, const QString &text, const QColor &color, 
                          int fontSize, const QString &fontFamily = "黑体", 
                          bool isBold = false, bool isItalic = false);
    void addRectAnnotation(const QRect &rect, const QString &text, const QColor &color, 
                          int fontSize, const QString &fontFamily = "黑体", 
                          bool isBold = false, bool isItalic = false);
    void clearAnnotations();
    void setCurrentColor(const QColor &color) { m_currentColor = color; }
    void setCurrentFontSize(int size) { m_currentFontSize = size; }
    void setCurrentFontFamily(const QString &family) { m_currentFontFamily = family; }
    void setCurrentBold(bool bold) { m_currentBold = bold; }
    void setCurrentItalic(bool italic) { m_currentItalic = italic; }
    const QList<TextAnnotation>& getAnnotations() const { return m_annotations; }
    void removeSelectedAnnotation();
    void removeAnnotationAt(int index);
    void updateSelectedAnnotation(const QString &text, const QColor &color, int fontSize, 
                                 const QString &fontFamily, bool isBold = false, bool isItalic = false);
    QPixmap getAnnotatedImage() const;
    int getSelectedAnnotation() const { return m_selectedAnnotation; }
    void setSelectedAnnotation(int index);
    
    // 缩放控制方法
    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void annotationClicked(int index);
    void annotationAdded(const QPoint &pos);
    void annotationRightClicked(int index, const QPoint &globalPos);

private:
    QPixmap m_originalPixmap;
    QList<TextAnnotation> m_annotations;
    QColor m_currentColor;
    int m_currentFontSize;
    QString m_currentFontFamily;
    bool m_currentBold;
    bool m_currentItalic;
    int m_selectedAnnotation;
    
    // 拖动相关
    bool m_isDraggingAnnotation;
    QPoint m_lastMousePos;
    QPoint m_dragOffset;
    
    // 图片缩放和拖动相关
    double m_scaleFactor;
    QPoint m_imageOffset;
    bool m_isDraggingImage;
    QPoint m_lastPanPoint;
    
    void updateDisplay();
    int findAnnotationAt(const QPoint &pos, const QRect &imageRect);
    void updateBoundingRect(TextAnnotation &annotation);
    QString showTextInputDialog(const QString &currentText = "");
};

class AnnotationPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnnotationPropertiesDialog(const TextAnnotation &annotation, QWidget *parent = nullptr);
    TextAnnotation getAnnotation() const;

private:
    QLineEdit *m_textEdit;
    QPushButton *m_colorButton;
    QSpinBox *m_fontSizeSpinBox;
    QFontComboBox *m_fontComboBox;
    QCheckBox *m_boldCheckBox;
    QCheckBox *m_italicCheckBox;
    
    QColor m_currentColor;
    
    void updateColorButton();
    void chooseColor();
};

class DialMarkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DialMarkDialog(QWidget *parent = nullptr, const QString &dialType = "BYQ-19");
    ~DialMarkDialog();

private slots:
    void loadDialImage();
    void chooseColor();
    void onFontSizeChanged(int size);
    void onFontFamilyChanged(const QString &family);
    void onBoldChanged(bool bold);
    void onItalicChanged(bool italic);
    void onAnnotationClicked(int index);
    void onAnnotationAdded(const QPoint &pos);
    void onAnnotationRightClicked(int index, const QPoint &globalPos);
    void removeSelectedAnnotation();
    void clearAllAnnotations();
    void saveAnnotations();
    void loadAnnotations();
    void onTextChanged();
    void updateAnnotationList();
    void exportImage();

private:
    Ui::DialMarkDialog *ui;
    AnnotationLabel *m_imageLabel;
    QScrollArea *m_scrollArea;
    QPushButton *m_colorButton;
    QSpinBox *m_fontSizeSpinBox;
    QFontComboBox *m_fontComboBox;
    QCheckBox *m_boldCheckBox;
    QCheckBox *m_italicCheckBox;
    QLineEdit *m_textEdit;
    QListWidget *m_annotationList;
    QPushButton *m_removeButton;
    QPushButton *m_clearButton;
    QPushButton *m_saveButton;
    QPushButton *m_loadButton;
    QPushButton *m_exportButton;
    
    QColor m_currentColor;
    QString m_dialType;  // 表盘类型
    
    // 表盘配置
    BYQDialConfig m_byqConfig;    // BYQ表盘配置
    YYQYDialConfig m_yyqyConfig;  // YYQY表盘配置
    
    // 表盘生成相关
    QImage generateDialImage();
    QImage generateBYQDialImage();   // BYQ类型表盘
    QPixmap generateYYQYDialImage();  // YYQY类型表盘
    
    // BYQ表盘绘制方法
    void drawBYQTicksAndNumbers(QPainter& p, const QPointF& C, double outerR,
                                double startDeg,double totalAngle, double spanDeg, double vmax, double majorStep,
                                QVector<double> points,QVector<double> pointsAngle);
    void drawBYQColorBands(QPainter& p, const QPointF& C, double outerR,
                           double startDeg, double totalAngle, double spanDeg, double vmax,
                           QVector<double> points,QVector<double> pointsAngle);
    void drawBYQUnitMPa(QPainter& p, const QPointF& C, double outerR);
    
    // YYQY表盘绘制方法  
    void drawYYQYTicks(QPainter& p, const QPointF& C, double outerR, double totalAngle,double maxPressure,
                       QVector<double> points,QVector<double> pointsAngle);
    void drawYYQYNumbers(QPainter& p, const QPointF& C, double outerR, double totalAngle, double maxPressure,
                         QVector<double> points, QVector<double> pointsAngle);
    void drawYYQYColorBands(QPainter& p, const QPointF& C, double outerR, double totalAngle,double maxPressure,
                                        QVector<double> points,QVector<double> pointsAngle);
    void drawYYQYCenterTexts(QPainter& p, const QPointF& C, double outerR);
    void drawYYQYPositionDot(QPainter& p, const QPointF& C, double outerR, double totalAngle);
    void drawYYQYLogo(QPainter& p, const QPointF& C, double outerR);  // 绘制商标
    
    void saveGeneratedDial();
    
    // 表盘配置参数
    struct DialConfig {
        int imageSize = 800;           // 图片尺寸
        int dialRadius = 350;          // 表盘半径
        double startAngle = -135.0;    // 起始角度（度）
        double endAngle = 135.0;       // 结束角度（度）
        double maxPressure = 25.0;     // 最大压力值
        QList<double> majorScales;     // 主刻度值
        QList<double> minorScales;     // 次刻度值
        QColor backgroundColor = Qt::white;
        QColor dialColor = Qt::black;
        QColor scaleColor = Qt::black;
        QColor numberColor = Qt::black;
    };
    
    DialConfig m_dialConfig;    
    QPushButton *m_generateButton;  // 生成表盘按钮
    QDoubleSpinBox *m_maxPressureSpin;    // 最大压力输入框（支持小数）
    QDoubleSpinBox *m_dialAngleSpin;      // 表盘角度输入框（支持小数，仅YYQY类型）

    void setupUI();
    void setupToolbar();
    void setupZoomShortcuts();
    void updateColorButton();
    void loadDialImageFromFile();
    void showAnnotationProperties(int index);
};

#endif // DIALMARKDIALOG_H 