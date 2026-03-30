#pragma once
#include "core/result/Result.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept {

/// A single versioned migration.
struct Migration {
    int version;
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
    int read_current_version();

    QSqlDatabase& db_;
};

// Explicit registration functions — call these before Database::open()
// to ensure MSVC linker doesn't strip the migration translation units.
void register_migration_v001();
void register_migration_v002();
void register_migration_v003();
void register_migration_v004();
void register_migration_v005();
void register_migration_v006();
void register_migration_v007();
void register_migration_v008();
void register_migration_v009();
void register_migration_v010();
void register_migration_v011();

} // namespace fincept
