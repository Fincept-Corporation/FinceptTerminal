#include "trading/AccountManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/AccountRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecretMigration.h"
#include "storage/secure/SecureStorage.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/brokers/BrokerTokenUtil.h"
#include "ui/formatting/FormatSettings.h"

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

namespace {

/// What the account shows while a stored credential cannot be read.
QString am_cred_unreadable_text(const SecretRead& r) {
    return AccountManager::tr("Could not read the stored credential: %1. The terminal retries automatically.")
        .arg(r.reason());
}

/// One line when an account's credentials start failing to read, one when they read
/// again (SecureStorage logs each key's own failure). Names only, never a value.
void am_log_cred_read(const QString& account_id, const QString& broker_id, bool failing) {
    LOG_TRANSITION("AccountManager", QStringLiteral("cred_read|") + account_id, failing,
                   failing ? LogLevel::Warn : LogLevel::Info,
                   failing ? logf("Broker credentials could not be read; the account shows an error and nothing is "
                                  "written, validated or purged until they read again",
                                  {{"broker", broker_id}, {"account", log_mask(account_id)}})
                           : logf("Broker credentials readable again",
                                  {{"broker", broker_id}, {"account", log_mask(account_id)}}));
}

/// Cash currency of a new paper book when nothing more specific is known (a broker
/// the registry does not know; the built-in account created without one): the
/// user's display currency (Settings > Currency & Number Format; Automatic = the
/// region's currency) — never a hard-coded code.
QString am_fallback_book_currency() {
    return fincept::ui::formatting::FormatSettings::instance().display_currency();
}

} // namespace

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
    if (!r.is_ok()) {
        LOG_ERROR("AccountManager", logf("Broker accounts could not be loaded; the account list starts empty",
                                         {{"error", QString::fromStdString(r.error())}}));
        return;
    }
    QMutexLocker lock(&mutex_);
    for (auto& a : r.value())
        accounts_.insert(a.account_id, std::move(a));
    LOG_DEBUG("AccountManager", QString("Loaded %1 broker accounts from DB").arg(accounts_.size()));

    // Restore a *tentative* connection state from the persisted token-expiry
    // hint. This is a fast path so the UI isn't blank on launch — it is NOT
    // authoritative. start_session_monitor() runs a live validation sweep right
    // after startup that overrides every state here with the real broker answer
    // (and silently refreshes where supported). A past expiry ⇒ TokenExpired; an
    // unknown/future expiry ⇒ optimistically Connected pending the sweep.
    auto& secure = SecureStorage::instance();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (auto it = accounts_.begin(); it != accounts_.end(); ++it) {
        const SecretRead token_r = secure.read(acct_key(it->account_id, "access_token"));
        const bool has_token = token_r.found() && !token_r.value.isEmpty();
        const SecretRead extra_r =
            has_token ? secure.read(acct_key(it->account_id, "additional_data")) : SecretRead::not_found();
        if (token_r.failed() || extra_r.failed()) {
            // Stored but unreadable: neither "never connected" (Disconnected) nor a
            // session to restore. Shown as an error until a read succeeds (the session
            // sweep reads again every few minutes), then put back to Disconnected /
            // Connecting for the sweep to decide. note_credential_read() does that.
            CredReadFailure f;
            f.prev_state = has_token ? ConnectionState::Connecting : ConnectionState::Disconnected;
            f.shown_error = am_cred_unreadable_text(token_r.failed() ? token_r : extra_r);
            it->state = ConnectionState::Error;
            it->error_message = f.shown_error;
            cred_read_failed_.insert(it->account_id, f);
            am_log_cred_read(it->account_id, it->broker_id, true);
            continue;
        }
        if (!has_token)
            continue;
        const qint64 expires_at = extra_r.found() ? token_expires_at_of(extra_r.value) : 0;
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
        cred_read_failed_.clear(); // load_from_db() records the accounts that still fail
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
    // A legacy credential that could not be read leaves the migration unfinished: that
    // account is NOT created (copying the unreadable fields would store them as blanks
    // in the new account) and the "done" flag is not set, so the next start tries again.
    // Accounts already migrated are recognised by their api_key, so a retry is harmless.
    int deferred = 0;
    auto defer = [&](const QString& prefix, const QString& broker_id, const QString& why) {
        ++deferred;
        LOG_TRANSITION("AccountManager", QStringLiteral("legacy_migration|") + prefix, 1, LogLevel::Warn,
                       logf("Legacy credentials not migrated this time; retried at the next start",
                            {{"broker", broker_id}, {"key_prefix", prefix}, {"reason", why}}));
    };

    for (const auto& broker_id : registry.list_brokers()) {
        auto* broker = registry.get(broker_id);
        if (!broker)
            continue;
        const auto prof = broker->profile();

        // Check for credentials stored in old format: broker.{id}.api_key
        // For native-paper brokers, also check broker.{id}.live.api_key and broker.{id}.paper.api_key
        auto try_migrate = [&](const QString& prefix, const QString& suffix) {
            const SecretRead key_r = secure.read(prefix + "api_key");
            if (key_r.failed()) {
                defer(prefix, broker_id, QStringLiteral("api_key unreadable (%1)").arg(key_r.error_name()));
                return;
            }
            if (!key_r.found())
                return;

            // Already migrated? Check if an account exists for this broker with same api_key
            const QString api_key = key_r.value;
            if (api_key.isEmpty())
                return;

            bool existing_unreadable = false;
            QString migrated_to; // the account an earlier run created for these credentials
            {
                QMutexLocker lock(&mutex_);
                for (const auto& existing : accounts_) {
                    if (existing.broker_id == broker_id) {
                        // Check if credentials match (already migrated)
                        const SecretRead existing_key = secure.read(acct_key(existing.account_id, "api_key"));
                        if (existing_key.failed())
                            existing_unreadable = true; // cannot tell — never create a duplicate on that
                        else if (existing_key.found() && existing_key.value == api_key && migrated_to.isEmpty())
                            migrated_to = existing.account_id;
                    }
                }
            }
            if (existing_unreadable && migrated_to.isEmpty()) {
                defer(prefix, broker_id, QStringLiteral("an existing account's api_key is unreadable"));
                return;
            }

            // Read every field BEFORE creating the account: one that cannot be read
            // aborts this account's migration instead of being copied as a blank.
            BrokerCredentials creds;
            creds.broker_id = broker_id;
            creds.api_key = api_key;
            QString unreadable_field;
            auto take = [&](const char* field, QString& dest) {
                const SecretRead r = secure.read(prefix + QLatin1String(field));
                if (r.found())
                    dest = r.value;
                else if (r.failed() && unreadable_field.isEmpty())
                    unreadable_field = QString::fromLatin1(field);
            };
            take("api_secret", creds.api_secret);
            take("access_token", creds.access_token);
            take("user_id", creds.user_id);
            take("additional_data", creds.additional_data);
            if (!unreadable_field.isEmpty()) {
                defer(prefix, broker_id, unreadable_field + QStringLiteral(" unreadable"));
                return;
            }

            // Create the new account — unless an earlier run already did: then this run only
            // completes / verifies its copy (an earlier copy that failed half-way is repaired).
            QString account_id = migrated_to;
            if (account_id.isEmpty()) {
                QString display_name = prof.display_name;
                if (!suffix.isEmpty())
                    display_name += " (" + suffix + ")";

                auto account = add_account(broker_id, display_name);
                if (account.account_id.isEmpty()) {
                    // add_account() logged why; storing under "account..*" would lose the copy.
                    defer(prefix, broker_id, QStringLiteral("the account could not be created"));
                    return;
                }
                account_id = account.account_id;
            }

            // Copy every field to the account-scoped key by the one migration rule
            // (storage/secure/SecretMigration.h): write -> read back and compare. The legacy
            // copy is kept (as before), so "only then anything else" is the done flag: it is
            // set only when every field of every account was verified. A store that cannot
            // be read, a failed write or a read-back that differs defers to the next start.
            // A different value already under the account key (a newer token saved after an
            // earlier migration) is the account's own and is left alone.
            const bool fresh = migrated_to.isEmpty();
            QString copy_failure;
            const auto copy_field = [&](const char* field, const QString& value) {
                if (!copy_failure.isEmpty())
                    return;
                const QString key = acct_key(account_id, field);
                secret_migration::Io io;
                io.read_stored = [&secure, key]() { return secure.read(key); };
                io.write_stored = [&secure, key](const QString& v) { return secure.store(key, v); };
                io.delete_old = []() { return Result<void>::ok(); }; // the legacy copy stays where it is
                const auto out = secret_migration::run(secret_migration::OldCopy::present(value),
                                                       secret_migration::OnConflict::KeepBoth, io);
                if (out.failure == secret_migration::Failure::None ||
                    (out.failure == secret_migration::Failure::Conflict && !fresh))
                    return;
                copy_failure = QStringLiteral("%1: %2").arg(QString::fromLatin1(field),
                                                            QString::fromLatin1(secret_migration::failure_name(out.failure)));
            };
            copy_field("api_key", creds.api_key);
            copy_field("api_secret", creds.api_secret);
            copy_field("access_token", creds.access_token);
            copy_field("user_id", creds.user_id);
            copy_field("additional_data", creds.additional_data);
            if (!copy_failure.isEmpty()) {
                // The account exists; the next start finds it by its api_key (once that
                // copy is verified) and completes the remaining fields.
                defer(prefix, broker_id, QStringLiteral("copy not verified (%1)").arg(copy_failure));
                return;
            }
            if (fresh) {
                ++migrated;
                emit credentials_changed(account_id);
                LOG_INFO("AccountManager",
                         QString("Migrated legacy credentials for %1 → account %2").arg(broker_id, account_id));
            }
        };

        if (prof.has_native_paper) {
            // Try live and paper environments
            try_migrate(QString("broker.%1.live.").arg(broker_id), "Live");
            try_migrate(QString("broker.%1.paper.").arg(broker_id), "Paper");
        } else {
            try_migrate(QString("broker.%1.").arg(broker_id), "");
        }
    }

    if (deferred > 0) {
        LOG_WARN("AccountManager", QString("Legacy credential migration unfinished: %1 created, %2 deferred to the "
                                           "next start (stored credentials could not be read)")
                                       .arg(migrated)
                                       .arg(deferred));
        return; // the flag stays unset: retried at the next start
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

    // Create linked paper portfolio. Its cash is in the broker's account currency
    // (the profile: INR for Zerodha, USD for Alpaca, EUR for Saxo); only a broker the
    // registry does not know falls back to the user's display currency.
    auto* broker = BrokerRegistry::instance().get(broker_id);
    double paper_balance = 1000000.0;
    QString currency;
    if (broker) {
        const auto prof = broker->profile();
        if (std::isfinite(prof.default_paper_balance) && prof.default_paper_balance > 0.0)
            paper_balance = prof.default_paper_balance;
        currency = prof.currency.trimmed().toUpper();
    }
    if (currency.isEmpty()) {
        currency = am_fallback_book_currency();
        LOG_WARN("AccountManager", QString("Paper book for broker '%1' has no profile currency: using the display "
                                           "currency %2")
                                       .arg(broker_id, currency));
    }

    // pt_create_portfolio throws on invalid args or DB failure. Catch here so a
    // throw does not propagate across the Qt signal/slot boundary and terminate
    // the app. Account creation proceeds without a paper portfolio on failure.
    try {
        auto portfolio = pt_create_portfolio(display_name + " Paper", paper_balance, currency, 1.0, "cross", 0.001,
                                             account.account_id);
        account.paper_portfolio_id = portfolio.id;
    } catch (const std::exception& e) {
        LOG_ERROR("AccountManager", logf("Paper portfolio could not be created for a new broker account",
                                         {{"broker", broker_id}, {"account", log_mask(account.account_id)},
                                          {"error", e.what()}}));
        account.paper_portfolio_id.clear();
    }

    // Persist to DB. If the insert fails (e.g. FK violation, disk full), do NOT
    // add the account to the in-memory cache — otherwise the UI would show a
    // "connected" account that silently vanishes on the next app launch.
    auto insert_result = AccountRepository::instance().insert(account);
    if (insert_result.is_err()) {
        LOG_ERROR("AccountManager", logf("Broker account could not be saved; it was not added",
                                         {{"broker", broker_id}, {"account", log_mask(account.account_id)},
                                          {"error", QString::fromStdString(insert_result.error())}}));
        // Clean up the orphaned paper portfolio we created above.
        if (!account.paper_portfolio_id.isEmpty()) {
            try {
                pt_delete_portfolio(account.paper_portfolio_id);
            } catch (const std::exception& e) {
                LOG_WARN("AccountManager", QString("Could not clean up orphaned paper portfolio %1: %2")
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
    LOG_INFO("AccountManager", logf("Broker account added", {{"broker", broker_id},
                                                             {"account", log_mask(account.account_id)},
                                                             {"mode", account.trading_mode}}));
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
    LOG_INFO("AccountManager",
             logf("Broker account removed", {{"broker", account.broker_id}, {"account", log_mask(account_id)}}));
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
    // The built-in paper account has no broker: it can never be switched to LIVE.
    if (is_builtin_paper_account(account_id)) {
        if (mode != QLatin1String("paper"))
            LOG_AUDIT("Orders", "Trading mode change refused",
                      {{"account", account_id}, {"to", mode}, {"reason", "the built-in paper account has no broker"}});
        return;
    }
    QString previous_mode; // for the audit line only
    QString broker_id;
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it == accounts_.end())
            return;
        previous_mode = it->trading_mode;
        broker_id = it->broker_id;
        it->trading_mode = mode;
    }
    if (previous_mode != mode)
        LOG_AUDIT("Orders", "Trading mode changed",
                  {{"venue", broker_id}, {"account", log_mask(account_id)}, {"from", previous_mode}, {"to", mode}});
    auto opt = AccountRepository::instance().find_by_id(account_id);
    if (opt) {
        opt->trading_mode = mode;
        AccountRepository::instance().update(*opt);
    }
    emit account_updated(account_id);
}

// ── Query ───────────────────────────────────────────────────────────────────

BrokerAccount AccountManager::get_account(const QString& account_id) const {
    if (is_builtin_paper_account(account_id))
        return builtin_paper_account();
    QMutexLocker lock(&mutex_);
    auto it = accounts_.find(account_id);
    if (it != accounts_.end())
        return it.value();
    return {};
}

// ── Built-in paper account ──────────────────────────────────────────────────

namespace {
// The portfolio behind the built-in account: the pt_portfolios row tagged with the
// account id in its `exchange` column (broker paper portfolios carry their account
// id there too). The newest wins if an interrupted reset ever left two.
QString am_find_builtin_portfolio() {
    const auto rows = pt_list_portfolios(QString::fromLatin1(kBuiltinPaperAccountId));
    QString id;
    QString created;
    for (const auto& p : rows) {
        if (id.isEmpty() || p.created_at >= created) {
            id = p.id;
            created = p.created_at;
        }
    }
    return id;
}
} // namespace

BrokerAccount AccountManager::builtin_paper_account() const {
    QString pid;
    {
        QMutexLocker lock(&mutex_);
        pid = builtin_portfolio_id_;
    }
    if (pid.isEmpty()) {
        pid = am_find_builtin_portfolio(); // DB read outside the lock
        if (!pid.isEmpty()) {
            QMutexLocker lock(&mutex_);
            builtin_portfolio_id_ = pid;
        }
    }
    BrokerAccount a;
    a.account_id = QString::fromLatin1(kBuiltinPaperAccountId);
    a.broker_id = QString::fromLatin1(kBuiltinPaperBrokerId);
    a.display_name = tr("Paper (delayed prices)");
    a.paper_portfolio_id = pid;
    a.trading_mode = QStringLiteral("paper");
    a.is_active = true;
    a.state = ConnectionState::Disconnected; // no broker session, ever
    return a;
}

BrokerAccount AccountManager::ensure_builtin_paper_account(const QString& currency, double starting_balance) {
    BrokerAccount a = builtin_paper_account();
    if (!a.paper_portfolio_id.isEmpty())
        return a;
    const double balance = (std::isfinite(starting_balance) && starting_balance > 0.0) ? starting_balance : 100000.0;
    const QString ccy = currency.trimmed().isEmpty() ? am_fallback_book_currency() : currency.trimmed().toUpper();
    try {
        // fee_rate 0.0005 (5 bp per fill) — a plain, stated simulation cost.
        const auto pf = pt_create_portfolio(tr("Paper (delayed prices)"), balance, ccy, 1.0, QStringLiteral("cross"),
                                            0.0005, QString::fromLatin1(kBuiltinPaperAccountId));
        {
            QMutexLocker lock(&mutex_);
            builtin_portfolio_id_ = pf.id;
        }
        a.paper_portfolio_id = pf.id;
        LOG_INFO("AccountManager",
                 QString("Created the built-in paper account: %1 %2 (portfolio %3)").arg(balance, 0, 'f', 2).arg(ccy, pf.id));
    } catch (const std::exception& e) {
        LOG_ERROR("AccountManager", logf("Built-in paper portfolio could not be created",
                                         {{"currency", ccy}, {"balance", balance}, {"error", e.what()}}));
    }
    return a;
}

bool AccountManager::reset_builtin_paper_account(const QString& currency, double starting_balance, QString* error) {
    const QString old_pid = builtin_paper_account().paper_portfolio_id;
    if (!(std::isfinite(starting_balance) && starting_balance > 0.0)) {
        if (error)
            *error = tr("The starting balance must be a positive amount.");
        return false;
    }
    const QString ccy = currency.trimmed().isEmpty() ? am_fallback_book_currency() : currency.trimmed().toUpper();
    // New portfolio first: a failure here leaves the old account untouched. The
    // lookup prefers the newest tagged row, so the new one wins from this moment.
    PtPortfolio fresh;
    try {
        fresh = pt_create_portfolio(tr("Paper (delayed prices)"), starting_balance, ccy, 1.0, QStringLiteral("cross"),
                                    0.0005, QString::fromLatin1(kBuiltinPaperAccountId));
    } catch (const std::exception& e) {
        LOG_ERROR("AccountManager", logf("Built-in paper account reset failed; the old account is unchanged",
                                         {{"currency", ccy}, {"balance", starting_balance}, {"error", e.what()}}));
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
    {
        QMutexLocker lock(&mutex_);
        builtin_portfolio_id_ = fresh.id;
    }
    if (!old_pid.isEmpty() && old_pid != fresh.id) {
        OrderMatcher::instance().clear_portfolio_orders(old_pid); // nothing of the old book may still fill
        try {
            pt_delete_portfolio(old_pid); // orders, positions, trades go with it
        } catch (const std::exception& e) {
            LOG_WARN("AccountManager",
                     QString("Built-in paper reset: old portfolio %1 not deleted: %2").arg(old_pid, e.what()));
        }
    }
    LOG_AUDIT("Orders", "Paper account reset",
              {{"account", QString::fromLatin1(kBuiltinPaperAccountId)},
               {"mode", "paper"},
               {"currency", ccy},
               {"balance", starting_balance},
               {"portfolio", log_mask(fresh.id)}});
    emit builtin_paper_reset(fresh.id);
    return true;
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
    // Callers load, change a field and save the whole set back. A field that could
    // not be read came back EMPTY, and writing that empty value would erase the
    // stored one for good. So an empty value is not written over a stored value that
    // cannot be read (busy / closed store, I/O, key unavailable on this account): it
    // is kept for the next read. Only a damaged row is replaced — it can never be
    // read again, and keeping it would leave the account failing on every load. A
    // non-empty value always replaces what is stored.
    int kept = 0;
    auto put = [&](const char* field, const QString& value) {
        const QString key = acct_key(account_id, field);
        if (value.isEmpty()) {
            const SecretRead current = secure.read(key);
            if (current.failed() && current.error != SecretRead::Error::Corrupt) {
                ++kept;
                return;
            }
        }
        if (auto stored = secure.store(key, value); stored.is_err())
            LOG_ERROR("AccountManager", logf("Broker credential could not be saved",
                                             {{"account", log_mask(account_id)},
                                              {"field", field},
                                              {"error", QString::fromStdString(stored.error())}}));
    };
    put("api_key", creds.api_key);
    put("api_secret", creds.api_secret);
    put("access_token", creds.access_token);
    put("refresh_token", creds.refresh_token);
    put("user_id", creds.user_id);
    put("additional_data", creds.additional_data);
    if (kept > 0)
        LOG_WARN_RL("AccountManager", QStringLiteral("save_kept|") + account_id, 60000,
                    logf("Credential fields left as stored: they could not be read, so they are not cleared",
                         {{"account", log_mask(account_id)}, {"fields", kept}}));

    // The live token just changed. Notify listeners (DataStreamManager) so any
    // running stream is rebuilt with the new credentials: WebSocket adapters
    // capture the access token at construction and have no setter, so without a
    // rebuild they keep resolving/subscribing with the stale token (HTTP 401)
    // and never stream ticks — the cause of price data stalling after a daily
    // re-auth until the next slow REST poll happened to reload the new token.
    emit credentials_changed(account_id);
}

BrokerCredentials AccountManager::load_credentials(const QString& account_id, QString* read_error) const {
    BrokerCredentials creds;
    auto& secure = SecureStorage::instance();

    // Get broker_id from account
    {
        QMutexLocker lock(&mutex_);
        auto it = accounts_.find(account_id);
        if (it != accounts_.end())
            creds.broker_id = it->broker_id;
    }

    // A field that cannot be read stays empty as before, but is reported (read_error,
    // account Error state, one log line) instead of passing for "not set".
    QString failure;
    auto take = [&](const char* field, QString& dest) {
        const SecretRead r = secure.read(acct_key(account_id, field));
        if (r.found())
            dest = r.value;
        else if (r.failed() && failure.isEmpty())
            failure = am_cred_unreadable_text(r);
    };
    take("api_key", creds.api_key);
    take("api_secret", creds.api_secret);
    take("access_token", creds.access_token);
    take("refresh_token", creds.refresh_token);
    take("user_id", creds.user_id);
    take("additional_data", creds.additional_data);

    note_credential_read(account_id, failure);
    if (read_error)
        *read_error = failure;
    return creds;
}

QString AccountManager::credential_read_error(const QString& account_id) const {
    QMutexLocker lock(&mutex_);
    const auto f = cred_read_failed_.constFind(account_id);
    return f == cred_read_failed_.constEnd() ? QString() : f->shown_error;
}

void AccountManager::note_credential_read(const QString& account_id, const QString& failure) const {
    const bool failing = !failure.isEmpty();
    QString broker_id;
    CredReadFailure recovered;
    {
        QMutexLocker lock(&mutex_);
        if (!failing && cred_read_failed_.isEmpty())
            return; // the common case: nothing is failing
        const auto acct = accounts_.constFind(account_id);
        if (acct == accounts_.constEnd())
            return; // not a stored account (e.g. the built-in paper account)
        broker_id = acct->broker_id;
        auto f = cred_read_failed_.find(account_id);
        if (failing) {
            if (f != cred_read_failed_.end() && f->shown_error == failure && acct->state == ConnectionState::Error &&
                acct->error_message == failure)
                return; // already shown
            if (f == cred_read_failed_.end()) {
                CredReadFailure nf;
                nf.prev_state = acct->state;
                nf.prev_error = acct->error_message;
                f = cred_read_failed_.insert(account_id, nf);
            }
            f->shown_error = failure;
        } else {
            if (f == cred_read_failed_.end())
                return;
            recovered = f.value();
            cred_read_failed_.erase(f);
        }
    }
    am_log_cred_read(account_id, broker_id, failing);

    // The state change runs on the main thread (loads happen on workers too, and
    // listeners of connection_state_changed expect the GUI thread). It re-checks
    // the record first: a read that recovered meanwhile wins, and a state someone
    // else set since (a fresh login) is never overwritten by the restore.
    auto* self = const_cast<AccountManager*>(this);
    QMetaObject::invokeMethod(
        self,
        [self, account_id, failing, recovered]() {
            ConnectionState state = ConnectionState::Error;
            QString error;
            {
                QMutexLocker lock(&self->mutex_);
                const auto acct = self->accounts_.constFind(account_id);
                if (acct == self->accounts_.constEnd())
                    return;
                if (failing) {
                    const auto f = self->cred_read_failed_.constFind(account_id);
                    if (f == self->cred_read_failed_.constEnd())
                        return; // read again since
                    if (acct->state == ConnectionState::Error && acct->error_message == f->shown_error)
                        return;
                    error = f->shown_error;
                } else {
                    if (acct->state != ConnectionState::Error || acct->error_message != recovered.shown_error)
                        return; // changed since: leave it
                    state = recovered.prev_state;
                    error = recovered.prev_error;
                }
            }
            self->set_connection_state(account_id, state, error);
        },
        Qt::QueuedConnection);
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
    // user can reconnect without re-entering everything. An additional_data that
    // cannot be read is left exactly as stored (never rewritten from nothing).
    const SecretRead extra_r = secure.read(acct_key(account_id, "additional_data"));
    if (extra_r.found() && !extra_r.value.isEmpty()) {
        QJsonParseError parse_error;
        auto obj = QJsonDocument::fromJson(extra_r.value.toUtf8(), &parse_error).object();
        if (parse_error.error != QJsonParseError::NoError)
            LOG_ERROR("AccountManager", logf("Stored broker additional_data is not valid JSON",
                                             {{"account", log_mask(account_id)},
                                              {"error", parse_error.errorString()}}));
        obj.remove(QStringLiteral("token_expires_at"));
        secure.store(acct_key(account_id, "additional_data"),
                     QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }
    LOG_INFO("AccountManager", logf("Broker session token cleared", {{"account", log_mask(account_id)}}));
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
        QString read_error;
        auto creds = load_credentials(a.account_id, &read_error);
        if (!read_error.isEmpty())
            continue; // unreadable: shown as an error; never validated, refreshed or purged on
                      // partial credentials. The next sweep reads again.
        if (creds.access_token.isEmpty())
            continue; // never connected — nothing to validate
        work.push_back({a.account_id, a.broker_id, std::move(creds)});
    }
    if (work.isEmpty()) {
        sweeping_.store(false);
        return;
    }

    LOG_DEBUG("AccountManager", QString("Session sweep: validating %1 active account(s)").arg(work.size()));

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
                    LOG_DEBUG("AccountManager", QString("sweep[%1/%2]: INCONCLUSIVE — leaving state unchanged (%3)")
                                                   .arg(w.broker_id, w.account_id, v.detail.left(160)));
                    continue;
                }

                const bool valid = v.status == SessionCheck::Status::Valid;
                const qint64 stored_exp = token_expires_at_of(w.creds.additional_data);
                const bool near_expiry = stored_exp > 0 && (stored_exp - now) < 600; // <10 min
                const bool can_refresh = broker->supports_silent_refresh();
                LOG_DEBUG("AccountManager",
                          QString("sweep[%1/%2]: valid=%3 near_expiry=%4 can_refresh=%5 real_exp=%6 %7")
                             .arg(w.broker_id, w.account_id)
                             .arg(valid)
                             .arg(near_expiry)
                             .arg(can_refresh)
                             .arg(v.expires_at_epoch)
                             .arg(v.detail.left(120)));

                // Healthy and not about to lapse → confirm Connected, and write the
                // broker's real expiry back to storage if it differs from our hint
                // (stops the persisted token_expires_at from going stale).
                if (valid && !(near_expiry && can_refresh)) {
                    if (v.expires_at_epoch > 0 && v.expires_at_epoch != stored_exp) {
                        BrokerCredentials nc = w.creds;
                        nc.additional_data = with_token_expiry(nc.additional_data, v.expires_at_epoch);
                        LOG_DEBUG("AccountManager", QString("sweep[%1/%2]: refreshed stored expiry → %3")
                                                       .arg(w.broker_id, w.account_id)
                                                       .arg(v.expires_at_epoch));
                        outcomes.push_back({w.account_id, ConnectionState::Connected, true, nc, false, {}});
                    } else {
                        outcomes.push_back({w.account_id, ConnectionState::Connected, false, {}, false, {}});
                    }
                    continue;
                }

                // Expired, or valid-but-imminently-expiring with refresh available.
                if (can_refresh) {
                    LOG_DEBUG("AccountManager",
                              QString("sweep[%1/%2]: attempting silent refresh").arg(w.broker_id, w.account_id));
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
                        LOG_DEBUG("AccountManager",
                                  QString("sweep[%1/%2]: silent refresh OK → Connected").arg(w.broker_id, w.account_id));
                        outcomes.push_back({w.account_id, ConnectionState::Connected, true, nc, false, {}});
                        continue;
                    }
                    // Refresh failed. If the token is still valid (we were only
                    // pre-emptively refreshing), keep it Connected. If it is dead,
                    // purge the stale session so it stops being re-validated.
                    if (valid) {
                        outcomes.push_back({w.account_id, ConnectionState::Connected, false, {}, false, {}});
                    } else {
                        LOG_WARN("AccountManager",
                                 logf("Broker session expired and could not be refreshed; sign in again",
                                      {{"broker", w.broker_id},
                                       {"account", log_mask(w.account_id)},
                                       {"error", t.error.left(160)}}));
                        outcomes.push_back({w.account_id, ConnectionState::TokenExpired, false, {}, true, t.error});
                    }
                    continue;
                }

                // No silent refresh available → reflect the live answer; purge if dead.
                if (!valid) {
                    LOG_WARN("AccountManager", logf("Broker session expired; sign in again",
                                                    {{"broker", w.broker_id},
                                                     {"account", log_mask(w.account_id)},
                                                     {"error", v.detail.left(160)}}));
                    outcomes.push_back({w.account_id, ConnectionState::TokenExpired, false, {}, true, v.detail});
                }
            } catch (const std::exception& e) {
                LOG_ERROR("AccountManager", logf("Broker session check failed",
                                                 {{"broker", w.broker_id},
                                                  {"account", log_mask(w.account_id)},
                                                  {"error", e.what()}}));
            } catch (...) {
                LOG_ERROR("AccountManager", logf("Broker session check failed",
                                                 {{"broker", w.broker_id},
                                                  {"account", log_mask(w.account_id)},
                                                  {"error", "unknown exception"}}));
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
