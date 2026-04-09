#include "trading/AccountManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/AccountRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "trading/BrokerRegistry.h"
#include "trading/PaperTrading.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QUuid>

namespace fincept::trading {

// ── Credential key helpers ──────────────────────────────────────────────────

static QString acct_key(const QString& account_id, const char* field) {
    return QString("account.%1.%2").arg(account_id, field);
}

// ── Singleton ───────────────────────────────────────────────────────────────

AccountManager& AccountManager::instance() {
    static AccountManager s;
    return s;
}

AccountManager::AccountManager() {
    load_from_db();
    migrate_legacy_credentials();
}

// ── DB bootstrap ────────────────────────────────────────────────────────────

void AccountManager::load_from_db() {
    auto r = AccountRepository::instance().find_all();
    if (!r.is_ok())
        return;
    QMutexLocker lock(&mutex_);
    for (auto& a : r.value())
        accounts_.insert(a.account_id, std::move(a));
    LOG_INFO("AccountManager", QString("Loaded %1 broker accounts from DB").arg(accounts_.size()));
}

// ── Legacy credential migration ─────────────────────────────────────────────

void AccountManager::migrate_legacy_credentials() {
    auto flag = SettingsRepository::instance().get("migration.accounts_v1");
    if (flag.is_ok() && flag.value() == "done")
        return;

    auto& secure = SecureStorage::instance();
    auto& registry = BrokerRegistry::instance();
    int migrated = 0;

    for (const auto& broker_id : registry.list_brokers()) {
        auto* broker = registry.get(broker_id);
        if (!broker)
            continue;
        const auto prof = broker->profile();

        // Check for credentials stored in old format: broker.{id}.api_key
        // For native-paper brokers, also check broker.{id}.live.api_key and broker.{id}.paper.api_key
        auto try_migrate = [&](const QString& prefix, const QString& suffix) {
            auto key_r = secure.retrieve(prefix + "api_key");
            if (!key_r.is_ok())
                return;

            // Already migrated? Check if an account exists for this broker with same api_key
            const QString api_key = key_r.value();
            if (api_key.isEmpty())
                return;

            {
                QMutexLocker lock(&mutex_);
                for (const auto& existing : accounts_) {
                    if (existing.broker_id == broker_id) {
                        // Check if credentials match (already migrated)
                        auto existing_key = secure.retrieve(acct_key(existing.account_id, "api_key"));
                        if (existing_key.is_ok() && existing_key.value() == api_key)
                            return; // already migrated
                    }
                }
            }

            // Create new account
            QString display_name = prof.display_name;
            if (!suffix.isEmpty())
                display_name += " (" + suffix + ")";

            auto account = add_account(broker_id, display_name);

            // Copy credentials to new account-scoped keys
            BrokerCredentials creds;
            creds.broker_id = broker_id;
            creds.api_key = api_key;
            auto s = secure.retrieve(prefix + "api_secret");
            if (s.is_ok()) creds.api_secret = s.value();
            s = secure.retrieve(prefix + "access_token");
            if (s.is_ok()) creds.access_token = s.value();
            s = secure.retrieve(prefix + "user_id");
            if (s.is_ok()) creds.user_id = s.value();
            s = secure.retrieve(prefix + "additional_data");
            if (s.is_ok()) creds.additional_data = s.value();

            save_credentials(account.account_id, creds);
            ++migrated;
            LOG_INFO("AccountManager", QString("Migrated legacy credentials for %1 → account %2")
                                           .arg(broker_id, account.account_id));
        };

        if (prof.has_native_paper) {
            // Try live and paper environments
            try_migrate(QString("broker.%1.live.").arg(broker_id), "Live");
            try_migrate(QString("broker.%1.paper.").arg(broker_id), "Paper");
        } else {
            try_migrate(QString("broker.%1.").arg(broker_id), "");
        }
    }

    SettingsRepository::instance().set("migration.accounts_v1", "done", "system");
    if (migrated > 0)
        LOG_INFO("AccountManager", QString("Legacy credential migration complete: %1 accounts created").arg(migrated));
}

// ── Account CRUD ────────────────────────────────────────────────────────────

BrokerAccount AccountManager::add_account(const QString& broker_id, const QString& display_name) {
    BrokerAccount account;
    account.account_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    account.broker_id = broker_id;
    account.display_name = display_name;
    account.trading_mode = "paper";
    account.is_active = true;
    account.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Create linked paper portfolio
    auto* broker = BrokerRegistry::instance().get(broker_id);
    double paper_balance = 1000000.0;
    QString currency = "INR";
    if (broker) {
        const auto prof = broker->profile();
        paper_balance = prof.default_paper_balance;
        currency = prof.currency;
    }

    auto portfolio = pt_create_portfolio(
        display_name + " Paper", paper_balance, currency,
        1.0, "cross", 0.001, account.account_id);
    account.paper_portfolio_id = portfolio.id;

    // Persist to DB
    AccountRepository::instance().insert(account);

    // Add to in-memory cache
    {
        QMutexLocker lock(&mutex_);
        accounts_.insert(account.account_id, account);
    }

    emit account_added(account.account_id);
    LOG_INFO("AccountManager", QString("Added account: %1 (%2) for broker %3")
                                   .arg(account.account_id, display_name, broker_id));
    return account;
}

void AccountManager::remove_account(const QString& account_id) {
    BrokerAccount account;
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return;
        account = it.value();
        accounts_.erase(it);
    }

