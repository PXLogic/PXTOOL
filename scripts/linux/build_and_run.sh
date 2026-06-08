#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/build.linux"
APP_PATH="${OUTPUT_DIR}/DSView"
SHARE_DIR="${ROOT_DIR}/share"
SPI_OUTPUT_PATH="${OUTPUT_DIR}/spi.so"
SPI_MODULE_PATH="${BUILD_DIR}/spi.so"

cd "${ROOT_DIR}"

echo "[1/4] Configure"
CMAKE_ARGS=(-S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo)
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
    CMAKE_ARGS+=(-G Ninja)
fi
cmake "${CMAKE_ARGS[@]}"

echo "[2/4] Build"
cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 4)"

if [ ! -x "${APP_PATH}" ]; then
    echo "ERROR: App not found: ${APP_PATH}"
    exit 1
fi

echo "[3/4] Stage runtime resources"
mkdir -p "${SHARE_DIR}/DSView" "${SHARE_DIR}/libsigrokdecode4DSL"
cmake -E copy_directory "${ROOT_DIR}/DSView/res" "${SHARE_DIR}/DSView/res"
cmake -E copy_directory "${ROOT_DIR}/DSView/demo" "${SHARE_DIR}/DSView/demo"
cmake -E copy_directory "${ROOT_DIR}/lang" "${SHARE_DIR}/DSView/lang"
cmake -E copy_directory "${ROOT_DIR}/libsigrokdecode4DSL/decoders" "${SHARE_DIR}/libsigrokdecode4DSL/decoders"
if [ -f "${SPI_OUTPUT_PATH}" ]; then
    mkdir -p "${SHARE_DIR}/DSView/cdecoders"
    cmake -E copy_if_different "${SPI_OUTPUT_PATH}" "${SHARE_DIR}/DSView/cdecoders/spi.so"
elif [ -f "${SPI_MODULE_PATH}" ]; then
    mkdir -p "${SHARE_DIR}/DSView/cdecoders"
    cmake -E copy_if_different "${SPI_MODULE_PATH}" "${SHARE_DIR}/DSView/cdecoders/spi.so"
fi

echo "[4/4] Launch"
pkill -x DSView 2>/dev/null && sleep 1 && echo "Stopped running DSView" || true
exec "${APP_PATH}"
