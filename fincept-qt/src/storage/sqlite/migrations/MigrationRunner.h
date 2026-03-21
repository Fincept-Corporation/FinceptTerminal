#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVector>
#include <functional>
#include "core/result/Result.h"

namespace fincept {

/// A single versioned migration.
struct Migration {
    int     version;
    QString name;
    std::function<Result<void>(QSqlDatabase&)> apply;
};

/// Runs versioned migrations tracked in a schema_version table.
/// Migration files auto-register via static initialization.
class MigrationRunner {
public:
    explicit MigrationRunner(QSqlDatabase& db);

    /// Apply all pending migrations in order.
    Result<void> run();

    /// Current applied schema version (0 if none).
    int current_version() const;

    /// Register a migration (called at static-init time by each v00N file).
    static void register_migration(Migration m);

    /// All registered migrations, sorted by version.
    static const QVector<Migration>& all_migrations();

private:
    Result<void> ensure_schema_version_table();
    Result<void> apply_migration(const Migration& m);
    Result<void> record_version(int version, const QString& name);
    int  read_current_version();

    QSqlDatabase& db_;
};

} // namespace fincept
