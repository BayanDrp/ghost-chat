#pragma once

#include "ghostchat/protocol/frame.hpp"

#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace ghostchat::routing {

// Controlled-flood mesh router (Approach A).
//
// It answers two questions for the transport layer and never touches the radio
// itself — Transport calls it and does the actual transmit:
//   * is this frame for me?            -> is_for_me()
//   * a frame for someone else: relay it, or drop it? -> relay()
//   * must an originator flood to reach dst?          -> needs_flood()
//
// State kept: a `seen` set of (sender, seq) so the same frame is relayed only
// once (prevents loops / broadcast storms). This is the natural place to later
// add Approach B (RREQ/RREP) or C (OGM) routing tables.
class MeshRouter {
public:
    explicit MeshRouter(std::uint64_t self, std::uint8_t default_ttl = 8);

    // True if the frame is destined for us or is a local broadcast.
    bool is_for_me(const protocol::Frame &frame) const;

    // Frame is for someone else: return the frame to re-transmit (ttl decremented,
    // kFlagRelayed set), or std::nullopt to drop it (our own echo, hop limit
    // reached, or already relayed).
    std::optional<protocol::Frame> relay(const protocol::Frame &frame);

    // Whether an originator must flood (instead of unicast) to reach dst, given
    // the list of directly reachable node ids.
    bool needs_flood(const std::vector<std::uint64_t> &direct_neighbors,
                     std::uint64_t dst) const;

    std::uint8_t default_ttl() const { return default_ttl_; }

private:
    std::uint64_t self_;
    std::uint8_t default_ttl_;
    std::set<std::pair<std::uint64_t, std::uint32_t>> seen_;
};

} // namespace ghostchat::routing
