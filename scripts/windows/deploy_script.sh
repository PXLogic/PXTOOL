#!/bin/bash
# =============================================================================
# PXTOOL Deploy Script
# Copies all runtime dependencies to build.windows after compilation.
# Run this once after BUILD, or when dependencies change.
# =============================================================================

# Resolve the MinGW64 prefix.
# /mingw64 is the canonical path inside a MinGW64 shell, but its /lib may not
# be fully exposed in all shell environments; fall back to the absolute path.
if [ -d /mingw64/lib ] && ls /mingw64/lib/python* >/dev/null 2>&1; then
    MINGW_PREFIX=/mingw64
elif [ -d /c/msys64/mingw64/lib ] && ls /c/msys64/mingw64/lib/python* >/dev/null 2>&1; then
    MINGW_PREFIX=/c/msys64/mingw64
else
    MINGW_PREFIX=/mingw64  # last resort
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SOURCE_DIR/build.windows"

cd "$BUILD_DIR" || { echo "ERROR: build.windows not found"; exit 1; }

if [ ! -f PXTOOL.exe ]; then
    echo "ERROR: PXTOOL.exe not found in $BUILD_DIR"
    echo "Please run scripts/windows/BUILD.bat first."
    exit 1
fi

echo ""
echo "======================================"
echo "PXTOOL Deploy - Runtime Dependencies"
echo "======================================"
echo ""

# --------------------------------------------------------------------------
# Step 1: Runtime DLL dependencies (MinGW64)
# Use ldd to find exact dependencies; fall back to copying all MinGW64 DLLs
# if ldd is not available in the current shell environment.
# --------------------------------------------------------------------------
echo "[1/6] Copying runtime DLL dependencies..."
COPIED=0
SKIPPED=0

# Try ldd first (works inside MinGW64 shell)
LDD_LINES=$(ldd PXTOOL.exe 2>/dev/null | grep '/mingw64' | awk '{print $3}' | grep -v '^$' | wc -l)

if [ "$LDD_LINES" -gt 0 ]; then
    # Precise mode: only copy what ldd says is needed
    while IFS= read -r dll_path; do
        dll_name=$(basename "$dll_path")
        if [ ! -f "./$dll_name" ]; then
            cp "$dll_path" "./$dll_name" && COPIED=$((COPIED+1))
        else
            SKIPPED=$((SKIPPED+1))
        fi
    done < <(ldd PXTOOL.exe 2>/dev/null | grep '/mingw64' | awk '{print $3}' | grep -v '^$')
    echo "  -> Copied: $COPIED DLLs, Skipped (already present): $SKIPPED"
else
    # Fallback: copy all MinGW64 DLLs (broader but safe)
    echo "  -> ldd not available, copying all MinGW64 DLLs as fallback..."
    for dll in "$MINGW_PREFIX/bin/"*.dll; do
        dll_name=$(basename "$dll")
        if [ ! -f "./$dll_name" ]; then
            cp "$dll" "./$dll_name" && COPIED=$((COPIED+1))
        else
            SKIPPED=$((SKIPPED+1))
        fi
    done
    echo "  -> Copied: $COPIED DLLs, Skipped (already present): $SKIPPED"
fi

# --------------------------------------------------------------------------
# Step 2: Qt5 platform plugins
# --------------------------------------------------------------------------
echo "[2/6] Copying Qt5 plugins..."
if [ ! -d plugins ]; then
    cp -r "$MINGW_PREFIX/share/qt5/plugins" ./plugins
    echo "  -> Qt plugins copied from $MINGW_PREFIX/share/qt5/plugins"
else
    echo "  -> plugins/ already present, skipping."
fi

# --------------------------------------------------------------------------
# Step 3: qt.conf (tells Qt where to find plugins relative to exe)
# --------------------------------------------------------------------------
echo "[3/6] Writing qt.conf..."
cat > qt.conf << 'EOF'
[Paths]
Prefix = .
Plugins = ./plugins
EOF
echo "  -> qt.conf written."

# --------------------------------------------------------------------------
# Step 4: Resource directories (res, demo, themes)
# Always sync with rsync (or cp -r --update as fallback) so that changes
# in the source tree are reflected in build.windows without a full clean.
# --------------------------------------------------------------------------
echo "[4/6] Syncing resource directories..."

# Helper: sync a source dir to a destination dir, always propagating updates.
sync_dir() {
    local src="$1" dst="$2" label="$3"
    if [ ! -d "$src" ]; then
        echo "  -> WARNING: $label source not found at $src, skipping."
        return
    fi
    if command -v rsync &>/dev/null; then
        rsync -a --delete "$src/" "$dst/"
        echo "  -> $label synced via rsync"
    else
        rm -rf "$dst"
        cp -r "$src" "$dst"
        echo "  -> $label copied (rsync unavailable, used cp)"
    fi
}

