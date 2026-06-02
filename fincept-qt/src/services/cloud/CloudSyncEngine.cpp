#include "services/cloud/CloudSyncEngine.h"

#include "auth/AuthManager.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "network/cloud/CloudClient.h"
#include "storage/sqlite/Database.h"
#include "storage/sync/CloudSyncSettings.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMetaType>
#include <QTimer>

namespace fincept::services::cloud {

namespace {
qint64 cloud_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}
constexpr qint64 kPullGateMs = 60'000;
constexpr int kDrainDebounceMs = 800;
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

    // Keep credentials fresh as the user logs in / out.
    connect(&fincept::auth::AuthManager::instance(), &fincept::auth::AuthManager::auth_state_changed, this,
            [this]() { update_credentials(); });

    // Debounced drain timer.
    drain_timer_ = new QTimer(this);
    drain_timer_->setSingleShot(true);
    drain_timer_->setInterval(kDrainDebounceMs);
    connect(drain_timer_, &QTimer::timeout, this, [this]() { drain(); });

    // Outbox -> engine notifier. record() can run on a worker thread (repositories
    // use per-thread DB connections), so hop to the engine's (main) thread.
    SyncOutbox::set_notifier([](QString entity) {
        QMetaObject::invokeMethod(
            &CloudSyncEngine::instance(), [entity]() { CloudSyncEngine::instance().on_outbox_record(entity); },
            Qt::QueuedConnection);
    });

    LOG_INFO("CloudSync", QString("Engine initialised (base=%1, master=%2)")
                              .arg(fincept::cloud::CloudClient::instance().base_url())
                              .arg(master_enabled() ? "on" : "off"));

    // Resume a previously-enabled session: drain any backlog and refresh.
    if (master_enabled() && can_enable()) {
        schedule_drain();
        refresh_all();
    }
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
        return;
    }
    CloudSyncSettings::set_master_enabled(on);
    if (on) {
        update_credentials();
        perform_first_enable();
    } else {
        paused_credits_ = false;
        for (auto it = adapters_.begin(); it != adapters_.end(); ++it)
            set_status(it.key(), Status::Disabled);
        LOG_INFO("CloudSync", "Cloud sync disabled (local cache retained)");
    }
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
    if (master_enabled() && !paused_credits_)
        schedule_drain();
}

void CloudSyncEngine::schedule_drain() {
    if (drain_timer_ && !drain_timer_->isActive())
        drain_timer_->start();
}

void CloudSyncEngine::drain() {
    if (draining_ || paused_credits_ || !master_enabled())
        return;
    auto r = SyncOutbox::pending_all();
    if (r.is_err()) {
        LOG_ERROR("CloudSync", QString("drain failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    drain_queue_ = r.value();
    drain_index_ = 0;
    if (drain_queue_.isEmpty())
        return;
    draining_ = true;
    push_next();
}

void CloudSyncEngine::push_next() {
    if (drain_index_ >= drain_queue_.size()) {
        draining_ = false;
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
            set_status(row.entity, Status::Paused, "insufficient_credits");
            emit credits_exhausted();
            LOG_WARN("CloudSync", "Push paused — insufficient credits");
            return;
        }
        if (res.ok) {
            SyncOutbox::mark_done(row.id);
            set_status(row.entity, Status::Idle);
        } else {
            SyncOutbox::bump_attempt(row.id, res.error);
            set_status(row.entity, Status::Error, res.error);
            // leave the row in the outbox; it retries on the next drain
        }
        ++drain_index_;
        push_next();
    });
}

void CloudSyncEngine::request_pull(const QString& entity, bool force) {
    if (!master_enabled() || !can_enable())
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
    paused_credits_ = false;
    for (auto it = adapters_.begin(); it != adapters_.end(); ++it)
        request_pull(it.key(), /*force=*/true);
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
    snapshot_local_db();
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
    const QString path = fincept::Database::instance().path();
    if (path.isEmpty())
        return;
    QFileInfo fi(path);
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    const QString backup = fi.absolutePath() + "/" + fi.completeBaseName() + "-precloud-" + stamp + ".db";
    if (QFile::copy(path, backup))
        LOG_INFO("CloudSync", "Local DB snapshot saved: " + backup);
    else
        LOG_WARN("CloudSync", "Local DB snapshot failed (continuing): " + backup);
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
