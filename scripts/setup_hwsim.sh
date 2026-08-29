#!/usr/bin/env bash
# Bring up two (or more) virtual WiFi radios via mac80211_hwsim and join a
# shared IBSS cell so ghostchat can exchange real 802.11 frames between them
# on a single machine (no second laptop needed).
#
# Usage: sudo ./scripts/setup_hwsim.sh [radios] [channel_mhz] [ssid]
#   radios       number of virtual radios (default 2)
#   channel_mhz  center frequency in MHz, e.g. 2412 = ch 1 (default 2412)
#   ssid         IBSS network name (default "ghostchat")
#
# After this prints the interface names, run in two terminals:
#   sudo ./build/ghostchat 0x01 <iface0>
#   sudo ./build/ghostchat 0x02 <iface1>
# Then:  d            (discover)
#        m <peer> hi  (send; probes automatically if peer unknown)

set -euo pipefail

RADIOS="${1:-2}"
CHANNEL="${2:-2412}"
SSID="${3:-ghostchat}"

if [ "$(id -u)" -ne 0 ]; then
    echo "must run as root: sudo $0 $*" >&2
    exit 1
fi

# Load the simulator (ignore if already loaded).
modprobe mac80211_hwsim radios="$RADIOS" 2>/dev/null || \
    echo "note: mac80211_hwsim already loaded or unavailable"

# Give udev a moment to name the interfaces.
sleep 1

mapfile -t IFACES < <(iw dev | awk '$1=="Interface"{print $2}')
if [ "${#IFACES[@]}" -lt 2 ]; then
    echo "expected at least 2 wifi interfaces, found: ${IFACES[*]:-none}" >&2
    echo "is mac80211_hwsim available?  modinfo mac80211_hwsim" >&2
    exit 1
fi

echo "Virtual radios: ${IFACES[*]}"

for if in "${IFACES[@]}"; do
    ip link set "$if" down
    iw dev "$if" set type ibss 2>/dev/null || true
    ip link set "$if" up
    iw dev "$if" ibss leave 2>/dev/null || true
    iw dev "$if" ibss join "$SSID" "$CHANNEL"
done

# Let the two IBSS stations find each other's beacons.
sleep 2
echo "Joined IBSS '$SSID' on ${CHANNEL} MHz."
echo
echo "Terminal 1:  sudo ./build/ghostchat 0x01 ${IFACES[0]}"
echo "Terminal 2:  sudo ./build/ghostchat 0x02 ${IFACES[1]}"
echo
echo "Then:  d            discover"
echo "       m <peer> hi  send (auto-probes if peer unknown)"
echo "       l            list peers"
echo "       q            quit"
