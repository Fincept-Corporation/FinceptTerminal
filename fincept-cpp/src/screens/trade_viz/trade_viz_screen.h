#pragma once
// Trade Visualization — Global trade flow analysis with charts & data tables
// Port of TradeVisualizationTab.tsx — country trade partners, export/import
// bar charts (ImPlot), stats KPIs, partner table, right info panel

#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::trade_viz {

// ============================================================================
// Types
// ============================================================================

struct TradePartner {
    std::string name;
    std::string id;
    double export_value = 0;   // USD
    double import_value = 0;
    double total_trade  = 0;
};

struct TradeData {
    std::string country_name;
    std::string country_id;
    int year = 2023;
    std::string hs_code = "All Products";
    double total_trade  = 0;
    double total_export = 0;
    double total_import = 0;
    std::vector<TradePartner> partners;
};

// ============================================================================
// Screen
// ============================================================================

class TradeVizScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Data ---
    TradeData data_;
    bool has_data_ = false;

    // --- Selection ---
    int country_idx_ = 0;
    int year_idx_    = 4;  // index into years array (2023)
    int top_n_idx_   = 1;  // index: 0=10, 1=15, 2=20, 3=30

    // --- View ---
    enum class View { Chart, Table, Stats };
    View active_view_ = View::Chart;

    // --- Helpers ---
    void load_dummy_data();
    static std::string format_currency(double val);

    // --- Render ---
    void render_header(float w);
    void render_controls(float w);
    void render_chart_view(float w, float h);
    void render_table_view(float w, float h);
    void render_stats_view(float w, float h);
    void render_info_panel(float w, float h);
    void render_status_bar(float w);
};

} // namespace fincept::trade_viz
