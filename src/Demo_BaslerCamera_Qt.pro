QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Demo_BaslerCamera_Qt
TEMPLATE = app

CONFIG   += c++11

#--------------------------------------------Basler-------------------------------------------
INCLUDEPATH += $$PWD/inc
LIBS += -L$$PWD/lib/Win32 -lGCBase_MD_VC120_v3_0_Basler_pylon_v5_0 -lGenApi_MD_VC120_v3_0_Basler_pylon_v5_0 -lPylonBase_MD_VC120_v5_0 -lPylonC_MD_VC120 -lPylonGUI_MD_VC120_v5_0 -lPylonUtility_MD_VC120_v5_0
#----------------------------------------------------------------------------------------------

CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/bin_debug
    LIBS += -L$$PWD/bin_debug
}
CONFIG(release, debug|release) {
    DESTDIR = $$PWD/bin_release
    LIBS += -L$$PWD/bin_release
}

SOURCES += src/main.cpp\
        src/mainwindow.cpp \
    src/grab_images2.cpp

HEADERS  += src/mainwindow.h \
    src/grab_images2.h

FORMS    += src/mainwindow.ui
