#include "services/cloud/CloudSyncEngine.h"

#include "auth/AuthManager.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "network/cloud/CloudClient.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/SqliteSupport.h"
#include "storage/sync/CloudSyncSettings.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <memory>

namespace fincept::services::cloud {

namespace {
qint64 cloud_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}
constexpr qint64 kPullGateMs = 60'000;
constexpr int kDrainDebounceMs = 800;

// Outbox retry policy. A failing outbox used to be re-sent in full (up to 500
// requests) on every local edit, forever.
// A row past SyncOutbox::kMaxPushAttempts is dead-lettered: pending_all() no
// longer returns it (filtered in SQL) and StorageManager's retention sweep
// deletes it.
constexpr int kCloudOutboxPassLimit = 500;        // pushes attempted per pass (unchanged)
constexpr int kCloudOutboxMaxFailuresPerPass = 3; // a pass stops after this many failed requests
constexpr qint64 kCloudBackoffBaseMs = 30'000;     // after a failing pass: 30 s, 1, 2, 4 ... min
constexpr qint64 kCloudBackoffMaxMs = 30 * 60'000; // ... capped at 30 min

// Availability check: one cheap authenticated GET on a /v1 resource the cloud
// API serves (finceptgo implements it). api.fincept.in has no /v1 routes: 404.
constexpr const char* kCloudProbeEndpoint = "/watchlists";
// Device-local record of the pause already announced (see pause_unavailable).
constexpr const char* kCloudPauseNoticeKey = "cloud.pause_notice";

// The request failed for a reason that is not this row's fault (transport,
// auth, rate limit, server trouble): stop the pass, don't count an attempt.
// AdapterResult carries only the error token: CloudClient's "network_error" /
// "http_<status>", or the cloud envelope's error code.
bool cloud_failure_is_transient(const QString& error) {
    static const QSet<QString> kCloudTransientCodes = {
        QStringLiteral("network_error"),     QStringLiteral("unauthenticated"),  QStringLiteral("invalid_api_key"),
        QStringLiteral("session_expired"),   QStringLiteral("forbidden"),        QStringLiteral("rate_limited"),
        QStringLiteral("rate_limit_exceeded"), QStringLiteral("too_many_attempts"), QStringLiteral("internal_error"),
        QStringLiteral("gateway_error"),     QStringLiteral("upstream_error"),   QStringLiteral("auth_unavailable"),
        QStringLiteral("not_configured")};
    static const QRegularExpression kCloudTransientHttp(QStringLiteral("^http_(401|403|408|429|5\\d\\d)$"));
    return kCloudTransientCodes.contains(error) || kCloudTransientHttp.match(error).hasMatch();
}

bool cloud_failure_is_404(const QString& error) {
    return error == QLatin1String("http_404") || error == QLatin1String("not_found");
}

CloudSyncEngine::Availability cloud_classify_probe(const fincept::cloud::CloudResponse& r) {
    using A = CloudSyncEngine::Availability;
    // A real cloud API answers with its {success, data} envelope; a bare 2xx
    // (captive portal, proxy page) is not a sync service.
    if ((r.ok && (r.data.isArray() || r.data.isObject())) || r.is_insufficient_credits() || r.status == 402)
        return A::Available; // out of credits still means the service exists
    if (r.status == 404 || r.status == 405 || r.status == 501)
        return A::NotDeployed;
    if (r.is_auth_error())
        return A::AuthRejected;
    if (r.status == 0)
        return A::Unreachable;
    return A::ServerError;
}

// `enabling`: the answer to the user switching sync on (nothing was changed);
// otherwise the reason an already-enabled sync is paused.
QString cloud_availability_text(CloudSyncEngine::Availability a, int http_status, bool enabling) {
    using A = CloudSyncEngine::Availability;
    QString text;
    switch (a) {
        case A::Available:
            return {};
        case A::NotDeployed:
            text = enabling ? QCoreApplication::translate("CloudSyncEngine",
                                                          "Fincept Cloud Sync is not available on the server yet (the "
                                                          "cloud API answered HTTP %1). Nothing was changed; your data "
                                                          "stays on this device.")
                            : QCoreApplication::translate("CloudSyncEngine",
                                                          "Cloud sync is paused: the server does not offer Fincept Cloud "
                                                          "Sync (HTTP %1). Local saves are unaffected. It is checked "
                                                          "again at the next start or with Refresh now.");
            break;
        case A::AuthRejected:
            text = enabling ? QCoreApplication::translate("CloudSyncEngine",
                                                          "The Fincept Cloud service did not accept your sign-in (HTTP "
                                                          "%1). Nothing was changed. Sign in again, then retry.")
                            : QCoreApplication::translate("CloudSyncEngine",
                                                          "Cloud sync is paused: the cloud service did not accept your "
                                                          "sign-in (HTTP %1). Local saves are unaffected. Sign in again, "
                                                          "or press Refresh now.");
            break;
        case A::Unreachable:
            return QCoreApplication::translate("CloudSyncEngine", "Could not reach the Fincept Cloud service. Nothing "
                                                                  "was changed. Check your connection and try again.");
        case A::ServerError:
            text = QCoreApplication::translate("CloudSyncEngine", "The Fincept Cloud service is not answering normally "
                                                                  "(HTTP %1). Nothing was changed. Try again later.");
            break;
    }
    return text.arg(http_status);
}

// Bumped (GUI thread only) whenever sync is switched on or off, so a pre-sync
// snapshot that finishes after the user switched sync off — or off and on again —
// does not start a stale first-enable reconcile.
int s_cloud_enable_generation = 0;

struct CloudSnapshotResult {
    bool ok = false;
    QString path;    ///< the copy that was written
    QString message; ///< plain-language reason when !ok
};

// The pre-sync safety copy of the local database. Runs on a worker thread, on
// that thread's own connection: VACUUM INTO (db::backup_to) writes a consistent
// snapshot of the live WAL database, committed -wal content included. The old
// QFile::copy of the .db alone missed everything not yet checkpointed and could
// be torn by a concurrent checkpoint.
CloudSnapshotResult cloud_snapshot_local_db(const QString& db_path) {
    CloudSnapshotResult out;
    const QFileInfo fi(db_path);
    const QString base = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral("-precloud-") +
                         QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    QString backup = base + QStringLiteral(".db");
    for (int n = 2; QFile::exists(backup); ++n)
        backup = base + QStringLiteral("-%1.db").arg(n);

    QSqlDatabase conn = fincept::Database::instance().connection();
    fincept::db::SqlErrorKind kind = fincept::db::SqlErrorKind::None;
    const auto r = fincept::db::backup_to(conn, backup, &kind);
    if (r.is_ok()) {
        out.ok = true;
        out.path = backup;
    } else {
        // backup_to has logged the driver detail.
        out.message = fincept::db::user_message(kind == fincept::db::SqlErrorKind::None ? fincept::db::SqlErrorKind::Other
                                                                                        : kind,
                                                QStringLiteral("The terminal database"));
    }
    return out;
}
} // namespace

