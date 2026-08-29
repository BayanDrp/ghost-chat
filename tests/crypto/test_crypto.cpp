#include "ghostchat/crypto/crypto.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace ghostchat::crypto;

int main() {
    auto key = derive_key("mysecret");
    std::string msg = "hello ghost";

    // 1) round-trip: encrypt then decrypt recovers the message
    auto blob = encrypt(key, {msg.begin(), msg.end()});
    auto dec = decrypt(key, blob);
    assert(dec.has_value());
    assert(std::string(dec->begin(), dec->end()) == msg);

    // 2) wrong key -> cannot decrypt
    auto wrong = derive_key("other");
    assert(!decrypt(wrong, blob).has_value());

    // 3) tampered data -> cannot decrypt (integrity check)
    auto tampered = blob;
    tampered[tampered.size() - 1] ^= 0xFF;
    assert(!decrypt(key, tampered).has_value());

    // 4) empty message works
    auto e = encrypt(key, {});
    auto d2 = decrypt(key, e);
    assert(d2.has_value() && d2->empty());

    // 5) same plaintext encrypts differently (random nonce)
    auto b2 = encrypt(key, {msg.begin(), msg.end()});
    assert(b2 != blob);

    std::cout << "crypto tests passed\n";
    return 0;
}
