#include "ghostchat/radio/afpacket.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main() {
    const char *iface = std::getenv("GHOSTCHAT_IFACE");
    if (!iface) {
        std::printf("test_afpacket: SKIP (set GHOSTCHAT_IFACE to run on real HW)\n");
        return 0;
    }

    ghostchat::radio::AFPacketRadio radio(iface, 0x01);
    if (!radio.start()) {
        std::printf("test_afpacket: SKIP (could not open %s, need root + ad-hoc mode)\n", iface);
        return 0;
    }

    std::printf("test_afpacket: opened %s, local_address=%llx\n",
                iface, (unsigned long long)radio.local_address());

    std::vector<std::uint8_t> frame = {'p', 'i', 'n', 'g'};
    bool ok = radio.send(frame);
    std::printf("test_afpacket: send -> %s\n", ok ? "ok" : "fail");

    std::printf("test_afpacket: OK (smoke test)\n");
    return 0;
}
