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
    // For kBuiltinPaperAccountId this returns the synthesised built-in paper account
    // (see BrokerAccount.h); every other id behaves exactly as before.
    BrokerAccount get_account(const QString& account_id) const;
    QVector<BrokerAccount> list_accounts() const;
    QVector<BrokerAccount> list_accounts(const QString& broker_id) const;
    QVector<BrokerAccount> active_accounts() const;
    bool has_account(const QString& account_id) const;

    // --- Credentials (delegates to SecureStorage with account-scoped keys) ---
    // save_credentials() never writes an EMPTY field over a stored value that cannot
    // be read (a caller that loaded, changed one field and saved would otherwise
    // blank it); only a damaged (Corrupt) row is replaced.
    void save_credentials(const QString& account_id, const BrokerCredentials& creds);
    // A field that cannot be read (secure storage busy, damaged row, key unavailable)
    // is left empty, as before, but it is NOT "not set": `read_error` (when given) gets
    // a plain sentence, the account shows ConnectionState::Error with that sentence
    // until a later read succeeds, and the failure is logged once. Callers that act on
    // the result (validate, purge, save back) must check `read_error` first.
    BrokerCredentials load_credentials(const QString& account_id, QString* read_error = nullptr) const;
    void clear_credentials(const QString& account_id);
    // Why `account_id`'s stored credentials could not be read at the last load; empty
    // when they could. For status checks (e.g. a "broker connected" requirement) that
    // must say "could not read the stored credential" instead of "connect a broker".
    // A load_credentials() call is the retry. Any thread.
    QString credential_read_error(const QString& account_id) const;

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

    // --- Built-in paper account (no broker; see BrokerAccount.h) ---
    // The record, without creating anything: paper_portfolio_id is empty until
    // ensure_builtin_paper_account() has run once (on this or an earlier launch).
    BrokerAccount builtin_paper_account() const;
    // The record, creating its paper portfolio on first use with `starting_balance`
    // in `currency` (the user's base currency). Idempotent; GUI thread.
    BrokerAccount ensure_builtin_paper_account(const QString& currency, double starting_balance);
    // Start over: delete the built-in portfolio (orders, positions, trades) and create
    // a fresh one with `starting_balance` in `currency`. Emits builtin_paper_reset.
    // Returns false (and keeps the old portfolio) when the new one cannot be created.
    bool reset_builtin_paper_account(const QString& currency, double starting_balance, QString* error = nullptr);

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
    // The built-in paper account was reset onto a new portfolio.
    void builtin_paper_reset(const QString& portfolio_id);

  private:
    AccountManager();
    void load_from_db();
    void migrate_legacy_credentials();
    void validate_all_sessions(); // timer slot — kicks one async validation sweep
    // Records the outcome of one credential read for `account_id` (empty `failure` =
    // every field readable): shows / clears the "could not read" Error state on the
    // main thread and logs the change once. Any thread; takes mutex_.
    void note_credential_read(const QString& account_id, const QString& failure) const;

    QHash<QString, BrokerAccount> accounts_;
    // Accounts whose stored credentials could not be read, with the state they had
    // before so it can be put back once a read succeeds (guarded by mutex_).
    struct CredReadFailure {
        ConnectionState prev_state = ConnectionState::Disconnected;
        QString prev_error;
        QString shown_error; // the sentence set as error_message
    };
    mutable QHash<QString, CredReadFailure> cred_read_failed_;
    mutable QMutex mutex_;
    // Built-in paper portfolio id, cached on first lookup (guarded by mutex_).
    mutable QString builtin_portfolio_id_;
    QTimer* validator_timer_ = nullptr;
    std::atomic<bool> sweeping_{false};
};

} // namespace fincept::trading
