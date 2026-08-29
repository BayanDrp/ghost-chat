# Architecture

GhostChat is layered so each piece has one job and can be tested (or swapped)
independently. Data flows down for send, up for receive.

```
   application / CLI  (src/main.cpp)
        |  send(payload) / poll()
        v
   transport::Transport        reliability, discovery, peers, (optional) crypto
        |  serialize(frame) / parse(frame)
        v
   protocol: Frame + codec      wire format, LE, FNV-1a checksum
        |  raw bytes
        v
   radio::Radio (AFPacketRadio / LoopbackRadio)   AF_PACKET, EtherType 0x6767
        |
   physical link: WiFi IBSS  |  Ethernet / veth  |  loopback (test)
```

## Boundaries

- **protocol ↔ radio**: the radio knows nothing about frame meaning; it just
  moves the serialized byte string. The protocol knows nothing about sockets.
- **transport ↔ protocol**: transport builds/parses frames and owns reliability,
  discovery, and the (optional) encryption. The `crypto` module is a leaf it
  calls — the algorithm is isolated and unit-tested on its own.
- **transport ↔ application**: the CLI calls `send`/`discover`/`poll` and
  registers `on_message` / `on_ack` / `on_peer` callbacks. The CLI owns the
  terminal and the live poll loop; transport owns the channel.

## Why this shape

- The radio backend is a single `Radio` interface, so the loopback double
  exercises the whole stack without hardware, and a future monitor-mode WiFi
  backend can be added without touching transport or protocol.
- Encryption is *inside* `send`/`poll` but *outside* as an algorithm, so it is
  mistake-proof (you cannot forget to encrypt when a key is set) yet the crypto
  code stays separate and testable.

## Test doubles

- `LoopbackRadio` (`make_loopback_pair`) — two cross-wired in-memory radios.
- `veth` pair (`scripts/setup_vnet.sh`) — two real interfaces on one machine,
  exercising the actual `AF_PACKET` code path.
- Real WiFi IBSS (`scripts/setup_wifi.sh`) — the intended off-grid deployment.
