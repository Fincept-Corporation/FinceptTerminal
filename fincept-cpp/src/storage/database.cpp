// Database Module — Full implementation matching Tauri/Rust architecture
// Thread-safe SQLite with WAL, performance PRAGMAs, domain operations

#include "database.h"
#include "screens/algo_trading/algo_types.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <random>
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
    return (int64_t)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    uint64_t a = dist(gen), b = dist(gen);
    // Set version 4 and variant bits
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char buf[37];
    std::snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        (uint32_t)(a >> 32),
        (uint16_t)(a >> 16),
        (uint16_t)(a),
        (uint16_t)(b >> 48),
        (unsigned long long)(b & 0x0000FFFFFFFFFFFFULL));
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

    // Performance PRAGMAs — matching Tauri configuration
    exec("PRAGMA journal_mode = WAL;"
         "PRAGMA synchronous = NORMAL;"
         "PRAGMA cache_size = -64000;"
         "PRAGMA temp_store = MEMORY;"
         "PRAGMA mmap_size = 30000000000;"
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
}

// ============================================================================
// CacheDatabase
// ============================================================================
CacheDatabase& CacheDatabase::instance() {
    static CacheDatabase db;
    return db;
}

CacheDatabase::~CacheDatabase() {
    close();
}

std::string CacheDatabase::default_cache_path() {
    auto dir = get_app_data_dir();
    std::filesystem::create_directories(dir);
    return dir + "/fincept_cache.db";
}

bool CacheDatabase::initialize(const std::string& db_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) return true;

    std::string path = db_path.empty() ? default_cache_path() : db_path;

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        LOG_ERROR("CacheDB", "Failed to open: %s", sqlite3_errmsg(db_));
        db_ = nullptr;
        return false;
    }

    // Cache DB: less strict durability for better write perf
    char* err = nullptr;
    sqlite3_exec(db_,
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = OFF;"
        "PRAGMA cache_size = -32000;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA mmap_size = 10000000000;"
        "PRAGMA page_size = 4096;",
        nullptr, nullptr, &err);
    if (err) sqlite3_free(err);

    create_schema();
    LOG_INFO("CacheDB", "Initialized at: %s", path.c_str());
    return true;
}

void CacheDatabase::close() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Statement CacheDatabase::prepare(const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("CacheDB prepare failed: ") + sqlite3_errmsg(db_));
    }
    return Statement(stmt);
}

void CacheDatabase::exec(const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("CacheDB exec failed: " + msg);
    }
}

void CacheDatabase::create_schema() {
    exec(R"(
        -- Unified cache table with TTL
        CREATE TABLE IF NOT EXISTS unified_cache (
            cache_key TEXT PRIMARY KEY,
            category TEXT NOT NULL,
            data TEXT NOT NULL,
            ttl_seconds INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            last_accessed_at INTEGER NOT NULL,
            hit_count INTEGER DEFAULT 0,
            size_bytes INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_cache_category ON unified_cache(category);
        CREATE INDEX IF NOT EXISTS idx_cache_expires ON unified_cache(expires_at);

        -- Tab session state persistence
        CREATE TABLE IF NOT EXISTS tab_sessions (
            tab_id TEXT PRIMARY KEY,
            tab_name TEXT NOT NULL,
            state TEXT NOT NULL,
            scroll_position TEXT,
            active_filters TEXT,
            selected_items TEXT,
            updated_at INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_tab_updated ON tab_sessions(updated_at);
    )");
}

// ============================================================================
// Operations — Settings
// ============================================================================
namespace ops {

void save_setting(const std::string& key, const std::string& value, const std::string& category) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO settings (setting_key, setting_value, category, updated_at) "
        "VALUES (?1, ?2, ?3, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, key);
    stmt.bind_text(2, value);
    stmt.bind_text(3, category);
    stmt.execute();
}

std::optional<std::string> get_setting(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT setting_value FROM settings WHERE setting_key = ?1");
    stmt.bind_text(1, key);
    if (stmt.step()) return stmt.col_text(0);
    return std::nullopt;
}

std::vector<Setting> get_all_settings() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT setting_key, setting_value, category, updated_at FROM settings");
    std::vector<Setting> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3)});
    }
    return result;
}

void delete_setting(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM settings WHERE setting_key = ?1");
    stmt.bind_text(1, key);
    stmt.execute();
}

// ============================================================================
// Operations — Key-Value Storage
// ============================================================================
void storage_set(const std::string& key, const std::string& value) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO key_value_storage (key, value, updated_at) VALUES (?1, ?2, ?3)");
    stmt.bind_text(1, key);
    stmt.bind_text(2, value);
    stmt.bind_int(3, now_timestamp());
    stmt.execute();
}

std::optional<std::string> storage_get(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT value FROM key_value_storage WHERE key = ?1");
    stmt.bind_text(1, key);
    if (stmt.step()) return stmt.col_text(0);
    return std::nullopt;
}

void storage_remove(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM key_value_storage WHERE key = ?1");
    stmt.bind_text(1, key);
    stmt.execute();
}

bool storage_has(const std::string& key) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT 1 FROM key_value_storage WHERE key = ?1 LIMIT 1");
    stmt.bind_text(1, key);
    return stmt.step();
}

std::vector<std::string> storage_keys() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT key FROM key_value_storage ORDER BY key");
    std::vector<std::string> keys;
    while (stmt.step()) keys.push_back(stmt.col_text(0));
    return keys;
}

void storage_set_many(const std::vector<std::pair<std::string, std::string>>& pairs) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.transaction([&]() {
        int64_t ts = now_timestamp();
        for (auto& [k, v] : pairs) {
            auto stmt = db.prepare(
                "INSERT OR REPLACE INTO key_value_storage (key, value, updated_at) VALUES (?1, ?2, ?3)");
            stmt.bind_text(1, k);
            stmt.bind_text(2, v);
            stmt.bind_int(3, ts);
            stmt.execute();
        }
        return true;
    });
}

// ============================================================================
// Operations — Credentials
// ============================================================================
void save_credential(const Credential& cred) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO credentials "
        "(service_name, username, password, api_key, api_secret, additional_data, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, cred.service_name);
    cred.username    ? stmt.bind_text(2, *cred.username)    : stmt.bind_null(2);
    cred.password    ? stmt.bind_text(3, *cred.password)    : stmt.bind_null(3);
    cred.api_key     ? stmt.bind_text(4, *cred.api_key)     : stmt.bind_null(4);
    cred.api_secret  ? stmt.bind_text(5, *cred.api_secret)  : stmt.bind_null(5);
    cred.additional_data ? stmt.bind_text(6, *cred.additional_data) : stmt.bind_null(6);
    stmt.execute();
}

std::vector<Credential> get_credentials() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, service_name, username, password, api_key, api_secret, "
        "additional_data, created_at, updated_at FROM credentials ORDER BY service_name");
    std::vector<Credential> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_int(0), stmt.col_text(1), stmt.col_text_opt(2),
            stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
            stmt.col_text_opt(6), stmt.col_text(7), stmt.col_text(8)
        });
    }
    return result;
}

std::optional<Credential> get_credential_by_service(const std::string& service_name) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, service_name, username, password, api_key, api_secret, "
        "additional_data, created_at, updated_at FROM credentials WHERE service_name = ?1");
    stmt.bind_text(1, service_name);
    if (!stmt.step()) return std::nullopt;
    return Credential{
        stmt.col_int(0), stmt.col_text(1), stmt.col_text_opt(2),
        stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
        stmt.col_text_opt(6), stmt.col_text(7), stmt.col_text(8)
    };
}

