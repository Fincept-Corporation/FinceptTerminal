#pragma once

#include <QString>

namespace fincept::services::prediction::kalshi_ns {

/// Kalshi API credentials. Kalshi authenticates with RSA-PSS signatures
/// (private key) + API key ID. Stored encrypted in SecureStorage under
/// the key "prediction/kalshi/credentials" once Phase 5 wires the account
/// dialog.
struct KalshiCredentials {
    QString api_key_id;      // UUID-style string from the Kalshi dashboard
    QString private_key_pem; // PEM-encoded RSA private key contents
    bool use_demo = false;   // target demo-api.kalshi.co vs api.elections.kalshi.com

    bool is_valid() const {
        return !api_key_id.isEmpty() && !private_key_pem.isEmpty();
    }
};

} // namespace fincept::services::prediction::kalshi_ns
