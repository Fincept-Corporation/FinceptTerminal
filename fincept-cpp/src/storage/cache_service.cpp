#include "cache_service.h"
#include "database.h"
#include "core/logger.h"
#include <chrono>

namespace fincept {

// ============================================================================
// CacheCategories — default TTL lookup
// ============================================================================
int64_t CacheCategories::default_ttl_for(const std::string& category) {
    static const std::map<std::string, int64_t> defaults = {
        {"market-quotes",     CacheTTL::FIVE_MIN},
        {"market-historical", CacheTTL::ONE_HOUR},
        {"news",              CacheTTL::TEN_MIN},
        {"forum",             CacheTTL::FIVE_MIN},
        {"economic",          CacheTTL::ONE_HOUR},
        {"reference",         CacheTTL::ONE_DAY},
        {"user-data",         CacheTTL::THIRTY_DAYS},
        {"api-response",      CacheTTL::FIFTEEN_MIN},
        {"system",            CacheTTL::THIRTY_DAYS},
    };
    auto it = defaults.find(category);
    return (it != defaults.end()) ? it->second : CacheTTL::TEN_MIN;
}

// ============================================================================
// CacheService — Singleton
// ============================================================================
CacheService& CacheService::instance() {
    static CacheService svc;
    return svc;
}

CacheService::~CacheService() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
bool CacheService::initialize() {
    if (initialized_.load()) return true;

    try {
        // Ensure cache database is initialized
        auto& cache_db = db::CacheDatabase::instance();
        if (!cache_db.initialize()) {
            LOG_ERROR("CacheService", "Failed to initialize cache database");
            return false;
        }

        // Check cache version — clear if mismatched
        check_version();

        // Start background cleanup
        start_background_cleanup();

        initialized_.store(true);
        LOG_INFO("CacheService", "Initialized (version %s)", CACHE_VERSION);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "Initialization failed: %s", e.what());
        return false;
    }
}

void CacheService::shutdown() {
    stop_background_cleanup();
    initialized_.store(false);
}

void CacheService::check_version() {
    try {
        auto entry = db::ops::cache_get_with_stale(VERSION_KEY);
        if (!entry || entry->data != std::string("\"") + CACHE_VERSION + "\"") {
            LOG_INFO("CacheService", "Cache version mismatch, clearing cache");
            db::ops::cache_clear_all();
            // Store new version
            std::string version_data = std::string("\"") + CACHE_VERSION + "\"";
            db::ops::cache_set(VERSION_KEY, version_data, "system", CacheTTL::THIRTY_DAYS);
        }
    } catch (const std::exception& e) {
        LOG_WARN("CacheService", "Version check failed: %s", e.what());
    }
}

// ============================================================================
// Core Cache Operations
// ============================================================================
std::optional<std::string> CacheService::get(const std::string& key) {
    if (!initialized_.load()) return std::nullopt;

    try {
        auto entry = db::ops::cache_get(key);
        if (!entry) return std::nullopt;
        return entry->data;
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "get error for '%s': %s", key.c_str(), e.what());
        return std::nullopt;
    }
}

std::optional<CacheService::StaleResult> CacheService::get_with_stale(const std::string& key) {
    if (!initialized_.load()) return std::nullopt;

    try {
        auto entry = db::ops::cache_get_with_stale(key);
        if (!entry) return std::nullopt;
        return StaleResult{entry->data, entry->is_expired};
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "get_with_stale error for '%s': %s", key.c_str(), e.what());
        return std::nullopt;
    }
}

bool CacheService::set(const std::string& key, const std::string& data,
                       const std::string& category, int64_t ttl) {
    if (!initialized_.load()) return false;

    try {
        // Check size limits before inserting
        ensure_cache_size();

        int64_t ttl_seconds = (ttl > 0) ? ttl : CacheCategories::default_ttl_for(category);
        db::ops::cache_set(key, data, category, ttl_seconds);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "set error for '%s': %s", key.c_str(), e.what());
        return false;
    }
}

bool CacheService::remove(const std::string& key) {
    if (!initialized_.load()) return false;

    try {
        return db::ops::cache_delete(key);
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "remove error for '%s': %s", key.c_str(), e.what());
        return false;
    }
}

std::map<std::string, std::string> CacheService::get_many(const std::vector<std::string>& keys) {
    std::map<std::string, std::string> result;
    if (!initialized_.load() || keys.empty()) return result;

    try {
        auto entries = db::ops::cache_get_many(keys);
        for (auto& entry : entries) {
            result[entry.cache_key] = std::move(entry.data);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "get_many error: %s", e.what());
    }
    return result;
}

// ============================================================================
// Category & Pattern Operations
// ============================================================================
int64_t CacheService::invalidate_category(const std::string& category) {
    if (!initialized_.load()) return 0;

    try {
        return db::ops::cache_invalidate_category(category);
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "invalidate_category error: %s", e.what());
        return 0;
    }
}

int64_t CacheService::invalidate_pattern(const std::string& pattern) {
    if (!initialized_.load()) return 0;

    try {
        return db::ops::cache_invalidate_pattern(pattern);
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "invalidate_pattern error: %s", e.what());
        return 0;
    }
}

// ============================================================================
// Cleanup & Maintenance
// ============================================================================
int64_t CacheService::cleanup() {
    try {
        return db::ops::cache_cleanup();
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "cleanup error: %s", e.what());
        return 0;
    }
}

int64_t CacheService::evict_lru(int64_t count) {
    try {
        return db::ops::cache_evict_lru(count);
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "evict_lru error: %s", e.what());
        return 0;
    }
}

