#include "ghostchat/routing/mesh.hpp"

#include "ghostchat/protocol/frame.hpp"

namespace ghostchat::routing {

using namespace ghostchat::protocol;

MeshRouter::MeshRouter(std::uint64_t self, std::uint8_t default_ttl)
    : self_(self), default_ttl_(default_ttl) {}

bool MeshRouter::is_for_me(const Frame &frame) const {
    return frame.header.receiver == self_ ||
           frame.header.receiver == kBroadcastAddress;
}

std::optional<Frame> MeshRouter::relay(const Frame &frame) {
    if (is_for_me(frame)) return std::nullopt;                // Transport handles it
    if (frame.header.sender == self_) return std::nullopt;   // never echo our own
    if (frame.header.ttl <= 1) return std::nullopt;          // hop limit reached
    if (seen_.count({frame.header.sender, frame.header.sequence}))
        return std::nullopt;                                  // already relayed
    seen_.insert({frame.header.sender, frame.header.sequence});

    Frame out = frame;
    out.header.ttl -= 1;
    out.header.flags |= kFlagRelayed;
    return out;
}

bool MeshRouter::needs_flood(const std::vector<std::uint64_t> &direct_neighbors,
                             std::uint64_t dst) const {
    for (auto n : direct_neighbors)
        if (n == dst) return false;  // direct neighbor -> unicast
    return true;                     // unknown -> flood
}

} // namespace ghostchat::routing
