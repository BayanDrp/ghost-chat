#pragma once

#include <cstdint>

namespace ghostchat::transport {

class Connection {
public:
    explicit Connection(std::uint64_t peer);
    std::uint64_t peer() const;

private:
    std::uint64_t peer_;
};

} // namespace ghostchat::transport
