#pragma once

#include <QByteArray>
#include <QString>

namespace fincept::wallet {

/// Verifies an Ed25519 signature against a message and a Solana public key.
///
/// All inputs are base58-encoded as Solana wallets emit them. The implementation
/// decodes base58 and delegates to the orlp/ed25519 reference implementation.
///
/// This is consulted **only** during the wallet-connect handshake to prove that
/// the user controls the private key behind the claimed public key. We never
/// store, transmit, or accept private keys.
class Ed25519Verifier {
  public:
    /// Returns true iff `signature_b58` is a valid Ed25519 signature of
    /// `message` produced by the private key paired with `pubkey_b58`.
    /// Returns false on any decode error or verification failure — never throws.
    static bool verify(const QString& pubkey_b58,
                       const QByteArray& message,
                       const QString& signature_b58) noexcept;

    /// Decodes a base58 string. Returns empty QByteArray on invalid input.
    /// Exposed for tests and for SolanaRpcClient (mint addresses are b58).
    static QByteArray decode_base58(const QString& s) noexcept;

    /// Encodes a byte array as base58. Used to log/display pubkeys.
    static QString encode_base58(const QByteArray& bytes);
};

} // namespace fincept::wallet
