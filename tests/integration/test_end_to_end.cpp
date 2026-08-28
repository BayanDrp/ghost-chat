#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/transport.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

namespace gc = ghostchat;

static int g_failures = 0;

#define CHECK(c)                                                              \
    do {                                                                     \
        if (!(c)) {                                                          \
            std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void test_end_to_end() {
    auto [ra, rb] = gc::radio::make_loopback_pair(0x01, 0x02);
    ra->start();
    rb->start();

    gc::transport::Transport ta(ra);
    gc::transport::Transport tb(rb);

    bool got_msg = false;
    tb.on_message([&](std::uint64_t, const std::vector<std::uint8_t> &p) {
        got_msg = (p == std::vector<std::uint8_t>{'h', 'i'});
    });

    bool got_ack = false;
    ta.on_ack([&](std::uint32_t) { got_ack = true; });

    ta.send(0x02, {'h', 'i'});

    for (int i = 0; i < 50 && (ta.pending() || !got_msg); ++i) {
        ta.poll();
        tb.poll();
    }

    CHECK(got_msg);
    CHECK(got_ack);
    CHECK(!ta.pending());
}

int main() {
    test_end_to_end();
    if (g_failures == 0) {
        std::printf("test_end_to_end: ALL PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "test_end_to_end: %d FAILED\n", g_failures);
    return 1;
}
