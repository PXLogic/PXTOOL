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
BUILD_APP="${ROOT}/build.macOS/PXTOOL.app"
PKG_ROOT="${INSTALL_PREFIX}/PXTOOL.app"
DIST_DIR="${ROOT}/build.macOS"
DIST_APP="${DIST_DIR}/PXTOOL.app"
FRAMEWORKS_DIR="${DIST_APP}/Contents/Frameworks"
DMG_OUT="${DIST_DIR}/PXTOOL.dmg"
SIGN_APP_SCRIPT="${ROOT}/scripts/macOS/sign-macos-app.sh"

SKIP_BUILD=0
NO_DMG=0
for arg in "$@"; do
  case "$arg" in
    --skip-build) SKIP_BUILD=1 ;;
    --no-dmg)     NO_DMG=1 ;;
  esac
done

detach_mounted_dmg() {
  local image_path="$1"
  local device

  command -v hdiutil >/dev/null 2>&1 || return 0

  device=$(hdiutil info 2>/dev/null | awk -v image_path="$image_path" '
    /^[[:space:]]*image-path[[:space:]]*:/ {
      current = $0
      sub(/^[^:]*:[[:space:]]*/, "", current)
    }
    current == image_path && /^[[:space:]]*dev-entry[[:space:]]*:/ {
      device = $0
      sub(/^[^:]*:[[:space:]]*/, "", device)
      print device
      exit
    }
  ')

  if [ -n "$device" ]; then
    hdiutil detach "$device" >/dev/null 2>&1 || \
      hdiutil detach "$device" -force >/dev/null 2>&1 || true
  fi
}

remove_dmg_artifact() {
  local artifact="$1"

  if [ -e "$artifact" ]; then
    detach_mounted_dmg "$artifact"
    rm -f "$artifact"
  fi
}

cleanup_dmg_artifacts() {
  local dmg_out="$1"
  local dmg_dir
  local dmg_name
  local stale_rw

  dmg_dir="$(dirname "$dmg_out")"
  dmg_name="$(basename "$dmg_out")"

  remove_dmg_artifact "$dmg_out"
  for stale_rw in "$dmg_dir"/rw.*."$dmg_name"; do
    [ -e "$stale_rw" ] || continue
    remove_dmg_artifact "$stale_rw"
  done
}

is_expected_macho_candidate() {
  local candidate="$1"
  local app="$2"
  local relative framework_root framework_name

  if [ "$candidate" = "$app/Contents/MacOS/PXTOOL" ]; then
    return 0
  fi

  case "$candidate" in
    "$app/Contents/MacOS/"*)
      relative="${candidate#"$app/Contents/MacOS/"}"
      [ "$relative" != */* ]
      return
      ;;
    "$app/Contents/Frameworks/"*|"$app/Contents/PlugIns/"*)
      case "$candidate" in
        *.dylib|*.bundle|*.so)
          return 0
          ;;
      esac
      if [[ "$candidate" == *.framework/* ]]; then
        framework_root="${candidate%%.framework/*}.framework"
        framework_name="${framework_root##*/}"
        framework_name="${framework_name%.framework}"
        [ "${candidate##*/}" = "$framework_name" ]
        return
      fi
      ;;
  esac

  return 1
}

find_framework_info_plist() {
  local framework="$1"
  local plist

  for plist in \
    "$framework/Resources/Info.plist" \
    "$framework/Versions/Current/Resources/Info.plist"; do
    if [ -f "$plist" ]; then
      printf '%s\n' "$plist"
      return 0
    fi
  done

  plist="$(find -L "$framework" -type f -path '*/Resources/Info.plist' -print -quit 2>/dev/null || true)"
  if [ -n "$plist" ]; then
    printf '%s\n' "$plist"
    return 0
  fi
  return 1
}

read_plist_value() {
  local plist="$1"
  local value

  if command -v plutil >/dev/null 2>&1; then
    if value="$(plutil -extract CFBundleShortVersionString raw -o - "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
    if value="$(plutil -extract CFBundleVersion raw -o - "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
  fi

  if [ -x /usr/libexec/PlistBuddy ]; then
    if value="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
    if value="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$plist" 2>/dev/null)"; then
      printf '%s\n' "$value"
      return 0
    fi
  fi

  return 1
}

find_qt_framework_dir() {
  local framework_name="$1"
  local framework_dir

  framework_dir="$FRAMEWORKS_DIR/$framework_name"
  if [ -d "$framework_dir" ]; then
    printf '%s\n' "$framework_dir"
    return 0
  fi

  framework_dir="$(find -L "$FRAMEWORKS_DIR" -type d -name "$framework_name" -print -quit 2>/dev/null || true)"
  if [ -n "$framework_dir" ]; then
    printf '%s\n' "$framework_dir"
    return 0
  fi
  return 1
}

