#include "database.h"
#include "screens/algo_trading/algo_types.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <random>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace fincept::db {

// ============================================================================
// Utility
// ============================================================================
int64_t now_timestamp() {
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string generate_uuid() {
    static std::mutex rng_mutex;
    static std::mt19937_64 gen(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dist;

    uint64_t a, b;
    {
        std::lock_guard<std::mutex> lock(rng_mutex);
        a = dist(gen);
        b = dist(gen);
    }
    // Set version 4 and variant bits
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char buf[37];
    std::snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        static_cast<uint32_t>(a >> 32),
        static_cast<uint16_t>(a >> 16),
        static_cast<uint16_t>(a),
        static_cast<uint16_t>(b >> 48),
        static_cast<unsigned long long>(b & 0x0000FFFFFFFFFFFFULL));
    return buf;
}

static std::string get_app_data_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return std::string(path) + "\\fincept-terminal";
    }
    return ".";
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    return std::string(home ? home : ".") + "/Library/Application Support/fincept-terminal";
#else
    const char* home = getenv("HOME");
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg) return std::string(xdg) + "/fincept-terminal";
    return std::string(home ? home : ".") + "/.local/share/fincept-terminal";
#endif
}

// ============================================================================
// Statement
// ============================================================================
Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::bind_text(int idx, const std::string& val) {
    sqlite3_bind_text(stmt_, idx, val.c_str(), (int)val.size(), SQLITE_TRANSIENT);
}

void Statement::bind_int(int idx, int64_t val) {
    sqlite3_bind_int64(stmt_, idx, val);
}

void Statement::bind_double(int idx, double val) {
    sqlite3_bind_double(stmt_, idx, val);
}

void Statement::bind_null(int idx) {
    sqlite3_bind_null(stmt_, idx);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(std::string("SQLite step failed: ") + sqlite3_errmsg(sqlite3_db_handle(stmt_)));
}

void Statement::execute() {
    int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(std::string("SQLite execute failed: ") + sqlite3_errmsg(sqlite3_db_handle(stmt_)));
    }
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

std::string Statement::col_text(int idx) const {
    const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, idx));
    return val ? val : "";
}

std::optional<std::string> Statement::col_text_opt(int idx) const {
    if (sqlite3_column_type(stmt_, idx) == SQLITE_NULL) return std::nullopt;
    const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, idx));
    return val ? std::optional<std::string>(val) : std::nullopt;
}

int64_t Statement::col_int(int idx) const {
    return sqlite3_column_int64(stmt_, idx);
}

double Statement::col_double(int idx) const {
    return sqlite3_column_double(stmt_, idx);
}

bool Statement::col_bool(int idx) const {
    return sqlite3_column_int(stmt_, idx) != 0;
}

int Statement::col_count() const {
    return sqlite3_column_count(stmt_);
}

// ============================================================================
// Database
// ============================================================================
Database& Database::instance() {
    static Database db;
    return db;
}

Database::~Database() {
    close();
}

std::string Database::default_db_path() {
    auto dir = get_app_data_dir();
    std::filesystem::create_directories(dir);
    return dir + "/fincept_terminal.db";
}

bool Database::initialize(const std::string& db_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) return true;

    std::string path = db_path.empty() ? default_db_path() : db_path;

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("DB", "Failed to open: %s", sqlite3_errmsg(db_));
        db_ = nullptr;
        return false;
    }

    // Performance PRAGMAs — tuned for reliability on low-end devices
    // mmap capped at 256MB (was 30GB) to avoid virtual address space exhaustion
    // on 32-bit or low-memory systems; cache_size 16MB (was 64MB) to reduce footprint
    exec("PRAGMA journal_mode = WAL;"
         "PRAGMA synchronous = NORMAL;"
         "PRAGMA cache_size = -16000;"
         "PRAGMA temp_store = MEMORY;"
         "PRAGMA mmap_size = 268435456;"
         "PRAGMA page_size = 4096;"
         "PRAGMA foreign_keys = ON;");

    create_schema();
    LOG_INFO("DB", "Initialized at: %s", path.c_str());
    return true;
}

