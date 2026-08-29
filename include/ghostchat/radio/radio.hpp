#pragma once

#include "ghostchat/protocol/frame.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ghostchat::radio {

class Radio {
public:
    virtual ~Radio() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool send(const std::vector<std::uint8_t> &frame) = 0;
    virtual bool broadcast(const std::vector<std::uint8_t> &frame) = 0;
    virtual std::optional<std::vector<std::uint8_t>> receive() = 0;
    virtual std::uint64_t local_address() const = 0;
    virtual const std::string &interface_name() const = 0;
    // One-hop neighbors (nodes reachable directly). Used by the transport to
    // decide unicast vs flood. Returns node ids, not link addresses.
    virtual std::vector<std::uint64_t> neighbors() const = 0;
};

using RadioPtr = std::shared_ptr<Radio>;

std::pair<RadioPtr, RadioPtr> make_loopback_pair(std::uint64_t addr_a,
                                                std::uint64_t addr_b);

// Build a multi-hop test fabric: `links` maps each node id to the node ids it
// can hear directly. Returns one Radio per node; a frame sent/broadcast by a
// node is delivered only to its listed neighbors (simulating real radio range,
// so a->c must go through b).
std::vector<RadioPtr> make_loopback_mesh(
    std::vector<std::pair<std::uint64_t, std::vector<std::uint64_t>>> links);

} // namespace ghostchat::radio
