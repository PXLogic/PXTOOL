#!/bin/bash
set -euo pipefail
# =============================================================================
# PXTOOL Deploy Script
# Copies all runtime dependencies to build.windows after compilation.
# Run this once after BUILD, or when dependencies change.
# =============================================================================

# Resolve the MinGW64 prefix.
# /mingw64 is the canonical path inside a MinGW64 shell. Some installations
# expose the same tree through the absolute /c/msys64 path instead.
if [ -d /mingw64/bin ] && [ -d /mingw64/lib ]; then
    MINGW_PREFIX=/mingw64
elif [ -d /c/msys64/mingw64/bin ] && [ -d /c/msys64/mingw64/lib ]; then
    MINGW_PREFIX=/c/msys64/mingw64
else
    echo "ERROR: MinGW64 installation was not found at /mingw64 or /c/msys64/mingw64."
    exit 1
fi
export PATH="$MINGW_PREFIX/bin:/usr/bin:/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SOURCE_DIR/build.windows"
CLEANUP_STALE_INSTALL_CHECKS="$SCRIPT_DIR/cleanup_stale_install_checks.sh"

cd "$BUILD_DIR" || { echo "ERROR: build.windows not found"; exit 1; }

if [ ! -f PXTOOL.exe ]; then
    echo "ERROR: PXTOOL.exe not found in $BUILD_DIR"
    echo "Please run scripts/windows/BUILD.bat first."
    exit 1
fi

if ! bash "$CLEANUP_STALE_INSTALL_CHECKS" "$BUILD_DIR"; then
    echo "ERROR: failed to remove stale install verification directories."
    exit 1
fi

echo ""
echo "======================================"
echo "PXTOOL Deploy - Runtime Dependencies"
echo "======================================"
echo ""

# --------------------------------------------------------------------------
# Step 1: Runtime DLL dependencies (MinGW64)
# Only copy exact dependencies. A full bin-directory fallback can deploy
# legacy Qt merely because it remains installed in MSYS2.
# --------------------------------------------------------------------------
WINDEPLOYQT="$MINGW_PREFIX/bin/windeployqt6.exe"
if [ ! -x "$WINDEPLOYQT" ]; then
    echo "ERROR: Qt6 deployment tool not found: $WINDEPLOYQT"
    exit 1
fi
if ! ldd PXTOOL.exe >/dev/null 2>&1; then
    echo "ERROR: ldd is required to identify MinGW runtime dependencies."
    exit 1
fi

DEPLOYMENT_PLUGIN_DIRS=(
    plugins
    accessible
    assetimporters
    platforms
    platforminputcontexts
    platformthemes
    imageformats
    iconengines
    styles
    generic
    geoservices
    multimedia
    positioning
    qml
    qmltooling
    renderers
    sceneparsers
    sensors
    texttospeech
    virtualkeyboard
    webview
    tls
    bearer
    canbus
    printsupport
    sqldrivers
    networkinformation
    xcbglintegrations
    egldeviceintegrations
    wayland-decoration-client
    wayland-graphics-integration-client
    wayland-shell-integration
    translations
)
for plugin_dir in "${DEPLOYMENT_PLUGIN_DIRS[@]}"; do
    rm -rf -- "$plugin_dir"
done
rm -f Qt*.dll Qt*.DLL qt.conf

echo "[1/8] Copying runtime DLL dependencies..."
COPIED=0
if ! LDD_OUTPUT="$(ldd PXTOOL.exe 2>&1)"; then
    echo "ERROR: ldd failed while identifying MinGW runtime dependencies."
    printf '%s\n' "$LDD_OUTPUT"
    exit 1
fi
while IFS= read -r dll_path; do
    [ -n "$dll_path" ] || continue
    dll_name=$(basename "$dll_path")
    case "$dll_name" in
        Qt[0-9]*.dll|Qt[0-9]*.DLL)
            case "$dll_name" in
                Qt6*.dll|Qt6*.DLL) ;;
                *)
                    echo "ERROR: non-Qt6 dependency reported by ldd: $dll_name"
                    exit 1
                    ;;
            esac
            ;;
    esac
    if ! cp -f "$dll_path" "./$dll_name"; then
        echo "ERROR: failed to copy runtime dependency: $dll_path"
        exit 1
    fi
    COPIED=$((COPIED + 1))
done < <(
    printf '%s\n' "$LDD_OUTPUT" \
        | awk -v prefix="$MINGW_PREFIX" '
            {
                for (i = 1; i <= NF; i++) {
                    if (index($i, prefix "/") == 1) {
                        print $i
                        next
                    }
                }
            }
        '
)
echo "  -> Copied: $COPIED non-Qt runtime DLLs"

