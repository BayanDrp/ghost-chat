# Transport layer

The transport layer turns the best-effort radio into a reliable, peer-aware
channel. It is implemented in `transport::Transport` and backed by two helpers:
`ReliabilityTracker` (per-frame ACK + retransmit) and `Connection` (a held peer).

## Reliability

Each outgoing Message gets a `seq` number. `Transport::send` serializes the
frame and records it in the `ReliabilityTracker` with `max_tries` (default 5)
and a `timeout` (default 200 ms). Every `poll()` call:

1. resends any tracked frame whose timeout elapsed and still has tries left,
2. feeds received frames to `parse`, then
3. on an `Ack` frame, removes the matching `seq` from the tracker.

When tries are exhausted the frame is dropped silently. Note the radio itself
does the actual transmission; the tracker only decides *when to repeat*.

## Discovery

`discover()` broadcasts a Discovery frame. On receiving a Discovery (that is
not itself a `DiscoveryResponse`), a node:

- records the sender as a peer (`peers_` set, `on_peer` callback, fired once
  per new peer),
- replies with a Discovery carrying `DiscoveryResponse` so the original sender
  also learns the peer.

Unicast to an unknown peer is handled transparently: `AFPacketRadio::send`
broadcasts a Discovery probe and queues the message, flushing it once the peer
reseponds and the neighbor MAC is learned.

## Receive / decrypt

`poll()` decrypts any frame marked `Encrypted` using the key supplied via
`set_key`. A frame that claims to be encrypted but cannot be decrypted (wrong
key or tampered) is **dropped** — it never reaches the `on_message` callback.
Without a key, `Encrypted` frames are ignored and plaintext frames pass
through unchanged, so encryption is purely opt-in.

## API

```cpp
Transport t(radio);
t.set_key("mysecret");              // optional
t.on_message([](uint64_t from, auto& p){ /* ... */ });
t.on_ack([](uint32_t seq){ /* ... */ });
t.on_peer([](uint64_t p){ /* ... */ });
t.send(peer, payload);              // encrypts if a key is set
t.discover();
t.poll();                           // call regularly (the CLI polls on a timer)
auto ps = t.peers();                // known peer node ids
```