void delete_credential(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM credentials WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

// ============================================================================
// Operations — LLM Config
// ============================================================================
std::vector<LLMConfig> get_llm_configs() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT provider, api_key, base_url, model, is_active, created_at, updated_at FROM llm_configs");
    std::vector<LLMConfig> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text_opt(1), stmt.col_text_opt(2),
            stmt.col_text(3), stmt.col_bool(4), stmt.col_text(5), stmt.col_text(6)
        });
    }
    return result;
}

void save_llm_config(const LLMConfig& config) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, config.provider);
    config.api_key  ? stmt.bind_text(2, *config.api_key)  : stmt.bind_null(2);
    config.base_url ? stmt.bind_text(3, *config.base_url) : stmt.bind_null(3);
    stmt.bind_text(4, config.model);
    stmt.bind_int(5, config.is_active ? 1 : 0);
    stmt.execute();
}

LLMGlobalSettings get_llm_global_settings() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1");
    if (stmt.step()) {
        return {stmt.col_double(0), stmt.col_int(1), stmt.col_text(2)};
    }
    return {};
}

void save_llm_global_settings(const LLMGlobalSettings& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE llm_global_settings SET temperature = ?1, max_tokens = ?2, system_prompt = ?3 WHERE id = 1");
    stmt.bind_double(1, s.temperature);
    stmt.bind_int(2, s.max_tokens);
    stmt.bind_text(3, s.system_prompt);
    stmt.execute();
}

void set_active_llm_provider(const std::string& provider) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.exec("UPDATE llm_configs SET is_active = 0");
    auto stmt = db.prepare("UPDATE llm_configs SET is_active = 1 WHERE provider = ?1");
    stmt.bind_text(1, provider);
    stmt.execute();
}

// ============================================================================
// Operations — Chat
// ============================================================================
ChatSession create_chat_session(const std::string& title) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string uuid = generate_uuid();
    auto stmt = db.prepare("INSERT INTO chat_sessions (session_uuid, title) VALUES (?1, ?2)");
    stmt.bind_text(1, uuid);
    stmt.bind_text(2, title);
    stmt.execute();

    auto q = db.prepare(
        "SELECT session_uuid, title, message_count, created_at, updated_at "
        "FROM chat_sessions WHERE session_uuid = ?1");
    q.bind_text(1, uuid);
    q.step();
    return {q.col_text(0), q.col_text(1), q.col_int(2), q.col_text(3), q.col_text(4)};
}

std::vector<ChatSession> get_chat_sessions(int limit) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string sql = "SELECT session_uuid, title, message_count, created_at, updated_at "
                      "FROM chat_sessions ORDER BY updated_at DESC";
    if (limit > 0) sql += " LIMIT " + std::to_string(limit);
    auto stmt = db.prepare(sql.c_str());
    std::vector<ChatSession> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_int(2), stmt.col_text(3), stmt.col_text(4)});
    }
    return result;
}

ChatMessage add_chat_message(const ChatMessage& msg) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT INTO chat_messages (id, session_uuid, role, content, provider, model, tokens_used) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)");
    stmt.bind_text(1, msg.id);
    stmt.bind_text(2, msg.session_uuid);
    stmt.bind_text(3, msg.role);
    stmt.bind_text(4, msg.content);
    msg.provider    ? stmt.bind_text(5, *msg.provider)      : stmt.bind_null(5);
    msg.model       ? stmt.bind_text(6, *msg.model)         : stmt.bind_null(6);
    msg.tokens_used ? stmt.bind_int(7, *msg.tokens_used)    : stmt.bind_null(7);
    stmt.execute();

    auto upd = db.prepare(
        "UPDATE chat_sessions SET message_count = message_count + 1, updated_at = CURRENT_TIMESTAMP "
        "WHERE session_uuid = ?1");
    upd.bind_text(1, msg.session_uuid);
    upd.execute();

    auto q = db.prepare(
        "SELECT id, session_uuid, role, content, timestamp, provider, model, tokens_used "
        "FROM chat_messages WHERE id = ?1");
    q.bind_text(1, msg.id);
    q.step();
    std::optional<int64_t> tokens;
    if (sqlite3_column_type(q.raw(), 7) != SQLITE_NULL) tokens = q.col_int(7);
    return {q.col_text(0), q.col_text(1), q.col_text(2), q.col_text(3),
            q.col_text(4), q.col_text_opt(5), q.col_text_opt(6), tokens};
}

std::vector<ChatMessage> get_chat_messages(const std::string& session_uuid) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, session_uuid, role, content, timestamp, provider, model, tokens_used "
        "FROM chat_messages WHERE session_uuid = ?1 ORDER BY timestamp ASC");
    stmt.bind_text(1, session_uuid);
    std::vector<ChatMessage> result;
    while (stmt.step()) {
        std::optional<int64_t> tokens;
        if (sqlite3_column_type(stmt.raw(), 7) != SQLITE_NULL) tokens = stmt.col_int(7);
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
            stmt.col_text(4), stmt.col_text_opt(5), stmt.col_text_opt(6), tokens
        });
    }
    return result;
}

void delete_chat_session(const std::string& session_uuid) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM chat_sessions WHERE session_uuid = ?1");
    stmt.bind_text(1, session_uuid);
    stmt.execute();
}

// ============================================================================
// Operations — Watchlist
// ============================================================================
Watchlist create_watchlist(const std::string& name, const std::string& color, const std::string& description) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string id = generate_uuid();
    auto stmt = db.prepare("INSERT INTO watchlists (id, name, description, color) VALUES (?1, ?2, ?3, ?4)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, name);
    stmt.bind_text(3, description);
    stmt.bind_text(4, color);
    stmt.execute();

    auto q = db.prepare("SELECT id, name, description, color, created_at, updated_at FROM watchlists WHERE id = ?1");
    q.bind_text(1, id);
    q.step();
    return {q.col_text(0), q.col_text(1), q.col_text_opt(2), q.col_text(3), q.col_text(4), q.col_text(5)};
}

std::vector<Watchlist> get_watchlists() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT id, name, description, color, created_at, updated_at FROM watchlists ORDER BY created_at DESC");
    std::vector<Watchlist> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text_opt(2),
                          stmt.col_text(3), stmt.col_text(4), stmt.col_text(5)});
    }
    return result;
}

void delete_watchlist(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM watchlists WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

WatchlistStock add_watchlist_stock(const std::string& watchlist_id, const std::string& symbol, const std::string& notes) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string id = generate_uuid();
    std::string upper_sym = symbol;
    std::transform(upper_sym.begin(), upper_sym.end(), upper_sym.begin(), ::toupper);
    auto stmt = db.prepare("INSERT INTO watchlist_stocks (id, watchlist_id, symbol, notes) VALUES (?1, ?2, ?3, ?4)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, watchlist_id);
    stmt.bind_text(3, upper_sym);
    stmt.bind_text(4, notes);
    stmt.execute();

    auto q = db.prepare("SELECT id, watchlist_id, symbol, added_at, notes FROM watchlist_stocks WHERE id = ?1");
    q.bind_text(1, id);
    q.step();
    return {q.col_text(0), q.col_text(1), q.col_text(2), q.col_text(3), q.col_text_opt(4)};
}

std::vector<WatchlistStock> get_watchlist_stocks(const std::string& watchlist_id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, watchlist_id, symbol, added_at, notes FROM watchlist_stocks "
        "WHERE watchlist_id = ?1 ORDER BY added_at DESC");
    stmt.bind_text(1, watchlist_id);
    std::vector<WatchlistStock> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3), stmt.col_text_opt(4)});
    }
    return result;
}

void remove_watchlist_stock(const std::string& watchlist_id, const std::string& symbol) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string upper_sym = symbol;
    std::transform(upper_sym.begin(), upper_sym.end(), upper_sym.begin(), ::toupper);
    auto stmt = db.prepare("DELETE FROM watchlist_stocks WHERE watchlist_id = ?1 AND symbol = ?2");
    stmt.bind_text(1, watchlist_id);
    stmt.bind_text(2, upper_sym);
    stmt.execute();
}

