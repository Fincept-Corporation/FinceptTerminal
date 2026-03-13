#pragma once
// Markets tab — main screen with global + regional market panels

#include "markets_data.h"
#include "markets_types.h"
#include <vector>
#include <string>
#include <ctime>

namespace fincept::markets {

class MarketsScreen {
public:
    void render();

private:
    void init();
    void render_header();
    void render_controls();
    void render_panels();
    void render_panel(const MarketCategory& cat);
    void refresh_all();

    bool initialized_ = false;

    MarketsData data_;
    std::vector<MarketCategory> global_markets_;
    std::vector<MarketCategory> regional_markets_;

    bool auto_update_ = true;
    float update_timer_ = 0;
    float update_interval_ = 600.0f; // 10 min default
    int interval_index_ = 0;        // combo index

    time_t last_refresh_ = 0;
};

} // namespace fincept::markets
