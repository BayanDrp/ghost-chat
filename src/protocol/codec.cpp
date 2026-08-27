#include "ghostchat/protocol/codec.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ghostchat::protocol {

namespace {

void put_u16(std::vector<std::uint8_t> &out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put_u32(std::vector<std::uint8_t> &out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

void put_u64(std::vector<std::uint8_t> &out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

std::uint16_t get_u16(const std::vector<std::uint8_t> &d, std::size_t off) {
    return static_cast<std::uint16_t>(d[off] | (std::uint16_t(d[off + 1]) << 8));
}

std::uint32_t get_u32(const std::vector<std::uint8_t> &d, std::size_t off) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= (std::uint32_t(d[off + i]) << (8 * i));
    return v;
}

std::uint64_t get_u64(const std::vector<std::uint8_t> &d, std::size_t off) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (std::uint64_t(d[off + i]) << (8 * i));
    return v;
}

std::vector<std::uint8_t> serialize_body(const Frame &frame) {
    std::vector<std::uint8_t> out;
    out.reserve(kWireHeaderSize + frame.payload.size());

    put_u16(out, frame.header.magic);
    out.push_back(frame.header.version);
    out.push_back(static_cast<std::uint8_t>(frame.header.type));
    out.push_back(frame.header.flags);
    put_u64(out, frame.header.sender);
    put_u64(out, frame.header.receiver);
    put_u32(out, frame.header.sequence);
    put_u16(out, frame.header.payloadSize);

    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return out;
}

} // namespace

std::uint32_t compute_checksum(const std::vector<std::uint8_t> &bytes) {
    std::uint32_t h = 2166136261u;
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 16777619u;
    }
    return h;
}

std::vector<std::uint8_t> serialize(const Frame &frame) {
    std::vector<std::uint8_t> out = serialize_body(frame);
    put_u32(out, compute_checksum(out));
    return out;
}

std::optional<Frame> parse(const std::vector<std::uint8_t> &data) {
    if (data.size() < kWireHeaderSize + 4) return std::nullopt;

    std::uint16_t magic = get_u16(data, 0);
    if (magic != kMagic) return std::nullopt;

    std::uint8_t version = data[2];
    if (version != kVersion) return std::nullopt;

    Frame frame;
    frame.header.magic = magic;
    frame.header.version = version;
    frame.header.type = static_cast<FrameType>(data[3]);
    frame.header.flags = data[4];
    frame.header.sender = get_u64(data, 5);
    frame.header.receiver = get_u64(data, 13);
    frame.header.sequence = get_u32(data, 21);
    frame.header.payloadSize = get_u16(data, 25);

    std::size_t body_len = kWireHeaderSize + frame.header.payloadSize;
    if (data.size() < body_len + 4) return std::nullopt;

    std::uint32_t stored_cs = get_u32(data, body_len);
    std::uint32_t calc_cs = compute_checksum(
        std::vector<std::uint8_t>(data.begin(), data.begin() + body_len));
    if (stored_cs != calc_cs) return std::nullopt;

    frame.payload.assign(data.begin() + kWireHeaderSize, data.begin() + body_len);
    frame.checksum = calc_cs;
    return frame;
}

} // namespace ghostchat::protocol