verify_qt_framework() {
  local framework_name="$1"
  local framework_lower
  local framework_dir plist version

  framework_lower="$(printf '%s' "$framework_name" | tr '[:upper:]' '[:lower:]')"

  if [[ "$framework_lower" =~ ^qt[0-9]+ ]] \
      && [[ ! "$framework_lower" =~ ^qt6([^0-9]|$) ]]; then
    echo "ERROR: non-Qt6 framework imported or bundled: $framework_name"
    return 1
  fi

  if ! framework_dir="$(find_qt_framework_dir "$framework_name")"; then
    echo "ERROR: Qt framework is not bundled: $framework_name"
    return 1
  fi
  if ! plist="$(find_framework_info_plist "$framework_dir")"; then
    echo "ERROR: Qt framework has no readable Info.plist: $framework_dir"
    return 1
  fi
  if ! version="$(read_plist_value "$plist")"; then
    echo "ERROR: could not read Qt framework version: $framework_dir"
    return 1
  fi
  if [[ ! "$version" =~ ^6[.][0-9]+([.][0-9]+)?$ ]]; then
    echo "ERROR: Qt framework is not version 6.x: $framework_dir ($version)"
    return 1
  fi
}

verify_macho_file() {
  local candidate="$1"
  local require_qt="${2:-0}"
  local macho_dependencies dependency dependency_lower framework_path framework_name qt_major
  local qt_import_found=0
  local dependency_list

  if [ ! -r "$candidate" ]; then
    echo "ERROR: Mach-O candidate is not readable: $candidate"
    return 1
  fi
  if ! macho_dependencies="$(otool -L "$candidate" 2>&1)"; then
    echo "ERROR: otool could not inspect Mach-O candidate: $candidate"
    printf '%s\n' "$macho_dependencies"
    return 1
  fi

  dependency_list="$(printf '%s\n' "$macho_dependencies" | awk 'NR > 1 { print $1 }')"
  while IFS= read -r dependency; do
    [ -n "$dependency" ] || continue
    dependency_lower="$(printf '%s' "$dependency" | tr '[:upper:]' '[:lower:]')"

    if [[ "$dependency_lower" =~ qt[@_-]?([0-9]+) ]]; then
      qt_major="${BASH_REMATCH[1]}"
      if [ "$qt_major" != 6 ]; then
        echo "ERROR: non-Qt6 import in Mach-O candidate: $candidate ($dependency)"
        return 1
      fi
    fi

    if [[ "$dependency" =~ (/[Qq][Tt][^/]*[.]framework)(/|$) ]]; then
      framework_path="${BASH_REMATCH[1]}"
      framework_name="${framework_path##*/}"
      if ! verify_qt_framework "$framework_name"; then
        return 1
      fi
      qt_import_found=1
    elif [[ "$dependency_lower" =~ (^|/)(lib)?qt[0-9]+ ]]; then
      if [[ ! "$dependency_lower" =~ (^|/)(lib)?qt6([^0-9]|$) ]]; then
        echo "ERROR: non-Qt6 import in Mach-O candidate: $candidate ($dependency)"
        return 1
      fi
      qt_import_found=1
    elif [[ "$dependency_lower" =~ (^|/)(lib)?qt[^/]*[.]dylib$ ]]; then
      echo "ERROR: unversioned Qt dylib cannot be verified as Qt6: $candidate ($dependency)"
      return 1
    fi
  done <<<"$dependency_list"

  if [ "$require_qt" -eq 1 ] && [ "$qt_import_found" -eq 0 ]; then
    echo "ERROR: main PXTOOL executable does not import Qt."
    return 1
  fi
}

