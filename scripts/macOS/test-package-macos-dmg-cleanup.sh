#!/usr/bin/env bash
# Regression test for rerunning package-macos.sh with existing DMG artifacts.

set -euo pipefail

SOURCE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

if ! grep -Fq -- 'cmake --build "$ROOT" --target stage_webui --parallel 1' \
  "$SOURCE_ROOT/scripts/macOS/package-macos.sh"; then
  echo "Expected package-macos.sh to stage the MCP browser Web Console into the app bundle." >&2
  exit 1
fi

SCRIPT_UNDER_TEST="$WORKDIR/scripts/macOS/package-macos.sh"
SIGN_SCRIPT="$WORKDIR/scripts/macOS/sign-macos-app.sh"
FAKEBIN="$WORKDIR/bin"
APP="$WORKDIR/build.macOS/PXTOOL.app"
PKG_APP="$WORKDIR/package-root/PXTOOL.app"
DMG_OUT="$WORKDIR/build.macOS/PXTOOL-1.0.0-arm64-macOS.dmg"
STALE_RW="$WORKDIR/build.macOS/rw.12345.PXTOOL-1.0.0-arm64-macOS.dmg"
QT_LIBS_DIR="$WORKDIR/qt/lib"
QT_LOCAL_BINARY="$QT_LIBS_DIR/QtGui.framework/Versions/A/QtGui"
QT_INSTALL_NAME="/opt/homebrew/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui"
MACDEPLOYQT_MARKER="$WORKDIR/macdeployqt-ran"
PY_FRAMEWORK="$APP/Contents/Frameworks/Python.framework/Versions/3.13"
PY_LIBRARY="$PY_FRAMEWORK/Python"
PY_BIN="$PY_FRAMEWORK/bin/python3.13"
PY_APP_BIN="$PY_FRAMEWORK/Resources/Python.app/Contents/MacOS/Python"
PY_HOMEBREW="/opt/homebrew/Cellar/python@3.13/3.13.5/Frameworks/Python.framework/Versions/3.13/Python"

mkdir -p \
  "$WORKDIR/scripts/macOS" \
  "$FAKEBIN" \
  "$WORKDIR/web/dist" \
  "$APP/Contents/MacOS/webui" \
  "$APP/Contents/Resources" \
  "$PY_FRAMEWORK/bin" \
  "$PY_FRAMEWORK/Resources/Python.app/Contents/MacOS" \
  "$QT_LIBS_DIR/QtGui.framework/Versions/A" \
  "$PKG_APP/Contents/Resources"

cp "$SOURCE_ROOT/scripts/macOS/package-macos.sh" "$SCRIPT_UNDER_TEST"
cp "$SOURCE_ROOT/scripts/macOS/sign-macos-app.sh" "$SIGN_SCRIPT"
chmod +x "$SCRIPT_UNDER_TEST"
chmod +x "$SIGN_SCRIPT"

touch "$APP/Contents/MacOS/PXTOOL"
chmod +x "$APP/Contents/MacOS/PXTOOL"
touch "$APP/Contents/MacOS/webui/index.html"
touch "$APP/Contents/Resources/PXTOOL.icns"
touch "$APP/Contents/Info.plist"
touch "$PY_BIN"
touch "$PY_APP_BIN"
touch "$PY_LIBRARY"
touch "$QT_LOCAL_BINARY"
touch "$WORKDIR/web/dist/index.html"
printf 'old dmg\n' >"$DMG_OUT"
printf 'stale read-write image\n' >"$STALE_RW"

cat >"$FAKEBIN/install_name_tool" <<'STUB'
#!/usr/bin/env bash
printf 'install_name_tool %s\n' "$*" >>"$FAKE_INSTALL_NAME_TOOL_LOG"
exit 0
STUB

cat >"$FAKEBIN/otool" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail

mode="${1:-}"
candidate="${2:-}"

