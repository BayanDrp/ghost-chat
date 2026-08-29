#pragma once

#include <array>
#include <cstdint>
#include <net/ethernet.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "ghostchat/radio/radio.hpp"

namespace ghostchat::radio {

class AFPacketRadio : public Radio {
public:
    explicit AFPacketRadio(std::string interface_name, std::uint64_t node_id);

    bool start();
    void stop();

    bool send(const std::vector<std::uint8_t> &frame);
    bool broadcast(const std::vector<std::uint8_t> &frame) override;
    std::optional<std::vector<std::uint8_t>> receive();

    std::uint64_t local_address() const;
    const std::string &interface_name() const;
    std::vector<std::uint64_t> neighbors() const;

private:
    bool raw_send(const std::vector<std::uint8_t> &frame,
                  const std::uint8_t dst_mac[ETH_ALEN]);
    void learn(const std::uint8_t *src_mac, const std::vector<std::uint8_t> &payload);
    std::vector<std::uint8_t> make_discovery(bool response, std::uint64_t dst_id);

    std::string name_;
    std::uint64_t node_id_ = 0;
    int sockfd_ = -1;
    int ifindex_ = 0;
    std::uint8_t local_mac_[ETH_ALEN]{};
    std::unordered_map<std::uint64_t, std::array<std::uint8_t, ETH_ALEN>> neighbors_;
    std::unordered_map<std::uint64_t, std::vector<std::vector<std::uint8_t>>>
        pending_unknown_;
    std::uint32_t discovery_seq_ = 0;
};

} // namespace ghostchat::radio
