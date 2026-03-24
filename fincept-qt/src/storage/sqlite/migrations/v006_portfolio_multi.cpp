// v006_portfolio_multi — Multi-portfolio support with transactions and snapshots.
// Migrates existing portfolio_holdings data into the new schema.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace fincept {
namespace {

static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v006(QSqlDatabase& db) {

    // ── Portfolios ────────────────────────────────────────────────────────────
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS portfolios ("
                     "  id          TEXT PRIMARY KEY,"
                     "  name        TEXT NOT NULL,"
                     "  owner       TEXT NOT NULL DEFAULT '',"
                     "  currency    TEXT NOT NULL DEFAULT 'USD',"
                     "  description TEXT DEFAULT '',"
                     "  created_at  TEXT DEFAULT (datetime('now')),"
                     "  updated_at  TEXT DEFAULT (datetime('now'))"
                     ")");
    if (r.is_err())
        return r;

    // ── Portfolio Assets ──────────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS portfolio_assets ("
                "  id                  INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  portfolio_id        TEXT NOT NULL REFERENCES portfolios(id) ON DELETE CASCADE,"
                "  symbol              TEXT NOT NULL,"
                "  quantity            REAL NOT NULL DEFAULT 0,"
                "  avg_buy_price       REAL NOT NULL DEFAULT 0,"
                "  first_purchase_date TEXT DEFAULT (datetime('now')),"
                "  last_updated        TEXT DEFAULT (datetime('now')),"
                "  UNIQUE(portfolio_id, symbol)"
                ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_pa_portfolio ON portfolio_assets(portfolio_id)");
    if (r.is_err())
        return r;

    // ── Portfolio Transactions ─────────────────────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS portfolio_transactions ("
                "  id                TEXT PRIMARY KEY,"
                "  portfolio_id      TEXT NOT NULL REFERENCES portfolios(id) ON DELETE CASCADE,"
                "  symbol            TEXT NOT NULL,"
                "  transaction_type  TEXT NOT NULL CHECK(transaction_type IN ('BUY','SELL','DIVIDEND','SPLIT')),"
                "  quantity          REAL NOT NULL,"
                "  price             REAL NOT NULL,"
                "  total_value       REAL NOT NULL,"
                "  transaction_date  TEXT NOT NULL,"
                "  notes             TEXT DEFAULT '',"
                "  created_at        TEXT DEFAULT (datetime('now'))"
                ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_ptx_portfolio ON portfolio_transactions(portfolio_id)");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_ptx_date ON portfolio_transactions(transaction_date)");
    if (r.is_err())
        return r;

    // ── Portfolio Snapshots (daily NAV history) ────────────────────────────────
    r = sql(db, "CREATE TABLE IF NOT EXISTS portfolio_snapshots ("
                "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  portfolio_id      TEXT NOT NULL REFERENCES portfolios(id) ON DELETE CASCADE,"
                "  total_value       REAL NOT NULL,"
                "  total_cost_basis  REAL NOT NULL,"
                "  total_pnl         REAL NOT NULL,"
                "  total_pnl_percent REAL NOT NULL,"
                "  snapshot_date     TEXT NOT NULL,"
                "  created_at        TEXT DEFAULT (datetime('now')),"
                "  UNIQUE(portfolio_id, snapshot_date)"
                ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_ps_portfolio ON portfolio_snapshots(portfolio_id)");
    if (r.is_err())
        return r;

    // ── Migrate existing portfolio_holdings into new schema ────────────────────
    {
        QSqlQuery check(db);
        if (check.exec("SELECT COUNT(*) FROM portfolio_holdings WHERE active = 1") && check.next() &&
            check.value(0).toInt() > 0) {

            // Create a "Default" portfolio for existing holdings
            QString default_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QSqlQuery ins(db);
            ins.prepare("INSERT OR IGNORE INTO portfolios (id, name, owner, currency) "
                        "VALUES (?, 'Default', 'User', 'USD')");
            ins.addBindValue(default_id);
            ins.exec();

            // Copy active holdings into portfolio_assets
            QSqlQuery copy(db);
            copy.prepare("INSERT OR IGNORE INTO portfolio_assets "
                         "(portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date) "
                         "SELECT ?, symbol, shares, avg_cost, added_at "
                         "FROM portfolio_holdings WHERE active = 1");
            copy.addBindValue(default_id);
            copy.exec();

            // Create matching BUY transactions for history
            QSqlQuery holdings(db);
            holdings.exec("SELECT symbol, shares, avg_cost, added_at FROM portfolio_holdings WHERE active = 1");
            while (holdings.next()) {
                QString txn_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                double qty = holdings.value(1).toDouble();
                double price = holdings.value(2).toDouble();

                QSqlQuery txn(db);
                txn.prepare("INSERT INTO portfolio_transactions "
                            "(id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
                            " transaction_date, notes) "
                            "VALUES (?, ?, ?, 'BUY', ?, ?, ?, ?, 'Migrated from legacy holdings')");
                txn.addBindValue(txn_id);
                txn.addBindValue(default_id);
                txn.addBindValue(holdings.value(0).toString()); // symbol
                txn.addBindValue(qty);
                txn.addBindValue(price);
                txn.addBindValue(qty * price);
                txn.addBindValue(holdings.value(3).toString()); // added_at as date
                txn.exec();
            }
        }
    }

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v006() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({6, "portfolio_multi", apply_v006});
}

} // namespace fincept
