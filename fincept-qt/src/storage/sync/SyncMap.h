#pragma once
#include "core/result/Result.h"

#include <QString>
#include <QVector>

#include <optional>

namespace fincept {

/// One local↔remote identity mapping.
struct SyncMapping {
    QString local_id;
    QString remote_id;
    QString remote_updated_at;
};

/// Accessor over the `sync_map` table — maps a local primary key to the server
/// `public_id` (and remembers the remote `updated_at` for last-write-wins).
/// Shared by all cloud domain adapters. Uses the calling thread's connection.
class SyncMap {
  public:
    static std::optional<QString> remote_id(const QString& entity, const QString& local_id);
    static std::optional<QString> local_id(const QString& entity, const QString& remote_id);

    static Result<void> put(const QString& entity, const QString& local_id, const QString& remote_id,
                            const QString& remote_updated_at = {});
    static Result<void> remove_by_local(const QString& entity, const QString& local_id);
    static Result<void> remove_by_remote(const QString& entity, const QString& remote_id);

    /// All mappings for an entity (used to reconcile remote deletions on pull).
    static Result<QVector<SyncMapping>> all(const QString& entity);
};

} // namespace fincept
