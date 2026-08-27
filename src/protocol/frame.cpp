#include "ghostchat/protocol/frame.hpp"

namespace ghostchat::protocol {

Frame create_frame(FrameType type, std::uint64_t sender, std::uint64_t receiver,
                   std::uint32_t sequence, std::uint8_t flags,
                   const std::vector<std::uint8_t> &payload) {
    Frame frame;
    frame.header.magic = kMagic;
    frame.header.version = kVersion;
    frame.header.type = type;
    frame.header.flags = flags;
    frame.header.sender = sender;
    frame.header.receiver = receiver;
    frame.header.sequence = sequence;
    frame.header.payloadSize = payload.size();
    frame.payload = payload;
    return frame;
}

} // namespace ghostchat::protocol
