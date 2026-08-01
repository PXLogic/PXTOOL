#!/usr/bin/env bash

qt6_init() {
    local qmake_bin="${QT6_QMAKE_BIN:-}"
    if [ -z "${qmake_bin}" ]; then
        qmake_bin="$(command -v qmake6 2>/dev/null || true)"
    fi
    if [ -z "${qmake_bin}" ] || [ ! -x "${qmake_bin}" ]; then
        echo "ERROR: qmake6 is required to configure PXTOOL with Qt6." >&2
        return 1
    fi

    local qt_version
    qt_version="$(${qmake_bin} -query QT_VERSION 2>/dev/null || true)"
    case "${qt_version}" in
        6.*) ;;
        *)
            echo "ERROR: qmake6 resolved to an unexpected Qt version: ${qt_version:-unknown}" >&2
            return 1
            ;;
    esac

    local qt6_bin_dir
    local qt6_lib_dir
    qt6_bin_dir="$(${qmake_bin} -query QT_INSTALL_BINS 2>/dev/null || true)"
    qt6_lib_dir="$(${qmake_bin} -query QT_INSTALL_LIBS 2>/dev/null || true)"
    if [ -z "${qt6_bin_dir}" ] || [ -z "${qt6_lib_dir}" ]; then
        echo "ERROR: qmake6 did not report Qt6 tool and library directories." >&2
        return 1
    fi

    local qt6_cmake_dir="${qt6_lib_dir}/cmake/Qt6"
    if [ ! -f "${qt6_cmake_dir}/Qt6Config.cmake" ]; then
        echo "ERROR: Qt6 CMake package not found: ${qt6_cmake_dir}/Qt6Config.cmake" >&2
        return 1
    fi
    if [ ! -x "${qt6_bin_dir}/lrelease" ]; then
        echo "ERROR: Qt6 lrelease not found: ${qt6_bin_dir}/lrelease" >&2
        return 1
    fi
    if ! "${qt6_bin_dir}/lrelease" -version 2>&1 | grep -Eq 'version 6(\.|$)'; then
        echo "ERROR: lrelease is not a Qt6 tool: ${qt6_bin_dir}/lrelease" >&2
        return 1
    fi

    export QT6_QMAKE_BIN="${qmake_bin}"
    export QT6_BIN_DIR="${qt6_bin_dir}"
    export QT6_CMAKE_DIR="${qt6_cmake_dir}"
    export PATH="${QT6_BIN_DIR}:${PATH}"

    echo "Qt6 version: ${qt_version}"
    echo "Qt6 tools: ${QT6_BIN_DIR}"
    echo "Qt6 CMake: ${QT6_CMAKE_DIR}"
}

qt6_prepare_build_dir() {
    local build_dir="$1"
    local cache_file="${build_dir}/CMakeCache.txt"
    if [ ! -f "${cache_file}" ]; then
        return 0
    fi

    local non_qt6_refs
    non_qt6_refs="$(grep -Eio 'qt[0-9]+' "${cache_file}" | sort -fu | grep -Eiv '^qt6$' || true)"
    if [ -n "${non_qt6_refs}" ]; then
        echo "Legacy Qt cache entries found in ${cache_file}: ${non_qt6_refs//$'\n'/, }"
        echo "Removing the generated build directory before the Qt6 configure."
        cmake -E remove_directory "${build_dir}"
    fi
}

qt6_verify_elf_dependencies() {
    local binary="$1"
    if [ ! -x "${binary}" ]; then
        echo "ERROR: Qt6 ELF validation target is missing or not executable: ${binary}" >&2
        return 1
    fi
    if ! command -v readelf >/dev/null 2>&1; then
        echo "ERROR: readelf is required for Qt6 ELF validation." >&2
        return 1
    fi

    local needed
    needed="$(readelf -d "${binary}" | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p')"
    local qt_needed
    qt_needed="$(printf '%s\n' "${needed}" | grep -Ei '^libqt[0-9]+' || true)"
    if [ -z "${qt_needed}" ]; then
        echo "ERROR: executable has no Qt6 runtime dependency: ${binary}" >&2
        return 1
    fi

    local non_qt6
    non_qt6="$(printf '%s\n' "${qt_needed}" | grep -Eiv '^libqt6' || true)"
    if [ -n "${non_qt6}" ]; then
        echo "ERROR: executable contains a non-Qt6 runtime dependency:" >&2
        printf '  %s\n' ${non_qt6} >&2
        return 1
    fi

    local module
    for module in Core Gui Widgets Network Svg; do
        if ! printf '%s\n' "${qt_needed}" | grep -Eqi "^libqt6${module}"; then
            echo "ERROR: executable is missing direct Qt6${module} linkage: ${binary}" >&2
            return 1
        fi
    done
}
