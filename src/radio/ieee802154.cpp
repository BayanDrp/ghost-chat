#include "ghostchat/radio/ieee802154.hpp"

#include "ghostchat/protocol/codec.hpp"
#include "ghostchat/protocol/frame.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>

namespace ghostchat::radio {

using namespace ghostchat::protocol;

// The 802.15.4 sockaddr is a stable kernel ABI but not exposed in Fedora's
// userspace headers, so we define the minimal bits we need.
#define GC_IEEE802154_ADDR_EXTENDED 2
#define GC_IEEE802154_PAN 0xffff

struct gc_ieee802154_addr {
    std::uint8_t mode;
    std::uint16_t pan_id;  // little-endian on the wire
    union {
        std::uint16_t short_addr;
        std::uint64_t extended_addr;  // little-endian on the wire
    };
};

struct gc_sockaddr_ieee802154 {
    sa_family_t family;
    gc_ieee802154_addr addr;
};

constexpr std::size_t kAddrLen = 8;

Ieee802154Radio::Ieee802154Radio(std::string interface_name,
                                 std::uint64_t node_id)
    : name_(std::move(interface_name)), node_id_(node_id), sockfd_(-1) {
    std::memset(local_addr_, 0, kAddrLen);
}

bool Ieee802154Radio::start() {
    sockfd_ = socket(AF_IEEE802154, SOCK_DGRAM, 0);
    if (sockfd_ < 0) return false;

    // Bind the socket to this radio interface (no ethertype exists at this
    // layer, so the device selects which PHY we send/receive on).
    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);
    if (setsockopt(sockfd_, SOL_SOCKET, SO_BINDTODEVICE, name_.c_str(),
                   static_cast<socklen_t>(name_.size() + 1)) < 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    if (ioctl(sockfd_, SIOCGIFHWADDR, &ifr) == 0)
        std::memcpy(local_addr_, ifr.ifr_hwaddr.sa_data, kAddrLen);

    fcntl(sockfd_, F_SETFL, O_NONBLOCK);
    return true;
}

void Ieee802154Radio::stop() {
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
}

std::vector<std::uint8_t> Ieee802154Radio::make_discovery(bool response,
                                                         std::uint64_t dst_id) {
    std::uint8_t flags = response ? kFlagDiscoveryResponse : 0;
    Frame f = create_frame(FrameType::Discovery, node_id_, dst_id,
                            discovery_seq_++, flags, {});
    return serialize(f);
}

bool Ieee802154Radio::raw_send(const std::vector<std::uint8_t> &frame,
                               const std::uint8_t dst_addr[8]) {
    gc_sockaddr_ieee802154 sa{};
    sa.family = AF_IEEE802154;
    sa.addr.mode = GC_IEEE802154_ADDR_EXTENDED;
    sa.addr.pan_id = htole16(GC_IEEE802154_PAN);
    std::memcpy(&sa.addr.extended_addr, dst_addr, kAddrLen);

    ssize_t n = sendto(sockfd_, frame.data(), frame.size(), 0,
                       reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa));
    return n > 0;
}

void Ieee802154Radio::learn(const std::uint8_t *src_addr,
                            const std::vector<std::uint8_t> &payload) {
    if (payload.size() < 13 + 8) return;
    std::uint64_t sid = 0;
    for (int i = 0; i < 8; ++i)
        sid |= static_cast<std::uint64_t>(payload[5 + i]) << (8 * i);

    std::array<std::uint8_t, 8> mac;
    std::memcpy(mac.data(), src_addr, kAddrLen);

    bool was_new = neighbors_.find(sid) == neighbors_.end();
    neighbors_[sid] = mac;
    if (was_new) {
        auto it = pending_unknown_.find(sid);
        if (it != pending_unknown_.end()) {
            auto frames = std::move(it->second);
            pending_unknown_.erase(it);
            for (auto &f : frames) send(f);
        }
    }
}

bool Ieee802154Radio::send(const std::vector<std::uint8_t> &frame) {
    if (sockfd_ < 0) return false;
    if (frame.size() < 4) return false;

    FrameType type = static_cast<FrameType>(frame[3]);
    std::uint64_t dst = 0;
    if (frame.size() >= 13 + 8)
        for (int i = 0; i < 8; ++i)
            dst |= static_cast<std::uint64_t>(frame[13 + i]) << (8 * i);

    std::uint8_t dst_mac[8];
    auto it = neighbors_.find(dst);
    bool known = (it != neighbors_.end());
    if (known) {
        std::memcpy(dst_mac, it->second.data(), kAddrLen);
        return raw_send(frame, dst_mac);
    }

    if (type == FrameType::Discovery) {
        std::memset(dst_mac, 0xFF, kAddrLen);
        return raw_send(frame, dst_mac);
    }

    std::vector<std::uint8_t> probe = make_discovery(false, kBroadcastAddress);
    std::memset(dst_mac, 0xFF, kAddrLen);
    if (!raw_send(probe, dst_mac)) return false;
    pending_unknown_[dst].push_back(frame);
    return true;
}

bool Ieee802154Radio::broadcast(const std::vector<std::uint8_t> &frame) {
    if (sockfd_ < 0) return false;
    std::uint8_t dst_addr[8];
    std::memset(dst_addr, 0xFF, 8);
    return raw_send(frame, dst_addr);
}

std::optional<std::vector<std::uint8_t>> Ieee802154Radio::receive() {
    if (sockfd_ < 0) return std::nullopt;

    std::uint8_t buf[2048];
    gc_sockaddr_ieee802154 sa{};
    socklen_t slen = sizeof(sa);
    ssize_t n = recvfrom(sockfd_, buf, sizeof(buf), MSG_DONTWAIT,
                         reinterpret_cast<struct sockaddr *>(&sa), &slen);
    if (n < 1) return std::nullopt;

    std::uint8_t *src = reinterpret_cast<std::uint8_t *>(&sa.addr.extended_addr);

    // Drop our own transmissions.
    if (std::memcmp(src, local_addr_, kAddrLen) == 0) return std::nullopt;

    std::vector<std::uint8_t> payload(buf, buf + n);
    learn(src, payload);

    if (payload.size() >= 4 &&
        static_cast<FrameType>(payload[3]) == FrameType::Discovery) {
        if (!(payload[4] & kFlagDiscoveryResponse)) {
            std::uint64_t sid = 0;
            for (int i = 0; i < 8; ++i)
                sid |= static_cast<std::uint64_t>(payload[5 + i]) << (8 * i);
            send(make_discovery(true, sid));
        }
    }

    return payload;
}

std::uint64_t Ieee802154Radio::local_address() const { return node_id_; }

const std::string &Ieee802154Radio::interface_name() const { return name_; }

std::vector<std::uint64_t> Ieee802154Radio::neighbors() const {
    std::vector<std::uint64_t> out;
    out.reserve(neighbors_.size());
    for (const auto &kv : neighbors_) out.push_back(kv.first);
    return out;
}

} // namespace ghostchat::radio