// ============================================================================
// Operations — M&A Deals
// ============================================================================
MADealRow create_ma_deal(const MADealRow& deal) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string id = deal.deal_id.empty() ? generate_uuid() : deal.deal_id;
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO ma_deals "
        "(deal_id, target_name, acquirer_name, deal_value, offer_price_per_share, "
        "premium_1day, payment_method, payment_cash_pct, payment_stock_pct, "
        "ev_revenue, ev_ebitda, synergies, status, deal_type, industry, "
        "announced_date, expected_close_date, breakup_fee, hostile_flag, "
        "tender_offer_flag, cross_border_flag, acquirer_country, target_country, "
        "created_at, updated_at) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,"
        "?16,?17,?18,?19,?20,?21,?22,?23,"
        "COALESCE((SELECT created_at FROM ma_deals WHERE deal_id=?1), datetime('now')),"
        "datetime('now'))");
    stmt.bind_text(1, id);
    stmt.bind_text(2, deal.target_name);
    stmt.bind_text(3, deal.acquirer_name);
    stmt.bind_double(4, deal.deal_value);
    stmt.bind_double(5, deal.offer_price_per_share);
    stmt.bind_double(6, deal.premium_1day);
    stmt.bind_text(7, deal.payment_method);
    stmt.bind_double(8, deal.payment_cash_pct);
    stmt.bind_double(9, deal.payment_stock_pct);
    stmt.bind_double(10, deal.ev_revenue);
    stmt.bind_double(11, deal.ev_ebitda);
    stmt.bind_double(12, deal.synergies);
    stmt.bind_text(13, deal.status);
    stmt.bind_text(14, deal.deal_type);
    stmt.bind_text(15, deal.industry);
    stmt.bind_text(16, deal.announced_date);
    stmt.bind_text(17, deal.expected_close_date);
    stmt.bind_double(18, deal.breakup_fee);
    stmt.bind_int(19, deal.hostile_flag);
    stmt.bind_int(20, deal.tender_offer_flag);
    stmt.bind_int(21, deal.cross_border_flag);
    stmt.bind_text(22, deal.acquirer_country);
    stmt.bind_text(23, deal.target_country);
    stmt.execute();

    MADealRow result = deal;
    result.deal_id = id;
    return result;
}

std::vector<MADealRow> get_all_ma_deals() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("SELECT * FROM ma_deals ORDER BY announced_date DESC");
    std::vector<MADealRow> deals;
    while (stmt.step()) {
        MADealRow d;
        d.deal_id              = stmt.col_text(0);
        d.target_name          = stmt.col_text(1);
        d.acquirer_name        = stmt.col_text(2);
        d.deal_value           = stmt.col_double(3);
        d.offer_price_per_share= stmt.col_double(4);
        d.premium_1day         = stmt.col_double(5);
        d.payment_method       = stmt.col_text(6);
        d.payment_cash_pct     = stmt.col_double(7);
        d.payment_stock_pct    = stmt.col_double(8);
        d.ev_revenue           = stmt.col_double(9);
        d.ev_ebitda            = stmt.col_double(10);
        d.synergies            = stmt.col_double(11);
        d.status               = stmt.col_text(12);
        d.deal_type            = stmt.col_text(13);
        d.industry             = stmt.col_text(14);
        d.announced_date       = stmt.col_text(15);
        d.expected_close_date  = stmt.col_text(16);
        d.breakup_fee          = stmt.col_double(17);
        d.hostile_flag         = static_cast<int>(stmt.col_int(18));
        d.tender_offer_flag    = static_cast<int>(stmt.col_int(19));
        d.cross_border_flag    = static_cast<int>(stmt.col_int(20));
        d.acquirer_country     = stmt.col_text(21);
        d.target_country       = stmt.col_text(22);
        d.created_at           = stmt.col_text(23);
        d.updated_at           = stmt.col_text(24);
        deals.push_back(std::move(d));
    }
    return deals;
}

std::vector<MADealRow> search_ma_deals(const std::string& query) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string pattern = "%" + query + "%";
    auto stmt = db.prepare(
        "SELECT * FROM ma_deals WHERE "
        "target_name LIKE ?1 OR acquirer_name LIKE ?1 OR industry LIKE ?1 OR deal_type LIKE ?1 "
        "ORDER BY announced_date DESC");
    stmt.bind_text(1, pattern);
    std::vector<MADealRow> deals;
    while (stmt.step()) {
        MADealRow d;
        d.deal_id              = stmt.col_text(0);
        d.target_name          = stmt.col_text(1);
        d.acquirer_name        = stmt.col_text(2);
        d.deal_value           = stmt.col_double(3);
        d.offer_price_per_share= stmt.col_double(4);
        d.premium_1day         = stmt.col_double(5);
        d.payment_method       = stmt.col_text(6);
        d.payment_cash_pct     = stmt.col_double(7);
        d.payment_stock_pct    = stmt.col_double(8);
        d.ev_revenue           = stmt.col_double(9);
        d.ev_ebitda            = stmt.col_double(10);
        d.synergies            = stmt.col_double(11);
        d.status               = stmt.col_text(12);
        d.deal_type            = stmt.col_text(13);
        d.industry             = stmt.col_text(14);
        d.announced_date       = stmt.col_text(15);
        d.expected_close_date  = stmt.col_text(16);
        d.breakup_fee          = stmt.col_double(17);
        d.hostile_flag         = static_cast<int>(stmt.col_int(18));
        d.tender_offer_flag    = static_cast<int>(stmt.col_int(19));
        d.cross_border_flag    = static_cast<int>(stmt.col_int(20));
        d.acquirer_country     = stmt.col_text(21);
        d.target_country       = stmt.col_text(22);
        d.created_at           = stmt.col_text(23);
        d.updated_at           = stmt.col_text(24);
        deals.push_back(std::move(d));
    }
    return deals;
}

void update_ma_deal(const MADealRow& deal) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE ma_deals SET target_name=?2, acquirer_name=?3, deal_value=?4, "
        "offer_price_per_share=?5, premium_1day=?6, payment_method=?7, "
        "payment_cash_pct=?8, payment_stock_pct=?9, ev_revenue=?10, ev_ebitda=?11, "
        "synergies=?12, status=?13, deal_type=?14, industry=?15, announced_date=?16, "
        "expected_close_date=?17, breakup_fee=?18, hostile_flag=?19, tender_offer_flag=?20, "
        "cross_border_flag=?21, acquirer_country=?22, target_country=?23, "
        "updated_at=datetime('now') WHERE deal_id=?1");
    stmt.bind_text(1, deal.deal_id);
    stmt.bind_text(2, deal.target_name);
    stmt.bind_text(3, deal.acquirer_name);
    stmt.bind_double(4, deal.deal_value);
    stmt.bind_double(5, deal.offer_price_per_share);
    stmt.bind_double(6, deal.premium_1day);
    stmt.bind_text(7, deal.payment_method);
    stmt.bind_double(8, deal.payment_cash_pct);
    stmt.bind_double(9, deal.payment_stock_pct);
    stmt.bind_double(10, deal.ev_revenue);
    stmt.bind_double(11, deal.ev_ebitda);
    stmt.bind_double(12, deal.synergies);
    stmt.bind_text(13, deal.status);
    stmt.bind_text(14, deal.deal_type);
    stmt.bind_text(15, deal.industry);
    stmt.bind_text(16, deal.announced_date);
    stmt.bind_text(17, deal.expected_close_date);
    stmt.bind_double(18, deal.breakup_fee);
    stmt.bind_int(19, deal.hostile_flag);
    stmt.bind_int(20, deal.tender_offer_flag);
    stmt.bind_int(21, deal.cross_border_flag);
    stmt.bind_text(22, deal.acquirer_country);
    stmt.bind_text(23, deal.target_country);
    stmt.execute();
}