# --------------------------------------------------------------------------
# Step 2: Qt6 runtime and plugins
# --------------------------------------------------------------------------
echo "[2/8] Deploying Qt6 runtime and plugins..."
if ! "$WINDEPLOYQT" --release --no-translations --no-compiler-runtime ./PXTOOL.exe; then
    echo "ERROR: Qt6 deployment tool failed: $WINDEPLOYQT"
    exit 1
fi

# --------------------------------------------------------------------------
# Step 3: qt.conf (tells Qt where to find plugins relative to exe)
# --------------------------------------------------------------------------
echo "[3/8] Writing qt.conf..."
cat > qt.conf << 'EOF'
[Paths]
Prefix = .
Plugins = .
EOF
echo "  -> qt.conf written."

scan_for_legacy_qt_artifact() {
    local error_message="$1"
    local legacy_qt_artifact legacy_qt_scan_status
    shift

    if legacy_qt_artifact="$(find "$@" -print -quit 2>&1)"; then
        if [ -n "$legacy_qt_artifact" ]; then
            echo "ERROR: $error_message"
            printf '       %s\n' "$legacy_qt_artifact"
            return 1
        fi
        return 0
    fi

    legacy_qt_scan_status=$?
    echo "ERROR: failed to scan deployment for legacy Qt artifacts (status $legacy_qt_scan_status)."
    printf '%s\n' "$legacy_qt_artifact"
    return "$legacy_qt_scan_status"
}

verify_staged_qt_artifacts() {
    scan_for_legacy_qt_artifact \
        "non-Qt6 versioned Qt file residue found in deployment." \
        . -type f -iname '*qt[0-9]*' ! -iname '*qt6*'
    scan_for_legacy_qt_artifact \
        "non-Qt6 versioned Qt path residue found in deployment." \
        . -type f -ipath '*qt[0-9]*' ! -ipath '*qt6*'
}

verify_staged_qt_artifacts

if ! command -v objdump >/dev/null 2>&1; then
    echo "ERROR: objdump is required to validate staged PE dependencies."
    exit 1
fi

scan_pe_dependencies() {
    local candidate="$1"
    local require_qt6="${2:-0}"
    local pe_dump import_name import_lower import_file_name qt6_found=0
    local -a qt_imports=()

    if [ ! -r "$candidate" ]; then
        echo "ERROR: staged PE candidate is not readable: $candidate"
        return 1
    fi
    if ! pe_dump="$(objdump -p "$candidate" 2>&1)"; then
        echo "ERROR: objdump could not inspect staged PE candidate: $candidate"
        printf '%s\n' "$pe_dump"
        return 1
    fi

    mapfile -t qt_imports < <(
        printf '%s\n' "$pe_dump" \
            | awk 'tolower($1) == "dll" && tolower($2) == "name:" { print $3 }'
    )
    for import_name in "${qt_imports[@]}"; do
        [ -n "$import_name" ] || continue
        import_lower="${import_name,,}"
        import_file_name="$(basename "$import_lower")"
        if [[ "$import_file_name" =~ ^(lib)?qt[0-9]+[^[:space:]]*\.dll$ ]]; then
            if [[ "$import_file_name" =~ ^(lib)?qt6([^0-9]|$) ]]; then
                qt6_found=1
            else
                echo "ERROR: non-Qt6 versioned Qt import in $candidate: $import_name"
                return 1
            fi
        fi
    done

    if [ "$require_qt6" -eq 1 ] && [ "$qt6_found" -eq 0 ]; then
        echo "ERROR: PXTOOL.exe does not import Qt6."
        return 1
    fi
}

verify_staged_pe_tree() {
    local candidate require_qt6 scan_status candidate_list

    if ! candidate_list="$(mktemp "${TMPDIR:-/tmp}/pxtool-pe.XXXXXX")"; then
        echo "ERROR: unable to create a temporary PE validation list."
        return 1
    fi
    if ! find -L . -type f \( \
            -iname '*.exe' -o \
            -iname '*.dll' -o \
            -iname '*.ocx' \
        \) -print0 >"${candidate_list}"; then
        rm -f "${candidate_list}"
        echo "ERROR: unable to enumerate staged PE files."
        return 1
    fi

    scan_status=0
    while IFS= read -r -d '' candidate; do
        require_qt6=0
        if [ "$candidate" = "./PXTOOL.exe" ]; then
            require_qt6=1
        fi
        if ! scan_pe_dependencies "$candidate" "$require_qt6"; then
            scan_status=1
            break
        fi
    done <"${candidate_list}"
    rm -f "${candidate_list}"
    return "${scan_status}"
}