void CacheService::clear_all() {
    try {
        db::ops::cache_clear_all();
        // Restore version key
        std::string version_data = std::string("\"") + CACHE_VERSION + "\"";
        db::ops::cache_set(VERSION_KEY, version_data, "system", CacheTTL::THIRTY_DAYS);
        LOG_INFO("CacheService", "Cache cleared");
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "clear_all error: %s", e.what());
    }
}

CacheService::Stats CacheService::get_stats() {
    Stats result{};
    try {
        auto db_stats = db::ops::cache_stats();
        result.total_entries = db_stats.total_entries;
        result.total_size_bytes = db_stats.total_size_bytes;
        result.expired_entries = db_stats.expired_entries;
        for (auto& cat : db_stats.categories) {
            result.categories.push_back({cat.category, cat.entry_count, cat.total_size});
        }
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "get_stats error: %s", e.what());
    }
    return result;
}

// ============================================================================
// Size Management & LRU Eviction
// ============================================================================
void CacheService::ensure_cache_size() {
    try {
        auto stats = db::ops::cache_stats();

        int64_t max_size_bytes = MAX_CACHE_SIZE_MB * 1024 * 1024;
        bool size_exceeded = stats.total_size_bytes > (int64_t)(max_size_bytes * CLEANUP_THRESHOLD);
        bool count_exceeded = stats.total_entries > (int64_t)(MAX_CACHE_ENTRIES * CLEANUP_THRESHOLD);

        if (!size_exceeded && !count_exceeded) return;

        LOG_INFO("CacheService", "Cache cleanup triggered — Size: %.2f MB, Entries: %lld",
                 (double)stats.total_size_bytes / (1024.0 * 1024.0),
                 (long long)stats.total_entries);

        // First: remove expired entries
        int64_t expired_removed = db::ops::cache_cleanup();
        if (expired_removed > 0) {
            LOG_INFO("CacheService", "Removed %lld expired entries", (long long)expired_removed);
        }

        // Re-check after cleanup
        auto new_stats = db::ops::cache_stats();
        bool still_over_size = new_stats.total_size_bytes > (int64_t)(max_size_bytes * 0.8);
        bool still_over_count = new_stats.total_entries > (int64_t)(MAX_CACHE_ENTRIES * 0.8);

        if (still_over_size || still_over_count) {
            // LRU eviction — remove 20% of entries, minimum 100
            int64_t target = std::max((int64_t)(new_stats.total_entries * 0.2), (int64_t)100);
            int64_t evicted = db::ops::cache_evict_lru(target);
            LOG_INFO("CacheService", "LRU evicted %lld entries", (long long)evicted);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "ensure_cache_size error: %s", e.what());
    }
}

// ============================================================================
// Tab Sessions
// ============================================================================
std::optional<std::string> CacheService::get_tab_session(const std::string& tab_id) {
    if (!initialized_.load()) return std::nullopt;

    try {
        auto session = db::ops::tab_session_get(tab_id);
        if (!session) return std::nullopt;
        return session->state;
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "get_tab_session error: %s", e.what());
        return std::nullopt;
    }
}

bool CacheService::set_tab_session(const std::string& tab_id, const std::string& tab_name,
                                   const std::string& state, const std::string& scroll_position,
                                   const std::string& active_filters,
                                   const std::string& selected_items) {
    if (!initialized_.load()) return false;

    try {
        db::ops::tab_session_set(tab_id, tab_name, state, scroll_position,
                                 active_filters, selected_items);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "set_tab_session error: %s", e.what());
        return false;
    }
}

bool CacheService::delete_tab_session(const std::string& tab_id) {
    if (!initialized_.load()) return false;

    try {
        db::ops::tab_session_delete(tab_id);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "delete_tab_session error: %s", e.what());
        return false;
    }
}

int64_t CacheService::cleanup_tab_sessions(int64_t max_age_days) {
    try {
        return db::ops::tab_session_cleanup(max_age_days);
    } catch (const std::exception& e) {
        LOG_ERROR("CacheService", "cleanup_tab_sessions error: %s", e.what());
        return 0;
    }
}

// ============================================================================
// Background Cleanup Thread
// ============================================================================
void CacheService::start_background_cleanup(int interval_seconds) {
    stop_background_cleanup();

    cleanup_running_.store(true);
    cleanup_thread_ = std::thread([this, interval_seconds]() {
        LOG_INFO("CacheService", "Background cleanup started (interval: %ds)", interval_seconds);

        while (cleanup_running_.load()) {
            // Sleep in small increments so we can stop quickly
            for (int i = 0; i < interval_seconds && cleanup_running_.load(); i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            if (!cleanup_running_.load()) break;

            try {
                int64_t expired = db::ops::cache_cleanup();
                if (expired > 0) {
                    LOG_INFO("CacheService", "Background cleanup: removed %lld expired entries",
                             (long long)expired);
                }

                // Also check size limits
                ensure_cache_size();

                // Cleanup old tab sessions (older than 30 days)
                db::ops::tab_session_cleanup(30);
            } catch (const std::exception& e) {
                LOG_ERROR("CacheService", "Background cleanup error: %s", e.what());
            }
        }

        LOG_INFO("CacheService", "Background cleanup stopped");
    });
}

void CacheService::stop_background_cleanup() {
    cleanup_running_.store(false);
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

// ============================================================================
// Key Generation Helpers
// ============================================================================
std::string CacheService::make_key(const std::string& category, const std::string& part1) {
    return category + ":" + part1;
}

std::string CacheService::make_key(const std::string& category, const std::string& part1,
                                   const std::string& part2) {
    return category + ":" + part1 + ":" + part2;
}

std::string CacheService::make_key(const std::string& category, const std::string& part1,
                                   const std::string& part2, const std::string& part3) {
    return category + ":" + part1 + ":" + part2 + ":" + part3;
}

} // namespace fincept
