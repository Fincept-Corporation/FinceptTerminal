#pragma once
// Storage & Cache Section — cache stats, database browser with password protection
// Mirrors StorageCacheSection.tsx from the Tauri project

#include <imgui.h>
#include <string>
#include <vector>
#include <cstdint>

namespace fincept::settings {

class StorageCacheSection {
public:
    void render();

private:
    bool initialized_ = false;

    // Cache stats
    int64_t cache_entries_ = 0;
    int64_t cache_size_ = 0;
    int64_t cache_expired_ = 0;

    // Per-category stats
    struct CatStat { std::string name; int64_t count; int64_t size; };
    std::vector<CatStat> category_stats_;

    // Database browser
    bool db_authenticated_ = false;
    char db_password_[128] = {};
    char db_new_password_[128] = {};
    bool setting_password_ = false;

    // Table browser
    std::vector<std::string> table_names_;
    int selected_table_ = -1;
    std::vector<std::string> column_names_;
    std::vector<std::vector<std::string>> table_rows_;
    int current_page_ = 0;
    static constexpr int ROWS_PER_PAGE = 50;

    // Custom query
    char query_input_[2048] = {};
    std::string query_result_;
    bool query_error_ = false;

    // Status
    std::string status_;
    double status_time_ = 0;

    void init();
    void refresh_cache_stats();
    void load_table_list();
    void load_table_data(const std::string& table_name, int offset = 0);
    void execute_query();

    void render_cache_panel();
    void render_db_browser();
    void render_db_login();
    void render_table_browser();
};

} // namespace fincept::settings
