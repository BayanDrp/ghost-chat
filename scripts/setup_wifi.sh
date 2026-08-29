#!/usr/bin/env bash
# Bring a WiFi interface into Ad-hoc (IBSS) mode on a fixed channel so two
# GhostChat nodes can talk directly without an access point.
# Usage: sudo ./scripts/setup_wifi.sh <interface> [ssid] [frequency_mhz]
set -euo pipefail

IFACE="${1:-wlan0}"
SSID="${2:-ghostchat}"
FREQ="${3:-2412}"   # channel 1

if [ "$(id -u)" -ne 0 ]; then
    echo "must run as root (sudo)" >&2
    exit 1
fi

echo "[*] configuring $IFACE as ad-hoc ($SSID @ ${FREQ}MHz)"
# NetworkManager will fight us for the interface; release it if present.
command -v nmcli >/dev/null 2>&1 && nmcli dev set "$IFACE" managed no 2>/dev/null || true
ip link set "$IFACE" down
iw dev "$IFACE" set type ibss
ip link set "$IFACE" up
iw dev "$IFACE" ibss join "$SSID" "$FREQ"
# Power-save makes the radio sleep and drop our frames; turn it off.
iw dev "$IFACE" set power_save off 2>/dev/null || true
sleep 1
ip addr show "$IFACE" | grep -q "$IFACE" && echo "[+] $IFACE is up in IBSS mode"
echo "[+] both nodes must join the same SSID + frequency to see each other"
