#pragma once
#include "core/result/Result.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

namespace fincept {

/// Separate SQLite database for ephemeral cache data.
/// Uses synchronous=NORMAL under WAL for safe, fast writes.
/// A QMutex serializes access: Qt's QSqlDatabase connection is not thread-safe,
/// and this cache is read/written from services whose callbacks may run on
/// worker threads (HttpClient, PythonRunner finished signals).
class CacheDatabase {
  public:
    static CacheDatabase& instance();

    Result<void> open(const QString& path);
    void close();
    bool is_open() const;

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

    QSqlDatabase& raw_db() { return db_; }

  private:
    CacheDatabase() = default;
    Result<void> apply_pragmas();
    Result<void> create_tables();

    QSqlDatabase db_;
    mutable QMutex mutex_;
};

} // namespace fincept
