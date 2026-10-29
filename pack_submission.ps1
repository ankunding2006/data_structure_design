param (
    [string]$PkgName = "Question4-UAV-Optimization-Source"
)

$zipFile = "$PkgName.zip"
if (Test-Path $zipFile) {
    Remove-Item -Force $zipFile
}

$filesToPack = Get-ChildItem -File | Where-Object {
    $_.Name -notmatch '\.exe$|\.zip$|qt_coverage_preview\.png'
}

Compress-Archive -Path $filesToPack.FullName -DestinationPath $zipFile -Force

$fileInfo = Get-Item $zipFile
$sizeKB = [math]::Round($fileInfo.Length / 1KB, 1)

Write-Host "========================================================" -ForegroundColor Green
Write-Host "[Success] Package created: $zipFile" -ForegroundColor Green
Write-Host "[Size] $sizeKB KB" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Green
