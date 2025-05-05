/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.6.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QGridLayout *gridLayout_3;
    QWidget *widget_pic;
    QGridLayout *gridLayout;
    QLabel *label;
    QWidget *widget_2;
    QGridLayout *gridLayout_2;
    QSpacerItem *verticalSpacer;
    QComboBox *comboBox_CFMode;
    QLabel *label_exTime;
    QPushButton *pushButton_getExTime;
    QPushButton *pushButton_GetMode;
    QPushButton *pushButton_SetExTime;
    QLabel *label_Mode;
    QLineEdit *lineEdit_SetMode;
    QPushButton *pushButton_CFMode;
    QLineEdit *lineEdit_exTime;
    QPushButton *pushButton_SetMode;
    QLabel *label_CFMode;
    QPushButton *pushButtonRotate;
    QPushButton *pushButton_Start;
    QSpacerItem *verticalSpacer_3;
    QSpacerItem *verticalSpacer_2;
    QSpacerItem *verticalSpacer_4;
    QLabel *label_size;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(846, 656);
        MainWindow->setMinimumSize(QSize(739, 0));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName("centralWidget");
        gridLayout_3 = new QGridLayout(centralWidget);
        gridLayout_3->setSpacing(6);
        gridLayout_3->setContentsMargins(11, 11, 11, 11);
        gridLayout_3->setObjectName("gridLayout_3");
        widget_pic = new QWidget(centralWidget);
        widget_pic->setObjectName("widget_pic");
        widget_pic->setMinimumSize(QSize(641, 0));
        gridLayout = new QGridLayout(widget_pic);
        gridLayout->setSpacing(0);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(widget_pic);
        label->setObjectName("label");
        label->setStyleSheet(QString::fromUtf8("background-color: rgb(170, 255, 255);"));
        label->setAlignment(Qt::AlignBottom|Qt::AlignRight|Qt::AlignTrailing);

        gridLayout->addWidget(label, 0, 0, 1, 1);


        gridLayout_3->addWidget(widget_pic, 0, 0, 1, 1);

        widget_2 = new QWidget(centralWidget);
        widget_2->setObjectName("widget_2");
        gridLayout_2 = new QGridLayout(widget_2);
        gridLayout_2->setSpacing(6);
        gridLayout_2->setContentsMargins(11, 11, 11, 11);
        gridLayout_2->setObjectName("gridLayout_2");
        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_2->addItem(verticalSpacer, 4, 0, 1, 1);

        comboBox_CFMode = new QComboBox(widget_2);
        comboBox_CFMode->addItem(QString());
        comboBox_CFMode->addItem(QString());
        comboBox_CFMode->setObjectName("comboBox_CFMode");

        gridLayout_2->addWidget(comboBox_CFMode, 12, 0, 1, 1);

        label_exTime = new QLabel(widget_2);
        label_exTime->setObjectName("label_exTime");
        label_exTime->setAlignment(Qt::AlignCenter);

        gridLayout_2->addWidget(label_exTime, 0, 0, 1, 1);

        pushButton_getExTime = new QPushButton(widget_2);
        pushButton_getExTime->setObjectName("pushButton_getExTime");

        gridLayout_2->addWidget(pushButton_getExTime, 1, 0, 1, 1);

        pushButton_GetMode = new QPushButton(widget_2);
        pushButton_GetMode->setObjectName("pushButton_GetMode");

        gridLayout_2->addWidget(pushButton_GetMode, 6, 0, 1, 1);

        pushButton_SetExTime = new QPushButton(widget_2);
        pushButton_SetExTime->setObjectName("pushButton_SetExTime");

        gridLayout_2->addWidget(pushButton_SetExTime, 3, 0, 1, 1);

        label_Mode = new QLabel(widget_2);
        label_Mode->setObjectName("label_Mode");
        label_Mode->setAlignment(Qt::AlignCenter);

        gridLayout_2->addWidget(label_Mode, 5, 0, 1, 1);

        lineEdit_SetMode = new QLineEdit(widget_2);
        lineEdit_SetMode->setObjectName("lineEdit_SetMode");

        gridLayout_2->addWidget(lineEdit_SetMode, 7, 0, 1, 1);

        pushButton_CFMode = new QPushButton(widget_2);
        pushButton_CFMode->setObjectName("pushButton_CFMode");

        gridLayout_2->addWidget(pushButton_CFMode, 11, 0, 1, 1);

        lineEdit_exTime = new QLineEdit(widget_2);
        lineEdit_exTime->setObjectName("lineEdit_exTime");

        gridLayout_2->addWidget(lineEdit_exTime, 2, 0, 1, 1);

        pushButton_SetMode = new QPushButton(widget_2);
        pushButton_SetMode->setObjectName("pushButton_SetMode");

        gridLayout_2->addWidget(pushButton_SetMode, 8, 0, 1, 1);

        label_CFMode = new QLabel(widget_2);
        label_CFMode->setObjectName("label_CFMode");

        gridLayout_2->addWidget(label_CFMode, 10, 0, 1, 1);

        pushButtonRotate = new QPushButton(widget_2);
        pushButtonRotate->setObjectName("pushButtonRotate");

        gridLayout_2->addWidget(pushButtonRotate, 16, 0, 1, 1);

        pushButton_Start = new QPushButton(widget_2);
        pushButton_Start->setObjectName("pushButton_Start");

        gridLayout_2->addWidget(pushButton_Start, 14, 0, 1, 1);

        verticalSpacer_3 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_2->addItem(verticalSpacer_3, 13, 0, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_2->addItem(verticalSpacer_2, 9, 0, 1, 1);

        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_2->addItem(verticalSpacer_4, 15, 0, 1, 1);


        gridLayout_3->addWidget(widget_2, 0, 1, 1, 1);

        label_size = new QLabel(centralWidget);
        label_size->setObjectName("label_size");
        label_size->setMinimumSize(QSize(0, 30));

        gridLayout_3->addWidget(label_size, 1, 0, 1, 1);

        MainWindow->setCentralWidget(centralWidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        label->setText(QString());
        comboBox_CFMode->setItemText(0, QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200", nullptr));
        comboBox_CFMode->setItemText(1, QCoreApplication::translate("MainWindow", "\345\205\263\351\227\255", nullptr));

        label_exTime->setText(QString());
        pushButton_getExTime->setText(QCoreApplication::translate("MainWindow", "\350\216\267\345\217\226\346\233\235\345\205\211\346\227\266\351\227\264", nullptr));
        pushButton_GetMode->setText(QCoreApplication::translate("MainWindow", "\350\216\267\345\217\226\345\275\223\345\211\215\346\250\241\345\274\217", nullptr));
        pushButton_SetExTime->setText(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256\346\233\235\345\205\211\346\227\266\351\227\264", nullptr));
        label_Mode->setText(QString());
        pushButton_CFMode->setText(QCoreApplication::translate("MainWindow", "\350\216\267\345\217\226\350\247\246\345\217\221\346\250\241\345\274\217", nullptr));
        pushButton_SetMode->setText(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256\346\250\241\345\274\217", nullptr));
        label_CFMode->setText(QString());
        pushButtonRotate->setText(QCoreApplication::translate("MainWindow", "\346\227\213\350\275\254\345\220\247\357\274\201", nullptr));
        pushButton_Start->setText(QCoreApplication::translate("MainWindow", "\345\274\200\345\247\213\351\207\207\351\233\206", nullptr));
        label_size->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
