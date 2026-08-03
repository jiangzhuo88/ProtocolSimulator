QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ProtocolSimulator
TEMPLATE = app

CONFIG += c++11
DESTDIR = $$PWD/bin/
SOURCES += \
    src/main.cpp \
    src/core/protocoltypes.cpp \
    src/core/configmanager.cpp \
    src/core/simconnection.cpp \
    src/core/simtcpserver.cpp \
    src/ui/mainwindow.cpp \
    src/ui/protocoleditdialog.cpp \
    src/ui/scenemanagedialog.cpp \
    src/ui/collapsiblegroupbox.cpp \
    src/ui/WheelEventFilter.cpp \
    src/core/ZTextEdit.cpp

HEADERS += \
    src/core/protocoltypes.h \
    src/core/configmanager.h \
    src/core/simconnection.h \
    src/core/simtcpserver.h \
    src/ui/mainwindow.h \
    src/ui/protocoleditdialog.h \
    src/ui/scenemanagedialog.h \
    src/ui/collapsiblegroupbox.h \
    src/ui/WheelEventFilter.h \
    src/core/ZTextEdit.h

INCLUDEPATH += src/core src/ui
