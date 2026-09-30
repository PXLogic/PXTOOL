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
for %%D in (CMakeFiles PXTOOL) do (
    if exist "build.windows\%%D\" (
        rmdir /s /q "build.windows\%%D" 2>nul
        if exist "build.windows\%%D\" (
            echo [ERROR] Could not remove stale build directory: build.windows\%%D
            exit /b 1
        )
    )
)
for /d %%D in ("build.windows\install-webui-check-*") do (
    rmdir /s /q "%%~fD" 2>nul
    if exist "%%~fD\" (
        echo [ERROR] Could not remove stale install verification directory: %%~fD
        exit /b 1
    )
)
for %%D in (plugins accessible assetimporters platforms platforminputcontexts platformthemes imageformats iconengines styles generic geoservices multimedia positioning qml qmltooling renderers sceneparsers sensors texttospeech virtualkeyboard webview tls bearer canbus printsupport sqldrivers networkinformation xcbglintegrations egldeviceintegrations wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration translations) do (
    rmdir /s /q "build.windows\%%D" 2>nul
    if exist "build.windows\%%D\" (
        echo [ERROR] Could not remove stale deployment directory: build.windows\%%D
        exit /b 1
    )
)
for %%F in (CMakeCache.txt PXTOOL.exe qt.conf) do (
    if exist "build.windows\%%F" (
        del /f /q "build.windows\%%F" 2>nul
        if exist "build.windows\%%F" (
            echo [ERROR] Could not remove stale build artifact: build.windows\%%F
            exit /b 1
        )
    )
)
if exist "build.windows\Qt*.dll" (
    del /f /q build.windows\Qt*.dll 2>nul
    if exist "build.windows\Qt*.dll" (
        echo [ERROR] Could not remove stale Qt runtime DLLs from build.windows
        exit /b 1
    )
)
echo Done.
echo.

set "MSYS2_PATH_TYPE=inherit"
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
echo [Package] Creating release zip...
REM CPack stages the package through the install() rules in CMakeLists.txt, so
REM the ZIP contains only the runtime tree. It deliberately does not archive
REM build.windows: that directory also holds the CMake build tree (_deps with a
REM full nlohmann/json git clone, CMakeFiles, moc/qrc output, test binaries) plus
REM every DLL any previous deployment ever left behind.
REM Run through MSYS2 so windeployqt6 and objdump are on PATH during install.
C:\msys64\usr\bin\bash.exe --login -c "cd \"$(cygpath -u '%CD%')\" && bash scripts/windows/package_script.sh"

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
