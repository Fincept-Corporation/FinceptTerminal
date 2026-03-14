#pragma once
// Geopolitics Screen — multi-panel geopolitical intelligence dashboard
// Layout: [Left: Filters] [Center: Map + Events] [Right: Event Detail]

#include "geopolitics_data.h"
#include "geopolitics_types.h"
#include "ui/tile_map.h"
#include "core/event_bus.h"
#include <string>
#include <vector>
#include <atomic>

namespace fincept::geopolitics {

class GeopoliticsScreen {
public:
    void render();

private:
    void init();
    void subscribe_auth_events();

    // Panel renderers
    void render_toolbar(float width);
    void render_filter_panel(float width, float height);
    void render_map_panel(float width, float height);
    void render_events_feed(float width, float height);
    void render_event_detail(float width, float height);
    void render_stats_bar(float width);

    // Event row
    void render_event_row(int index, const GeoEvent& event, float width);

    // Build map points from events
    void build_map_points();

    bool initialized_ = false;
    std::atomic<bool> needs_reinit_{false};
    int auth_sub_id_ = 0;

    // Data
    GeopoliticsData data_;
    double last_fetch_ = 0;

    // Map
    ui::TileMap tile_map_;
    std::vector<ui::MapPoint> map_points_;
    bool map_points_dirty_ = true;

    // View mode: 0 = map+list split, 1 = map only, 2 = list only
    int view_mode_ = 0;

    // UI State
    int selected_event_ = -1;
    char country_filter_buf_[128] = {};
    char category_filter_buf_[128] = {};
    std::string active_country_ = "India";
    std::string active_category_;
    int current_page_ = 1;

    // Cached snapshots (refreshed each frame from data_)
    std::vector<GeoEvent> events_snapshot_;
    std::vector<UniqueCountry> countries_snapshot_;
    std::vector<UniqueCategory> categories_snapshot_;
};

} // namespace fincept::geopolitics
