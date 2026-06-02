#include "storage/sync/SyncMap.h"

#include "storage/sqlite/Database.h"

namespace fincept {

std::optional<QString> SyncMap::remote_id(const QString& entity, const QString& local_id) {
    auto r = Database::instance().execute("SELECT remote_id FROM sync_map WHERE entity = ? AND local_id = ?",
                                          {entity, local_id});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return q.value(0).toString();
}

std::optional<QString> SyncMap::local_id(const QString& entity, const QString& remote_id) {
    auto r = Database::instance().execute("SELECT local_id FROM sync_map WHERE entity = ? AND remote_id = ?",
                                          {entity, remote_id});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return q.value(0).toString();
}

Result<void> SyncMap::put(const QString& entity, const QString& local_id, const QString& remote_id,
                          const QString& remote_updated_at) {
    auto r = Database::instance().execute(
        "INSERT INTO sync_map(entity, local_id, remote_id, remote_updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(entity, local_id) DO UPDATE SET remote_id = excluded.remote_id, "
        "remote_updated_at = excluded.remote_updated_at",
        {entity, local_id, remote_id, remote_updated_at});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<void> SyncMap::remove_by_local(const QString& entity, const QString& local_id) {
    auto r = Database::instance().execute("DELETE FROM sync_map WHERE entity = ? AND local_id = ?",
                                          {entity, local_id});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<void> SyncMap::remove_by_remote(const QString& entity, const QString& remote_id) {
    auto r = Database::instance().execute("DELETE FROM sync_map WHERE entity = ? AND remote_id = ?",
                                          {entity, remote_id});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<QVector<SyncMapping>> SyncMap::all(const QString& entity) {
    auto r = Database::instance().execute(
        "SELECT local_id, remote_id, remote_updated_at FROM sync_map WHERE entity = ?", {entity});
    if (r.is_err())
        return Result<QVector<SyncMapping>>::err(r.error());
    QVector<SyncMapping> out;
    auto& q = r.value();
    while (q.next())
        out.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString()});
    return Result<QVector<SyncMapping>>::ok(std::move(out));
}

} // namespace fincept
