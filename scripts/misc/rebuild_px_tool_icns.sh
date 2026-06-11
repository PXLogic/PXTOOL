#!/usr/bin/env bash
# Regenerate the macOS app icon from PXTOOL/icons/dock_app_icon.png.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PNG="$ROOT/PXTOOL/icons/dock_app_icon.png"
OUT="$ROOT/PXTOOL.icns"
TMP=$(mktemp -d)
IST="$TMP/icon.iconset"

mkdir -p "$IST"
sips -z 16 16     "$PNG" --out "$IST/icon_16x16.png"
sips -z 32 32     "$PNG" --out "$IST/icon_16x16@2x.png"
sips -z 32 32     "$PNG" --out "$IST/icon_32x32.png"
sips -z 64 64     "$PNG" --out "$IST/icon_32x32@2x.png"
sips -z 128 128   "$PNG" --out "$IST/icon_128x128.png"
sips -z 256 256   "$PNG" --out "$IST/icon_128x128@2x.png"
sips -z 256 256   "$PNG" --out "$IST/icon_256x256.png"
sips -z 512 512   "$PNG" --out "$IST/icon_256x256@2x.png"
sips -z 512 512   "$PNG" --out "$IST/icon_512x512.png"
sips -z 1024 1024 "$PNG" --out "$IST/icon_512x512@2x.png"

iconutil -c icns "$IST" -o "$OUT"
rm -rf "$TMP"
echo "Wrote $OUT"
