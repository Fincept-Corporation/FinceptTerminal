#pragma once
// HyperliquidSigner — EIP-712 typed-data signer for Hyperliquid actions.
//
// CURRENT STATE (Phase 5c — partial):
//   * The structural surface (sign_action / sign_l1_action) is in place.
//   * keccak256 + ECDSA over secp256k1 are NOT yet linked.
//     `sign_action()` returns Result::err("hl_signer_not_wired") until the
//     libsecp256k1 + a keccak primitive land via FetchContent.
//
// To wire the live signer:
//   1. FetchContent libsecp256k1 (SHA-pinned).
//   2. Add a small keccak256 implementation (XKCP or sha3-1.0 — header-only).
//   3. Replace sign_action() body per the Hyperliquid EIP-712 spec:
//        struct HyperliquidSignTransaction { name: string, version: string,
//                                            chainId: uint256, agentPayloadHash: bytes32 }
//      domain: {name: "HyperliquidSignTransaction", version: "1",
//               chainId: <1 mainnet | 1337 testnet>}
//      digest = keccak256("\x19\x01" || domainSeparator || structHash)
//      sig = ECDSA_SIGN_RFC6979(privKey, digest); v += 27.
//   4. Add `tests/alpha_arena/test_hyperliquid_signer.cpp` against the known
//      vector published in the HL docs.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 5c and
// https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api

#include "core/result/Result.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace fincept::trading::hyperliquid {

/// Signed envelope handed straight to HyperliquidClient::exchange().
struct SignedAction {
    QJsonObject action;     // verbatim action object
    qint64 nonce_ms = 0;    // ms timestamp; HL also accepts a counter
    QString signature_hex;  // 0x<r><s><v> 65-byte hex
    QString signer_address; // 0x<20-byte hex> recovered from signature
};

class HyperliquidSigner {
  public:
    /// Build the signed envelope for an HL `exchange` action.
    /// `agent_priv_key_hex` MUST be a 0x-prefixed 32-byte hex string read from
    /// SecureStorage; it never leaves the in-process address space.
    static Result<SignedAction> sign_action(const QJsonObject& action,
                                             const QString& agent_priv_key_hex,
                                             bool is_testnet);

    /// keccak256 of the canonical JSON of `action` (sorted keys, no whitespace).
    /// Public for testability — known-vector tests assert this output.
    static QByteArray action_struct_hash(const QJsonObject& action);

    /// Returns true once libsecp256k1 + keccak are linked. Until then, the
    /// signer is a stub and live mode falls through to "signer_not_wired".
    static bool is_wired();
};

} // namespace fincept::trading::hyperliquid
