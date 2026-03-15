#pragma once
// Database Module — High-performance SQLite with RAII, thread-safety, and
// domain-specific operations. Mirrors the Tauri/Rust database architecture.
//
// Architecture:
//   Database        — Singleton connection manager (WAL, performance PRAGMAs)
//   CacheDatabase   — Separate DB for TTL-based caching + tab sessions
//   Operations      — Domain-specific CRUD (settings, credentials, LLM, chat, watchlist, etc.)
//   Statement       — RAII wrapper for sqlite3_stmt (auto-finalize)

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <mutex>
#include <cstdint>

// Forward declarations for algo trading types
namespace fincept::algo {
    struct AlgoStrategy;
    struct AlgoDeployment;
    struct AlgoMetrics;
    struct AlgoTrade;
    struct AlgoOrderSignal;
    struct CandleData;
    struct CustomPythonStrategy;
    struct PriceCacheEntry;
}

struct sqlite3;
struct sqlite3_stmt;

namespace fincept::db {

// ============================================================================
// RAII Statement wrapper — auto-finalizes on destruction
// ============================================================================
class Statement {
public:
    Statement() = default;
    explicit Statement(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // Binding (1-indexed)
    void bind_text(int idx, const std::string& val);
    void bind_int(int idx, int64_t val);
    void bind_double(int idx, double val);
    void bind_null(int idx);

    // Stepping
    bool step();          // returns true if SQLITE_ROW
    void execute();       // step once, expect SQLITE_DONE
    void reset();

    // Column accessors (0-indexed)
    std::string col_text(int idx) const;
    std::optional<std::string> col_text_opt(int idx) const;
    int64_t col_int(int idx) const;
    double col_double(int idx) const;
    bool col_bool(int idx) const;
    int col_count() const;

    sqlite3_stmt* raw() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// ============================================================================
// Database — Thread-safe singleton, WAL mode, performance PRAGMAs
// ============================================================================
class Database {
public:
    static Database& instance();

    bool initialize(const std::string& db_path = "");
    void close();
    bool is_open() const { return db_ != nullptr; }

    // Prepare a statement (caller owns the Statement via RAII)
    Statement prepare(const char* sql);

    // Execute SQL with no result
    void exec(const char* sql);

    // Transaction helpers
    void begin_transaction();
    void commit();
    void rollback();

    // Scoped transaction — commits on success, rolls back on exception
    template<typename Fn>
    auto transaction(Fn&& fn) -> decltype(fn()) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        begin_transaction();
        try {
            auto result = fn();
            commit();
            return result;
        } catch (...) {
            rollback();
            throw;
        }
    }

    // Lock for external compound operations
    std::recursive_mutex& mutex() { return mutex_; }

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

private:
    Database() = default;
    ~Database();

    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_;

    void create_schema();
    static std::string default_db_path();
};

// ============================================================================
// CacheDatabase — Separate DB for caching with TTL
// ============================================================================
class CacheDatabase {
public:
    static CacheDatabase& instance();

    bool initialize(const std::string& db_path = "");
    void close();

    Statement prepare(const char* sql);
    void exec(const char* sql);

    std::recursive_mutex& mutex() { return mutex_; }

    CacheDatabase(const CacheDatabase&) = delete;
    CacheDatabase& operator=(const CacheDatabase&) = delete;

private:
    CacheDatabase() = default;
    ~CacheDatabase();

    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_;

