// v066_notes_board — the Notes BOARD's own attributes, in side tables keyed by
// note id so that financial_notes and NotesRepository (the dashboard Notes tile,
// Chat Mode's "Send to Notes", MCP tools, cloud sync) stay exactly as they are:
//
//   note_board_meta     one row per note: workflow status, manual order
//                       (sort_rank, ascending), review date, origin ("agent_runs"
//                       + run id for notes saved from Chat Mode), times
//   note_board_history  status / review / archive events (the detail pane's history)
//   note_boards         named boards = saved views (definition JSON)
//   note_wall_items     free-form wall position of a note on a board
//   note_wall_groups    user-drawn titled groups on a board's wall
//
// Existing notes get their row here: status from the category (archived ->
// CLOSED), order by last edit (newest first), origin from Chat Mode's
// "agent-run" tag. The SQL is built from storage/repositories/NoteBoardTypes.h —
// the same rules the screen applies to notes created later by other writers.
// A delete trigger on financial_notes removes a note's board rows with it.
//
// SAFE ON A POPULATED DATABASE AND IDEMPOTENT: CREATE … IF NOT EXISTS and
// INSERT OR IGNORE only; nothing existing is altered or dropped. No ADD COLUMN.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after
// register_migration_v064() (v065 is the Derivatives position book), listed in
// LegacyRootAdoption.cpp register_known_migrations() (kKnownHighestMigration).

#include "storage/repositories/NoteBoardTypes.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v066();

namespace {

Result<void> sql_v066(QSqlDatabase& db, const QString& stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(QStringLiteral("v066: %1 — %2").arg(stmt.left(80), q.lastError().text()).toStdString());
    return Result<void>::ok();
}

bool v066_table_exists(QSqlDatabase& db, const QString& table) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?"));
    q.addBindValue(table);
    return q.exec() && q.next();
}

Result<void> apply_v066(QSqlDatabase& db) {
    static const char* const kCreate[] = {
        "CREATE TABLE IF NOT EXISTS note_board_meta ("
        "  note_id           INTEGER PRIMARY KEY,"
        "  status            TEXT NOT NULL DEFAULT 'IDEA',"
        "  sort_rank         REAL NOT NULL DEFAULT 0,"
        "  review_on         TEXT NOT NULL DEFAULT '',"
        "  origin            TEXT NOT NULL DEFAULT '',"
        "  origin_ref        TEXT NOT NULL DEFAULT '',"
        "  status_changed_at INTEGER NOT NULL DEFAULT 0,"
        "  reviewed_at       INTEGER NOT NULL DEFAULT 0"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_note_board_meta_review ON note_board_meta(review_on)",

        "CREATE TABLE IF NOT EXISTS note_board_history ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  note_id    INTEGER NOT NULL,"
        "  at         INTEGER NOT NULL,"
        "  kind       TEXT NOT NULL,"
        "  from_value TEXT NOT NULL DEFAULT '',"
        "  to_value   TEXT NOT NULL DEFAULT ''"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_note_board_history_note ON note_board_history(note_id, at)",

        "CREATE TABLE IF NOT EXISTS note_boards ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT NOT NULL,"
        "  position   INTEGER NOT NULL DEFAULT 0,"
        "  definition TEXT NOT NULL DEFAULT '{}',"
        "  created_at INTEGER NOT NULL DEFAULT 0,"
        "  updated_at INTEGER NOT NULL DEFAULT 0"
        ")",

        "CREATE TABLE IF NOT EXISTS note_wall_items ("
        "  board_id INTEGER NOT NULL,"
        "  note_id  INTEGER NOT NULL,"
        "  x        REAL NOT NULL DEFAULT 0,"
        "  y        REAL NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (board_id, note_id)"
        ")",

        "CREATE TABLE IF NOT EXISTS note_wall_groups ("
        "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  board_id INTEGER NOT NULL,"
        "  title    TEXT NOT NULL DEFAULT '',"
        "  x        REAL NOT NULL DEFAULT 0,"
        "  y        REAL NOT NULL DEFAULT 0,"
        "  w        REAL NOT NULL DEFAULT 0,"
        "  h        REAL NOT NULL DEFAULT 0,"
        "  tone     INTEGER NOT NULL DEFAULT 0"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_note_wall_groups_board ON note_wall_groups(board_id)",
    };
    for (const char* stmt : kCreate) {
        auto r = sql_v066(db, QString::fromLatin1(stmt));
        if (r.is_err())
            return r;
    }

    if (!v066_table_exists(db, QStringLiteral("financial_notes")))
        return Result<void>::ok(); // nothing to attach the rows / trigger to

    // A deleted note takes its board rows with it.
    auto t = sql_v066(db, QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_financial_notes_board_cleanup "
                                         "AFTER DELETE ON financial_notes BEGIN "
                                         "DELETE FROM note_board_meta WHERE note_id = OLD.id; "
                                         "DELETE FROM note_board_history WHERE note_id = OLD.id; "
                                         "DELETE FROM note_wall_items WHERE note_id = OLD.id; "
                                         "END"));
    if (t.is_err())
        return t;

    // Existing notes: status from category / archived, order by last edit, Chat Mode origin.
    const QString status = notes_board::default_status_sql(QStringLiteral("n.category"), QStringLiteral("n.is_archived"));
    const QString rank = notes_board::default_rank_sql(QStringLiteral("n.updated_at"), QStringLiteral("n.created_at"));
    const QString origin = QStringLiteral("CASE WHEN (',' || LOWER(REPLACE(COALESCE(n.tags, ''), ' ', '')) || ',') "
                                          "LIKE '%,%1,%' THEN '%2' ELSE '' END")
                               .arg(notes_board::kAgentRunTag, notes_board::kOriginAgentRuns);
    return sql_v066(db, QStringLiteral("INSERT OR IGNORE INTO note_board_meta (note_id, status, sort_rank, review_on, "
                                       "origin, origin_ref, status_changed_at, reviewed_at) "
                                       "SELECT n.id, %1, %2, '', %3, '', 0, 0 FROM financial_notes n")
                            .arg(status, rank, origin));
}

} // anonymous namespace

void register_migration_v066() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({66, "notes_board", apply_v066});
}

} // namespace fincept