CloudSyncEngine& CloudSyncEngine::instance() {
    static CloudSyncEngine s;
    return s;
}

CloudSyncEngine::CloudSyncEngine() {
    qRegisterMetaType<CloudSyncEngine::Status>("fincept::services::cloud::CloudSyncEngine::Status");
}

void CloudSyncEngine::initialize() {
    if (initialized_)
        return;
    initialized_ = true;

    fincept::cloud::CloudClient::instance().set_base_url(fincept::AppConfig::instance().cloud_base_url());
    update_credentials();

    // Keep credentials fresh as the user logs in / out. A previously-enabled
    // session must also RESUME when the sign-in completes: initialize() runs while
    // the saved session is still being validated (AuthManager never trusts the
    // persisted `authenticated` flag), so its own resume check below almost always
    // sees "not signed in" and nothing would otherwise drain the backlog or pull
    // until the next local edit. Only the signed-out -> signed-in edge triggers it;
    // auth_state_changed also fires on every periodic profile refresh.
    auto was_signed_in = std::make_shared<bool>(can_enable());
    connect(&fincept::auth::AuthManager::instance(), &fincept::auth::AuthManager::auth_state_changed, this,
            [this, was_signed_in]() {
                update_credentials();
                const bool signed_in = can_enable();
                const bool just_signed_in = signed_in && !*was_signed_in;
                *was_signed_in = signed_in;
                if (just_signed_in && master_enabled()) {
                    LOG_INFO("CloudSync", "Signed in — resuming cloud sync");
                    resume_sync();
                }
            });

    // Debounced drain timer.
    drain_timer_ = new QTimer(this);
    drain_timer_->setSingleShot(true);
    drain_timer_->setInterval(kDrainDebounceMs);
    connect(drain_timer_, &QTimer::timeout, this, [this]() { drain(); });

    // Retry timer for a backlog left by a failing pass (see end_pass()).
    backoff_timer_ = new QTimer(this);
    backoff_timer_->setSingleShot(true);
    connect(backoff_timer_, &QTimer::timeout, this, [this]() { drain(); });

    // Outbox -> engine notifier. record() can run on a worker thread (repositories
    // use per-thread DB connections), so hop to the engine's (main) thread.
    SyncOutbox::set_notifier([](QString entity) {
        QMetaObject::invokeMethod(
            &CloudSyncEngine::instance(), [entity]() { CloudSyncEngine::instance().on_outbox_record(entity); },
            Qt::QueuedConnection);
    });

    LOG_DEBUG("CloudSync", QString("Engine initialised (base=%1, master=%2)")
                               .arg(fincept::cloud::CloudClient::instance().base_url())
                              .arg(master_enabled() ? "on" : "off"));

    // Resume a previously-enabled session: check the service, then drain any
    // backlog and refresh.
    if (master_enabled() && can_enable())
        resume_sync();
}

