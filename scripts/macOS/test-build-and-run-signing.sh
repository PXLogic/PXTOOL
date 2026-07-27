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
SRD_C_DECODER_BUILD_DIR="$WORKDIR/build.macOS/decoders/c_decoders"

mkdir -p \
  "$WORKDIR/scripts/macOS" \
  "$FAKEBIN" \
  "$WORKDIR/home" \
  "$APP/Contents/MacOS/webui" \
  "$PY_DYNLOAD" \
  "$PYCACHE_DIR" \
  "$QT_PLUGINS_DIR/platforms" \
  "$APP/Contents/Resources/share/PXTOOL/cdecoders" \
  "$SRD_C_DECODER_BUILD_DIR"

cp "$SOURCE_ROOT/scripts/macOS/build_and_run.sh" "$SCRIPT_UNDER_TEST"
cp "$SOURCE_ROOT/scripts/macOS/sign-macos-app.sh" "$SIGN_SCRIPT" 2>/dev/null || true
chmod +x "$SCRIPT_UNDER_TEST"
[ ! -f "$SIGN_SCRIPT" ] || chmod +x "$SIGN_SCRIPT"

touch "$APP/Contents/MacOS/PXTOOL"
touch "$APP/Contents/MacOS/webui/index.html"
touch "$PY_DYNLOAD/zlib.cpython-313-darwin.so"
mkdir -p "$QT_PLUGINS_DIR/platforms"
touch "$QT_PLUGINS_DIR/platforms/libqcocoa.dylib"
touch "$QT_CONF"
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
  FAKE_OPEN_LOG="$WORKDIR/open.log" \
  HOME="$WORKDIR/home" \
  PATH="$FAKEBIN:$PATH" \
  "$SCRIPT_UNDER_TEST" >"$WORKDIR/run.log" 2>&1; then
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

echo "build_and_run signing test passed"
