#include "storage/workspace/CrashRecovery.h"

#include "core/logging/Logger.h"
#include "storage/workspace/WorkspaceDb.h"

#include <QDateTime>
#include <QSqlQuery>

namespace fincept {

namespace {
constexpr const char* kCrashRecoveryTag = "CrashRecovery";
constexpr const char* kKeyCleanShutdown = "last_clean_shutdown_at";
} // namespace

CrashRecovery::CrashRecovery(WorkspaceDb* db, WorkspaceSnapshotRing* ring)
    : db_(db), ring_(ring) {}

bool CrashRecovery::needs_recovery() const {
    if (!db_ || !db_->is_open())
        return false;

    const qint64 latest_auto_ms = latest_auto_snapshot_ms();
    if (latest_auto_ms == 0) {
        // No auto snapshots — nothing to recover from. First run (or wipe).
        LOG_DEBUG(kCrashRecoveryTag, "No auto snapshots — no recovery needed");
        return false;
    }

    const qint64 clean_ms = last_clean_shutdown_ms();
    if (clean_ms == 0) {
        // Marker missing entirely — definitive unclean shutdown.
        LOG_INFO(kCrashRecoveryTag, QString("Recovery needed: no clean-shutdown marker, latest auto=%1")
                              .arg(QDateTime::fromMSecsSinceEpoch(latest_auto_ms).toString(Qt::ISODate)));
        return true;
    }

    // Clean marker exists but is older than the latest auto snapshot —
    // means the user was working past the last clean shutdown and then
    // crashed. Recovery is warranted.
    if (clean_ms < latest_auto_ms) {
        LOG_INFO(kCrashRecoveryTag, QString("Recovery needed: clean=%1 < latest auto=%2")
                              .arg(QDateTime::fromMSecsSinceEpoch(clean_ms).toString(Qt::ISODate))
                              .arg(QDateTime::fromMSecsSinceEpoch(latest_auto_ms).toString(Qt::ISODate)));
        return true;
    }

    LOG_DEBUG(kCrashRecoveryTag, "Last shutdown was clean — no recovery needed");
    return false;
}

Result<QList<WorkspaceSnapshotRing::Entry>>
CrashRecovery::latest_snapshots(int limit) const {
    if (!ring_)
        return Result<QList<WorkspaceSnapshotRing::Entry>>::err("Ring not configured");
    // Combine auto + crash_recovery, leave named_save out (the recovery
    // dialog distinguishes them). Cheap path: fetch from latest() and
    // filter, since limit is small.
    auto all = ring_->latest(limit * 2);
    if (all.is_err())
        return all;

    QList<WorkspaceSnapshotRing::Entry> filtered;
    for (const auto& e : all.value()) {
        if (e.kind == "auto" || e.kind == "crash_recovery")
            filtered.append(e);
        if (filtered.size() >= limit)
            break;
    }
    return Result<QList<WorkspaceSnapshotRing::Entry>>::ok(filtered);
}

Result<void> CrashRecovery::mark_clean_shutdown() {
    if (!db_ || !db_->is_open())
        return Result<void>::err("WorkspaceDb not open");
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    auto r = db_->set_meta(kKeyCleanShutdown, QString::number(now_ms));
    if (r.is_err()) {
        LOG_WARN(kCrashRecoveryTag, QString("Failed to write clean-shutdown marker: %1")
                              .arg(QString::fromStdString(r.error())));
    } else {
        LOG_INFO(kCrashRecoveryTag, "Clean-shutdown marker written");
    }
    return r;
}

qint64 CrashRecovery::latest_auto_snapshot_ms() const {
    if (!db_ || !db_->is_open())
        return 0;
    auto r = db_->execute(
        "SELECT MAX(created_at) FROM workspace_snapshot WHERE kind = 'auto'");
    if (r.is_err())
        return 0;
    QSqlQuery q = std::move(r.value());
    if (!q.next())
        return 0;
    const QVariant v = q.value(0);
    return v.isNull() ? 0 : v.toLongLong();
}

qint64 CrashRecovery::last_clean_shutdown_ms() const {
    if (!db_ || !db_->is_open())
        return 0;
    const QString s = db_->meta(kKeyCleanShutdown);
    if (s.isEmpty())
        return 0;
    bool ok = false;
    const qint64 ms = s.toLongLong(&ok);
    return ok ? ms : 0;
}

} // namespace fincept
