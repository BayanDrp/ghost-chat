#include "ghostchat/protocol/codec.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace gc = ghostchat::protocol;

static int g_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "FAIL %s:%d -> %s\n", __FILE__, __LINE__, #cond);                 \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (0)

static void test_serialize() {
    std::vector<std::uint8_t> payload = {'h', 'i', '!'};

    gc::Frame frame =
        gc::create_frame(gc::FrameType::Message, 0x11, 0x22, 7, gc::kFlagAckRequested, payload);

    std::vector<std::uint8_t> data = gc::serialize(frame);

    CHECK(data.size() == gc::kWireHeaderSize + payload.size() + 4);
}

static void test_round_trip() {
    std::vector<std::uint8_t> payload = {'h', 'i', '!'};

    gc::Frame original =
        gc::create_frame(gc::FrameType::Message, 0x11, 0x22, 7, gc::kFlagAckRequested, payload);

    std::vector<std::uint8_t> data = gc::serialize(original);

    auto parsed = gc::parse(data);

    CHECK(parsed.has_value());

    if (!parsed)
        return;

    CHECK(parsed->header.magic == original.header.magic);
    CHECK(parsed->header.version == original.header.version);
    CHECK(parsed->header.type == original.header.type);
    CHECK(parsed->header.flags == original.header.flags);

    CHECK(parsed->header.sender == original.header.sender);
    CHECK(parsed->header.receiver == original.header.receiver);

    CHECK(parsed->header.sequence == original.header.sequence);
    CHECK(parsed->header.payloadSize == original.header.payloadSize);

    CHECK(parsed->payload == original.payload);
    CHECK(parsed->checksum != 0);
}

static void test_bad_magic() {
    std::vector<std::uint8_t> payload = {'x'};

    gc::Frame frame = gc::create_frame(gc::FrameType::Message, 1, 2, 1, 0, payload);

    auto data = gc::serialize(frame);

    data[0] ^= 0xFF;

    auto parsed = gc::parse(data);

    CHECK(!parsed.has_value());
}

static void test_bad_checksum() {
    std::vector<std::uint8_t> payload = {'x'};

    gc::Frame frame = gc::create_frame(gc::FrameType::Message, 1, 2, 1, 0, payload);

    auto data = gc::serialize(frame);

    data[27] ^= 0xFF;

    auto parsed = gc::parse(data);

    CHECK(!parsed.has_value());
}

static void test_empty_payload() {
    gc::Frame frame = gc::create_frame(gc::FrameType::Ack, 1, 2, 99, 0, {});

    auto data = gc::serialize(frame);
    auto parsed = gc::parse(data);

    CHECK(parsed.has_value());

    if (!parsed)
        return;

    CHECK(parsed->payload.empty());
    CHECK(parsed->header.payloadSize == 0);
}

int main() {
    test_serialize();
    test_round_trip();
    test_bad_magic();
    test_bad_checksum();
    test_empty_payload();

    if (g_failures == 0) {
        std::printf("test_codec: ALL PASSED\n");
        return 0;
    }

    std::fprintf(stderr, "test_codec: %d FAILED\n", g_failures);
    return 1;
}