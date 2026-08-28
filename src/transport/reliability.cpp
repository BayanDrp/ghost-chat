#include "ghostchat/transport/reliability.hpp"

namespace ghostchat::transport {

void ReliabilityTracker::add(std::uint32_t seq, std::vector<std::uint8_t> raw,
                            int max_tries, std::chrono::milliseconds timeout) {
    PendingFrame pf;
    pf.raw = std::move(raw);
    pf.sequence = seq;
    pf.tries_left = max_tries;
    pf.timeout = timeout;
    pf.next_attempt = std::chrono::steady_clock::now() + timeout;
    pending_[seq] = std::move(pf);
}

bool ReliabilityTracker::ack(std::uint32_t seq) {
    return pending_.erase(seq) > 0;
}

std::vector<std::vector<std::uint8_t>> ReliabilityTracker::resend_due(
    std::chrono::steady_clock::time_point now) {
    std::vector<std::vector<std::uint8_t>> out;
    for (auto it = pending_.begin(); it != pending_.end();) {
        auto &pf = it->second;
        if (now >= pf.next_attempt) {
            if (pf.tries_left <= 0) {
                it = pending_.erase(it);
                continue;
            }
            out.push_back(pf.raw);
            --pf.tries_left;
            pf.next_attempt = now + pf.timeout;
        }
        ++it;
    }
    return out;
}

} // namespace ghostchat::transport
