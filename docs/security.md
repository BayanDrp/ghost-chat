# Security

GhostChat's goal is traffic that is unreadable to anyone merely listening on the
radio. Confidentiality and integrity come from a single, well-understood
primitive: **AES-256-GCM**, provided by OpenSSL and isolated in the `crypto`
module.

## What is encrypted

Only the **payload**. The frame header (`sender`, `receiver`, `seq`, flags) stays
in the clear because the radio and neighbor table need it for delivery and
routing. A passive observer therefore learns *that* two node ids are talking,
but not *what* they say. (Hiding even the node ids would require encrypting the
whole frame, which would break radio delivery — a later, harder step.)

## How it works

- A passphrase is turned into a 32-byte key with SHA-256 (`derive_key`). This is
  a simple KDF, not a slow/salted password hash — fine for a learning MVP.
- `encrypt` outputs `nonce(12) || ciphertext || GCM tag(16)`. The 12-byte nonce
  is random per call, so the same message encrypts to different bytes each time
  (no pattern for an eavesdropper to match).
- `decrypt` returns `nullopt` if the GCM tag check fails — i.e. wrong key *or*
  tampered data. `Transport::poll` drops such frames.

## Opt-in

Encryption is enabled only when `Transport::set_key` is called (CLI `-k` /
`GHOSTCHAT_KEY`). With no key, frames are plaintext and behave exactly as
before. A frame marked `Encrypted` that a receiver cannot decrypt is silently
discarded, so a node without the passphrase sees nothing.

## Known limitations / future work

- **Static PSK**: every participant shares one password. No forward secrecy and
  no per-pair keys.
- **Planned upgrade (not implemented)**: replace the PSK with an authenticated
  Diffie-Hellman / ECDH handshake (the "TLS math" approach) that *derives* a
  fresh session key automatically — no shared password, and compromise of one
  laptop does not expose past traffic. This drops into the same `key_` slot in
  `Transport`, so the frame format and the rest of the stack do not change.
- A replay window (tracking recent `(sender, nonce)`) is not yet enforced; GCM
  integrity is, but a captured frame could be replayed.
