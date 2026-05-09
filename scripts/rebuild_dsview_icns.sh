#!/usr/bin/env bash
# Regenerate DSView.icns from DSView/icons/titlebar_wave_icon.svg (TitleBar stripe logo).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SVG="$ROOT/DSView/icons/titlebar_wave_icon.svg"
OUT="$ROOT/DSView.icns"
TMP=$(mktemp -d)
PNG="$TMP/base.png"
IST="$TMP/icon.iconset"

mkdir -p "$IST"
qlmanage -t -s 1024 -o "$TMP" "$SVG"
mv "$TMP/$(basename "$SVG").png" "$PNG"

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
