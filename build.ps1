# PowerShell build script
Write-Host "[Building] Compiling main.cpp with g++ ..." -ForegroundColor Cyan
g++ -O2 -Wall main.cpp -o uav_solver.exe
if ($LASTEXITCODE -eq 0) {
    Write-Host "[Success] Compilation succeeded: uav_solver.exe generated!" -ForegroundColor Green
} else {
    Write-Host "[Error] Compilation failed!" -ForegroundColor Red
}