void CloudSyncEngine::register_adapter(CloudDomainAdapter* adapter) {
    if (!adapter)
        return;
    adapters_.insert(adapter->entity(), adapter);
}

CloudDomainAdapter* CloudSyncEngine::adapter_for(const QString& entity) const {
    return adapters_.value(entity, nullptr);
}

bool CloudSyncEngine::master_enabled() const {
    return CloudSyncSettings::master_enabled();
}

bool CloudSyncEngine::can_enable() const {
    const auto& s = fincept::auth::AuthManager::instance().session();
    return s.authenticated && !s.api_key.isEmpty();
}

void CloudSyncEngine::update_credentials() {
    const auto& s = fincept::auth::AuthManager::instance().session();
    if (s.authenticated && !s.api_key.isEmpty())
        fincept::cloud::CloudClient::instance().set_credentials(s.api_key, s.session_token);
    else
        fincept::cloud::CloudClient::instance().clear_credentials();
}

void CloudSyncEngine::set_master_enabled(bool on) {
    if (on && !can_enable()) {
        set_status(QString(), Status::Disabled, "sign_in_required");
        emit enable_finished(false, tr("Sign in to enable cloud sync."));
        return;
    }
    if (on) {
        if (master_enabled()) {
            emit enable_finished(true, {});
            return;
        }
        if (enable_pending_)
            return; // the pending check answers via enable_finished
        // Ask the service first. Enabling used to flip the flag, copy the whole
        // local DB to a "-precloud-" backup and queue every domain even though
        // api.fincept.in has no /v1 API — so every domain just showed http_404.
        enable_pending_ = true;
        probing_ = true;
        probe_service([this](Availability a, int http_status) {
            probing_ = false;
            if (!enable_pending_)
                return; // switched off again while the check ran
            enable_pending_ = false;
            if (a != Availability::Available) {
                const QString why = cloud_availability_text(a, http_status, /*enabling=*/true);
                LOG_WARN("CloudSync", "Cloud sync not enabled: " + why);
                emit enable_finished(false, why);
                return; // nothing changed: no flag, no snapshot, nothing queued
            }
            CloudSyncSettings::set_master_enabled(true);
            service_paused_ = false;
            service_message_.clear();
            clear_pause_notice();
            failed_passes_ = 0;
            next_drain_ms_ = 0;
            update_credentials();
            // Safety copy first (off the GUI thread); the first-enable reconcile
            // runs when it is done — see snapshot_local_db().
            snapshot_local_db();
            emit enable_finished(true, {});
        });
        return;
    }

    ++s_cloud_enable_generation; // a snapshot still running must not start a reconcile
    const bool enable_was_pending = enable_pending_;
    enable_pending_ = false;
    CloudSyncSettings::set_master_enabled(false);
    paused_credits_ = false;
    if (enable_was_pending)
        emit enable_finished(false, {});
    const bool was_paused = service_paused_;
    service_paused_ = false;
    service_message_.clear();
    clear_pause_notice();
    failed_passes_ = 0;
    next_drain_ms_ = 0;
    if (backoff_timer_)
        backoff_timer_->stop();
    for (auto it = adapters_.begin(); it != adapters_.end(); ++it)
        set_status(it.key(), Status::Disabled);
    if (was_paused)
        emit service_state_changed(false, {});
    LOG_INFO("CloudSync", "Cloud sync disabled (local cache retained)");
}

