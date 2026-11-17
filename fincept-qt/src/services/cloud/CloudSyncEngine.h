#pragma once
#include "services/cloud/CloudDomainAdapter.h"
#include "storage/sync/SyncOutbox.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class QTimer;

namespace fincept::services::cloud {

/// Orchestrates Fincept Cloud mirror sync: drains the durable outbox (push) and
/// pulls cloud→local on demand. QObject singleton; deliberately NOT a DataHub
/// producer (user-document CRUD reads come from the local repo cache, not a hub
/// topic). See fincept-qt/CLOUD_SYNC_PLAN.md.
class CloudSyncEngine : public QObject {
    Q_OBJECT
  public:
    enum class Status { Disabled, Idle, Syncing, Error, Paused /* credits */, Unavailable /* no cloud service */ };
    Q_ENUM(Status)

    /// Outcome of the one-request availability check run before sync is
    /// switched on and whenever an enabled sync resumes.
    enum class Availability { Available, NotDeployed, AuthRejected, Unreachable, ServerError };

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

    /// set_master_enabled(true) is waiting for its availability check.
    bool enable_in_progress() const { return enable_pending_; }
    /// Sync is enabled but paused because the cloud service is not there (or
    /// refused the credentials). Local saves are unaffected. Cleared by a
    /// successful check: next start-up, sign-in, or refresh_all().
    bool service_paused() const { return service_paused_; }
    QString service_message() const { return service_message_; }

    // ── User actions (Settings UI) ──
    /// Turning sync ON first asks the cloud service one cheap authenticated
    /// question; only a working service enables it (then the local DB snapshot
    /// and first-enable reconcile run). The answer arrives as enable_finished().
    void set_master_enabled(bool on);
    void set_domain_excluded(const QString& entity, bool excluded);
    void refresh_all();                                                  // manual Refresh
    void request_pull(const QString& entity, bool force = false);        // screen-entry / manual
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
    /// Answer to set_master_enabled(true): enabled, or refused with the reason
    /// (refused = nothing changed: no flag, no snapshot, no requests queued).
    void enable_finished(bool enabled, QString message);
    /// Enabled sync paused / resumed because of the cloud service's availability.
    void service_state_changed(bool paused, QString message);

  private:
    CloudSyncEngine();
    Q_DISABLE_COPY(CloudSyncEngine)

    using ProbeDone = std::function<void(Availability, int http_status)>;

    void update_credentials();
    void probe_service(ProbeDone done);
    void resume_sync();    // probe, then drain + pull everything (startup, sign-in, Refresh)
    void verify_service(); // probe only; pauses when the service has gone
    void pause_unavailable(Availability why, int http_status);
    void clear_pause_notice();
    void schedule_drain();
    void drain();
    void push_next();
    void end_pass();
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

    // Availability (see set_master_enabled / resume_sync).
    bool probing_ = false;
    bool enable_pending_ = false; // an enable is waiting on its check (cleared by switching off)
    bool service_paused_ = false;
    bool pause_notice_shown_ = false; // pause announced (also remembered across restarts in settings)
    QString service_message_;

    // Push-pass accounting for the retry backoff (see end_pass).
    QTimer* backoff_timer_ = nullptr;
    int failed_passes_ = 0;     // consecutive passes that had a failed request
    qint64 next_drain_ms_ = 0;  // no drain before this time (0 = no backoff)
    int pass_failures_ = 0;     // failed requests in the current pass
    int pass_successes_ = 0;    // successful pushes in the current pass
    bool pass_only_404_ = true; // every failure in this pass was a 404
};

} // namespace fincept::services::cloud
