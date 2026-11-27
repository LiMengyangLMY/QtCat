@echo off
chcp 65001 >nul
set "QTCAT_ROOT=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$root=$env:QTCAT_ROOT.TrimEnd('\'); $ws=New-Object -ComObject WScript.Shell; $desk=[Environment]::GetFolderPath('Desktop'); $lnk=Join-Path $desk 'QtCatPet.lnk'; $exe=Join-Path $root 'dist\QtCatPet.exe'; $dir=Join-Path $root 'dist'; $s=$ws.CreateShortcut($lnk); $s.TargetPath=$exe; $s.WorkingDirectory=$dir; $s.IconLocation=$exe+',0'; $s.Description='QtCat 桌面宠物'; $s.Save(); Write-Host ('已成功创建桌面快捷方式: ' + $lnk) -ForegroundColor Green"
pause