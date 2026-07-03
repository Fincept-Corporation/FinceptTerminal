#pragma once
// HyperliquidSigner — EIP-712 typed-data signer for Hyperliquid actions.
//
// STATE: the signing primitives (keccak256 + secp256k1 ECDSA + EIP-712 typed
// data) are linked, but the live ORDER path is NOT production-ready — is_wired()
// returns false so the Alpha Arena live gate stays closed (no real wallet key is
// captured) until order placement + signature vectors are verified.
//
// EIP-712 shape (reference):
//        struct HyperliquidSignTransaction { name: string, version: string,
//                                            chainId: uint256, agentPayloadHash: bytes32 }
//      domain: {name: "HyperliquidSignTransaction", version: "1",
//               chainId: <1 mainnet | 1337 testnet>}
//      digest = keccak256("\x19\x01" || domainSeparator || structHash)
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

    /// True now that keccak + secp256k1 ECDSA are linked (always true today).
    /// Retained as a guard hook so callers can branch on signer availability.
    static bool is_wired();
};

} // namespace fincept::trading::hyperliquid
