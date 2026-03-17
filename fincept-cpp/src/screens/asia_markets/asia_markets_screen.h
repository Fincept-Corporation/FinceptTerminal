#pragma once
// Asia Markets Screen — Asian exchange data explorer with 398+ stock endpoints
// Port of AsiaMarketsTab.tsx — 8 stock categories, 4 market regions,
// endpoint browsing, query execution, table/JSON views, favorites, history

#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::asia_markets {

// ============================================================================
// Stock category definition
// ============================================================================

struct StockCategory {
    std::string id;
    std::string name;
    std::string script;
    std::string description;
    ImVec4      color;
};

// ============================================================================
// Market region
// ============================================================================

struct MarketRegion {
    std::string id;      // CN_A, CN_B, HK, US
    std::string name;
    std::string flag;    // text flag label
};

// ============================================================================
// History & Favorites (shared types with AKShare)
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

class AsiaMarketsScreen {
public:
    void render();

private:
    // --- Initialization ---
    bool initialized_ = false;
    void init();

    // --- Static catalogs ---
    std::vector<StockCategory> categories_catalog_;
    std::vector<MarketRegion>  regions_;
    int active_category_idx_ = 0;
    int active_region_idx_   = 0;

    // --- Endpoints (loaded per-category from Python) ---
    std::vector<std::string> endpoints_;
    std::map<std::string, std::vector<std::string>> endpoint_groups_;
    std::set<std::string> expanded_groups_;
    std::string selected_endpoint_;

    enum class AsyncStatus { Idle, Loading, Success, Error };
    AsyncStatus endpoints_status_ = AsyncStatus::Idle;
    AsyncStatus data_status_      = AsyncStatus::Idle;

    // --- Query result ---
    nlohmann::json data_;
    std::vector<std::string> columns_;
    int row_count_ = 0;
    std::string error_;
    std::string response_info_;

    // --- View ---
    enum class ViewMode { Table, JSON };
    ViewMode view_mode_ = ViewMode::Table;
    int sort_col_       = -1;
    bool sort_asc_      = true;

    // --- Search & params ---
    char search_buf_[128]  = {};
    char symbol_buf_[64]   = "000001";

    // --- Bottom panel ---
    enum class BottomTab { Data, Favorites, History };
    BottomTab bottom_tab_    = BottomTab::Favorites;
    bool bottom_minimized_   = false;

    // --- History & Favorites ---
    std::vector<QueryHistoryItem> history_;
    std::vector<FavoriteEndpoint> favorites_;

    // --- Async ---
    std::future<void> async_task_;
    bool is_async_busy() const;
    void check_async();

    // --- Data fetching ---
    void load_endpoints(const StockCategory& cat);
    void execute_query(const std::string& script, const std::string& endpoint);
    void parse_response(const nlohmann::json& j);

    // --- Persistence ---
    void load_history_and_favorites();
    void save_history();
    void save_favorites();
    void add_history(const std::string& script, const std::string& endpoint,
                     bool success, int count);
    bool is_favorite(const std::string& script, const std::string& endpoint) const;
    void toggle_favorite(const std::string& script, const std::string& endpoint);

    // --- Render sections ---
    void render_header(float w);
    void render_category_bar(float w);
    void render_region_bar(float w);
    void render_main_area(float w, float h);
    void render_endpoint_sidebar(float w, float h);
    void render_data_panel(float w, float h);
    void render_table_view(float w, float h);
    void render_json_view(float w, float h);
    void render_bottom_panel(float w, float h);
    void render_bottom_data(float w, float h);
    void render_bottom_favorites(float w, float h);
    void render_bottom_history(float w, float h);
    void render_status_bar(float w);
};

} // namespace fincept::asia_markets
