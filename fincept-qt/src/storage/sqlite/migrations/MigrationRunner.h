#pragma once
#include "core/result/Result.h"

#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <string>

namespace fincept {

/// A single versioned migration.
struct Migration {
    int version;
    QString name;
    std::function<Result<void>(QSqlDatabase&)> apply;
};

/// What a database file is relative to this build. Computed by
/// `MigrationRunner::inspect()` without writing anything to the file.
enum class SchemaState {
    Fresh,        ///< no tables at all: every migration will run
    UpToDate,     ///< at this build's highest version, structurally compatible
    NeedsUpgrade, ///< an older version of this build's schema; pending migrations will run
    Newer,        ///< MAX(schema_version) is above this build's highest version
    Incompatible, ///< tables or columns this build's own migrations create are missing
};

struct SchemaInspection {
    SchemaState state = SchemaState::Fresh;
    int current = 0;     ///< MAX(version) recorded in the file (0 when none)
    int target = 0;      ///< highest version this build registers
    QStringList missing; ///< "table x" / "column x.y" absent from the file (Incompatible; for the log)
};

/// Runs versioned migrations tracked in a schema_version table.
/// Migration files auto-register via static initialization.
///
/// Safety contract (do not weaken — a half-migrated DB is silent data loss):
///   - The file is inspected BEFORE anything is written, including the
///     schema_version table itself (`inspect()`).
///   - A database written by a NEWER build is refused outright (`kErrSchemaNewer`).
///     Version numbers alone cannot identify the product line that wrote a file:
///     builds number their migrations independently. So the file's tables and
///     columns are also compared with a reference schema built by running this
///     build's own migrations up to the file's version on a throwaway in-memory
///     database; anything missing refuses the file as `kErrSchemaIncompatible`.
///     Extra tables and columns are allowed. In both cases nothing is applied.
///   - Before the FIRST forward migration of a session, a consistent snapshot of
///     the database file is written to
///     `<db>.pre-v<from>-to-v<target>.<yyyyMMdd-hhmmss>.bak` (skipped for an empty
///     file). Backup failure aborts the migration — migrating with no recovery
///     path is the bug this class exists to prevent. After a successful run only
///     the newest `kBackupsToKeep` backups of this file are kept.
///   - Any failure returns an error tagged with `kErrMigrationFailed` and, when
///     one exists, the backup path embedded in the message.
///
/// Callers MUST treat all three tagged errors as fatal: show a blocking dialog
/// and never boot the UI onto a half-migrated, future or foreign schema.
class MigrationRunner {
  public:
    /// Error tag prefixes. `run()` prefixes its error strings with one of these
    /// so a caller can classify the failure without parsing prose.
    static constexpr const char* kErrSchemaNewer = "[schema-newer]";
    static constexpr const char* kErrSchemaIncompatible = "[schema-incompatible]";
    static constexpr const char* kErrMigrationFailed = "[migration-failed]";

    /// Pre-migration backups kept per database file after a successful run.
    static constexpr int kBackupsToKeep = 3;

    /// True when `error_message` came from `run()` refusing a future schema.
    static bool is_schema_newer_error(const std::string& error_message);
    /// True when `error_message` came from `run()` refusing a foreign schema.
    static bool is_schema_incompatible_error(const std::string& error_message);
    /// True when `error_message` came from a migration that failed to apply.
    static bool is_migration_failure(const std::string& error_message);
    /// True for any of the three — "this build must not use the database".
    static bool is_fatal_error(const std::string& error_message);

    explicit MigrationRunner(QSqlDatabase& db);

    /// Classify the file without writing to it (no DDL, no schema_version
    /// table). The result is remembered and reused by the next `run()` on this
    /// runner. An error means the file could not be read (raw driver text).
    Result<SchemaInspection> inspect();

    /// The last successful `inspect()` (also the one `run()` used).
    const SchemaInspection& last_inspection() const { return inspection_; }

    /// Refuse a Newer / Incompatible file (tagged error, nothing written), else
    /// apply all pending migrations in order. Calls `inspect()` unless it was
    /// already called on this runner.
    Result<void> run();

