#pragma once
// Keccak256 — minimal Ethereum-style keccak256 (NOT NIST SHA3-256).
//
// Public-domain implementation derived from the Keccak Team reference.
// Differs from SHA3-256 only in the multi-rate padding rule (0x01 instead
// of 0x06). Used by Hyperliquid's EIP-712 signer.
//
// 32-byte digest output for any input.

#include <QByteArray>

#include <cstdint>

namespace fincept::trading::hyperliquid {

QByteArray keccak256(const QByteArray& input);

} // namespace fincept::trading::hyperliquid
