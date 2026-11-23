# build.ps1
# 完整构建：提取素材 -> qmake -> mingw32-make -> windeployqt 打包到 dist
# 用法：powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 [-SkipAssets]
#   -SkipAssets  跳过素材提取，直接使用 resources\assets 中已有的 PNG（无需 ffmpeg / Python）

param(
    [switch]$SkipAssets
)

$ErrorActionPreference = 'Continue'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$QtRoot = Join-Path $ProjectRoot 'third_party\qt'

$QtBin = Join-Path $QtRoot '5.15.2\mingw81_64\bin'
$MingwBin = Join-Path $QtRoot 'Tools\mingw810_64\bin'
$QMake = Join-Path $QtBin 'qmake.exe'
$Make = Join-Path $MingwBin 'mingw32-make.exe'
$WinDeployQt = Join-Path $QtBin 'windeployqt.exe'

$BuildDir = Join-Path $ProjectRoot 'build'
$DistDir = Join-Path $ProjectRoot 'dist'
$TargetExe = Join-Path $DistDir 'QtCatPet.exe'

if (-not (Test-Path -LiteralPath $QMake)) {
    Write-Host '未找到项目内 Qt 工具链，请先运行: .\scripts\install_tools.ps1' -ForegroundColor Yellow
    exit 1
}
if (-not (Test-Path -LiteralPath $Make)) {
    Write-Host '未找到 MinGW 工具链，请先运行: .\scripts\install_tools.ps1' -ForegroundColor Yellow
    exit 1
}

# 1. 提取素材
Write-Host '==> 步骤 1/4: 提取素材' -ForegroundColor Cyan
if ($SkipAssets) {
    Write-Host '已指定 -SkipAssets，跳过素材提取。' -ForegroundColor Yellow
} else {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $ScriptDir 'extract_assets.ps1')
    if ($LASTEXITCODE -ne 0) { throw '素材提取失败' }
}

# 2. 配置编译环境
$env:PATH = "$QtBin;$MingwBin;$env:PATH"

New-Item -ItemType Directory -Force -Path $BuildDir, $DistDir | Out-Null

# 3. qmake + make
Write-Host '==> 步骤 2/4: qmake' -ForegroundColor Cyan
Push-Location $BuildDir
try {
    & $QMake "..\DesktopPet.pro"
    if ($LASTEXITCODE -ne 0) { throw 'qmake 失败' }

    Write-Host '==> 步骤 3/4: mingw32-make' -ForegroundColor Cyan
    & $Make -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) { throw 'make 失败' }
}
finally {
    Pop-Location
}

$BuiltExe = Join-Path $BuildDir 'release\QtCatPet.exe'
if (-not (Test-Path -LiteralPath $BuiltExe)) {
    $BuiltExe = Join-Path $BuildDir 'QtCatPet.exe'
}
if (-not (Test-Path -LiteralPath $BuiltExe)) {
    throw "未找到编译产物: $BuiltExe"
}

Copy-Item -Force $BuiltExe $DistDir

# 4. windeployqt 打包 Qt 运行库
Write-Host '==> 步骤 4/4: windeployqt 打包运行库' -ForegroundColor Cyan
& $WinDeployQt $TargetExe
if ($LASTEXITCODE -ne 0) { throw 'windeployqt 失败' }

Write-Host ''
Write-Host "构建完成: $TargetExe" -ForegroundColor Green