void delete_ma_deal(const std::string& deal_id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM ma_deals WHERE deal_id = ?1");
    stmt.bind_text(1, deal_id);
    stmt.execute();
}

// ============================================================================
// Operations — Cache (separate CacheDatabase)
// ============================================================================

// Helper to parse a cache row from a SELECT statement
static CacheEntry parse_cache_row(Statement& stmt, int64_t now) {
    int64_t expires_at = stmt.col_int(5);
    return CacheEntry{
        stmt.col_text(0),   // cache_key
        stmt.col_text(1),   // category
        stmt.col_text(2),   // data
        stmt.col_int(3),    // ttl_seconds
        stmt.col_int(4),    // created_at
        expires_at,          // expires_at
        stmt.col_int(6),    // last_accessed_at
        stmt.col_int(7),    // hit_count
        stmt.col_int(8),    // size_bytes
        now > expires_at     // is_expired
    };
}

std::optional<CacheEntry> cache_get(const std::string& key) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();

    auto stmt = db.prepare(
        "SELECT cache_key, category, data, ttl_seconds, created_at, expires_at, "
        "last_accessed_at, hit_count, size_bytes FROM unified_cache WHERE cache_key = ?1");
    stmt.bind_text(1, key);
    if (!stmt.step()) return std::nullopt;

    auto entry = parse_cache_row(stmt, now);

    if (!entry.is_expired) {
        auto upd = db.prepare(
            "UPDATE unified_cache SET last_accessed_at = ?1, hit_count = hit_count + 1 WHERE cache_key = ?2");
        upd.bind_int(1, now);
        upd.bind_text(2, key);
        upd.execute();
        return entry;
    }
    return std::nullopt;
}

std::optional<CacheEntry> cache_get_with_stale(const std::string& key) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();

    auto stmt = db.prepare(
        "SELECT cache_key, category, data, ttl_seconds, created_at, expires_at, "
        "last_accessed_at, hit_count, size_bytes FROM unified_cache WHERE cache_key = ?1");
    stmt.bind_text(1, key);
    if (!stmt.step()) return std::nullopt;

    return parse_cache_row(stmt, now);
}

std::vector<CacheEntry> cache_get_many(const std::vector<std::string>& keys) {
    if (keys.empty()) return {};
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();

    // Build query with placeholders
    std::string placeholders;
    for (size_t i = 0; i < keys.size(); i++) {
        if (i > 0) placeholders += ",";
        placeholders += "?" + std::to_string(i + 1);
    }
    std::string sql =
        "SELECT cache_key, category, data, ttl_seconds, created_at, expires_at, "
        "last_accessed_at, hit_count, size_bytes FROM unified_cache "
        "WHERE cache_key IN (" + placeholders + ") AND expires_at > ?" +
        std::to_string(keys.size() + 1);

    auto stmt = db.prepare(sql.c_str());
    for (size_t i = 0; i < keys.size(); i++) {
        stmt.bind_text(static_cast<int>(i + 1), keys[i]);
    }
    stmt.bind_int(static_cast<int>(keys.size() + 1), now);

    std::vector<CacheEntry> results;
    while (stmt.step()) {
        results.push_back(parse_cache_row(stmt, now));
    }
    return results;
}

void cache_set(const std::string& key, const std::string& data,
               const std::string& category, int64_t ttl_seconds) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();
    int64_t expires = now + ttl_seconds;
    int64_t size = (int64_t)data.size();

    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO unified_cache "
        "(cache_key, category, data, ttl_seconds, created_at, expires_at, last_accessed_at, hit_count, size_bytes) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?5, 0, ?7)");
    stmt.bind_text(1, key);
    stmt.bind_text(2, category);
    stmt.bind_text(3, data);
    stmt.bind_int(4, ttl_seconds);
    stmt.bind_int(5, now);
    stmt.bind_int(6, expires);
    stmt.bind_int(7, size);
    stmt.execute();
}

bool cache_delete(const std::string& key) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto chk = db.prepare("SELECT 1 FROM unified_cache WHERE cache_key = ?1");
    chk.bind_text(1, key);
    bool existed = chk.step();
    if (existed) {
        auto stmt = db.prepare("DELETE FROM unified_cache WHERE cache_key = ?1");
        stmt.bind_text(1, key);
        stmt.execute();
    }
    return existed;
}

int64_t cache_invalidate_category(const std::string& category) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t count = 0;
    {
        auto cnt = db.prepare("SELECT COUNT(*) FROM unified_cache WHERE category = ?1");
        cnt.bind_text(1, category);
        if (cnt.step()) count = cnt.col_int(0);
    }
    auto stmt = db.prepare("DELETE FROM unified_cache WHERE category = ?1");
    stmt.bind_text(1, category);
    stmt.execute();
    return count;
}

int64_t cache_invalidate_pattern(const std::string& pattern) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t count = 0;
    {
        auto cnt = db.prepare("SELECT COUNT(*) FROM unified_cache WHERE cache_key LIKE ?1");
        cnt.bind_text(1, pattern);
        if (cnt.step()) count = cnt.col_int(0);
    }
    auto stmt = db.prepare("DELETE FROM unified_cache WHERE cache_key LIKE ?1");
    stmt.bind_text(1, pattern);
    stmt.execute();
    return count;
}

int64_t cache_evict_lru(int64_t count) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    // Delete the N least recently accessed entries
    int64_t before = 0;
    {
        auto s = db.prepare("SELECT COUNT(*) FROM unified_cache");
        if (s.step()) before = s.col_int(0);
    }
    std::string sql =
        "DELETE FROM unified_cache WHERE cache_key IN ("
        "SELECT cache_key FROM unified_cache ORDER BY last_accessed_at ASC LIMIT ?1)";
    auto stmt = db.prepare(sql.c_str());
    stmt.bind_int(1, count);
    stmt.execute();
    int64_t after = 0;
    {
        auto s = db.prepare("SELECT COUNT(*) FROM unified_cache");
        if (s.step()) after = s.col_int(0);
    }
    return before - after;
}

int64_t cache_cleanup() {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();
    int64_t count = 0;
    {
        auto cnt = db.prepare("SELECT COUNT(*) FROM unified_cache WHERE expires_at <= ?1");
        cnt.bind_int(1, now);
        if (cnt.step()) count = cnt.col_int(0);
    }
    auto stmt = db.prepare("DELETE FROM unified_cache WHERE expires_at <= ?1");
    stmt.bind_int(1, now);
    stmt.execute();
    return count;
}

CacheStats cache_stats() {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    CacheStats stats{};
    {
        auto s = db.prepare("SELECT COUNT(*) FROM unified_cache");
        if (s.step()) stats.total_entries = s.col_int(0);
    }
    {
        auto s = db.prepare("SELECT COALESCE(SUM(size_bytes), 0) FROM unified_cache");
        if (s.step()) stats.total_size_bytes = s.col_int(0);
    }
    {
        auto s = db.prepare("SELECT COUNT(*) FROM unified_cache WHERE expires_at <= ?1");
        s.bind_int(1, now_timestamp());
        if (s.step()) stats.expired_entries = s.col_int(0);
    }
    // Per-category breakdown
    {
        auto s = db.prepare(
            "SELECT category, COUNT(*), COALESCE(SUM(size_bytes), 0) "
            "FROM unified_cache GROUP BY category");
        while (s.step()) {
            stats.categories.push_back({s.col_text(0), s.col_int(1), s.col_int(2)});
        }
    }
    return stats;
}

