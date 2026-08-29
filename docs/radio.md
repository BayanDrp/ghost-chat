# Radio layer

The radio moves raw bytes between nodes. It is hidden behind the abstract
`radio::Radio` interface so the rest of the stack is independent of the
physical link.

```cpp
class Radio {
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool send(const std::vector<uint8_t>& frame) = 0;
    virtual std::optional<std::vector<uint8_t>> receive() = 0;
    virtual uint64_t local_address() const = 0;
    virtual const std::string& interface_name() const = 0;
};
```

`frame` here is the fully serialized GhostChat frame (header + payload +
checksum) passed down from the transport layer.

## LoopbackRadio

`make_loopback_pair(a, b)` returns two cross-wired `LoopbackRadio`s used as a
dev/test double: what one sends, the other receives. No hardware required.

## AFPacketRadio

The real backend. Constructed with an interface name and a node id:

```cpp
radio::AFPacketRadio(iface, node_id);
```

- Opens an `AF_PACKET` / `SOCK_RAW` socket bound to the interface for the custom
  EtherType `0x6767`.
- Maintains a **neighbor table** mapping node id → MAC, learned from received
  frame source addresses.
- `send()`: looks up the destination node id in the table and transmits
  **unicast** to that MAC; broadcasts only for Discovery. Unknown destinations
  trigger a Discovery probe and queue the frame.
- `receive()`: reads frames, drops its own transmissions (source MAC == local),
  and auto-replies to Discovery so peers learn each other.
- Non-blocking (`O_NONBLOCK`); the transport drives it via `poll()`.

## Ieee802154Radio

The real RF backend. Same `Radio` interface and same neighbor-table /
discovery-probe behavior as `AFPacketRadio`, but speaking true IEEE 802.15.4
MAC frames instead of Ethernet:

```cpp
radio::Ieee802154Radio(iface, node_id);   // iface e.g. "wpan0"
```

- Opens an `AF_IEEE802154` / `SOCK_DGRAM` socket, bound to the interface with
  `SO_BINDTODEVICE` (there is no EtherType at this layer, so the device selects
  the radio).
- Addresses frames by the interface's **64-bit extended address**, which maps
  directly onto our 64-bit `node_id` field — no truncation. The neighbor table
  learns extended addresses from received frame source addresses.
- `send()`: unicast to the peer's extended address when known; broadcasts
  (`0xFF…FF`) for Discovery; probes + queues for unknown destinations.
- `receive()`: reads 802.15.4 frames, drops its own transmissions (source
  extended address == local), auto-replies to Discovery, and returns the
  payload (stray 802.15.4 traffic is rejected by the frame checksum/magic).
- Non-blocking (`O_NONBLOCK`); the transport drives it via `poll()`.

Select it on the CLI with `-r ieee802154`
(`scripts/setup_802154.sh` brings up a testable `fakelb` loopback).

## What link it runs on

- **Ethernet / `veth`**: works as-is; the kernel carries the custom EtherType.
- **WiFi IBSS (ad-hoc)**: the interface presents as Ethernet to `AF_PACKET`;
  the kernel wraps frames in 802.11. This is the intended off-grid mode
  (`scripts/setup_wifi.sh`).
- **`mac80211_hwsim`**: virtual WiFi radios for single-machine testing when the
  kernel module is available (`scripts/setup_hwsim.sh`).
- **IEEE 802.15.4 (`AF_IEEE802154`)**: real low-power RF. Use real 802.15.4
  hardware, or the kernel `mac802154` + `fakelb` loopback for single-machine
  testing (`scripts/setup_802154.sh`, gives `wpan0`/`wpan1`). Select with
  `-r ieee802154`.
