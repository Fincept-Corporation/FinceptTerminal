#include "storage/workspace/WorkspaceSnapshotRing.h"

#include "core/logging/Logger.h"
#include "storage/workspace/WorkspaceDb.h"

#include <QSqlQuery>

namespace fincept {

namespace {
constexpr const char* kSnapshotRingTag = "SnapshotRing";
} // namespace

WorkspaceSnapshotRing::WorkspaceSnapshotRing(WorkspaceDb* db,
                                             int max_auto,
                                             qint64 min_interval_ms)
    : db_(db), max_auto_(max_auto), min_interval_ms_(min_interval_ms) {}

Result<qint64> WorkspaceSnapshotRing::add(const QByteArray& payload,
                                          const QString& kind,
                                          bool force) {
    if (!db_ || !db_->is_open())
        return Result<qint64>::err("WorkspaceDb not open");
    if (payload.isEmpty())
        return Result<qint64>::err("Empty snapshot payload");

    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();

    // Rate limit auto writes. Named/crash kinds always go through.
    if (kind == "auto" && !force) {
        if (last_auto_ms_ != 0 && (now_ms - last_auto_ms_) < min_interval_ms_) {
            // Suppressed by rate limit. Return ok() with id=0 so the caller
            // can distinguish from SQL failures. Logging this would be very
            // chatty during normal use, so we don't.
            return Result<qint64>::ok(0);
        }
    }

    auto r = db_->execute(
        "INSERT INTO workspace_snapshot(created_at, kind, payload, name) VALUES(?, ?, ?, NULL)",
        {now_ms, kind, payload});
    if (r.is_err())
        return Result<qint64>::err(r.error());

    const QVariant id_var = r.value().lastInsertId();
    const qint64 id = id_var.toLongLong();

    if (kind == "auto") {
        last_auto_ms_ = now_ms;
        // Trim to ring size. Failure here is logged but not fatal — the
        // snapshot is still committed; we just have an over-full ring
        // until the next attempt.
        auto trim = trim_auto();
        if (trim.is_err()) {
            LOG_WARN(kSnapshotRingTag, QString("trim_auto failed: %1")
                                  .arg(QString::fromStdString(trim.error())));
        }
    }
    return Result<qint64>::ok(id);
}

Result<qint64> WorkspaceSnapshotRing::add_named(const QByteArray& payload, const QString& name) {
    if (!db_ || !db_->is_open())
        return Result<qint64>::err("WorkspaceDb not open");
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    auto r = db_->execute(
        "INSERT INTO workspace_snapshot(created_at, kind, payload, name) VALUES(?, 'named_save', ?, ?)",
        {now_ms, payload, name});
    if (r.is_err())
        return Result<qint64>::err(r.error());
    return Result<qint64>::ok(r.value().lastInsertId().toLongLong());
}

Result<qint64> WorkspaceSnapshotRing::add_crash_recovery(const QByteArray& payload) {
    return add(payload, "crash_recovery", /*force=*/true);
}

Result<QList<WorkspaceSnapshotRing::Entry>>
WorkspaceSnapshotRing::latest(int limit) const {
    if (!db_ || !db_->is_open())
        return Result<QList<Entry>>::err("WorkspaceDb not open");
    auto r = db_->execute(
        "SELECT snapshot_id, created_at, kind, COALESCE(name, '') "
        "FROM workspace_snapshot ORDER BY created_at DESC LIMIT ?",
        {limit});
    if (r.is_err())
        return Result<QList<Entry>>::err(r.error());

    QList<Entry> out;
    QSqlQuery q = std::move(r.value());
    while (q.next()) {
        Entry e;
        e.snapshot_id = q.value(0).toLongLong();
        e.created_at_ms = q.value(1).toLongLong();
        e.kind = q.value(2).toString();
        e.name = q.value(3).toString();
        out.append(e);
    }
    return Result<QList<Entry>>::ok(out);
}

Result<QList<WorkspaceSnapshotRing::Entry>>
WorkspaceSnapshotRing::latest_auto(int limit) const {
    if (!db_ || !db_->is_open())
        return Result<QList<Entry>>::err("WorkspaceDb not open");
    auto r = db_->execute(
        "SELECT snapshot_id, created_at, kind, COALESCE(name, '') "
        "FROM workspace_snapshot WHERE kind = 'auto' "
        "ORDER BY created_at DESC LIMIT ?",
        {limit});
    if (r.is_err())
        return Result<QList<Entry>>::err(r.error());

    QList<Entry> out;
    QSqlQuery q = std::move(r.value());
    while (q.next()) {
        Entry e;
        e.snapshot_id = q.value(0).toLongLong();
        e.created_at_ms = q.value(1).toLongLong();
        e.kind = q.value(2).toString();
        e.name = q.value(3).toString();
        out.append(e);
    }
    return Result<QList<Entry>>::ok(out);
}

Result<QByteArray> WorkspaceSnapshotRing::load_payload(qint64 snapshot_id) const {
    if (!db_ || !db_->is_open())
        return Result<QByteArray>::err("WorkspaceDb not open");
    auto r = db_->execute("SELECT payload FROM workspace_snapshot WHERE snapshot_id = ?",
                          {snapshot_id});
    if (r.is_err())
        return Result<QByteArray>::err(r.error());

    QSqlQuery q = std::move(r.value());
    if (!q.next())
        return Result<QByteArray>::err("snapshot_id not found");
    return Result<QByteArray>::ok(q.value(0).toByteArray());
}

Result<void> WorkspaceSnapshotRing::erase(qint64 snapshot_id) {
    if (!db_ || !db_->is_open())
        return Result<void>::err("WorkspaceDb not open");
    auto r = db_->execute("DELETE FROM workspace_snapshot WHERE snapshot_id = ?", {snapshot_id});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

bool WorkspaceSnapshotRing::would_throttle_auto() const {
    if (last_auto_ms_ == 0)
        return false;
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    return (now_ms - last_auto_ms_) < min_interval_ms_;
}

Result<void> WorkspaceSnapshotRing::trim_auto() {
    if (!db_ || !db_->is_open())
        return Result<void>::err("WorkspaceDb not open");

    // Single statement: delete auto rows whose created_at falls below the
    // (max_auto_)th newest. Subquery approach is portable and lets SQLite
    // index-walk the (kind, created_at) idx without temp table.
    auto r = db_->execute(
        "DELETE FROM workspace_snapshot "
        "WHERE kind = 'auto' AND snapshot_id NOT IN ("
        "  SELECT snapshot_id FROM workspace_snapshot "
        "  WHERE kind = 'auto' ORDER BY created_at DESC LIMIT ?"
        ")",
        {max_auto_});
    if (r.is_err())
        return Result<void>::err(r.error());

    const int removed = r.value().numRowsAffected();
    if (removed > 0)
        LOG_DEBUG(kSnapshotRingTag, QString("Trimmed %1 auto snapshot(s)").arg(removed));
    return Result<void>::ok();
}

} // namespace fincept
