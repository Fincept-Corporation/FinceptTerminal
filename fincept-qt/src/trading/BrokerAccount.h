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
    QString account_id;             // UUID, permanent, generated once
    QString broker_id;              // maps to BrokerRegistry ("angelone", "zerodha", etc.)
    QString display_name;           // user label: "Zerodha - Tilak", "Angel One - Wife"
    QString paper_portfolio_id;     // linked PtPortfolio UUID for paper trading
    QString trading_mode = "paper"; // "live" or "paper" — per-account
    bool is_active = true;          // user can deactivate without deleting
    QString created_at;             // RFC3339

    // Memory-only state (not persisted in DB)
    ConnectionState state = ConnectionState::Disconnected;
    QString error_message;
};

// ── Built-in paper account (no broker) ──────────────────────────────────────
// A local paper account that trades on delayed public prices, for users with no
// broker. AccountManager synthesises its record on get_account(); it is NEVER
// stored in broker_accounts, never returned by list_accounts() / active_accounts()
// and has_account() is false for it — so nothing that enumerates accounts (stream
// manager, session sweep, broadcast / basket / replication dialogs, dashboards)
// can see it, and DataStreamManager::start_stream() refuses it. Its broker id is
// registered with no broker, so every BrokerRegistry::get() lookup for it is null
// and UnifiedTrading refuses every broker-bound call for it outright. Its trading
// mode is always "paper". The paper portfolio behind it is the pt_portfolios row
// whose `exchange` column equals kBuiltinPaperAccountId.
inline constexpr char kBuiltinPaperAccountId[] = "builtin:paper";
inline constexpr char kBuiltinPaperBrokerId[] = "builtin-paper";

inline bool is_builtin_paper_account(const QString& account_id) {
    return account_id == QLatin1String(kBuiltinPaperAccountId);
}
inline bool is_builtin_paper_broker(const QString& broker_id) {
    return broker_id == QLatin1String(kBuiltinPaperBrokerId);
}

inline const char* connection_state_str(ConnectionState s) {
    switch (s) {
        case ConnectionState::Disconnected:
            return "Disconnected";
        case ConnectionState::Connecting:
            return "Connecting";
        case ConnectionState::Connected:
            return "Connected";
        case ConnectionState::TokenExpired:
            return "Token Expired";
        case ConnectionState::Error:
            return "Error";
    }
    return "Unknown";
}

} // namespace fincept::trading
