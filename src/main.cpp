#include "ghostchat/radio/afpacket.hpp"
#include "ghostchat/radio/ieee802154.hpp"
#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/transport.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <unistd.h>
#include <string>
#include <vector>

using namespace ghostchat;

int main(int argc, char **argv) {
    std::uint64_t node_id = 0x01;
    std::string iface = std::getenv("GHOSTCHAT_IFACE") ? std::getenv("GHOSTCHAT_IFACE") : "";
    std::string radio_type = "afpacket";
    if (argc > 1) node_id = std::strtoull(argv[1], nullptr, 16);
    if (argc > 2) iface = argv[2];
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-r" && i + 1 < argc) radio_type = argv[++i];
    }

    radio::RadioPtr radio;
    transport::Transport *peer_transport = nullptr;
    std::unique_ptr<transport::Transport> peer_holder;

    if (!iface.empty()) {
        radio::Radio *r = nullptr;
        if (radio_type == "ieee802154")
            r = new radio::Ieee802154Radio(iface, node_id);
        else
            r = new radio::AFPacketRadio(iface, node_id);
        if (!r->start()) {
            std::cerr << "failed to start radio on " << iface << "\n";
            return 1;
        }
        radio.reset(r);
        std::cout << "ghostchat node " << std::hex << node_id << " on " << iface
                  << " (" << radio_type << ")\n";
    } else {
        std::uint64_t peer_id = (node_id == 0x01) ? 0x02 : 0x01;
        auto [ra, rb] = radio::make_loopback_pair(node_id, peer_id);
        ra->start();
        rb->start();
        radio = ra;

        peer_holder = std::make_unique<transport::Transport>(rb);
        peer_transport = peer_holder.get();
        peer_transport->on_message([](std::uint64_t from,
                                       const std::vector<std::uint8_t> &p) {
            std::cout << "\n[peer " << std::hex << from << "] "
                      << std::string(p.begin(), p.end()) << "\n> ";
            std::cout.flush();
        });
        std::cout << "ghostchat node " << std::hex << node_id
                  << " (loopback self-mode, peer=" << peer_id << ")\n";
    }

    std::string key;
    if (auto *e = std::getenv("GHOSTCHAT_KEY")) key = e;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "-k" && i + 1 < argc) key = argv[++i];

    transport::Transport t(radio);
    if (!key.empty()) {
        t.set_key(key);
        if (peer_transport) peer_transport->set_key(key);
        std::cout << "encryption: ON\n";
    }
    auto render = [](std::uint64_t from, const std::vector<std::uint8_t> &p) {
        std::cout << "\n[" << std::hex << from << "] "
                  << std::string(p.begin(), p.end()) << "\n> ";
        std::cout.flush();
    };
    t.on_message(render);
    t.on_peer([](std::uint64_t p) {
        std::cout << "\n[discovered peer " << std::hex << p << "]\n> ";
        std::cout.flush();
    });
    t.on_ack([](std::uint32_t seq) {
        std::cout << "\n[ack #" << seq << "]\n> ";
        std::cout.flush();
    });

    std::cout << "commands: d=discover  m <peer> <msg>  l=list  q=quit\n> ";
    std::cout.flush();

    bool running = true;
    auto exec = [&](const std::string &line) {
        if (line.empty()) return;
        char cmd = line[0];
        if (cmd == 'q') {
            running = false;
        } else if (cmd == 'd') {
            t.discover();
            std::cout << "discovery sent\n> ";
        } else if (cmd == 'l') {
            auto ps = t.peers();
            std::cout << "peers:";
            for (auto p : ps) std::cout << " " << std::hex << p;
            std::cout << "\n> ";
        } else if (cmd == 'm') {
            std::istringstream iss(line);
            std::string _;
            std::uint64_t dst = 0;
            std::string msg;
            iss >> _ >> std::hex >> dst;
            std::getline(iss, msg);
            if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
            t.send(dst, std::vector<std::uint8_t>(msg.begin(), msg.end()));
            std::cout << "sent to " << std::hex << dst << "\n> ";
        } else {
            std::cout << "unknown command (d/m/l/q)\n> ";
        }
        std::cout.flush();
    };

    std::string pending;
    char rbuf[256];
    while (running) {
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        int pr = poll(&pfd, 1, 50);

        if (pr > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(STDIN_FILENO, rbuf, sizeof(rbuf) - 1);
            if (n <= 0) break;  // EOF / ctrl-D
            rbuf[n] = '\0';
            pending += rbuf;
            size_t pos;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                pending.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                exec(line);
                t.poll();
                if (peer_transport) peer_transport->poll();
                if (!running) break;
            }
        }

        t.poll();
        if (peer_transport) peer_transport->poll();
    }

    radio->stop();
    return 0;
}
