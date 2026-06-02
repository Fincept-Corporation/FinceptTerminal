#pragma once
#include "services/cloud/CloudDomainAdapter.h"
#include "storage/sync/SyncOutbox.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace fincept::services::cloud {

/// Orchestrates Fincept Cloud mirror sync: drains the durable outbox (push) and
/// pulls cloud→local on demand. QObject singleton; deliberately NOT a DataHub
/// producer (user-document CRUD reads come from the local repo cache, not a hub
/// topic). See fincept-qt/CLOUD_SYNC_PLAN.md.
class CloudSyncEngine : public QObject {
    Q_OBJECT
  public:
    enum class Status { Disabled, Idle, Syncing, Error, Paused /* credits */ };
    Q_ENUM(Status)

    static CloudSyncEngine& instance();

    /// Idempotent. Call once during deferred startup (after DB open + auth init).
    /// In P1+, register adapters BEFORE calling this so the startup backlog drain
    /// and refresh see them.
    void initialize();

    /// Domain adapters self-register here (P1+). The engine is otherwise
    /// domain-agnostic.
    void register_adapter(CloudDomainAdapter* adapter);

    bool master_enabled() const;
    bool can_enable() const; // authenticated && has api_key

    // ── User actions (Settings UI) ──
    void set_master_enabled(bool on);
    void set_domain_excluded(const QString& entity, bool excluded);
    void refresh_all();                                           // manual Refresh
    void request_pull(const QString& entity, bool force = false); // screen-entry / manual
    void resolve_first_enable(const QString& entity, bool upload_merge); // answer first_enable_conflict

    Status status(const QString& entity) const;
    int pending_count(const QString& entity) const;

    /// Invoked (queued, on the engine's thread) by the SyncOutbox notifier after a
    /// successful record(). Public only so the free-function notifier can reach it.
    void on_outbox_record(const QString& entity);

  signals:
    void status_changed(QString entity, fincept::services::cloud::CloudSyncEngine::Status status, int pending,
                        QString error);
    void cloud_data_changed(QString entity);    // open screens should reload
    void credits_exhausted();                   // sync paused until top-up / refresh
    void first_enable_conflict(QString entity); // both local & cloud have data → UI prompts

  private:
    CloudSyncEngine();
    Q_DISABLE_COPY(CloudSyncEngine)

    void update_credentials();
    void schedule_drain();
    void drain();
    void push_next();
    void perform_first_enable();
    void snapshot_local_db();
    void set_status(const QString& entity, Status s, const QString& error = {});
    CloudDomainAdapter* adapter_for(const QString& entity) const;

    QHash<QString, CloudDomainAdapter*> adapters_;
    QHash<QString, Status> status_;
    QHash<QString, qint64> last_pull_ms_;

    QTimer* drain_timer_ = nullptr;
    QVector<OutboxRow> drain_queue_;
    int drain_index_ = 0;
    bool draining_ = false;
    bool paused_credits_ = false;
    bool initialized_ = false;
};

} // namespace fincept::services::cloud
