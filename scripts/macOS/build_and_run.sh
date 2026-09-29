#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
APP_NAME="PXTOOL"
APP_PATH="${ROOT_DIR}/build.macOS/${APP_NAME}.app"
SIGN_APP_SCRIPT="${ROOT_DIR}/scripts/macOS/sign-macos-app.sh"
APP_WEBUI_PATH="${APP_PATH}/Contents/MacOS/webui/index.html"
APP_CDECODER_DIR="${APP_PATH}/Contents/Resources/share/PXTOOL/cdecoders"
SPI_MODULE_PATH="${ROOT_DIR}/build.macOS/spi.dylib"
SRD_C_DECODER_BUILD_DIR="${ROOT_DIR}/build.macOS/decoders/c_decoders"
SRD_C_DECODER_APP_DIR="${APP_PATH}/Contents/Resources/share/libsigrokdecode/decoders/c_decoders"

# C decoder runtime directory must match GetUserDataDir()+"/cdecoders" in
# pv/config/appconfig.cpp (Qt QStandardPaths::AppDataLocation +
# QCoreApplication organization/application name -> DreamSourceLab/PXTOOL).
CDECODER_RUNTIME_DIR="${HOME}/Library/Application Support/DreamSourceLab/PXTOOL/cdecoders"

# Qt tool discovery is shared with package-macos.sh.
# shellcheck source=scripts/macOS/qt6_env.sh
. "${SCRIPT_DIR}/qt6_env.sh"

# Qt built by the official installer keeps @rpath-relative framework install
# names, so rewriting the executable's imports is a no-op there. Those builds
# instead need an LC_RPATH pointing at the local Qt library directory, because
# cleanup_packaged_qt_artifacts removes the copies under Contents/Frameworks.
ensure_local_qt_rpath() {
    local executable="$1"
    local qt_libs="$2"
    local load_commands existing_rpaths

    if ! load_commands="$(otool -l "${executable}" 2>&1)"; then
        echo "ERROR: otool could not read load commands: ${executable}"
        printf '%s\n' "${load_commands}"
        return 1
    fi
    existing_rpaths="$(printf '%s\n' "${load_commands}" \
        | awk '/LC_RPATH/ { in_rpath = 1; next }
               in_rpath && $1 == "path" { print $2; in_rpath = 0 }')"
    if printf '%s\n' "${existing_rpaths}" | grep -Fqx "${qt_libs}"; then
        return 0
    fi
    install_name_tool -add_rpath "${qt_libs}" "${executable}"
    echo "Added local Qt rpath: ${qt_libs}"
}

restore_local_qt_framework_imports() {
    local executable="${APP_PATH}/Contents/MacOS/${APP_NAME}"
    local qtpaths qt_libs dependencies dependency framework_suffix local_dependency install_name
    local needs_local_qt_rpath=0

    qtpaths="$(command -v qtpaths6 || true)"
    if [ -z "${qtpaths}" ]; then
        echo "ERROR: qtpaths6 was not found on PATH."
        return 1
    fi
    if ! qt_libs="$("${qtpaths}" --query QT_INSTALL_LIBS 2>&1)"; then
        echo "ERROR: qtpaths6 could not report QT_INSTALL_LIBS."
        printf '%s\n' "${qt_libs}"
        return 1
    fi
    if ! dependencies="$(otool -L "${executable}" 2>&1)"; then
        echo "ERROR: otool could not inspect ${executable}."
        printf '%s\n' "${dependencies}"
        return 1
    fi

    while IFS= read -r dependency; do
        case "${dependency}" in
            @executable_path/../Frameworks/Qt*.framework/*)
                framework_suffix="${dependency#@executable_path/../Frameworks/}"
                ;;
            @rpath/Qt*.framework/*)
                framework_suffix="${dependency#@rpath/}"
                ;;
            "${qt_libs}"/Qt*.framework/*)
                framework_suffix="${dependency#"${qt_libs}"/}"
                ;;
            *)
                continue
                ;;
        esac

        local_dependency="${qt_libs}/${framework_suffix}"
        if [ ! -f "${local_dependency}" ]; then
            echo "ERROR: local Qt framework dependency is missing: ${local_dependency}"
            return 1
        fi
        if ! install_name="$(otool -D "${local_dependency}" 2>&1 | awk 'NR == 2 { print; exit }')" \
            || [ -z "${install_name}" ]; then
            echo "ERROR: could not read Qt framework install name: ${local_dependency}"
            return 1
        fi
        case "${install_name}" in
            @rpath/*)
                # A -change to the same @rpath value would do nothing; dyld has
                # to resolve it through an rpath entry instead.
                needs_local_qt_rpath=1
                ;;
            *)
                install_name_tool -change "${dependency}" "${install_name}" "${executable}"
                ;;
        esac
    done < <(printf '%s\n' "${dependencies}" | awk 'NR > 1 { print $1 }')

    if [ "${needs_local_qt_rpath}" -eq 1 ]; then
        ensure_local_qt_rpath "${executable}" "${qt_libs}" || return 1
    fi
}

cleanup_packaged_qt_artifacts() {
    rm -f "${APP_PATH}/Contents/Resources/qt.conf"
    rm -rf "${APP_PATH}/Contents/PlugIns"
    if [ -d "${APP_PATH}/Contents/Frameworks" ]; then
        find "${APP_PATH}/Contents/Frameworks" -type d -name 'Qt*.framework' -prune -exec rm -rf {} +
        find "${APP_PATH}/Contents/Frameworks" -type f -iname '*qt*.dylib' -delete
    fi
}

cd "${ROOT_DIR}"

require_qt6_tools_on_path "${ROOT_DIR}"

echo "[1/4] Configure upstream-compat demo and build"
CPU_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
cmake . -DDSVIEW_ENABLE_UPSTREAM_COMPAT_DEMO=ON
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

restore_local_qt_framework_imports
cleanup_packaged_qt_artifacts

echo "[4/5] Re-sign app bundle"
"${SIGN_APP_SCRIPT}" "${APP_PATH}" "${CDECODER_RUNTIME_DIR}/spi.dylib"

echo "[5/5] Kill existing instance and launch"
pkill -x "${APP_NAME}" 2>/dev/null && sleep 1 && echo "Killed running ${APP_NAME}" || echo "No running ${APP_NAME} found"
open "${APP_PATH}"
