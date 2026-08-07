#!/bin/bash
set -euo pipefail

export PATH="/usr/bin:/bin:$PATH"

if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
    echo "ERROR: cleanup requires an existing build directory." >&2
    exit 1
fi

BUILD_DIR="$1"

find "$BUILD_DIR" \
    -mindepth 1 \
    -maxdepth 1 \
    -type d \
    -name 'install-webui-check-*' \
    -exec rm -rf -- {} +