void cache_clear_all() {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.exec("DELETE FROM unified_cache");
}

// ============================================================================
// Operations — Tab Sessions
// ============================================================================
std::optional<TabSession> tab_session_get(const std::string& tab_id) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT tab_id, tab_name, state, scroll_position, active_filters, "
        "selected_items, updated_at, created_at FROM tab_sessions WHERE tab_id = ?1");
    stmt.bind_text(1, tab_id);
    if (!stmt.step()) return std::nullopt;
    return TabSession{
        stmt.col_text(0), stmt.col_text(1), stmt.col_text(2),
        stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
        stmt.col_int(6), stmt.col_int(7)
    };
}

void tab_session_set(const std::string& tab_id, const std::string& tab_name,
                     const std::string& state, const std::string& scroll_position,
                     const std::string& active_filters, const std::string& selected_items) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t now = now_timestamp();
    auto stmt = db.prepare(
        "INSERT INTO tab_sessions (tab_id, tab_name, state, scroll_position, active_filters, selected_items, updated_at, created_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?7) "
        "ON CONFLICT(tab_id) DO UPDATE SET "
        "tab_name = ?2, state = ?3, scroll_position = ?4, active_filters = ?5, selected_items = ?6, updated_at = ?7");
    stmt.bind_text(1, tab_id);
    stmt.bind_text(2, tab_name);
    stmt.bind_text(3, state);
    stmt.bind_text(4, scroll_position);
    stmt.bind_text(5, active_filters);
    stmt.bind_text(6, selected_items);
    stmt.bind_int(7, now);
    stmt.execute();
}

void tab_session_delete(const std::string& tab_id) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM tab_sessions WHERE tab_id = ?1");
    stmt.bind_text(1, tab_id);
    stmt.execute();
}

std::vector<TabSession> tab_session_get_all() {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT tab_id, tab_name, state, scroll_position, active_filters, "
        "selected_items, updated_at, created_at FROM tab_sessions ORDER BY updated_at DESC");
    std::vector<TabSession> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2),
            stmt.col_text_opt(3), stmt.col_text_opt(4), stmt.col_text_opt(5),
            stmt.col_int(6), stmt.col_int(7)
        });
    }
    return result;
}

int64_t tab_session_cleanup(int64_t max_age_days) {
    auto& db = CacheDatabase::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t cutoff = now_timestamp() - (max_age_days * 24 * 60 * 60);
    int64_t count = 0;
    {
        auto cnt = db.prepare("SELECT COUNT(*) FROM tab_sessions WHERE updated_at < ?1");
        cnt.bind_int(1, cutoff);
        if (cnt.step()) count = cnt.col_int(0);
    }
    auto stmt = db.prepare("DELETE FROM tab_sessions WHERE updated_at < ?1");
    stmt.bind_int(1, cutoff);
    stmt.execute();
    return count;
}

// ============================================================================
// Agent Configs
// ============================================================================
void ops::save_agent_config(const AgentConfigRow& config) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO agent_configs (id, name, description, category, config_json, "
        "created_at, updated_at) VALUES (?1, ?2, ?3, ?4, ?5, "
        "COALESCE((SELECT created_at FROM agent_configs WHERE id = ?1), datetime('now')), "
        "datetime('now'))");
    stmt.bind_text(1, config.id);
    stmt.bind_text(2, config.name);
    stmt.bind_text(3, config.description);
    stmt.bind_text(4, config.category);
    stmt.bind_text(5, config.config_json);
    stmt.execute();
}

std::vector<AgentConfigRow> ops::get_agent_configs() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, category, config_json, created_at, updated_at "
        "FROM agent_configs ORDER BY name");
    std::vector<AgentConfigRow> result;
    while (stmt.step()) {
        AgentConfigRow row;
        row.id = stmt.col_text(0);
        row.name = stmt.col_text(1);
        row.description = stmt.col_text(2);
        row.category = stmt.col_text(3);
        row.config_json = stmt.col_text(4);
        row.created_at = stmt.col_text(5);
        row.updated_at = stmt.col_text(6);
        result.push_back(std::move(row));
    }
    return result;
}

std::optional<AgentConfigRow> ops::get_agent_config(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, category, config_json, created_at, updated_at "
        "FROM agent_configs WHERE id = ?1");
    stmt.bind_text(1, id);
    if (stmt.step()) {
        AgentConfigRow row;
        row.id = stmt.col_text(0);
        row.name = stmt.col_text(1);
        row.description = stmt.col_text(2);
        row.category = stmt.col_text(3);
        row.config_json = stmt.col_text(4);
        row.created_at = stmt.col_text(5);
        row.updated_at = stmt.col_text(6);
        return row;
    }
    return std::nullopt;
}

void ops::delete_agent_config(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM agent_configs WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- LLM Model Configs ---

std::vector<LLMModelConfig> ops::get_llm_model_configs() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, provider, model_id, display_name, api_key, base_url, "
        "is_enabled, is_default, created_at, updated_at FROM llm_model_configs");
    std::vector<LLMModelConfig> result;
    while (stmt.step()) {
        LLMModelConfig cfg;
        cfg.id = stmt.col_text(0);
        cfg.provider = stmt.col_text(1);
        cfg.model_id = stmt.col_text(2);
        cfg.display_name = stmt.col_text(3);
        cfg.api_key = stmt.col_text_opt(4);
        cfg.base_url = stmt.col_text_opt(5);
        cfg.is_enabled = stmt.col_bool(6);
        cfg.is_default = stmt.col_bool(7);
        cfg.created_at = stmt.col_text(8);
        cfg.updated_at = stmt.col_text(9);
        result.push_back(std::move(cfg));
    }
    return result;
}

void ops::save_llm_model_config(const LLMModelConfig& config) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO llm_model_configs "
        "(id, provider, model_id, display_name, api_key, base_url, is_enabled, is_default, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, config.id);
    stmt.bind_text(2, config.provider);
    stmt.bind_text(3, config.model_id);
    stmt.bind_text(4, config.display_name);
    if (config.api_key) stmt.bind_text(5, *config.api_key); else stmt.bind_null(5);
    if (config.base_url) stmt.bind_text(6, *config.base_url); else stmt.bind_null(6);
    stmt.bind_int(7, config.is_enabled ? 1 : 0);
    stmt.bind_int(8, config.is_default ? 1 : 0);
    stmt.execute();
}

void ops::delete_llm_model_config(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM llm_model_configs WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

void ops::toggle_llm_model_config_enabled(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE llm_model_configs SET is_enabled = NOT is_enabled, "
        "updated_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- Data Sources ---

void ops::save_data_source(const DataSource& source) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT OR REPLACE INTO data_sources "
        "(id, alias, display_name, description, type, provider, category, config, enabled, tags, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, source.id);
    stmt.bind_text(2, source.alias);
    stmt.bind_text(3, source.display_name);
    if (source.description) stmt.bind_text(4, *source.description); else stmt.bind_null(4);
    stmt.bind_text(5, source.type);
    stmt.bind_text(6, source.provider);
    if (source.category) stmt.bind_text(7, *source.category); else stmt.bind_null(7);
    stmt.bind_text(8, source.config);
    stmt.bind_int(9, source.enabled ? 1 : 0);
    if (source.tags) stmt.bind_text(10, *source.tags); else stmt.bind_null(10);
    stmt.execute();
}

std::vector<DataSource> ops::get_all_data_sources() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, alias, display_name, description, type, provider, "
        "category, config, enabled, tags, created_at, updated_at FROM data_sources");
    std::vector<DataSource> result;
    while (stmt.step()) {
        DataSource ds;
        ds.id = stmt.col_text(0);
        ds.alias = stmt.col_text(1);
        ds.display_name = stmt.col_text(2);
        ds.description = stmt.col_text_opt(3);
        ds.type = stmt.col_text(4);
        ds.provider = stmt.col_text(5);
        ds.category = stmt.col_text_opt(6);
        ds.config = stmt.col_text(7);
        ds.enabled = stmt.col_bool(8);
        ds.tags = stmt.col_text_opt(9);
        ds.created_at = stmt.col_text(10);
        ds.updated_at = stmt.col_text(11);
        result.push_back(std::move(ds));
    }
    return result;
}

