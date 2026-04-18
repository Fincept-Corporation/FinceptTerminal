// v018_widget_config — Adds config_json column to dashboard_widget_instances
// so each tile can persist per-instance widget configuration (broker/account
// pickers, symbol filters, agent ids, etc.).

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

Result<void> apply_v018(QSqlDatabase& db) {
    auto r = sql(db, "ALTER TABLE dashboard_widget_instances ADD COLUMN config_json TEXT DEFAULT '{}'");
    if (r.is_err()) {
        // Column may already exist if a dev ran a pre-release build — tolerate
        QSqlQuery check(db);
        check.exec("SELECT config_json FROM dashboard_widget_instances LIMIT 1");
        if (check.lastError().isValid())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v018() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({18, "widget_config", apply_v018});
}

} // namespace fincept
