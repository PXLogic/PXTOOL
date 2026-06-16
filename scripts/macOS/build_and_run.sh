#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_NAME="PXTOOL"
APP_PATH="${ROOT_DIR}/build.macOS/${APP_NAME}.app"
APP_WEBUI_PATH="${APP_PATH}/Contents/MacOS/webui/index.html"
APP_CDECODER_DIR="${APP_PATH}/Contents/Resources/share/PXTOOL/cdecoders"
SPI_MODULE_PATH="${ROOT_DIR}/build.macOS/spi.dylib"
SRD_C_DECODER_BUILD_DIR="${ROOT_DIR}/build.macOS/decoders/c_decoders"
SRD_C_DECODER_APP_DIR="${APP_PATH}/Contents/Resources/share/libsigrokdecode/decoders/c_decoders"

# C decoder runtime directory must match GetUserDataDir()+"/cdecoders" in
# pv/config/appconfig.cpp (Qt QStandardPaths::AppDataLocation +
# QCoreApplication organization/application name -> DreamSourceLab/PXTOOL).
CDECODER_RUNTIME_DIR="${HOME}/Library/Application Support/DreamSourceLab/PXTOOL/cdecoders"

cd "${ROOT_DIR}"

echo "[1/4] Build (make)"
CPU_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
make -j"${CPU_COUNT}"
cmake --build "${ROOT_DIR}" --target stage_webui --parallel 1

if [ ! -f "${APP_WEBUI_PATH}" ]; then
    echo "ERROR: MCP browser Web Console not found at ${APP_WEBUI_PATH}"
    exit 1
fi

echo "[2/4] Verify bundled C decoder"
if [ ! -f "${SPI_MODULE_PATH}" ] && [ ! -f "${APP_CDECODER_DIR}/spi.dylib" ]; then
    echo "ERROR: spi.dylib not found. Re-run CMake configure so the spi target is available."
    exit 1
fi

echo "[3/4] Deploy bundled C decoder dylib to runtime cdecoders dir"
if [ ! -d "${SRD_C_DECODER_BUILD_DIR}" ]; then
    echo "ERROR: built libsigrokdecode C decoder directory not found: ${SRD_C_DECODER_BUILD_DIR}"
    exit 1
fi
rm -rf "${SRD_C_DECODER_APP_DIR}"
mkdir -p "${SRD_C_DECODER_APP_DIR}"
cp -R "${SRD_C_DECODER_BUILD_DIR}/." "${SRD_C_DECODER_APP_DIR}/"

mkdir -p "${CDECODER_RUNTIME_DIR}"
if [ -f "${SPI_MODULE_PATH}" ]; then
    cp -v "${SPI_MODULE_PATH}" "${CDECODER_RUNTIME_DIR}/spi.dylib"
else
    cp -v "${APP_CDECODER_DIR}/spi.dylib" "${CDECODER_RUNTIME_DIR}/spi.dylib"
fi

echo "[4/4] Kill existing instance and launch"
pkill -x "${APP_NAME}" 2>/dev/null && sleep 1 && echo "Killed running ${APP_NAME}" || echo "No running ${APP_NAME} found"
open "${APP_PATH}"
