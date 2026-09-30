#!/bin/bash
set -euo pipefail
# =============================================================================
# PXTOOL Package Script
# Produces PXTOOL-<version>-win64.zip from the install() rules in CMakeLists.txt.
#
# The archive is built by CPack from a freshly staged install tree, not by
# compressing build.windows. build.windows is a CMake build directory: it holds
# _deps (a full nlohmann/json git clone, ~250 MB compressed), CMakeFiles, moc/qrc
# output and test binaries, and it accumulates every DLL any previous deployment
# copied there without ever pruning them.
#
# Run this after BUILD and DEPLOY, or via scripts/windows/FULL_BUILD.bat.
# =============================================================================

# Resolve the MinGW64 prefix the same way deploy_script.sh does.
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

CMAKE="$MINGW_PREFIX/bin/cmake.exe"
CPACK="$MINGW_PREFIX/bin/cpack.exe"
# CPACK_PACKAGE_DIRECTORY points at the source tree, so CPack stages the package
# here before archiving it.
CPACK_STAGING_DIR="$SOURCE_DIR/_CPack_Packages"

for tool in "$CMAKE" "$CPACK"; do
    if [ ! -x "$tool" ]; then
        echo "ERROR: required tool not found: $tool"
        exit 1
    fi
done

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "ERROR: $BUILD_DIR is not configured. Run scripts/windows/BUILD.bat first."
    exit 1
fi
if [ ! -f "$BUILD_DIR/PXTOOL.exe" ]; then
    echo "ERROR: PXTOOL.exe not found in $BUILD_DIR. Run scripts/windows/BUILD.bat first."
    exit 1
fi

read_version_field() {
    local field="$1"
    sed -n "s/^[[:space:]]*set([[:space:]]*${field}[[:space:]]\+\([^)[:space:]]\+\).*/\1/p" \
        "$SOURCE_DIR/CMakeLists.txt" | head -1
}

VERSION_MAJOR="$(read_version_field DS_VERSION_MAJOR)"
VERSION_MINOR="$(read_version_field DS_VERSION_MINOR)"
VERSION_MICRO="$(read_version_field DS_VERSION_MICRO)"
if [ -z "$VERSION_MAJOR" ] || [ -z "$VERSION_MINOR" ] || [ -z "$VERSION_MICRO" ]; then
    echo "ERROR: could not read DS_VERSION_* from CMakeLists.txt."
    exit 1
fi
VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_MICRO"
ZIP_NAME="PXTOOL-$VERSION-win64.zip"
ZIP_PATH="$SOURCE_DIR/$ZIP_NAME"

echo ""
echo "======================================"
echo "PXTOOL Package - version $VERSION"
echo "======================================"
echo ""

# The MCP Web Console is produced by the `webui` target, not by the main build,
# so a missing web/dist means the package would silently ship without it.
if [ ! -f "$SOURCE_DIR/web/dist/index.html" ]; then
    echo "ERROR: web/dist/index.html is missing, the MCP browser Web Console would not be packaged."
    echo "       Run: cmake --build build.windows --target webui"
    exit 1
fi

# Drop archives for every version, not just the current one, so a version bump
# cannot leave the previous ZIP lying around next to the new one.
shopt -s nullglob
for stale in "$SOURCE_DIR"/PXTOOL-*-win64.zip; do
    rm -f "$stale"
    echo "  Deleted : $(basename "$stale")"
done
shopt -u nullglob

# The install rules only ever add to the staging directory, they never remove.
# Reusing a previous stage would therefore archive files the current version no
# longer installs, which is the same drift that made the old ZIP unreliable.
rm -rf -- "$CPACK_STAGING_DIR"

echo "  Staging and archiving via CPack..."
cd "$BUILD_DIR"
if ! "$CPACK" -G ZIP; then
    echo ""
    echo "ERROR: cpack failed."
    echo "       Staging tree left in place for inspection: $CPACK_STAGING_DIR"
    exit 1
fi

if [ ! -f "$ZIP_PATH" ]; then
    echo "ERROR: expected archive was not produced: $ZIP_PATH"
    echo "       Staging tree left in place for inspection: $CPACK_STAGING_DIR"
    exit 1
fi

ZIP_SIZE="$(du -h "$ZIP_PATH" | cut -f1)"

# Only on success: a failed run's stage is worth keeping to look at, but a good
# one is a ~200 MB duplicate of the archive sitting in the source tree.
rm -rf -- "$CPACK_STAGING_DIR"

echo ""
echo "======================================"
echo "Done: $ZIP_NAME ($ZIP_SIZE)"
echo "======================================"
echo ""
