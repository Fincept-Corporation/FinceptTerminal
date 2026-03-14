#pragma once
// Fincept Terminal Dashboard — Production Fincept-style widget grid
// Real data via yfinance, resizable widgets, Market Pulse sidebar, template system
// Layout: Header | F-Key Bar | Ticker | [Widget Grid + Market Pulse] | Status

#include "dashboard_data.h"
#include "widget_registry.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <chrono>

namespace fincept::dashboard {

class DashboardScreen {
public:
    void render();

private:
    bool initialized_ = false;
    std::chrono::steady_clock::time_point session_start_;
    float ticker_offset_ = 0;

    // Layout state
    DashboardLayout layout_;
    bool show_add_widget_modal_ = false;
    bool show_template_picker_ = false;
    bool first_run_ = true;
    int refresh_interval_idx_ = 2;  // 0=1m, 1=5m, 2=10m, 3=30m

    // Economic indicator data (hardcoded for now — FRED integration later)
    struct EconEntry { std::string name; float value; float change; };
    std::vector<EconEntry> econ_data_;

    // Geopolitics risk data (hardcoded for now — API integration later)
    struct GeoEntry { std::string region; float risk; float trend; };
    std::vector<GeoEntry> geo_data_;

    // Performance data (hardcoded for now — portfolio integration later)
    struct PerfEntry { std::string metric; float value; float change; };
    std::vector<PerfEntry> perf_data_;

    void init();
    void init_static_data();  // econ, geo, perf (until APIs are connected)

    // Top-level layout sections
    void render_header_bar(float width);
    void render_fkey_bar(float width);
    void render_ticker_bar(float width);
    void render_toolbar(float width);
    void render_widget_grid(float width, float height);
    void render_market_pulse(float width, float height);
    void render_status_bar(float width);

    // Widget frame — wraps any widget with header, accent, close button
    void render_widget_frame(const char* title, const ImVec4& accent,
                              float w, float h, WidgetType type);

    // Individual widget renderers (read from DashboardData)
    void widget_global_indices(float w, float h);
    void widget_forex(float w, float h);
    void widget_commodities(float w, float h);
    void widget_crypto(float w, float h);
    void widget_sector_heatmap(float w, float h);
    void widget_news(float w, float h);
    void widget_economic(float w, float h);
    void widget_geopolitics(float w, float h);
    void widget_performance(float w, float h);
    void widget_top_movers(float w, float h);
    void widget_market_data(float w, float h);

    // Render a generic quote table (reused by many widgets)
    void render_quote_table(const char* table_id,
                            const std::vector<QuoteEntry>& quotes,
                            const char* col1, const char* col2, const char* col3,
                            bool show_volume, int price_decimals);

    // Modals
    void render_add_widget_modal();
    void render_template_picker_modal();

    // Widget management
    void add_widget(WidgetType type);
    void remove_widget(const std::string& id);
    void apply_template(const std::string& template_id);

    // Layout persistence
    void save_layout();
    void load_layout();

    // Helpers
    static ImVec4 accent_for(WidgetType type);
    static const char* loading_text(bool loading, bool has_data);
};

} // namespace fincept::dashboard
