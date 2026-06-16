#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/build.linux"
APP_PATH="${OUTPUT_DIR}/PXTOOL"
SHARE_DIR="${ROOT_DIR}/share"
SPI_OUTPUT_PATH="${OUTPUT_DIR}/spi.so"
SPI_MODULE_PATH="${BUILD_DIR}/spi.so"
UDEV_RULES_PATH="/etc/udev/rules.d/60-dreamsourcelab.rules"

cd "${ROOT_DIR}"

echo "[1/4] Configure"
CMAKE_ARGS=(-S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo)
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
    CMAKE_ARGS+=(-G Ninja)
fi
cmake "${CMAKE_ARGS[@]}"

echo "[2/4] Build"
cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 4)"
cmake --build "${BUILD_DIR}" --target stage_webui --parallel 1

if [ ! -x "${APP_PATH}" ]; then
    echo "ERROR: App not found: ${APP_PATH}"
    exit 1
fi

if [ ! -f "${OUTPUT_DIR}/webui/index.html" ]; then
    echo "ERROR: MCP browser Web Console not found at ${OUTPUT_DIR}/webui/index.html"
    exit 1
fi

echo "[3/4] Stage runtime resources"
mkdir -p "${SHARE_DIR}/PXTOOL" "${SHARE_DIR}/libsigrokdecode"
cmake -E copy_directory "${ROOT_DIR}/PXTOOL/res" "${SHARE_DIR}/PXTOOL/res"
cmake -E copy_directory "${ROOT_DIR}/PXTOOL/demo" "${SHARE_DIR}/PXTOOL/demo"
cmake -E copy_directory "${ROOT_DIR}/lang" "${SHARE_DIR}/PXTOOL/lang"
cmake -E copy_directory "${ROOT_DIR}/libsigrokdecode/decoders" "${SHARE_DIR}/libsigrokdecode/decoders"
if [ -f "${SPI_OUTPUT_PATH}" ]; then
    mkdir -p "${SHARE_DIR}/PXTOOL/cdecoders"
    cmake -E copy_if_different "${SPI_OUTPUT_PATH}" "${SHARE_DIR}/PXTOOL/cdecoders/spi.so"
elif [ -f "${SPI_MODULE_PATH}" ]; then
    mkdir -p "${SHARE_DIR}/PXTOOL/cdecoders"
    cmake -E copy_if_different "${SPI_MODULE_PATH}" "${SHARE_DIR}/PXTOOL/cdecoders/spi.so"
fi

if [ ! -f "${UDEV_RULES_PATH}" ]; then
    echo "WARNING: USB udev rules are not installed."
    echo "         Run: bash scripts/linux/install_udev_rules.sh"
fi

echo "[4/4] Launch"
pkill -x PXTOOL 2>/dev/null && sleep 1 && echo "Stopped running PXTOOL" || true
exec "${APP_PATH}"