verify_macos_qt_bundle() {
  local app="$1"
  local main_executable="$app/Contents/MacOS/PXTOOL"
  local framework_dir framework_name framework_lower candidate file_description require_qt
  local qt_framework_count=0

  if [ ! -r "$main_executable" ]; then
    echo "ERROR: main PXTOOL executable is missing or unreadable: $main_executable"
    return 1
  fi
  if ! command -v file >/dev/null 2>&1 || ! command -v otool >/dev/null 2>&1; then
    echo "ERROR: file and otool are required to validate the macOS bundle."
    return 1
  fi
  if ! command -v plutil >/dev/null 2>&1 && [ ! -x /usr/libexec/PlistBuddy ]; then
    echo "ERROR: plutil or PlistBuddy is required to validate Qt framework versions."
    return 1
  fi
  if [ ! -d "$FRAMEWORKS_DIR" ]; then
    echo "ERROR: Qt framework directory is missing: $FRAMEWORKS_DIR"
    return 1
  fi

  while IFS= read -r -d '' framework_dir; do
    framework_name="${framework_dir##*/}"
    framework_lower="$(printf '%s' "$framework_name" | tr '[:upper:]' '[:lower:]')"
    if [[ "$framework_lower" == qt*.framework ]]; then
      if ! verify_qt_framework "$framework_name"; then
        return 1
      fi
      qt_framework_count=$((qt_framework_count + 1))
    fi
  done < <(find -L "$FRAMEWORKS_DIR" -type d -name '*.framework' -print0)

  if [ "$qt_framework_count" -eq 0 ]; then
    echo "ERROR: no Qt6 framework was found in the app bundle."
    return 1
  fi

  if ! find -L "$app" -type f -print0 | while IFS= read -r -d '' candidate; do
    if [ ! -r "$candidate" ]; then
      echo "ERROR: bundle file is not readable: $candidate"
      exit 1
    fi
    if ! file_description="$(file -b "$candidate" 2>&1)"; then
      echo "ERROR: file could not inspect bundle candidate: $candidate"
      printf '%s\n' "$file_description"
      exit 1
    fi
    if [[ "$file_description" == *Mach-O* ]]; then
      require_qt=0
      if [ "$candidate" = "$main_executable" ]; then
        require_qt=1
      fi
      if ! verify_macho_file "$candidate" "$require_qt"; then
        exit 1
      fi
    elif is_expected_macho_candidate "$candidate" "$app"; then
      echo "ERROR: expected Mach-O candidate is invalid: $candidate"
      printf '%s\n' "$file_description"
      exit 1
    fi
  done; then
    return 1
  fi

  local legacy_qt_artifact legacy_qt_scan_status
  if legacy_qt_artifact="$(find -L "$app" -type f \( \
    -iname '*qt[0-9]*' -o -ipath '*qt[0-9]*' \
  \) ! -ipath '*qt6*' -print -quit 2>&1)"; then
    :
  else
    legacy_qt_scan_status=$?
    echo "ERROR: failed to scan app bundle for legacy Qt artifacts (status $legacy_qt_scan_status)."
    printf '%s\n' "$legacy_qt_artifact"
    return "$legacy_qt_scan_status"
  fi
  if [ -n "$legacy_qt_artifact" ]; then
    echo "ERROR: non-Qt6 Qt artifact remains in app bundle: $legacy_qt_artifact"
    return 1
  fi
}

# Step 1: Build
if [ $SKIP_BUILD -eq 0 ]; then
  echo "[1/6] Building PXTOOL..."
  cd "$ROOT"
  cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" .
  make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
  cmake --build "$ROOT" --target webui --parallel 1
  cmake --install .
else
  echo "[1/6] Skipping build/install (--skip-build)"
  if [ ! -f "$ROOT/web/dist/index.html" ]; then
    echo "ERROR: --skip-build was used but web/dist/index.html is missing."
    echo "       Run without --skip-build, or run: cmake --build $ROOT --target webui"
    exit 1
  fi
fi

# Step 2: Assemble bundle
echo "[2/6] Assembling app bundle..."
mkdir -p "$DIST_DIR"

if [ "$BUILD_APP" != "$DIST_APP" ]; then
  rm -rf "$DIST_APP"
  cp -R "$BUILD_APP" "$DIST_APP"
elif [ ! -d "$DIST_APP" ]; then
  echo "ERROR: built app not found at $DIST_APP"
  exit 1
else
  # Keep non-Qt frameworks such as Python, but force macdeployqt to rebuild
  # the Qt frameworks and plugin tree instead of reusing stale deployment data.
  rm -rf "$DIST_APP/Contents/PlugIns"
  if [ -d "$FRAMEWORKS_DIR" ]; then
    find "$FRAMEWORKS_DIR" -type d -name 'Qt*.framework' -prune -exec rm -rf {} +
    find "$FRAMEWORKS_DIR" -type f -iname '*qt*.dylib' -exec rm -f {} +
  fi
fi

# The build tree may contain symlinks back into package-root (e.g. share ->).
# Remove them before overlaying the real resources so cp doesn't see identical inodes.
find "$DIST_APP/Contents/Resources" -type l -delete

# Copy data resources from package-root (res/, decoders, lang, demo, etc.)
cp -R "$PKG_ROOT/Contents/Resources/" "$DIST_APP/Contents/Resources/"

