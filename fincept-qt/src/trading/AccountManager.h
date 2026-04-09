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

    // --- Connection state (memory-only) ---
    void set_connection_state(const QString& account_id, ConnectionState state, const QString& error = {});
    ConnectionState connection_state(const QString& account_id) const;

    // --- Convenience ---
    IBroker* broker_for(const QString& account_id) const;

  signals:
    void account_added(const QString& account_id);
    void account_removed(const QString& account_id);
    void account_updated(const QString& account_id);
    void connection_state_changed(const QString& account_id, ConnectionState state);

  private:
    AccountManager();
    void load_from_db();
    void migrate_legacy_credentials();

    QHash<QString, BrokerAccount> accounts_;
    mutable QMutex mutex_;
};

} // namespace fincept::trading
