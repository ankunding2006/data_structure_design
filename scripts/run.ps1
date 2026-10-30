# PowerShell execution script
if (-not (Test-Path ".\uav_solver.exe")) {
    Write-Host "[Notice] Compiling main.cpp first ..." -ForegroundColor Yellow
    g++ -O2 -Wall main.cpp -o uav_solver.exe
}

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host " 1. Running C++ Core Solver (Output in terminal)" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan
.\uav_solver.exe 42 16 20.0 100.0

Write-Host ""
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host " 2. Launching Graphical User Interface (GUI)" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan
python gui.py
