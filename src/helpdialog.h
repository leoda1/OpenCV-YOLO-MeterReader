#pragma once
#include <QDialog>

class QTextBrowser;

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget* parent = nullptr);
    ~HelpDialog() override = default;

private:
    void buildUi();
    QString buildHtml() const;

    QTextBrowser* m_browser = nullptr;
};