#pragma once

#include "ghostchat/radio/radio.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ghostchat::radio {

// Real RF backend: IEEE 802.15.4 (low-power personal-area radio, the MAC layer
// used by Zigbee/Thread). Frames are true 802.15.4 MAC frames addressed by the
// interface's 64-bit extended address. Works over real 802.15.4 hardware, or on
// one machine via the kernel `fakelb`/`mac802154` loopback (see
// scripts/setup_802154.sh).
class Ieee802154Radio : public Radio {
public:
    Ieee802154Radio(std::string interface_name, std::uint64_t node_id);
    bool start() override;
    void stop() override;
    bool send(const std::vector<std::uint8_t> &frame) override;
    bool broadcast(const std::vector<std::uint8_t> &frame) override;
    std::optional<std::vector<std::uint8_t>> receive() override;
    std::uint64_t local_address() const override;
    const std::string &interface_name() const override;
    std::vector<std::uint64_t> neighbors() const override;

private:
    std::vector<std::uint8_t> make_discovery(bool response, std::uint64_t dst_id);
    bool raw_send(const std::vector<std::uint8_t> &frame,
                  const std::uint8_t dst_addr[8]);
    void learn(const std::uint8_t *src_addr, const std::vector<std::uint8_t> &payload);

    std::string name_;
    std::uint64_t node_id_;
    int sockfd_ = -1;
    std::uint8_t local_addr_[8]{};
    std::unordered_map<std::uint64_t, std::array<std::uint8_t, 8>> neighbors_;
    std::unordered_map<std::uint64_t, std::vector<std::vector<std::uint8_t>>>
        pending_unknown_;
    std::uint32_t discovery_seq_ = 0;
};

} // namespace ghostchat::radio
