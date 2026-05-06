// Keccak256 — Ethereum-style keccak. Public-domain implementation derived
// from the reference Keccak code. Pre-NIST padding (0x01) — *not* SHA3-256.

#include "trading/exchanges/hyperliquid/Keccak256.h"

#include <cstdint>
#include <cstring>

namespace fincept::trading::hyperliquid {

namespace {

constexpr int kRateBytes = 136;   // (1600 - 256) / 8
constexpr int kStateLanes = 25;

constexpr uint64_t kRoundConstants[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL,
    0x8000000080008000ULL, 0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008AULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

constexpr int kRotationOffsets[25] = {
     0,  1, 62, 28, 27,
    36, 44,  6, 55, 20,
     3, 10, 43, 25, 39,
    41, 45, 15, 21,  8,
    18,  2, 61, 56, 14,
};

inline uint64_t rotl64(uint64_t x, int n) {
    return (x << (n & 63)) | (x >> ((64 - (n & 63)) & 63));
}

void keccak_f1600(uint64_t s[kStateLanes]) {
    for (int round = 0; round < 24; ++round) {
        // θ
        uint64_t C[5];
        for (int x = 0; x < 5; ++x) {
            C[x] = s[x] ^ s[x + 5] ^ s[x + 10] ^ s[x + 15] ^ s[x + 20];
        }
        for (int x = 0; x < 5; ++x) {
            uint64_t D = C[(x + 4) % 5] ^ rotl64(C[(x + 1) % 5], 1);
            for (int y = 0; y < 5; ++y) s[x + 5 * y] ^= D;
        }
        // ρ + π
        uint64_t B[25];
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                int idx = x + 5 * y;
                int new_x = y;
                int new_y = (2 * x + 3 * y) % 5;
                B[new_x + 5 * new_y] = rotl64(s[idx], kRotationOffsets[idx]);
            }
        }
        // χ
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                s[x + 5 * y] = B[x + 5 * y] ^
                               ((~B[((x + 1) % 5) + 5 * y]) & B[((x + 2) % 5) + 5 * y]);
            }
        }
        // ι
        s[0] ^= kRoundConstants[round];
    }
}

} // namespace

QByteArray keccak256(const QByteArray& input) {
    uint64_t state[kStateLanes];
    std::memset(state, 0, sizeof(state));

    const auto* data = reinterpret_cast<const uint8_t*>(input.constData());
    int len = input.size();

    // Absorb full blocks.
    while (len >= kRateBytes) {
        for (int i = 0; i < kRateBytes / 8; ++i) {
            uint64_t lane = 0;
            std::memcpy(&lane, data + i * 8, 8);
            state[i] ^= lane;
        }
        keccak_f1600(state);
        data += kRateBytes;
        len -= kRateBytes;
    }

    // Final block with keccak (pre-NIST) padding: 0x01 ... 0x80.
    uint8_t block[kRateBytes];
    std::memset(block, 0, kRateBytes);
    if (len > 0) std::memcpy(block, data, len);
    block[len] = 0x01;
    block[kRateBytes - 1] |= 0x80;
    for (int i = 0; i < kRateBytes / 8; ++i) {
        uint64_t lane = 0;
        std::memcpy(&lane, block + i * 8, 8);
        state[i] ^= lane;
    }
    keccak_f1600(state);

    // Squeeze 32 bytes.
    QByteArray out(32, 0);
    std::memcpy(out.data(), state, 32);
    return out;
}

} // namespace fincept::trading::hyperliquid
