#include "ghostchat/radio/radio.hpp"

#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ghostchat::radio {

class LoopbackRadio : public Radio {
public:
    LoopbackRadio(std::uint64_t addr, std::uint64_t peer_addr,
                  std::shared_ptr<std::deque<std::vector<std::uint8_t>>> inbox,
                  std::shared_ptr<std::deque<std::vector<std::uint8_t>>> peer_inbox,
                  std::shared_ptr<std::mutex> mtx, std::string name)
        : addr_(addr), peer_addr_(peer_addr), inbox_(std::move(inbox)),
          peer_inbox_(std::move(peer_inbox)), mtx_(std::move(mtx)),
          name_(std::move(name)) {}

    bool start() override { return true; }
    void stop() override {}

    bool send(const std::vector<std::uint8_t> &frame) override {
        std::lock_guard<std::mutex> lk(*mtx_);
        peer_inbox_->push_back(frame);
        return true;
    }

    bool broadcast(const std::vector<std::uint8_t> &frame) override {
        return send(frame);
    }

    bool send_to(const std::vector<std::uint8_t> &frame, std::uint64_t next_hop) override {
        if (next_hop != peer_addr_) return false;  // not our direct peer
        return send(frame);
    }

    std::optional<std::vector<std::uint8_t>> receive() override {
        std::lock_guard<std::mutex> lk(*mtx_);
        if (inbox_->empty()) return std::nullopt;
        auto f = std::move(inbox_->front());
        inbox_->pop_front();
        return f;
    }

    std::uint64_t local_address() const override { return addr_; }
    const std::string &interface_name() const override { return name_; }
    std::vector<std::uint64_t> neighbors() const override { return {peer_addr_}; }

private:
    std::uint64_t addr_;
    std::uint64_t peer_addr_;
    std::shared_ptr<std::deque<std::vector<std::uint8_t>>> inbox_;
    std::shared_ptr<std::deque<std::vector<std::uint8_t>>> peer_inbox_;
    std::shared_ptr<std::mutex> mtx_;
    std::string name_;
};

std::pair<RadioPtr, RadioPtr> make_loopback_pair(std::uint64_t addr_a,
                                                std::uint64_t addr_b) {
    auto mtx = std::make_shared<std::mutex>();
    auto a_inbox = std::make_shared<std::deque<std::vector<std::uint8_t>>>();
    auto b_inbox = std::make_shared<std::deque<std::vector<std::uint8_t>>>();
    auto a = std::make_shared<LoopbackRadio>(addr_a, addr_b, a_inbox, b_inbox, mtx,
                                             "loopbackA");
    auto b = std::make_shared<LoopbackRadio>(addr_b, addr_a, b_inbox, a_inbox, mtx,
                                             "loopbackB");
    return {a, b};
}

// ---- Multi-hop test fabric -------------------------------------------------
// A shared bus: each node has its own inbox; sending/broadcasting delivers only
// to the inboxes of its listed neighbors, so a frame must be repeated by
// intermediate nodes to reach a node that isn't a direct neighbor.
class MeshFabric {
public:
    std::mutex mtx;
    std::unordered_map<std::uint64_t, std::shared_ptr<std::deque<std::vector<std::uint8_t>>>>
        inboxes;
};

class MeshRadio : public Radio {
public:
    MeshRadio(std::uint64_t addr, std::shared_ptr<MeshFabric> fabric,
              std::vector<std::uint64_t> nbrs, std::string name)
        : addr_(addr), fabric_(std::move(fabric)), nbrs_(std::move(nbrs)),
          name_(std::move(name)) {}

    bool start() override { return true; }
    void stop() override {}

    bool send(const std::vector<std::uint8_t> &frame) override {
        std::uint64_t dst = receiver_of(frame);
        std::lock_guard<std::mutex> lk(fabric_->mtx);
        for (auto n : nbrs_)
            if (n == dst && fabric_->inboxes.count(n))
                fabric_->inboxes[n]->push_back(frame);
        return true;
    }

    bool broadcast(const std::vector<std::uint8_t> &frame) override {
        std::lock_guard<std::mutex> lk(fabric_->mtx);
        for (auto n : nbrs_)
            if (fabric_->inboxes.count(n))
                fabric_->inboxes[n]->push_back(frame);
        return true;
    }

    bool send_to(const std::vector<std::uint8_t> &frame, std::uint64_t next_hop) override {
        std::lock_guard<std::mutex> lk(fabric_->mtx);
        auto it = fabric_->inboxes.find(next_hop);
        if (it == fabric_->inboxes.end()) return false;
        it->second->push_back(frame);
        return true;
    }

    std::optional<std::vector<std::uint8_t>> receive() override {
        auto ib = fabric_->inboxes[addr_];
        std::lock_guard<std::mutex> lk(fabric_->mtx);
        if (ib->empty()) return std::nullopt;
        auto f = std::move(ib->front());
        ib->pop_front();
        return f;
    }

    std::uint64_t local_address() const override { return addr_; }
    const std::string &interface_name() const override { return name_; }
    std::vector<std::uint64_t> neighbors() const override { return nbrs_; }

private:
    static std::uint64_t receiver_of(const std::vector<std::uint8_t> &frame) {
        if (frame.size() < 22) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(frame[14 + i]) << (8 * i);
        return v;
    }

    std::uint64_t addr_;
    std::shared_ptr<MeshFabric> fabric_;
    std::vector<std::uint64_t> nbrs_;
    std::string name_;
};

std::vector<RadioPtr> make_loopback_mesh(
    std::vector<std::pair<std::uint64_t, std::vector<std::uint64_t>>> links) {
    auto fabric = std::make_shared<MeshFabric>();
    for (auto &kv : links)
        fabric->inboxes[kv.first] =
            std::make_shared<std::deque<std::vector<std::uint8_t>>>();
    std::vector<RadioPtr> out;
    for (auto &kv : links) {
        std::string name = "mesh" + std::to_string(kv.first);
        out.push_back(std::make_shared<MeshRadio>(kv.first, fabric, kv.second,
                                                  name));
    }
    return out;
}

} // namespace ghostchat::radio
