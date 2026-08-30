#include "ghostchat/transport/transport.hpp"

#include "ghostchat/protocol/codec.hpp"
#include "ghostchat/protocol/frame.hpp"

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

namespace ghostchat::transport {

using namespace ghostchat::protocol;

namespace {
// RREQ/RREP payload layout: [target(8 LE) | prev_hop(8 LE)].
// `prev_hop` is always the node transmitting the frame right now, so an
// intermediate that receives it learns its next-hop toward the originator/target.
std::vector<std::uint8_t> pack_route(std::uint64_t target, std::uint64_t prev_hop) {
    std::vector<std::uint8_t> p(16);
    for (int i = 0; i < 8; ++i)
        p[i] = static_cast<std::uint8_t>((target >> (8 * i)) & 0xFF);
    for (int i = 0; i < 8; ++i)
        p[8 + i] = static_cast<std::uint8_t>((prev_hop >> (8 * i)) & 0xFF);
    return p;
}
std::pair<std::uint64_t, std::uint64_t> unpack_route(const std::vector<std::uint8_t> &p) {
    if (p.size() < 16) return {0, 0};
    std::uint64_t target = 0, prev = 0;
    for (int i = 0; i < 8; ++i)
        target |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    for (int i = 0; i < 8; ++i)
        prev |= static_cast<std::uint64_t>(p[8 + i]) << (8 * i);
    return {target, prev};
}
void set_prev_hop(std::vector<std::uint8_t> &p, std::uint64_t self) {
    if (p.size() < 16) return;
    for (int i = 0; i < 8; ++i)
        p[8 + i] = static_cast<std::uint8_t>((self >> (8 * i)) & 0xFF);
}
}  // namespace

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
    const std::uint64_t dst = frame.header.receiver;
    Frame out = frame;
    if (dst == kBroadcastAddress) {
        radio_->broadcast(serialize(out));
        return;
    }
    if (!mesh_.needs_flood(radio_->neighbors(), dst)) {
        radio_->send(serialize(out));  // direct neighbor
        return;
    }
    if (mesh_.knows_route(dst)) {       // unicast along learned route
        out.header.ttl = mesh_.default_ttl();
        radio_->send_to(serialize(out), mesh_.next_hop(dst));
        return;
    }
    out.header.ttl = mesh_.default_ttl();  // flood fallback
    radio_->broadcast(serialize(out));
}

// Route-aware (re)transmit of an already-serialized frame.
void Transport::deliver(const std::vector<std::uint8_t> &raw) {
    auto fr = parse(raw);
    if (!fr) { radio_->broadcast(raw); return; }
    const std::uint64_t dst = fr->header.receiver;
    if (dst == kBroadcastAddress) { radio_->broadcast(raw); return; }
    if (!mesh_.needs_flood(radio_->neighbors(), dst)) { radio_->send(raw); return; }
    if (mesh_.knows_route(dst)) {
        // Unicast via the learned next-hop. If that hop is gone, drop the route
        // and fall through to the flood fallback (route repair).
        if (radio_->send_to(raw, mesh_.next_hop(dst))) return;
        mesh_.forget_route(dst);
    }
    radio_->broadcast(raw);  // flood fallback
}

void Transport::send_rreq(std::uint64_t dst) {
    Frame rreq = create_frame(FrameType::RouteRequest, self(), kBroadcastAddress,
                              next_seq_++, 0, pack_route(dst, self()));
    rreq.header.ttl = mesh_.default_ttl();
    radio_->broadcast(serialize(rreq));
}

void Transport::send_rrep(std::uint64_t originator, std::uint64_t final_dst) {
    Frame rrep = create_frame(FrameType::RouteReply, self(), originator,
                              next_seq_++, 0, pack_route(final_dst, self()));
    rrep.header.ttl = mesh_.default_ttl();
    if (mesh_.knows_route(originator))
        radio_->send_to(serialize(rrep), mesh_.next_hop(originator));
    else
        radio_->broadcast(serialize(rrep));  // safety: shouldn't happen
}

void Transport::flush_pending(std::uint64_t dst) {
    auto it = pending_data_.find(dst);
    if (it == pending_data_.end()) return;
    for (auto &raw : it->second) deliver(raw);  // route exists now -> unicast
    pending_data_.erase(it);
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
    if (!direct) f.header.ttl = mesh_.default_ttl();  // multi-hop needs a hop limit
    auto raw = serialize(f);
    tracker_.add(seq, raw, max_tries_, timeout_);

    if (direct) {                       // one-hop: straight unicast
        radio_->send(raw);
        return true;
    }
    if (mesh_.knows_route(dst)) {       // we already know the path
        deliver(raw);
        return true;
    }
    // Unknown multi-hop destination: ask for a route, queue the data until the
    // RREP arrives (resends will flood it as a fallback in the meantime).
    send_rreq(dst);
    pending_data_[dst].push_back(raw);
    return true;
}

void Transport::poll() {
    auto now = std::chrono::steady_clock::now();
    for (auto &raw : tracker_.resend_due(now)) deliver(raw);

    std::optional<std::vector<std::uint8_t>> f;
    while ((f = radio_->receive()).has_value()) {
        auto frame = parse(*f);
        if (!frame) continue;
        auto &h = frame->header;

        // --- Approach B control frames (handled regardless of receiver) ---
        if (h.type == FrameType::RouteRequest) {
            if (h.sender == self()) continue;          // ignore our own RREQ echoed back
            auto [target, prev] = unpack_route(frame->payload);
            mesh_.learn_route(h.sender, prev, now);     // reverse route to originator
            if (target == self()) {
                send_rrep(h.sender, target);            // we are the destination
            } else {
                auto r = mesh_.flood(*frame);           // flood the RREQ onward
                if (r) {
                    set_prev_hop(r->payload, self());
                    radio_->broadcast(serialize(*r));
                }
            }
            continue;
        }
        if (h.type == FrameType::RouteReply) {
            if (h.sender == self()) continue;          // ignore our own RREP echoed back
            auto [target, prev] = unpack_route(frame->payload);
            mesh_.learn_route(target, prev, now);       // forward route to target
            if (h.receiver == self()) {
                flush_pending(target);                 // route ready: send queued data
            } else {
                auto r = mesh_.flood(*frame);          // relay RREP back toward originator
                if (r) {
                    set_prev_hop(r->payload, self());
                    radio_->broadcast(serialize(*r));
                }
            }
            continue;
        }

        // --- Data / discovery / ack frames ---
        bool for_me = mesh_.is_for_me(*frame);
        if (!for_me) {
            auto fp = mesh_.forward(*frame);
            if (fp) {
                if (fp->flood) radio_->broadcast(serialize(fp->frame));
                else radio_->send_to(serialize(fp->frame), fp->next_hop);
            }
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
