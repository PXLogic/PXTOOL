#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="${ROOT_DIR}/build.dir/DSView.app"
APP_NAME="DSView"

cd "${ROOT_DIR}"

echo "[1/3] Build (make)"
CPU_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
make -j"${CPU_COUNT}"

echo "[2/3] Kill existing instance"
pkill -x "${APP_NAME}" 2>/dev/null && sleep 1 && echo "Killed running ${APP_NAME}" || echo "No running ${APP_NAME} found"

echo "[3/3] Launch ${APP_NAME}"
open "${APP_PATH}"
