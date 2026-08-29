#include "ghostchat/crypto/crypto.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace ghostchat::crypto {

namespace {
constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kTagSize = 16;
}

Key derive_key(const std::string &passphrase) {
    Key key{};
    unsigned int len = 0;
    EVP_Digest(passphrase.data(), passphrase.size(), key.data(), &len,
               EVP_sha256(), nullptr);
    return key;
}

std::vector<std::uint8_t> encrypt(const Key &key,
                                  const std::vector<std::uint8_t> &plaintext) {
    std::vector<std::uint8_t> out;
    out.resize(kNonceSize + plaintext.size() + kTagSize);

    unsigned char *nonce = out.data();
    RAND_bytes(nonce, static_cast<int>(kNonceSize));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                        static_cast<int>(kNonceSize), nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce);

    int len = 0;
    unsigned char *ct = out.data() + kNonceSize;
    EVP_EncryptUpdate(ctx, ct, &len, plaintext.data(),
                      static_cast<int>(plaintext.size()));
    int ct_len = len;
    EVP_EncryptFinal_ex(ctx, ct + ct_len, &len);
    ct_len += len;

    unsigned char *tag = out.data() + kNonceSize + ct_len;
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagSize),
                        tag);

    EVP_CIPHER_CTX_free(ctx);
    out.resize(kNonceSize + static_cast<std::size_t>(ct_len) + kTagSize);
    return out;
}

std::optional<std::vector<std::uint8_t>> decrypt(const Key &key,
                                                 const std::vector<std::uint8_t> &blob) {
    if (blob.size() < kNonceSize + kTagSize) return std::nullopt;
    const unsigned char *nonce = blob.data();
    std::size_t ct_len = blob.size() - kNonceSize - kTagSize;
    const unsigned char *ct = blob.data() + kNonceSize;
    const unsigned char *tag = blob.data() + kNonceSize + ct_len;

    std::vector<std::uint8_t> pt;
    pt.resize(ct_len);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                        static_cast<int>(kNonceSize), nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce);

    int len = 0;
    EVP_DecryptUpdate(ctx, pt.data(), &len, ct, static_cast<int>(ct_len));
    int pt_len = len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagSize),
                        const_cast<unsigned char *>(tag));
    int rv = EVP_DecryptFinal_ex(ctx, pt.data() + pt_len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (rv <= 0) return std::nullopt;  // tag mismatch => wrong key / tampered
    pt_len += len;
    pt.resize(static_cast<std::size_t>(pt_len));
    return pt;
}

} // namespace ghostchat::crypto