if [ ! -f "$DIST_APP/Contents/MacOS/webui/index.html" ]; then
  echo "ERROR: MCP browser Web Console missing from app bundle at Contents/MacOS/webui/index.html"
  exit 1
fi

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
MACDEPLOYQT="$(command -v macdeployqt || true)"
if [ -z "$MACDEPLOYQT" ]; then
  echo "ERROR: macdeployqt was not found on PATH."
  exit 1
fi
if MACDEPLOYQT_VERSION="$("$MACDEPLOYQT" -version 2>&1)"; then
  :
else
  MACDEPLOYQT_VERSION_STATUS=$?
  echo "ERROR: macdeployqt -version failed (status $MACDEPLOYQT_VERSION_STATUS)."
  printf '%s\n' "$MACDEPLOYQT_VERSION"
  exit "$MACDEPLOYQT_VERSION_STATUS"
fi
if ! printf '%s\n' "$MACDEPLOYQT_VERSION" \
    | grep -Eq '^[[:space:]]*macdeployqt[[:space:]]+6([.][0-9]+){1,2}[[:space:]]*$'; then
  echo "ERROR: macdeployqt 6 is required."
  printf '%s\n' "$MACDEPLOYQT_VERSION"
  exit 1
fi
MACDEPLOYQT_LOG="$(mktemp)"
MACDEPLOYQT_ARGS=("$DIST_APP" -verbose=1 -no-codesign)
for libpath in /opt/homebrew/lib /opt/homebrew/Frameworks; do
  if [ -d "$libpath" ]; then
    MACDEPLOYQT_ARGS+=("-libpath=$libpath")
  fi
done
if ! "$MACDEPLOYQT" "${MACDEPLOYQT_ARGS[@]}" >"$MACDEPLOYQT_LOG" 2>&1; then
  cat "$MACDEPLOYQT_LOG"
  rm -f "$MACDEPLOYQT_LOG"
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

echo "  Verifying all Mach-O files and Qt frameworks..."
verify_macos_qt_bundle "$DIST_APP"

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
SRD_CDECODERS_DIR="$DIST_APP/Contents/Resources/share/libsigrokdecode/decoders/c_decoders"
if [ -d "$SRD_CDECODERS_DIR" ]; then
  SRD_CDECODER_COUNT=$(find "$SRD_CDECODERS_DIR" -type f -name "*.dylib" -o -name "*.so" | wc -l | tr -d ' ')
  echo "  OK: libsigrokdecode C decoders ($SRD_CDECODER_COUNT modules)"
else
  echo "  WARNING: libsigrokdecode C decoders missing at $SRD_CDECODERS_DIR"
fi

# Step 5: Verify dependencies and sign
echo "[5/6] Verifying dependencies and signing..."

if [ -d "$FRAMEWORKS_DIR/Python.framework" ]; then
  echo "  OK: Python.framework"
fi

echo "  Re-signing app bundle..."
"$SIGN_APP_SCRIPT" "$DIST_APP"

# Final check for any remaining external dependencies.
MACHO_DEPENDENCIES=""
if MACHO_DEPENDENCIES="$(otool -L "$DIST_APP/Contents/MacOS/PXTOOL" 2>&1)"; then
  :
else
  OTOOL_STATUS=$?
  echo "ERROR: unable to inspect PXTOOL Mach-O dependencies (status $OTOOL_STATUS)."
  printf '%s\n' "$MACHO_DEPENDENCIES"
  exit "$OTOOL_STATUS"
fi

BROKEN=""
if BROKEN="$(printf '%s\n' "$MACHO_DEPENDENCIES" | awk 'NR > 1 && ($1 ~ /^\/opt\/homebrew\// || $1 ~ /^\/usr\/local\//) { print $1 }')"; then
  :
else
  echo "ERROR: unable to scan PXTOOL Mach-O dependencies for external paths."
  exit 1
fi

if [ -n "$BROKEN" ]; then
  echo "ERROR: The following libs still reference external paths:"
  echo "$BROKEN" | sed 's/^/    /'
  exit 1
else
  echo "  All external libs resolved."
fi

# Step 6: Create DMG
if [ $NO_DMG -eq 0 ]; then
  echo "[6/6] Creating DMG..."
  # Get version from Info.plist
  VERSION=$(defaults read "$DIST_APP/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "1.0")
  DMG_OUT="${DIST_DIR}/PXTOOL-${VERSION}-arm64-macOS.dmg"
  cleanup_dmg_artifacts "$DMG_OUT"

  create-dmg \
    --volname "PXTOOL ${VERSION}" \
    --volicon "$DIST_APP/Contents/Resources/PXTOOL.icns" \
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