case "$mode" in
  -D)
    printf '%s:\n%s\n' "$candidate" "$FAKE_QT_INSTALL_NAME"
    ;;
  -l)
    printf '          cmd LC_RPATH\n'
    if [[ "$candidate" == *QtGui.framework* ]]; then
      printf '         path @loader_path/../../../ (offset 12)\n'
    else
      printf '         path @executable_path/../Frameworks (offset 12)\n'
    fi
    ;;
  -L)
    printf '%s:\n' "$candidate"
    if [[ "$candidate" == *QtGui.framework* ]]; then
      printf '\t@rpath/QtGui.framework/Versions/A/QtGui (compatibility version 6.0.0, current version 6.11.2)\n'
      if grep -Fq -- "-change @rpath/Carbon.framework/Versions/A/Carbon /System/Library/Frameworks/Carbon.framework/Versions/A/Carbon $candidate" "$FAKE_INSTALL_NAME_TOOL_LOG"; then
        printf '\t/System/Library/Frameworks/Carbon.framework/Versions/A/Carbon (compatibility version 2.0.0, current version 170.0.0)\n'
      else
        printf '\t@rpath/Carbon.framework/Versions/A/Carbon (compatibility version 2.0.0, current version 170.0.0)\n'
      fi
    elif [[ "$candidate" == */Contents/MacOS/PXTOOL ]]; then
      if [ -e "$FAKE_MACDEPLOYQT_MARKER" ]; then
        printf '\t@rpath/QtGui.framework/Versions/A/QtGui (compatibility version 6.0.0, current version 6.11.2)\n'
      else
        printf '\t%s/QtGui.framework/Versions/A/QtGui (compatibility version 6.0.0, current version 6.11.2)\n' "$FAKE_QT_LIBS_DIR"
      fi
    elif [[ "$candidate" == *Python.framework*/bin/python3.13 ]]; then
      printf '\t%s (compatibility version 3.13.0, current version 3.13.0)\n' "$FAKE_PY_HOMEBREW"
    elif [[ "$candidate" == *Python.framework*/Resources/Python.app/Contents/MacOS/Python ]]; then
      printf '\t%s (compatibility version 3.13.0, current version 3.13.0)\n' "$FAKE_PY_HOMEBREW"
    else
      printf '\t@rpath/QtGui.framework/Versions/A/QtGui (compatibility version 6.0.0, current version 6.11.2)\n'
    fi
    ;;
esac
exit 0
STUB

cat >"$FAKEBIN/macdeployqt" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "-version" ]; then
  echo "Usage: macdeployqt app-bundle [options]" >&2
  exit 1
fi

app="$1"
touch "$FAKE_MACDEPLOYQT_MARKER"
mkdir -p "$app/Contents/Frameworks/QtGui.framework/Versions/A"
mkdir -p "$app/Contents/Frameworks/QtGui.framework/Resources"
touch "$app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui"
chmod +x "$app/Contents/Frameworks/QtGui.framework/Versions/A/QtGui"
touch "$app/Contents/Frameworks/QtGui.framework/Resources/Info.plist"
for framework in QtQml QtQmlMeta QtQmlModels QtQmlWorkerScript QtQuick; do
  mkdir -p "$app/Contents/Frameworks/$framework.framework/Versions/A"
  mkdir -p "$app/Contents/Frameworks/$framework.framework/Resources"
  touch "$app/Contents/Frameworks/$framework.framework/Versions/A/$framework"
  chmod +x "$app/Contents/Frameworks/$framework.framework/Versions/A/$framework"
  touch "$app/Contents/Frameworks/$framework.framework/Resources/Info.plist"
done
exit 0
STUB

cat >"$FAKEBIN/qtpaths6" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "--qt-version" ]; then
  echo "6.11.2"
  exit 0
fi
if [ "${1:-}" = "--query" ] && [ "${2:-}" = "QT_INSTALL_LIBS" ]; then
  printf '%s\n' "$FAKE_QT_LIBS_DIR"
  exit 0
fi
exit 1
STUB

cat >"$FAKEBIN/plutil" <<'STUB'
#!/usr/bin/env bash
echo "6.11.2"
STUB

