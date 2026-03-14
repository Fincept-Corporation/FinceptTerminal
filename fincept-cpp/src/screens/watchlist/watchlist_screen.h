#pragma once
// Watchlist Screen — three-panel layout: sidebar | stock list | stock detail
// Mirrors WatchlistTab.tsx from the Tauri project

#include "watchlist_types.h"
#include "watchlist_data.h"
#include "storage/database.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::watchlist {

class WatchlistScreen {
public:
    void render();

private:
    bool initialized_ = false;

    // Watchlist list
    std::vector<WatchlistWithCount> watchlists_;
    int selected_watchlist_ = -1;

    // Stock list for selected watchlist
    std::vector<WatchlistStockEntry> stocks_;
    int selected_stock_ = -1;
    SortCriteria sort_by_ = SortCriteria::Change;

    // Data fetcher
    WatchlistData data_;
    float refresh_timer_ = 0;
    static constexpr float REFRESH_INTERVAL = 600.0f; // 10 min

    // Create watchlist modal
    bool show_create_modal_ = false;
    char create_name_[64] = {};
    char create_desc_[256] = {};
    int create_color_idx_ = 0;

    // Add stock modal
    bool show_add_stock_ = false;
    char add_symbol_[16] = {};
    char add_notes_[256] = {};

    // Status
    std::string status_;
    double status_time_ = 0;

    // Init & data
    void init();
    void load_watchlists();
    void load_stocks();
    void refresh_quotes();
    void sort_stocks();

    // Rendering panels
    void render_header_bar();
    void render_info_bar();
    void render_sidebar(float width, float height);
    void render_stock_list(float width, float height);
    void render_stock_detail(float width, float height);
    void render_status_bar();

    // Modals
    void render_create_modal();
    void render_add_stock_modal();

    // Actions
    void create_watchlist();
    void delete_watchlist(const std::string& id);
    void add_stock();
    void remove_stock(const std::string& symbol);
};

} // namespace fincept::watchlist
