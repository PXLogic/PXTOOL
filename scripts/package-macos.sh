#!/usr/bin/env bash
# Build a distributable macOS .app bundle + .dmg for DSView.
#
# Usage:
#   bash scripts/package-macos.sh [--skip-build] [--no-dmg]
#
# Output:
#   dist/DSView.app   — standalone app bundle
#   dist/DSView.dmg   — DMG installer (unless --no-dmg)

set -euo pipefail

# ── Config ────────────────────────────────────────────────────────────────────
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_APP="${ROOT}/build.dir/DSView.app"
PKG_ROOT="${ROOT}/package-root/DSView.app"
DIST_DIR="${ROOT}/dist"
DIST_APP="${DIST_DIR}/DSView.app"
DMG_OUT="${DIST_DIR}/DSView.dmg"

SKIP_BUILD=0
NO_DMG=0
for arg in "$@"; do
  case "$arg" in
    --skip-build) SKIP_BUILD=1 ;;
    --no-dmg)     NO_DMG=1 ;;
  esac
done

# ── Step 1: Build ─────────────────────────────────────────────────────────────
if [ $SKIP_BUILD -eq 0 ]; then
  echo "[1/6] Building DSView..."
  cd "$ROOT"
  make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
else
  echo "[1/6] Skipping build (--skip-build)"
fi

# ── Step 2: Assemble bundle ───────────────────────────────────────────────────
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

# ── Step 3: macdeployqt — bundle Qt frameworks ────────────────────────────────
echo "[3/6] Running macdeployqt..."
MACDEPLOYQT="$(which macdeployqt)"
"$MACDEPLOYQT" "$DIST_APP" -verbose=1

# ── Step 4: Ensure rpath is set (macdeployqt handles Qt + most dylibs) ────────
echo "[4/6] Verifying rpath and macdeployqt-bundled dylibs..."
FRAMEWORKS_DIR="$DIST_APP/Contents/Frameworks"

# Ensure @executable_path/../Frameworks is in rpath for non-Qt libs
install_name_tool -add_rpath "@executable_path/../Frameworks" \
  "$DIST_APP/Contents/MacOS/DSView" 2>/dev/null || true

# Confirm the key libs were bundled by macdeployqt
for lib in libglib-2.0.0.dylib libusb-1.0.0.dylib libfftw3.3.dylib; do
  if [ -f "$FRAMEWORKS_DIR/$lib" ]; then
    echo "  OK: $lib"
  else
    echo "  WARNING: $lib not found in bundle — macdeployqt may have missed it"
  fi
done

# Confirm bundled C decoders survived the copy from package-root/.
CDECODERS_DIR="$DIST_APP/Contents/Resources/share/DSView/cdecoders"
for dylib in spi.dylib; do
  if [ -f "$CDECODERS_DIR/$dylib" ]; then
    echo "  OK: cdecoders/$dylib"
  else
    echo "  WARNING: cdecoders/$dylib missing — did 'make install' populate package-root?"
  fi
done

# ── Step 5: Bundle Python.framework + fix remaining homebrew paths ────────────
echo "[5/6] Bundling Python.framework and verifying dependencies..."

# Locate Python framework from Homebrew
PY_HOMEBREW_LIB=$(otool -L "$DIST_APP/Contents/MacOS/DSView" 2>/dev/null \
  | grep -E "/opt/homebrew.*Python.framework.*/Python" | awk '{print $1}' || true)

if [ -n "$PY_HOMEBREW_LIB" ]; then
  # Derive the framework root from the library path
  # e.g. /opt/homebrew/opt/python@3.13/Frameworks/Python.framework/Versions/3.13/Python
  PY_VERSION=$(echo "$PY_HOMEBREW_LIB" | grep -oE "Versions/[0-9.]+" | head -1 | cut -d/ -f2)
  PY_FRAMEWORK_SRC=$(echo "$PY_HOMEBREW_LIB" | sed 's|/Versions/.*||')

  echo "  Found Python ${PY_VERSION} at: $PY_FRAMEWORK_SRC"
  echo "  Copying Python.framework into bundle..."

  PY_DEST="$FRAMEWORKS_DIR/Python.framework"
  rm -rf "$PY_DEST"
  mkdir -p "$PY_DEST/Versions/${PY_VERSION}"
  cp -R "$PY_FRAMEWORK_SRC/Versions/${PY_VERSION}/." "$PY_DEST/Versions/${PY_VERSION}/"
  ln -sf "${PY_VERSION}" "$PY_DEST/Versions/Current"
  ln -sf "Versions/Current/Python" "$PY_DEST/Python"
  ln -sf "Versions/Current/Resources" "$PY_DEST/Resources"
  chmod +w "$PY_DEST/Versions/${PY_VERSION}/Python"

  echo "  Fixing install names..."
  install_name_tool -id \
    "@rpath/Python.framework/Versions/${PY_VERSION}/Python" \
    "$PY_DEST/Versions/${PY_VERSION}/Python"
  install_name_tool -change \
    "$PY_HOMEBREW_LIB" \
    "@rpath/Python.framework/Versions/${PY_VERSION}/Python" \
    "$DIST_APP/Contents/MacOS/DSView"

  echo "  Re-signing app bundle..."
  codesign --force --deep --sign - "$DIST_APP" 2>&1 | grep -v "^$" || true
else
  echo "  No Python.framework reference found, skipping."
fi

# Final check for any remaining homebrew paths
BROKEN=$(otool -L "$DIST_APP/Contents/MacOS/DSView" 2>/dev/null \
  | grep -E "/opt/homebrew|/usr/local" | grep -v "^/usr/local/lib/libSystem" \
  | awk '{print $1}' || true)

if [ -n "$BROKEN" ]; then
  echo "  WARNING: The following libs still reference homebrew paths:"
  echo "$BROKEN" | sed 's/^/    /'
else
  echo "  All external libs resolved."
fi

# ── Step 6: Create DMG ────────────────────────────────────────────────────────
if [ $NO_DMG -eq 0 ]; then
  echo "[6/6] Creating DMG..."
  # Get version from Info.plist
  VERSION=$(defaults read "$DIST_APP/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "1.0")
  DMG_OUT="${DIST_DIR}/DSView-${VERSION}.dmg"

  create-dmg \
    --volname "DSView ${VERSION}" \
    --volicon "$DIST_APP/Contents/Resources/DSView.icns" \
    --window-pos 200 120 \
    --window-size 600 400 \
    --icon-size 100 \
    --icon "DSView.app" 150 180 \
    --hide-extension "DSView.app" \
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
