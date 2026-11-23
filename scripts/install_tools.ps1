# install_tools.ps1
# 一次性环境安装：将 Qt 5.15.2 (MinGW) 与所需 Python 包安装到项目内 third_party\qt
# 用法：powershell -ExecutionPolicy Bypass -File .\scripts\install_tools.ps1

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$QtRoot = Join-Path $ProjectRoot 'third_party\qt'

Write-Host '==> 安装 Python 依赖 (aqtinstall, Pillow)' -ForegroundColor Cyan
python -m pip install --disable-pip-version-check aqtinstall Pillow

Write-Host "==> Qt 安装目录: $QtRoot" -ForegroundColor Cyan

$QMake = Join-Path $QtRoot '5.15.2\mingw81_64\bin\qmake.exe'
if (Test-Path -LiteralPath $QMake) {
    Write-Host 'Qt 5.15.2 已存在，跳过下载。' -ForegroundColor Green
} else {
    Write-Host '==> 下载 Qt 5.15.2 (win64_mingw81)，约 1GB，请耐心等待...' -ForegroundColor Cyan
    python -m aqt install-qt windows desktop 5.15.2 win64_mingw81 -O $QtRoot
}

$MingwMake = Join-Path $QtRoot 'Tools\mingw810_64\bin\mingw32-make.exe'
if (Test-Path -LiteralPath $MingwMake) {
    Write-Host 'MinGW 工具链已存在，跳过下载。' -ForegroundColor Green
} else {
    Write-Host '==> 下载 MinGW 8.1.0 工具链...' -ForegroundColor Cyan
    python -m aqt install-tool windows desktop tools_mingw81 -O $QtRoot
}

Write-Host ''
Write-Host '安装完成。目录结构：' -ForegroundColor Green
Write-Host "  Qt bin     : $(Join-Path $QtRoot '5.15.2\mingw81_64\bin')"
Write-Host "  MinGW bin  : $(Join-Path $QtRoot 'Tools\mingw810_64\bin')"
Write-Host '下一步运行: .\scripts\build.ps1'
