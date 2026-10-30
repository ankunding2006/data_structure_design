QT += core gui widgets

CONFIG += c++17

TARGET = UAVCoverageOptimizer
TEMPLATE = app

INCLUDEPATH += src

SOURCES += \
    src/gui/main_gui.cpp \
    src/gui/mainwindow.cpp \
    src/gui/canvas.cpp \
    src/core/uav_model.cpp

HEADERS += \
    src/gui/mainwindow.h \
    src/gui/canvas.h \
    src/core/uav_model.h
