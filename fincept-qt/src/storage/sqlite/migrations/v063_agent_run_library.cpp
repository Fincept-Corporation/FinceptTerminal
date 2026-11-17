// v063_agent_run_library — Chat Mode run artefacts, task templates and schedules
// (services/agent_tasks WP5 / WP6), on top of v059's agent_task_* tables.
//
//   agent_run_artefacts   what runs produced (reports, tables, tool results, files)
//                         with provenance (source_json), a pin flag and the run's
//                         title. NO foreign key to agent_task_runs on purpose: a
//                         pinned artefact outlives its run (ArtefactService deletes
//                         the unpinned ones with their run, and sweeps orphans).
//                         v059's agent_task_artefacts rows are moved here (that
//                         table cascades with its run, so it cannot keep a pinned
//                         artefact); the old table stays, empty, for older readers.
//   agent_run_templates   the user's own task templates (the whole template as
//                         JSON; the built-ins live in code)
//   agent_task_runs       + active_ms, active_since (working time without pauses,
//                           the elapsed time shown and the max_minutes clock),
//                           credits_unreported (turns charged but not reported)
//   agent_task_schedules  + time_zone, once_at, catch_up_minutes, last_run_id,
//                           last_status, last_error
//
// SAFE ON A POPULATED DATABASE AND IDEMPOTENT: CREATE … IF NOT EXISTS, INSERT OR
// IGNORE for the move (the moved rows are then removed from the old table, so a
// second run moves nothing), and every ADD COLUMN guarded by PRAGMA table_info
// (a table that does not exist is skipped — MigrationRunner::inspect() builds its
// reference schema with this migration too). Only NOT NULL columns with a constant
// default are added.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after
// register_migration_v061() (v062 is another work package's), listed in
// LegacyRootAdoption.cpp register_known_migrations() (kKnownHighestMigration).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v063();

