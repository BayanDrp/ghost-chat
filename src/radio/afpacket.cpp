#include "ghostchat/radio/afpacket.hpp"

#include "ghostchat/protocol/codec.hpp"
#include "ghostchat/protocol/frame.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ghostchat::radio {

using namespace ghostchat::protocol;

constexpr std::uint16_t kEtherType = 0x6767;
constexpr std::size_t kEthHeaderSize = 14;
constexpr std::size_t kMaxEthFrame = 1514;

static std::uint64_t read_u64_le(const std::uint8_t *p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return v;
}

AFPacketRadio::AFPacketRadio(std::string interface_name, std::uint64_t node_id)
    : name_(std::move(interface_name)), node_id_(node_id), sockfd_(-1), ifindex_(0) {
    std::memset(local_mac_, 0xFF, ETH_ALEN);
}

bool AFPacketRadio::start() {
    sockfd_ = socket(AF_PACKET, SOCK_RAW, htons(kEtherType));
    if (sockfd_ < 0)
        return false;

    ifindex_ = if_nametoindex(name_.c_str());
    if (ifindex_ == 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(sockfd_, SIOCGIFHWADDR, &ifr) == 0) {
        std::memcpy(local_mac_, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    }

    struct sockaddr_ll sll{};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex_;
    sll.sll_protocol = htons(kEtherType);
    if (bind(sockfd_, reinterpret_cast<struct sockaddr *>(&sll), sizeof(sll)) < 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    fcntl(sockfd_, F_SETFL, O_NONBLOCK);
    return true;
}

void AFPacketRadio::stop() {
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
}

std::vector<std::uint8_t> AFPacketRadio::make_discovery(bool response, std::uint64_t dst_id) {
    std::uint8_t flags = response ? kFlagDiscoveryResponse : 0;
    Frame f = create_frame(FrameType::Discovery, node_id_, dst_id, discovery_seq_++, flags, {});
    return serialize(f);
}

bool AFPacketRadio::raw_send(const std::vector<std::uint8_t> &frame,
                             const std::uint8_t dst_mac[ETH_ALEN]) {
    std::uint8_t buf[kMaxEthFrame];
    std::memcpy(buf, dst_mac, ETH_ALEN);
    std::memcpy(buf + ETH_ALEN, local_mac_, ETH_ALEN);
    std::uint16_t et = htons(kEtherType);
    std::memcpy(buf + 2 * ETH_ALEN, &et, 2);
    std::memcpy(buf + kEthHeaderSize, frame.data(), frame.size());

    struct sockaddr_ll sa{};
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = ifindex_;
    sa.sll_halen = ETH_ALEN;
    std::memcpy(sa.sll_addr, dst_mac, ETH_ALEN);

    ssize_t n = sendto(sockfd_, buf, kEthHeaderSize + frame.size(), 0,
                       reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa));
    return n > 0;
}

void AFPacketRadio::learn(const std::uint8_t *src_mac, const std::vector<std::uint8_t> &payload) {
    if (payload.size() < 13 + 8)
        return;
    std::uint64_t sid = read_u64_le(payload.data() + 5);
    std::array<std::uint8_t, ETH_ALEN> mac;
    std::memcpy(mac.data(), src_mac, ETH_ALEN);

    bool was_new = neighbors_.find(sid) == neighbors_.end();
    neighbors_[sid] = mac;
    if (was_new) {
        auto it = pending_unknown_.find(sid);
        if (it != pending_unknown_.end()) {
            auto frames = std::move(it->second);
            pending_unknown_.erase(it);
            for (auto &f : frames)
                send(f);
        }
    }
}

bool AFPacketRadio::send(const std::vector<std::uint8_t> &frame) {
    if (sockfd_ < 0)
        return false;
    if (frame.size() + kEthHeaderSize > kMaxEthFrame)
        return false;
    if (frame.size() < 4)
        return false;

    FrameType type = static_cast<FrameType>(frame[3]);
    std::uint64_t dst = (frame.size() >= 13 + 8) ? read_u64_le(frame.data() + 13) : 0;

    std::uint8_t dst_mac[ETH_ALEN];
    auto it = neighbors_.find(dst);
    bool known = (it != neighbors_.end());
    if (known) {
        std::memcpy(dst_mac, it->second.data(), ETH_ALEN);
        return raw_send(frame, dst_mac);
    }

    if (type == FrameType::Discovery) {
        std::memset(dst_mac, 0xFF, ETH_ALEN);
        return raw_send(frame, dst_mac);
    }

    std::vector<std::uint8_t> probe = make_discovery(false, kBroadcastAddress);
    std::memset(dst_mac, 0xFF, ETH_ALEN);
    if (!raw_send(probe, dst_mac))
        return false;
    pending_unknown_[dst].push_back(frame);
    return true;
}
bool AFPacketRadio::send_to(const std::vector<std::uint8_t> &frame, std::uint64_t next_hop) {
    if (sockfd_ < 0)
        return false;
    if (frame.size() + kEthHeaderSize > kMaxEthFrame)
        return false;
    std::uint8_t dst_mac[ETH_ALEN];
    auto it = neighbors_.find(next_hop);
    if (it == neighbors_.end())
        std::memset(dst_mac, 0xFF, ETH_ALEN); // safety fallback: broadcast
    else
        std::memcpy(dst_mac, it->second.data(), ETH_ALEN);
    return raw_send(frame, dst_mac);
}

bool AFPacketRadio::broadcast(const std::vector<std::uint8_t> &frame) {
    if (sockfd_ < 0)
        return false;
    if (frame.size() + kEthHeaderSize > kMaxEthFrame)
        return false;
    std::uint8_t dst_mac[ETH_ALEN];
    std::memset(dst_mac, 0xFF, ETH_ALEN);
    return raw_send(frame, dst_mac);
}

std::optional<std::vector<std::uint8_t>> AFPacketRadio::receive() {
    if (sockfd_ < 0)
        return std::nullopt;

    std::uint8_t buf[kMaxEthFrame];
    ssize_t n = recvfrom(sockfd_, buf, sizeof(buf), MSG_DONTWAIT, nullptr, nullptr);
    if (n < static_cast<ssize_t>(kEthHeaderSize))
        return std::nullopt;

    std::uint16_t et{};
    std::memcpy(&et, buf + 2 * ETH_ALEN, 2);
    if (ntohs(et) != kEtherType)
        return std::nullopt;

    bool is_broadcast = true;
    for (int i = 0; i < ETH_ALEN; ++i)
        if (buf[i] != 0xFF)
            is_broadcast = false;
    bool is_ours = std::memcmp(buf, local_mac_, ETH_ALEN) == 0;
    if (!is_broadcast && !is_ours)
        return std::nullopt;

    if (std::memcmp(buf + ETH_ALEN, local_mac_, ETH_ALEN) == 0)
        return std::nullopt;

    std::vector<std::uint8_t> payload(buf + kEthHeaderSize, buf + n);
    learn(buf + ETH_ALEN, payload);

    if (payload.size() >= 4 && static_cast<FrameType>(payload[3]) == FrameType::Discovery) {
        if (!(payload[4] & kFlagDiscoveryResponse)) {
            std::uint64_t sid = read_u64_le(payload.data() + 5);
            send(make_discovery(true, sid));
        }
    }

    return payload;
}

std::uint64_t AFPacketRadio::local_address() const { return node_id_; }

const std::string &AFPacketRadio::interface_name() const { return name_; }

std::vector<std::uint64_t> AFPacketRadio::neighbors() const {
    std::vector<std::uint64_t> out;
    out.reserve(neighbors_.size());
    for (const auto &kv : neighbors_)
        out.push_back(kv.first);
    return out;
}

} // namespace ghostchat::radio
