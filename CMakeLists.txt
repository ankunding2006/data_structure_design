cmake_minimum_required(VERSION 3.16)
project(UAVCoverageOptimizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include_directories(src)

# 1. 核心业务与算法库 (Core Library)
add_library(uav_core STATIC
    src/core/uav_model.h
    src/core/uav_model.cpp
)
target_include_directories(uav_core PUBLIC src)

# 2. 工业级 Qt 桌面交互仿真平台
add_executable(UAVCoverageOptimizer
    src/gui/main_gui.cpp
    src/gui/mainwindow.h
    src/gui/mainwindow.cpp
    src/gui/canvas.h
    src/gui/canvas.cpp
)
target_link_libraries(UAVCoverageOptimizer PRIVATE
    uav_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# 3. 独立纯 C++ 命令行求解评测工具
add_executable(uav_solver
    src/cli/main.cpp
)
target_link_libraries(uav_solver PRIVATE
    uav_core
)
