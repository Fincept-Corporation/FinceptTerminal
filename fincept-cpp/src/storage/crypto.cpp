#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <cstring>

namespace fincept::storage {

static constexpr int AES_KEY_SIZE = 32;  // 256 bits
static constexpr int GCM_NONCE_SIZE = 12;
static constexpr int GCM_TAG_SIZE = 16;

std::vector<unsigned char> Crypto::random_bytes(size_t count) {
    std::vector<unsigned char> buf(count);
    RAND_bytes(buf.data(), (int)count);
    return buf;
}

std::string Crypto::base64_encode(const std::vector<unsigned char>& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    BIO_write(bio, data.data(), (int)data.size());
    BIO_flush(bio);

    BUF_MEM* buf_mem = nullptr;
    BIO_get_mem_ptr(bio, &buf_mem);

    std::string result(buf_mem->data, buf_mem->length);
    BIO_free_all(bio);
    return result;
}

std::vector<unsigned char> Crypto::base64_decode(const std::string& encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.data(), (int)encoded.size());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    std::vector<unsigned char> result(encoded.size());
    int len = BIO_read(bio, result.data(), (int)result.size());
    result.resize(len > 0 ? len : 0);

    BIO_free_all(bio);
    return result;
}

std::string Crypto::derive_key(const std::string& passphrase, const std::string& salt) {
    std::vector<unsigned char> key(AES_KEY_SIZE);
    PKCS5_PBKDF2_HMAC(
        passphrase.c_str(), (int)passphrase.size(),
        reinterpret_cast<const unsigned char*>(salt.c_str()), (int)salt.size(),
        100000, EVP_sha256(),
        AES_KEY_SIZE, key.data()
    );
    return std::string(reinterpret_cast<char*>(key.data()), AES_KEY_SIZE);
}

std::optional<std::string> Crypto::encrypt(const std::string& plaintext, const std::string& key) {
    if (key.size() < AES_KEY_SIZE) return std::nullopt;

    auto nonce = random_bytes(GCM_NONCE_SIZE);
    std::vector<unsigned char> ciphertext(plaintext.size());
    std::vector<unsigned char> tag(GCM_TAG_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;

    int len = 0;
    bool ok = true;

    ok = ok && EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_SIZE, nullptr);
    ok = ok && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                reinterpret_cast<const unsigned char*>(key.data()), nonce.data());
    ok = ok && EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                reinterpret_cast<const unsigned char*>(plaintext.data()), (int)plaintext.size());
    int ciphertext_len = len;
    ok = ok && EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag.data());

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return std::nullopt;

    ciphertext.resize(ciphertext_len);

    // Pack: nonce || ciphertext || tag
    std::vector<unsigned char> packed;
    packed.reserve(GCM_NONCE_SIZE + ciphertext_len + GCM_TAG_SIZE);
    packed.insert(packed.end(), nonce.begin(), nonce.end());
    packed.insert(packed.end(), ciphertext.begin(), ciphertext.end());
    packed.insert(packed.end(), tag.begin(), tag.end());

    return base64_encode(packed);
}

std::optional<std::string> Crypto::decrypt(const std::string& ciphertext_b64, const std::string& key) {
    if (key.size() < AES_KEY_SIZE) return std::nullopt;

    auto packed = base64_decode(ciphertext_b64);
    if (packed.size() < GCM_NONCE_SIZE + GCM_TAG_SIZE) return std::nullopt;

    // Unpack: nonce || ciphertext || tag
    auto nonce_begin = packed.begin();
    auto ct_begin = nonce_begin + GCM_NONCE_SIZE;
    auto tag_begin = packed.end() - GCM_TAG_SIZE;
    int ct_len = (int)(tag_begin - ct_begin);

    if (ct_len < 0) return std::nullopt;

    std::vector<unsigned char> plaintext(ct_len);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;

    int len = 0;
    bool ok = true;

    ok = ok && EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_SIZE, nullptr);
    ok = ok && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                reinterpret_cast<const unsigned char*>(key.data()), &(*nonce_begin));
    ok = ok && EVP_DecryptUpdate(ctx, plaintext.data(), &len, &(*ct_begin), ct_len);
    int plaintext_len = len;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, (void*)&(*tag_begin));

    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    if (!ok || ret <= 0) return std::nullopt;

    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);
}

} // namespace fincept::storage
