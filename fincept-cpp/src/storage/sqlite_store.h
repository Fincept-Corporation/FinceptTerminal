#pragma once
// SQLite-based key-value store for session and settings persistence
// Thread-safe, auto-creates tables, matches Tauri app's storage model

#include <string>
#include <optional>
#include <mutex>

struct sqlite3;

namespace fincept::storage {

class SqliteStore {
public:
    static SqliteStore& instance();

    bool initialize(const std::string& db_path = "");
    void close();

    // Key-value operations (mirrors Tauri sqliteService)
    bool save_setting(const std::string& key, const std::string& value, const std::string& category = "general");
    std::optional<std::string> get_setting(const std::string& key);
    bool delete_setting(const std::string& key);

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

private:
    SqliteStore() = default;
    ~SqliteStore();

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
    bool initialized_ = false;

    bool create_tables();
    std::string get_default_db_path();
};

} // namespace fincept::storage
