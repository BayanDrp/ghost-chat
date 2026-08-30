# GhostChat Wireshark Dissector

Lua dissector for the GhostChat protocol.

## Installation

### Linux
```bash
cp ghostchat.lua ~/.local/lib/wireshark/plugins/
# or system-wide:
sudo cp ghostchat.lua /usr/lib/x86_64-linux-gnu/wireshark/plugins/
```

### Windows
Copy `ghostchat.lua` to:
```
%APPDATA%\Wireshark\plugins\
```

### Wireshark (any OS)
1. Open Wireshark
2. `Help` → `About Wireshark` → `Folders` → `Personal Lua Plugins` (or `Global Lua Plugins`)
2. Copy `ghostchat.lua` there
3. Restart Wireshark

## Usage

The dissector automatically activates for:
- **Ethernet type 0x6767** (GhostChat over raw Ethernet / AF_PACKET)
- **IEEE 802.15.4** frames (if DLT_IEEE802_15_4 is used)

### Features
- Full header decode (magic, version, type, TTL, flags, sender, receiver, seq, payload size)
- Flag bit decoding (ACK, ENCRYPTED, RELAYED, DISCOVERY_RESPONSE)
- Payload display (hex + ASCII if printable)
- FNV-1a checksum verification (shows ✓/✗)
- Info column summary: `Type sender→receiver seq=X flags len=Y`

### Frame Types
| Value | Name |
|-------|------|
| 0x01 | Discovery |
| 0x02 | Message |
| 0x03 | Ack |
| 0x04 | RouteRequest |
| 0x05 | RouteReply |

### Flags
| Bit | Name |
|-----|------|
| 0x01 | ACK_REQUESTED |
| 0x02 | ENCRYPTED |
| 0x04 | RELAYED |
| 0x08 | DISCOVERY_RESPONSE |

## Testing

Capture GhostChat traffic on a raw Ethernet interface:
```bash
# On the machine running ghostchat
sudo tcpdump -i wlan0 -w ghostchat.pcap ether proto 0x6767

# Open in Wireshark
wireshark ghostchat.pcap
```

Or capture IEEE 802.15.4:
```bash
sudo tcpdump -i wpan0 -w ghostchat_802154.pcap
```

## Development

The dissector is in `tools/wireshark/ghostchat.lua`. Edit and reload in Wireshark:
`Analyze` → `Reload Lua Plugins` (Ctrl+Shift+L)