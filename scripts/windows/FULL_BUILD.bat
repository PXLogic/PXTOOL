@echo off
title PXTOOL Full Build
cd /d "%~dp0\..\.."

echo.
echo ======================================
echo  PXTOOL FULL Rebuild (clean)
echo ======================================
echo.

echo [Step 0] Stopping PXTOOL if running...
taskkill /F /IM PXTOOL.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   Stopped running PXTOOL.exe
    timeout /t 1 /nobreak >nul
)

echo [Step 0b] Cleaning old build artifacts...
rmdir /s /q build.dir\CMakeFiles 2>nul
del /f /q build.dir\CMakeCache.txt 2>nul
del /f /q build.dir\PXTOOL.exe 2>nul
echo Done.
echo.

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

echo.
echo [Package] Creating release zip...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$root = '%CD%'; " ^
  "$cmake = Get-Content \"$root\CMakeLists.txt\" -Raw; " ^
  "$major = [regex]::Match($cmake, 'set\(DS_VERSION_MAJOR\s+(\S+)\)').Groups[1].Value; " ^
  "$minor = [regex]::Match($cmake, 'set\(DS_VERSION_MINOR\s+(\S+)\)').Groups[1].Value; " ^
  "$micro = [regex]::Match($cmake, 'set\(DS_VERSION_MICRO\s+(\S+)\)').Groups[1].Value; " ^
  "$ver = \"$major.$minor.$micro\"; " ^
  "$zipName = \"PXTOOL-$ver-win64.zip\"; " ^
  "$zipPath = \"$root\$zipName\"; " ^
  "Write-Host \"  Version : $ver\"; " ^
  "Write-Host \"  Output  : $zipPath\"; " ^
  "Get-ChildItem \"$root\" -Filter 'PXTOOL-*-win64.zip' | ForEach-Object { Remove-Item $_.FullName -Force; Write-Host \"  Deleted : $($_.Name)\" }; " ^
  "$buildDir = \"$root\build.dir\"; " ^
  "if (-not (Test-Path $buildDir)) { Write-Host 'ERROR: build.dir not found'; exit 1 }; " ^
  "Compress-Archive -Path \"$buildDir\*\" -DestinationPath $zipPath -CompressionLevel Optimal; " ^
  "Write-Host \"  Done: $zipName ($([math]::Round((Get-Item $zipPath).Length/1MB,1)) MB)\""

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Packaging failed.
    echo.
    pause
    exit /b 1
)

echo.
echo ======================================
echo  All done!
echo ======================================
echo.
pause
