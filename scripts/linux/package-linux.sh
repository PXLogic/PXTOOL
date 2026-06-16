#!/usr/bin/env bash
set -euo pipefail

# Build a Linux Debian package from the CMake install tree.
#
# Usage:
#   bash scripts/linux/package-linux.sh [--skip-build]
#
# Output:
#   build.linux/package/pxtool_<version>_<arch>.deb

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/build.linux"
STAGE_DIR="${OUTPUT_DIR}/package-root"
DIST_DIR="${OUTPUT_DIR}/package"
VERSION="1.0.0"
DEB_ARCH="$(dpkg --print-architecture 2>/dev/null || true)"
if [ -z "${DEB_ARCH}" ]; then
    case "$(uname -m)" in
        x86_64) DEB_ARCH="amd64" ;;
        aarch64|arm64) DEB_ARCH="arm64" ;;
        armv7l) DEB_ARCH="armhf" ;;
        *) DEB_ARCH="$(uname -m)" ;;
    esac
fi
PACKAGE_NAME="pxtool_${VERSION}_${DEB_ARCH}"
PACKAGE_PATH="${DIST_DIR}/${PACKAGE_NAME}.deb"

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

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "ERROR: dpkg-deb is required to create a .deb package."
    exit 1
fi

if [ "${SKIP_BUILD}" -eq 0 ]; then
    echo "[1/4] Configure"
    CMAKE_ARGS=(-S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr)
    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ] && command -v ninja >/dev/null 2>&1; then
        CMAKE_ARGS+=(-G Ninja)
    fi
    cmake "${CMAKE_ARGS[@]}"

    echo "[2/4] Build"
    cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 4)"
    cmake --build "${BUILD_DIR}" --target webui --parallel 1
else
    echo "[1/4] Configure/build skipped"
    if [ ! -f "${ROOT_DIR}/web/dist/index.html" ]; then
        echo "ERROR: --skip-build was used but web/dist/index.html is missing."
        echo "       Run without --skip-build, or run: cmake --build ${BUILD_DIR} --target webui"
        exit 1
    fi
fi

echo "[3/4] Install into staging root"
rm -rf "${STAGE_DIR}"
DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}" --prefix /usr --strip

if [ ! -f "${STAGE_DIR}/usr/bin/webui/index.html" ]; then
    echo "ERROR: MCP browser Web Console missing from package root at /usr/bin/webui/index.html"
    exit 1
fi
if [ ! -d "${STAGE_DIR}/usr/share/libsigrokdecode/decoders/c_decoders" ]; then
    echo "ERROR: C decoder modules missing from package root at /usr/share/libsigrokdecode/decoders/c_decoders"
    exit 1
fi

echo "[4/4] Create Debian package"
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}"

mkdir -p "${STAGE_DIR}/DEBIAN"
cat > "${STAGE_DIR}/DEBIAN/control" <<EOF
Package: pxtool
Version: ${VERSION}
Section: electronics
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: DreamSourceLab <support@dreamsourcelab.com>
Depends: libc6, libstdc++6, libqt5core5a, libqt5gui5, libqt5widgets5, libqt5svg5, libglib2.0-0, libusb-1.0-0, zlib1g, libfftw3-double3
Description: PXTOOL logic analyzer application
 PXTOOL is a Qt-based logic analyzer application with bundled decoders,
 firmware resources, desktop integration, and udev rules for USB access.
EOF

cat > "${STAGE_DIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e

if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || true
    udevadm trigger --subsystem-match=usb || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi

exit 0
EOF

cat > "${STAGE_DIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e

if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || true
    udevadm trigger --subsystem-match=usb || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi

exit 0
EOF

chmod 0755 "${STAGE_DIR}/DEBIAN/postinst" "${STAGE_DIR}/DEBIAN/postrm"
find "${STAGE_DIR}" -type d -exec chmod 0755 {} +
dpkg-deb --root-owner-group --build "${STAGE_DIR}" "${PACKAGE_PATH}"

echo "Package created:"
echo "  ${PACKAGE_PATH}"