sync_dir "$SOURCE_DIR/PXTOOL/res"    ./res    "res/ (firmware & device configs)"
sync_dir "$SOURCE_DIR/PXTOOL/demo"   ./demo   "demo/ (demo pattern files)"
sync_dir "$SOURCE_DIR/PXTOOL/themes" ./themes "themes/"

# Note: translations are embedded inside PXTOOL.exe as Qt resources
# (language.qrc → qrc_language.cpp).  There is no separate lang/ directory
# needed at runtime; the block below is kept only for forward-compatibility
# in case a disk-based loader is added later.
if [ -d "$SOURCE_DIR/PXTOOL/lang" ]; then
    sync_dir "$SOURCE_DIR/PXTOOL/lang" ./lang "lang/ (optional disk translations)"
fi

# --------------------------------------------------------------------------
# Step 5: Python protocol decoders (libsigrokdecode)
# --------------------------------------------------------------------------
echo "[5/6] Copying Python decoders..."
if [ ! -d decoders ]; then
    cp -r "$SOURCE_DIR/libsigrokdecode/decoders" ./decoders
    # Remove non-Python files that cause "Failed to load decoder" errors
    rm -f ./decoders/文件夹.bat ./decoders/subfolders_list.txt 2>/dev/null || true
    DECODER_COUNT=$(find ./decoders -name "pd.py" | wc -l)
    echo "  -> decoders/ copied ($DECODER_COUNT Python decoders found)"
else
    echo "  -> decoders/ already present, skipping."
fi

# --------------------------------------------------------------------------
# Step 6: Bundle Python standard library
# Python's stdlib must be present alongside the app so that no system-wide
# Python installation is needed on the end-user's machine.
# The app's PYTHONHOME is set to <app_dir> so Python looks for stdlib at
# <app_dir>/lib/pythonX.Y/
# --------------------------------------------------------------------------
echo "[6/7] Bundling Python standard library..."

# Detect the Python version from the DLL already in build.windows
PY_VER=$(ls libpython3.*.dll 2>/dev/null | grep -oP '3\.\d+' | head -1)

if [ -z "$PY_VER" ]; then
    echo "  -> WARNING: Could not detect Python version from libpython*.dll"
else
    PY_SRC="$MINGW_PREFIX/lib/python${PY_VER}"
    PY_DST="./lib/python${PY_VER}"

    if [ ! -d "$PY_SRC" ]; then
        echo "  -> WARNING: Python stdlib not found at $PY_SRC"
    elif [ ! -d "$PY_DST/encodings" ]; then
        # If encodings is missing, the previous copy was incomplete — redo it
        echo "  -> lib/python${PY_VER}/ incomplete (encodings missing), re-copying..."
        rm -rf "$PY_DST"
        mkdir -p "$PY_DST"
        cp -r "$PY_SRC"/* "$PY_DST/" 2>/dev/null || true
        # Clean up test suites and cache to save space
        find "$PY_DST" -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
        find "$PY_DST" -type d -name test -exec rm -rf {} + 2>/dev/null || true
        find "$PY_DST" -type d -name tests -exec rm -rf {} + 2>/dev/null || true
        PY_SIZE=$(du -sh "$PY_DST" 2>/dev/null | cut -f1)
        echo "  -> Python ${PY_VER} stdlib bundled to lib/python${PY_VER}/ (${PY_SIZE})"
    else
        echo "  -> lib/python${PY_VER}/ already present and complete, skipping."
    fi
fi

# --------------------------------------------------------------------------
# App icon beside executable (QApplication::setWindowIcon loads this file)
# --------------------------------------------------------------------------
if [ -f "$SOURCE_DIR/win-app-logo.ico" ]; then
    cp -f "$SOURCE_DIR/win-app-logo.ico" ./win-app-logo.ico
    echo "  -> win-app-logo.ico copied beside PXTOOL.exe"
fi

# --------------------------------------------------------------------------
# Step 7: C decoders (compiled .dll files)
# --------------------------------------------------------------------------
echo "[7/7] Setting up C decoders..."
mkdir -p cdecoders
# Copy spi.dll built by CMake (lives in build.windows root after build)
if [ -f spi.dll ]; then
    cp -f spi.dll cdecoders/spi.dll
    echo "  -> spi.dll -> cdecoders/spi.dll"
else
    echo "  -> WARNING: spi.dll not found in build.windows (C decoders may not show [C]/[Py] options)"
fi

# --------------------------------------------------------------------------
# Done
# --------------------------------------------------------------------------
echo ""
echo "======================================"
echo "Deployment complete!"
echo "Run PXTOOL: $BUILD_DIR/PXTOOL.exe"
echo "======================================"
echo ""