verify_staged_pe_tree

# --------------------------------------------------------------------------
# Step 4: Resource directories (res, demo, themes)
# Always sync with rsync (or cp -r --update as fallback) so that changes
# in the source tree are reflected in build.windows without a full clean.
# --------------------------------------------------------------------------
echo "[4/8] Syncing resource directories..."

# Helper: sync a source dir to a destination dir, always propagating updates.
sync_dir() {
    local src="$1" dst="$2" label="$3"
    if [ ! -d "$src" ]; then
        echo "  -> WARNING: $label source not found at $src, skipping."
        return
    fi
    local src_real dst_real
    src_real=$(cd "$src" && pwd -P)
    if [ -d "$dst" ]; then
        dst_real=$(cd "$dst" && pwd -P)
        if [ "$src_real" = "$dst_real" ]; then
            echo "  -> $label already in place, skipping."
            return
        fi
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
verify_staged_qt_artifacts

# --------------------------------------------------------------------------
# Step 5: Python protocol decoders (libsigrokdecode)
# --------------------------------------------------------------------------
echo "[5/8] Copying Python decoders..."
if [ ! -d decoders ]; then
    cp -r "$SOURCE_DIR/libsigrokdecode/decoders" ./decoders
    # Remove non-Python files that cause "Failed to load decoder" errors
    rm -f ./decoders/文件夹.bat ./decoders/subfolders_list.txt 2>/dev/null || true
    DECODER_COUNT=$(find ./decoders -name "pd.py" | wc -l)
    echo "  -> decoders/ copied ($DECODER_COUNT Python decoders found)"
else
    echo "  -> decoders/ already present, skipping."
fi
if [ -d "$BUILD_DIR/decoders/c_decoders" ]; then
    sync_dir "$BUILD_DIR/decoders/c_decoders" ./decoders/c_decoders "C decoders/"
else
    echo "  -> WARNING: built C decoder directory not found at $BUILD_DIR/decoders/c_decoders"
fi
verify_staged_qt_artifacts

# --------------------------------------------------------------------------
# Step 6: Bundle Python standard library
# Python's stdlib must be present alongside the app so that no system-wide
# Python installation is needed on the end-user's machine.
# The app's PYTHONHOME is set to <app_dir> so Python looks for stdlib at
# <app_dir>/lib/pythonX.Y/
# --------------------------------------------------------------------------
echo "[6/8] Bundling Python standard library..."

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
# Step 7: MCP browser Web Console
# --------------------------------------------------------------------------
echo "[7/8] Syncing MCP browser Web Console..."
if [ -d "$SOURCE_DIR/web/dist" ]; then
    sync_dir "$SOURCE_DIR/web/dist" ./webui "webui/ (MCP browser Web Console)"
elif [ -f "./webui/index.html" ]; then
    echo "  -> webui/ already present from CMake staging."
else
    echo "ERROR: web/dist not found and build.windows/webui is missing."
    echo "       Run: cmake --build build.windows --target stage_webui"
    exit 1
fi

if [ ! -f "./webui/index.html" ]; then
    echo "ERROR: MCP browser Web Console missing at build.windows/webui/index.html"
    exit 1
fi

# --------------------------------------------------------------------------
# Step 8: C decoders (compiled .dll files)
# --------------------------------------------------------------------------
echo "[8/8] Setting up C decoders..."
mkdir -p cdecoders
# Copy the example CDecoderRegistry SPI engine. This uses the pv/cdecoders ABI,
# which is separate from libsigrokdecode's decoders/c_decoders modules above.
if [ -f spi.dll ]; then
    cp -f spi.dll cdecoders/spi.dll
    echo "  -> spi.dll -> cdecoders/spi.dll"
else
    echo "  -> WARNING: spi.dll not found in build.windows (C decoders may not show [C]/[Py] options)"
fi

echo "  Verifying final staged PE dependencies..."
verify_staged_pe_tree
verify_staged_qt_artifacts

# --------------------------------------------------------------------------
# Done
# --------------------------------------------------------------------------
echo ""
echo "======================================"
echo "Deployment complete!"
echo "Run PXTOOL: $BUILD_DIR/PXTOOL.exe"
echo "======================================"
echo ""
