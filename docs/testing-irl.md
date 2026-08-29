# Real-world testing (IRL)

The loopback and `veth`/`fakelb` tests prove the stack on one machine. To know
it actually works over the air, run two nodes on **real radios**. Two options:

- **A — WiFi IBSS** (no extra hardware: two machines/laptops with WiFi).
- **B — IEEE 802.15.4 RF** (real low-power transceivers, or the same `fakelb`
  trick on one box if you only have one machine).

Both use the same `ghostchat` binary; only the interface and `-r` flag differ.

---

## Option A — WiFi IBSS (ad-hoc), two machines

No special hardware: any two computers with WiFi can talk directly, no AP, no
internet. Uses the default `afpacket` backend.

On **each** machine:

```sh
sudo ./scripts/setup_wifi.sh wlan0 ghostchat 2412
```

This puts `wlan0` into IBSS mode on channel 1 (2412 MHz) with SSID `ghostchat`,
releases NetworkManager's grip on it, and disables power-save (power-save makes
the radio sleep and silently drop frames).

Then run a node on each machine:

```sh
# machine 1
sudo ./build/ghostchat 0x01 wlan0

# machine 2
sudo ./build/ghostchat 0x02 wlan0
```

Inside node 1: type `d` to discover, then `m 0x02 hello`. You should see
`[discovered peer 2]`, `[2] hello` on the other side, and `[ack #n]` on both.
`l` lists known peers.

Add `-k mysecret` on **both** sides to encrypt (PSK, AES-256-GCM); a third party
listening on the same channel hears only undecryptable noise.

### Gotchas
- Both nodes must use the **same SSID and frequency** or they won't associate.
- Keep messages **under ~2000 bytes**: WiFi's max data-unit (2304) minus frame
  header/checksum; larger messages get dropped by the link before they reach us.
- The binary needs `CAP_NET_RAW` (run with `sudo`, or
  `sudo setcap cap_net_raw,cap_net_admin=ep ./build/ghostchat` once).
- Some WiFi drivers/regulatory settings block ad-hoc; if `ibss join` fails, try
  a different channel or check `iw reg get`.

---

## Option B — IEEE 802.15.4 RF

Real low-power radio (the MAC behind Zigbee/Thread). Uses `-r ieee802154`.

### One machine (kernel loopback)
If you only have one box, the kernel `fakelb` gives two virtual 802.15.4 radios
that talk to each other:

```sh
sudo ./scripts/setup_802154.sh          # loads mac802154+fakelb, brings up wpan0/1
sudo ./build/ghostchat 0x01 wpan0 -r ieee802154
sudo ./build/ghostchat 0x02 wpan1 -r ieee802154
```

This is what the CI/example above uses; it proves the 802.15.4 path end-to-end
without hardware.

### Two machines (real transceivers)
You need Linux-supported 802.15.4 hardware, e.g. a USB 802.15.4 dongle
(TI CC2531/CC2650-class) or a board with an `at86rf233`/similar transceiver,
where the kernel `mac802154` stack drives the radio.

On each machine:

```sh
sudo modprobe mac802154
sudo modprobe <your-driver>        # e.g. the module for your dongle/SoC
sudo ip link add dev wpan0 type ieee802154
sudo ip link set wpan0 up
```

Then run a node on each:

```sh
sudo ./build/ghostchat 0x01 wpan0 -r ieee802154
sudo ./build/ghostchat 0x02 wpan0 -r ieee802154
```

(The 802.15.4 backend addresses frames by the interface's 64-bit extended
address, which maps directly onto the `node_id`, so no extra config is needed.)

---

## How to know it worked
On either backend, a real test shows:

- `d` → `[discovered peer N]` on both sides.
- `m <peer> <msg>` → the message printed on the peer, with `[ack #n]` on both.
- `l` → lists the peer id(s).

If discovery finds nothing, the two radios aren't actually associated on the
same channel/network — that's a link-layer issue, not a GhostChat bug.
