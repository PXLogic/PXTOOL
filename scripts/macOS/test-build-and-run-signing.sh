#!/usr/bin/env bash
# Regression test for build_and_run.sh re-signing the app before launch.

set -euo pipefail

SOURCE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

SCRIPT_UNDER_TEST="$WORKDIR/scripts/macOS/build_and_run.sh"
SIGN_SCRIPT="$WORKDIR/scripts/macOS/sign-macos-app.sh"
FAKEBIN="$WORKDIR/bin"
APP="$WORKDIR/build.macOS/PXTOOL.app"
PY_DYNLOAD="$APP/Contents/Frameworks/Python.framework/Versions/3.13/lib/python3.13/lib-dynload"
BROKEN_SITE_PACKAGES="$APP/Contents/Frameworks/Python.framework/Versions/3.13/lib/python3.13/site-packages"
PYCACHE_DIR="$APP/Contents/Resources/share/libsigrokdecode/decoders/spi/__pycache__"
QT_CONF="$APP/Contents/Resources/qt.conf"
QT_PLUGINS_DIR="$APP/Contents/PlugIns"
QT_FRAMEWORK="$APP/Contents/Frameworks/QtGui.framework"
QT_DYLIB="$APP/Contents/Frameworks/libQt6Example.dylib"
QT_LIBS_DIR="$WORKDIR/qt/lib"
QT_LOCAL_BINARY="$QT_LIBS_DIR/QtGui.framework/Versions/A/QtGui"
QT_INSTALL_NAME="/opt/homebrew/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui"
SRD_C_DECODER_BUILD_DIR="$WORKDIR/build.macOS/decoders/c_decoders"

mkdir -p \
  "$WORKDIR/scripts/macOS" \
  "$FAKEBIN" \
  "$WORKDIR/home" \
  "$APP/Contents/MacOS/webui" \
  "$PY_DYNLOAD" \
  "$QT_FRAMEWORK/Versions/A" \
  "$QT_LIBS_DIR/QtGui.framework/Versions/A" \
  "$PYCACHE_DIR" \
  "$QT_PLUGINS_DIR/platforms" \
  "$APP/Contents/Resources/share/PXTOOL/cdecoders" \
  "$SRD_C_DECODER_BUILD_DIR"

cp "$SOURCE_ROOT/scripts/macOS/build_and_run.sh" "$SCRIPT_UNDER_TEST"
cp "$SOURCE_ROOT/scripts/macOS/qt6_env.sh" "$WORKDIR/scripts/macOS/qt6_env.sh"
cp "$SOURCE_ROOT/scripts/macOS/sign-macos-app.sh" "$SIGN_SCRIPT" 2>/dev/null || true
chmod +x "$SCRIPT_UNDER_TEST"
[ ! -f "$SIGN_SCRIPT" ] || chmod +x "$SIGN_SCRIPT"

touch "$APP/Contents/MacOS/PXTOOL"
touch "$APP/Contents/MacOS/webui/index.html"
touch "$PY_DYNLOAD/zlib.cpython-313-darwin.so"
mkdir -p "$QT_PLUGINS_DIR/platforms"
touch "$QT_PLUGINS_DIR/platforms/libqcocoa.dylib"
touch "$QT_CONF"
touch "$QT_FRAMEWORK/Versions/A/QtGui"
touch "$QT_DYLIB"
touch "$QT_LOCAL_BINARY"
ln -s ../../../../../../lib/python3.13/site-packages "$BROKEN_SITE_PACKAGES"
touch "$PYCACHE_DIR/__init__.cpython-313.pyc"
touch "$WORKDIR/build.macOS/spi.dylib"
touch "$SRD_C_DECODER_BUILD_DIR/spi.dylib"

cat >"$FAKEBIN/sysctl" <<'STUB'
#!/usr/bin/env bash
echo 8
STUB

cat >"$FAKEBIN/cmake" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB

cat >"$FAKEBIN/make" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB

cat >"$FAKEBIN/pkill" <<'STUB'
#!/usr/bin/env bash
exit 1
STUB

cat >"$FAKEBIN/codesign" <<'STUB'
#!/usr/bin/env bash
printf 'codesign %s\n' "$*" >>"$FAKE_CODESIGN_LOG"
exit 0
STUB

cat >"$FAKEBIN/install_name_tool" <<'STUB'
#!/usr/bin/env bash
printf 'install_name_tool %s\n' "$*" >>"$FAKE_INSTALL_NAME_TOOL_LOG"
exit 0
STUB

cat >"$FAKEBIN/otool" <<'STUB'
#!/usr/bin/env bash
if [ "${1:-}" = "-D" ]; then
  printf '%s:\n%s\n' "${2:-}" "$FAKE_QT_INSTALL_NAME"
  exit 0
fi
if [ "${1:-}" = "-l" ]; then
  printf '%s:\n' "${2:-}"
  for rpath in ${FAKE_EXISTING_RPATHS:-}; do
    printf '          cmd LC_RPATH\n      cmdsize 48\n         path %s (offset 12)\n' "$rpath"
  done
  exit 0
fi
printf '%s:\n' "${2:-}"
printf '\t@executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui (compatibility version 6.0.0, current version 6.11.2)\n'
STUB

cat >"$FAKEBIN/qtpaths6" <<'STUB'
#!/usr/bin/env bash
if [ "${1:-}" = "--query" ] && [ "${2:-}" = "QT_INSTALL_LIBS" ]; then
  printf '%s\n' "$FAKE_QT_LIBS_DIR"
  exit 0
fi
exit 1
STUB

cat >"$FAKEBIN/file" <<'STUB'
#!/usr/bin/env bash
case "$*" in
  *PXTOOL|*.dylib|*.so)
    echo "Mach-O 64-bit arm64"
    ;;
  *)
    echo "ASCII text"
    ;;
