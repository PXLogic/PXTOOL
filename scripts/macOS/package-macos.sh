#!/usr/bin/env bash
# Build a distributable macOS .app bundle + .dmg for PXTOOL.
#
# Usage:
#   bash scripts/macOS/package-macos.sh [--skip-build] [--no-dmg]
#
# Output:
#   build.macOS/PXTOOL.app   - standalone app bundle
#   build.macOS/PXTOOL.dmg   - DMG installer (unless --no-dmg)

set -euo pipefail

# Config
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALL_PREFIX="${ROOT}/package-root"
BUILD_APP="${ROOT}/build.dir/PXTOOL.app"
PKG_ROOT="${INSTALL_PREFIX}/PXTOOL.app"
DIST_DIR="${ROOT}/build.macOS"
DIST_APP="${DIST_DIR}/PXTOOL.app"
FRAMEWORKS_DIR="${DIST_APP}/Contents/Frameworks"
DMG_OUT="${DIST_DIR}/PXTOOL.dmg"

SKIP_BUILD=0
NO_DMG=0
for arg in "$@"; do
  case "$arg" in
    --skip-build) SKIP_BUILD=1 ;;
    --no-dmg)     NO_DMG=1 ;;
  esac
done

# Step 1: Build
if [ $SKIP_BUILD -eq 0 ]; then
  echo "[1/6] Building PXTOOL..."
  cd "$ROOT"
  cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .
  make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
  cmake --install .
else
  echo "[1/6] Skipping build/install (--skip-build)"
fi

