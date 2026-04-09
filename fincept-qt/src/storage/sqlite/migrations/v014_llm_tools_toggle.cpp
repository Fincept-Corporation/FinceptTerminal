// v014_llm_tools_toggle — Add tools_enabled column to llm_configs.
// Allows per-provider control over whether MCP tools are sent to the LLM.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v014(QSqlDatabase& db) {
    // Add tools_enabled column — defaults to 1 (enabled) for all providers
    auto r = sql(db, "ALTER TABLE llm_configs ADD COLUMN tools_enabled INTEGER DEFAULT 1");
    if (r.is_err()) {
        // Column may already exist if user ran a dev build — ignore
        QSqlQuery check(db);
        check.exec("SELECT tools_enabled FROM llm_configs LIMIT 1");
        if (check.lastError().isValid())
            return r; // real error
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v014() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({14, "llm_tools_toggle", apply_v014});
}

} // namespace fincept
