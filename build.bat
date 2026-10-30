@echo off
setlocal
chcp 65001 >nul

echo ========================================================
echo  [1/2] 配置与编译 UAVCoverageOptimizer (CMake + Ninja)
echo ========================================================
set PATH=C:\msys64\ucrt64\bin;%PATH%

cmake -B build -G "Ninja" -DCMAKE_MAKE_PROGRAM="C:/msys64/ucrt64/bin/ninja.exe" -DCMAKE_CXX_COMPILER="C:/msys64/ucrt64/bin/g++.exe" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [错误] CMake 配置失败！
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  [2/2] 正在构建项目核心静态库与目标执行程序...
echo ========================================================
cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 编译构建失败！
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [成功] 编译构建完成！生成目标文件：
echo   - build\UAVCoverageOptimizer.exe (Qt6 工业级遥测交互平台)
echo   - build\uav_solver.exe           (独立纯 C++ 算法评测工具)
echo.
pause