void ops::delete_data_source(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM data_sources WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// ============================================================================
// Operations — Notes
// ============================================================================

static int64_t count_words(const std::string& text) {
    int64_t count = 0;
    bool in_word = false;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++count;
        }
    }
    return count;
}

static Note read_note_row(Statement& stmt) {
    Note n;
    n.id           = stmt.col_int(0);
    n.title        = stmt.col_text(1);
    n.content      = stmt.col_text(2);
    n.category     = stmt.col_text(3);
    n.priority     = stmt.col_text(4);
    n.tags         = stmt.col_text(5);
    n.tickers      = stmt.col_text(6);
    n.sentiment    = stmt.col_text(7);
    n.is_favorite  = stmt.col_bool(8);
    n.is_archived  = stmt.col_bool(9);
    n.color_code   = stmt.col_text(10);
    n.created_at   = stmt.col_text(11);
    n.updated_at   = stmt.col_text(12);
    n.reminder_date = stmt.col_text_opt(13);
    n.word_count   = stmt.col_int(14);
    return n;
}

static const char* NOTE_SELECT_COLS =
    "id, title, content, category, priority, tags, tickers, sentiment, "
    "is_favorite, is_archived, color_code, created_at, updated_at, reminder_date, word_count";

Note ops::create_note(const Note& note) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t wc = count_words(note.content);
    auto stmt = db.prepare(
        "INSERT INTO notes (title, content, category, priority, tags, tickers, sentiment, "
        "is_favorite, is_archived, color_code, reminder_date, word_count) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
    stmt.bind_text(1, note.title);
    stmt.bind_text(2, note.content);
    stmt.bind_text(3, note.category);
    stmt.bind_text(4, note.priority);
    stmt.bind_text(5, note.tags);
    stmt.bind_text(6, note.tickers);
    stmt.bind_text(7, note.sentiment);
    stmt.bind_int(8, note.is_favorite ? 1 : 0);
    stmt.bind_int(9, note.is_archived ? 1 : 0);
    stmt.bind_text(10, note.color_code.empty() ? "#FF8C00" : note.color_code);
    if (note.reminder_date) stmt.bind_text(11, *note.reminder_date); else stmt.bind_null(11);
    stmt.bind_int(12, wc);
    stmt.execute();

    // Retrieve the inserted row
    auto q = db.prepare((std::string("SELECT ") + NOTE_SELECT_COLS +
                         " FROM notes WHERE id = last_insert_rowid()").c_str());
    q.step();
    return read_note_row(q);
}

std::vector<Note> ops::get_all_notes() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare((std::string("SELECT ") + NOTE_SELECT_COLS +
                            " FROM notes ORDER BY updated_at DESC").c_str());
    std::vector<Note> result;
    while (stmt.step()) {
        result.push_back(read_note_row(stmt));
    }
    return result;
}

void ops::update_note(const Note& note) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    int64_t wc = count_words(note.content);
    auto stmt = db.prepare(
        "UPDATE notes SET title=?1, content=?2, category=?3, priority=?4, "
        "tags=?5, tickers=?6, sentiment=?7, is_favorite=?8, is_archived=?9, "
        "color_code=?10, reminder_date=?11, word_count=?12, updated_at=CURRENT_TIMESTAMP "
        "WHERE id=?13");
    stmt.bind_text(1, note.title);
    stmt.bind_text(2, note.content);
    stmt.bind_text(3, note.category);
    stmt.bind_text(4, note.priority);
    stmt.bind_text(5, note.tags);
    stmt.bind_text(6, note.tickers);
    stmt.bind_text(7, note.sentiment);
    stmt.bind_int(8, note.is_favorite ? 1 : 0);
    stmt.bind_int(9, note.is_archived ? 1 : 0);
    stmt.bind_text(10, note.color_code);
    if (note.reminder_date) stmt.bind_text(11, *note.reminder_date); else stmt.bind_null(11);
    stmt.bind_int(12, wc);
    stmt.bind_int(13, note.id);
    stmt.execute();
}

void ops::delete_note(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM notes WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

std::vector<Note> ops::search_notes(const std::string& query) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string like = "%" + query + "%";
    auto stmt = db.prepare((std::string("SELECT ") + NOTE_SELECT_COLS +
        " FROM notes WHERE title LIKE ?1 OR content LIKE ?1 OR tags LIKE ?1 "
        "OR tickers LIKE ?1 ORDER BY updated_at DESC").c_str());
    stmt.bind_text(1, like);
    std::vector<Note> result;
    while (stmt.step()) {
        result.push_back(read_note_row(stmt));
    }
    return result;
}

void ops::toggle_note_favorite(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE notes SET is_favorite = NOT is_favorite, updated_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

void ops::archive_note(int64_t id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE notes SET is_archived = NOT is_archived, updated_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_int(1, id);
    stmt.execute();
}

// ============================================================================
// Algo Trading Operations
// ============================================================================

void ops::save_algo_strategy(const algo::AlgoStrategy& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_strategies
        (id, name, description, symbols, entry_conditions, exit_conditions,
         entry_logic, exit_logic, timeframe, stop_loss, take_profit, trailing_stop,
         position_size, max_positions, provider, is_active, updated_at)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, s.id);
    stmt.bind_text(2, s.name);
    stmt.bind_text(3, s.description);
    stmt.bind_text(4, s.symbols);
    stmt.bind_text(5, s.entry_conditions);
    stmt.bind_text(6, s.exit_conditions);
    stmt.bind_text(7, s.entry_logic);
    stmt.bind_text(8, s.exit_logic);
    stmt.bind_text(9, s.timeframe);
    stmt.bind_double(10, s.stop_loss);
    stmt.bind_double(11, s.take_profit);
    stmt.bind_double(12, s.trailing_stop);
    stmt.bind_double(13, s.position_size);
    stmt.bind_int(14, s.max_positions);
    stmt.bind_text(15, s.provider);
    stmt.bind_int(16, s.is_active ? 1 : 0);
    stmt.execute();
}

static algo::AlgoStrategy read_algo_strategy_row(Statement& stmt) {
    algo::AlgoStrategy s;
    s.id               = stmt.col_text(0);
    s.name             = stmt.col_text(1);
    s.description      = stmt.col_text(2);
    s.symbols          = stmt.col_text(3);
    s.entry_conditions = stmt.col_text(4);
    s.exit_conditions  = stmt.col_text(5);
    s.entry_logic      = stmt.col_text(6);
    s.exit_logic       = stmt.col_text(7);
    s.timeframe        = stmt.col_text(8);
    s.stop_loss        = stmt.col_double(9);
    s.take_profit      = stmt.col_double(10);
    s.trailing_stop    = stmt.col_double(11);
    s.position_size    = stmt.col_double(12);
    s.max_positions    = (int)stmt.col_int(13);
    s.provider         = stmt.col_text(14);
    s.is_active        = stmt.col_bool(15);
    s.created_at       = stmt.col_text(16);
    s.updated_at       = stmt.col_text(17);
    return s;
}

std::vector<algo::AlgoStrategy> ops::list_algo_strategies() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, symbols, entry_conditions, exit_conditions, "
        "entry_logic, exit_logic, timeframe, stop_loss, take_profit, trailing_stop, "
        "position_size, max_positions, provider, is_active, created_at, updated_at "
        "FROM algo_strategies ORDER BY updated_at DESC");
    std::vector<algo::AlgoStrategy> result;
    while (stmt.step()) result.push_back(read_algo_strategy_row(stmt));
    return result;
}

