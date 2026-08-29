# Frame format

All multi-byte integers are **little-endian**. The whole frame (header +
payload) is covered by a 32-bit FNV-1a checksum appended as a trailer; a frame
whose checksum does not verify is dropped during `parse`.

## Header — 27 bytes, fixed

| offset | size | field | notes |
|--------|------|-------|-------|
| 0  | 2 | `magic`     | `0x6767` |
| 2  | 1 | `version`   | `1` |
| 3  | 1 | `type`      | Discovery `0x01` / Message `0x02` / Ack `0x03` |
| 4  | 1 | `flags`     | see protocol.md |
| 5  | 8 | `sender`    | node id; low 48 bits = WiFi MAC |
| 13 | 8 | `receiver`  | node id; `0xFFFFFFFFFFFFFFFF` = broadcast |
| 21 | 4 | `seq`       | per-sender sequence number (for ACK/retransmit) |
| 25 | 2 | `payloadSize` | length of the payload in bytes |

## Payload

`payloadSize` bytes immediately follow the header (max `2304`). For a Message
or Discovery with the `Encrypted` flag set, the payload is not plaintext but:

```
nonce (12 bytes) || ciphertext (payloadSize - 28) || GCM tag (16 bytes)
```

The receiver splits the nonce off the front and verifies the tag before
handing the decrypted bytes to the application.

## Checksum

A 32-bit FNV-1a hash of the header + payload (everything before the trailer) is
appended as the final 4 bytes. `serialize` writes it; `parse` recomputes and
compares, returning `nullopt` on mismatch.

## Sizes

- Minimum frame (empty payload, no encryption): 27 + 0 + 4 (checksum) = 31 bytes.
- Maximum frame: 27 + 2304 + 4 = 2335 bytes (fits an 802.11 data frame / Ethernet
  MTU comfortably).