namespace {

Result<void> sql_v063(QSqlDatabase& db, const QString& stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(QStringLiteral("v063: %1 — %2").arg(stmt.left(80), q.lastError().text()).toStdString());
    return Result<void>::ok();
}

/// Column names of `table` (lower case); `exists` = false when the table is absent.
Result<QSet<QString>> v063_columns(QSqlDatabase& db, const QString& table, bool* exists) {
    QSqlQuery q(db);
    // Table names are the compile-time constants below — never user input.
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return Result<QSet<QString>>::err(q.lastError().text().toStdString());
    QSet<QString> out;
    while (q.next())
        out.insert(q.value(1).toString().toLower());
    *exists = !out.isEmpty();
    return Result<QSet<QString>>::ok(out);
}

struct V063Column {
    const char* table;
    const char* column;
    const char* decl;
};

constexpr V063Column kV063Columns[] = {
    {"agent_task_runs", "active_ms", "INTEGER NOT NULL DEFAULT 0"},
    {"agent_task_runs", "active_since", "INTEGER NOT NULL DEFAULT 0"},
    {"agent_task_runs", "credits_unreported", "INTEGER NOT NULL DEFAULT 0"},
    {"agent_task_schedules", "time_zone", "TEXT NOT NULL DEFAULT ''"},
    {"agent_task_schedules", "once_at", "INTEGER NOT NULL DEFAULT 0"},
    {"agent_task_schedules", "catch_up_minutes", "INTEGER NOT NULL DEFAULT 720"},
    {"agent_task_schedules", "last_run_id", "TEXT NOT NULL DEFAULT ''"},
    {"agent_task_schedules", "last_status", "TEXT NOT NULL DEFAULT ''"},
    {"agent_task_schedules", "last_error", "TEXT NOT NULL DEFAULT ''"},
};

Result<void> apply_v063(QSqlDatabase& db) {
    static const char* const kCreate[] = {
        "CREATE TABLE IF NOT EXISTS agent_run_artefacts ("
        "  id          TEXT PRIMARY KEY,"
        "  run_id      TEXT NOT NULL,"
        "  run_title   TEXT NOT NULL DEFAULT '',"
        "  step_idx    INTEGER NOT NULL DEFAULT -1,"
        "  kind        TEXT NOT NULL DEFAULT '',"
        "  title       TEXT NOT NULL DEFAULT '',"
        "  mime        TEXT NOT NULL DEFAULT '',"
        "  stored_name TEXT NOT NULL DEFAULT '',"
        "  inline_json TEXT NOT NULL DEFAULT '',"
        "  bytes       INTEGER NOT NULL DEFAULT 0,"
        "  source_json TEXT NOT NULL DEFAULT '{}',"
        "  pinned      INTEGER NOT NULL DEFAULT 0,"
        "  created_at  INTEGER NOT NULL,"
        "  updated_at  INTEGER NOT NULL DEFAULT 0"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_agent_run_artefacts_run ON agent_run_artefacts(run_id)",
        "CREATE INDEX IF NOT EXISTS idx_agent_run_artefacts_pinned ON agent_run_artefacts(pinned)",

        "CREATE TABLE IF NOT EXISTS agent_run_templates ("
        "  id            TEXT PRIMARY KEY,"
        "  title         TEXT NOT NULL DEFAULT '',"
        "  template_json TEXT NOT NULL DEFAULT '{}',"
        "  created_at    INTEGER NOT NULL,"
        "  updated_at    INTEGER NOT NULL"
        ")",
    };
    for (const char* stmt : kCreate) {
        auto r = sql_v063(db, QString::fromLatin1(stmt));
        if (r.is_err())
            return r;
    }

    // Move v059's artefact rows (that table cascades with its run).
    bool old_exists = false;
    auto old_cols = v063_columns(db, QStringLiteral("agent_task_artefacts"), &old_exists);
    if (old_cols.is_err())
        return Result<void>::err(old_cols.error());
    if (old_exists) {
        bool runs_exist = false;
        auto run_cols = v063_columns(db, QStringLiteral("agent_task_runs"), &runs_exist);
        if (run_cols.is_err())
            return Result<void>::err(run_cols.error());
        const QString title = runs_exist ? QStringLiteral("COALESCE((SELECT r.title FROM agent_task_runs r WHERE r.id = "
                                                          "a.run_id), '')")
                                         : QStringLiteral("''");
        auto moved = sql_v063(
            db, QStringLiteral("INSERT OR IGNORE INTO agent_run_artefacts (id, run_id, run_title, step_idx, kind, title, "
                               "mime, stored_name, inline_json, bytes, source_json, pinned, created_at, updated_at) "
                               "SELECT a.id, a.run_id, %1, a.step_idx, a.kind, a.title, a.mime, a.stored_name, "
                               "a.inline_json, a.bytes, '{}', 0, a.created_at, a.created_at FROM agent_task_artefacts a")
                    .arg(title));
        if (moved.is_err())
            return moved;
        auto cleared = sql_v063(db, QStringLiteral("DELETE FROM agent_task_artefacts"));
        if (cleared.is_err())
            return cleared;
    }

    QHash<QString, QSet<QString>> known;
    QHash<QString, bool> present;
    for (const V063Column& c : kV063Columns) {
        const QString table = QString::fromLatin1(c.table);
        if (!present.contains(table)) {
            bool exists = false;
            auto cols = v063_columns(db, table, &exists);
            if (cols.is_err())
                return Result<void>::err(cols.error());
            present.insert(table, exists);
            known.insert(table, cols.value());
        }
        if (!present.value(table))
            continue;
        const QString column = QString::fromLatin1(c.column);
        if (known[table].contains(column))
            continue; // re-run: already there
        auto r = sql_v063(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, QLatin1String(c.decl)));
        if (r.is_err())
            return r;
        known[table].insert(column);
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v063() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({63, "agent_run_library", apply_v063});
}

} // namespace fincept
