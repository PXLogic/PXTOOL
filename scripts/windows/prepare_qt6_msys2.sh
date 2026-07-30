#!/bin/bash
set -euo pipefail

usage() {
    echo "Usage: $0 [--purge-qt5]"
}

case "${1:-}" in
    "")
        PURGE_QT5=0
        ;;
    --purge-qt5)
        PURGE_QT5=1
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

mapfile -t qt5_packages < <(pacman -Qq | grep '^mingw-w64-x86_64-qt5-' || true)

if [ "${#qt5_packages[@]}" -eq 0 ]; then
    echo "No MSYS2 Qt5 packages are installed."
    exit 0
fi

printf 'Installed Qt5 packages:\n'
printf '  %s\n' "${qt5_packages[@]}"

if [ "$PURGE_QT5" -eq 0 ]; then
    echo "Run '$0 --purge-qt5' to remove these packages."
    exit 0
fi

read -r -p "Remove the listed Qt5 packages and unused dependencies? [y/N] " response
if [ "$response" != "y" ] && [ "$response" != "Y" ]; then
    echo "Qt5 packages were not removed."
    exit 1
fi

pacman -Rns -- "${qt5_packages[@]}"

if pacman -Qq | grep -q '^mingw-w64-x86_64-qt5-'; then
    echo "ERROR: Qt5 packages remain installed."
    exit 1
fi

echo "MSYS2 Qt5 packages removed; Windows builds now use Qt6 only."
