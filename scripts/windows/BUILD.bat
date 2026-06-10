@echo off
title PXTOOL Build
cd /d "%~dp0\..\.."

echo.
echo ======================================
echo  PXTOOL Incremental Build
echo ======================================
echo.

taskkill /F /IM PXTOOL.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Stopped running PXTOOL.exe
    timeout /t 1 /nobreak >nul
)

C:\msys64\usr\bin\bash.exe --login -c "cd \"$(cygpath -u '%CD%')\" && bash scripts/windows/build_script.sh"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed. See output above.
    echo.
    pause
    exit /b 1
)

echo.
echo [OK] Build succeeded.
echo.

echo [Deploy] Copying runtime dependencies...
C:\msys64\usr\bin\bash.exe --login -c "cd \"$(cygpath -u '%CD%')\" && bash scripts/windows/deploy_script.sh"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Deploy failed. See output above.
    echo.
    pause
    exit /b 1
)

echo.
echo [OK] Deploy succeeded.
echo.
pause
