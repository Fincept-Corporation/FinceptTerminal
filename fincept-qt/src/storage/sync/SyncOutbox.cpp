#include "storage/sync/SyncOutbox.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"
#include "storage/sync/CloudSyncSettings.h"

namespace fincept {
namespace {
// Per-thread re-entrancy guard depth. >0 means a pull is applying remote rows on
// this thread, so record() must be a no-op (otherwise the applied rows would be
// re-queued for push — an echo loop).
thread_local int g_outbox_apply_depth = 0;

// Engine-supplied drain trigger. Plain std::function keeps storage decoupled from
// the services layer.
std::function<void(QString)> g_outbox_notifier;
} // namespace

SyncOutbox::ApplyGuard::ApplyGuard() {
    ++g_outbox_apply_depth;
}

SyncOutbox::ApplyGuard::~ApplyGuard() {
    --g_outbox_apply_depth;
}

bool SyncOutbox::applying_remote() {
    return g_outbox_apply_depth > 0;
}

void SyncOutbox::set_notifier(std::function<void(QString)> cb) {
    g_outbox_notifier = std::move(cb);
}

Result<void> SyncOutbox::record(const QString& entity, const QString& local_id, const QString& op,
                                const QString& payload) {
    if (applying_remote())
        return Result<void>::ok(); // echo-loop guard
    if (!CloudSyncSettings::should_sync(entity))
        return Result<void>::ok(); // sync disabled or domain excluded

    auto r = Database::instance().execute(
        "INSERT INTO sync_outbox(entity, local_id, op, payload) VALUES(?, ?, ?, ?)",
        {entity, local_id, op, payload});
    if (r.is_err()) {
        LOG_ERROR("CloudSync", QString("outbox record failed: %1").arg(QString::fromStdString(r.error())));
        return Result<void>::err(r.error());
    }
    if (g_outbox_notifier)
        g_outbox_notifier(entity);
    return Result<void>::ok();
}

Result<void> SyncOutbox::record_unique(const QString& entity, const QString& local_id, const QString& op,
                                       const QString& payload) {
    if (applying_remote())
        return Result<void>::ok();
    if (!CloudSyncSettings::should_sync(entity))
        return Result<void>::ok();
    auto chk = Database::instance().execute(
        "SELECT 1 FROM sync_outbox WHERE entity = ? AND local_id = ? AND op = ? LIMIT 1", {entity, local_id, op});
    if (chk.is_ok() && chk.value().next())
        return Result<void>::ok(); // already pending → existing row reconciles latest state
    return record(entity, local_id, op, payload);
}

Result<QVector<OutboxRow>> SyncOutbox::pending_all(int limit) {
    auto r = Database::instance().execute(
        "SELECT id, entity, local_id, op, payload, attempts FROM sync_outbox ORDER BY id ASC LIMIT ?",
        {limit});
    if (r.is_err())
        return Result<QVector<OutboxRow>>::err(r.error());

    QVector<OutboxRow> rows;
    auto& q = r.value();
    while (q.next()) {
        OutboxRow row;
        row.id = q.value(0).toLongLong();
        row.entity = q.value(1).toString();
        row.local_id = q.value(2).toString();
        row.op = q.value(3).toString();
        row.payload = q.value(4).toString();
        row.attempts = q.value(5).toInt();
        rows.append(row);
    }
    return Result<QVector<OutboxRow>>::ok(std::move(rows));
}

Result<int> SyncOutbox::count(const QString& entity) {
    auto r = Database::instance().execute("SELECT COUNT(*) FROM sync_outbox WHERE entity = ?", {entity});
    if (r.is_err())
        return Result<int>::err(r.error());
    auto& q = r.value();
    const int n = q.next() ? q.value(0).toInt() : 0;
    return Result<int>::ok(n);
}

Result<void> SyncOutbox::mark_done(qint64 id) {
    auto r = Database::instance().execute("DELETE FROM sync_outbox WHERE id = ?", {id});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<void> SyncOutbox::bump_attempt(qint64 id, const QString& error) {
    auto r = Database::instance().execute(
        "UPDATE sync_outbox SET attempts = attempts + 1, last_error = ? WHERE id = ?", {error, id});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

} // namespace fincept
