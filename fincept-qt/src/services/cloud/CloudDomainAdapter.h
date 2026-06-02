#pragma once
#include "storage/sync/SyncOutbox.h"

#include <QString>

#include <functional>

namespace fincept::services::cloud {

/// Outcome of an adapter push.
struct AdapterResult {
    bool ok = false;                // pushed successfully (outbox row may be retired)
    bool retry = false;             // transient failure — keep the row, retry later
    bool credits_exhausted = false; // pause all sync until the user tops up / refreshes
    QString error;
};

using PushDone = std::function<void(AdapterResult)>;
using PullDone = std::function<void(bool ok, QString error)>;
using CountDone = std::function<void(bool ok, int count)>;

/// One adapter per syncable domain. Translates between local repository rows and
/// the cloud /v1 endpoints, and owns the sync_map bookkeeping for its entity.
///
/// Implementations live in P1+ (WatchlistCloudAdapter, NotesCloudAdapter,
/// PortfolioCloudAdapter). The engine is otherwise domain-agnostic.
class CloudDomainAdapter {
  public:
    virtual ~CloudDomainAdapter() = default;

    /// Entity tag used in sync_outbox / sync_map (e.g. "watchlist").
    virtual QString entity() const = 0;

    /// Push a single outbox row. On success, upsert sync_map(local_id <-> remote_id).
    virtual void push(const OutboxRow& row, PushDone done) = 0;

    /// Pull the full cloud collection into the local cache. MUST wrap local writes
    /// in `SyncOutbox::ApplyGuard` (no echo loop), reconcile via sync_map, and apply
    /// last-write-wins by updated_at.
    virtual void pull(PullDone done) = 0;

    /// Number of LOCAL rows for this domain (for first-enable empty/conflict checks).
    virtual int local_count() const = 0;

    /// Number of REMOTE rows (async, one GET) for first-enable checks.
    virtual void remote_count(CountDone done) = 0;

    /// Enqueue every existing local row into the outbox as a create — used by the
    /// first-enable "upload local" path when the cloud account is empty.
    virtual void enqueue_all_local() = 0;
};

} // namespace fincept::services::cloud