void CloudSyncEngine::probe_service(ProbeDone done) {
    update_credentials();
    fincept::cloud::CloudClient::instance().get(
        QString::fromLatin1(kCloudProbeEndpoint),
        [done = std::move(done)](fincept::cloud::CloudResponse resp) { done(cloud_classify_probe(resp), resp.status); },
        this);
}

void CloudSyncEngine::resume_sync() {
    if (!master_enabled() || !can_enable() || probing_)
        return;
    probing_ = true;
    probe_service([this](Availability a, int http_status) {
        probing_ = false;
        if (!master_enabled())
            return;
        if (a == Availability::NotDeployed || a == Availability::AuthRejected) {
            pause_unavailable(a, http_status);
            return;
        }
        // Available — or a transient outage, which the pushes' backoff absorbs.
        // A paused sync only resumes on a positive answer.
        if (a != Availability::Available && service_paused_)
            return;
        if (a == Availability::Available)
            clear_pause_notice();
        if (service_paused_) {
            service_paused_ = false;
            service_message_.clear();
            LOG_INFO("CloudSync", "Cloud service available again — sync resumed");
            emit service_state_changed(false, {});
        }
        paused_credits_ = false;
        schedule_drain();
        for (auto it = adapters_.begin(); it != adapters_.end(); ++it)
            request_pull(it.key(), /*force=*/true);
    });
}

void CloudSyncEngine::verify_service() {
    if (!master_enabled() || !can_enable() || probing_ || service_paused_)
        return;
    probing_ = true;
    probe_service([this](Availability a, int http_status) {
        probing_ = false;
        if (master_enabled() && (a == Availability::NotDeployed || a == Availability::AuthRejected))
            pause_unavailable(a, http_status);
    });
}

void CloudSyncEngine::pause_unavailable(Availability why, int http_status) {
    const QString message = cloud_availability_text(why, http_status, /*enabling=*/false);
    const bool was_paused = service_paused_;
    service_paused_ = true;
    service_message_ = message;
    if (drain_timer_)
        drain_timer_->stop();
    if (backoff_timer_)
        backoff_timer_->stop();
    for (auto it = adapters_.begin(); it != adapters_.end(); ++it)
        set_status(it.key(), Status::Unavailable, QStringLiteral("service_unavailable"));
    if (!was_paused) {
        LOG_WARN("CloudSync", "Sync paused: " + message);
        emit service_state_changed(true, message);
    }
    // Say so once — not on every start-up check. The reason is remembered
    // (device-local "cloud" category, never synced) until sync works again.
    const QString notice = QStringLiteral("%1:%2").arg(static_cast<int>(why)).arg(http_status);
    auto& repo = SettingsRepository::instance();
    const auto seen = repo.get(QString::fromLatin1(kCloudPauseNoticeKey));
    if (!pause_notice_shown_ && !(seen.is_ok() && seen.value() == notice)) {
        pause_notice_shown_ = true;
        repo.set(QString::fromLatin1(kCloudPauseNoticeKey), notice, QString::fromLatin1(CloudSyncSettings::kCategory));
        fincept::ui::ToastService::instance().post(fincept::ui::ToastService::Severity::Warning, message,
                                                   QStringLiteral("cloud_sync"));
    }
}

