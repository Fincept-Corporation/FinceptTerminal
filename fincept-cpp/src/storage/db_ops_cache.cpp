#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace fincept::db {
namespace ops {

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

} // namespace ops
} // namespace fincept::db
