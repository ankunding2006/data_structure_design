@echo off
set PATH=C:\msys64\ucrt64\bin;%PATH%

if not exist "build\UAVCoverageOptimizer.exe" (
    echo [Notice] Compiling project first ...
    call build.bat
    if %ERRORLEVEL% NEQ 0 (
        echo [Error] Failed to build application.
        pause
        exit /b 1
    )
)

echo ========================================================
echo  Launching UAV Coverage & Power Optimization System (Qt6)
echo ========================================================
start "" "build\UAVCoverageOptimizer.exe"