void CloudSyncEngine::clear_pause_notice() {
    pause_notice_shown_ = false;
    auto& repo = SettingsRepository::instance();
    const auto seen = repo.get(QString::fromLatin1(kCloudPauseNoticeKey));
    if (seen.is_ok() && !seen.value().isEmpty())
        repo.set(QString::fromLatin1(kCloudPauseNoticeKey), QString(), QString::fromLatin1(CloudSyncSettings::kCategory));
}

void CloudSyncEngine::set_domain_excluded(const QString& entity, bool excluded) {
    CloudSyncSettings::set_domain_excluded(entity, excluded);
    if (!excluded && master_enabled() && can_enable())
        request_pull(entity, /*force=*/true);
    else if (excluded)
        set_status(entity, Status::Disabled);
}

void CloudSyncEngine::on_outbox_record(const QString& entity) {
    Q_UNUSED(entity);
    if (master_enabled() && !paused_credits_ && !service_paused_)
        schedule_drain();
}

void CloudSyncEngine::schedule_drain() {
    // While backing off after a failing pass, local edits do not trigger a new
    // pass; the backoff timer drains the whole backlog when it expires.
    if (next_drain_ms_ > cloud_now_ms()) {
        if (backoff_timer_ && !backoff_timer_->isActive())
            backoff_timer_->start(static_cast<int>(next_drain_ms_ - cloud_now_ms()));
        return;
    }
    if (drain_timer_ && !drain_timer_->isActive())
        drain_timer_->start();
}

