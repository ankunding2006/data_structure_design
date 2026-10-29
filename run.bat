@echo off
if not exist uav_solver.exe (
    echo [Notice] Compiling main.cpp first ...
    g++ -O2 -Wall main.cpp -o uav_solver.exe
    if %ERRORLEVEL% NEQ 0 (
        echo [Error] Compilation failed!
        pause
        exit /b 1
    )
)

echo =======================================================
echo  1. Running C++ Core Solver (Output in terminal)
echo =======================================================
uav_solver.exe 42 16 20.0 100.0

echo.
echo =======================================================
echo  2. Launching Graphical User Interface (GUI)
echo =======================================================
python gui.py
