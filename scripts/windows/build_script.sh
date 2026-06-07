#!/bin/bash
# =============================================================================
# DSView Build Script (compile only)
# Runs CMake configuration + make compilation inside MinGW64 environment.
# After this, run deploy_script.sh to copy runtime dependencies.
# =============================================================================

export MSYS2_PATH_TYPE=inherit
export MINGW_PREFIX=/mingw64
export PATH="$MINGW_PREFIX/bin:/usr/bin:/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SOURCE_DIR/build.windows"

echo ""
echo "======================================"
echo "DSView Build Script (MSYS2 MinGW64)"
echo "======================================"
echo ""

# --------------------------------------------------------------------------
# Verify required tools are present
# --------------------------------------------------------------------------
MISSING=0
for TOOL in gcc g++ cmake mingw32-make; do
    if ! command -v "$TOOL" &>/dev/null && ! [ -f "$MINGW_PREFIX/bin/${TOOL}.exe" ]; then
        echo "ERROR: Required tool not found: $TOOL"
        echo "       Expected at: $MINGW_PREFIX/bin/${TOOL}.exe"
        MISSING=1
    fi
done

if [ $MISSING -eq 1 ]; then
    echo ""
    echo "Make sure you are running this script inside MSYS2 MinGW64 environment."
    echo "Use scripts/windows/BUILD.bat to launch correctly."
    exit 1
fi

echo "Compiler : $(gcc --version | head -1)"
echo "CMake    : $("$MINGW_PREFIX/bin/cmake.exe" --version | head -1)"
echo "Make     : $("$MINGW_PREFIX/bin/mingw32-make.exe" --version | head -1)"
echo ""

cd "$SOURCE_DIR"
mkdir -p build.windows
cd build.windows

# --------------------------------------------------------------------------
# CMake configuration
# Only run CMake if CMakeCache.txt is missing or CMakeLists.txt is newer.
# --------------------------------------------------------------------------
NEED_CMAKE=0
if [ ! -f "CMakeCache.txt" ]; then
    NEED_CMAKE=1
    echo "[Step 1/2] Configuring with CMake (first time setup)..."
elif [ "$SOURCE_DIR/CMakeLists.txt" -nt "CMakeCache.txt" ]; then
    NEED_CMAKE=1
    echo "[Step 1/2] CMakeLists.txt changed — re-configuring with CMake..."
else
    echo "[Step 1/2] CMake already configured, skipping."
fi

if [ $NEED_CMAKE -eq 1 ]; then
    echo ""
    "$MINGW_PREFIX/bin/cmake.exe" "$SOURCE_DIR" \
        -G "MinGW Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_PREFIX/bin/gcc.exe" \
        -DCMAKE_CXX_COMPILER="$MINGW_PREFIX/bin/g++.exe" \
        -DCMAKE_MAKE_PROGRAM="$MINGW_PREFIX/bin/mingw32-make.exe"

    if [ $? -ne 0 ]; then
        echo ""
        echo "ERROR: CMake configuration failed."
        exit 1
    fi
fi

echo ""

# --------------------------------------------------------------------------
# Step 1b: Compile language files (.ts → .qm)
# lrelease regenerates .qm in-place inside DSView/languages/ so that
# language.qrc embeds up-to-date translations when rcc runs during make.
# Note: qt5_add_translation in CMakeLists.txt outputs to the build output dir,
# a different location than the .qm files referenced by language.qrc —
# therefore we must run lrelease here to update the source-dir .qm files.
# --------------------------------------------------------------------------
echo "[Step 1b/2] Compiling language files (.ts → .qm)..."
LRELEASE_BIN=""
for _candidate in \
        "$MINGW_PREFIX/bin/lrelease-qt5.exe" \
        "$MINGW_PREFIX/bin/lrelease.exe" \
        lrelease-qt5 lrelease; do
    if [ -f "$_candidate" ] || command -v "$_candidate" &>/dev/null; then
        LRELEASE_BIN="$_candidate"
        break
    fi
done

if [ -z "$LRELEASE_BIN" ]; then
    echo "  WARNING: lrelease not found — .qm files will NOT be regenerated."
    echo "  Install with: pacman -S mingw-w64-x86_64-qt5-tools"
