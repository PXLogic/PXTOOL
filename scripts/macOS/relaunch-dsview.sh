#!/usr/bin/env bash
# Rebuild PXTOOL and restart the app bundle (macOS).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
make -j8
pkill -f "build.macOS/PXTOOL.app/Contents/MacOS/PXTOOL" 2>/dev/null || true
sleep 0.35
open "$ROOT/build.macOS/PXTOOL.app"
