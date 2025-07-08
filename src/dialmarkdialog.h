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
#include <QGroupBox>
#include <QListWidget>
#include <QFontComboBox>
#include <QRubberBand>
#include <QMenu>
#include <QCheckBox>
#include <QInputDialog>

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

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

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
    explicit DialMarkDialog(QWidget *parent = nullptr);
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
    
    void setupUI();
    void setupToolbar();
    void updateColorButton();
    void loadDialImageFromFile();
    void showAnnotationProperties(int index);
};

#endif // DIALMARKDIALOG_H 