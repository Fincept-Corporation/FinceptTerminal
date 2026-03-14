#pragma once
// CacheService — High-level caching layer with TTL presets, size management,
// LRU eviction, background cleanup, and version-aware cache invalidation.
// Mirrors cacheService.ts from the Tauri project.
//
// Usage:
//   auto& cache = fincept::CacheService::instance();
//   cache.initialize();
//   cache.set("market-quotes:AAPL", json_str, "market-quotes");
//   auto result = cache.get("market-quotes:AAPL");

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>

namespace fincept {

// ============================================================================
// TTL Presets (seconds)
// ============================================================================
namespace CacheTTL {
    constexpr int64_t ONE_MIN    = 60;
    constexpr int64_t FIVE_MIN   = 300;
    constexpr int64_t TEN_MIN    = 600;
    constexpr int64_t FIFTEEN_MIN = 900;
    constexpr int64_t THIRTY_MIN = 1800;
    constexpr int64_t ONE_HOUR   = 3600;
    constexpr int64_t SIX_HOURS  = 21600;
    constexpr int64_t TWELVE_HOURS = 43200;
    constexpr int64_t ONE_DAY    = 86400;
    constexpr int64_t SEVEN_DAYS = 604800;
    constexpr int64_t THIRTY_DAYS = 2592000;

    // Backward compat aliases
    constexpr int64_t SHORT  = ONE_MIN;
    constexpr int64_t MEDIUM = FIVE_MIN;
    constexpr int64_t LONG   = TEN_MIN;
    constexpr int64_t HOUR   = ONE_HOUR;
    constexpr int64_t DAY    = ONE_DAY;
    constexpr int64_t WEEK   = SEVEN_DAYS;
}

// ============================================================================
// Cache Categories with default TTLs
// ============================================================================
struct CacheCategoryInfo {
    const char* name;
    int64_t default_ttl;
};

namespace CacheCategories {
    constexpr CacheCategoryInfo MARKET_QUOTES     = {"market-quotes",     CacheTTL::FIVE_MIN};
    constexpr CacheCategoryInfo MARKET_HISTORICAL = {"market-historical", CacheTTL::ONE_HOUR};
    constexpr CacheCategoryInfo NEWS              = {"news",              CacheTTL::TEN_MIN};
    constexpr CacheCategoryInfo FORUM             = {"forum",             CacheTTL::FIVE_MIN};
    constexpr CacheCategoryInfo ECONOMIC          = {"economic",          CacheTTL::ONE_HOUR};
    constexpr CacheCategoryInfo REFERENCE         = {"reference",         CacheTTL::ONE_DAY};
    constexpr CacheCategoryInfo USER_DATA         = {"user-data",         CacheTTL::THIRTY_DAYS};
    constexpr CacheCategoryInfo API_RESPONSE      = {"api-response",      CacheTTL::FIFTEEN_MIN};
    constexpr CacheCategoryInfo SYSTEM            = {"system",            CacheTTL::THIRTY_DAYS};

    // Get default TTL for a category name, returns 600 (10 min) if unknown
    int64_t default_ttl_for(const std::string& category);
}

// ============================================================================
// CacheService — Singleton
// ============================================================================
class CacheService {
public:
    static CacheService& instance();

    // --- Lifecycle ---
    bool initialize();
    void shutdown();
    bool is_initialized() const { return initialized_.load(); }

    // --- Core Cache Operations ---

    // Get a cached value (returns nullopt if expired or not found)
    std::optional<std::string> get(const std::string& key);

    // Get cached value even if stale (for stale-while-revalidate pattern)
    struct StaleResult {
        std::string data;
        bool is_stale;
    };
    std::optional<StaleResult> get_with_stale(const std::string& key);

    // Set a cached value. If ttl == 0, uses the category's default TTL.
    bool set(const std::string& key, const std::string& data,
             const std::string& category, int64_t ttl = 0);

    // Delete a cached value
    bool remove(const std::string& key);

    // Get multiple cached values
    std::map<std::string, std::string> get_many(const std::vector<std::string>& keys);

    // --- Category & Pattern Operations ---
    int64_t invalidate_category(const std::string& category);
    int64_t invalidate_pattern(const std::string& pattern); // SQL LIKE pattern, use % wildcard

    // --- Cleanup & Maintenance ---
    int64_t cleanup();           // Remove expired entries
    int64_t evict_lru(int64_t count); // Remove N least recently used entries
    void clear_all();            // Clear entire cache (preserves version key)

    // --- Stats ---
    struct Stats {
        int64_t total_entries;
        int64_t total_size_bytes;
        int64_t expired_entries;
        struct Category {
            std::string name;
            int64_t entry_count;
            int64_t total_size;
        };
        std::vector<Category> categories;
    };
    Stats get_stats();

    // --- Tab Sessions ---
    std::optional<std::string> get_tab_session(const std::string& tab_id);
    bool set_tab_session(const std::string& tab_id, const std::string& tab_name,
                         const std::string& state,
                         const std::string& scroll_position = "",
                         const std::string& active_filters = "",
                         const std::string& selected_items = "");
    bool delete_tab_session(const std::string& tab_id);
    int64_t cleanup_tab_sessions(int64_t max_age_days = 30);

    // --- Background Cleanup ---
    void start_background_cleanup(int interval_seconds = 900); // default 15 min
    void stop_background_cleanup();

    // --- Key Generation Helpers ---
    static std::string make_key(const std::string& category, const std::string& part1);
    static std::string make_key(const std::string& category, const std::string& part1,
                                const std::string& part2);
    static std::string make_key(const std::string& category, const std::string& part1,
                                const std::string& part2, const std::string& part3);

    // --- Configuration ---
    static constexpr const char* CACHE_VERSION = "1.0.0";
    static constexpr int64_t MAX_CACHE_SIZE_MB = 100;
    static constexpr int64_t MAX_CACHE_ENTRIES = 10000;
    static constexpr double CLEANUP_THRESHOLD = 0.9;

    CacheService(const CacheService&) = delete;
    CacheService& operator=(const CacheService&) = delete;

private:
    CacheService() = default;
    ~CacheService();

    std::atomic<bool> initialized_{false};

    // Background cleanup thread
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_{false};
    std::mutex cleanup_mutex_;

    // Size management — evicts LRU entries when limits are approached
    void ensure_cache_size();

    // Version management
    void check_version();

    static constexpr const char* VERSION_KEY = "__cache_version__";
};

} // namespace fincept
