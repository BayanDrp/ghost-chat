#include "ghostchat/transport/transport.hpp"

#include "ghostchat/protocol/codec.hpp"
#include "ghostchat/protocol/frame.hpp"

#include <chrono>
#include <optional>

namespace ghostchat::transport {

using namespace ghostchat::protocol;

Transport::Transport(radio::RadioPtr radio)
    : radio_(std::move(radio)), mesh_(radio_->local_address()) {}

std::uint64_t Transport::self() const { return radio_->local_address(); }
bool Transport::pending() const { return tracker_.pending(); }

void Transport::on_message(
    std::function<void(std::uint64_t, const std::vector<std::uint8_t> &)> cb) {
    msg_cb_ = std::move(cb);
}
void Transport::on_ack(std::function<void(std::uint32_t)> cb) {
    ack_cb_ = std::move(cb);
}

void Transport::on_peer(std::function<void(std::uint64_t)> cb) {
    peer_cb_ = std::move(cb);
}

void Transport::set_key(const std::string &passphrase) {
    key_ = crypto::derive_key(passphrase);
}

void Transport::discover() {
    std::uint8_t flags = 0;
    std::vector<std::uint8_t> payload;
    if (key_) {
        payload = crypto::encrypt(*key_, {});
        flags |= kFlagEncrypted;
    }
    Frame f = create_frame(FrameType::Discovery, self(), kBroadcastAddress,
                            next_seq_++, flags, payload);
    radio_->send(serialize(f));
}

void Transport::emit(const protocol::Frame &frame) {
    bool direct = !mesh_.needs_flood(radio_->neighbors(), frame.header.receiver);
    if (direct) {
        radio_->send(serialize(frame));
    } else {
        // Flood: give it a hop limit so relays can repeat it.
        Frame f = frame;
        f.header.ttl = mesh_.default_ttl();
        radio_->broadcast(serialize(f));
    }
}

bool Transport::send(std::uint64_t dst, const std::vector<std::uint8_t> &payload) {
    std::uint32_t seq = next_seq_++;
    std::uint8_t flags = kFlagAckRequested;
    std::vector<std::uint8_t> out = payload;
    if (key_) {
        out = crypto::encrypt(*key_, payload);
        flags |= kFlagEncrypted;
    }
    Frame f = create_frame(FrameType::Message, self(), dst, seq, flags, out);
    bool direct = !mesh_.needs_flood(radio_->neighbors(), dst);
    if (!direct) f.header.ttl = mesh_.default_ttl();  // flood needs a hop limit
    auto raw = serialize(f);
    tracker_.add(seq, raw, max_tries_, timeout_);
    if (direct) return radio_->send(raw);
    return radio_->broadcast(raw);
}

void Transport::poll() {
    auto now = std::chrono::steady_clock::now();
    for (auto &raw : tracker_.resend_due(now)) {
        radio_->send(raw);
    }

        std::optional<std::vector<std::uint8_t>> f;
        while ((f = radio_->receive()).has_value()) {
            auto frame = parse(*f);
            if (!frame) continue;
            auto &h = frame->header;

            // Mesh relay: a frame addressed to someone else is repeated, not
            // consumed. This is what makes multi-hop delivery work.
            bool for_me = mesh_.is_for_me(*frame);
            if (!for_me) {
                auto relayed = mesh_.relay(*frame);
                if (relayed) radio_->broadcast(serialize(*relayed));
                continue;
            }

            if (h.flags & kFlagEncrypted) {
                if (!key_) continue;  // encryption on, but we hold no key: ignore
                auto pt = crypto::decrypt(*key_, frame->payload);
                if (!pt) continue;    // wrong key or tampered: drop the frame
                frame->payload = std::move(*pt);
            }

            if (h.type == FrameType::Ack) {
                tracker_.ack(h.sequence);
                peers_.insert(h.sender);
                if (ack_cb_) ack_cb_(h.sequence);
            } else if (h.type == FrameType::Discovery) {
                auto [it, is_new] = peers_.insert(h.sender);
                if (is_new && peer_cb_) peer_cb_(h.sender);
                if (!(h.flags & kFlagDiscoveryResponse)) {
                    std::uint8_t rflags = kFlagDiscoveryResponse;
                    std::vector<std::uint8_t> rpay;
                    if (key_) {
                        rpay = crypto::encrypt(*key_, {});
                        rflags |= kFlagEncrypted;
                    }
                    Frame resp = create_frame(FrameType::Discovery, self(), h.sender,
                                              next_seq_++, rflags, rpay);
                    emit(resp);
                }
            } else if (h.type == FrameType::Message) {
                auto [it, is_new] = peers_.insert(h.sender);
                if (is_new && peer_cb_) peer_cb_(h.sender);
                if (msg_cb_) msg_cb_(h.sender, frame->payload);
                if (h.flags & kFlagAckRequested) {
                    Frame ack =
                        create_frame(FrameType::Ack, self(), h.sender, h.sequence, 0, {});
                    emit(ack);
                }
            }
        }
}

std::vector<std::uint64_t> Transport::peers() const {
    return {peers_.begin(), peers_.end()};
}

} // namespace ghostchat::transport