else
    echo "  Using: $LRELEASE_BIN"
    QM_UPDATED=0
    for TS_FILE in "$SOURCE_DIR/DSView/languages"/*.ts; do
        [ -f "$TS_FILE" ] || continue
        QM_FILE="${TS_FILE%.ts}.qm"
        if [ ! -f "$QM_FILE" ] || [ "$TS_FILE" -nt "$QM_FILE" ]; then
            if "$LRELEASE_BIN" "$TS_FILE" -qm "$QM_FILE" -silent; then
                echo "  -> $(basename "$TS_FILE") → $(basename "$QM_FILE") [updated]"
                QM_UPDATED=$((QM_UPDATED + 1))
            else
                echo "  WARNING: lrelease failed for $(basename "$TS_FILE")"
            fi
        else
            echo "  -> $(basename "$TS_FILE") unchanged, skipping"
        fi
    done
    if [ "$QM_UPDATED" -gt 0 ]; then
        # Touch language.qrc so CMake/make detects it changed and re-runs rcc,
        # which re-embeds the updated .qm files into qrc_language.cpp.
        touch "$SOURCE_DIR/DSView/languages/language.qrc"
        echo "  -> language.qrc touched — rcc will re-embed $QM_UPDATED updated file(s)"
    else
        echo "  -> All language files are up to date."
    fi
fi
echo ""

# Stop a running PXTOOL.exe so the linker can overwrite build.windows/PXTOOL.exe
if command -v tasklist.exe &>/dev/null && command -v taskkill.exe &>/dev/null; then
    if tasklist.exe 2>/dev/null | grep -qi 'PXTOOL.exe'; then
        echo "Stopping running PXTOOL.exe (required to relink)..."
        taskkill.exe //F //IM PXTOOL.exe >/dev/null 2>&1 || true
        sleep 1
    fi
fi

# --------------------------------------------------------------------------
# Compilation
# Use -j1 to avoid parallel moc file conflicts on Windows.
# Capture make's real exit code via PIPESTATUS.
# --------------------------------------------------------------------------
echo "[Step 2/2] Compiling DSView..."
echo ""

# applogo.rc.obj is not always rebuilt when only win-app-logo.ico changes; force it.
RC_OBJ="$BUILD_DIR/CMakeFiles/DSView.dir/applogo.rc.obj"
ICO_SRC="$SOURCE_DIR/win-app-logo.ico"
if [ -f "$ICO_SRC" ]; then
    if [ ! -f "$RC_OBJ" ] || [ "$ICO_SRC" -nt "$RC_OBJ" ] || [ "$SOURCE_DIR/applogo.rc" -nt "$RC_OBJ" ]; then
        echo "  -> Icon or applogo.rc changed; removing stale applogo.rc.obj"
        rm -f "$RC_OBJ"
    fi
    if [ "$ICO_SRC" -nt "$SOURCE_DIR/DSView/DSView.qrc" ]; then
        touch "$SOURCE_DIR/DSView/DSView.qrc"
        echo "  -> win-app-logo.ico changed; touching DSView.qrc for rcc rebuild"
    fi
fi

BUILD_START=$(date +%s)
echo "Build start: $(date '+%H:%M:%S')"
echo ""

MAKE_LOG="$BUILD_DIR/make_output.log"

"$MINGW_PREFIX/bin/mingw32-make.exe" -j1 2>&1 | tee "$MAKE_LOG"
MAKE_EXIT=${PIPESTATUS[0]}

BUILD_END=$(date +%s)
BUILD_TIME=$((BUILD_END - BUILD_START))

echo ""
echo "======================================"

if [ $MAKE_EXIT -ne 0 ]; then
    echo "X Build FAILED (make exit code: $MAKE_EXIT)"
    echo ""
    echo "Last errors:"
    grep -i "error:" "$MAKE_LOG" | head -10
    echo ""
    echo "Build time: $((BUILD_TIME / 60)) min $((BUILD_TIME % 60)) sec"
    echo "======================================"
    echo ""
    exit 1
elif [ -f "$BUILD_DIR/PXTOOL.exe" ]; then
    if [ -f "$SOURCE_DIR/win-app-logo.ico" ]; then
        cp -f "$SOURCE_DIR/win-app-logo.ico" "$BUILD_DIR/win-app-logo.ico"
        echo "  -> win-app-logo.ico copied to build.windows (runtime window icon)"
    fi
    echo "Build successful!"
    echo ""
    ls -lh "$BUILD_DIR/PXTOOL.exe"
    echo ""
    echo "Build time: $((BUILD_TIME / 60)) min $((BUILD_TIME % 60)) sec"
    echo "======================================"
    echo ""
    exit 0
else
    echo "X Build FAILED — PXTOOL.exe not found after make."
    echo "  See $MAKE_LOG for details."
    echo ""
    echo "Build time: $((BUILD_TIME / 60)) min $((BUILD_TIME % 60)) sec"
    echo "======================================"
    echo ""
    exit 1
fi
