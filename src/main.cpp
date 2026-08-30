#include "ghostchat/radio/afpacket.hpp"
#include "ghostchat/radio/ieee802154.hpp"
#include "ghostchat/radio/radio.hpp"
#include "ghostchat/transport/transport.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <signal.h>
#include <string>
#include <unistd.h>
#include <vector>

using namespace ghostchat;

static std::atomic<bool> g_running{true};
static transport::Transport *g_transport = nullptr;
static radio::Radio *g_radio = nullptr;

void handle_signal(int) {
    g_running = false;
}

struct Color {
    static const char *RESET;
    static const char *BOLD;
    static const char *DIM;
    static const char *RED;
    static const char *GREEN;
    static const char *YELLOW;
    static const char *BLUE;
    static const char *MAGENTA;
    static const char *CYAN;
    static const char *GRAY;
};
const char *Color::RESET  = "\033[0m";
const char *Color::BOLD   = "\033[1m";
const char *Color::DIM    = "\033[2m";
const char *Color::RED    = "\033[31m";
const char *Color::GREEN  = "\033[32m";
const char *Color::YELLOW = "\033[33m";
const char *Color::BLUE   = "\033[34m";
const char *Color::MAGENTA= "\033[35m";
const char *Color::CYAN   = "\033[36m";
const char *Color::GRAY   = "\033[90m";

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    std::ostringstream oss;
    oss << buf << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string to_hex(std::uint64_t v) {
    std::ostringstream oss;
    oss << std::hex << v;
    return oss.str();
}

void print_status_line(std::uint64_t node_id, const std::string &iface,
                       const std::string &radio_type, bool encrypted) {
    std::cout << Color::DIM << "┌─ " << Color::BOLD << "ghostchat"
              << Color::DIM << " [" << to_hex(node_id) << "]";
    if (!iface.empty()) {
        std::cout << " on " << iface << " (" << radio_type << ")";
    } else {
        std::cout << " (loopback)";
    }
    if (encrypted) std::cout << " " << Color::GREEN << "🔒 encrypted" << Color::DIM;
    std::cout << " ─┐" << Color::RESET << "\n";
}

void print_help() {
    std::cout << R"(
ghostchat - decentralized off-grid chat

Usage:
  ghostchat [node_id] [iface] [options]

Arguments:
  node_id       Your node ID in hex (default: 0x01)
  iface         Network interface (empty = loopback self-test)

Options:
  -r, --radio TYPE    Radio backend: afpacket | ieee802154 (default: afpacket)
  -k, --key PASSPHRASE  Encryption passphrase (or GHOSTCHAT_KEY env)
  -h, --help           Show this help

Commands (at runtime):
  d                       Discover peers
  l                       List known peers
  m <peer> <message>      Send message to peer (hex ID)
  q                       Quit
  h                       Show this help

Environment:
  GHOSTCHAT_IFACE    Default interface
  GHOSTCHAT_KEY      Default encryption key

Examples:
  ghostchat 0x01 wlan0 -k secret
  ghostchat 0x02 wpan0 -r ieee802154
  ghostchat 0x01           # loopback self-test (peer=0x02)
)";
}

