#pragma once

#include "services/prediction/kalshi/KalshiCredentials.h"

#include <QString>

#include <optional>

namespace fincept::services::prediction {

/// Polymarket credential set. See the plan §C.
///
/// `private_key` is the Polygon/Ethereum private key used to sign EIP-712
/// orders via py_clob_client (Phase 6). `signature_type`:
///   0 = EOA              (direct externally-owned account)
///   1 = POLY_PROXY       (Polymarket proxy wallet — default for UI users)
///   2 = POLY_GNOSIS_SAFE (Polymarket Gnosis Safe)
/// `funder_address` is optional when signature_type is POLY_PROXY; the
/// Python bridge derives it via CREATE2 if omitted.
/// `api_key`/`api_secret`/`api_passphrase` are the L2 CLOB credentials
/// derived once from `POST /auth/api-key` and cached here to avoid
/// re-deriving on every session.
struct PolymarketCredentials {
    QString private_key;
    QString funder_address;
    int signature_type = 1;

    QString api_key;
    QString api_secret;
    QString api_passphrase;

    bool is_valid() const {
        // Minimum to operate: a private key. The derived L2 creds are
        // populated lazily on first successful derivation.
        return !private_key.isEmpty();
    }
};

/// Load/save credential sets from SecureStorage (OS-backed encrypted
/// store). Keys used:
///   prediction/polymarket/credentials  → JSON blob of PolymarketCredentials
///   prediction/kalshi/credentials      → JSON blob of KalshiCredentials
class PredictionCredentialStore {
  public:
    static std::optional<PolymarketCredentials> load_polymarket();
    static bool save_polymarket(const PolymarketCredentials& creds);
    static bool clear_polymarket();

    static std::optional<kalshi_ns::KalshiCredentials> load_kalshi();
    static bool save_kalshi(const kalshi_ns::KalshiCredentials& creds);
    static bool clear_kalshi();
};

} // namespace fincept::services::prediction
