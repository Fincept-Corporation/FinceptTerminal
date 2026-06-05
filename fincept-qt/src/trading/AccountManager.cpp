#include "trading/AccountManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/AccountRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/PaperTrading.h"
#include "trading/brokers/BrokerTokenUtil.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>

#include <cmath>
#include <exception>

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

    // Restore a *tentative* connection state from the persisted token-expiry
    // hint. This is a fast path so the UI isn't blank on launch — it is NOT
    // authoritative. start_session_monitor() runs a live validation sweep right
    // after startup that overrides every state here with the real broker answer
    // (and silently refreshes where supported). A past expiry ⇒ TokenExpired; an
    // unknown/future expiry ⇒ optimistically Connected pending the sweep.
    auto& secure = SecureStorage::instance();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (auto it = accounts_.begin(); it != accounts_.end(); ++it) {
        auto token_r = secure.retrieve(acct_key(it->account_id, "access_token"));
        if (!token_r.is_ok() || token_r.value().isEmpty())
            continue;
        auto extra_r = secure.retrieve(acct_key(it->account_id, "additional_data"));
        const qint64 expires_at = extra_r.is_ok() ? token_expires_at_of(extra_r.value()) : 0;
        // Never show green unless we can positively justify it:
        //   past expiry          → TokenExpired (red) — definitively dead
        //   future expiry         → Connected (green) — positive validity hint
        //   unknown expiry (==0)  → Connecting (neutral) — legacy token with no
        //                           recorded expiry; the live sweep decides green
        //                           vs red. Do NOT assume green for these.
        if (expires_at > 0 && expires_at <= now)
            it->state = ConnectionState::TokenExpired;
        else if (expires_at > now)
            it->state = ConnectionState::Connected;
        else
            it->state = ConnectionState::Connecting;
    }
}

void AccountManager::reload_from_db() {
    {
        QMutexLocker lock(&mutex_);
        accounts_.clear();
    }
    load_from_db(); // re-acquires the lock internally
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
        if (std::isfinite(prof.default_paper_balance) && prof.default_paper_balance > 0.0)
            paper_balance = prof.default_paper_balance;
        if (!prof.currency.isEmpty())
            currency = prof.currency;
    }

    // pt_create_portfolio throws on invalid args or DB failure. Catch here so a
    // throw does not propagate across the Qt signal/slot boundary and terminate
    // the app. Account creation proceeds without a paper portfolio on failure.
    try {
        auto portfolio = pt_create_portfolio(
            display_name + " Paper", paper_balance, currency,
            1.0, "cross", 0.001, account.account_id);
        account.paper_portfolio_id = portfolio.id;
    } catch (const std::exception& e) {
        LOG_ERROR("AccountManager",
                  QString("Failed to create paper portfolio for %1 (%2): %3")
                      .arg(broker_id, display_name, e.what()));
        account.paper_portfolio_id.clear();
    }

    // Persist to DB. If the insert fails (e.g. FK violation, disk full), do NOT
    // add the account to the in-memory cache — otherwise the UI would show a
    // "connected" account that silently vanishes on the next app launch.
    auto insert_result = AccountRepository::instance().insert(account);
    if (insert_result.is_err()) {
        LOG_ERROR("AccountManager",
                  QString("Failed to persist account %1 (%2) for broker %3: %4")
                      .arg(account.account_id, display_name, broker_id,
                           QString::fromStdString(insert_result.error())));
        // Clean up the orphaned paper portfolio we created above.
        if (!account.paper_portfolio_id.isEmpty()) {
            try {
                pt_delete_portfolio(account.paper_portfolio_id);
            } catch (const std::exception& e) {
                LOG_WARN("AccountManager",
                         QString("Could not clean up orphaned paper portfolio %1: %2")
                             .arg(account.paper_portfolio_id, e.what()));
            }
        }
        return {}; // empty BrokerAccount signals failure to caller
    }

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

// NOTE: creds.additional_data is an opaque JSON blob owned by the individual
// broker dialog. E.g., Zerodha stores {"password":"...","totp_secret":"..."}.
// AccountManager does not interpret it.
void AccountManager::save_credentials(const QString& account_id, const BrokerCredentials& creds) {
    auto& secure = SecureStorage::instance();
    secure.store(acct_key(account_id, "api_key"), creds.api_key);
    secure.store(acct_key(account_id, "api_secret"), creds.api_secret);
    secure.store(acct_key(account_id, "access_token"), creds.access_token);
    secure.store(acct_key(account_id, "refresh_token"), creds.refresh_token);
    secure.store(acct_key(account_id, "user_id"), creds.user_id);
    secure.store(acct_key(account_id, "additional_data"), creds.additional_data);

    // The live token just changed. Notify listeners (DataStreamManager) so any
    // running stream is rebuilt with the new credentials: WebSocket adapters
    // capture the access token at construction and have no setter, so without a
    // rebuild they keep resolving/subscribing with the stale token (HTTP 401)
    // and never stream ticks — the cause of price data stalling after a daily
    // re-auth until the next slow REST poll happened to reload the new token.
    emit credentials_changed(account_id);
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
    r = secure.retrieve(acct_key(account_id, "refresh_token"));
    if (r.is_ok()) creds.refresh_token = r.value();
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
    secure.remove(acct_key(account_id, "refresh_token"));
    secure.remove(acct_key(account_id, "user_id"));
    secure.remove(acct_key(account_id, "additional_data"));
}

