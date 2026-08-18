QT += core gui network concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ProtocolSimulator
TEMPLATE = app

CONFIG += c++11
DESTDIR = $$PWD/ProtocolSimulator/

INCLUDEPATH += src/core src/ui
INCLUDEPATH += $$PWD/src/ZDDS/include
LIBS += -L$$PWD/src/ZDDS/lib -lZDDSd
LIBS += -L$$PWD/src/ZDDS/lib -lzmq

system(cp -r "$$PWD/src/ZDDS/lib/lib*.so*" "$$DESTDIR")

# ===== 国际化: 翻译文件 + 构建时生成qm =====
TRANSLATIONS += translations/ProtocolSimulator_en.ts

# 先运行 build_qm.py 生成 .qm, 再让下面的 RESOURCES/qrc 能找到它
# 注: 若 python3 不可用, 请手动执行: python3 scripts/build_qm.py translations/ProtocolSimulator_en.ts
system(python3 $$PWD/scripts/build_qm.py $$PWD/translations/ProtocolSimulator_en.ts $$PWD/translations/ProtocolSimulator_en.qm)

RESOURCES += translations/translations.qrc

SOURCES += \
    src/core/ZDDSMgr.cpp \
    src/core/ZDDSProtolcol.cpp \
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
    src/core/ZDDSMgr.h \
    src/core/ZDDSProtolcol.h \
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