    void create_schema();
    static std::string default_cache_path();
};

// ============================================================================
// Data types — mirrors Tauri types.rs
// ============================================================================

struct Setting {
    std::string key;
    std::string value;
    std::string category;
    std::string updated_at;
};

struct Credential {
    int64_t id = 0;
    std::string service_name;
    std::optional<std::string> username;
    std::optional<std::string> password;
    std::optional<std::string> api_key;
    std::optional<std::string> api_secret;
    std::optional<std::string> additional_data;
    std::string created_at;
    std::string updated_at;
};

struct LLMConfig {
    std::string provider;
    std::optional<std::string> api_key;
    std::optional<std::string> base_url;
    std::string model;
    bool is_active = false;
    std::string created_at;
    std::string updated_at;
};

struct LLMGlobalSettings {
    double temperature = 0.7;
    int64_t max_tokens = 2000;
    std::string system_prompt;
};

struct ChatSession {
    std::string session_uuid;
    std::string title;
    int64_t message_count = 0;
    std::string created_at;
    std::string updated_at;
};

struct ChatMessage {
    std::string id;
    std::string session_uuid;
    std::string role;
    std::string content;
    std::string timestamp;
    std::optional<std::string> provider;
    std::optional<std::string> model;
    std::optional<int64_t> tokens_used;
};

struct Watchlist {
    std::string id;
    std::string name;
    std::optional<std::string> description;
    std::string color;
    std::string created_at;
    std::string updated_at;
};

struct WatchlistStock {
    std::string id;
    std::string watchlist_id;
    std::string symbol;
    std::string added_at;
    std::optional<std::string> notes;
};

struct MADealRow {
    std::string deal_id;
    std::string target_name;
    std::string acquirer_name;
    double deal_value         = 0;
    double offer_price_per_share = 0;
    double premium_1day       = 0;
    std::string payment_method;
    double payment_cash_pct   = 0;
    double payment_stock_pct  = 0;
    double ev_revenue         = 0;
    double ev_ebitda          = 0;
    double synergies          = 0;
    std::string status;
    std::string deal_type;
    std::string industry;
    std::string announced_date;
    std::string expected_close_date;
    double breakup_fee        = 0;
    int hostile_flag           = 0;
    int tender_offer_flag      = 0;
    int cross_border_flag      = 0;
    std::string acquirer_country;
    std::string target_country;
    std::string created_at;
    std::string updated_at;
};

struct CacheEntry {
    std::string cache_key;
    std::string category;
    std::string data;
    int64_t ttl_seconds;
    int64_t created_at;
    int64_t expires_at;
    int64_t last_accessed_at;
    int64_t hit_count;
    int64_t size_bytes;
    bool is_expired;
};

struct CategoryStats {
    std::string category;
    int64_t entry_count;
    int64_t total_size;
};

struct CacheStats {
    int64_t total_entries;
    int64_t total_size_bytes;
    int64_t expired_entries;
    std::vector<CategoryStats> categories;
};

struct TabSession {
    std::string tab_id;
    std::string tab_name;
    std::string state;
    std::optional<std::string> scroll_position;
    std::optional<std::string> active_filters;
    std::optional<std::string> selected_items;
    int64_t updated_at;
    int64_t created_at;
};

struct LLMModelConfig {
    std::string id;
    std::string provider;
    std::string model_id;
    std::string display_name;
    std::optional<std::string> api_key;
    std::optional<std::string> base_url;
    bool is_enabled = true;
    bool is_default = false;
    std::string created_at;
    std::string updated_at;
};

struct DataSource {
    std::string id;
    std::string alias;
    std::string display_name;
    std::optional<std::string> description;
    std::string type;       // "websocket" or "rest_api"
    std::string provider;
    std::optional<std::string> category;
    std::string config;     // JSON string
    bool enabled = true;
    std::optional<std::string> tags;
    std::string created_at;
    std::string updated_at;
};

struct AgentConfigRow {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    std::string config_json;   // Full AgentConfig serialized as JSON
    std::string created_at;
    std::string updated_at;
};

struct Note {
    int64_t id = 0;
    std::string title;
    std::string content;
    std::string category;   // RESEARCH, TRADE_IDEA, MARKET_ANALYSIS, EARNINGS, ECONOMIC, PORTFOLIO, GENERAL
    std::string priority;   // HIGH, MEDIUM, LOW
    std::string tags;       // comma-separated
    std::string tickers;    // comma-separated ticker symbols
    std::string sentiment;  // BULLISH, BEARISH, NEUTRAL
    bool is_favorite = false;
    bool is_archived = false;
    std::string color_code; // hex color
    std::string created_at;
    std::string updated_at;
    std::optional<std::string> reminder_date;
    int64_t word_count = 0;
};

// ============================================================================
// Operations — domain-specific CRUD functions
// ============================================================================
namespace ops {

// --- Settings ---
void save_setting(const std::string& key, const std::string& value,
                  const std::string& category = "general");
std::optional<std::string> get_setting(const std::string& key);
std::vector<Setting> get_all_settings();
void delete_setting(const std::string& key);

// --- Key-Value Storage ---
void storage_set(const std::string& key, const std::string& value);
std::optional<std::string> storage_get(const std::string& key);
void storage_remove(const std::string& key);
bool storage_has(const std::string& key);
std::vector<std::string> storage_keys();
void storage_set_many(const std::vector<std::pair<std::string, std::string>>& pairs);

// --- Credentials ---
void save_credential(const Credential& cred);
std::vector<Credential> get_credentials();
std::optional<Credential> get_credential_by_service(const std::string& service_name);
void delete_credential(int64_t id);

// --- LLM Config ---
std::vector<LLMConfig> get_llm_configs();
void save_llm_config(const LLMConfig& config);
LLMGlobalSettings get_llm_global_settings();
void save_llm_global_settings(const LLMGlobalSettings& settings);
void set_active_llm_provider(const std::string& provider);

// --- Chat ---
ChatSession create_chat_session(const std::string& title);
std::vector<ChatSession> get_chat_sessions(int limit = -1);
ChatMessage add_chat_message(const ChatMessage& msg);
std::vector<ChatMessage> get_chat_messages(const std::string& session_uuid);
void delete_chat_session(const std::string& session_uuid);
void update_chat_session_title(const std::string& session_uuid, const std::string& title);
void clear_chat_session_messages(const std::string& session_uuid);

// --- Watchlist ---
Watchlist create_watchlist(const std::string& name, const std::string& color,
                           const std::string& description = "");
std::vector<Watchlist> get_watchlists();
void delete_watchlist(const std::string& id);
WatchlistStock add_watchlist_stock(const std::string& watchlist_id,
                                    const std::string& symbol,
                                    const std::string& notes = "");
std::vector<WatchlistStock> get_watchlist_stocks(const std::string& watchlist_id);
void remove_watchlist_stock(const std::string& watchlist_id, const std::string& symbol);

// --- M&A Deals ---
MADealRow create_ma_deal(const MADealRow& deal);
std::vector<MADealRow> get_all_ma_deals();
std::vector<MADealRow> search_ma_deals(const std::string& query);
void update_ma_deal(const MADealRow& deal);
void delete_ma_deal(const std::string& deal_id);

// --- Cache (uses CacheDatabase) ---
std::optional<CacheEntry> cache_get(const std::string& key);
std::optional<CacheEntry> cache_get_with_stale(const std::string& key);
std::vector<CacheEntry> cache_get_many(const std::vector<std::string>& keys);
void cache_set(const std::string& key, const std::string& data,
               const std::string& category, int64_t ttl_seconds);
bool cache_delete(const std::string& key);
int64_t cache_invalidate_category(const std::string& category);
int64_t cache_invalidate_pattern(const std::string& pattern);
int64_t cache_evict_lru(int64_t count);
int64_t cache_cleanup();
CacheStats cache_stats();
void cache_clear_all();

// --- Tab Sessions (uses CacheDatabase) ---
std::optional<TabSession> tab_session_get(const std::string& tab_id);
void tab_session_set(const std::string& tab_id, const std::string& tab_name,
                     const std::string& state,
                     const std::string& scroll_position = "",
                     const std::string& active_filters = "",
                     const std::string& selected_items = "");
void tab_session_delete(const std::string& tab_id);
std::vector<TabSession> tab_session_get_all();
int64_t tab_session_cleanup(int64_t max_age_days);

// --- LLM Model Configs ---
std::vector<LLMModelConfig> get_llm_model_configs();
void save_llm_model_config(const LLMModelConfig& config);
void delete_llm_model_config(const std::string& id);
void toggle_llm_model_config_enabled(const std::string& id);

// --- Data Sources ---
void save_data_source(const DataSource& source);
std::vector<DataSource> get_all_data_sources();
void delete_data_source(const std::string& id);

// --- Agent Configs ---
void save_agent_config(const AgentConfigRow& config);
std::vector<AgentConfigRow> get_agent_configs();
std::optional<AgentConfigRow> get_agent_config(const std::string& id);
void delete_agent_config(const std::string& id);

// --- Notes ---
Note create_note(const Note& note);
std::vector<Note> get_all_notes();
void update_note(const Note& note);
void delete_note(int64_t id);
std::vector<Note> search_notes(const std::string& query);
void toggle_note_favorite(int64_t id);
void archive_note(int64_t id);

// --- Algo Trading: Strategies ---
void save_algo_strategy(const algo::AlgoStrategy& s);
std::vector<algo::AlgoStrategy> list_algo_strategies();
std::optional<algo::AlgoStrategy> get_algo_strategy(const std::string& id);
void delete_algo_strategy(const std::string& id);

// --- Algo Trading: Deployments ---
void save_algo_deployment(const algo::AlgoDeployment& d);
std::vector<algo::AlgoDeployment> list_algo_deployments();
std::optional<algo::AlgoDeployment> get_algo_deployment(const std::string& id);
void update_algo_deployment_status(const std::string& id, const std::string& status,
                                    const std::string& error = "");
void delete_algo_deployment(const std::string& id);

// --- Algo Trading: Metrics ---
void save_algo_metrics(const algo::AlgoMetrics& m);
std::optional<algo::AlgoMetrics> get_algo_metrics(const std::string& deployment_id);

// --- Algo Trading: Trades ---
void save_algo_trade(const algo::AlgoTrade& t);
std::vector<algo::AlgoTrade> get_algo_trades(const std::string& deployment_id, int limit = 100);

// --- Algo Trading: Order Signals ---
void save_algo_signal(const algo::AlgoOrderSignal& s);
std::vector<algo::AlgoOrderSignal> get_pending_signals();
void update_signal_status(const std::string& id, const std::string& status,
                           const std::string& error = "");

// --- Algo Trading: Candle Cache ---
void insert_candles(const std::vector<algo::CandleData>& candles);
std::vector<algo::CandleData> get_candle_cache(const std::string& symbol,
                                                 const std::string& timeframe,
                                                 int limit = 500);
int64_t count_candles(const std::string& symbol, const std::string& timeframe);

// --- Algo Trading: Custom Python Strategies ---
void save_custom_python_strategy(const algo::CustomPythonStrategy& s);
std::vector<algo::CustomPythonStrategy> list_custom_python_strategies();
std::optional<algo::CustomPythonStrategy> get_custom_python_strategy(const std::string& id);
void delete_custom_python_strategy(const std::string& id);

// --- Algo Trading: Price Cache ---
void update_price_cache(const std::string& symbol, double price,
                         double volume, double change_pct);
std::vector<algo::PriceCacheEntry> get_price_cache_entries();

} // namespace ops

// ============================================================================
// UUID generation (simple v4)
// ============================================================================
std::string generate_uuid();

// Current unix timestamp
int64_t now_timestamp();

} // namespace fincept::db
