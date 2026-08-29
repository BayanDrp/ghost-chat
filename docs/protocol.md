# Protocol

GhostChat speaks a single custom frame type carried over any link that moves
Ethernet/802.11 frames (WiFi IBSS, Ethernet, a `veth` pair, or the loopback
test double). There is no IP layer: frames are addressed by **node id**
(derived from the on-wire `sender`/`receiver` fields), not by MAC.

## Frame types

| value | name | purpose |
|-------|------|---------|
| `0x01` | Discovery  | announce presence / bootstrap the neighbor table |
| `0x02` | Message    | application payload (optionally encrypted) |
| `0x03` | Ack        | reliability acknowledgement for a Message |

## Flags

| bit | name | meaning |
|-----|------|---------|
| `0x01` | AckRequested        | receiver should send an Ack |
| `0x02` | Encrypted          | payload is locked (AES-256-GCM) |
| `0x04` | Relayed            | (reserved) frame was forwarded by a mesh node |
| `0x08` | DiscoveryResponse  | Discovery sent in reply to a Discovery |

## Addressing

`sender` and `receiver` are 64-bit fields; the low 48 bits hold the WiFi MAC.
`0xFFFFFFFFFFFFFFFF` is the broadcast address and doubles as the broadcast MAC.
The radio uses the `receiver` field to pick a neighbor MAC from its table and
transmit unicast; unknown peers trigger a Discovery probe first.

## Wire format

See `docs/frame-format.md` for the exact byte layout and `docs/security.md` for
how encryption wraps the payload.
