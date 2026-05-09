#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${ROOT_DIR}/package-root"
OPEN_APP=1

for arg in "$@"; do
  case "${arg}" in
    --no-open)
      OPEN_APP=0
      ;;
    *)
      echo "Unknown option: ${arg}"
      echo "Usage: $(basename "$0") [--no-open]"
      exit 1
      ;;
  esac
done

cd "${ROOT_DIR}"

echo "[1/4] Configure (CMake)"
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" .

echo "[2/4] Build (make)"
CPU_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
make -j"${CPU_COUNT}"

echo "[3/4] Package image (DMG)"
cpack -G DragNDrop

LATEST_DMG=""
for dmg in "${ROOT_DIR}"/DSView-*-Darwin.dmg; do
  if [[ -f "${dmg}" ]]; then
    LATEST_DMG="${dmg}"
  fi
done

if [[ -n "${LATEST_DMG}" ]]; then
  echo "DMG generated: ${LATEST_DMG}"
else
  echo "Warning: no DMG found with pattern DSView-*-Darwin.dmg"
fi

if [[ "${OPEN_APP}" -eq 1 ]]; then
  echo "[4/4] Launch app"
  open "${ROOT_DIR}/build.dir/DSView.app"
else
  echo "[4/4] Skip app launch (--no-open)"
fi
