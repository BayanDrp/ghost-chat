# GhostChat

A decentralized, server-less, off-grid chat protocol that sends frames directly
over the radio. It supports two raw-link backends:

- **WiFi / Ethernet** (`AF_PACKET`, EtherType `0x6767`) — 802.11 in IBSS
  (ad-hoc) mode, or plain Ethernet / `veth`.
- **IEEE 802.15.4** (`AF_IEEE802154`) — real low-power RF (the MAC layer behind
  Zigbee/Thread), over real 802.15.4 hardware or the kernel `fakelb` loopback.

No IP, no TCP, no access point, no internet, no server.

Messages are exchanged as custom frames with a unique EtherType. An optional
pre-shared-key layer encrypts message payloads with AES-256-GCM so bystanders
on the radio cannot read them.

## Status

Functional MVP / proof-of-concept. The full stack (protocol → radio →
transport → CLI) works end-to-end and is tested on loopback, on real raw
sockets (`veth`), and on real **IEEE 802.15.4 RF** frames (the kernel
`mac802154` + `fakelb` loopback). Real off-grid WiFi (IBSS) has not been
field-validated on this machine (no second laptop, `mac80211_hwsim` absent),
and encryption is currently PSK-only (no DH key exchange yet).

## Build

```sh
make            # configure + build into build/
make test       # run the ctest suite
```

Requires a C++20 compiler and OpenSSL (`libcrypto`) for the encryption module.

## Run

Interactive CLI:

```sh
./build/ghostchat <node_id_hex> <interface> [-r afpacket|ieee802154] [-k <passphrase>]
```

Commands inside the prompt:

| key | action |
|-----|--------|
| `d` | discover peers (broadcasts Discovery; peers auto-reply) |
| `m <peer> <msg>` | send a message (auto-probes if the peer is unknown) |
| `l` | list known peers |
| `q` | quit |

Example — two nodes over a `veth` pair on one machine:

```sh
sudo ./scripts/setup_vnet.sh                 # creates veth0 <-> veth1
sudo ./build/ghostchat 0x01 veth0
sudo ./build/ghostchat 0x02 veth1
```

Example — two nodes over real **IEEE 802.15.4 RF** on one machine (kernel
`fakelb` loopback gives two virtual 802.15.4 radios, `wpan0`/`wpan1`):

```sh
sudo ./scripts/setup_802154.sh               # loads mac802154+fakelb, up wpan0/1
sudo ./build/ghostchat 0x01 wpan0 -r ieee802154
sudo ./build/ghostchat 0x02 wpan1 -r ieee802154
```

(Or grant the binary `CAP_NET_RAW` once with
`sudo setcap cap_net_raw,cap_net_admin=ep ./build/ghostchat` and drop the
`sudo`.) Add `-k mysecret` (or `GHOSTCHAT_KEY=mysecret`) on both sides to
encrypt; a node without the key receives only undecryptable noise.

For real off-grid WiFi, see `scripts/setup_wifi.sh` (puts an interface into
IBSS/ad-hoc mode). `-r` selects the link backend (`afpacket`, the default; or
`ieee802154` for RF).

## Layout

| path | what |
|------|------|
| `include/ghostchat/protocol` `src/protocol` | wire `Frame`, LE `serialize`/`parse`, FNV-1a checksum |
| `include/ghostchat/radio` `src/radio` | `Radio` interface, loopback double, `AFPacketRadio` (`AF_PACKET`), `Ieee802154Radio` (`AF_IEEE802154`) |
| `include/ghostchat/transport` `src/transport` | reliability (seq/ACK/retransmit), `Connection`, `Transport` |
| `include/ghostchat/crypto` `src/crypto` | AES-256-GCM lock (OpenSSL) |
| `src/main.cpp` | interactive CLI |
| `scripts/` | `setup_vnet.sh`, `setup_wifi.sh`, `setup_hwsim.sh`, `setup_802154.sh`, `teardown_vnet.sh` |
| `docs/` | protocol / frame / transport / radio / security / architecture notes |
| `tools/`, `wireshark/` | (reserved) packet dump, fuzzer, dissector |

See `docs/architecture.md` for how the layers fit together.
