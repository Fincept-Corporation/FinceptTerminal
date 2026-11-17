// v061_harness_runs — the Agent Workbench runs any agent harness (Claude Code,
// Codex, Grok, …; src/services/harness), not just two, with a reasoning effort,
// a permission mode and limits per run; and LLM profiles can name an effort.
//
//   coding_agent_runs       + effort, permission_mode, harness_version,
//                             time_limit_s, max_turns, cached_tokens
//   coding_agent_schedules  + effort, permission_mode, time_limit_s, max_turns,
//                             workdir
//   llm_profiles            + reasoning_effort
//
// `agent` stays the harness id, stored verbatim ("claude" / "codex" in every
// existing row — exactly the registry ids). An EMPTY permission_mode means "a
// row from before v061": readers derive it from the old `mode` column
// (analysis → read-only, workspace_edits → workspace write), so no backfill
// UPDATE is needed and the old column keeps its meaning.
//
// SAFE ON A POPULATED DATABASE AND IDEMPOTENT: every ADD COLUMN is guarded by a
// PRAGMA table_info check (SQLite has no ADD COLUMN IF NOT EXISTS), and a table
// that does not exist is skipped, so the migration also runs cleanly on the
// in-memory reference database MigrationRunner::inspect() builds. Only
// NOT NULL columns WITH a constant default are added, which SQLite allows on a
// table that already has rows.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after
// register_migration_v060(), listed in LegacyRootAdoption.cpp
// register_known_migrations() (kKnownHighestMigration = 61).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v061();

namespace {

struct V061Column {
    const char* table;
    const char* column;
    const char* decl;
};

// Every column is NOT NULL with a constant default: the repository binds '' / 0
// rather than NULL, and existing rows read the default.
constexpr V061Column kV061Columns[] = {
    {"coding_agent_runs", "effort", "TEXT NOT NULL DEFAULT ''"},
    {"coding_agent_runs", "permission_mode", "TEXT NOT NULL DEFAULT ''"},
    {"coding_agent_runs", "harness_version", "TEXT NOT NULL DEFAULT ''"},
    {"coding_agent_runs", "time_limit_s", "INTEGER NOT NULL DEFAULT 0"},
    {"coding_agent_runs", "max_turns", "INTEGER NOT NULL DEFAULT 0"},
    {"coding_agent_runs", "cached_tokens", "INTEGER NOT NULL DEFAULT 0"},
    {"coding_agent_schedules", "effort", "TEXT NOT NULL DEFAULT ''"},
    {"coding_agent_schedules", "permission_mode", "TEXT NOT NULL DEFAULT ''"},
    {"coding_agent_schedules", "time_limit_s", "INTEGER NOT NULL DEFAULT 0"},
    {"coding_agent_schedules", "max_turns", "INTEGER NOT NULL DEFAULT 0"},
    {"coding_agent_schedules", "workdir", "TEXT NOT NULL DEFAULT ''"},
    {"llm_profiles", "reasoning_effort", "TEXT NOT NULL DEFAULT ''"},
};

/// Column names of `table` (lower case); `exists` = false when the table is absent.
Result<QSet<QString>> v061_columns(QSqlDatabase& db, const QString& table, bool* exists) {
    QSqlQuery q(db);
    // The table names are compile-time constants above — never user input.
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return Result<QSet<QString>>::err(q.lastError().text().toStdString());
    QSet<QString> out;
    while (q.next())
        out.insert(q.value(1).toString().toLower());
    *exists = !out.isEmpty(); // PRAGMA table_info returns no rows for a missing table
    return Result<QSet<QString>>::ok(out);
}

Result<void> apply_v061(QSqlDatabase& db) {
    QHash<QString, QSet<QString>> known;  // table -> columns
    QHash<QString, bool> present;         // table -> exists
    for (const V061Column& c : kV061Columns) {
        const QString table = QString::fromLatin1(c.table);
        if (!present.contains(table)) {
            bool exists = false;
            auto cols = v061_columns(db, table, &exists);
            if (cols.is_err())
                return Result<void>::err(cols.error());
            present.insert(table, exists);
            known.insert(table, cols.value());
        }
        if (!present.value(table))
            continue; // e.g. a reference schema without llm_profiles — nothing to extend
        const QString column = QString::fromLatin1(c.column);
        if (known[table].contains(column))
            continue; // already there (re-run, or a newer build added it) — idempotent
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, QLatin1String(c.decl))))
            return Result<void>::err(QStringLiteral("v061: %1.%2: %3").arg(table, column, q.lastError().text()).toStdString());
        known[table].insert(column);
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v061() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({61, "harness_runs", apply_v061});
}

} // namespace fincept
