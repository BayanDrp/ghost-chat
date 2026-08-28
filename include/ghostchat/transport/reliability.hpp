#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ghostchat::transport {

struct PendingFrame {
    std::vector<std::uint8_t> raw;
    std::uint32_t sequence;
    int tries_left;
    std::chrono::milliseconds timeout;
    std::chrono::steady_clock::time_point next_attempt;
};

class ReliabilityTracker {
public:
    void add(std::uint32_t seq, std::vector<std::uint8_t> raw, int max_tries,
             std::chrono::milliseconds timeout);
    bool ack(std::uint32_t seq);
    std::vector<std::vector<std::uint8_t>> resend_due(
        std::chrono::steady_clock::time_point now);
    bool pending() const { return !pending_.empty(); }
    void clear() { pending_.clear(); }

private:
    std::unordered_map<std::uint32_t, PendingFrame> pending_;
};

} // namespace ghostchat::transport
