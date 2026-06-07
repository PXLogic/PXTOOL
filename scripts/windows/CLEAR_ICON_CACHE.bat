@echo off
REM Refresh Windows shell icon cache so PXTOOL.exe shows the new embedded icon.
echo Closing Explorer to clear icon cache...
taskkill /f /im explorer.exe >nul 2>&1
del /a /q "%localappdata%\Microsoft\Windows\Explorer\iconcache_*.db" >nul 2>&1
del /a /q "%localappdata%\Microsoft\Windows\Explorer\thumbcache_*.db" >nul 2>&1
start explorer.exe
echo Done. Open build.windows\PXTOOL.exe from Explorer (not an old shortcut).
pause
