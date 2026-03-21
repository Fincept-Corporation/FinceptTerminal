// v001_initial — Creates all Phase 1 tables.
// Replaces the old inline run_migrations() approach.

#include "storage/sqlite/migrations/MigrationRunner.h"
#include <QSqlQuery>
#include <QSqlError>

namespace fincept {
namespace {

/// Helper: exec a SQL statement on the given db, return Result<void>.
static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt)) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> apply_v001(QSqlDatabase& db) {

    // ── Settings ─────────────────────────────────────────────────────────────
    auto r = sql(db,
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT,"
        "  category TEXT DEFAULT 'general',"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Credentials (API keys for data providers) ────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS credentials ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  service_name TEXT NOT NULL UNIQUE,"
        "  username TEXT,"
        "  password TEXT,"
        "  api_key TEXT,"
        "  api_secret TEXT,"
        "  additional_data TEXT,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Key-Value Storage (generic) ──────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS key_value_storage ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ")");
    if (r.is_err()) return r;

    // ── Watchlists (parent) ──────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS watchlists ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  description TEXT DEFAULT '',"
        "  color TEXT DEFAULT '#FF6600',"
        "  sort_order INTEGER DEFAULT 0,"
        "  is_default INTEGER DEFAULT 0,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Watchlist Stocks (child) ─────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS watchlist_stocks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  watchlist_id TEXT NOT NULL REFERENCES watchlists(id) ON DELETE CASCADE,"
        "  symbol TEXT NOT NULL,"
        "  name TEXT DEFAULT '',"
        "  exchange TEXT DEFAULT '',"
        "  notes TEXT,"
        "  sort_order INTEGER DEFAULT 0,"
        "  added_at TEXT DEFAULT (datetime('now')),"
        "  UNIQUE(watchlist_id, symbol)"
        ")");
    if (r.is_err()) return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_wl_stocks_wl ON watchlist_stocks(watchlist_id)");
    if (r.is_err()) return r;

    // ── Financial Notes ──────────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS financial_notes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  content TEXT NOT NULL DEFAULT '',"
        "  category TEXT NOT NULL DEFAULT 'general',"
        "  priority TEXT DEFAULT 'MEDIUM',"
        "  tags TEXT,"
        "  tickers TEXT,"
        "  sentiment TEXT DEFAULT 'NEUTRAL',"
        "  is_favorite INTEGER DEFAULT 0,"
        "  is_archived INTEGER DEFAULT 0,"
        "  color_code TEXT DEFAULT '#FF8800',"
        "  attachments TEXT,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now')),"
        "  reminder_date TEXT,"
        "  word_count INTEGER DEFAULT 0"
        ")");
    if (r.is_err()) return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_notes_category ON financial_notes(category)");
    if (r.is_err()) return r;

    // ── Note Templates ───────────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS note_templates ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  description TEXT,"
        "  content TEXT NOT NULL,"
        "  category TEXT NOT NULL DEFAULT 'general',"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Reports ──────────────────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS reports ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  content_json TEXT,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Report Templates ─────────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS report_templates ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  description TEXT,"
        "  template_data TEXT NOT NULL,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Portfolio Holdings (fixes missing table bug) ─────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS portfolio_holdings ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  symbol TEXT NOT NULL,"
        "  name TEXT DEFAULT '',"
        "  shares REAL NOT NULL DEFAULT 0,"
        "  avg_cost REAL NOT NULL DEFAULT 0,"
        "  active INTEGER DEFAULT 1,"
        "  added_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading: Portfolios ────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS pt_portfolios ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  initial_balance REAL NOT NULL,"
        "  balance REAL NOT NULL,"
        "  currency TEXT DEFAULT 'USD',"
        "  leverage REAL DEFAULT 1.0,"
        "  margin_mode TEXT DEFAULT 'cross',"
        "  fee_rate REAL DEFAULT 0.001,"
        "  exchange TEXT DEFAULT '',"
        "  created_at TEXT NOT NULL"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading: Orders ────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS pt_orders ("
        "  id TEXT PRIMARY KEY,"
        "  portfolio_id TEXT NOT NULL REFERENCES pt_portfolios(id) ON DELETE CASCADE,"
        "  symbol TEXT NOT NULL,"
        "  side TEXT NOT NULL,"
        "  order_type TEXT NOT NULL,"
        "  quantity REAL NOT NULL,"
        "  price REAL,"
        "  stop_price REAL,"
        "  filled_qty REAL DEFAULT 0,"
        "  avg_price REAL,"
        "  status TEXT DEFAULT 'pending',"
        "  reduce_only INTEGER DEFAULT 0,"
        "  time_in_force TEXT DEFAULT 'GTC',"
        "  product TEXT,"
        "  exchange TEXT,"
        "  created_at TEXT NOT NULL,"
        "  filled_at TEXT"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading: Positions ─────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS pt_positions ("
        "  id TEXT PRIMARY KEY,"
        "  portfolio_id TEXT NOT NULL REFERENCES pt_portfolios(id) ON DELETE CASCADE,"
        "  symbol TEXT NOT NULL,"
        "  side TEXT NOT NULL,"
        "  quantity REAL NOT NULL,"
        "  entry_price REAL NOT NULL,"
        "  current_price REAL DEFAULT 0,"
        "  unrealized_pnl REAL DEFAULT 0,"
        "  realized_pnl REAL DEFAULT 0,"
        "  leverage REAL DEFAULT 1.0,"
        "  margin_mode TEXT DEFAULT 'cross',"
        "  liquidation_price REAL,"
        "  opened_at TEXT NOT NULL,"
        "  closed_at TEXT,"
        "  status TEXT DEFAULT 'open'"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading: Trades ────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS pt_trades ("
        "  id TEXT PRIMARY KEY,"
        "  portfolio_id TEXT NOT NULL REFERENCES pt_portfolios(id) ON DELETE CASCADE,"
        "  order_id TEXT NOT NULL REFERENCES pt_orders(id) ON DELETE CASCADE,"
        "  symbol TEXT NOT NULL,"
        "  side TEXT NOT NULL,"
        "  price REAL NOT NULL,"
        "  quantity REAL NOT NULL,"
        "  fee REAL DEFAULT 0,"
        "  pnl REAL DEFAULT 0,"
        "  timestamp TEXT NOT NULL"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading: Margin Blocks ─────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS pt_margin_blocks ("
        "  id TEXT PRIMARY KEY,"
        "  portfolio_id TEXT NOT NULL REFERENCES pt_portfolios(id) ON DELETE CASCADE,"
        "  order_id TEXT NOT NULL UNIQUE REFERENCES pt_orders(id) ON DELETE CASCADE,"
        "  symbol TEXT NOT NULL,"
        "  blocked_amount REAL NOT NULL,"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading: Holdings (T+1 settlement) ─────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS pt_holdings ("
        "  id TEXT PRIMARY KEY,"
        "  portfolio_id TEXT NOT NULL REFERENCES pt_portfolios(id) ON DELETE CASCADE,"
        "  symbol TEXT NOT NULL,"
        "  quantity REAL NOT NULL DEFAULT 0,"
        "  average_price REAL NOT NULL DEFAULT 0,"
        "  invested_value REAL NOT NULL DEFAULT 0,"
        "  current_price REAL NOT NULL DEFAULT 0,"
        "  current_value REAL NOT NULL DEFAULT 0,"
        "  pnl REAL DEFAULT 0,"
        "  pnl_percent REAL DEFAULT 0,"
        "  t1_quantity REAL DEFAULT 0,"
        "  available_quantity REAL DEFAULT 0,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now')),"
        "  UNIQUE(portfolio_id, symbol)"
        ")");
    if (r.is_err()) return r;

    // ── Paper Trading Indexes ────────────────────────────────────────────────
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_orders_portfolio ON pt_orders(portfolio_id)");
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_orders_status ON pt_orders(status)");
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_positions_portfolio ON pt_positions(portfolio_id)");
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_positions_status ON pt_positions(status)");
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_trades_portfolio ON pt_trades(portfolio_id)");
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_margin_blocks_portfolio ON pt_margin_blocks(portfolio_id)");
    sql(db, "CREATE INDEX IF NOT EXISTS idx_pt_holdings_portfolio ON pt_holdings(portfolio_id)");

    // ── News Monitors (moved from inline ensure_table) ───────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS news_monitors ("
        "  id TEXT PRIMARY KEY,"
        "  label TEXT NOT NULL,"
        "  keywords TEXT NOT NULL,"
        "  color TEXT NOT NULL,"
        "  enabled INTEGER NOT NULL DEFAULT 1,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── User RSS Feeds ───────────────────────────────────────────────────────
    r = sql(db,
        "CREATE TABLE IF NOT EXISTS user_rss_feeds ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  url TEXT NOT NULL UNIQUE,"
        "  category TEXT NOT NULL DEFAULT 'GENERAL',"
        "  region TEXT NOT NULL DEFAULT 'GLOBAL',"
        "  source_name TEXT NOT NULL DEFAULT '',"
        "  enabled INTEGER DEFAULT 1,"
        "  is_default INTEGER DEFAULT 0,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  updated_at TEXT DEFAULT (datetime('now'))"
        ")");
    if (r.is_err()) return r;

    // ── Migrate data from old flat watchlist table if it exists ───────────────
    {
        QSqlQuery check(db);
        if (check.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='watchlist'")
            && check.next()) {
            // Old table exists — migrate data
            QSqlQuery migrate(db);

            // Create a default watchlist
            migrate.exec(
                "INSERT OR IGNORE INTO watchlists (id, name, color, is_default) "
                "VALUES ('default', 'Default', '#FF6600', 1)");

            // Copy symbols
            migrate.exec(
                "INSERT OR IGNORE INTO watchlist_stocks (watchlist_id, symbol, name, added_at) "
                "SELECT 'default', symbol, COALESCE(name, ''), added_at FROM watchlist");

            // Drop old table
            migrate.exec("DROP TABLE IF EXISTS watchlist");
        }
    }

    // ── Drop old migrations tracking table (replaced by schema_version) ──────
    sql(db, "DROP TABLE IF EXISTS migrations");

    // ── Migrate old notes table data if it exists ────────────────────────────
    {
        QSqlQuery check(db);
        if (check.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='notes'")
            && check.next()) {
            QSqlQuery migrate(db);
            migrate.exec(
                "INSERT OR IGNORE INTO financial_notes (title, content, category, created_at, updated_at) "
                "SELECT title, COALESCE(content, ''), 'general', created_at, updated_at FROM notes");
            migrate.exec("DROP TABLE IF EXISTS notes");
        }
    }

    return Result<void>::ok();
}

// Auto-register at static initialization time.
static const bool registered_v001 = [] {
    MigrationRunner::register_migration({1, "initial_schema", apply_v001});
    return true;
}();

} // anonymous namespace
} // namespace fincept