esac
STUB

cat >"$FAKEBIN/open" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail

if ! grep -q -- 'zlib.cpython-313-darwin.so' "$FAKE_CODESIGN_LOG"; then
  echo "Expected embedded Python extension modules to be re-signed before open." >&2
  exit 1
fi

if ! grep -q -- 'spi.dylib' "$FAKE_CODESIGN_LOG"; then
  echo "Expected runtime C decoder dylibs to be re-signed before open." >&2
  exit 1
fi

if ! grep -q -- '--verify --deep --strict' "$FAKE_CODESIGN_LOG"; then
  echo "Expected app signature to be verified before open." >&2
  exit 1
fi

printf 'open %s\n' "$*" >"$FAKE_OPEN_LOG"
STUB

chmod +x "$FAKEBIN"/*

if ! FAKE_CODESIGN_LOG="$WORKDIR/codesign.log" \
  FAKE_INSTALL_NAME_TOOL_LOG="$WORKDIR/install-name-tool.log" \
  FAKE_OPEN_LOG="$WORKDIR/open.log" \
  FAKE_QT_INSTALL_NAME="$QT_INSTALL_NAME" \
  FAKE_QT_LIBS_DIR="$QT_LIBS_DIR" \
  HOME="$WORKDIR/home" \
  PATH="$FAKEBIN:$PATH" \
  "$SCRIPT_UNDER_TEST" >"$WORKDIR/run.log" 2>&1; then
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- \
  "-change @executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui $QT_INSTALL_NAME $APP/Contents/MacOS/PXTOOL" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected build_and_run.sh to restore the local QtGui import before cleanup." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -q "open $APP" "$WORKDIR/open.log"; then
  echo "Expected build_and_run.sh to launch the staged app." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if [ -L "$BROKEN_SITE_PACKAGES" ]; then
  echo "Expected build_and_run.sh signing helper to remove broken Python symlinks." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if [ -d "$PYCACHE_DIR" ]; then
  echo "Expected build_and_run.sh signing helper to remove Python bytecode caches." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if [ -e "$QT_CONF" ] || [ -e "$QT_PLUGINS_DIR" ]; then
  echo "Expected build_and_run.sh to remove packaged Qt deployment artifacts before launch." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if [ -e "$QT_FRAMEWORK" ] || [ -e "$QT_DYLIB" ]; then
  echo "Expected build_and_run.sh to remove stale packaged Qt libraries before launch." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if [ ! -d "$APP/Contents/Frameworks/Python.framework" ]; then
  echo "Expected build_and_run.sh to preserve non-Qt frameworks." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

# The run above covers Homebrew's Qt, whose frameworks have absolute install
# names and are fixed up with install_name_tool -change. Qt from the official
# online installer or aqtinstall is different: its frameworks carry @rpath
# install names, so -change would rewrite @rpath to the identical @rpath and do
# nothing. Those builds need an LC_RPATH pointing at the local Qt lib directory,
# because cleanup_packaged_qt_artifacts removes the in-bundle Qt frameworks.
#
# That branch used to be untested, and it cost a real regression: a refactor
# deleted ensure_local_qt_rpath() outright and every test here still passed,
# because this stub reports an absolute install name. The runs below exercise it.
run_build_and_run() { # run_build_and_run <qt_install_name> [existing_rpaths]
  : >"$WORKDIR/install-name-tool.log"
  : >"$WORKDIR/codesign.log"
  touch "$PY_DYNLOAD/zlib.cpython-313-darwin.so"

  FAKE_CODESIGN_LOG="$WORKDIR/codesign.log" \
  FAKE_INSTALL_NAME_TOOL_LOG="$WORKDIR/install-name-tool.log" \
  FAKE_OPEN_LOG="$WORKDIR/open.log" \
  FAKE_QT_INSTALL_NAME="$1" \
  FAKE_QT_LIBS_DIR="$QT_LIBS_DIR" \
  FAKE_EXISTING_RPATHS="${2:-}" \
  HOME="$WORKDIR/home" \
  PATH="$FAKEBIN:$PATH" \
  "$SCRIPT_UNDER_TEST" >"$WORKDIR/run.log" 2>&1
}

RPATH_INSTALL_NAME="@rpath/QtGui.framework/Versions/A/QtGui"

if ! run_build_and_run "$RPATH_INSTALL_NAME"; then
  echo "Expected build_and_run.sh to succeed for @rpath Qt install names." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if ! grep -Fq -- "-add_rpath $QT_LIBS_DIR $APP/Contents/MacOS/PXTOOL" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected an LC_RPATH to the local Qt libs when Qt uses @rpath install names." >&2
  cat "$WORKDIR/run.log" >&2
  cat "$WORKDIR/install-name-tool.log" >&2
  exit 1
fi

if grep -Fq -- "-change $RPATH_INSTALL_NAME $RPATH_INSTALL_NAME" \
  "$WORKDIR/install-name-tool.log"; then
  echo "Expected no no-op -change when the install name is already @rpath-relative." >&2
  cat "$WORKDIR/install-name-tool.log" >&2
  exit 1
fi

# Re-running must not stack a second copy of the same LC_RPATH.
if ! run_build_and_run "$RPATH_INSTALL_NAME" "$QT_LIBS_DIR"; then
  echo "Expected build_and_run.sh to succeed when the Qt rpath already exists." >&2
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

if grep -Fq -- "-add_rpath $QT_LIBS_DIR" "$WORKDIR/install-name-tool.log"; then
  echo "Expected no duplicate LC_RPATH when the Qt rpath is already present." >&2
  cat "$WORKDIR/install-name-tool.log" >&2
  exit 1
fi

echo "build_and_run signing test passed"
