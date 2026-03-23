#pragma once
#include "core/result/Result.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

namespace fincept {

/// SQLite database wrapper with WAL mode, prepared statements, and transaction support.
class Database {
  public:
    static Database& instance();

    Result<void> open(const QString& path);
    void close();
    bool is_open() const;

    Result<QSqlQuery> execute(const QString& sql, const QVariantList& params = {});
    Result<void> exec(const QString& sql);

    // Transaction support
    Result<void> begin_transaction();
    Result<void> commit();
    Result<void> rollback();

    QSqlDatabase& raw_db() { return db_; }

  private:
    Database() = default;
    Result<void> apply_pragmas();

    QSqlDatabase db_;
};

} // namespace fincept
