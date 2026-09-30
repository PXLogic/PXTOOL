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
# Generated from CMake/InstallWindowsRuntime.cmake.in at configure time. Shared
# with the release package (scripts/windows/package_script.sh) so that running
# from build.windows exercises the same runtime that ships in the ZIP.
INSTALL_RUNTIME_SCRIPT="$BUILD_DIR/InstallWindowsRuntime.cmake"

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

if [ ! -f "$INSTALL_RUNTIME_SCRIPT" ]; then
    echo "ERROR: $INSTALL_RUNTIME_SCRIPT was not generated."
    echo "       Re-run CMake configuration: scripts/windows/BUILD.bat"
    exit 1
fi

echo ""
echo "======================================"
echo "PXTOOL Deploy - Runtime Dependencies"
echo "======================================"
echo ""

# --------------------------------------------------------------------------
# Clean slate for everything the runtime staging step re-creates.
#
# Every DLL is removed, not just Qt's. Earlier revisions deployed the whole
# MinGW bin directory, and the incremental copy that replaced it never pruned
# what it no longer needed, so build.windows kept accumulating DLLs the app does
# not load (155 of 186 at the time this was fixed, ~80 MB). Beyond the wasted
# space, that residue silently satisfied dependencies the deployment itself was
# failing to stage, so the build tree ran while a clean machine would not.
# --------------------------------------------------------------------------
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
rm -f -- *.dll *.DLL qt.conf
echo "  -> Removed previously deployed DLLs, Qt plugin directories and qt.conf"

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
# Step 1: Resource directories (res, demo, themes)
# Always sync with rsync (or cp -r --update as fallback) so that changes
# in the source tree are reflected in build.windows without a full clean.
# --------------------------------------------------------------------------
echo "[1/6] Syncing resource directories..."

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
# Step 2: Python protocol decoders (libsigrokdecode)
# --------------------------------------------------------------------------
echo "[2/6] Copying Python decoders..."
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
# Step 3: Bundle Python standard library
# Python's stdlib must be present alongside the app so that no system-wide
# Python installation is needed on the end-user's machine.
# The app's PYTHONHOME is set to <app_dir> so Python looks for stdlib at
# <app_dir>/lib/pythonX.Y/
# --------------------------------------------------------------------------
echo "[3/6] Bundling Python standard library..."

# Detect the Python version from the toolchain, not from a libpython3.*.dll in
# build.windows: the cleanup above removes every staged DLL, and the stdlib has
# to match what the app will actually link against anyway.
PY_VER=""
for py_stdlib in "$MINGW_PREFIX"/lib/python3.*; do
    if [ -d "$py_stdlib" ]; then
        PY_VER="${py_stdlib##*/python}"
        break
    fi
done

if [ -z "$PY_VER" ]; then
    echo "ERROR: no Python 3 standard library found under $MINGW_PREFIX/lib."
    exit 1
else
    PY_SRC="$MINGW_PREFIX/lib/python${PY_VER}"
    PY_DST="./lib/python${PY_VER}"

    if [ ! -d "$PY_SRC" ]; then
        echo "ERROR: Python stdlib not found at $PY_SRC"
        exit 1
    fi

    # Re-staged from scratch rather than patched in place. An incremental copy
    # keeps whatever MSYS2 has since removed or replaced (an old pip wheel, the
    # dropped libxml2 bindings), which makes the build tree diverge from the
    # release package. Removing lib/python3.* first also clears the stdlib of a
    # previous Python version after an MSYS2 upgrade.
    rm -rf ./lib/python3.*
    mkdir -p "$PY_DST"
    cp -r "$PY_SRC"/* "$PY_DST/" 2>/dev/null || true
    # Same pruning the install rules apply: caches and test suites are not needed
    # at runtime and roughly halve the stdlib size.
    find "$PY_DST" -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
    find "$PY_DST" -type d -name test -exec rm -rf {} + 2>/dev/null || true
    find "$PY_DST" -type d -name tests -exec rm -rf {} + 2>/dev/null || true
    if [ ! -d "$PY_DST/encodings" ]; then
        echo "ERROR: Python stdlib copy is incomplete, $PY_DST/encodings is missing."
        exit 1
    fi
    PY_SIZE=$(du -sh "$PY_DST" 2>/dev/null | cut -f1)
    echo "  -> Python ${PY_VER} stdlib bundled to lib/python${PY_VER}/ (${PY_SIZE})"
fi

# --------------------------------------------------------------------------
# App icon beside executable (QApplication::setWindowIcon loads this file)
# --------------------------------------------------------------------------
if [ -f "$SOURCE_DIR/win-app-logo.ico" ]; then
    cp -f "$SOURCE_DIR/win-app-logo.ico" ./win-app-logo.ico
    echo "  -> win-app-logo.ico copied beside PXTOOL.exe"
fi

# --------------------------------------------------------------------------
# Step 4: MCP browser Web Console
# --------------------------------------------------------------------------
echo "[4/6] Syncing MCP browser Web Console..."
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
# Step 5: C decoders (compiled .dll files)
# --------------------------------------------------------------------------
echo "[5/6] Setting up C decoders..."
# cdecoders/ is the CDecoderRegistry plugin directory (pv/cdecoders ABI), which
# is separate from libsigrokdecode's decoders/c_decoders modules staged above.
# It ships empty on purpose: the example SPI plugin is no longer built, because
# it claimed the Python SPI decoder's id and produced a second, option-less
# "SPI(C)" row next to the built-in spi_c decoder. See the note next to
# pv/cdecoders/example_spi in CMakeLists.txt.
mkdir -p cdecoders
# Drop plugins left behind by builds that still shipped the example.
rm -f cdecoders/spi.dll spi.dll
echo "  -> cdecoders/ created (empty; no example plugin is shipped)"

# --------------------------------------------------------------------------
# Step 6: Qt6 runtime, qt.conf and the MinGW dependency closure
#
# Delegated to the CMake install script so that build.windows and the release
# ZIP are staged by one implementation. It runs last because it derives the
# dependency closure from what is already on disk: the Qt plugins it deploys, the
# C decoder modules from step 5 and the CPython extension modules from step 3 are
# all loaded at runtime, so their imports (glib, jpeg, sqlite3, ssl, ...) are not
# visible in PXTOOL.exe's own import table.
# --------------------------------------------------------------------------
echo "[6/6] Deploying Qt6 runtime and MinGW dependencies..."
if ! "$MINGW_PREFIX/bin/cmake.exe" \
        -DCMAKE_INSTALL_PREFIX="$(cygpath -m "$BUILD_DIR")" \
        -P "$(cygpath -m "$INSTALL_RUNTIME_SCRIPT")"; then
    echo "ERROR: runtime deployment failed."
    exit 1
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