std::vector<std::string> split_args(const std::string &line) {
    std::vector<std::string> args;
    std::string cur;
    bool in_quote = false;
    char quote_char = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if ((c == '"' || c == '\'') && !in_quote) {
            in_quote = true;
            quote_char = c;
        } else if (c == quote_char && in_quote) {
            in_quote = false;
            quote_char = 0;
        } else if (c == ' ' && !in_quote) {
            if (!cur.empty()) {
                args.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) args.push_back(cur);
    return args;
}

std::string read_line_with_completion(const std::vector<std::string> &peers) {
    // Simple line editor with tab completion for peers
    std::string line;
    char c;
    while (true) {
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 50) <= 0) return "";
        if (read(STDIN_FILENO, &c, 1) != 1) return "";
        if (c == '\n' || c == '\r') {
            std::cout << "\n";
            return line;
        }
        if (c == 127 || c == 8) { // backspace
            if (!line.empty()) {
                line.pop_back();
                std::cout << "\b \b";
                std::cout.flush();
            }
        } else if (c == '\t') { // tab completion
            std::vector<std::string> matches;
            for (const auto &p : peers) {
                if (p.size() >= line.size() && p.compare(0, line.size(), line) == 0) {
                    matches.push_back(p);
                }
            }
            if (matches.size() == 1) {
                std::string rest = matches[0].substr(line.size());
                line += rest;
                std::cout << rest;
                std::cout.flush();
            } else if (matches.size() > 1) {
                std::cout << "\n";
                for (const auto &m : matches) std::cout << m << " ";
                std::cout << "\n> " << line;
                std::cout.flush();
            }
        } else if (c >= 32 && c < 127) {
            line += c;
            std::cout << c;
            std::cout.flush();
        }
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::uint64_t node_id = 0x01;
    std::string iface = std::getenv("GHOSTCHAT_IFACE") ? std::getenv("GHOSTCHAT_IFACE") : "";
    std::string radio_type = "afpacket";
    std::string key;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help();
            return 0;
        } else if (arg == "-r" || arg == "--radio") {
            if (i + 1 < argc) radio_type = argv[++i];
        } else if (arg == "-k" || arg == "--key") {
            if (i + 1 < argc) key = argv[++i];
        } else if (arg[0] != '-') {
            if (node_id == 0x01 && (i == 1 || argv[i-1][0] != '-'))
                node_id = std::strtoull(arg.c_str(), nullptr, 16);
            else if (iface.empty())
                iface = arg;
        }
    }
    if (key.empty() && std::getenv("GHOSTCHAT_KEY")) key = std::getenv("GHOSTCHAT_KEY");

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
            std::cerr << Color::RED << "failed to start radio on " << iface << Color::RESET << "\n";
            return 1;
        }
        radio.reset(r);
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
            std::cout << "\n" << Color::CYAN << "[" << timestamp() << "]"
                      << Color::RESET << " [" << Color::MAGENTA << to_hex(from) << Color::RESET << "] "
                      << std::string(p.begin(), p.end()) << "\n> ";
            std::cout.flush();
        });
    }

    g_transport = new transport::Transport(radio);
    g_radio = radio.get();
    transport::Transport &t = *g_transport;

    if (!key.empty()) {
        t.set_key(key);
        if (peer_transport) peer_transport->set_key(key);
    }

    t.on_message([&](std::uint64_t from, const std::vector<std::uint8_t> &p) {
        std::cout << "\n" << Color::CYAN << "[" << timestamp() << "]"
                  << Color::RESET << " [" << Color::MAGENTA << to_hex(from) << Color::RESET << "] "
                  << std::string(p.begin(), p.end()) << "\n> ";
        std::cout.flush();
    });
    t.on_peer([&](std::uint64_t p) {
        std::cout << "\n" << Color::GREEN << "[" << timestamp() << "]"
                  << Color::RESET << " discovered peer " << Color::BOLD << to_hex(p) << Color::RESET << "\n> ";
        std::cout.flush();
    });
    t.on_ack([](std::uint32_t seq) {
        std::cout << "\n" << Color::YELLOW << "[" << timestamp() << "]"
                  << Color::RESET << " ACK #" << seq << "\n> ";
        std::cout.flush();
    });

    print_status_line(node_id, iface, radio_type, !key.empty());
    std::cout << "commands: " << Color::BOLD << "d" << Color::RESET << "=discover  "
              << Color::BOLD << "m" << Color::RESET << " <peer> <msg>  "
              << Color::BOLD << "l" << Color::RESET << "=list  "
              << Color::BOLD << "q" << Color::RESET << "=quit  "
              << Color::BOLD << "h" << Color::RESET << "=help\n> ";
    std::cout.flush();

    std::string pending;
    char rbuf[256];

    while (g_running) {
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        int pr = poll(&pfd, 1, 50);

        if (pr > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(STDIN_FILENO, rbuf, sizeof(rbuf) - 1);
            if (n <= 0) break;
            rbuf[n] = '\0';
            pending += rbuf;
            size_t pos;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                pending.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (!line.empty()) {
                    auto args = split_args(line);
                    if (args.empty()) continue;
                    std::string cmd = args[0];
                    if (cmd == "q") {
                        g_running = false;
                    } else if (cmd == "d") {
                        t.discover();
                        std::cout << "discovery sent\n> ";
                    } else if (cmd == "l") {
                        auto ps = t.peers();
                        std::cout << "peers:";
                        for (auto p : ps) std::cout << " " << Color::BOLD << to_hex(p) << Color::RESET;
                        std::cout << "\n> ";
                    } else if (cmd == "m") {
                        if (args.size() < 3) {
                            std::cout << "usage: m <peer_hex> <message>\n> ";
                        } else {
                            std::uint64_t dst = std::strtoull(args[1].c_str(), nullptr, 16);
                            std::string msg;
                            for (size_t i = 2; i < args.size(); ++i) {
                                if (i > 2) msg += ' ';
                                msg += args[i];
                            }
                            t.send(dst, std::vector<std::uint8_t>(msg.begin(), msg.end()));
                            std::cout << "sent to " << Color::BOLD << to_hex(dst) << Color::RESET << "\n> ";
                        }
                    } else if (cmd == "h") {
                        print_help();
                        std::cout << "> ";
                    } else {
                        std::cout << Color::RED << "unknown command" << Color::RESET << " (d/m/l/q/h)\n> ";
                    }
                } else {
                    std::cout << "> ";
                }
                std::cout.flush();
            }
        }

        t.poll();
        if (peer_transport) peer_transport->poll();
    }

    std::cout << Color::DIM << "\nshutting down..." << Color::RESET << "\n";
    if (g_transport) delete g_transport;
    if (g_radio) g_radio->stop();
    return 0;
}