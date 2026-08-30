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

bool MeshRouter::needs_flood(const std::vector<std::uint64_t> &direct_neighbors,
                              std::uint64_t dst) const {
    for (auto n : direct_neighbors)
        if (n == dst) return false;  // direct neighbor -> unicast
    return true;                     // unknown -> flood
}

std::optional<Frame> MeshRouter::flood(const protocol::Frame &frame) {
    if (frame.header.sender == self_) return std::nullopt;   // never echo our own
    if (frame.header.ttl <= 1) return std::nullopt;          // hop limit reached
    auto key = std::make_tuple(frame.header.sender,
                               static_cast<std::uint8_t>(frame.header.type),
                               frame.header.sequence);
    if (seen_.count(key)) return std::nullopt;               // already relayed
    seen_.insert(key);

    Frame out = frame;
    out.header.ttl -= 1;
    out.header.flags |= kFlagRelayed;
    return out;
}

std::optional<MeshRouter::ForwardPlan> MeshRouter::forward(const protocol::Frame &frame) {
    if (is_for_me(frame)) return std::nullopt;                // Transport handles it
    if (frame.header.sender == self_) return std::nullopt;   // never echo our own
    if (frame.header.ttl <= 1) return std::nullopt;          // hop limit reached
    auto key = std::make_tuple(frame.header.sender,
                               static_cast<std::uint8_t>(frame.header.type),
                               frame.header.sequence);
    if (seen_.count(key)) return std::nullopt;               // already relayed
    seen_.insert(key);

    Frame out = frame;
    out.header.ttl -= 1;
    out.header.flags |= kFlagRelayed;

    ForwardPlan plan;
    plan.frame = std::move(out);
    if (knows_route(frame.header.receiver)) {
        plan.flood = false;
        plan.next_hop = next_hop(frame.header.receiver);
    } else {
        plan.flood = true;
        plan.next_hop = 0;
    }
    return plan;
}

bool MeshRouter::knows_route(std::uint64_t dst) const {
    return routes_.count(dst) > 0;
}

std::uint64_t MeshRouter::next_hop(std::uint64_t dst) const {
    auto it = routes_.find(dst);
    return it == routes_.end() ? 0 : it->second;
}

void MeshRouter::learn_route(std::uint64_t dst, std::uint64_t via,
                             std::chrono::steady_clock::time_point now) {
    routes_[dst] = via;
    last_seen_[dst] = now;
}

void MeshRouter::forget_route(std::uint64_t dst) {
    routes_.erase(dst);
    last_seen_.erase(dst);
}

void MeshRouter::purge(std::chrono::steady_clock::time_point now) {
    for (auto it = routes_.begin(); it != routes_.end(); ) {
        if (now - last_seen_[it->first] > kRouteExpiry) {
            last_seen_.erase(it->first);
            it = routes_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace ghostchat::routing
