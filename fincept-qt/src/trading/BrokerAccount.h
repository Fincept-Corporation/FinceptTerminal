#pragma once
// BrokerAccount — represents one authenticated broker account.
// A user can have multiple accounts for the same broker type.

#include <QString>

namespace fincept::trading {

/// Connection state for a broker account (memory-only, not persisted).
enum class ConnectionState { Disconnected, Connecting, Connected, TokenExpired, Error };

/// One authenticated broker account.
/// Multiple accounts can share the same broker_id (e.g. two Zerodha accounts).
/// The IBroker* instance (from BrokerRegistry) is stateless and shared —
/// different accounts simply pass different BrokerCredentials.
struct BrokerAccount {
    QString account_id;           // UUID, permanent, generated once
    QString broker_id;            // maps to BrokerRegistry ("angelone", "zerodha", etc.)
    QString display_name;         // user label: "Zerodha - Tilak", "Angel One - Wife"
    QString paper_portfolio_id;   // linked PtPortfolio UUID for paper trading
    QString trading_mode = "paper"; // "live" or "paper" — per-account
    bool is_active = true;        // user can deactivate without deleting
    QString created_at;           // RFC3339

    // Memory-only state (not persisted in DB)
    ConnectionState state = ConnectionState::Disconnected;
    QString error_message;
};

inline const char* connection_state_str(ConnectionState s) {
    switch (s) {
    case ConnectionState::Disconnected: return "Disconnected";
    case ConnectionState::Connecting:   return "Connecting";
    case ConnectionState::Connected:    return "Connected";
    case ConnectionState::TokenExpired: return "Token Expired";
    case ConnectionState::Error:        return "Error";
    }
    return "Unknown";
}

} // namespace fincept::trading
