#include "storage/sqlite/migrations/MigrationRunner.h"

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace fincept {

namespace {
constexpr const char* kMigrationTag = "DB";

bool has_error_tag(const std::string& message, const char* tag) {
    return message.rfind(tag, 0) == 0;
}

QString mr_quote_identifier(QString name) {
    name.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + name + QLatin1Char('"');
}

/// Lower-case column names of `table`, or an error when the table cannot be read.
Result<QSet<QString>> mr_table_columns(QSqlDatabase& db, const QString& table) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(mr_quote_identifier(table))))
        return Result<QSet<QString>>::err(q.lastError().text().toStdString());
    QSet<QString> cols;
    while (q.next())
        cols.insert(q.value(1).toString().toLower());
    return Result<QSet<QString>>::ok(std::move(cols));
}

/// MAX(schema_version) recorded in a backup file, or -1 when it cannot be read.
/// Opens the file as an immutable, read-only URI on a private connection that is
/// removed again: a backup copied from a WAL database is itself in WAL mode, and a
/// plain read-only open of such a file fails without a -shm it may not create.
int mr_backup_schema_version(const QString& path) {
    const QString name = QStringLiteral("fincept_backup_probe_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    int version = -1;
    {
        QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        probe.setDatabaseName(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString(QUrl::FullyEncoded) +
                              QStringLiteral("?immutable=1"));
        probe.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_OPEN_URI"));
        if (probe.open()) {
            QSqlQuery q(probe);
            if (q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) && q.next())
                version = q.value(0).toInt();
            q.finish();
            probe.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return version;
}

QDateTime mr_last_modified(const QString& path) {
    const QFileInfo fi(path);
    return fi.exists() ? fi.lastModified() : QDateTime();
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

bool MigrationRunner::is_schema_incompatible_error(const std::string& error_message) {
    return has_error_tag(error_message, kErrSchemaIncompatible);
}

bool MigrationRunner::is_migration_failure(const std::string& error_message) {
    return has_error_tag(error_message, kErrMigrationFailed);
}

bool MigrationRunner::is_fatal_error(const std::string& error_message) {
    // All three are fatal. A newer schema used to be tolerated ("a dev build next
    // to a release"), which is exactly how a database from a different product
    // line — numbered independently, missing tables this build needs — was
    // accepted and every repository then failed with "no such table". The
    // developer case is served by a separate per-lineage database instead.
    return is_migration_failure(error_message) || is_schema_newer_error(error_message) ||
           is_schema_incompatible_error(error_message);
}

QStringList MigrationRunner::list_backups(const QString& db_file) {
    const QFileInfo fi(db_file);
    QDir dir = fi.absoluteDir();
    const QFileInfoList entries =
        dir.entryInfoList({fi.fileName() + QStringLiteral(".pre-v*.bak")}, QDir::Files, QDir::Time /* newest first */);
    QStringList out;
    out.reserve(entries.size());
    for (const QFileInfo& e : entries)
        out << e.absoluteFilePath();
    return out;
}

// ── Instance methods ─────────────────────────────────────────────────────────

MigrationRunner::MigrationRunner(QSqlDatabase& db) : db_(db) {}

Result<QHash<QString, QStringList>> MigrationRunner::reference_schema(int version) {
    using R = Result<QHash<QString, QStringList>>;
    const QString name = QStringLiteral("fincept_schema_reference_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QHash<QString, QStringList> tables;
    QString error;
    {
        QSqlDatabase ref = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        ref.setDatabaseName(QStringLiteral(":memory:"));
        if (!ref.open()) {
            error = QStringLiteral("cannot open an in-memory database: ") + ref.lastError().text();
        } else {
            {
                QSqlQuery q(ref);
                q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
            }
            MigrationRunner runner(ref);
            auto r = runner.ensure_schema_version_table();
            if (r.is_err())
                error = QString::fromStdString(r.error());
            for (const Migration& m : all_migrations()) {
                if (!error.isEmpty() || m.version > version)
                    break;
                QSqlQuery q(ref);
                q.exec(QStringLiteral("BEGIN"));
                auto ar = m.apply(ref);
                if (ar.is_err()) {
                    q.exec(QStringLiteral("ROLLBACK"));
                    error = QString("migration v%1 (%2) failed on the reference database: %3")
                                .arg(m.version)
                                .arg(m.name, QString::fromStdString(ar.error()));
                    break;
                }
                q.exec(QStringLiteral("COMMIT"));
            }
            if (error.isEmpty()) {
                QStringList names;
                QStringList virtual_tables;
                {
                    QSqlQuery q(ref);
                    if (!q.exec(QStringLiteral("SELECT name, sql FROM sqlite_master WHERE type = 'table'"))) {
                        error = q.lastError().text();
                    } else {
                        while (q.next()) {
                            const QString n = q.value(0).toString();
                            if (n.startsWith(QLatin1String("sqlite_"), Qt::CaseInsensitive))
                                continue;
                            if (q.value(1).toString().trimmed().startsWith(QLatin1String("CREATE VIRTUAL TABLE"),
                                                                           Qt::CaseInsensitive))
                                virtual_tables << n.toLower();
                            names << n;
                        }
                    }
                }
                for (const QString& n : names) {
                    const QString lower = n.toLower();
                    const bool is_virtual_or_shadow =
                        std::any_of(virtual_tables.cbegin(), virtual_tables.cend(), [&lower](const QString& vt) {
                            return lower == vt || lower.startsWith(vt + QLatin1Char('_'));
                        });
                    if (is_virtual_or_shadow)
                        continue;
                    auto cols = mr_table_columns(ref, n);
                    if (cols.is_err()) {
                        error = QString::fromStdString(cols.error());
                        break;
                    }
                    tables.insert(lower, QStringList(cols.value().cbegin(), cols.value().cend()));
                }
            }
            ref.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    if (!error.isEmpty())
        return R::err(error.toStdString());
    return R::ok(std::move(tables));
}

Result<SchemaInspection> MigrationRunner::inspect() {
    using R = Result<SchemaInspection>;
    QElapsedTimer timer;
    timer.start();

    SchemaInspection out;
    out.target = highest_registered_version();
    const QString file = db_.databaseName();

    // Everything below is a read. Nothing — not even the schema_version table —
    // may be written before the file is known to belong to this build.
    QHash<QString, QString> tables; // lower-case name -> name as stored
    {
        QSqlQuery q(db_);
        if (!q.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table'")))
            return R::err("cannot read the table list: " + q.lastError().text().toStdString());
        while (q.next()) {
            const QString n = q.value(0).toString();
            tables.insert(n.toLower(), n);
        }
    }
    int user_tables = 0;
    for (auto it = tables.cbegin(); it != tables.cend(); ++it) {
        if (!it.key().startsWith(QLatin1String("sqlite_")) && it.key() != QLatin1String("schema_version"))
            ++user_tables;
    }

    auto finish = [&](SchemaState state) {
        out.state = state;
        inspection_ = out;
        inspected_ = true;
        static const char* names[] = {"fresh", "up to date", "needs upgrade", "newer", "incompatible"};
        LOG_DEBUG(kMigrationTag, QString("Schema inspected db=%1 state=\"%2\" file_version=%3 build_version=%4 duration_ms=%5")
                                    .arg(privacy::redact_home(file), QLatin1String(names[static_cast<int>(state)]))
                                    .arg(out.current)
                                    .arg(out.target)
                                    .arg(timer.elapsed()));
        if (!out.missing.isEmpty()) {
            LOG_WARN(kMigrationTag, QString("Database lacks schema objects this build creates db=%1 count=%2 missing=\"%3\"")
                                        .arg(privacy::redact_home(file))
                                        .arg(out.missing.size())
                                        .arg(out.missing.mid(0, 20).join(QLatin1String(", "))));
        }
        return R::ok(out);
    };

    if (!tables.contains(QStringLiteral("schema_version"))) {
        if (user_tables == 0)
            return finish(SchemaState::Fresh);
        // Tables but no version record: not a file this migration system wrote
        // (schema_version is created before the first migration runs).
        out.missing << QStringLiteral("table schema_version");
        return finish(SchemaState::Incompatible);
    }

    {
        QSqlQuery q(db_);
        if (!q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version"))) {
            const QSqlError e = q.lastError();
            if (db::classify(e) != db::SqlErrorKind::Schema)
                return R::err("cannot read schema_version: " + e.text().toStdString());
            out.missing << QStringLiteral("column schema_version.version");
            return finish(SchemaState::Incompatible);
        }
        if (q.next())
            out.current = q.value(0).toInt(); // 0 if NULL
    }

    if (out.current > out.target)
        return finish(SchemaState::Newer);

    // Cheap extra signal: does each recorded migration name match ours? Names
    // have changed within this build's own history, so log only — never block.
    {
        QHash<int, QString> recorded;
        QSqlQuery q(db_);
        if (q.exec(QStringLiteral("SELECT version, name FROM schema_version"))) {
            while (q.next())
                recorded.insert(q.value(0).toInt(), q.value(1).toString());
        }
        QStringList mismatches;
        for (const Migration& m : all_migrations()) {
            if (m.version > out.current)
                break;
            const auto it = recorded.constFind(m.version);
            if (it == recorded.cend())
                mismatches << QString("v%1 not recorded").arg(m.version);
            else if (it.value() != m.name)
                mismatches << QString("v%1 recorded as '%2', this build calls it '%3'").arg(m.version).arg(it.value(), m.name);
        }
        if (!mismatches.isEmpty()) {
            LOG_WARN(kMigrationTag, QString("Schema version records differ from this build's migrations db=%1 count=%2 "
                                            "entries=\"%3\"")
                                        .arg(privacy::redact_home(file))
                                        .arg(mismatches.size())
                                        .arg(mismatches.mid(0, 8).join(QLatin1String("; "))));
        }
    }

    if (out.current == 0) {
        if (user_tables == 0)
            return finish(SchemaState::Fresh);
        out.missing << QStringLiteral("schema_version rows (the file has tables but no recorded version)");
        return finish(SchemaState::Incompatible);
    }

    // The structural check below builds a reference schema by running every
    // migration on an in-memory database — about a second on the GUI thread,
    // before the window appears, on every start. Its answer can only change
    // when the file's schema or this build's migrations change, so it is
    // skipped when neither has since the last check that passed: SQLite bumps
    // the schema cookie (PRAGMA schema_version) on every schema change, and the
    // file's recorded version and this build's migration set are part of the
    // remembered signature too. Kept in QSettings, not in the file: inspect()
    // never writes to the database.
    int cookie = -1;
    {
        QSqlQuery q(db_);
        if (q.exec(QStringLiteral("PRAGMA schema_version")) && q.next())
            cookie = q.value(0).toInt();
    }
    const QString verified_key =
        QStringLiteral("storage/schema_verified/") +
        QString::fromLatin1(
            QCryptographicHash::hash(QFileInfo(file).absoluteFilePath().toUtf8(), QCryptographicHash::Md5).toHex());
    // The file's creation time ties the signature to this file, not to any
    // file that later appears at the same path with an equal cookie.
    const QDateTime born = QFileInfo(file).birthTime();
    const QString verified_sig = QStringLiteral("%1|%2|%3|%4|%5")
                                     .arg(cookie)
                                     .arg(out.current)
                                     .arg(out.target)
                                     .arg(all_migrations().size())
                                     .arg(born.isValid() ? born.toMSecsSinceEpoch() : 0);
    if (cookie >= 0 && !file.isEmpty() && !file.startsWith(QLatin1String(":memory:")) &&
        QSettings().value(verified_key).toString() == verified_sig) {
        LOG_DEBUG(kMigrationTag, QString("Schema unchanged since last verified, structural check skipped db=%1 cookie=%2")
                                     .arg(privacy::redact_home(file))
                                     .arg(cookie));
        return finish(out.current == out.target ? SchemaState::UpToDate : SchemaState::NeedsUpgrade);
    }

    // Structural check: every table and column this build's migrations up to the
    // file's version create must be present. Extras are fine (runtime-created
    // tables, columns added by a newer build of this lineage).
    auto ref = reference_schema(out.current);
    if (ref.is_err())
        return R::err("cannot build the reference schema: " + ref.error());
    for (auto it = ref.value().cbegin(); it != ref.value().cend(); ++it) {
        const auto found = tables.constFind(it.key());
        if (found == tables.cend()) {
            out.missing << QStringLiteral("table ") + it.key();
            continue;
        }
        auto cols = mr_table_columns(db_, found.value());
        if (cols.is_err())
            return R::err("cannot read the columns of " + found.value().toStdString() + ": " + cols.error());
        for (const QString& c : it.value()) {
            if (!cols.value().contains(c))
                out.missing << QStringLiteral("column %1.%2").arg(it.key(), c);
        }
    }
    out.missing.sort();
    if (!out.missing.isEmpty())
        return finish(SchemaState::Incompatible);
    // Passed: remember the signature so an unchanged file skips the check.
    if (cookie >= 0 && !file.isEmpty() && !file.startsWith(QLatin1String(":memory:")))
        QSettings().setValue(verified_key, verified_sig);
    return finish(out.current == out.target ? SchemaState::UpToDate : SchemaState::NeedsUpgrade);
}

Result<void> MigrationRunner::run() {
    backup_path_.clear();
    QElapsedTimer run_timer;
    run_timer.start();

    if (!inspected_) {
        auto ir = inspect();
        if (ir.is_err()) {
            return Result<void>::err(std::string(kErrMigrationFailed) + " Cannot read the database schema of " +
                                     db_.databaseName().toStdString() + ": " + ir.error());
        }
    }
    inspected_ = false; // a later run() re-inspects
    const SchemaInspection si = inspection_;
    const int current = si.current;
    const int target = si.target;

    // (a) Refusals. Nothing has been written to the file at this point, and
    // nothing will be: the caller must not use this database.
    if (si.state == SchemaState::Newer) {
        const QString detail = QString("database schema v%1 is newer than this build supports (v%2): %3 was written "
                                       "by a newer or different build. Nothing was changed.")
                                   .arg(current)
                                   .arg(target)
                                   .arg(db_.databaseName());
        LOG_ERROR(kMigrationTag, privacy::redact_text(detail));
        return Result<void>::err(std::string(kErrSchemaNewer) + " " + detail.toStdString());
    }
    if (si.state == SchemaState::Incompatible) {
        const QString detail = QString("database schema v%1 does not match this build's schema at that version: %2 "
                                       "lacks %3 (%4%5). It was probably written by a different build. Nothing was "
                                       "changed.")
                                   .arg(QString::number(current), db_.databaseName(), // one call: no re-scan of the path
                                        si.missing.size() == 1 ? QStringLiteral("1 table/column")
                                                               : QString("%1 tables/columns").arg(si.missing.size()),
                                        si.missing.mid(0, 5).join(QLatin1String(", ")),
                                        si.missing.size() > 5 ? QStringLiteral(", ...") : QString());
        LOG_ERROR(kMigrationTag, privacy::redact_text(detail));
        return Result<void>::err(std::string(kErrSchemaIncompatible) + " " + detail.toStdString());
    }

    auto r = ensure_schema_version_table();
    if (r.is_err()) {
        return Result<void>::err(std::string(kErrMigrationFailed) + " " + r.error());
    }

    const auto& migrations = all_migrations();

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
            LOG_ERROR(kMigrationTag, privacy::redact_text(detail));
            return Result<void>::err(std::string(kErrMigrationFailed) + " " + detail.toStdString());
        }
    }

    // (c) Unambiguous failure reporting — every early return below carries the
    // tag plus the backup path, so the caller can tell the user where the
    // recoverable copy lives.
    int applied_through = current;
    int applied_count = 0;
    auto fail = [this, &applied_through](const QString& what) {
        QString detail = what + QString(" Schema left at v%1.").arg(applied_through);
        if (!backup_path_.isEmpty())
            detail += QString(" Pre-migration backup: %1").arg(backup_path_);
        LOG_ERROR(kMigrationTag, privacy::redact_text(detail));
        return Result<void>::err(std::string(kErrMigrationFailed) + " " + detail.toStdString());
    };

    for (const auto& m : migrations) {
        if (m.version <= current)
            continue;

        LOG_DEBUG(kMigrationTag, QString("Migration applying version=%1 name=%2").arg(m.version).arg(m.name));

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
        ++applied_count;
    }

    if (!migrations.isEmpty()) {
        LOG_INFO(kMigrationTag, QString("Database schema ready version=%1 migrations_applied=%2 from_version=%3 "
                                        "duration_ms=%4")
                                    .arg(read_current_version())
                                    .arg(applied_count)
                                    .arg(current)
                                    .arg(run_timer.elapsed()));
    }
    prune_backups();
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
    // An empty database (first start) has nothing to protect.
    if (current_version == 0) {
        backup_path_.clear();
        LOG_DEBUG(kMigrationTag, "New database, no pre-migration backup needed");
        return Result<void>::ok();
    }

    // Reuse a backup only when it is provably a copy of the database as it is
    // now: same recorded version AND taken after the database (and its WAL)
    // last changed. That is the case after an earlier attempt failed on its
    // first migration. A same-version backup from months ago is NOT reused —
    // reporting a stale file as "the recovery copy" was the old behaviour.
    // An EMPTY -wal holds no changes: opening the database in WAL mode creates it
    // fresh, so its timestamp is "now" on every start.
    QDateTime db_changed = mr_last_modified(db_file);
    const QFileInfo wal(db_file + QStringLiteral("-wal"));
    if (wal.exists() && wal.size() > 0 && (!db_changed.isValid() || wal.lastModified() > db_changed))
        db_changed = wal.lastModified();
    for (const QString& candidate : list_backups(db_file)) {
        const QDateTime taken = mr_last_modified(candidate);
        if (!taken.isValid() || (db_changed.isValid() && taken < db_changed))
            continue;
        if (mr_backup_schema_version(candidate) != current_version)
            continue;
        backup_path_ = candidate;
        LOG_DEBUG(kMigrationTag, QString("Reusing pre-migration backup version=%1 backup=%2")
                                     .arg(current_version)
                                     .arg(privacy::redact_home(candidate)));
        return Result<void>::ok();
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    // One .arg() call: a chained call re-scans the text, so a "%2" or "%3" in the profile path would
    // receive the version numbers and the backup would be written under a different name.
    const QString dest = QStringLiteral("%1.pre-v%2-to-v%3.%4.bak")
                             .arg(db_file, QString::number(current_version), QString::number(target_version), stamp);
    db::SqlErrorKind kind = db::SqlErrorKind::None;
    auto r = db::backup_to(db_, dest, &kind);
    if (r.is_err()) {
        if (kind == db::SqlErrorKind::Full)
            return Result<void>::err("not enough disk space for the backup — " + r.error());
        return r;
    }
    backup_path_ = dest;
    LOG_INFO(kMigrationTag, QString("Pre-migration backup written from_version=%1 to_version=%3 backup=%2")
                                .arg(current_version)
                                .arg(privacy::redact_home(dest))
                                .arg(target_version));
    return Result<void>::ok();
}

void MigrationRunner::prune_backups() {
    const QString db_file = db_.databaseName();
    if (db_file.isEmpty() || db_file.startsWith(QLatin1String(":memory:")))
        return;
    const QStringList backups = list_backups(db_file); // newest first
    for (int i = kBackupsToKeep; i < backups.size(); ++i) {
        if (backups.at(i) == backup_path_)
            continue;
        if (QFile::remove(backups.at(i)))
            LOG_DEBUG(kMigrationTag, "Removed old pre-migration backup backup=" + privacy::redact_home(backups.at(i)));
        else
            LOG_WARN(kMigrationTag,
                     "Old pre-migration backup could not be removed backup=" + privacy::redact_home(backups.at(i)));
    }
    // Leftovers of a backup interrupted mid-write are never usable.
    const QFileInfo fi(db_file);
    const QFileInfoList partials =
        fi.absoluteDir().entryInfoList({fi.fileName() + QStringLiteral(".pre-v*.bak.partial")}, QDir::Files);
    for (const QFileInfo& p : partials)
        QFile::remove(p.absoluteFilePath());
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
