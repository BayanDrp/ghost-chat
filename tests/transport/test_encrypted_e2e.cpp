#include "ghostchat/crypto/crypto.hpp"
#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/transport.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace ghostchat;

int main() {
    // --- same key on both sides: message arrives decrypted ---
    {
        auto [ra, rb] = radio::make_loopback_pair(0x01, 0x02);
        ra->start();
        rb->start();
        transport::Transport t1(ra), t2(rb);
        t1.set_key("secret");
        t2.set_key("secret");

        bool got = false;
        std::vector<std::uint8_t> got_msg;
        t2.on_message([&](std::uint64_t, const std::vector<std::uint8_t> &p) {
            got = true;
            got_msg = p;
        });

        t1.send(0x02, {'h', 'i'});
        for (int i = 0; i < 30 && !got; ++i) {
            t1.poll();
            t2.poll();
        }
        assert(got);
        assert(std::string(got_msg.begin(), got_msg.end()) == "hi");
    }

    // --- different keys: receiver cannot decrypt -> frame dropped, no delivery ---
    {
        auto [ra, rb] = radio::make_loopback_pair(0x03, 0x04);
        ra->start();
        rb->start();
        transport::Transport a(ra), b(rb);
        a.set_key("secret");
        b.set_key("wrong");

        bool got = false;
        b.on_message([&](std::uint64_t, const std::vector<std::uint8_t> &) {
            got = true;
        });

        a.send(0x04, {'x'});
        for (int i = 0; i < 30 && !got; ++i) {
            a.poll();
            b.poll();
        }
        assert(!got);  // encrypted with a different key -> silently dropped
    }

    // --- no key at all: plain old plaintext behavior still works ---
    {
        auto [ra, rb] = radio::make_loopback_pair(0x05, 0x06);
        ra->start();
        rb->start();
        transport::Transport t1(ra), t2(rb);

        bool got = false;
        t2.on_message([&](std::uint64_t, const std::vector<std::uint8_t> &) {
            got = true;
        });
        t1.send(0x06, {'z'});
        for (int i = 0; i < 30 && !got; ++i) {
            t1.poll();
            t2.poll();
        }
        assert(got);
    }

    std::cout << "encrypted e2e tests passed\n";
    return 0;
}
