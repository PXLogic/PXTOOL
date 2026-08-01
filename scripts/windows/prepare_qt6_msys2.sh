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

mapfile -t legacy_qt_packages < <(
    pacman -Qq | grep -E '^mingw-w64-x86_64-qt[0-9]+-' \
        | grep -v '^mingw-w64-x86_64-qt6-' || true
)

if [ "${#legacy_qt_packages[@]}" -eq 0 ]; then
    echo "No legacy MSYS2 Qt packages are installed."
    exit 0
fi

printf 'Installed legacy MSYS2 Qt packages:\n'
printf '  %s\n' "${legacy_qt_packages[@]}"

if [ "$PURGE_LEGACY_QT" -eq 0 ]; then
    echo "Run '$0 --purge-legacy-qt' to remove these packages."
    exit 0
fi

read -r -p "Remove the listed legacy Qt packages and unused dependencies? [y/N] " response
if [ "$response" != "y" ] && [ "$response" != "Y" ]; then
    echo "Legacy Qt packages were not removed."
    exit 1
fi

pacman -Rns -- "${legacy_qt_packages[@]}"

if pacman -Qq | grep -E '^mingw-w64-x86_64-qt[0-9]+-' \
    | grep -v '^mingw-w64-x86_64-qt6-' >/dev/null; then
    echo "ERROR: Legacy Qt packages remain installed."
    exit 1
fi

echo "Legacy MSYS2 Qt packages removed; Windows builds now use Qt6 only."
