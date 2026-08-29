#!/usr/bin/env bash
# Bring up IEEE 802.15.4 radios for real-RF testing on one machine.
# Uses the kernel 'mac802154' + 'fakelb' loopback (two virtual 802.15.4 radios
# that talk to each other), or real 802.15.4 hardware if the drivers find it.
#
# Usage: sudo ./scripts/setup_802154.sh [iface0] [iface1]
#   iface0/iface1  names for the two wpan interfaces (default wpan0, wpan1)
#
# Then run in two terminals:
#   sudo ./build/ghostchat 0x01 wpan0 -r ieee802154
#   sudo ./build/ghostchat 0x02 wpan1 -r ieee802154

set -euo pipefail
A="${1:-wpan0}"
B="${2:-wpan1}"

if [ "$(id -u)" -ne 0 ]; then
    echo "must run as root: sudo $0" >&2
    exit 1
fi

modprobe mac802154 2>/dev/null || true
modprobe fakelb    2>/dev/null || true

# Create the wpan interfaces if they don't exist yet.
ip link show "$A" >/dev/null 2>&1 || ip link add dev "$A" type ieee802154
ip link show "$B" >/dev/null 2>&1 || ip link add dev "$B" type ieee802154
ip link set "$A" up
ip link set "$B" up

sleep 1
echo "IEEE 802.15.4 interfaces ready: $A, $B"
echo "Terminal 1:  sudo ./build/ghostchat 0x01 $A -r ieee802154"
echo "Terminal 2:  sudo ./build/ghostchat 0x02 $B -r ieee802154"
