#pragma once

#include "ghostchat/crypto/crypto.hpp"
#include "ghostchat/radio/radio.hpp"
#include "ghostchat/routing/mesh.hpp"
#include "ghostchat/transport/reliability.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>

namespace ghostchat::transport {

class Transport {
public:
    explicit Transport(radio::RadioPtr radio);

    bool send(std::uint64_t dst, const std::vector<std::uint8_t> &payload);
    void discover();
    void poll();

    // Enable encryption. Optional: if never called, all frames stay plaintext
    // (backward compatible). When set, payloads are encrypted and marked with
    // kFlagEncrypted; receive() drops frames it cannot decrypt.
    void set_key(const std::string &passphrase);
    void on_message(
        std::function<void(std::uint64_t, const std::vector<std::uint8_t> &)> cb);
    void on_ack(std::function<void(std::uint32_t)> cb);
    void on_peer(std::function<void(std::uint64_t)> cb);

    bool pending() const;
    std::uint64_t self() const;
    std::vector<std::uint64_t> peers() const;

private:
    // Send a frame to its destination: unicast when the destination is a direct
    // neighbor, otherwise broadcast (flood) so intermediate nodes can relay it.
    void emit(const protocol::Frame &frame);

    radio::RadioPtr radio_;
    std::optional<crypto::Key> key_;
    ReliabilityTracker tracker_;
    std::unordered_set<std::uint64_t> peers_;
    routing::MeshRouter mesh_;
    std::uint32_t next_seq_ = 0;
    std::function<void(std::uint64_t, const std::vector<std::uint8_t> &)> msg_cb_;
    std::function<void(std::uint32_t)> ack_cb_;
    std::function<void(std::uint64_t)> peer_cb_;
    std::chrono::milliseconds timeout_{200};
    int max_tries_ = 5;
};

} // namespace ghostchat::transport