std::optional<algo::AlgoStrategy> ops::get_algo_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, symbols, entry_conditions, exit_conditions, "
        "entry_logic, exit_logic, timeframe, stop_loss, take_profit, trailing_stop, "
        "position_size, max_positions, provider, is_active, created_at, updated_at "
        "FROM algo_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_algo_strategy_row(stmt);
    return std::nullopt;
}

void ops::delete_algo_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM algo_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- Deployments ---

void ops::save_algo_deployment(const algo::AlgoDeployment& d) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_deployments
        (id, strategy_id, strategy_name, mode, status, pid, portfolio_id,
         started_at, stopped_at, error_message, config)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)
    )");
    stmt.bind_text(1, d.id);
    stmt.bind_text(2, d.strategy_id);
    stmt.bind_text(3, d.strategy_name);
    stmt.bind_text(4, d.mode);
    stmt.bind_text(5, algo::deployment_status_str(d.status));
    stmt.bind_int(6, d.pid);
    stmt.bind_text(7, d.portfolio_id);
    stmt.bind_text(8, d.started_at);
    stmt.bind_text(9, d.stopped_at);
    stmt.bind_text(10, d.error_message);
    stmt.bind_text(11, d.config);
    stmt.execute();
}

static algo::AlgoDeployment read_algo_deployment_row(Statement& stmt) {
    algo::AlgoDeployment d;
    d.id            = stmt.col_text(0);
    d.strategy_id   = stmt.col_text(1);
    d.strategy_name = stmt.col_text(2);
    d.mode          = stmt.col_text(3);
    d.status        = algo::parse_deployment_status(stmt.col_text(4));
    d.pid           = (int)stmt.col_int(5);
    d.portfolio_id  = stmt.col_text(6);
    d.started_at    = stmt.col_text(7);
    d.stopped_at    = stmt.col_text(8);
    d.error_message = stmt.col_text(9);
    d.config        = stmt.col_text(10);
    return d;
}

std::vector<algo::AlgoDeployment> ops::list_algo_deployments() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, strategy_id, strategy_name, mode, status, pid, portfolio_id, "
        "started_at, stopped_at, error_message, config "
        "FROM algo_deployments ORDER BY started_at DESC");
    std::vector<algo::AlgoDeployment> result;
    while (stmt.step()) result.push_back(read_algo_deployment_row(stmt));
    return result;
}

std::optional<algo::AlgoDeployment> ops::get_algo_deployment(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, strategy_id, strategy_name, mode, status, pid, portfolio_id, "
        "started_at, stopped_at, error_message, config "
        "FROM algo_deployments WHERE id = ?1");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_algo_deployment_row(stmt);
    return std::nullopt;
}

void ops::update_algo_deployment_status(const std::string& id, const std::string& status,
                                         const std::string& error) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    if (status == "stopped" || status == "error" || status == "completed") {
        auto stmt = db.prepare(
            "UPDATE algo_deployments SET status = ?2, error_message = ?3, "
            "stopped_at = CURRENT_TIMESTAMP WHERE id = ?1");
        stmt.bind_text(1, id);
        stmt.bind_text(2, status);
        stmt.bind_text(3, error);
        stmt.execute();
    } else {
        auto stmt = db.prepare(
            "UPDATE algo_deployments SET status = ?2, error_message = ?3 WHERE id = ?1");
        stmt.bind_text(1, id);
        stmt.bind_text(2, status);
        stmt.bind_text(3, error);
        stmt.execute();
    }
}

void ops::delete_algo_deployment(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    // Cascade: delete trades, metrics, signals first
    auto s1 = db.prepare("DELETE FROM algo_trades WHERE deployment_id = ?1");
    s1.bind_text(1, id); s1.execute();
    auto s2 = db.prepare("DELETE FROM algo_metrics WHERE deployment_id = ?1");
    s2.bind_text(1, id); s2.execute();
    auto s3 = db.prepare("DELETE FROM algo_order_signals WHERE deployment_id = ?1");
    s3.bind_text(1, id); s3.execute();
    auto s4 = db.prepare("DELETE FROM algo_deployments WHERE id = ?1");
    s4.bind_text(1, id); s4.execute();
}

// --- Metrics ---

void ops::save_algo_metrics(const algo::AlgoMetrics& m) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_metrics
        (deployment_id, total_trades, winning_trades, losing_trades, total_pnl,
         max_drawdown, win_rate, avg_win, avg_loss, sharpe_ratio, updated_at)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, m.deployment_id);
    stmt.bind_int(2, m.total_trades);
    stmt.bind_int(3, m.winning_trades);
    stmt.bind_int(4, m.losing_trades);
    stmt.bind_double(5, m.total_pnl);
    stmt.bind_double(6, m.max_drawdown);
    stmt.bind_double(7, m.win_rate);
    stmt.bind_double(8, m.avg_win);
    stmt.bind_double(9, m.avg_loss);
    stmt.bind_double(10, m.sharpe_ratio);
    stmt.execute();
}

std::optional<algo::AlgoMetrics> ops::get_algo_metrics(const std::string& deployment_id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT deployment_id, total_trades, winning_trades, losing_trades, total_pnl, "
        "max_drawdown, win_rate, avg_win, avg_loss, sharpe_ratio, updated_at "
        "FROM algo_metrics WHERE deployment_id = ?1");
    stmt.bind_text(1, deployment_id);
    if (!stmt.step()) return std::nullopt;
    algo::AlgoMetrics m;
    m.deployment_id = stmt.col_text(0);
    m.total_trades  = (int)stmt.col_int(1);
    m.winning_trades = (int)stmt.col_int(2);
    m.losing_trades  = (int)stmt.col_int(3);
    m.total_pnl     = stmt.col_double(4);
    m.max_drawdown  = stmt.col_double(5);
    m.win_rate      = stmt.col_double(6);
    m.avg_win       = stmt.col_double(7);
    m.avg_loss      = stmt.col_double(8);
    m.sharpe_ratio  = stmt.col_double(9);
    m.updated_at    = stmt.col_text(10);
    return m;
}

// --- Trades ---

void ops::save_algo_trade(const algo::AlgoTrade& t) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO algo_trades
        (id, deployment_id, symbol, side, quantity, price, pnl, fees, order_type, status, executed_at)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)
    )");
    stmt.bind_text(1, t.id);
    stmt.bind_text(2, t.deployment_id);
    stmt.bind_text(3, t.symbol);
    stmt.bind_text(4, t.side);
    stmt.bind_double(5, t.quantity);
    stmt.bind_double(6, t.price);
    stmt.bind_double(7, t.pnl);
    stmt.bind_double(8, t.fees);
    stmt.bind_text(9, t.order_type);
    stmt.bind_text(10, t.status);
    stmt.bind_text(11, t.executed_at);
    stmt.execute();
}

std::vector<algo::AlgoTrade> ops::get_algo_trades(const std::string& deployment_id, int limit) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, deployment_id, symbol, side, quantity, price, pnl, fees, "
        "order_type, status, executed_at FROM algo_trades "
        "WHERE deployment_id = ?1 ORDER BY executed_at DESC LIMIT ?2");
    stmt.bind_text(1, deployment_id);
    stmt.bind_int(2, limit);
    std::vector<algo::AlgoTrade> result;
    while (stmt.step()) {
        algo::AlgoTrade t;
        t.id            = stmt.col_text(0);
        t.deployment_id = stmt.col_text(1);
        t.symbol        = stmt.col_text(2);
        t.side          = stmt.col_text(3);
        t.quantity      = stmt.col_double(4);
        t.price         = stmt.col_double(5);
        t.pnl           = stmt.col_double(6);
        t.fees          = stmt.col_double(7);
        t.order_type    = stmt.col_text(8);
        t.status        = stmt.col_text(9);
        t.executed_at   = stmt.col_text(10);
        result.push_back(t);
    }
    return result;
}

