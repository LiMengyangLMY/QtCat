# publish_release_branch.ps1
# 生成/覆盖更新面向普通用户的单提交孤儿分支 `release`（下载即用，不包含个人书库与源码）
# 用法：
#   powershell -ExecutionPolicy Bypass -File .\scripts\publish_release_branch.ps1
#   powershell -ExecutionPolicy Bypass -File .\scripts\publish_release_branch.ps1 -Push

param(
    [switch]$Push,
    [string]$BranchName = 'release'
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$DistDir = Join-Path $ProjectRoot 'dist'
$ExePath = Join-Path $DistDir 'QtCatPet.exe'

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "未找到 $ExePath，请先运行 .\scripts\build.ps1 编译。"
}

$TmpStage = Join-Path $env:TEMP ('qtcat_release_' + [System.Guid]::NewGuid().ToString('N'))
$StageDist = Join-Path $TmpStage 'dist'

try {
    Write-Host "==> 1/4 准备打包目录: $TmpStage" -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $StageDist | Out-Null

    # 拷贝 dist 内容，排除个人数据（books/、bookshelf.csv、临时/日志文件）
    Get-ChildItem -LiteralPath $DistDir -Force | Where-Object {
        $_.Name -notin @('books', 'bookshelf.csv', 'bookshelf.csv.tmp') -and
        $_.Extension -notin @('.log', '.tmp')
    } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $StageDist -Recurse -Force
    }

    Write-Host '==> 2/4 生成启动脚本与用户说明文档' -ForegroundColor Cyan
    $Utf8NoBom = New-Object System.Text.UTF8Encoding $false

    $StartBat = @"
@echo off
chcp 65001 >nul
start "" /D "%~dp0dist" "%~dp0dist\QtCatPet.exe"
"@
    [IO.File]::WriteAllText((Join-Path $TmpStage '启动桌宠.bat'), $StartBat.Replace("`r`n", "`n").Replace("`n", "`r`n"), $Utf8NoBom)

    $ShortcutBat = @"
@echo off
chcp 65001 >nul
set "QTCAT_ROOT=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "`$root=`$env:QTCAT_ROOT.TrimEnd('\'); `$ws=New-Object -ComObject WScript.Shell; `$desk=[Environment]::GetFolderPath('Desktop'); `$lnk=Join-Path `$desk 'QtCatPet.lnk'; `$exe=Join-Path `$root 'dist\QtCatPet.exe'; `$dir=Join-Path `$root 'dist'; `$s=`$ws.CreateShortcut(`$lnk); `$s.TargetPath=`$exe; `$s.WorkingDirectory=`$dir; `$s.IconLocation=`$exe+',0'; `$s.Description='QtCat 桌面宠物'; `$s.Save(); Write-Host ('已成功创建桌面快捷方式: ' + `$lnk) -ForegroundColor Green"
pause
"@
    [IO.File]::WriteAllText((Join-Path $TmpStage '创建桌面快捷方式.bat'), $ShortcutBat.Replace("`r`n", "`n").Replace("`n", "`r`n"), $Utf8NoBom)

    $ReleaseGitIgnore = @"
dist/books/
dist/bookshelf.csv
dist/bookshelf.csv.tmp
*.log
"@
    [IO.File]::WriteAllText((Join-Path $TmpStage '.gitignore'), $ReleaseGitIgnore.Replace("`r`n", "`n"), $Utf8NoBom)

    $ReleaseReadme = @"
# QtCat 桌面宠物（下载即用版）

本分支为**打包即用发行分支**，无需安装 Qt 或任何开发环境，下载解压即可直接运行。  
*(如需查看源码或参与开发，请切换至 ``main`` 分支。)*

## 快速开始

1. 点击 GitHub 页面右上角 **Code → Download ZIP** 下载并解压到任意目录；
2. 双击 **`启动桌宠.bat`** 即可直接启动小猫；
3. 如需在桌面创建图标，双击 **`创建桌面快捷方式.bat`** 即可自动按当前路径生成带猫咪图标的桌面快捷方式。

## 使用说明

- **移动位置**：鼠标左键按住拖拽
- **旋转动画**：鼠标悬停在猫咪本体上
- **导入电子书**：支持 ``.txt`` / ``.epub`` / ``.mobi`` / ``.azw`` / ``.azw3``，直接将电子书拖到小猫身上即可导入（或右键打开“摸鱼书架”管理与删除）
- **手动翻行**：点击文本框右侧 ``▲`` / ``▼`` 按钮，或使用鼠标滚轮 / 键盘 ``J``・``K``・方向键
- **自动播放**：右键菜单 / 空格键 / 鼠标中键启停（每行 1.5 秒）
- **一键隐蔽（老板键）**：双击猫咪主体或按 ``Esc`` 键瞬间隐藏文字并停止自动播放
- **字号与颜色**：右键菜单可调白到黑颜色、多档字号与自定义字号，也支持 ``Ctrl + 滚轮`` 实时缩放
- **进度保存**：导入的电子书自动存入 ``dist/books/``，阅读进度保存在 ``dist/bookshelf.csv``（主动退出立即保存，并每 5 分钟自动兜底保存）
"@
    [IO.File]::WriteAllText((Join-Path $TmpStage 'README.md'), $ReleaseReadme.Replace("`r`n", "`n"), $Utf8NoBom)

    Write-Host "==> 3/4 生成单提交孤儿分支 '$BranchName'" -ForegroundColor Cyan
    $MainCommit = (git -C $ProjectRoot rev-parse --short HEAD).Trim()
    $Timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm'
    $GitDir = Join-Path $ProjectRoot '.git'
    $TmpIndex = "$TmpStage.index"
    $OldIndex = $env:GIT_INDEX_FILE

    try {
        $env:GIT_INDEX_FILE = $TmpIndex
        & git --git-dir="$GitDir" --work-tree="$TmpStage" add -A
        if ($LASTEXITCODE -ne 0) { throw 'git add 失败' }

        $Tree = (& git --git-dir="$GitDir" write-tree).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $Tree) { throw 'git write-tree 失败' }

        $CommitMsg = "release: build from main@$MainCommit ($Timestamp)"
        $Commit = (& git --git-dir="$GitDir" commit-tree $Tree -m $CommitMsg).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $Commit) { throw 'git commit-tree 失败' }

        & git --git-dir="$GitDir" update-ref "refs/heads/$BranchName" $Commit
        if ($LASTEXITCODE -ne 0) { throw 'git update-ref 失败' }
    }
    finally {
        $env:GIT_INDEX_FILE = $OldIndex
        Remove-Item -LiteralPath $TmpIndex -Force -ErrorAction SilentlyContinue
    }

    Write-Host "==> 4/4 本地孤儿分支 '$BranchName' 已更新完成 ($($Commit.Substring(0,7)))！" -ForegroundColor Green

    if ($Push) {
        Write-Host "==> 正在推送到 origin/$BranchName ..." -ForegroundColor Cyan
        & git -C $ProjectRoot push --force -u origin $BranchName
        if ($LASTEXITCODE -ne 0) { throw "推送 origin/$BranchName 失败" }
        Write-Host "已成功推送到 origin/$BranchName。" -ForegroundColor Green
    } else {
        Write-Host "如需推送到 GitHub，请运行: git push -f -u origin $BranchName" -ForegroundColor Yellow
    }
}
finally {
    Remove-Item -Recurse -Force $TmpStage -ErrorAction SilentlyContinue
}