    // Remove credentials from SecureStorage
    clear_credentials(account_id);

    // Remove paper portfolio if it exists
    if (!account.paper_portfolio_id.isEmpty())
        pt_delete_portfolio(account.paper_portfolio_id);

    // Remove from DB
    AccountRepository::instance().remove(account_id);

    emit account_removed(account_id);
    LOG_INFO("AccountManager", QString("Removed account: %1 (%2)")
                                   .arg(account_id, account.display_name));
}

void AccountManager::update_display_name(const QString& account_id, const QString& name) {
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return;
        it->display_name = name;
    }
    // Persist
    auto opt = AccountRepository::instance().find_by_id(account_id);
    if (opt) {
        opt->display_name = name;
        AccountRepository::instance().update(*opt);
    }
    emit account_updated(account_id);
}

void AccountManager::set_active(const QString& account_id, bool active) {
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return;
        it->is_active = active;
    }
    auto opt = AccountRepository::instance().find_by_id(account_id);
    if (opt) {
        opt->is_active = active;
        AccountRepository::instance().update(*opt);
    }
    emit account_updated(account_id);
}

void AccountManager::set_trading_mode(const QString& account_id, const QString& mode) {
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return;
        it->trading_mode = mode;
    }
    auto opt = AccountRepository::instance().find_by_id(account_id);
    if (opt) {
        opt->trading_mode = mode;
        AccountRepository::instance().update(*opt);
    }
    emit account_updated(account_id);
}

// ── Query ───────────────────────────────────────────────────────────────────

BrokerAccount AccountManager::get_account(const QString& account_id) const {
    QMutexLocker lock(&mutex_);
    auto it = accounts_.find(account_id);
    if (it != accounts_.end())
        return it.value();
    return {};
}

QVector<BrokerAccount> AccountManager::list_accounts() const {
    QMutexLocker lock(&mutex_);
    QVector<BrokerAccount> result;
    result.reserve(accounts_.size());
    for (const auto& a : accounts_)
        result.append(a);
    return result;
}

QVector<BrokerAccount> AccountManager::list_accounts(const QString& broker_id) const {
    QMutexLocker lock(&mutex_);
    QVector<BrokerAccount> result;
    for (const auto& a : accounts_) {
        if (a.broker_id == broker_id)
            result.append(a);
    }
    return result;
}

QVector<BrokerAccount> AccountManager::active_accounts() const {
    QMutexLocker lock(&mutex_);
    QVector<BrokerAccount> result;
    for (const auto& a : accounts_) {
        if (a.is_active)
            result.append(a);
    }
    return result;
}

bool AccountManager::has_account(const QString& account_id) const {
    QMutexLocker lock(&mutex_);
    return accounts_.contains(account_id);
}

// ── Credentials ─────────────────────────────────────────────────────────────

void AccountManager::save_credentials(const QString& account_id, const BrokerCredentials& creds) {
    auto& secure = SecureStorage::instance();
    secure.store(acct_key(account_id, "api_key"), creds.api_key);
    secure.store(acct_key(account_id, "api_secret"), creds.api_secret);
    secure.store(acct_key(account_id, "access_token"), creds.access_token);
    secure.store(acct_key(account_id, "user_id"), creds.user_id);
    secure.store(acct_key(account_id, "additional_data"), creds.additional_data);
}

BrokerCredentials AccountManager::load_credentials(const QString& account_id) const {
    BrokerCredentials creds;
    auto& secure = SecureStorage::instance();

    // Get broker_id from account
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it != accounts_.end())
            creds.broker_id = it->broker_id;
    }

    auto r = secure.retrieve(acct_key(account_id, "api_key"));
    if (r.is_ok()) creds.api_key = r.value();
    r = secure.retrieve(acct_key(account_id, "api_secret"));
    if (r.is_ok()) creds.api_secret = r.value();
    r = secure.retrieve(acct_key(account_id, "access_token"));
    if (r.is_ok()) creds.access_token = r.value();
    r = secure.retrieve(acct_key(account_id, "user_id"));
    if (r.is_ok()) creds.user_id = r.value();
    r = secure.retrieve(acct_key(account_id, "additional_data"));
    if (r.is_ok()) creds.additional_data = r.value();

    return creds;
}

void AccountManager::clear_credentials(const QString& account_id) {
    auto& secure = SecureStorage::instance();
    secure.remove(acct_key(account_id, "api_key"));
    secure.remove(acct_key(account_id, "api_secret"));
    secure.remove(acct_key(account_id, "access_token"));
    secure.remove(acct_key(account_id, "user_id"));
    secure.remove(acct_key(account_id, "additional_data"));
}

// ── Connection state ────────────────────────────────────────────────────────

void AccountManager::set_connection_state(const QString& account_id, ConnectionState state, const QString& error) {
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return;
        it->state = state;
        it->error_message = error;
    }
    emit connection_state_changed(account_id, state);
}

ConnectionState AccountManager::connection_state(const QString& account_id) const {
    QMutexLocker lock(&mutex_);
    auto it = accounts_.find(account_id);
    if (it != accounts_.end())
        return it->state;
    return ConnectionState::Disconnected;
}

// ── Convenience ─────────────────────────────────────────────────────────────

IBroker* AccountManager::broker_for(const QString& account_id) const {
    QString broker_id;
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return nullptr;
        broker_id = it->broker_id;
    }
    return BrokerRegistry::instance().get(broker_id);
}

} // namespace fincept::trading
