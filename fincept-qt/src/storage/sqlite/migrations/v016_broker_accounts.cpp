// v016_broker_accounts — Add broker_accounts table for multi-account support.
// Each row represents one authenticated broker account (a user can have
// multiple accounts for the same broker type).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v016(QSqlDatabase& db) {
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS broker_accounts ("
                     "  id TEXT PRIMARY KEY,"
                     "  broker_id TEXT NOT NULL,"
                     "  display_name TEXT NOT NULL,"
                     "  paper_portfolio_id TEXT,"
                     "  trading_mode TEXT DEFAULT 'paper',"
                     "  is_active INTEGER DEFAULT 1,"
                     "  created_at TEXT NOT NULL,"
                     "  FOREIGN KEY (paper_portfolio_id) REFERENCES pt_portfolios(id) ON DELETE SET NULL"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_broker_accounts_broker ON broker_accounts(broker_id)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v016() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({16, "broker_accounts", apply_v016});
}

} // namespace fincept
