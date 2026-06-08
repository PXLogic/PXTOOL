#!/usr/bin/env bash
set -euo pipefail

# Build a Linux tarball from the CMake install tree.
#
# Usage:
#   bash scripts/linux/package-linux.sh [--skip-build]
#
# Output:
#   build.linux/package/PXTOOL-<version>-linux-x86_64.tar.gz

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/build.linux"
STAGE_DIR="${OUTPUT_DIR}/package-root"
DIST_DIR="${OUTPUT_DIR}/package"
VERSION="1.0.0"
ARCH="$(uname -m)"
PACKAGE_NAME="PXTOOL-${VERSION}-linux-${ARCH}"
PACKAGE_PATH="${DIST_DIR}/${PACKAGE_NAME}.tar.gz"

SKIP_BUILD=0
for arg in "$@"; do
    case "${arg}" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help)
            sed -n '3,11p' "$0"
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: ${arg}"
            exit 1
            ;;
    esac
done

cd "${ROOT_DIR}"

if [ "${SKIP_BUILD}" -eq 0 ]; then
    echo "[1/4] Configure"
    CMAKE_ARGS=(-S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr)
    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
        CMAKE_ARGS+=(-G Ninja)
    fi
    cmake "${CMAKE_ARGS[@]}"

    echo "[2/4] Build"
    cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 4)"
else
    echo "[1/4] Configure/build skipped"
fi

echo "[3/4] Install into staging root"
rm -rf "${STAGE_DIR}"
DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}" --prefix /usr --strip

echo "[4/4] Create tarball"
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}"
tar -C "${STAGE_DIR}" -czf "${PACKAGE_PATH}" .

echo "Package created:"
echo "  ${PACKAGE_PATH}"
