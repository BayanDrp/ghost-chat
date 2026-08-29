# GhostChat

A decentralized, server-less, off-grid chat protocol that sends frames directly
over the radio (WiFi/802.11 in IBSS mode, or Ethernet) using Linux `AF_PACKET`
raw sockets — no IP, no TCP, no access point, no internet, no server.

Messages are exchanged as custom frames with a unique EtherType. An optional
pre-shared-key layer encrypts message payloads with AES-256-GCM so bystanders
on the radio cannot read them.

## Status

Functional MVP / proof-of-concept. The full stack (protocol → radio →
transport → CLI) works end-to-end and is tested on loopback and on real raw
sockets (a `veth` pair). Real off-grid WiFi (IBSS) has not been field-validated
on this machine (no second laptop, `mac80211_hwsim` absent), and encryption is
currently PSK-only (no DH key exchange yet).

## Build

```sh
make            # configure + build into build/
make test       # run the ctest suite
```

Requires a C++20 compiler and OpenSSL (`libcrypto`) for the encryption module.

## Run

Interactive CLI:

```sh
./build/ghostchat <node_id_hex> <interface> [-k <passphrase>]
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

(Or grant the binary `CAP_NET_RAW` once with
`sudo setcap cap_net_raw,cap_net_admin=ep ./build/ghostchat` and drop the
`sudo`.) Add `-k mysecret` (or `GHOSTCHAT_KEY=mysecret`) on both sides to
encrypt; a node without the key receives only undecryptable noise.

For real off-grid WiFi, see `scripts/setup_wifi.sh` (puts an interface into
IBSS/ad-hoc mode).

## Layout

| path | what |
|------|------|
| `include/ghostchat/protocol` `src/protocol` | wire `Frame`, LE `serialize`/`parse`, FNV-1a checksum |
| `include/ghostchat/radio` `src/radio` | `Radio` interface, loopback double, `AFPacketRadio` (`AF_PACKET`) |
| `include/ghostchat/transport` `src/transport` | reliability (seq/ACK/retransmit), `Connection`, `Transport` |
| `include/ghostchat/crypto` `src/crypto` | AES-256-GCM lock (OpenSSL) |
| `src/main.cpp` | interactive CLI |
| `scripts/` | `setup_vnet.sh`, `setup_wifi.sh`, `setup_hwsim.sh`, `teardown_vnet.sh` |
| `docs/` | protocol / frame / transport / radio / security / architecture notes |
| `tools/`, `wireshark/` | (reserved) packet dump, fuzzer, dissector |

See `docs/architecture.md` for how the layers fit together.
