#pragma once

#include "ghostchat/protocol/frame.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ghostchat::radio {

class Radio {
public:
    virtual ~Radio() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool send(const std::vector<std::uint8_t> &frame) = 0;
    virtual std::optional<std::vector<std::uint8_t>> receive() = 0;
    virtual std::uint64_t local_address() const = 0;
    virtual const std::string &interface_name() const = 0;
};

using RadioPtr = std::shared_ptr<Radio>;

std::pair<RadioPtr, RadioPtr> make_loopback_pair(std::uint64_t addr_a,
                                                 std::uint64_t addr_b);

} // namespace ghostchat::radio
