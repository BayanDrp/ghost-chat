#include "ghostchat/transport/connection.hpp"

namespace ghostchat::transport {

Connection::Connection(std::uint64_t peer) : peer_(peer) {}
std::uint64_t Connection::peer() const { return peer_; }

} // namespace ghostchat::transport
