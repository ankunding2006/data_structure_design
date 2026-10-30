param (
    [string]$PkgName = ""
)

if ([string]::IsNullOrWhiteSpace($PkgName)) {
    $PkgName = [System.Text.Encoding]::UTF8.GetString([System.Text.Encoding]::UTF8.GetBytes("第4题-肖弋洋-电信25-02-04242912"))
}

$zipFile = Join-Path (Get-Location) "$PkgName.zip"
if (Test-Path $zipFile) {
    Remove-Item -Force $zipFile
}

$excludePatterns = @(
    '\.exe$',
    '\.zip$',
    '\.git',
    '\\build\\',
    '\\build$',
    '\.ninja',
    'CMakeCache\.txt',
    'CMakeFiles'
)

$rootPath = (Get-Item .).FullName
$allFiles = Get-ChildItem -Path $rootPath -Recurse -File | Where-Object {
    $filePath = $_.FullName
    $match = $false
    foreach ($p in $excludePatterns) {
        if ($filePath -match $p) {
            $match = $true
            break
        }
    }
    -not $match
}

$tempDir = Join-Path $env:TEMP ("pkg_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempDir | Out-Null

foreach ($file in $allFiles) {
    $relPath = $file.FullName.Substring($rootPath.Length).TrimStart('\', '/')
    $destFile = Join-Path $tempDir $relPath
    $destSubDir = Split-Path $destFile -Parent
    if (-not (Test-Path $destSubDir)) {
        New-Item -ItemType Directory -Path $destSubDir -Force | Out-Null
    }
    Copy-Item -Path $file.FullName -Destination $destFile -Force
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($tempDir, $zipFile, [System.IO.Compression.CompressionLevel]::Optimal, $false)

Remove-Item -Recurse -Force $tempDir

$fileInfo = Get-Item $zipFile
$sizeKB = [math]::Round($fileInfo.Length / 1KB, 1)

Write-Host "========================================================" -ForegroundColor Green
Write-Host "[Success] Package created: $zipFile" -ForegroundColor Green
Write-Host "[Size] $sizeKB KB (Clean source package, ready for email)" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Green
