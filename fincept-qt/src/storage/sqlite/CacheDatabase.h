#pragma once
#include "core/result/Result.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

namespace fincept {

/// Separate SQLite database for ephemeral cache data.
/// Uses synchronous=OFF for maximum write speed.
class CacheDatabase {
  public:
    static CacheDatabase& instance();

    Result<void> open(const QString& path);
    void close();
    bool is_open() const;

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

  private:
    CacheDatabase() = default;
    Result<void> apply_pragmas();
    Result<void> create_tables();

    QSqlDatabase db_;
};

} // namespace fincept