void AccountManager::clear_session(const QString& account_id) {
    auto& secure = SecureStorage::instance();
    secure.remove(acct_key(account_id, "access_token"));
    secure.remove(acct_key(account_id, "refresh_token"));
    // Drop the expiry hint from additional_data but keep the reusable login
    // material (TOTP secret, secret_api_key, client_code, feed_token, …) so the
    // user can reconnect without re-entering everything.
    auto extra_r = secure.retrieve(acct_key(account_id, "additional_data"));
    if (extra_r.is_ok() && !extra_r.value().isEmpty()) {
        auto obj = QJsonDocument::fromJson(extra_r.value().toUtf8()).object();
        obj.remove(QStringLiteral("token_expires_at"));
        secure.store(acct_key(account_id, "additional_data"),
                     QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }
    LOG_INFO("AccountManager", QString("Purged stale session token for account %1").arg(account_id));
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

// ── Live session monitoring ─────────────────────────────────────────────────

void AccountManager::start_session_monitor() {
    if (!validator_timer_) {
        validator_timer_ = new QTimer(this);
        validator_timer_->setInterval(5 * 60 * 1000); // 5 minutes
        connect(validator_timer_, &QTimer::timeout, this, &AccountManager::validate_all_sessions);
    }
    validator_timer_->start();
    validate_all_sessions(); // confirm restored states immediately on launch
}

// Pings each active account's broker on a worker thread to determine the real
// token state, silently refreshing where supported. All SecureStorage access
// stays on the main thread; only the broker HTTP calls run off-thread (P1).
void AccountManager::validate_all_sessions() {
    bool expected = false;
    if (!sweeping_.compare_exchange_strong(expected, true))
        return; // a sweep is already in flight — skip this tick

    // Snapshot the work on the main thread (loads credentials from SecureStorage).
    struct Work {
        QString account_id;
        QString broker_id;
        BrokerCredentials creds;
    };
    QVector<Work> work;
    for (const auto& a : active_accounts()) {
        auto creds = load_credentials(a.account_id);
        if (creds.access_token.isEmpty())
            continue; // never connected — nothing to validate
        work.push_back({a.account_id, a.broker_id, std::move(creds)});
    }
    if (work.isEmpty()) {
        sweeping_.store(false);
        return;
    }

    LOG_INFO("AccountManager", QString("Session sweep: validating %1 active account(s)").arg(work.size()));

    QPointer<AccountManager> self = this; // singleton, but keep the guard idiom
    (void)QtConcurrent::run([self, work]() {
        struct Outcome {
            QString account_id;
            ConnectionState state;
            bool has_new_creds = false;
            BrokerCredentials new_creds;
            bool purge_session = false; // clear the dead token from storage
            QString error;
        };
        QVector<Outcome> outcomes;
        auto& registry = BrokerRegistry::instance();
        const qint64 now = QDateTime::currentSecsSinceEpoch();

        for (const auto& w : work) {
            IBroker* broker = registry.get(w.broker_id);
            if (!broker)
                continue;

            // Broker validate/refresh code runs HTTP + JSON/crypto helpers; a
            // throw here would propagate out of the QtConcurrent worker and
            // call std::terminate (hard crash, no crashdump). Contain it per
            // account — a failure to validate one account must never abort the
            // sweep or the app. Mirrors the pt_create_portfolio guard above.
            try {
            auto v = broker->validate_session(w.creds);
            if (v.status == SessionCheck::Status::Inconclusive) {
                // network/rate-limit/other — never downgrade or purge
                LOG_INFO("AccountManager", QString("sweep[%1/%2]: INCONCLUSIVE — leaving state unchanged (%3)")
                                               .arg(w.broker_id, w.account_id, v.detail.left(160)));
                continue;
            }

            const bool valid = v.status == SessionCheck::Status::Valid;
            const qint64 stored_exp = token_expires_at_of(w.creds.additional_data);
            const bool near_expiry = stored_exp > 0 && (stored_exp - now) < 600; // <10 min
            const bool can_refresh = broker->supports_silent_refresh();
            LOG_INFO("AccountManager", QString("sweep[%1/%2]: valid=%3 near_expiry=%4 can_refresh=%5 real_exp=%6 %7")
                                           .arg(w.broker_id, w.account_id)
                                           .arg(valid).arg(near_expiry).arg(can_refresh)
                                           .arg(v.expires_at_epoch)
                                           .arg(v.detail.left(120)));

            // Healthy and not about to lapse → confirm Connected, and write the
            // broker's real expiry back to storage if it differs from our hint
            // (stops the persisted token_expires_at from going stale).
            if (valid && !(near_expiry && can_refresh)) {
                if (v.expires_at_epoch > 0 && v.expires_at_epoch != stored_exp) {
                    BrokerCredentials nc = w.creds;
                    nc.additional_data = with_token_expiry(nc.additional_data, v.expires_at_epoch);
                    LOG_INFO("AccountManager", QString("sweep[%1/%2]: refreshed stored expiry → %3")
                                                   .arg(w.broker_id, w.account_id).arg(v.expires_at_epoch));
                    outcomes.push_back({w.account_id, ConnectionState::Connected, true, nc, false, {}});
                } else {
                    outcomes.push_back({w.account_id, ConnectionState::Connected, false, {}, false, {}});
                }
                continue;
            }

            // Expired, or valid-but-imminently-expiring with refresh available.
            if (can_refresh) {
                LOG_INFO("AccountManager", QString("sweep[%1/%2]: attempting silent refresh").arg(w.broker_id, w.account_id));
                auto t = broker->refresh_session(w.creds);
                if (t.success && !t.access_token.isEmpty()) {
                    BrokerCredentials nc = w.creds;
                    nc.access_token = t.access_token;
                    if (!t.refresh_token.isEmpty())
                        nc.refresh_token = t.refresh_token;
                    if (!t.user_id.isEmpty())
                        nc.user_id = t.user_id;
                    if (!t.additional_data.isEmpty()) {
                        auto base = QJsonDocument::fromJson(nc.additional_data.toUtf8()).object();
                        const auto add = QJsonDocument::fromJson(t.additional_data.toUtf8()).object();
                        for (auto it = add.begin(); it != add.end(); ++it)
                            base.insert(it.key(), it.value());
                        nc.additional_data = QString::fromUtf8(QJsonDocument(base).toJson(QJsonDocument::Compact));
                    }
                    LOG_INFO("AccountManager", QString("sweep[%1/%2]: silent refresh OK → Connected").arg(w.broker_id, w.account_id));
                    outcomes.push_back({w.account_id, ConnectionState::Connected, true, nc, false, {}});
                    continue;
                }
                // Refresh failed. If the token is still valid (we were only
                // pre-emptively refreshing), keep it Connected. If it is dead,
                // purge the stale session so it stops being re-validated.
                if (valid) {
                    outcomes.push_back({w.account_id, ConnectionState::Connected, false, {}, false, {}});
                } else {
                    LOG_WARN("AccountManager", QString("sweep[%1/%2]: refresh FAILED on dead token → purge + TokenExpired (%3)")
                                                   .arg(w.broker_id, w.account_id, t.error.left(160)));
                    outcomes.push_back({w.account_id, ConnectionState::TokenExpired, false, {}, true, t.error});
                }
                continue;
            }

            // No silent refresh available → reflect the live answer; purge if dead.
            if (!valid) {
                LOG_WARN("AccountManager", QString("sweep[%1/%2]: token DEAD, no silent refresh → purge + TokenExpired (%3)")
                                               .arg(w.broker_id, w.account_id, v.detail.left(160)));
                outcomes.push_back({w.account_id, ConnectionState::TokenExpired, false, {}, true, v.detail});
            }
            } catch (const std::exception& e) {
                LOG_ERROR("AccountManager", QString("Session validation threw for %1 (%2): %3")
                                                .arg(w.account_id, w.broker_id, QString::fromUtf8(e.what())));
            } catch (...) {
                LOG_ERROR("AccountManager", QString("Session validation threw (non-std) for %1 (%2)")
                                                .arg(w.account_id, w.broker_id));
            }
        }

        // Apply results on the main thread (SecureStorage + signal emission).
        QMetaObject::invokeMethod(
            qApp,
            [self, outcomes]() {
                if (self) {
                    for (const auto& o : outcomes) {
                        if (o.purge_session)
                            self->clear_session(o.account_id); // drop the dead token from storage
                        else if (o.has_new_creds)
                            self->save_credentials(o.account_id, o.new_creds);
                        self->set_connection_state(o.account_id, o.state, o.error);
                    }
                    self->sweeping_.store(false);
                }
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::trading
