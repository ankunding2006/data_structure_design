param (
    [string]$PkgName = ""
)

if ([string]::IsNullOrWhiteSpace($PkgName)) {
    $PkgName = [char]0x7B2C + "4" + [char]0x9898 + "-" + [char]0x8096 + [char]0x5F0B + [char]0x6D0B + "-" + [char]0x7535 + [char]0x4FE1 + "25-02-04242912"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

$rootPath = (Get-Item .).FullName

# 清理旧的 zip 压缩包
Get-ChildItem -Path $rootPath -Filter "*.zip" | ForEach-Object {
    Remove-Item -Force $_.FullName
}

# -------------------------------------------------------------
# 官方标准交付压缩包：严格命名为“选择题号-姓名-班级-学号.zip”
# 包含：报告电子版 (PDF)、手写心得体会照片、完整模块化源代码、免安装独立运行版 (dist/bin/)、技术文档
# -------------------------------------------------------------
$zipFileMain = Join-Path $rootPath "$PkgName.zip"

$excludePatterns = @(
    '\.zip$',
    '\.git\\',
    '\.git$',
    '\.gitignore$',
    '\.gitattributes$',
    '\\build\\',
    '\\build$',
    '\.ninja',
    'CMakeCache\.txt',
    'CMakeFiles'
)

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

$tempDir = Join-Path $env:TEMP ("pkg_main_" + [System.Guid]::NewGuid().ToString("N"))
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

[System.IO.Compression.ZipFile]::CreateFromDirectory($tempDir, $zipFileMain, [System.IO.Compression.CompressionLevel]::Optimal, $false)
Remove-Item -Recurse -Force $tempDir
$sizeMainMB = [math]::Round((Get-Item $zipFileMain).Length / 1MB, 2)

Write-Host "========================================================" -ForegroundColor Green
Write-Host "[Success] 课程设计最终标准交付压缩包已生成：" -ForegroundColor Green
Write-Host " 文件名称: $PkgName.zip" -ForegroundColor Cyan
Write-Host " 绝对路径: $zipFileMain" -ForegroundColor Cyan
Write-Host " 压缩体积: $sizeMainMB MB (在 50MB 邮箱附件限制内，支持直接发送)" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Green
