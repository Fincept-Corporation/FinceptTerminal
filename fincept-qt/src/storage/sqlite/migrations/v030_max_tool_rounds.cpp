// v030_max_tool_rounds — add max_tool_rounds to llm_global_settings.
//
// Exposes the previously-hardcoded LlmService tool-call loop ceiling
// (LlmToolLoop.cpp's MAX_ROUNDS = 40) as a user-editable global setting.
// Raising it lets long workflows (e.g. populating a multi-section report)
// finish without hitting the summary-fallback path; lowering it caps the
// blast radius of a runaway model.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v030_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v030(QSqlDatabase& db) {
    // SQLite has no ADD COLUMN IF NOT EXISTS — guard via PRAGMA.
    QSqlQuery probe(db);
    if (!probe.exec("PRAGMA table_info(llm_global_settings)"))
        return Result<void>::err(probe.lastError().text().toStdString());
    bool has_col = false;
    while (probe.next()) {
        if (probe.value(1).toString() == QStringLiteral("max_tool_rounds")) {
            has_col = true;
            break;
        }
    }
    if (!has_col) {
        if (auto r = v030_sql(db,
                "ALTER TABLE llm_global_settings ADD COLUMN max_tool_rounds INTEGER DEFAULT 40");
            r.is_err())
            return r;
    }
    // Backfill the singleton row in case ADD COLUMN ran but DEFAULT wasn't picked up
    // (older SQLite versions populate NULL for existing rows even with a DEFAULT).
    return v030_sql(db,
            "UPDATE llm_global_settings SET max_tool_rounds = 40 "
            "WHERE max_tool_rounds IS NULL");
}

} // anonymous namespace

void register_migration_v030() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({30, "max_tool_rounds", apply_v030});
}

} // namespace fincept
