#include "services/wallet/Ed25519Verifier.h"

#include "core/logging/Logger.h"

#include <QByteArray>

extern "C" {
#include "ed25519.h"
}

#include <array>
#include <cstring>

namespace fincept::wallet {

namespace {

constexpr const char* kBase58Alphabet =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Build a reverse-lookup table once. -1 indicates an invalid character.
std::array<int8_t, 256> build_b58_table() {
    std::array<int8_t, 256> table{};
    table.fill(-1);
    for (int i = 0; i < 58; ++i) {
        table[static_cast<unsigned char>(kBase58Alphabet[i])] = static_cast<int8_t>(i);
    }
    return table;
}

const std::array<int8_t, 256>& b58_table() {
    static const std::array<int8_t, 256> table = build_b58_table();
    return table;
}

} // namespace

QByteArray Ed25519Verifier::decode_base58(const QString& s) noexcept {
    if (s.isEmpty()) {
        return {};
    }
    const QByteArray ascii = s.toLatin1();
    const auto& table = b58_table();

    // Count leading '1's — each represents a leading zero byte.
    int zeros = 0;
    while (zeros < ascii.size() && ascii[zeros] == '1') {
        ++zeros;
    }

    // Allocate enough room for the worst-case decode: ceil(N * log(58) / log(256)).
    std::vector<unsigned char> b256(static_cast<size_t>(ascii.size() * 733 / 1000 + 1), 0);
    int b256_len = 0;

    for (int i = zeros; i < ascii.size(); ++i) {
        const int8_t carry_seed = table[static_cast<unsigned char>(ascii[i])];
        if (carry_seed < 0) {
            return {}; // invalid character
        }
        int carry = carry_seed;
        for (int j = 0; j < b256_len || carry; ++j) {
            if (j == static_cast<int>(b256.size())) {
                return {}; // overflow guard
            }
            carry += 58 * b256[b256.size() - 1 - j];
            b256[b256.size() - 1 - j] = static_cast<unsigned char>(carry & 0xff);
            carry >>= 8;
            if (j >= b256_len) {
                b256_len = j + 1;
            }
        }
    }

    QByteArray out;
    out.reserve(zeros + b256_len);
    for (int i = 0; i < zeros; ++i) {
        out.append('\0');
    }
    out.append(reinterpret_cast<const char*>(b256.data() + b256.size() - b256_len),
               b256_len);
    return out;
}

QString Ed25519Verifier::encode_base58(const QByteArray& bytes) {
    if (bytes.isEmpty()) {
        return {};
    }
    int zeros = 0;
    while (zeros < bytes.size() && bytes[zeros] == '\0') {
        ++zeros;
    }

    std::vector<unsigned char> b58(static_cast<size_t>(bytes.size() * 138 / 100 + 1), 0);
    int b58_len = 0;

    for (int i = zeros; i < bytes.size(); ++i) {
        int carry = static_cast<unsigned char>(bytes[i]);
        for (int j = 0; j < b58_len || carry; ++j) {
            if (j == static_cast<int>(b58.size())) {
                b58.push_back(0);
            }
            carry += 256 * b58[b58.size() - 1 - j];
            b58[b58.size() - 1 - j] = static_cast<unsigned char>(carry % 58);
            carry /= 58;
            if (j >= b58_len) {
                b58_len = j + 1;
            }
        }
    }

    QString out;
    out.reserve(zeros + b58_len);
    for (int i = 0; i < zeros; ++i) {
        out.append(QLatin1Char('1'));
    }
    for (int i = static_cast<int>(b58.size()) - b58_len; i < static_cast<int>(b58.size()); ++i) {
        out.append(QLatin1Char(kBase58Alphabet[b58[i]]));
    }
    return out;
}

bool Ed25519Verifier::verify(const QString& pubkey_b58,
                             const QByteArray& message,
                             const QString& signature_b58) noexcept {
    const QByteArray pubkey = decode_base58(pubkey_b58);
    if (pubkey.size() != 32) {
        LOG_WARN("WalletVerify", "Invalid pubkey length: " + QString::number(pubkey.size()));
        return false;
    }
    const QByteArray signature = decode_base58(signature_b58);
    if (signature.size() != 64) {
        LOG_WARN("WalletVerify", "Invalid signature length: " + QString::number(signature.size()));
        return false;
    }
    if (message.isEmpty()) {
        return false;
    }

    const int ok = ::ed25519_verify(
        reinterpret_cast<const unsigned char*>(signature.constData()),
        reinterpret_cast<const unsigned char*>(message.constData()),
        static_cast<size_t>(message.size()),
        reinterpret_cast<const unsigned char*>(pubkey.constData()));
    return ok == 1;
}

} // namespace fincept::wallet
