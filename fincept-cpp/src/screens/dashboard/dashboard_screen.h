#pragma once
// Fincept Terminal Dashboard — Production Fincept-style widget grid
// Real data via yfinance, resizable widgets, Market Pulse sidebar, template system
// Layout: Header | F-Key Bar | Ticker | [Widget Grid + Market Pulse] | Status

#include "dashboard_data.h"
#include "widget_registry.h"
#include "media/mpv_player.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace fincept::dashboard {

class DashboardScreen {
public:
    void render();

private:
    bool initialized_ = false;
    std::chrono::steady_clock::time_point session_start_;
    float ticker_offset_ = 0;
    uint64_t last_data_version_ = 0;       // tracks DashboardData::data_version() to skip redundant work
    uint64_t ticker_data_version_ = 0;    // version when ticker_quotes_ was last assembled
    std::vector<QuoteEntry> ticker_quotes_;  // cached assembled ticker data
    float ticker_total_width_ = 0;         // cached total text width

    // Layout state
    DashboardLayout layout_;
    bool show_add_widget_modal_ = false;
    bool show_template_picker_ = false;
    bool first_run_ = true;
    int refresh_interval_idx_ = 2;  // 0=1m, 1=5m, 2=10m, 3=30m
    float pulse_width_ = 300.0f;    // user-resizable sidebar width

    // Economic indicator data (live from MarketPulse yfinance quotes)
    struct EconEntry { std::string name; float value = 0; float change = 0; };
    std::vector<EconEntry> econ_data_;

    // Geopolitics risk data (derived from regional index volatility)
    struct GeoEntry { std::string region; float risk = 0; float trend = 0; };
    std::vector<GeoEntry> geo_data_;

    // Performance data (derived from major index returns)
    struct PerfEntry { std::string metric; float value = 0; float change = 0; };
    std::vector<PerfEntry> perf_data_;

    void init();
    void refresh_live_panels();  // econ, geo, perf from live market data

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
                              float w, float h, WidgetType type,
                              const std::string& widget_id);

    // === Original widget renderers ===
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
    void widget_youtube_stream(float w, float h);

    // === New widget renderers (migrated from React) ===
    void widget_watchlist(float w, float h, const WidgetInstance& wi);
    void widget_portfolio(float w, float h, const WidgetInstance& wi);
    void widget_stock_quote(float w, float h, const WidgetInstance& wi);
    void widget_economic_calendar(float w, float h, const WidgetInstance& wi);
    void widget_market_sentiment(float w, float h, const WidgetInstance& wi);
    void widget_quick_trade(float w, float h, const WidgetInstance& wi);
    void widget_notes(float w, float h, const WidgetInstance& wi);
    void widget_screener(float w, float h, const WidgetInstance& wi);
    void widget_risk_metrics(float w, float h, const WidgetInstance& wi);
    void widget_algo_status(float w, float h, const WidgetInstance& wi);
    void widget_alpha_arena(float w, float h, const WidgetInstance& wi);
    void widget_backtest_summary(float w, float h, const WidgetInstance& wi);
    void widget_watchlist_alerts(float w, float h, const WidgetInstance& wi);
    void widget_live_signals(float w, float h, const WidgetInstance& wi);
    void widget_dbnomics(float w, float h, const WidgetInstance& wi);
    void widget_akshare(float w, float h, const WidgetInstance& wi);
    void widget_maritime(float w, float h, const WidgetInstance& wi);
    void widget_polymarket(float w, float h, const WidgetInstance& wi);
    void widget_forum(float w, float h, const WidgetInstance& wi);
    void widget_datasource(float w, float h, const WidgetInstance& wi);

    // Widget edit/config state
    std::string editing_widget_id_;       // widget currently being edited (popup open)
    char edit_buf_[256] = {};             // generic text edit buffer
    char edit_buf2_[256] = {};            // second text buffer (for title)
    int edit_combo_idx_ = 0;             // combo selection
    float edit_float_ = 0;              // float value
    int edit_int_ = 0;                  // int value
    bool edit_just_opened_ = false;     // true on first frame of popup
    void render_widget_config_popup();   // config editor popup

    // Drag & resize state
    std::string dragging_widget_id_;
    std::string resizing_widget_id_;
    ImVec2 drag_start_mouse_ = {0, 0};
    int drag_start_col_ = 0;
    int drag_start_row_ = 0;
    int resize_start_col_span_ = 1;
    int resize_start_row_span_ = 1;
    float grid_cell_w_ = 100.0f;
    float grid_cell_h_ = 100.0f;
    float grid_gap_ = 4.0f;

    // YouTube/Video widget state
    struct YouTubeEntry {
        std::string url;
        std::string title;
        std::string author;
        bool fetched = false;
        bool fetching = false;
    };
    char yt_url_buf_[512] = {};
    std::vector<YouTubeEntry> yt_entries_;
    int yt_active_player_ = -1;
    int yt_volume_ = 80;
    std::unique_ptr<media::MpvPlayer> yt_player_;
    void yt_fetch_metadata(int index);
    void save_yt_entries();
    void load_yt_entries();

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

    // Current widget being rendered (for new widgets that need config access)
    const WidgetInstance* current_widget_ = nullptr;
};

} // namespace fincept::dashboard