cat >"$FAKEBIN/codesign" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB

cat >"$FAKEBIN/defaults" <<'STUB'
#!/usr/bin/env bash
echo "1.0.0"
STUB

cat >"$FAKEBIN/file" <<'STUB'
#!/usr/bin/env bash
case "$*" in
  *PXTOOL|*QtGui|*.dylib|*.so)
    echo "Mach-O 64-bit arm64"
    ;;
  *)
    echo "ASCII text"
    ;;
esac
STUB

cat >"$FAKEBIN/hdiutil" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail

case "${1:-}" in
  info)
    printf 'image-path      : %s\n' "$FAKE_HDIUTIL_IMAGE"
    printf 'system-entities :\n'
    printf '    dev-entry   : /dev/disk99\n'
    ;;
  detach)
    printf '%s\n' "$*" >>"$FAKE_HDIUTIL_LOG"
    ;;
esac
exit 0
STUB

cat >"$FAKEBIN/create-dmg" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
args=("$@")
out="${args[$#-2]}"
src="${args[$#-1]}"
base="$(basename "$out")"

if [ -e "$out" ]; then
  echo "hdiutil: convert failed - file already exists" >&2
  exit 1
fi

for stale in "$src"/rw.*."$base"; do
  if [ -e "$stale" ]; then
    echo "hdiutil: create failed - stale read-write image exists" >&2
    exit 1
  fi
done

printf 'new dmg\n' >"$out"
STUB

chmod +x "$FAKEBIN"/*

if ! FAKE_HDIUTIL_IMAGE="$STALE_RW" \
  FAKE_HDIUTIL_LOG="$WORKDIR/hdiutil.log" \
  FAKE_INSTALL_NAME_TOOL_LOG="$WORKDIR/install-name-tool.log" \
  FAKE_MACDEPLOYQT_MARKER="$MACDEPLOYQT_MARKER" \
  FAKE_PY_HOMEBREW="$PY_HOMEBREW" \
  FAKE_QT_INSTALL_NAME="$QT_INSTALL_NAME" \
  FAKE_QT_LIBS_DIR="$QT_LIBS_DIR" \
  PATH="$FAKEBIN:$PATH" \
  "$SCRIPT_UNDER_TEST" --skip-build >"$WORKDIR/run.log" 2>&1; then
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- \
  "-change $PY_HOMEBREW @loader_path/../Python $PY_BIN" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected package script to restore the bundled Python CLI import." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- \
  "-id @executable_path/../Frameworks/Python.framework/Versions/3.13/Python $PY_LIBRARY" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected package script to set the bundled Python framework install name." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- \
  "-change $PY_HOMEBREW @loader_path/../../../../Python $PY_APP_BIN" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected package script to restore the bundled Python.app import." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- \
  "-change $QT_LOCAL_BINARY $QT_INSTALL_NAME $APP/Contents/MacOS/PXTOOL" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected package script to restore the local QtGui import before cleanup." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- \
  "-change @rpath/Carbon.framework/Versions/A/Carbon /System/Library/Frameworks/Carbon.framework/Versions/A/Carbon $APP/Contents/Frameworks/QtGui.framework/Versions/A/QtGui" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected package script to restore the Carbon system-framework import." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

for framework in QtQml QtQmlMeta QtQmlModels QtQmlWorkerScript QtQuick; do
  if [ -e "$APP/Contents/Frameworks/$framework.framework" ]; then
    echo "Expected package script to remove optional $framework.framework." >&2
    cat "$WORKDIR/run.log" >&2
    exit 1
  fi
done

if [ "$(cat "$DMG_OUT")" != "new dmg" ]; then
  echo "Expected package script to recreate the final DMG." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if [ -e "$STALE_RW" ]; then
  echo "Expected package script to remove stale create-dmg read-write image." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -q '/dev/disk99' "$WORKDIR/hdiutil.log"; then
  echo "Expected package script to detach the mounted stale read-write image." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

echo "package-macos DMG cleanup test passed"