// --- Order Signals ---

void ops::save_algo_signal(const algo::AlgoOrderSignal& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT INTO algo_order_signals
        (id, deployment_id, symbol, side, quantity, order_type, price, stop_price, status)
        VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)
    )");
    stmt.bind_text(1, s.id);
    stmt.bind_text(2, s.deployment_id);
    stmt.bind_text(3, s.symbol);
    stmt.bind_text(4, s.side);
    stmt.bind_double(5, s.quantity);
    stmt.bind_text(6, s.order_type);
    stmt.bind_double(7, s.price);
    stmt.bind_double(8, s.stop_price);
    stmt.bind_text(9, s.status);
    stmt.execute();
}

std::vector<algo::AlgoOrderSignal> ops::get_pending_signals() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT s.id, s.deployment_id, s.symbol, s.side, s.quantity, s.order_type, "
        "s.price, s.stop_price, s.status, s.created_at, s.executed_at, s.error "
        "FROM algo_order_signals s "
        "INNER JOIN algo_deployments d ON s.deployment_id = d.id "
        "WHERE s.status = 'pending' AND d.status = 'running' "
        "ORDER BY s.created_at ASC");
    std::vector<algo::AlgoOrderSignal> result;
    while (stmt.step()) {
        algo::AlgoOrderSignal sig;
        sig.id            = stmt.col_text(0);
        sig.deployment_id = stmt.col_text(1);
        sig.symbol        = stmt.col_text(2);
        sig.side          = stmt.col_text(3);
        sig.quantity      = stmt.col_double(4);
        sig.order_type    = stmt.col_text(5);
        sig.price         = stmt.col_double(6);
        sig.stop_price    = stmt.col_double(7);
        sig.status        = stmt.col_text(8);
        sig.created_at    = stmt.col_text(9);
        sig.executed_at   = stmt.col_text(10);
        sig.error         = stmt.col_text(11);
        result.push_back(sig);
    }
    return result;
}

void ops::update_signal_status(const std::string& id, const std::string& status,
                                const std::string& error) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE algo_order_signals SET status = ?2, error = ?3, "
        "executed_at = CURRENT_TIMESTAMP WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.bind_text(2, status);
    stmt.bind_text(3, error);
    stmt.execute();
}

// --- Candle Cache ---

void ops::insert_candles(const std::vector<algo::CandleData>& candles) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    db.begin_transaction();
    try {
        for (auto& c : candles) {
            auto stmt = db.prepare(R"(
                INSERT OR IGNORE INTO candle_cache
                (symbol, timeframe, timestamp, open, high, low, close, volume, provider)
                VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)
            )");
            stmt.bind_text(1, c.symbol);
            stmt.bind_text(2, c.timeframe);
            stmt.bind_int(3, c.timestamp);
            stmt.bind_double(4, c.open);
            stmt.bind_double(5, c.high);
            stmt.bind_double(6, c.low);
            stmt.bind_double(7, c.close);
            stmt.bind_double(8, c.volume);
            stmt.bind_text(9, c.provider);
            stmt.execute();
        }
        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}

std::vector<algo::CandleData> ops::get_candle_cache(const std::string& symbol,
                                                      const std::string& timeframe,
                                                      int limit) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT symbol, timeframe, timestamp, open, high, low, close, volume, provider "
        "FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2 "
        "ORDER BY timestamp DESC LIMIT ?3");
    stmt.bind_text(1, symbol);
    stmt.bind_text(2, timeframe);
    stmt.bind_int(3, limit);
    std::vector<algo::CandleData> result;
    while (stmt.step()) {
        algo::CandleData c;
        c.symbol    = stmt.col_text(0);
        c.timeframe = stmt.col_text(1);
        c.timestamp = stmt.col_int(2);
        c.open      = stmt.col_double(3);
        c.high      = stmt.col_double(4);
        c.low       = stmt.col_double(5);
        c.close     = stmt.col_double(6);
        c.volume    = stmt.col_double(7);
        c.provider  = stmt.col_text(8);
        result.push_back(c);
    }
    return result;
}

int64_t ops::count_candles(const std::string& symbol, const std::string& timeframe) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT COUNT(*) FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2");
    stmt.bind_text(1, symbol);
    stmt.bind_text(2, timeframe);
    if (stmt.step()) return stmt.col_int(0);
    return 0;
}

// --- Custom Python Strategies ---

void ops::save_custom_python_strategy(const algo::CustomPythonStrategy& s) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO custom_python_strategies
        (id, name, description, code, category, parameters, updated_at)
        VALUES (?1,?2,?3,?4,?5,?6,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, s.id);
    stmt.bind_text(2, s.name);
    stmt.bind_text(3, s.description);
    stmt.bind_text(4, s.code);
    stmt.bind_text(5, s.category);
    stmt.bind_text(6, s.parameters);
    stmt.execute();
}

std::vector<algo::CustomPythonStrategy> ops::list_custom_python_strategies() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, code, category, parameters, created_at, updated_at "
        "FROM custom_python_strategies ORDER BY updated_at DESC");
    std::vector<algo::CustomPythonStrategy> result;
    while (stmt.step()) {
        algo::CustomPythonStrategy s;
        s.id          = stmt.col_text(0);
        s.name        = stmt.col_text(1);
        s.description = stmt.col_text(2);
        s.code        = stmt.col_text(3);
        s.category    = stmt.col_text(4);
        s.parameters  = stmt.col_text(5);
        s.created_at  = stmt.col_text(6);
        s.updated_at  = stmt.col_text(7);
        result.push_back(s);
    }
    return result;
}

std::optional<algo::CustomPythonStrategy> ops::get_custom_python_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, name, description, code, category, parameters, created_at, updated_at "
        "FROM custom_python_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    if (!stmt.step()) return std::nullopt;
    algo::CustomPythonStrategy s;
    s.id          = stmt.col_text(0);
    s.name        = stmt.col_text(1);
    s.description = stmt.col_text(2);
    s.code        = stmt.col_text(3);
    s.category    = stmt.col_text(4);
    s.parameters  = stmt.col_text(5);
    s.created_at  = stmt.col_text(6);
    s.updated_at  = stmt.col_text(7);
    return s;
}

void ops::delete_custom_python_strategy(const std::string& id) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM custom_python_strategies WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// --- Price Cache ---

void ops::update_price_cache(const std::string& symbol, double price,
                              double volume, double change_pct) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(R"(
        INSERT OR REPLACE INTO strategy_price_cache
        (symbol, price, volume, change_pct, updated_at)
        VALUES (?1,?2,?3,?4,CURRENT_TIMESTAMP)
    )");
    stmt.bind_text(1, symbol);
    stmt.bind_double(2, price);
    stmt.bind_double(3, volume);
    stmt.bind_double(4, change_pct);
    stmt.execute();
}

std::vector<algo::PriceCacheEntry> ops::get_price_cache_entries() {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT symbol, price, volume, change_pct, updated_at FROM strategy_price_cache");
    std::vector<algo::PriceCacheEntry> result;
    while (stmt.step()) {
        algo::PriceCacheEntry e;
        e.symbol     = stmt.col_text(0);
        e.price      = stmt.col_double(1);
        e.volume     = stmt.col_double(2);
        e.change_pct = stmt.col_double(3);
        e.updated_at = stmt.col_text(4);
        result.push_back(e);
    }
    return result;
}

} // namespace ops
} // namespace fincept::db
