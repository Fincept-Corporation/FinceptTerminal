#pragma once
// AKShare Data Explorer — Browse 26 AKShare data sources with 1000+ endpoints
// Port of AkShareDataTab.tsx — source/endpoint selection, query parameters,
// table/JSON view, pagination, sorting, favorites, history

#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::akshare {

// ============================================================================
// Data source definition (mirrors AKSHARE_DATA_SOURCES from akshareApi.ts)
// ============================================================================

struct DataSource {
    std::string id;
    std::string name;
    std::string script;       // Python script filename
    std::string description;
    ImVec4      color;
    std::vector<std::string> categories;
};

// ============================================================================
// Query history & favorites
// ============================================================================

struct QueryHistoryItem {
    std::string id;
    std::string script;
    std::string endpoint;
    int64_t     timestamp = 0;
    bool        success   = false;
    int         count     = 0;
};

struct FavoriteEndpoint {
    std::string script;
    std::string endpoint;
    std::string name;
};

// ============================================================================
// Screen
// ============================================================================

class AkShareScreen {
public:
    void render();

private:
    // --- Initialization ---
    bool initialized_ = false;
    void init();

    // --- Data sources (static catalog) ---
    std::vector<DataSource> sources_;
    int selected_source_idx_ = -1;

    // --- Endpoints (loaded per-source from Python) ---
    std::vector<std::string> endpoints_;
    std::map<std::string, std::vector<std::string>> categories_;
    std::set<std::string> expanded_categories_;
    int selected_endpoint_idx_ = -1;
    std::string selected_endpoint_;

    // --- Endpoint loading state ---
    enum class AsyncStatus { Idle, Loading, Success, Error };
    AsyncStatus endpoints_status_ = AsyncStatus::Idle;
    AsyncStatus data_status_      = AsyncStatus::Idle;

    // --- Query result data ---
    nlohmann::json data_;                     // array of objects
    std::vector<std::string> columns_;        // column headers
    int row_count_ = 0;
    std::string error_;
    std::string response_info_;               // "N rows | timestamp"

    // --- View state ---
    enum class ViewMode { Table, JSON };
    ViewMode view_mode_ = ViewMode::Table;
    int sort_col_       = -1;
    bool sort_asc_      = true;
    int current_page_   = 1;
    int page_size_      = 50;

    // --- Panel collapse ---
    bool sources_collapsed_   = false;
    bool endpoints_collapsed_ = false;

    // --- Search ---
    char source_search_[128]   = {};
    char endpoint_search_[128] = {};

    // --- Query parameters ---
    char symbol_[64]     = "000001";
    char start_date_[16] = {};
    char end_date_[16]   = {};
    int  period_idx_     = 0;   // 0=daily, 1=weekly, 2=monthly
    char market_[16]     = "sh";
    char adjust_[16]     = {};
    bool show_params_    = false;

    // --- History & Favorites ---
    std::vector<QueryHistoryItem> history_;
    std::vector<FavoriteEndpoint> favorites_;
    bool show_history_   = false;
    bool show_favorites_ = false;

    // --- Async ---
    std::future<void> async_task_;
    bool is_async_busy() const;
    void check_async();

    // --- Data fetching ---
    void load_endpoints(const DataSource& src);
    void execute_query(const std::string& script, const std::string& endpoint);
    std::vector<std::string> build_params(const std::string& endpoint) const;
    void parse_response(const nlohmann::json& j);

    // --- History / Favorites persistence ---
    void load_history_and_favorites();
    void save_history();
    void save_favorites();
    void add_history(const std::string& script, const std::string& endpoint,
                     bool success, int count);
    bool is_favorite(const std::string& script, const std::string& endpoint) const;
    void toggle_favorite(const std::string& script, const std::string& endpoint);

    // --- Render sections ---
    void render_header(float w);
    void render_sources_panel(float w, float h);
    void render_endpoints_panel(float w, float h);
    void render_params_bar(float w);
    void render_data_panel(float w, float h);
    void render_table_view(float w, float h);
    void render_json_view(float w, float h);
    void render_pagination(float w);
    void render_history_popup();
    void render_favorites_popup();
    void render_status_bar(float w);
};

} // namespace fincept::akshare