void CloudSyncEngine::drain() {
    // Signed out: every push would 401 and burn one of the row's limited retry
    // attempts (SyncOutbox::kMaxPushAttempts dead-letters it). Leave the backlog
    // untouched; the sign-in handler in initialize() re-schedules the drain.
    if (draining_ || paused_credits_ || service_paused_ || !master_enabled() || !can_enable())
        return;
    if (next_drain_ms_ > cloud_now_ms()) {
        schedule_drain(); // still backing off: re-arm the timer, don't push
        return;
    }
    // Dead-lettered rows (more than SyncOutbox::kMaxPushAttempts failures) are
    // filtered out in SQL, so the pass limit counts only rows still to push.
    // Local data is untouched; the retention sweep deletes the dead rows.
    auto r = SyncOutbox::pending_all(kCloudOutboxPassLimit);
    if (r.is_err()) {
        LOG_ERROR("CloudSync", QString("drain failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    drain_queue_ = std::move(r.value());
    drain_index_ = 0;
    pass_failures_ = 0;
    pass_successes_ = 0;
    pass_only_404_ = true;
    if (drain_queue_.isEmpty()) {
        failed_passes_ = 0; // nothing left to retry
        return;
    }
    draining_ = true;
    push_next();
}

void CloudSyncEngine::end_pass() {
    draining_ = false;
    drain_queue_.clear();
    if (pass_failures_ == 0) {
        failed_passes_ = 0;
        next_drain_ms_ = 0;
        return;
    }
    // Exponential backoff between failing passes: 30 s, 1, 2, 4 ... capped at
    // 30 min. A failing outbox now costs at most kCloudOutboxMaxFailuresPerPass
    // failed requests per pass instead of a full re-send on every local edit.
    ++failed_passes_;
    const qint64 delay = std::min(kCloudBackoffMaxMs, kCloudBackoffBaseMs << std::min(failed_passes_ - 1, 6));
    next_drain_ms_ = cloud_now_ms() + delay;
    if (backoff_timer_)
        backoff_timer_->start(static_cast<int>(delay));
    LOG_INFO("CloudSync", QString("Push pass: %1 failed, %2 ok — next attempt in %3 s")
                              .arg(pass_failures_)
                              .arg(pass_successes_)
                              .arg(delay / 1000));
    // Nothing got through and every failure was a 404: the cloud API may have
    // gone away. One availability check decides whether to pause.
    if (pass_successes_ == 0 && pass_only_404_)
        verify_service();
}

void CloudSyncEngine::push_next() {
    if (service_paused_ || !master_enabled()) {
        // Switched off / paused mid-pass: the in-flight push was still recorded;
        // stop here without arming a backoff.
        draining_ = false;
        drain_queue_.clear();
        return;
    }
    if (drain_index_ >= drain_queue_.size()) {
        end_pass();
        return;
    }
    const OutboxRow row = drain_queue_.at(drain_index_);
    CloudDomainAdapter* adapter = adapter_for(row.entity);
    if (!adapter) {
        // No adapter for this entity yet — skip, leave the row in the outbox.
        ++drain_index_;
        push_next();
        return;
    }
    set_status(row.entity, Status::Syncing);
    adapter->push(row, [this, row](AdapterResult res) {
        if (res.credits_exhausted) {
            paused_credits_ = true;
            draining_ = false;
            drain_queue_.clear();
            set_status(row.entity, Status::Paused, "insufficient_credits");
            emit credits_exhausted();
            LOG_WARN("CloudSync", "Push paused — insufficient credits");
            return;
        }
        if (res.ok) {
            SyncOutbox::mark_done(row.id);
            set_status(row.entity, Status::Idle);
            ++pass_successes_;
        } else if (cloud_failure_is_transient(res.error)) {
            // Transport / auth / rate limit / server trouble: not this row's
            // fault, so no attempt is counted. Stop the pass and back off.
            set_status(row.entity, Status::Error, res.error);
            ++pass_failures_;
            pass_only_404_ = false;
            end_pass();
            return;
        } else {
            // Leave the row in the outbox; it retries on a later pass until it
            // passes SyncOutbox::kMaxPushAttempts.
            SyncOutbox::bump_attempt(row.id, res.error);
            set_status(row.entity, Status::Error, res.error);
            // "awaiting_create" made no request (its create has not synced yet).
            if (res.error != QLatin1String("awaiting_create")) {
                ++pass_failures_;
                if (!cloud_failure_is_404(res.error))
                    pass_only_404_ = false;
                if (pass_failures_ >= kCloudOutboxMaxFailuresPerPass) {
                    end_pass();
                    return;
                }
            }
        }
        ++drain_index_;
        push_next();
    });
}

void CloudSyncEngine::request_pull(const QString& entity, bool force) {
    if (!master_enabled() || !can_enable() || service_paused_)
        return;
    if (!CloudSyncSettings::should_sync(entity))
        return;
    CloudDomainAdapter* adapter = adapter_for(entity);
    if (!adapter)
        return;

    const qint64 now = cloud_now_ms();
    if (!force && last_pull_ms_.value(entity, 0) + kPullGateMs > now)
        return; // rate-gated
    last_pull_ms_[entity] = now;

    // Push-before-pull so newer local edits win LWW. drain() is async; for v1 we
    // kick it then pull (P1 can chain strictly once tested end-to-end).
    schedule_drain();
    set_status(entity, Status::Syncing);
    adapter->pull([this, entity](bool ok, QString error) {
        if (ok) {
            set_status(entity, Status::Idle);
            emit cloud_data_changed(entity);
        } else {
            set_status(entity, Status::Error, error);
        }
    });
}

void CloudSyncEngine::refresh_all() {
    // Manual Refresh is also the way out of a pause: it re-checks the service
    // and, when it answers, drains the backlog (no backoff wait — the user
    // asked) and pulls every domain.
    next_drain_ms_ = 0;
    resume_sync();
}

void CloudSyncEngine::resolve_first_enable(const QString& entity, bool upload_merge) {
    CloudDomainAdapter* adapter = adapter_for(entity);
    if (!adapter)
        return;
    if (upload_merge) {
        adapter->enqueue_all_local(); // push local up; cloud rows already present → server merges
        schedule_drain();
    } else {
        request_pull(entity, /*force=*/true); // use cloud (local already snapshotted)
    }
}

void CloudSyncEngine::perform_first_enable() {
    // Runs after snapshot_local_db() has finished (successfully or not).
    if (adapters_.isEmpty()) {
        LOG_INFO("CloudSync", "Cloud sync enabled (no adapters registered yet)");
        return;
    }
    for (auto it = adapters_.begin(); it != adapters_.end(); ++it) {
        CloudDomainAdapter* adapter = it.value();
        const QString entity = it.key();
        const bool local_has = adapter->local_count() > 0;
        set_status(entity, Status::Syncing);
        adapter->remote_count([this, entity, adapter, local_has](bool ok, int count) {
            if (!ok) {
                set_status(entity, Status::Error, "remote_count_failed");
                return;
            }
            const bool remote_has = count > 0;
            if (!remote_has) {
                adapter->enqueue_all_local(); // cloud empty → upload local
                schedule_drain();
            } else if (!local_has) {
                request_pull(entity, /*force=*/true); // local empty → pull cloud
            } else {
                emit first_enable_conflict(entity); // both → UI decides
            }
        });
    }
}

void CloudSyncEngine::snapshot_local_db() {
    // The safety copy taken before first-enable merges cloud data into the local
    // database. It is written on a worker thread (VACUUM INTO can take seconds on
    // a large database) and the reconcile starts only when it is finished, so the
    // copy holds the pre-sync state. A failed copy is logged and shown to the
    // user; sync continues, as before, but the user now knows there is no copy.
    const int generation = ++s_cloud_enable_generation;
    const QString path = fincept::Database::instance().path();
    if (path.isEmpty()) {
        LOG_WARN("CloudSync", "No local database is open — no pre-sync snapshot taken");
        perform_first_enable();
        return;
    }
    (void)QtConcurrent::run([this, path, generation]() {
        const CloudSnapshotResult r = cloud_snapshot_local_db(path);
        QMetaObject::invokeMethod(
            this,
            [this, r, generation]() {
                if (r.ok) {
                    LOG_INFO("CloudSync", "Local DB snapshot saved: " + r.path);
                } else {
                    LOG_WARN("CloudSync", "Local DB snapshot failed (continuing without one): " + r.message);
                    fincept::ui::ToastService::instance().post(
                        fincept::ui::ToastService::Severity::Warning,
                        tr("Cloud sync is on, but the safety copy of your local data could not be made. %1")
                            .arg(r.message),
                        QStringLiteral("cloud_sync"));
                }
                if (generation != s_cloud_enable_generation || !master_enabled()) {
                    LOG_INFO("CloudSync", "Sync was switched off while the snapshot ran — first-enable skipped");
                    return;
                }
                perform_first_enable();
            },
            Qt::QueuedConnection);
    });
}

void CloudSyncEngine::set_status(const QString& entity, Status s, const QString& error) {
    if (!entity.isEmpty())
        status_[entity] = s;
    int pending = 0;
    if (!entity.isEmpty()) {
        auto c = SyncOutbox::count(entity);
        pending = c.is_ok() ? c.value() : 0;
    }
    emit status_changed(entity, s, pending, error);
}

CloudSyncEngine::Status CloudSyncEngine::status(const QString& entity) const {
    return status_.value(entity, master_enabled() ? Status::Idle : Status::Disabled);
}

int CloudSyncEngine::pending_count(const QString& entity) const {
    auto c = SyncOutbox::count(entity);
    return c.is_ok() ? c.value() : 0;
}

} // namespace fincept::services::cloud
