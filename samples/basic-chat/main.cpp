#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/transport.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

using namespace ghostchat;

int main() {
    auto [ra, rb] = radio::make_loopback_pair(0x01, 0x02);
    ra->start();
    rb->start();

    transport::Transport ta(ra);
    transport::Transport tb(rb);

    tb.on_message([](std::uint64_t from, const std::vector<std::uint8_t> &p) {
        std::printf("B received from %llx: %.*s\n", (unsigned long long)from,
                    (int)p.size(), (const char *)p.data());
    });
    ta.on_ack([](std::uint32_t seq) { std::printf("A got ACK #%u\n", seq); });

    ta.send(0x02, {'h', 'e', 'l', 'l', 'o'});

    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < end) {
        ta.poll();
        tb.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::printf("A still waiting for ACK: %s\n", ta.pending() ? "yes" : "no");
    return 0;
}
