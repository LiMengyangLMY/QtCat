# extract_assets.ps1
# 从两个 mp4 中提取 PNG 素材（等价于 extract_assets.sh）
# 用法：powershell -ExecutionPolicy Bypass -File .\scripts\extract_assets.ps1

$ErrorActionPreference = 'Continue'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# 视频文件名（含中文，用 Unicode 码点拼出，避免脚本编码问题）
# 静止1.mp4  -> U+9759 静, U+6B62 止
# 旋转1.mp4  -> U+65CB 旋, U+8F6C 转
$MediaDir    = Join-Path $ProjectRoot 'media'
$IdleVideo   = Join-Path $MediaDir (-join ([char]0x9759, [char]0x6B62, '1.mp4'))
$RotateVideo = Join-Path $MediaDir (-join ([char]0x65CB, [char]0x8F6C, '1.mp4'))

$IdleDir   = Join-Path $ProjectRoot 'resources\assets\idle'
$RotateDir = Join-Path $ProjectRoot 'resources\assets\rotate'
$MatteScript = Join-Path $ScriptDir 'remove_black_background.py'

$TmpDir = Join-Path $env:TEMP ('qtcat_' + [System.Guid]::NewGuid().ToString('N'))
$TmpIdleDir = Join-Path $TmpDir 'idle'
$TmpRotateDir = Join-Path $TmpDir 'rotate'

try {
    # 先检查依赖，避免在缺少 ffmpeg 时误删已有素材
    if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
        throw '未找到 ffmpeg，请先安装并加入 PATH。'
    }

    New-Item -ItemType Directory -Force -Path $IdleDir, $RotateDir, $TmpIdleDir, $TmpRotateDir | Out-Null
    Remove-Item -Path "$IdleDir\*.png", "$RotateDir\*.png" -Force -ErrorAction SilentlyContinue

    Write-Host '==> 提取静止态 (idle)' -ForegroundColor Cyan
    & ffmpeg -y -i $IdleVideo -vf 'crop=850:760:480:170' -update 1 -frames:v 1 "$TmpIdleDir\idle_raw.png" *> $null
    if ($LASTEXITCODE -ne 0) { throw 'ffmpeg 提取静止态失败' }
    python $MatteScript "$TmpIdleDir\idle_raw.png" "$IdleDir\idle.png" --size 256
    if ($LASTEXITCODE -ne 0) { throw 'python 处理静止态失败' }

    Write-Host '==> 提取旋转序列 (rotate)' -ForegroundColor Cyan
    & ffmpeg -y -i $RotateVideo -vf 'fps=15,crop=960:600:480:140' "$TmpRotateDir\frame_%02d.png" *> $null
    if ($LASTEXITCODE -ne 0) { throw 'ffmpeg 提取旋转序列失败' }

    Get-ChildItem -Path $TmpRotateDir -Filter 'frame_*.png' | ForEach-Object {
        Write-Host "  处理 $($_.Name)"
        python $MatteScript $_.FullName (Join-Path $RotateDir $_.Name) --size 256
        if ($LASTEXITCODE -ne 0) { throw "python 处理 $($_.Name) 失败" }
    }

    Write-Host '素材提取完成。' -ForegroundColor Green
}
finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}

