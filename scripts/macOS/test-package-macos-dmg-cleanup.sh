#!/usr/bin/env bash
# Regression test for rerunning package-macos.sh with existing DMG artifacts.

set -euo pipefail

SOURCE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

SCRIPT_UNDER_TEST="$WORKDIR/scripts/macOS/package-macos.sh"
SIGN_SCRIPT="$WORKDIR/scripts/macOS/sign-macos-app.sh"
FAKEBIN="$WORKDIR/bin"
APP="$WORKDIR/build.macOS/PXTOOL.app"
PKG_APP="$WORKDIR/package-root/PXTOOL.app"
DMG_OUT="$WORKDIR/build.macOS/PXTOOL-1.0.0-arm64-macOS.dmg"
STALE_RW="$WORKDIR/build.macOS/rw.12345.PXTOOL-1.0.0-arm64-macOS.dmg"

mkdir -p \
  "$WORKDIR/scripts/macOS" \
  "$FAKEBIN" \
  "$WORKDIR/web/dist" \
  "$APP/Contents/MacOS/webui" \
  "$APP/Contents/Resources" \
  "$PKG_APP/Contents/Resources"

cp "$SOURCE_ROOT/scripts/macOS/package-macos.sh" "$SCRIPT_UNDER_TEST"
cp "$SOURCE_ROOT/scripts/macOS/sign-macos-app.sh" "$SIGN_SCRIPT"
chmod +x "$SCRIPT_UNDER_TEST"
chmod +x "$SIGN_SCRIPT"

touch "$APP/Contents/MacOS/PXTOOL"
touch "$APP/Contents/MacOS/webui/index.html"
touch "$APP/Contents/Resources/PXTOOL.icns"
touch "$APP/Contents/Info.plist"
touch "$WORKDIR/web/dist/index.html"
printf 'old dmg\n' >"$DMG_OUT"
printf 'stale read-write image\n' >"$STALE_RW"

cat >"$FAKEBIN/install_name_tool" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB

cat >"$FAKEBIN/otool" <<'STUB'
#!/usr/bin/env bash
exit 0
STUB

cat >"$FAKEBIN/macdeployqt" <<'STUB'
#!/usr/bin/env bash
exit 0
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
  *PXTOOL|*.dylib|*.so)
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
  PATH="$FAKEBIN:$PATH" \
  "$SCRIPT_UNDER_TEST" --skip-build >"$WORKDIR/run.log" 2>&1; then
  cat "$WORKDIR/run.log" >&2
  exit 1
fi

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