    /// Current applied schema version (0 if none).
    int current_version() const;

    /// Highest version this build knows how to apply (0 if none registered).
    static int highest_registered_version();

    /// Path of the pre-migration backup taken (or reused) by the last `run()`, or empty.
    QString backup_path() const { return backup_path_; }

    /// Register a migration (called at static-init time by each v00N file).
    static void register_migration(Migration m);

    /// All registered migrations, sorted by version.
    static const QVector<Migration>& all_migrations();

    /// Pre-migration backups of `db_file` (`<file>.pre-v*.bak`, both naming
    /// schemes), newest first by modification time.
    static QStringList list_backups(const QString& db_file);

  private:
    Result<void> ensure_schema_version_table();
    Result<void> apply_migration(const Migration& m);
    Result<void> record_version(int version, const QString& name);
    int read_current_version();

    /// Table -> column names (lower case) that this build's migrations up to
    /// `version` create, built on a throwaway in-memory connection. FTS virtual
    /// tables and their shadow tables are left out: whether they exist depends on
    /// the SQLite library, not on the schema version.
    static Result<QHash<QString, QStringList>> reference_schema(int version);

    /// Snapshot the database to
    /// `<db>.pre-v<current>-to-v<target>.<yyyyMMdd-hhmmss>.bak` with VACUUM INTO
    /// (free space checked first). Reuses an existing backup only when its own
    /// MAX(schema_version) equals `current_version`.
    Result<void> backup_database(int current_version, int target_version);

    /// Delete all but the newest `kBackupsToKeep` backups of this database.
    void prune_backups();

    QSqlDatabase& db_;
    QString backup_path_;
    SchemaInspection inspection_;
    bool inspected_ = false;
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
void register_migration_v012();
void register_migration_v013();
void register_migration_v014();
void register_migration_v015();
void register_migration_v016();
void register_migration_v017();
void register_migration_v018();
void register_migration_v019();
void register_migration_v020();
void register_migration_v021();
void register_migration_v022();
void register_migration_v023();
void register_migration_v024();
void register_migration_v025();
void register_migration_v026();
void register_migration_v027();
void register_migration_v028();
void register_migration_v029();
void register_migration_v030();
void register_migration_v031();
void register_migration_v032();
void register_migration_v033();
void register_migration_v034();
void register_migration_v035();
void register_migration_v036();
void register_migration_v037();
void register_migration_v038();
void register_migration_v039();
void register_migration_v040();
void register_migration_v041();
void register_migration_v042();
void register_migration_v043();
void register_migration_v044();
void register_migration_v045();
void register_migration_v046();
void register_migration_v047();
void register_migration_v048();
void register_migration_v049();
void register_migration_v050();
void register_migration_v051();
void register_migration_v052();
void register_migration_v053();
void register_migration_v054();
void register_migration_v055();
void register_migration_v056();
void register_migration_v057();
void register_migration_v058();
void register_migration_v059();
void register_migration_v060();
void register_migration_v061();
void register_migration_v062(); // Data Sources health + Data Mapping v2 (specs, revisions, outputs)
void register_migration_v063(); // Chat Mode artefacts / templates / schedules (v062 belongs to another package)
void register_migration_v064(); // M&A saved cases + Alt Investments funds / flows / holdings / DD checklists
void register_migration_v065(); // Derivatives position book + saved strategies
void register_migration_v066(); // Notes board: status / order / review rows, named boards, wall layout

/// The FTS5 search index over news_articles (news_fts plus its three sync
/// triggers). v013 creates it when the SQLite library has FTS5 and skips it
/// otherwise; Database::open() calls this on every start to converge:
///   - FTS5 available, index or a trigger missing (the file was first migrated
///     by a build without FTS5): create them and index the existing rows;
///   - FTS5 NOT available but the triggers exist (the file came from a build
///     with FTS5): drop the triggers, so inserts into news_articles keep
///     working; search falls back to LIKE.
/// No-op when news_articles does not exist. Runs in its own transaction.
Result<void> ensure_news_fts(QSqlDatabase& db, bool fts5_available);

} // namespace fincept
