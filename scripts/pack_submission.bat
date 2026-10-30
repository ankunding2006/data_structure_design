@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul

echo ========================================================
echo   课程设计源码与成果一键打包脚本 (邮件提交专用)
echo ========================================================
echo.
echo 说明：按课程设计要求，提交格式为：“选择题号-姓名-班级-学号.zip”
echo 本脚本自动归集模块化源码、工程配置、技术文档与对比效果图，
echo 并自动过滤 build/ 临时构建目录，确保体积在数百KB以内，适合邮件发送。
echo.

set /p pkg_name="请输入打包文件名 (直接回车默认: 第4题-肖弋洋-电信25-02-04242912): "
if "!pkg_name!"=="" set pkg_name=第4题-肖弋洋-电信25-02-04242912

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0pack_submission.ps1" -PkgName "!pkg_name!"

echo.
pause
