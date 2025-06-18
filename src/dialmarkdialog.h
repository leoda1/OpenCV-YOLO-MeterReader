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

namespace Ui {
class DialMarkDialog;
}

// 文本标注项
struct TextAnnotation {
    QPoint position;
    QString text;
    QColor color;
    int fontSize;
    bool isSelected;
    QRect boundingRect;
    
    TextAnnotation() : color(Qt::red), fontSize(12), isSelected(false) {}
};

// 自定义图片显示标签类，支持鼠标交互
class AnnotationLabel : public QLabel
{
    Q_OBJECT

public:
    explicit AnnotationLabel(QWidget *parent = nullptr);
    void setImage(const QPixmap &pixmap);
    void addTextAnnotation(const QPoint &pos, const QString &text, const QColor &color, int fontSize);
    void clearAnnotations();
    void setCurrentColor(const QColor &color) { m_currentColor = color; }
    void setCurrentFontSize(int size) { m_currentFontSize = size; }
    void setAnnotationMode(bool enabled) { m_annotationMode = enabled; }
    const QList<TextAnnotation>& getAnnotations() const { return m_annotations; }
    void removeSelectedAnnotation();
    void updateSelectedAnnotation(const QString &text, const QColor &color, int fontSize);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void annotationClicked(int index);
    void annotationAdded(const QPoint &pos);

private:
    QPixmap m_originalPixmap;
    QList<TextAnnotation> m_annotations;
    QColor m_currentColor;
    int m_currentFontSize;
    bool m_annotationMode;
    int m_selectedAnnotation;
    
    void updateDisplay();
    int findAnnotationAt(const QPoint &pos);
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
    void onAnnotationClicked(int index);
    void onAnnotationAdded(const QPoint &pos);
    void removeAnnotation();
    void clearAllAnnotations();
    void saveAnnotations();
    void loadAnnotations();
    void onTextChanged();
    void updateAnnotationList();

private:
    Ui::DialMarkDialog *ui;
    AnnotationLabel *m_imageLabel;
    QScrollArea *m_scrollArea;
    QPushButton *m_colorButton;
    QSpinBox *m_fontSizeSpinBox;
    QLineEdit *m_textEdit;
    QListWidget *m_annotationList;
    QPushButton *m_removeButton;
    QPushButton *m_clearButton;
    QPushButton *m_saveButton;
    QPushButton *m_loadButton;
    
    QColor m_currentColor;
    
    void setupUI();
    void setupToolbar();
    void updateColorButton();
    void loadDialImageFromFile();
};

#endif // DIALMARKDIALOG_H 