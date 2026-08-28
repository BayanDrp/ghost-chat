#pragma once

#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/reliability.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace ghostchat::transport {

class Transport {
public:
    explicit Transport(radio::RadioPtr radio);

    bool send(std::uint64_t dst, const std::vector<std::uint8_t> &payload);
    void poll();
    void on_message(
        std::function<void(std::uint64_t, const std::vector<std::uint8_t> &)> cb);
    void on_ack(std::function<void(std::uint32_t)> cb);

    bool pending() const;
    std::uint64_t self() const;

private:
    radio::RadioPtr radio_;
    ReliabilityTracker tracker_;
    std::uint32_t next_seq_ = 0;
    std::function<void(std::uint64_t, const std::vector<std::uint8_t> &)> msg_cb_;
    std::function<void(std::uint32_t)> ack_cb_;
    std::chrono::milliseconds timeout_{200};
    int max_tries_ = 5;
};

} // namespace ghostchat::transport
