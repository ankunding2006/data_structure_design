@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul

echo ========================================================
echo   课程设计源码与成果一键打包脚本 (邮件提交专用)
echo ========================================================
echo.
echo 说明：按课程设计要求，提交格式为：“选择题号-姓名-班级-学号.zip”
echo 本脚本会自动打包所有源代码、工程文件、说明文档及对比图，
echo 并自动过滤 build 临时构建目录，确保体积在数百KB以内，适合邮件发送。
echo.

set /p pkg_name="请输入打包文件名 (直接回车默认: 第4题-肖奕扬-信通24-04242912): "
if "!pkg_name!"=="" set pkg_name=第4题-肖奕扬-信通24-04242912

if exist "!pkg_name!.zip" del "!pkg_name!.zip"

powershell -NoProfile -Command ^
  "$zipName = '!pkg_name!.zip'; " ^
  "$files = Get-ChildItem -File | Where-Object { $_.Name -notmatch '\.exe$|\.zip$|qt_coverage_preview\.png' } | Select-Object -ExpandProperty FullName; " ^
  "Compress-Archive -Path $files -DestinationPath $zipName -Force; " ^
  "Write-Host '[成功] 已生成轻量级邮件源码包:' $zipName -ForegroundColor Green; " ^
  "$size = (Get-Item $zipName).Length / 1KB; " ^
  "Write-Host ('[大小] ' + [math]::Round($size, 1) + ' KB (远低于邮件附件大小限制)') -ForegroundColor Cyan"

echo.
pause
