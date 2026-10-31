@chcp 65001 >nul
@echo off
setlocal enabledelayedexpansion
title 无人机地面基站覆盖与功率优化系统 - 工业级启动引导

echo =======================================================================
echo          无人机地面基站覆盖与功率优化系统 (免安装独立运行版)
echo          作者：肖弋洋 (04242912)  班级：电子信息工程25-02班
echo          中国矿业大学信息与控制工程学院  2026年秋
echo =======================================================================
echo.
echo [提示] 本运行版已封装全部独立依赖库，无需配置任何 Qt 或 C++ 编译器环境！
echo.
echo 请选择要启动的功能模式：
echo.
echo   [1] 启动工业级 Qt6 遥测交互图形界面平台 (推荐演示)
echo   [2] 启动独立纯 C++ 算法控制台评测验证工具 (快速跑分)
echo   [3] 退出
echo.
set "choice="
set /p choice="请输入数字编号 [1/2/3] (直接回车默认 1): "

if not defined choice set choice=1
set "choice=%choice: =%"

if "%choice%"=="1" goto run_gui
if "%choice%"=="2" goto run_cli
if "%choice%"=="3" goto exit_app

echo.
echo [提示] 无效输入，默认启动图形界面...
goto run_gui

:run_gui
echo.
echo 正在启动 Qt6 遥测态势交互系统...
start "" "%~dp0dist\bin\UAVCoverageOptimizer.exe"
exit /b 0

:run_cli
echo.
echo =======================================================================
echo 正在执行纯 C++ 算法控制台核心评测：
echo =======================================================================
"%~dp0dist\bin\uav_solver.exe" 42 16 20.0 100.0
echo.
pause
exit /b 0

:exit_app
exit /b 0
