#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_NAME="PXTOOL"
APP_PATH="${ROOT_DIR}/build.dir/${APP_NAME}.app"

# C decoder runtime directory must match GetUserDataDir()+"/cdecoders" in
# pv/config/appconfig.cpp (Qt QStandardPaths::AppDataLocation +
# QCoreApplication organization/application name -> DreamSourceLab/PXTOOL).
CDECODER_RUNTIME_DIR="${HOME}/Library/Application Support/DreamSourceLab/PXTOOL/cdecoders"

cd "${ROOT_DIR}"

echo "[1/4] Build (make)"
CPU_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
make -j"${CPU_COUNT}"

echo "[2/4] Build example C decoders (if any)"
for builddir in pv/cdecoders/example_*/build; do
    [ -d "${builddir}" ] || continue
    echo "  - $(dirname "${builddir}")"
    (cd "${builddir}" && cmake --build . -j"${CPU_COUNT}" >/dev/null)
done

echo "[3/4] Deploy example C decoder dylibs to runtime cdecoders dir"
mkdir -p "${CDECODER_RUNTIME_DIR}"
for src in pv/cdecoders/example_*/build/*.dylib; do
    [ -f "${src}" ] || continue
    cp -v "${src}" "${CDECODER_RUNTIME_DIR}/"
done

echo "[4/4] Kill existing instance and launch"
pkill -x "${APP_NAME}" 2>/dev/null && sleep 1 && echo "Killed running ${APP_NAME}" || echo "No running ${APP_NAME} found"
open "${APP_PATH}"
