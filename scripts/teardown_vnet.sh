#!/usr/bin/env bash
# Remove the veth pair created by setup_vnet.sh (the "undo" step).
#
# Usage: sudo ./scripts/teardown_vnet.sh
set -euo pipefail

A="${VETH_A:-veth0}"

if [ "$(id -u)" -ne 0 ]; then
    echo "must run as root: sudo $0" >&2
    exit 1
fi

ip link del "$A" 2>/dev/null || true
echo "Removed $A (and its peer)."