void Database::close() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Statement Database::prepare(const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("SQLite prepare failed: ") + sqlite3_errmsg(db_) + " SQL: " + sql);
    }
    return Statement(stmt);
}

void Database::exec(const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

void Database::begin_transaction() { exec("BEGIN TRANSACTION;"); }
void Database::commit() { exec("COMMIT;"); }
void Database::rollback() { exec("ROLLBACK;"); }

void Database::create_schema() {
    exec(R"(
        -- Settings table
        CREATE TABLE IF NOT EXISTS settings (
            setting_key TEXT PRIMARY KEY,
            setting_value TEXT NOT NULL,
            category TEXT,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Key-value generic storage
        CREATE TABLE IF NOT EXISTS key_value_storage (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at INTEGER
        );

        -- Credentials
        CREATE TABLE IF NOT EXISTS credentials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            service_name TEXT NOT NULL UNIQUE,
            username TEXT,
            password TEXT,
            api_key TEXT,
            api_secret TEXT,
            additional_data TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- LLM configurations
        CREATE TABLE IF NOT EXISTS llm_configs (
            provider TEXT PRIMARY KEY,
            api_key TEXT,
            base_url TEXT,
            model TEXT NOT NULL,
            is_active INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- LLM global settings
        CREATE TABLE IF NOT EXISTS llm_global_settings (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            temperature REAL DEFAULT 0.7,
            max_tokens INTEGER DEFAULT 2000,
            system_prompt TEXT DEFAULT ''
        );
        INSERT OR IGNORE INTO llm_global_settings (id) VALUES (1);

        -- LLM model configurations (user-added custom models)
        CREATE TABLE IF NOT EXISTS llm_model_configs (
            id TEXT PRIMARY KEY,
            provider TEXT NOT NULL,
            model_id TEXT NOT NULL,
            display_name TEXT NOT NULL,
            api_key TEXT,
            base_url TEXT,
            is_enabled INTEGER DEFAULT 1,
            is_default INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_llm_model_configs_provider ON llm_model_configs(provider);
        CREATE INDEX IF NOT EXISTS idx_llm_model_configs_enabled ON llm_model_configs(is_enabled);

        -- Insert default Fincept LLM config if not exists
        INSERT OR IGNORE INTO llm_configs (provider, api_key, base_url, model, is_active)
        VALUES ('fincept', '', 'https://api.fincept.in/research/llm', 'fincept-llm', 1);

        -- Chat sessions
        CREATE TABLE IF NOT EXISTS chat_sessions (
            session_uuid TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            message_count INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Chat messages
        CREATE TABLE IF NOT EXISTS chat_messages (
            id TEXT PRIMARY KEY,
            session_uuid TEXT NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
            provider TEXT,
            model TEXT,
            tokens_used INTEGER,
            FOREIGN KEY (session_uuid) REFERENCES chat_sessions(session_uuid) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_chat_messages_session ON chat_messages(session_uuid);

        -- Watchlists
        CREATE TABLE IF NOT EXISTS watchlists (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            color TEXT DEFAULT '#FF8C00',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Watchlist stocks
        CREATE TABLE IF NOT EXISTS watchlist_stocks (
            id TEXT PRIMARY KEY,
            watchlist_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            added_at TEXT DEFAULT CURRENT_TIMESTAMP,
            notes TEXT,
            FOREIGN KEY (watchlist_id) REFERENCES watchlists(id) ON DELETE CASCADE,
            UNIQUE(watchlist_id, symbol)
        );
        CREATE INDEX IF NOT EXISTS idx_watchlist_stocks_wid ON watchlist_stocks(watchlist_id);

        -- Data sources
        CREATE TABLE IF NOT EXISTS data_sources (
            id TEXT PRIMARY KEY,
            alias TEXT NOT NULL,
            display_name TEXT NOT NULL,
            description TEXT,
            type TEXT NOT NULL,
            provider TEXT NOT NULL,
            category TEXT,
            config TEXT NOT NULL DEFAULT '{}',
            enabled INTEGER DEFAULT 1,
            tags TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Portfolio Management (investment tracking)
        CREATE TABLE IF NOT EXISTS portfolios (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            owner TEXT NOT NULL DEFAULT '',
            currency TEXT NOT NULL DEFAULT 'USD',
            description TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS portfolio_assets (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            quantity REAL NOT NULL,
            avg_buy_price REAL NOT NULL,
            first_purchase_date TEXT DEFAULT CURRENT_TIMESTAMP,
            last_updated TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE,
            UNIQUE(portfolio_id, symbol)
        );
        CREATE INDEX IF NOT EXISTS idx_portfolio_assets_pid ON portfolio_assets(portfolio_id);

        CREATE TABLE IF NOT EXISTS portfolio_transactions (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            transaction_type TEXT NOT NULL,
            quantity REAL NOT NULL,
            price REAL NOT NULL,
            total_value REAL NOT NULL,
            transaction_date TEXT DEFAULT CURRENT_TIMESTAMP,
            notes TEXT,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_portfolio_txn_pid ON portfolio_transactions(portfolio_id);

        -- Paper Trading (simulated trading engine)
        CREATE TABLE IF NOT EXISTS pt_portfolios (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            initial_balance REAL NOT NULL,
            balance REAL NOT NULL,
            currency TEXT DEFAULT 'USD',
            leverage REAL DEFAULT 1.0,
            margin_mode TEXT DEFAULT 'cross',
            fee_rate REAL DEFAULT 0.001,
            exchange TEXT DEFAULT '',
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS pt_orders (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            order_type TEXT NOT NULL,
            quantity REAL NOT NULL,
            price REAL,
            stop_price REAL,
            filled_qty REAL DEFAULT 0,
            avg_price REAL,
            status TEXT DEFAULT 'pending',
            reduce_only INTEGER DEFAULT 0,
            created_at TEXT NOT NULL,
            filled_at TEXT,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id)
        );
        CREATE INDEX IF NOT EXISTS idx_pt_orders_pid ON pt_orders(portfolio_id);

        CREATE TABLE IF NOT EXISTS pt_positions (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            quantity REAL NOT NULL,
            entry_price REAL NOT NULL,
            current_price REAL NOT NULL,
            unrealized_pnl REAL DEFAULT 0,
            realized_pnl REAL DEFAULT 0,
            leverage REAL DEFAULT 1.0,
            liquidation_price REAL,
            opened_at TEXT NOT NULL,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
            UNIQUE(portfolio_id, symbol, side)
        );
        CREATE INDEX IF NOT EXISTS idx_pt_positions_pid ON pt_positions(portfolio_id);
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS pt_trades (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            order_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            price REAL NOT NULL,
            quantity REAL NOT NULL,
            fee REAL NOT NULL,
            pnl REAL DEFAULT 0,
            timestamp TEXT NOT NULL,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
            FOREIGN KEY (order_id) REFERENCES pt_orders(id)
        );
        CREATE INDEX IF NOT EXISTS idx_pt_trades_pid ON pt_trades(portfolio_id);

        CREATE TABLE IF NOT EXISTS agent_configs (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            category TEXT DEFAULT 'custom',
            config_json TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at TEXT NOT NULL DEFAULT (datetime('now'))
        );

        -- M&A Deals
        CREATE TABLE IF NOT EXISTS ma_deals (
            deal_id TEXT PRIMARY KEY,
            target_name TEXT NOT NULL,
            acquirer_name TEXT NOT NULL,
            deal_value REAL DEFAULT 0,
            offer_price_per_share REAL,
            premium_1day REAL DEFAULT 0,
            payment_method TEXT,
            payment_cash_pct REAL DEFAULT 0,
            payment_stock_pct REAL DEFAULT 0,
            ev_revenue REAL,
            ev_ebitda REAL,
            synergies REAL,
            status TEXT DEFAULT 'Unknown',
            deal_type TEXT,
            industry TEXT DEFAULT 'General',
            announced_date TEXT,
            expected_close_date TEXT,
            breakup_fee REAL,
            hostile_flag INTEGER DEFAULT 0,
            tender_offer_flag INTEGER DEFAULT 0,
            cross_border_flag INTEGER DEFAULT 0,
            acquirer_country TEXT,
            target_country TEXT,
            created_at TEXT DEFAULT (datetime('now')),
            updated_at TEXT DEFAULT (datetime('now'))
        );
        CREATE INDEX IF NOT EXISTS idx_ma_deals_industry ON ma_deals(industry);
        CREATE INDEX IF NOT EXISTS idx_ma_deals_status ON ma_deals(status);

        -- Notes
        CREATE TABLE IF NOT EXISTS notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            content TEXT NOT NULL DEFAULT '',
            category TEXT NOT NULL DEFAULT 'GENERAL',
            priority TEXT NOT NULL DEFAULT 'MEDIUM',
            tags TEXT DEFAULT '',
            tickers TEXT DEFAULT '',
            sentiment TEXT DEFAULT 'NEUTRAL',
            is_favorite INTEGER DEFAULT 0,
            is_archived INTEGER DEFAULT 0,
            color_code TEXT DEFAULT '#FF8C00',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            reminder_date TEXT,
            word_count INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_notes_category ON notes(category);
        CREATE INDEX IF NOT EXISTS idx_notes_priority ON notes(priority);
        CREATE INDEX IF NOT EXISTS idx_notes_favorite ON notes(is_favorite);
        CREATE INDEX IF NOT EXISTS idx_notes_archived ON notes(is_archived);

        -- Algo Trading: Strategies
        CREATE TABLE IF NOT EXISTS algo_strategies (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            symbols TEXT NOT NULL DEFAULT '[]',
            entry_conditions TEXT NOT NULL DEFAULT '[]',
            exit_conditions TEXT NOT NULL DEFAULT '[]',
            entry_logic TEXT NOT NULL DEFAULT 'AND',
            exit_logic TEXT NOT NULL DEFAULT 'AND',
            timeframe TEXT NOT NULL DEFAULT '5m',
            stop_loss REAL DEFAULT 0,
            take_profit REAL DEFAULT 0,
            trailing_stop REAL DEFAULT 0,
            position_size REAL DEFAULT 1,
            max_positions INTEGER DEFAULT 1,
            provider TEXT DEFAULT 'fyers',
            is_active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )");

    exec(R"(
        -- Algo Trading: Deployments
        CREATE TABLE IF NOT EXISTS algo_deployments (
            id TEXT PRIMARY KEY,
            strategy_id TEXT NOT NULL,
            strategy_name TEXT DEFAULT '',
            mode TEXT NOT NULL DEFAULT 'paper',
            status TEXT NOT NULL DEFAULT 'starting',
            pid INTEGER DEFAULT 0,
            portfolio_id TEXT DEFAULT '',
            started_at TEXT DEFAULT CURRENT_TIMESTAMP,
            stopped_at TEXT,
            error_message TEXT DEFAULT '',
            config TEXT DEFAULT '{}',
            FOREIGN KEY (strategy_id) REFERENCES algo_strategies(id)
        );
        CREATE INDEX IF NOT EXISTS idx_algo_deployments_strategy ON algo_deployments(strategy_id);
        CREATE INDEX IF NOT EXISTS idx_algo_deployments_status ON algo_deployments(status);

        -- Algo Trading: Metrics (per deployment)
        CREATE TABLE IF NOT EXISTS algo_metrics (
            deployment_id TEXT PRIMARY KEY,
            total_trades INTEGER DEFAULT 0,
            winning_trades INTEGER DEFAULT 0,
            losing_trades INTEGER DEFAULT 0,
            total_pnl REAL DEFAULT 0,
            max_drawdown REAL DEFAULT 0,
            win_rate REAL DEFAULT 0,
            avg_win REAL DEFAULT 0,
            avg_loss REAL DEFAULT 0,
            sharpe_ratio REAL DEFAULT 0,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (deployment_id) REFERENCES algo_deployments(id) ON DELETE CASCADE
        );

        -- Algo Trading: Trades
        CREATE TABLE IF NOT EXISTS algo_trades (
            id TEXT PRIMARY KEY,
            deployment_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            quantity REAL NOT NULL,
            price REAL NOT NULL,
            pnl REAL DEFAULT 0,
            fees REAL DEFAULT 0,
            order_type TEXT DEFAULT 'market',
            status TEXT DEFAULT 'filled',
            executed_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (deployment_id) REFERENCES algo_deployments(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_algo_trades_deployment ON algo_trades(deployment_id);

        -- Algo Trading: Order Signals (bridge between Python runner and order execution)
        CREATE TABLE IF NOT EXISTS algo_order_signals (
            id TEXT PRIMARY KEY,
            deployment_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            quantity REAL NOT NULL,
            order_type TEXT DEFAULT 'market',
            price REAL DEFAULT 0,
            stop_price REAL DEFAULT 0,
            status TEXT DEFAULT 'pending',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            executed_at TEXT,
            error TEXT DEFAULT '',
            FOREIGN KEY (deployment_id) REFERENCES algo_deployments(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_algo_signals_status ON algo_order_signals(status);
    )");

    exec(R"(
        -- Algo Trading: Candle Cache
        CREATE TABLE IF NOT EXISTS candle_cache (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            timeframe TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            open REAL NOT NULL,
            high REAL NOT NULL,
            low REAL NOT NULL,
            close REAL NOT NULL,
            volume REAL DEFAULT 0,
            provider TEXT DEFAULT 'fyers',
            UNIQUE(symbol, timeframe, timestamp, provider)
        );
        CREATE INDEX IF NOT EXISTS idx_candle_cache_lookup ON candle_cache(symbol, timeframe, timestamp);

        -- Algo Trading: Custom Python Strategies
        CREATE TABLE IF NOT EXISTS custom_python_strategies (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            code TEXT NOT NULL,
            category TEXT DEFAULT 'custom',
            parameters TEXT DEFAULT '{}',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        -- Algo Trading: Strategy Price Cache (live prices for deployed strategies)
        CREATE TABLE IF NOT EXISTS strategy_price_cache (
            symbol TEXT NOT NULL,
            price REAL NOT NULL,
            volume REAL DEFAULT 0,
            change_pct REAL DEFAULT 0,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (symbol)
        );
    )");

    // --- Schema migrations: add missing columns to existing tables ---
    // SQLite has no ADD COLUMN IF NOT EXISTS, so we attempt each and ignore
    // "duplicate column name" errors.
    auto try_add_column = [this](const char* sql) {
        char* err = nullptr;
        sqlite3_exec(db_, sql, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);  // silently ignore (duplicate column)
    };

    // algo_strategies: columns added after initial schema
    try_add_column("ALTER TABLE algo_strategies ADD COLUMN position_size REAL DEFAULT 1;");
    try_add_column("ALTER TABLE algo_strategies ADD COLUMN max_positions INTEGER DEFAULT 1;");
    try_add_column("ALTER TABLE algo_strategies ADD COLUMN provider TEXT DEFAULT 'fyers';");
    try_add_column("ALTER TABLE algo_strategies ADD COLUMN is_active INTEGER DEFAULT 1;");
    try_add_column("ALTER TABLE algo_strategies ADD COLUMN trailing_stop REAL DEFAULT 0;");

    // pt_portfolios: exchange column for broker/exchange isolation
    try_add_column("ALTER TABLE pt_portfolios ADD COLUMN exchange TEXT DEFAULT '';");
}

// ============================================================================

} // namespace fincept::db
