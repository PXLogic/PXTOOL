#!/bin/bash
set -euo pipefail

usage() {
    echo "Usage: $0 [--purge-legacy-qt]"
}

case "${1:-}" in
    "")
        PURGE_LEGACY_QT=0
        ;;
    --purge-legacy-qt)
        PURGE_LEGACY_QT=1
        ;;
    *)
        usage
        exit 2
        ;;
esac

pacman -Syu --needed \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-svg \
    mingw-w64-x86_64-qt6-tools

query_legacy_qt_packages() {
    local installed_packages pacman_status legacy_qt_candidates filter_status

    if installed_packages="$(pacman -Qq)"; then
        pacman_status=0
    else
        pacman_status=$?
    fi
    if [ "$pacman_status" -ne 0 ]; then
        echo "ERROR: pacman -Qq failed; cannot verify the MSYS2 Qt environment." >&2
        return "$pacman_status"
    fi

    if legacy_qt_candidates="$(grep -E '^mingw-w64-x86_64-qt[0-9]+-' <<< "$installed_packages")"; then
        :
    else
        filter_status=$?
        if [ "$filter_status" -gt 1 ]; then
            echo "ERROR: failed to filter MSYS2 packages for Qt packages." >&2
            return "$filter_status"
        fi
        return 0
    fi

    if grep -v '^mingw-w64-x86_64-qt6-' <<< "$legacy_qt_candidates"; then
        :
    else
        filter_status=$?
        if [ "$filter_status" -gt 1 ]; then
            echo "ERROR: failed to filter legacy Qt packages." >&2
            return "$filter_status"
        fi
    fi
    return 0
}

legacy_qt_output=""
if ! legacy_qt_output="$(query_legacy_qt_packages)"; then
    echo "ERROR: Unable to verify installed legacy MSYS2 Qt packages." >&2
    exit 1
fi
legacy_qt_packages=()
if [ -n "$legacy_qt_output" ]; then
    mapfile -t legacy_qt_packages <<< "$legacy_qt_output"
fi

if [ "${#legacy_qt_packages[@]}" -eq 0 ]; then
    echo "No legacy MSYS2 Qt packages are installed."
    exit 0
fi

printf 'Installed legacy MSYS2 Qt packages:\n'
printf '  %s\n' "${legacy_qt_packages[@]}"

if [ "$PURGE_LEGACY_QT" -eq 0 ]; then
    echo "ERROR: Legacy Qt packages must be removed before using the Qt6-only build environment."
    echo "Run '$0 --purge-legacy-qt' to remove these packages."
    exit 1
fi

read -r -p "Remove the listed legacy Qt packages and unused dependencies? [y/N] " response
if [ "$response" != "y" ] && [ "$response" != "Y" ]; then
    echo "Legacy Qt packages were not removed."
    exit 1
fi

pacman -Rns -- "${legacy_qt_packages[@]}"

if ! remaining_legacy_qt="$(query_legacy_qt_packages)"; then
    echo "ERROR: Unable to verify the post-purge MSYS2 Qt package state." >&2
    exit 1
fi
if [ -n "$remaining_legacy_qt" ]; then
    echo "ERROR: Legacy Qt packages remain installed."
    printf '  %s\n' "$remaining_legacy_qt"
    exit 1
fi

echo "Legacy MSYS2 Qt packages removed; Windows builds now use Qt6 only."
