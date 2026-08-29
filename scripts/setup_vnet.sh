#!/usr/bin/env bash
# Create a veth pair (two real Ethernet interfaces) on one machine so ghostchat's
# real AF_PACKET backend can be exercised end-to-end without a second laptop or
# the mac80211_hwsim module (which may be absent from the kernel).
#
# Traffic crosses a genuine raw-socket path over real interfaces; the only
# difference from true off-grid WiFi is the link layer (Ethernet vs 802.11 IBSS,
# which the kernel encapsulates for us either way). The protocol/transport/radio
# code above AF_PACKET is identical.
#
# Usage:  sudo ./scripts/setup_vnet.sh [up|down]

set -euo pipefail

A="${VETH_A:-veth0}"
B="${VETH_B:-veth1}"

if [ "$(id -u)" -ne 0 ]; then
    echo "must run as root: sudo $0 ${1:-up}" >&2
    exit 1
fi

case "${1:-up}" in
    up)
        # Remove any stale pair first.
        ip link del "$A" 2>/dev/null || true
        ip link add "$A" type veth peer name "$B"
        ip link set "$A" up
        ip link set "$B" up
        sleep 1
        echo "Created veth pair $A <-> $B (both up)."
        echo
        echo "Terminal 1:  sudo ./build/ghostchat 0x01 $A"
        echo "Terminal 2:  sudo ./build/ghostchat 0x02 $B"
        echo
        echo "Then:  d            discover"
        echo "       m <peer> hi  send (auto-probes if peer unknown)"
        echo "       l            list peers"
        echo "       q            quit"
        ;;
    down)
        ip link del "$A" 2>/dev/null || true
        echo "Removed $A (and peer $B)."
        ;;
    *)
        echo "usage: $0 [up|down]" >&2
        exit 1
        ;;
esac
