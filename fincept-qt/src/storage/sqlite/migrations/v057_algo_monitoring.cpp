// v057_algo_monitoring — Algo Trading: honest costs, persisted risk state and
// live monitoring history.
//
//   algo_trades       + reason, latency_ms  (DeploymentRunner::persist_trade has
//                       always INSERTed these two columns, but no migration ever
//                       created them — v023 named the column signal_reason — so
//                       every trade row failed to insert and the trade log was
//                       empty), + fees, gross_pnl, ref_price, mode, ts_ms
//   algo_strategies   + position_size_pct, cost_model_json, entry_side,
//                       risk_free_pct, last_backtest_json, last_backtest_at
//   algo_deployments  + risk_json (drawdown / trades-per-day / window limits),
//                       cost_model_json (fee model used for paper fills and the
//                       estimated fees deducted from live P&L)
//   algo_metrics      + daily_pnl, daily_date, peak_equity, risk_halted,
//                       halt_reason, total_fees, trades_today — the daily-loss
//                       state survives a restart and rolls over on the
//                       EXCHANGE trading date
//   algo_equity_snapshots (new) — per-deployment mark-to-market P&L over time
//   algo_events           (new) — per-deployment signal / order / risk log
//
// SAFE ON A POPULATED DATABASE: ADD COLUMN with defaults and new tables only;
// a column that already exists is skipped.
//
// REGISTRATION: declare `void register_migration_v057();` in MigrationRunner.h
// and call it from main.cpp after register_migration_v056().

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> sql_v057(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromUtf8(stmt)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

bool v057_has_column(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return false;
    while (q.next())
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

Result<void> v057_add_column(QSqlDatabase& db, const char* table, const char* column, const char* decl) {
    const QString t = QString::fromUtf8(table);
    const QString c = QString::fromUtf8(column);
    if (v057_has_column(db, t, c))
        return Result<void>::ok();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(t, c, QString::fromUtf8(decl)))) {
        const QString err = q.lastError().text();
        if (!err.contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive))
            return Result<void>::err(err.toStdString());
    }
    return Result<void>::ok();
}

Result<void> apply_v057(QSqlDatabase& db) {
    struct Col {
        const char* table;
        const char* column;
        const char* decl;
    };
    const Col cols[] = {
        {"algo_trades", "reason", "TEXT DEFAULT ''"},
        {"algo_trades", "latency_ms", "INTEGER DEFAULT 0"},
        {"algo_trades", "fees", "REAL DEFAULT 0"},
        {"algo_trades", "gross_pnl", "REAL DEFAULT 0"},
        {"algo_trades", "ref_price", "REAL DEFAULT 0"},
        {"algo_trades", "mode", "TEXT DEFAULT ''"},
        {"algo_trades", "ts_ms", "INTEGER DEFAULT 0"},

        {"algo_strategies", "position_size_pct", "REAL DEFAULT 100"},
        {"algo_strategies", "cost_model_json", "TEXT DEFAULT '{}'"},
        {"algo_strategies", "entry_side", "TEXT DEFAULT 'BUY'"},
        {"algo_strategies", "risk_free_pct", "REAL DEFAULT 0"},
        {"algo_strategies", "last_backtest_json", "TEXT DEFAULT '{}'"},
        {"algo_strategies", "last_backtest_at", "TEXT DEFAULT ''"},

        {"algo_deployments", "risk_json", "TEXT DEFAULT '{}'"},
        {"algo_deployments", "cost_model_json", "TEXT DEFAULT '{}'"},

        {"algo_metrics", "daily_pnl", "REAL DEFAULT 0"},
        {"algo_metrics", "daily_date", "INTEGER DEFAULT 0"},
        {"algo_metrics", "peak_equity", "REAL DEFAULT 0"},
        {"algo_metrics", "risk_halted", "INTEGER DEFAULT 0"},
        {"algo_metrics", "halt_reason", "TEXT DEFAULT ''"},
        {"algo_metrics", "total_fees", "REAL DEFAULT 0"},
        {"algo_metrics", "trades_today", "INTEGER DEFAULT 0"},
    };
    for (const auto& c : cols) {
        auto r = v057_add_column(db, c.table, c.column, c.decl);
        if (r.is_err())
            return r;
    }

    auto r = sql_v057(db, "CREATE TABLE IF NOT EXISTS algo_equity_snapshots ("
                          "  deployment_id TEXT NOT NULL,"
                          "  ts_ms         INTEGER NOT NULL,"   // epoch ms
                          "  equity        REAL NOT NULL DEFAULT 0," // realized + unrealized P&L
                          "  realized      REAL NOT NULL DEFAULT 0,"
                          "  unrealized    REAL NOT NULL DEFAULT 0,"
                          "  fees          REAL NOT NULL DEFAULT 0,"
                          "  price         REAL NOT NULL DEFAULT 0,"
                          "  position_qty  REAL NOT NULL DEFAULT 0,"
                          "  PRIMARY KEY (deployment_id, ts_ms)"
                          ")");
    if (r.is_err())
        return r;

    r = sql_v057(db, "CREATE TABLE IF NOT EXISTS algo_events ("
                     "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  deployment_id TEXT NOT NULL,"
                     "  ts_ms         INTEGER NOT NULL,"
                     "  kind          TEXT NOT NULL DEFAULT 'info'," // signal|order|fill|reject|risk|info|error
                     "  message       TEXT NOT NULL DEFAULT ''"
                     ")");
    if (r.is_err())
        return r;
    return sql_v057(db, "CREATE INDEX IF NOT EXISTS idx_algo_events_deployment ON algo_events(deployment_id, ts_ms)");
}

} // anonymous namespace

void register_migration_v057() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({57, "algo_monitoring", apply_v057});
}

} // namespace fincept
