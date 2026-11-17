// v064_ma_alt_workbench — the M&A Analytics and Alternative Investments workbench tables (one
// migration for both tabs):
//
//   ma_cases           M&A saved cases: a named copy of one module's inputs (and the result shown
//                      when it was saved), reopened and compared side by side
//                      (services/ma_analytics/MaCaseStore).
//   alt_funds          the private-markets fund book: one row per fund the user tracks
//   alt_fund_flows     its dated capital calls, distributions, NAV statements and fees; amounts are
//                      positive, the kind gives the direction (services/alt_investments/AltFundStore)
//   alt_holdings       alternative holdings entered by hand for the allocation & liquidity view,
//                      with their liquidity terms
//   alt_dd_checklists  named due-diligence checklists: the template they follow and each item's
//                      status and note as JSON
//
// Local only: none of these tables is part of cloud sync or the workspace export (fund cash flows
// are the user's own private data). All timestamps are epoch milliseconds.
//
// SAFE ON A POPULATED DATABASE AND IDEMPOTENT: only CREATE TABLE / INDEX IF NOT EXISTS; nothing
// existing is altered. MigrationRunner::inspect() builds its reference schema with this too.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after register_migration_v063(),
// listed in LegacyRootAdoption.cpp register_known_migrations() (kKnownHighestMigration).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v064();

namespace {

Result<void> apply_v064(QSqlDatabase& db) {
    static const char* const kStatements[] = {
        "CREATE TABLE IF NOT EXISTS ma_cases ("
        "  id          TEXT PRIMARY KEY,"
        "  name        TEXT NOT NULL,"
        "  module      TEXT NOT NULL,"
        "  context     TEXT NOT NULL DEFAULT '',"
        "  inputs_json TEXT NOT NULL DEFAULT '{}',"
        "  result_json TEXT NOT NULL DEFAULT '{}',"
        "  notes       TEXT NOT NULL DEFAULT '',"
        "  created_at  INTEGER NOT NULL,"
        "  updated_at  INTEGER NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_ma_cases_module ON ma_cases(module, updated_at)",

        "CREATE TABLE IF NOT EXISTS alt_funds ("
        "  id           TEXT PRIMARY KEY,"
        "  name         TEXT NOT NULL,"
        "  strategy     TEXT NOT NULL DEFAULT '',"
        "  vintage_year INTEGER NOT NULL DEFAULT 0,"
        "  commitment   REAL NOT NULL DEFAULT 0,"
        "  currency     TEXT NOT NULL DEFAULT 'USD',"
        "  benchmark    TEXT NOT NULL DEFAULT '',"
        "  notes        TEXT NOT NULL DEFAULT '',"
        "  created_at   INTEGER NOT NULL,"
        "  updated_at   INTEGER NOT NULL"
        ")",
        "CREATE TABLE IF NOT EXISTS alt_fund_flows ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  fund_id   TEXT NOT NULL,"
        "  flow_date TEXT NOT NULL,"
        "  kind      TEXT NOT NULL,"
        "  amount    REAL NOT NULL,"
        "  note      TEXT NOT NULL DEFAULT ''"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_alt_fund_flows_fund ON alt_fund_flows(fund_id, flow_date)",

        "CREATE TABLE IF NOT EXISTS alt_holdings ("
        "  id           TEXT PRIMARY KEY,"
        "  name         TEXT NOT NULL,"
        "  asset_class  TEXT NOT NULL DEFAULT 'other',"
        "  value        REAL NOT NULL DEFAULT 0,"
        "  currency     TEXT NOT NULL DEFAULT 'USD',"
        "  liquidity    TEXT NOT NULL DEFAULT 'locked',"
        "  notice_days  INTEGER NOT NULL DEFAULT 0,"
        "  lockup_end   TEXT NOT NULL DEFAULT '',"
        "  unfunded     REAL NOT NULL DEFAULT 0,"
        "  portfolio_id TEXT NOT NULL DEFAULT '',"
        "  notes        TEXT NOT NULL DEFAULT '',"
        "  created_at   INTEGER NOT NULL,"
        "  updated_at   INTEGER NOT NULL"
        ")",

        "CREATE TABLE IF NOT EXISTS alt_dd_checklists ("
        "  id         TEXT PRIMARY KEY,"
        "  name       TEXT NOT NULL,"
        "  template   TEXT NOT NULL,"
        "  items_json TEXT NOT NULL DEFAULT '[]',"
        "  created_at INTEGER NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ")",
    };
    for (const char* stmt : kStatements) {
        QSqlQuery q(db);
        if (!q.exec(QString::fromLatin1(stmt)))
            return Result<void>::err(
                QStringLiteral("v064: %1 — %2").arg(QString::fromLatin1(stmt).left(80), q.lastError().text()).toStdString());
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v064() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({64, "ma_alt_workbench", apply_v064});
}

} // namespace fincept
