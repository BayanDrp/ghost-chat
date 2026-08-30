#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ghostchat::protocol {

constexpr std::uint16_t kMagic = 0x6767;
constexpr std::uint8_t kVersion = 1;
constexpr std::uint16_t kMaxPayloadSize = 2304;
constexpr std::uint64_t kBroadcastAddress = 0xFFFFFFFFFFFFFFFF;

enum class FrameType : std::uint8_t {
    Discovery = 0x01,
    Message = 0x02,
    Ack = 0x03,
    RouteRequest = 0x04,
    RouteReply = 0x05
};

constexpr std::uint8_t kFlagAckRequested = 0x01;
constexpr std::uint8_t kFlagEncrypted = 0x02;
constexpr std::uint8_t kFlagRelayed = 0x04;
constexpr std::uint8_t kFlagDiscoveryResponse = 0x08;

struct FrameHeader {
    std::uint16_t magic;
    std::uint8_t version;
    FrameType type;
    std::uint8_t ttl;
    std::uint8_t flags;

    std::uint64_t sender;
    std::uint64_t receiver;

    std::uint32_t sequence;
    std::uint16_t payloadSize;
};

struct Frame {
    FrameHeader header;
    std::vector<std::uint8_t> payload;
    std::uint32_t checksum;
};

Frame create_frame(FrameType type, std::uint64_t sender, std::uint64_t receiver,
                   std::uint32_t sequence, std::uint8_t flags,
                   const std::vector<std::uint8_t> &payload);
constexpr std::size_t kHeaderSize = sizeof(FrameHeader);
constexpr std::size_t kWireHeaderSize = 28;
constexpr std::size_t kChecksumSize = sizeof(std::uint32_t);
constexpr std::size_t kMaxFrameSize = kWireHeaderSize + kMaxPayloadSize + kChecksumSize;

} // namespace ghostchat::protocol
