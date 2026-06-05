#pragma once
// AccountManager — manages multiple broker accounts for multi-account trading.
// Replaces UnifiedTrading's single-session model for account management.
// Credentials are stored in SecureStorage with account-scoped keys.
// Account metadata is persisted in the broker_accounts SQLite table.

#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

class QTimer;

namespace fincept::trading {

class IBroker;

class AccountManager : public QObject {
    Q_OBJECT
  public:
    static AccountManager& instance();

    // --- Account CRUD ---
    BrokerAccount add_account(const QString& broker_id, const QString& display_name);
    void remove_account(const QString& account_id);
    void update_display_name(const QString& account_id, const QString& name);
    void set_active(const QString& account_id, bool active);
    void set_trading_mode(const QString& account_id, const QString& mode);

    // --- Query ---
    BrokerAccount get_account(const QString& account_id) const;
    QVector<BrokerAccount> list_accounts() const;
    QVector<BrokerAccount> list_accounts(const QString& broker_id) const;
    QVector<BrokerAccount> active_accounts() const;
    bool has_account(const QString& account_id) const;

    // --- Credentials (delegates to SecureStorage with account-scoped keys) ---
    void save_credentials(const QString& account_id, const BrokerCredentials& creds);
    BrokerCredentials load_credentials(const QString& account_id) const;
    void clear_credentials(const QString& account_id);

    // Purge only the live session (access_token + refresh_token + the
    // token_expires_at hint) while KEEPING the reusable login material
    // (api_key/api_secret + any stored TOTP/secret keys) so the user can
    // re-authenticate easily. Used when a token is confirmed dead and cannot be
    // silently refreshed — stops a stale token from lingering and from being
    // re-validated on every launch/sweep.
    void clear_session(const QString& account_id);

    // --- Connection state (memory-only) ---
    void set_connection_state(const QString& account_id, ConnectionState state, const QString& error = {});
    ConnectionState connection_state(const QString& account_id) const;

    // --- Convenience ---
    IBroker* broker_for(const QString& account_id) const;

    // --- Live session monitoring ---
    // Starts a periodic background sweep that pings each active account's broker
    // to confirm its token is still valid, silently refreshing (or marking
    // TokenExpired) as needed. Call once after QApplication is constructed.
    // Also runs one sweep immediately. Safe to call more than once.
    void start_session_monitor();

    // Reload the in-memory account map from the database. The singleton loads
    // eagerly in its constructor on first access — but if that first access
    // happens before Database::open() (the constructor's find_all() then returns
    // nothing), the map stays empty for the entire session and configured
    // brokers appear to "vanish" on restart. Call this once on the main thread
    // right after the DB is open to guarantee accounts are loaded. Idempotent.
    void reload_from_db();

  signals:
    void account_added(const QString& account_id);
    void account_removed(const QString& account_id);
    void account_updated(const QString& account_id);
    // Emitted whenever an account's stored credentials change — manual re-auth
    // or silent token refresh. DataStreamManager listens to rebuild any live
    // stream so WebSocket adapters that capture the access token at construction
    // pick up the fresh token instead of streaming on a now-dead one.
    void credentials_changed(const QString& account_id);
    void connection_state_changed(const QString& account_id, ConnectionState state);

  private:
    AccountManager();
    void load_from_db();
    void migrate_legacy_credentials();
    void validate_all_sessions(); // timer slot — kicks one async validation sweep

    QHash<QString, BrokerAccount> accounts_;
    mutable QMutex mutex_;
    QTimer* validator_timer_ = nullptr;
    std::atomic<bool> sweeping_{false};
};

} // namespace fincept::trading
