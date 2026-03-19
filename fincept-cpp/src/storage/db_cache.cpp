#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace fincept::db {

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

} // namespace fincept::db
