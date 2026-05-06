// v023_algo_trading_deploy — Algo Trading deployment + order routing schema.
//
// Background: prior to this migration, algo_trading tables were created
// lazily by `scripts/algo_trading/backtest_engine.py::cmd_save_strategy`
// (only `algo_strategies`) and the runner inserted into `algo_deployments`,
// `algo_order_signals`, `algo_metrics`, `algo_trades` blind — those tables
// were never actually created. The runner had been failing silently because
// no UI ever invoked the deploy path.
//
// This migration moves schema ownership to C++ and adds the columns required
// for the new deploy flow (broker selection, multiple backends, paper
// portfolio link, per-deployment risk limits, heartbeat for crash recovery).
//
// Idempotency: dev databases that previously had `algo_strategies` created
// by the Python path will retain that table; ADD COLUMN guards via PRAGMA
// table_info skip already-present columns.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v023_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

static bool v023_column_exists(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    q.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

static Result<void> v023_add_column_if_missing(QSqlDatabase& db, const QString& table, const QString& column,
                                                const QString& decl) {
    if (v023_column_exists(db, table, column))
        return Result<void>::ok();
    const auto stmt = QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, decl).toUtf8();
    return v023_sql(db, stmt.constData());
}

Result<void> apply_v023(QSqlDatabase& db) {
    // ── Strategy registry (DSL strategies) ──────────────────────────────────
    // QC strategies live in scripts/strategies/_registry.py — not in SQL.
    // This table only holds DSL strategies built in StrategyBuilderPanel.
    {
        auto r = v023_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS algo_strategies (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                timeframe TEXT DEFAULT '1d',
                entry_conditions TEXT DEFAULT '[]',
                exit_conditions TEXT DEFAULT '[]',
                entry_logic TEXT DEFAULT 'AND',
                exit_logic TEXT DEFAULT 'AND',
                stop_loss REAL DEFAULT 0,
                take_profit REAL DEFAULT 0,
                trailing_stop REAL DEFAULT 0,
                trailing_stop_type TEXT DEFAULT 'percent',
                is_active INTEGER DEFAULT 1,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err())
            return r;
    }

    // ── Deployments ─────────────────────────────────────────────────────────
    {
        auto r = v023_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS algo_deployments (
                id TEXT PRIMARY KEY,
                strategy_id TEXT NOT NULL,
                strategy_name TEXT DEFAULT '',
                strategy_kind TEXT DEFAULT 'dsl',           -- 'dsl' | 'qc'
                symbol TEXT NOT NULL,
                exchange TEXT DEFAULT '',
                product_type TEXT DEFAULT '',
                mode TEXT NOT NULL DEFAULT 'paper',         -- 'paper' | 'live'
                backend TEXT NOT NULL DEFAULT 'paper',      -- 'paper' | 'equity_broker' | 'crypto_exchange'
                broker_id TEXT DEFAULT '',
                broker_account_id TEXT DEFAULT '',
                paper_portfolio_id TEXT DEFAULT '',
                timeframe TEXT DEFAULT '5m',
                quantity REAL NOT NULL DEFAULT 1,
                max_order_value REAL DEFAULT 0,
                max_daily_loss REAL DEFAULT 0,
                status TEXT NOT NULL DEFAULT 'pending',     -- pending|starting|running|stopped|error|crashed
                error_message TEXT DEFAULT '',
                pid INTEGER DEFAULT 0,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err())
            return r;
    }

    // For DBs that pre-existed (e.g. created by an older runner build), add
    // the new columns idempotently. CREATE TABLE IF NOT EXISTS above will
    // skip when present, leaving older schemas needing ALTER.
    struct ColumnSpec {
        const char* table;
        const char* column;
        const char* decl;
    };
    const ColumnSpec deployment_cols[] = {
        {"algo_deployments", "strategy_kind",      "TEXT DEFAULT 'dsl'"},
        {"algo_deployments", "exchange",           "TEXT DEFAULT ''"},
        {"algo_deployments", "product_type",       "TEXT DEFAULT ''"},
        {"algo_deployments", "backend",            "TEXT DEFAULT 'paper'"},
        {"algo_deployments", "broker_id",          "TEXT DEFAULT ''"},
        {"algo_deployments", "broker_account_id",  "TEXT DEFAULT ''"},
        {"algo_deployments", "paper_portfolio_id", "TEXT DEFAULT ''"},
        {"algo_deployments", "max_order_value",    "REAL DEFAULT 0"},
        {"algo_deployments", "max_daily_loss",     "REAL DEFAULT 0"},
        {"algo_deployments", "pid",                "INTEGER DEFAULT 0"},
    };
    for (const auto& c : deployment_cols) {
        auto r = v023_add_column_if_missing(db, c.table, c.column, c.decl);
        if (r.is_err())
            return r;
    }

    // ── Order signals ───────────────────────────────────────────────────────
    {
        auto r = v023_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS algo_order_signals (
                id TEXT PRIMARY KEY,
                deployment_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                exchange TEXT DEFAULT '',
                product_type TEXT DEFAULT '',
                side TEXT NOT NULL,                         -- BUY | SELL
                quantity REAL NOT NULL,
                order_type TEXT NOT NULL DEFAULT 'MARKET',  -- MARKET | LIMIT | STOP_MARKET | STOP_LIMIT
                price REAL DEFAULT NULL,
                trigger_price REAL DEFAULT NULL,
                status TEXT NOT NULL DEFAULT 'pending',     -- pending|claimed|dispatched|filled|partial|failed
                broker_order_id TEXT DEFAULT '',
                error TEXT DEFAULT '',
                attempt_count INTEGER DEFAULT 0,
                dispatched_at TEXT DEFAULT NULL,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err())
            return r;
    }
    const ColumnSpec signal_cols[] = {
        {"algo_order_signals", "exchange",        "TEXT DEFAULT ''"},
        {"algo_order_signals", "product_type",    "TEXT DEFAULT ''"},
        {"algo_order_signals", "trigger_price",   "REAL DEFAULT NULL"},
        {"algo_order_signals", "broker_order_id", "TEXT DEFAULT ''"},
        {"algo_order_signals", "error",           "TEXT DEFAULT ''"},
        {"algo_order_signals", "attempt_count",   "INTEGER DEFAULT 0"},
        {"algo_order_signals", "dispatched_at",   "TEXT DEFAULT NULL"},
    };
    for (const auto& c : signal_cols) {
        auto r = v023_add_column_if_missing(db, c.table, c.column, c.decl);
        if (r.is_err())
            return r;
    }

    // Index for the dispatcher's hot-path query
    {
        auto r = v023_sql(db, "CREATE INDEX IF NOT EXISTS idx_algo_order_signals_status "
                              "ON algo_order_signals(status, created_at)");
        if (r.is_err())
            return r;
    }
    {
        auto r = v023_sql(db, "CREATE INDEX IF NOT EXISTS idx_algo_order_signals_deployment "
                              "ON algo_order_signals(deployment_id)");
        if (r.is_err())
            return r;
    }

    // ── Per-deployment metrics (UPSERT key: deployment_id) ──────────────────
    {
        auto r = v023_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS algo_metrics (
                deployment_id TEXT PRIMARY KEY,
                total_pnl REAL DEFAULT 0,
                unrealized_pnl REAL DEFAULT 0,
                total_trades INTEGER DEFAULT 0,
                win_rate REAL DEFAULT 0,
                max_drawdown REAL DEFAULT 0,
                current_position_qty REAL DEFAULT 0,
                current_position_side TEXT DEFAULT '',
                current_position_entry REAL DEFAULT 0,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err())
            return r;
    }

    // ── Trade audit log (paper + live) ──────────────────────────────────────
    {
        auto r = v023_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS algo_trades (
                id TEXT PRIMARY KEY,
                deployment_id TEXT NOT NULL,
                symbol TEXT NOT NULL,
                side TEXT NOT NULL,
                quantity REAL NOT NULL,
                price REAL NOT NULL,
                pnl REAL DEFAULT 0,
                signal_reason TEXT DEFAULT '',
                broker_order_id TEXT DEFAULT '',
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err())
            return r;
    }
    {
        auto r = v023_sql(db, "CREATE INDEX IF NOT EXISTS idx_algo_trades_deployment "
                              "ON algo_trades(deployment_id, created_at)");
        if (r.is_err())
            return r;
    }
    // For older trade tables, ensure broker_order_id column exists.
    {
        auto r = v023_add_column_if_missing(db, "algo_trades", "broker_order_id", "TEXT DEFAULT ''");
        if (r.is_err())
            return r;
    }

    // ── Heartbeat (crash recovery) ──────────────────────────────────────────
    // Runner upserts every 5s. AlgoTradingService reaps deployments where
    // last_beat is older than 30s and status='running' on app start.
    {
        auto r = v023_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS algo_deployment_heartbeat (
                deployment_id TEXT PRIMARY KEY,
                pid INTEGER DEFAULT 0,
                last_beat TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )sql");
        if (r.is_err())
            return r;
    }

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v023() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({23, "algo_trading_deploy", apply_v023});
}

} // namespace fincept
