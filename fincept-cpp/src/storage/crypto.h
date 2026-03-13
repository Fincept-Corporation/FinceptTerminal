#pragma once
// AES-256-GCM encryption for credential storage
// Matches the Tauri/Rust backend's encryption approach

#include <string>
#include <vector>
#include <optional>

namespace fincept::storage {

class Crypto {
public:
    // Encrypt plaintext with AES-256-GCM
    // Returns base64-encoded ciphertext (nonce + ciphertext + tag)
    static std::optional<std::string> encrypt(const std::string& plaintext, const std::string& key);

    // Decrypt base64-encoded ciphertext with AES-256-GCM
    static std::optional<std::string> decrypt(const std::string& ciphertext_b64, const std::string& key);

    // Derive a 256-bit key from a passphrase using PBKDF2
    static std::string derive_key(const std::string& passphrase, const std::string& salt = "fincept_terminal_v3");

    // Generate cryptographically secure random bytes
    static std::vector<unsigned char> random_bytes(size_t count);

    // Base64 encode/decode
    static std::string base64_encode(const std::vector<unsigned char>& data);
    static std::vector<unsigned char> base64_decode(const std::string& encoded);
};

} // namespace fincept::storage
