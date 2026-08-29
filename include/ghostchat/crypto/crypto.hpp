#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ghostchat::crypto {

// A 32-byte AES-256 key. The "lock" needs exactly this size.
using Key = std::array<std::uint8_t, 32>;

// Turn a human passphrase into a Key (simple KDF: SHA-256 of the passphrase).
// Not a slow/salted password hash -- fine for a learning MVP, can be
// strengthened later without changing the rest of the design.
Key derive_key(const std::string &passphrase);

// AES-256-GCM encrypt. Output layout: nonce(12) || ciphertext || tag(16).
// A fresh random nonce is used every call, so encrypting the same text twice
// produces different output (this is what stops trivial pattern matching).
std::vector<std::uint8_t> encrypt(const Key &key,
                                  const std::vector<std::uint8_t> &plaintext);

// AES-256-GCM decrypt. Returns nullopt if the key is wrong or the data was
// tampered with (the GCM tag check fails).
std::optional<std::vector<std::uint8_t>> decrypt(const Key &key,
                                                 const std::vector<std::uint8_t> &blob);

} // namespace ghostchat::crypto
