#include "sqlite_store.h"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace fincept::storage {

SqliteStore& SqliteStore::instance() {
    static SqliteStore store;
    return store;
}

SqliteStore::~SqliteStore() {
    close();
}

std::string SqliteStore::get_default_db_path() {
    std::string base_dir;

#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        base_dir = std::string(path) + "\\FinceptTerminal";
    } else {
        base_dir = ".";
    }
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    base_dir = std::string(home ? home : ".") + "/Library/Application Support/FinceptTerminal";
#else
    const char* home = getenv("HOME");
    base_dir = std::string(home ? home : ".") + "/.local/share/fincept-terminal";
#endif

    std::filesystem::create_directories(base_dir);
    return base_dir + "/fincept_settings.db";
}

bool SqliteStore::initialize(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    std::string path = db_path.empty() ? get_default_db_path() : db_path;

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[SqliteStore] Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Enable WAL mode for better concurrent access
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    if (!create_tables()) {
        return false;
    }

    initialized_ = true;
    std::cout << "[SqliteStore] Initialized at: " << path << std::endl;
    return true;
}

bool SqliteStore::create_tables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            category TEXT DEFAULT 'general',
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "[SqliteStore] Failed to create tables: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

void SqliteStore::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        initialized_ = false;
    }
}

bool SqliteStore::save_setting(const std::string& key, const std::string& value, const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = R"(
        INSERT OR REPLACE INTO settings (key, value, category, updated_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::optional<std::string> SqliteStore::get_setting(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    const char* sql = "SELECT value FROM settings WHERE key = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return std::nullopt;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<std::string> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (val && val[0] != '\0') {
            result = std::string(val);
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool SqliteStore::delete_setting(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM settings WHERE key = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

} // namespace fincept::storage
