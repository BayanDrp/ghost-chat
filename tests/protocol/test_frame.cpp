#include "ghostchat/protocol/frame.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

namespace gc = ghostchat::protocol;

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "FAIL %s:%d  -> %s\n", __FILE__, __LINE__,  \
                         #cond);                                             \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void test_constants() {
    CHECK(gc::kMagic == 0x6767);
    CHECK(gc::kVersion == 1);
    CHECK(gc::kMaxPayloadSize == 2304);
    CHECK(gc::kBroadcastAddress == 0xFFFFFFFFFFFFFFFF);
    CHECK(gc::kHeaderSize == sizeof(gc::FrameHeader));
    CHECK(gc::kMaxFrameSize == gc::kWireHeaderSize + gc::kMaxPayloadSize + gc::kChecksumSize);
}

static void test_create_basic() {
    std::vector<std::uint8_t> payload = {'h', 'i', '!'};
    gc::Frame f = gc::create_frame(gc::FrameType::Message, 0x11, 0x22, 7,
                                  gc::kFlagAckRequested, payload);

    CHECK(f.header.magic == gc::kMagic);
    CHECK(f.header.version == gc::kVersion);
    CHECK(f.header.type == gc::FrameType::Message);
    CHECK(f.header.flags == gc::kFlagAckRequested);
    CHECK(f.header.sender == 0x11);
    CHECK(f.header.receiver == 0x22);
    CHECK(f.header.sequence == 7);
    CHECK(f.header.payloadSize == 3);
    CHECK(f.payload == payload);
    CHECK(f.checksum == 0);
}

static void test_create_broadcast() {
    std::vector<std::uint8_t> payload = {0xAA};
    gc::Frame f = gc::create_frame(gc::FrameType::Discovery, 0x99,
                                  gc::kBroadcastAddress, 1, 0, payload);

    CHECK(f.header.receiver == gc::kBroadcastAddress);
    CHECK(f.header.type == gc::FrameType::Discovery);
    CHECK(f.header.payloadSize == 1);
}

static void test_flag_combos() {
    std::uint8_t flags = gc::kFlagEncrypted | gc::kFlagRelayed;
    gc::Frame f = gc::create_frame(gc::FrameType::Message, 1, 2, 0, flags, {});

    CHECK(f.header.flags == (gc::kFlagEncrypted | gc::kFlagRelayed));
    CHECK((f.header.flags & gc::kFlagEncrypted) != 0);
    CHECK((f.header.flags & gc::kFlagRelayed) != 0);
    CHECK((f.header.flags & gc::kFlagAckRequested) == 0);
}

static void test_empty_payload() {
    gc::Frame f = gc::create_frame(gc::FrameType::Ack, 5, 6, 99, 0, {});
    CHECK(f.header.payloadSize == 0);
    CHECK(f.payload.empty());
}

int main() {
    test_constants();
    test_create_basic();
    test_create_broadcast();
    test_flag_combos();
    test_empty_payload();

    if (g_failures == 0) {
        std::printf("test_frame: ALL PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "test_frame: %d FAILED\n", g_failures);
    return 1;
}
