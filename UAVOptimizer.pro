QT += core gui widgets

CONFIG += c++17

TARGET = UAVCoverageOptimizer
TEMPLATE = app

SOURCES += \
    main_gui.cpp \
    mainwindow.cpp \
    canvas.cpp \
    uav_model.cpp

HEADERS += \
    mainwindow.h \
    canvas.h \
    uav_model.h
