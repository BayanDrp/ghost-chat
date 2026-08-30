--[[
GhostChat Wireshark Lua Dissector
==================================
Load in Wireshark:  File → Export Packet Dissections → Lua → Load this file
Or copy to ~/.local/lib/wireshark/plugins/ (Linux) or %APPDATA%\Wireshark\plugins\ (Windows)

Protocol: GhostChat decentralized off-grid chat
Wire format (little-endian):
  magic(2) version(1) type(1) ttl(1) flags(1) sender(8) receiver(8) seq(4) payload_size(2) payload payload_size checksum(4)
]]

local ghostchat = Proto("ghostchat", "GhostChat Protocol")

-- Frame types
local frame_types = {
    [0x01] = "Discovery",
    [0x02] = "Message",
    [0x03] = "Ack",
    [0x04] = "RouteRequest",
    [0x05] = "RouteReply",
}

-- Flag bits
local flag_names = {
    [0x01] = "ACK_REQUESTED",
    [0x02] = "ENCRYPTED",
    [0x04] = "RELAYED",
    [0x08] = "DISCOVERY_RESPONSE",
}

-- Fields
local f_magic      = ProtoField.uint16("ghostchat.magic", "Magic", base.HEX)
local f_version    = ProtoField.uint8("ghostchat.version", "Version", base.DEC)
local f_type       = ProtoField.uint8("ghostchat.type", "Frame Type", base.HEX, frame_types)
local f_ttl        = ProtoField.uint8("ghostchat.ttl", "TTL", base.DEC)
local f_flags      = ProtoField.uint8("ghostchat.flags", "Flags", base.HEX, nil, 0xFF)
local f_sender     = ProtoField.uint64("ghostchat.sender", "Sender", base.HEX)
local f_receiver   = ProtoField.uint64("ghostchat.receiver", "Receiver", base.HEX)
local f_seq        = ProtoField.uint32("ghostchat.sequence", "Sequence", base.DEC)
local f_payload_sz = ProtoField.uint16("ghostchat.payload_size", "Payload Size", base.DEC)
local f_payload    = ProtoField.bytes("ghostchat.payload", "Payload")
local f_checksum   = ProtoField.uint32("ghostchat.checksum", "Checksum (FNV-1a)", base.HEX)
local f_flag_ack   = ProtoField.bool("ghostchat.flag.ack", "ACK Requested", 8, nil, 0x01)
local f_flag_enc   = ProtoField.bool("ghostchat.flag.enc", "Encrypted", 8, nil, 0x02)
local f_flag_relay = ProtoField.bool("ghostchat.flag.relay", "Relayed", 8, nil, 0x04)
local f_flag_disc  = ProtoField.bool("ghostchat.flag.disc_resp", "Discovery Response", 8, nil, 0x08)

ghostchat.fields = {
    f_magic, f_version, f_type, f_ttl, f_flags,
    f_flag_ack, f_flag_enc, f_flag_relay, f_flag_disc,
    f_sender, f_receiver, f_seq, f_payload_sz, f_payload, f_checksum
}

-- FNV-1a 32-bit
local function fnv1a(data)
    local hash = 0x811c9dc5
    for i = 1, #data do
        hash = bit.bxor(hash, data:byte(i))
        hash = (hash * 0x01000193) % 0x100000000
    end
    return hash
end

-- Dissector
function ghostchat.dissector(tvb, pinfo, tree)
    local len = tvb:len()
    if len < 28 then return end  -- minimum header

    pinfo.cols.protocol = "GhostChat"

    local subtree = tree:add(ghostchat, tvb(), "GhostChat Protocol")

    -- Magic
    local magic = tvb(0, 2):le_uint()
    subtree:add(f_magic, tvb(0, 2))
    if magic ~= 0x6767 then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR, "Invalid magic: " .. string.format("0x%04x", magic))
    end

    -- Version
    local version = tvb(2, 1):uint()
    subtree:add(f_version, tvb(2, 1))

    -- Frame type
    local ftype = tvb(3, 1):uint()
    subtree:add(f_type, tvb(3, 1))

    -- TTL
    local ttl = tvb(4, 1):uint()
    subtree:add(f_ttl, tvb(4, 1))

    -- Flags
    local flags = tvb(5, 1):uint()
    local flags_tree = subtree:add(f_flags, tvb(5, 1))
    flags_tree:add(f_flag_ack, tvb(5, 1))
    flags_tree:add(f_flag_enc, tvb(5, 1))
    flags_tree:add(f_flag_relay, tvb(5, 1))
    flags_tree:add(f_flag_disc, tvb(5, 1))

    -- Sender / Receiver / Sequence
    local sender = tvb(6, 8):le_uint64()
    local receiver = tvb(14, 8):le_uint64()
    local seq = tvb(22, 4):le_uint()
    subtree:add(f_sender, tvb(6, 8))
    subtree:add(f_receiver, tvb(14, 8))
    subtree:add(f_seq, tvb(22, 4))

    -- Payload size
    local payload_size = tvb(26, 2):le_uint()
    subtree:add(f_payload_sz, tvb(26, 2))

    -- Payload
    local payload_offset = 28
    if payload_size > 0 then
        if len >= payload_offset + payload_size then
            local payload_tvb = tvb(payload_offset, payload_size)
            subtree:add(f_payload, payload_tvb)
            -- Try to decode as UTF-8 if printable
            local payload_str = payload_tvb:string()
            if payload_str and payload_str:match("^[%g%s]*$") then
                subtree:add("Payload (ASCII)", payload_str)
            end
        end
    end

    -- Checksum
    local checksum_offset = payload_offset + payload_size
    if len >= checksum_offset + 4 then
        local checksum = tvb(checksum_offset, 4):le_uint()
        subtree:add(f_checksum, tvb(checksum_offset, 4))

        -- Verify checksum
        local calc_data = tvb(0, checksum_offset):bytes()
        local calc = fnv1a(calc_data)
        if calc ~= checksum then
            subtree:add_expert_info(PI_CHECKSUM, PI_WARN,
                string.format("Checksum mismatch: got 0x%08x, expected 0x%08x", checksum, calc))
        else
            subtree:add("Checksum OK", string.format("0x%08x", calc))
        end
    end

    -- Info column
    local type_str = frame_types[ftype] or string.format("Unknown(0x%02x)", ftype)
    local flags_str = ""
    if bit.band(flags, 0x01) ~= 0 then flags_str = flags_str .. " ACK" end
    if bit.band(flags, 0x02) ~= 0 then flags_str = flags_str .. " ENC" end
    if bit.band(flags, 0x04) ~= 0 then flags_str = flags_str .. " RLY" end
    if bit.band(flags, 0x08) ~= 0 then flags_str = flags_str .. " DISC_RSP" end

    pinfo.cols.info = string.format("%s %s→%s seq=%d%s len=%d",
        type_str, to_hex(sender), to_hex(receiver), seq, flags_str, payload_size)
end

-- Register for Ethernet type 0x6767 (GhostChat EtherType)
local ethertype_table = DissectorTable.get("ethertype")
ethertype_table:add(0x6767, ghostchat)

-- Also register for raw UDP port if used (optional)
-- local udp_table = DissectorTable.get("udp.port")
-- udp_table:add(6767, ghostchat)

-- Helper
function to_hex(n)
    if n == 0 then return "0" end
    local hex = ""
    while n > 0 do
        local d = n % 16
        hex = string.format("%x", d) .. hex
        n = math.floor(n / 16)
    end
    return "0x" .. hex
end

-- Register
register_postdissector(ghostchat)

print("GhostChat dissector loaded")