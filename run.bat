@echo off
setlocal
chcp 65001 >nul

set PATH=C:\msys64\ucrt64\bin;%PATH%

if not exist "build\UAVCoverageOptimizer.exe" (
    echo [提示] 检测到尚未编译，正在自动执行构建...
    call build.bat
    if %ERRORLEVEL% NEQ 0 (
        echo [错误] 构建失败，无法启动程序。
        pause
        exit /b 1
    )
)

echo ========================================================
echo  正在启动 无人机地面基站覆盖与功率优化系统 [Qt6 平台]...
echo ========================================================
start "" "build\UAVCoverageOptimizer.exe"