# Step 2: Assemble bundle
echo "[2/6] Assembling app bundle..."
rm -rf "$DIST_DIR" || { rm -rf "$DIST_DIR"/* "$DIST_DIR"/.[!.]* 2>/dev/null; true; }
mkdir -p "$DIST_DIR"

# Start from the built binary + existing bundle shell
cp -R "$BUILD_APP" "$DIST_APP"

# The build tree may contain symlinks back into package-root (e.g. share ->).
# Remove them before overlaying the real resources so cp doesn't see identical inodes.
find "$DIST_APP/Contents/Resources" -type l -delete

# Copy data resources from package-root (res/, decoders, lang, demo, etc.)
cp -R "$PKG_ROOT/Contents/Resources/" "$DIST_APP/Contents/Resources/"

# Bundle Python before macdeployqt scans dependencies. Homebrew's Python
# framework exposes compatibility paths that macdeployqt may not resolve, so
# make the app point at the in-bundle framework first.
echo "[3/6] Bundling Python.framework and running macdeployqt..."
mkdir -p "$FRAMEWORKS_DIR"
install_name_tool -add_rpath "@executable_path/../Frameworks" \
  "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null || true

PY_HOMEBREW_LIB=$(otool -L "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null \
  | grep -E "/opt/homebrew.*Python.framework.*/Python" | awk '{print $1}' || true)

if [ -n "$PY_HOMEBREW_LIB" ]; then
  PY_VERSION=$(echo "$PY_HOMEBREW_LIB" | grep -oE "Versions/[0-9.]+" | head -1 | cut -d/ -f2)
  PY_FRAMEWORK_SRC=$(echo "$PY_HOMEBREW_LIB" | sed 's|/Versions/.*||')
  PY_DEST="$FRAMEWORKS_DIR/Python.framework"

  echo "  Found Python ${PY_VERSION} at: $PY_FRAMEWORK_SRC"
  rm -rf "$PY_DEST"
  mkdir -p "$PY_DEST/Versions/${PY_VERSION}"
  cp -R "$PY_FRAMEWORK_SRC/Versions/${PY_VERSION}/." "$PY_DEST/Versions/${PY_VERSION}/"
  ln -sf "${PY_VERSION}" "$PY_DEST/Versions/Current"
  ln -sf "Versions/Current/Python" "$PY_DEST/Python"
  ln -sf "Versions/Current/Resources" "$PY_DEST/Resources"
  chmod +w "$PY_DEST/Versions/${PY_VERSION}/Python"

  install_name_tool -id \
    "@rpath/Python.framework/Versions/${PY_VERSION}/Python" \
    "$PY_DEST/Versions/${PY_VERSION}/Python"
  install_name_tool -change \
    "$PY_HOMEBREW_LIB" \
    "@rpath/Python.framework/Versions/${PY_VERSION}/Python" \
    "$DIST_APP/Contents/MacOS/PXTOOL"
else
  echo "  No Homebrew Python.framework reference found."
fi

# Step 3: macdeployqt - bundle Qt frameworks
MACDEPLOYQT="$(which macdeployqt)"
MACDEPLOYQT_LOG="$(mktemp)"
MACDEPLOYQT_ARGS=("$DIST_APP" -verbose=1 -no-codesign)
for libpath in /opt/homebrew/lib /opt/homebrew/Frameworks; do
  if [ -d "$libpath" ]; then
    MACDEPLOYQT_ARGS+=("-libpath=$libpath")
  fi
done
if ! "$MACDEPLOYQT" "${MACDEPLOYQT_ARGS[@]}" >"$MACDEPLOYQT_LOG" 2>&1; then
  cat "$MACDEPLOYQT_LOG"
  exit 1
fi
awk '
  /QtPdf\.framework|QtVirtualKeyboard(Qml)?\.framework/ { skip_next = 1; next }
  skip_next && /using QList/ { skip_next = 0; next }
  { skip_next = 0; print }
' "$MACDEPLOYQT_LOG"
rm -f "$MACDEPLOYQT_LOG"

# macdeployqt deploys broad plugin sets. PXTOOL does not use these optional
# plugins, and they can drag in optional Homebrew Qt frameworks.
for plugin in \
  "$DIST_APP/Contents/PlugIns/imageformats/libqpdf.dylib" \
  "$DIST_APP/Contents/PlugIns/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"; do
  if [ -f "$plugin" ]; then
    rm -f "$plugin"
    echo "  Removed optional plugin: ${plugin#$DIST_APP/Contents/PlugIns/}"
  fi
done

# Step 4: Ensure rpath is set (macdeployqt handles Qt + most dylibs)
echo "[4/6] Verifying rpath and macdeployqt-bundled dylibs..."

# Ensure @executable_path/../Frameworks is in rpath for non-Qt libs
install_name_tool -add_rpath "@executable_path/../Frameworks" \
  "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null || true

# Confirm the key libs were bundled by macdeployqt
for lib in libglib-2.0.0.dylib libusb-1.0.0.dylib libfftw3.3.dylib; do
  if [ -f "$FRAMEWORKS_DIR/$lib" ]; then
    echo "  OK: $lib"
  else
    echo "  WARNING: $lib not found in bundle - macdeployqt may have missed it"
  fi
done

# Confirm bundled C decoders survived the copy from package-root/.
CDECODERS_DIR="$DIST_APP/Contents/Resources/share/PXTOOL/cdecoders"
for dylib in spi.dylib; do
  if [ -f "$CDECODERS_DIR/$dylib" ]; then
    echo "  OK: cdecoders/$dylib"
  else
    echo "  WARNING: cdecoders/$dylib missing - did 'make install' populate package-root?"
  fi
done

# Step 5: Verify dependencies and sign
echo "[5/6] Verifying dependencies and signing..."

if [ -d "$FRAMEWORKS_DIR/Python.framework" ]; then
  echo "  OK: Python.framework"
fi

echo "  Re-signing app bundle..."
codesign --force --deep --sign - "$DIST_APP" 2>&1 | grep -v "^$" || true

# Final check for any remaining homebrew paths
BROKEN=$(otool -L "$DIST_APP/Contents/MacOS/PXTOOL" 2>/dev/null \
  | grep -E "/opt/homebrew|/usr/local" | grep -v "^/usr/local/lib/libSystem" \
  | awk '{print $1}' || true)

if [ -n "$BROKEN" ]; then
  echo "  WARNING: The following libs still reference homebrew paths:"
  echo "$BROKEN" | sed 's/^/    /'
else
  echo "  All external libs resolved."
fi

# Step 6: Create DMG
if [ $NO_DMG -eq 0 ]; then
  echo "[6/6] Creating DMG..."
  # Get version from Info.plist
  VERSION=$(defaults read "$DIST_APP/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "1.0")
  DMG_OUT="${DIST_DIR}/PXTOOL-${VERSION}-arm64-macOS.dmg"

  create-dmg \
    --volname "PXTOOL ${VERSION}" \
    --volicon "$DIST_APP/Contents/Resources/DSView.icns" \
    --window-pos 200 120 \
    --window-size 600 400 \
    --icon-size 100 \
    --icon "PXTOOL.app" 150 180 \
    --hide-extension "PXTOOL.app" \
    --app-drop-link 450 180 \
    "$DMG_OUT" \
    "$DIST_DIR" \
    2>&1 | tail -5

  echo ""
  echo "  DMG created: $DMG_OUT"
else
  echo "[6/6] Skipping DMG (--no-dmg)"
fi

echo ""
echo "Done! Distributable bundle:"
echo "  App: $DIST_APP"
[ $NO_DMG -eq 0 ] && echo "  DMG: $DMG_OUT"
du -sh "$DIST_APP"