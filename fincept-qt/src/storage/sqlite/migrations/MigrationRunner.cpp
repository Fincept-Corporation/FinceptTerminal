#include "storage/sqlite/migrations/MigrationRunner.h"

#include "core/logging/Logger.h"

#include <QFile>

#include <algorithm>

namespace fincept {

namespace {
constexpr const char* kMigrationTag = "DB";

bool has_error_tag(const std::string& message, const char* tag) {
    return message.rfind(tag, 0) == 0;
}
} // namespace

// ── Static migration registry ────────────────────────────────────────────────

static QVector<Migration>& migration_registry() {
    static QVector<Migration> registry;
    return registry;
}

void MigrationRunner::register_migration(Migration m) {
    auto& reg = migration_registry();
    reg.append(std::move(m));
    // Keep sorted by version
    std::sort(reg.begin(), reg.end(), [](const Migration& a, const Migration& b) { return a.version < b.version; });
}

const QVector<Migration>& MigrationRunner::all_migrations() {
    return migration_registry();
}

int MigrationRunner::highest_registered_version() {
    const auto& reg = migration_registry();
    return reg.isEmpty() ? 0 : reg.last().version; // registry is kept sorted
}

bool MigrationRunner::is_schema_newer_error(const std::string& error_message) {
    return has_error_tag(error_message, kErrSchemaNewer);
}

bool MigrationRunner::is_migration_failure(const std::string& error_message) {
    return has_error_tag(error_message, kErrMigrationFailed);
}

bool MigrationRunner::is_fatal_error(const std::string& error_message) {
    // Only a FAILED migration is fatal. A half-applied schema is genuinely
    // unsafe to run against, and continuing would let every repository read and
    // write a shape that is neither the old nor the new one.
    //
    // A NEWER-than-build schema is deliberately NOT fatal. It is the normal
    // state whenever a released build and a dev build share one AppData
    // directory — the released build migrates the DB forward, then the dev tree
    // (which has fewer migrations registered) opens the same file. Refusing to
    // start there bricks the developer's terminal for a condition the app has
    // always tolerated: every migration is skipped by the `version <= current`
    // test and the app runs normally.
    //
    // The risk is real but bounded and situational, so it is surfaced as a loud
    // warning at the call site rather than a hard stop. Making it fatal was
    // tested against a live v57 database on a v51 build and blocked startup
    // outright.
    return is_migration_failure(error_message);
}

// ── Instance methods ─────────────────────────────────────────────────────────

MigrationRunner::MigrationRunner(QSqlDatabase& db) : db_(db) {}

Result<void> MigrationRunner::run() {
    backup_path_.clear();

    auto r = ensure_schema_version_table();
    if (r.is_err()) {
        return Result<void>::err(std::string(kErrMigrationFailed) + " " + r.error());
    }

    const int current = read_current_version();
    const auto& migrations = all_migrations();
    const int target = highest_registered_version();

    // (a) Downgrade notice. A DB written by a NEWER build is running against an
    // older one: `run()` only skips `version <= current`, so every future
    // migration is silently treated as "already applied" and this build queries
    // a schema it does not fully understand.
    //
    // Warn, do NOT refuse. This is the normal state whenever a released build
    // and a dev build share one AppData directory — the release migrates the DB
    // forward, then the dev tree (fewer migrations registered) opens the same
    // file. The app has always tolerated it; making it fatal was tested against
    // a live v57 database on a v51 build and blocked startup outright, which is
    // a far worse outcome than the risk it guards against. Tables this build
    // does not know about are simply unused; the exposure is a table it DOES
    // know that gained columns it cannot see.
    //
    // Returning ok() here is deliberate: an err() would send main() down the
    // degraded DB-less path (no accounts, no repositories), which is not what
    // "schema is ahead" warrants.
    if (current > target) {
        LOG_WARN(kMigrationTag,
                 QString("database schema v%1 is NEWER than this build (v%2) — all migrations skipped. "
                         "Normal when a released build shares this AppData directory; if data looks wrong, "
                         "run the newer build or restore a pre-upgrade backup of %3.")
                     .arg(current)
                     .arg(target)
                     .arg(db_.databaseName()));
        return Result<void>::ok();
    }

    // (b) Back up before touching anything. Migrations are individually
    // transactional but the SEQUENCE is not: v046 can commit and v047 fail,
    // leaving a schema that matches no released build. The snapshot is the only
    // way back from that.
    if (current < target) {
        auto br = backup_database(current, target);
        if (br.is_err()) {
            const QString detail = QString("cannot migrate v%1 → v%2: pre-migration backup failed (%3). Refusing to "
                                           "migrate without a recovery copy — free disk space or check permissions "
                                           "on %4 and relaunch.")
                                       .arg(current)
                                       .arg(target)
                                       .arg(QString::fromStdString(br.error()), db_.databaseName());
            LOG_ERROR(kMigrationTag, detail);
            return Result<void>::err(std::string(kErrMigrationFailed) + " " + detail.toStdString());
        }
    }

    // (c) Unambiguous failure reporting — every early return below carries the
    // tag plus the backup path, so the caller can tell the user where the
    // recoverable copy lives.
    int applied_through = current;
    auto fail = [this, &applied_through](const QString& what) {
        QString detail = what + QString(" Schema left at v%1.").arg(applied_through);
        if (!backup_path_.isEmpty())
            detail += QString(" Pre-migration backup: %1").arg(backup_path_);
        LOG_ERROR(kMigrationTag, detail);
        return Result<void>::err(std::string(kErrMigrationFailed) + " " + detail.toStdString());
    };

    for (const auto& m : migrations) {
        if (m.version <= current)
            continue;

        LOG_INFO(kMigrationTag, QString("Applying migration v%1: %2").arg(m.version).arg(m.name));

        // Begin transaction for this migration
        QSqlQuery q(db_);
        if (!q.exec("BEGIN IMMEDIATE")) {
            return fail(QString("Failed to begin transaction for migration v%1 (%2): %3.")
                            .arg(m.version)
                            .arg(m.name, q.lastError().text()));
        }

        // Apply
        auto ar = apply_migration(m);
        if (ar.is_err()) {
            q.exec("ROLLBACK");
            return fail(QString("Migration v%1 (%2) failed: %3.")
                            .arg(m.version)
                            .arg(m.name, QString::fromStdString(ar.error())));
        }

        // Record version
        auto vr = record_version(m.version, m.name);
        if (vr.is_err()) {
            q.exec("ROLLBACK");
            return fail(QString("Migration v%1 (%2) applied but its version could not be recorded: %3.")
                            .arg(m.version)
                            .arg(m.name, QString::fromStdString(vr.error())));
        }

        // Commit
        if (!q.exec("COMMIT")) {
            return fail(QString("Failed to commit migration v%1 (%2): %3.")
                            .arg(m.version)
                            .arg(m.name, q.lastError().text()));
        }

        applied_through = m.version;
        LOG_INFO(kMigrationTag, QString("Migration v%1 applied successfully").arg(m.version));
    }

    if (!migrations.isEmpty()) {
        LOG_INFO(kMigrationTag, QString("Schema at version %1").arg(read_current_version()));
    }
    return Result<void>::ok();
}

int MigrationRunner::current_version() const {
    // const_cast because read uses QSqlQuery which needs non-const db
    return const_cast<MigrationRunner*>(this)->read_current_version();
}

// ── Private helpers ──────────────────────────────────────────────────────────

Result<void> MigrationRunner::ensure_schema_version_table() {
    QSqlQuery q(db_);
    if (!q.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                "  version INTEGER PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  applied_at TEXT DEFAULT (datetime('now'))"
                ")")) {
        return Result<void>::err("Failed to create schema_version table: " + q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> MigrationRunner::backup_database(int current_version, int target_version) {
    const QString db_file = db_.databaseName();
    // In-memory / unnamed databases have nothing on disk to copy. Not an error.
    if (db_file.isEmpty() || db_file.startsWith(QLatin1String(":memory:"))) {
        backup_path_.clear();
        return Result<void>::ok();
    }

    const QString dest = QStringLiteral("%1.pre-v%2.bak").arg(db_file).arg(target_version);

    // A backup left by an earlier FAILED attempt at the same target is the same
    // vintage as what we would write now — keep the known-good copy rather than
    // overwriting it with a database a partially-applied run may have touched.
    if (QFile::exists(dest)) {
        backup_path_ = dest;
        LOG_INFO(kMigrationTag,
                 QString("Reusing existing pre-migration backup for v%1 → v%2: %3")
                     .arg(current_version)
                     .arg(target_version)
                     .arg(dest));
        return Result<void>::ok();
    }

    // Preferred: VACUUM INTO. Produces a consistent, fully-checkpointed snapshot
    // from an OPEN connection (SQLite >= 3.27; Qt 6.8 ships far newer), so no
    // close/reopen dance and no torn WAL.
    QString vacuum_error;
    {
        QString literal = dest;
        literal.replace(QLatin1Char('\''), QStringLiteral("''"));
        QSqlQuery q(db_);
        if (q.exec(QStringLiteral("VACUUM INTO '%1'").arg(literal))) {
            backup_path_ = dest;
            LOG_INFO(kMigrationTag, QString("Backed up schema v%1 to %2 before migrating to v%3")
                                        .arg(current_version)
                                        .arg(dest)
                                        .arg(target_version));
            return Result<void>::ok();
        }
        vacuum_error = q.lastError().text();
        LOG_WARN(kMigrationTag, QString("VACUUM INTO backup failed (%1) — falling back to checkpoint + file copy")
                                    .arg(vacuum_error));
    }

    // Fallback: fold the WAL back into the main file so it is self-contained,
    // then copy it. Migrations run before any other connection exists, so there
    // is no concurrent writer to tear the copy.
    {
        QSqlQuery q(db_);
        if (!q.exec("PRAGMA wal_checkpoint(TRUNCATE)")) {
            LOG_WARN(kMigrationTag, QString("wal_checkpoint before backup failed: %1").arg(q.lastError().text()));
        }
    }
    QFile::remove(dest); // a failed VACUUM INTO can leave a partial file behind
    if (!QFile::copy(db_file, dest)) {
        return Result<void>::err(QString("VACUUM INTO failed (%1) and the file copy to %2 also failed")
                                     .arg(vacuum_error, dest)
                                     .toStdString());
    }

    backup_path_ = dest;
    LOG_INFO(kMigrationTag, QString("Backed up schema v%1 to %2 (file copy) before migrating to v%3")
                                .arg(current_version)
                                .arg(dest)
                                .arg(target_version));
    return Result<void>::ok();
}

Result<void> MigrationRunner::apply_migration(const Migration& m) {
    return m.apply(db_);
}

Result<void> MigrationRunner::record_version(int version, const QString& name) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO schema_version (version, name) VALUES (?, ?)");
    q.bindValue(0, version);
    q.bindValue(1, name);
    if (!q.exec()) {
        return Result<void>::err("Failed to record migration version: " + q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

int MigrationRunner::read_current_version() {
    QSqlQuery q(db_);
    if (!q.exec("SELECT MAX(version) FROM schema_version")) {
        return 0;
    }
    if (!q.next())
        return 0;
    return q.value(0).toInt(); // returns 0 if NULL
}

} // namespace fincept
