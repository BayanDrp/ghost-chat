#include "ghostchat/transport/transport.hpp"

#include "ghostchat/protocol/codec.hpp"
#include "ghostchat/protocol/frame.hpp"

#include <chrono>
#include <optional>

namespace ghostchat::transport {

using namespace ghostchat::protocol;

Transport::Transport(radio::RadioPtr radio) : radio_(std::move(radio)) {}

std::uint64_t Transport::self() const { return radio_->local_address(); }
bool Transport::pending() const { return tracker_.pending(); }

void Transport::on_message(
    std::function<void(std::uint64_t, const std::vector<std::uint8_t> &)> cb) {
    msg_cb_ = std::move(cb);
}
void Transport::on_ack(std::function<void(std::uint32_t)> cb) {
    ack_cb_ = std::move(cb);
}

bool Transport::send(std::uint64_t dst, const std::vector<std::uint8_t> &payload) {
    std::uint32_t seq = next_seq_++;
    Frame f = create_frame(FrameType::Message, self(), dst, seq,
                           kFlagAckRequested, payload);
    auto raw = serialize(f);
    tracker_.add(seq, raw, max_tries_, timeout_);
    return radio_->send(raw);
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

        if (h.type == FrameType::Ack) {
            tracker_.ack(h.sequence);
            if (ack_cb_) ack_cb_(h.sequence);
        } else if (h.type == FrameType::Message) {
            if (msg_cb_) msg_cb_(h.sender, frame->payload);
            if (h.flags & kFlagAckRequested) {
                Frame ack =
                    create_frame(FrameType::Ack, self(), h.sender, h.sequence, 0, {});
                radio_->send(serialize(ack));
            }
        }
    }
}

} // namespace ghostchat::transport
