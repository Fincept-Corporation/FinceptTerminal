#pragma once
// Watchlist Screen — Bloomberg Terminal style
// Dense 4-zone layout: command bar | sidebar+table+detail | status bar

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
    static constexpr float REFRESH_INTERVAL = 600.0f;

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

    // Bloomberg blink state
    float blink_timer_ = 0;
    bool blink_on_ = true;

    // Init & data
    void init();
    void load_watchlists();
    void load_stocks();
    void refresh_quotes();
    void sort_stocks();

    // Bloomberg-style rendering
    void render_command_bar(float width);
    void render_sidebar(float width, float height);
    void render_stock_table(float width, float height);
    void render_detail_panel(float width, float height);
    void render_footer_bar(float width);

    // Modals
    void render_create_modal();
    void render_add_stock_modal();

    // Actions
    void create_watchlist();
    void delete_watchlist(const std::string& id);
    void add_stock();
    void remove_stock(const std::string& symbol);

    // Bloomberg drawing helpers
    void bb_label(const char* label, const char* value, ImVec4 val_color = {1,1,1,1});
    void bb_separator_line(float width);
    bool bb_func_key(const char* label, bool active, float width = 0);
};

} // namespace fincept::watchlist
