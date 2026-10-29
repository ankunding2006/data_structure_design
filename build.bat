@echo off
echo [Building] Compiling main.cpp with g++ ...
g++ -O2 -Wall main.cpp -o uav_solver.exe
if %ERRORLEVEL% NEQ 0 (
    echo [Error] Compilation failed!
    pause
    exit /b %ERRORLEVEL%
)
echo [Success] Compilation succeeded: uav_solver.exe generated!
