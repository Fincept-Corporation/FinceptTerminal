#pragma once
// Maritime Intelligence — Global shipping & trade route visualization
// Port of MaritimeTab.tsx — trade routes, vessel tracking, area search,
// 2D map canvas with route/vessel plotting, intelligence dashboard

#include <imgui.h>
#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::maritime {

// ============================================================================
// Data types
// ============================================================================

struct LatLng {
    float lat = 0;
    float lng = 0;
};

struct TradeRoute {
    std::string name;
    std::string value;      // e.g. "$156B"
    std::string status;     // active, delayed, critical
    int vessels = 0;
    LatLng start;
    LatLng end;
    ImVec4 color = {0.4f, 0.7f, 1.0f, 0.6f};
};

struct AirRoute {
    std::string name;
    std::string value;
    int flights = 0;
    LatLng start;
    LatLng end;
    ImVec4 color = {1.0f, 0.78f, 0.39f, 0.7f};
};

struct PresetLocation {
    std::string name;
    float lat = 0;
    float lng = 0;
    std::string type;   // Port, Ship, Industry, Bank, Exchange
};

struct VesselData {
    std::string imo;
    std::string name;
    float lat = 0;
    float lng = 0;
    std::string speed;
    std::string angle;
    std::string from_port;
    std::string to_port;
};

struct IntelligenceData {
    std::string threat_level = "low";
    int active_vessels     = 0;
    int monitored_routes   = 48;
    std::string trade_volume = "$847.3B";
};

// ============================================================================
// Screen
// ============================================================================

class MaritimeScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Static data ---
    std::vector<TradeRoute> ocean_routes_;
    std::vector<AirRoute> air_routes_;
    std::vector<PresetLocation> presets_;
    IntelligenceData intel_;

    // --- Vessel data (from API) ---
    std::vector<VesselData> vessels_;
    bool loading_vessels_ = false;
    std::string vessel_error_;

    // --- Search ---
    char search_imo_[32] = {};
    VesselData search_result_;
    bool has_search_result_ = false;
    bool searching_ = false;
    std::string search_error_;

    // --- Map state ---
    ImVec2 map_offset_ = {0, 0};
    float map_zoom_ = 1.0f;
    bool show_ocean_routes_ = true;
    bool show_air_routes_   = false;
    bool show_vessels_      = true;
    bool show_ports_        = true;
    int selected_route_     = -1;

    // --- Async ---
    std::future<void> async_task_;
    bool is_async_busy() const;
    void check_async();

    // --- API calls ---
    void load_area_vessels();
    void search_vessel_by_imo();

    // --- Map projection helpers ---
    ImVec2 geo_to_screen(float lat, float lng, ImVec2 center, float w, float h) const;

    // --- Render sections ---
    void render_header(float w);
    void render_left_panel(float w, float h);
    void render_intel_stats(float w);
    void render_route_list(float w, float h);
    void render_layer_toggles(float w);
    void render_map_canvas(float w, float h);
    void render_right_panel(float w, float h);
    void render_vessel_search(float w);
    void render_search_result(float w);
    void render_preset_locations(float w);
    void render_status_bar(float w);
};

} // namespace fincept::maritime
