cmake_minimum_required(VERSION 3.16)
project(UAVCoverageOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 1. 工业级 Qt 桌面交互软件
add_executable(UAVCoverageOptimizer
    main_gui.cpp
    mainwindow.h
    mainwindow.cpp
    canvas.h
    canvas.cpp
    uav_model.h
    uav_model.cpp
)

target_link_libraries(UAVCoverageOptimizer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# 2. 独立纯 C++ 命令行求解器 (供终端评测验证)
add_executable(uav_solver
    main.cpp
)
