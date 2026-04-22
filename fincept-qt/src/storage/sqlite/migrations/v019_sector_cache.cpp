// v019_sector_cache — Adds sector column to portfolio_assets and creates the
// sector_cache table used by SectorResolver to memoize yfinance sector lookups
// across app launches.

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

static bool column_exists(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    q.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

Result<void> apply_v019(QSqlDatabase& db) {
    if (!column_exists(db, "portfolio_assets", "sector")) {
        auto r = sql(db, "ALTER TABLE portfolio_assets ADD COLUMN sector TEXT DEFAULT ''");
        if (r.is_err())
            return r;
    }

    auto r = sql(db, "CREATE TABLE IF NOT EXISTS sector_cache ("
                     "  symbol      TEXT PRIMARY KEY,"
                     "  sector      TEXT NOT NULL DEFAULT '',"
                     "  industry    TEXT DEFAULT '',"
                     "  quote_type  TEXT DEFAULT '',"
                     "  resolved_at INTEGER NOT NULL"
                     ")");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v019() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({19, "sector_cache", apply_v019});
}

} // namespace fincept
