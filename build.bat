@echo off
echo ========================================================
echo  [1/2] Configuring C++ Qt6 Project with CMake and Ninja
echo ========================================================
set PATH=C:\msys64\ucrt64\bin;%PATH%

cmake -B build -G "Ninja" -DCMAKE_MAKE_PROGRAM="C:/msys64/ucrt64/bin/ninja.exe" -DCMAKE_CXX_COMPILER="C:/msys64/ucrt64/bin/g++.exe" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [Error] CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo  [2/2] Building UAVCoverageOptimizer.exe and uav_solver.exe
echo ========================================================
cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo [Error] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [Success] Compilation succeeded! Target executables:
echo   - build\UAVCoverageOptimizer.exe (Industrial Qt6 GUI)
echo   - build\uav_solver.exe (Standalone CLI Solver)
