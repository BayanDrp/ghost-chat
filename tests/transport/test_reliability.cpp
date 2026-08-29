#include "ghostchat/transport/reliability.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

using namespace ghostchat::transport;
using namespace std::chrono_literals;

int main() {
    std::vector<std::uint8_t> raw = {'a', 'b', 'c'};

    // add -> pending, not yet due
    ReliabilityTracker tr;
    tr.add(1, raw, 3, 100ms);
    assert(tr.pending());
    auto t0 = std::chrono::steady_clock::now();
    assert(tr.resend_due(t0).empty());  // next_attempt is t0 + 100ms

    // after one timeout: due once, decrements tries (3 -> 2)
    auto t1 = t0 + 200ms;
    auto r1 = tr.resend_due(t1);
    assert(r1.size() == 1);
    assert(r1[0] == raw);
    assert(tr.resend_due(t1).empty());  // same time, next attempt moved forward

    // subsequent timeouts keep returning the frame until tries run out
    auto t2 = t1 + 200ms;
    assert(tr.resend_due(t2).size() == 1);  // tries 2 -> 1
    auto t3 = t2 + 200ms;
    assert(tr.resend_due(t3).size() == 1);  // tries 1 -> 0

    // next due call: tries exhausted -> frame dropped, no longer pending
    auto t4 = t3 + 200ms;
    assert(tr.resend_due(t4).empty());
    assert(!tr.pending());

    // ack removes a tracked frame; unknown seq returns false
    ReliabilityTracker tr2;
    tr2.add(5, raw, 3, 100ms);
    assert(tr2.ack(5));
    assert(!tr2.pending());
    assert(!tr2.ack(99));

    std::cout << "reliability tests passed\n";
    return 0;
}
