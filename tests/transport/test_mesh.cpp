#include "ghostchat/protocol/codec.hpp"
#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/transport.hpp"

#include <cstdio>
#include <cstdint>
#include <vector>

using namespace ghostchat;

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d  -> %s\n", __FILE__, __LINE__,    \
                         #cond);                                               \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// A 2-hop chain a - b - c where a and c cannot hear each other directly.
static void test_mesh_flood_delivers() {
    auto radios = radio::make_loopback_mesh({
        {0x01, {0x02}},
        {0x02, {0x01, 0x03}},
        {0x03, {0x02}},
    });
    transport::Transport a(radios[0]), b(radios[1]), c(radios[2]);

    std::uint64_t got_from = 0;
    std::vector<std::uint8_t> got_msg;
    bool acked = false;
    c.on_message([&](std::uint64_t from, const std::vector<std::uint8_t> &p) {
        got_from = from;
        got_msg = p;
    });
    a.on_ack([&](std::uint32_t) { acked = true; });

    a.send(0x03, {'h', 'i'});

    for (int i = 0; i < 30 && got_msg.empty(); ++i) {
        a.poll();
        b.poll();
        c.poll();
    }
    CHECK(!got_msg.empty());        // message reached c over 2 hops
    CHECK(got_from == 0x01);
    CHECK((got_msg == std::vector<std::uint8_t>{'h', 'i'}));

    for (int i = 0; i < 30 && !acked; ++i) {
        a.poll();
        b.poll();
        c.poll();
    }
    CHECK(acked);                   // end-to-end ACK came back over the hops
}

// A TTL of 1 must not cross a 2-hop chain: b drops it instead of relaying.
static void test_mesh_ttl_limits_range() {
    auto radios = radio::make_loopback_mesh({
        {0x01, {0x02}},
        {0x02, {0x01, 0x03}},
        {0x03, {0x02}},
    });
    transport::Transport c(radios[2]);

    bool got = false;
    c.on_message([&](std::uint64_t, const std::vector<std::uint8_t> &) {
        got = true;
    });

    protocol::Frame f = protocol::create_frame(
        protocol::FrameType::Message, 0x01, 0x03, 1,
        protocol::kFlagAckRequested, {'x'});
    f.header.ttl = 1;
    radios[0]->broadcast(protocol::serialize(f));

    for (int i = 0; i < 10 && !got; ++i) c.poll();
    CHECK(!got);  // b received it but ttl<=1, so it was not relayed to c
}

int main() {
    test_mesh_flood_delivers();
    test_mesh_ttl_limits_range();
    if (g_failures) {
        std::fprintf(stderr, "%d mesh test(s) failed\n", g_failures);
        return 1;
    }
    std::printf("mesh tests passed\n");
    return 0;
}
