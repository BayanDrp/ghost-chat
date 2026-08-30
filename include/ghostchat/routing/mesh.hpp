#pragma once

#include "ghostchat/protocol/frame.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <tuple>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ghostchat::routing {

// Mesh router. Combines the Approach A controlled-flood relay with the
// Approach B (on-demand RREQ/RREP) routing table.
//
// It answers these questions for the transport layer and never touches the radio
// itself — Transport calls it and does the actual transmit:
//   * is this frame for me?                 -> is_for_me()
//   * a frame for someone else: forward it (unicast via a learned route, or
//     flood), or drop it?                   -> forward()  (relay() kept as flood-only until Step 5)
//   * must an originator flood to reach dst?-> needs_flood()
//   * do we already know a next-hop for dst?-> knows_route() / next_hop()
//
// State kept: a `seen` set of (sender, seq) so the same frame is relayed only
// once (prevents loops / broadcast storms), plus a `routes_` table (dst ->
// next-hop) learned from RREQ/RREP, aged out by purge().
class MeshRouter {
  public:
    explicit MeshRouter(std::uint64_t self, std::uint8_t default_ttl = 8);

    // True if the frame is destined for us or is a local broadcast.
    bool is_for_me(const protocol::Frame &frame) const;

    // Flood a control frame (RREQ/RREP) to all neighbors. Does NOT stop at
    // is_for_me(), because control frames use a broadcast receiver (RREQ) or
    // someone else's receiver (RREP) yet must still be repeated. Still applies the
    // own-sender / ttl / seen drops to prevent storms.
    std::optional<protocol::Frame> flood(const protocol::Frame &frame);

    // Result of forward(): the frame to re-transmit, plus how to send it.
    struct ForwardPlan {
        protocol::Frame frame;        // ttl decremented, kFlagRelayed set
        bool flood = true;            // true => broadcast; false => unicast to next_hop
        std::uint64_t next_hop = 0;   // unicast destination (valid when !flood)
    };

    // Frame is for someone else: decide how to re-transmit it, or return
    // std::nullopt to drop it (our own echo, hop limit, already relayed, or it's
    // actually for us). If we know a route to frame.header.receiver, the plan is
    // unicast to that next_hop; otherwise it floods (broadcast). This is the
    // route-aware successor to relay().
    std::optional<ForwardPlan> forward(const protocol::Frame &frame);

    // Whether an originator must flood (instead of unicast) to reach dst, given
    // the list of directly reachable node ids.
    bool needs_flood(const std::vector<std::uint64_t> &direct_neighbors, std::uint64_t dst) const;

    std::uint8_t default_ttl() const { return default_ttl_; }

    // --- Approach B (on-demand routing) state/accessors ---
    // True if we currently have a next-hop for dst.
    bool knows_route(std::uint64_t dst) const;
    // Next-hop node id for dst. Only valid when knows_route(dst) is true.
    std::uint64_t next_hop(std::uint64_t dst) const;
    // Record dst -> via (via is always a direct neighbor). Stamps last_seen.
    void learn_route(std::uint64_t dst, std::uint64_t via,
                     std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    // Drop any route we hold for dst (used on ACK failure / repair).
    void forget_route(std::uint64_t dst);
    // Erase routes older than kRouteExpiry.
    void purge(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    static constexpr std::chrono::seconds kRouteExpiry{5};

  private:
    std::uint64_t self_;
    std::uint8_t default_ttl_;
    // (sender, type, seq) of frames we've already repeated, so the same frame is
    // relayed only once. Includes the type so different frame types that happen to
    // share a seq (e.g. a RREP and an ACK from the same node) don't collide.
    std::set<std::tuple<std::uint64_t, std::uint8_t, std::uint32_t>> seen_;
    std::unordered_map<std::uint64_t, std::uint64_t> routes_;
    std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> last_seen_;
};

} // namespace ghostchat::routing
