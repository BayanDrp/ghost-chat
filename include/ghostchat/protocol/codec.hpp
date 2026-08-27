#pragma once

#include "frame.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ghostchat::protocol {

std::vector<std::uint8_t> serialize(const Frame &frame);

std::optional<Frame> parse(const std::vector<std::uint8_t> &data);

std::uint32_t compute_checksum(const std::vector<std::uint8_t> &bytes);

} // namespace ghostchat::protocol
