param (
    [string]$PkgName = ""
)

if ([string]::IsNullOrWhiteSpace($PkgName)) {
    $PkgName = [char]0x7B2C + "4" + [char]0x9898 + "-" + [char]0x8096 + [char]0x5F0B + [char]0x6D0B + "-" + [char]0x7535 + [char]0x4FE1 + "25-02-04242912"
}

$suffixSrc = [char]0x6E90 + [char]0x7801 + [char]0x62A5 + [char]0x544A + [char]0x7248
$suffixDist = [char]0x514D + [char]0x5B89 + [char]0x88C5 + [char]0x8FD0 + [char]0x884C + [char]0x5B8C + [char]0x6574 + [char]0x7248

Add-Type -AssemblyName System.IO.Compression.FileSystem

$rootPath = (Get-Item .).FullName

# -------------------------------------------------------------
# 1. 生成纯源码与报告归档包 (供代码审查、查重与极速上传)
# -------------------------------------------------------------
$zipFileSrc = Join-Path $rootPath "$PkgName-$suffixSrc.zip"
if (Test-Path $zipFileSrc) { Remove-Item -Force $zipFileSrc }

$excludePatternsSrc = @(
    '\.exe$',
    '\.zip$',
    '\.git',
    '\\build\\',
    '\\build$',
    '\\dist\\',
    '\\dist$',
    '\.ninja',
    'CMakeCache\.txt',
    'CMakeFiles'
)

$allFilesSrc = Get-ChildItem -Path $rootPath -Recurse -File | Where-Object {
    $filePath = $_.FullName
    $match = $false
    foreach ($p in $excludePatternsSrc) {
        if ($filePath -match $p) {
            $match = $true
            break
        }
    }
    -not $match
}

$tempDirSrc = Join-Path $env:TEMP ("pkg_src_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempDirSrc | Out-Null

foreach ($file in $allFilesSrc) {
    $relPath = $file.FullName.Substring($rootPath.Length).TrimStart('\', '/')
    $destFile = Join-Path $tempDirSrc $relPath
    $destSubDir = Split-Path $destFile -Parent
    if (-not (Test-Path $destSubDir)) {
        New-Item -ItemType Directory -Path $destSubDir -Force | Out-Null
    }
    Copy-Item -Path $file.FullName -Destination $destFile -Force
}

[System.IO.Compression.ZipFile]::CreateFromDirectory($tempDirSrc, $zipFileSrc, [System.IO.Compression.CompressionLevel]::Optimal, $false)
Remove-Item -Recurse -Force $tempDirSrc
$sizeSrcKB = [math]::Round((Get-Item $zipFileSrc).Length / 1KB, 1)

# -------------------------------------------------------------
# 2. 生成免环境独立运行完整包 (含绿色独立二进制发行版，解压双击直接运行)
# -------------------------------------------------------------
$zipFileDist = Join-Path $rootPath "$PkgName-$suffixDist.zip"
if (Test-Path $zipFileDist) { Remove-Item -Force $zipFileDist }

$excludePatternsDist = @(
    '\.zip$',
    '\.git',
    '\\build\\',
    '\\build$',
    '\.ninja',
    'CMakeCache\.txt',
    'CMakeFiles'
)

$allFilesDist = Get-ChildItem -Path $rootPath -Recurse -File | Where-Object {
    $filePath = $_.FullName
    $match = $false
    foreach ($p in $excludePatternsDist) {
        if ($filePath -match $p) {
            $match = $true
            break
        }
    }
    -not $match
}

$tempDirDist = Join-Path $env:TEMP ("pkg_dist_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempDirDist | Out-Null

foreach ($file in $allFilesDist) {
    $relPath = $file.FullName.Substring($rootPath.Length).TrimStart('\', '/')
    $destFile = Join-Path $tempDirDist $relPath
    $destSubDir = Split-Path $destFile -Parent
    if (-not (Test-Path $destSubDir)) {
        New-Item -ItemType Directory -Path $destSubDir -Force | Out-Null
    }
    Copy-Item -Path $file.FullName -Destination $destFile -Force
}

[System.IO.Compression.ZipFile]::CreateFromDirectory($tempDirDist, $zipFileDist, [System.IO.Compression.CompressionLevel]::Optimal, $false)
Remove-Item -Recurse -Force $tempDirDist
$sizeDistMB = [math]::Round((Get-Item $zipFileDist).Length / 1MB, 2)

Write-Host "========================================================" -ForegroundColor Green
Write-Host "[Success] 交付压缩包生成完毕：" -ForegroundColor Green
Write-Host " 1. 源码报告版 : $zipFileSrc ($sizeSrcKB KB)" -ForegroundColor Cyan
Write-Host " 2. 免安装完整版 : $zipFileDist ($sizeDistMB MB)" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Green
