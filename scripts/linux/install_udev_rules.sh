#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RULE_SRC="${ROOT_DIR}/DSView/DreamSourceLab.rules"
RULE_DST="/etc/udev/rules.d/60-dreamsourcelab.rules"

if [ ! -f "${RULE_SRC}" ]; then
    echo "ERROR: udev rules source not found: ${RULE_SRC}"
    exit 1
fi

echo "Installing udev rules:"
echo "  ${RULE_DST}"
sudo install -m 0644 "${RULE_SRC}" "${RULE_DST}"
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb || true

echo "udev rules installed. Replug the hardware device if it is already connected."
