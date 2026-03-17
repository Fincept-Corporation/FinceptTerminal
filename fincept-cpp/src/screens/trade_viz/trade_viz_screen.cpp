// Trade Visualization — Global trade flow analysis
// Port of TradeVisualizationTab.tsx — ImPlot horizontal bar chart for
// export/import flows, partner data table, stats KPI cards, info sidebar.
// Populated with realistic dummy data per user request.

#include "trade_viz_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace fincept::trade_viz {

using namespace theme::colors;

// ============================================================================
// Country + Year lists
// ============================================================================

struct CountryDef { const char* id; const char* name; };
static const CountryDef COUNTRIES[] = {
    {"ocaus", "Australia"},   {"sachn", "China"},        {"eufra", "France"},
    {"eudeu", "Germany"},     {"asind", "India"},        {"asidn", "Indonesia"},
    {"asjpn", "Japan"},       {"askor", "South Korea"},  {"eugbr", "United Kingdom"},
    {"nausa", "United States"},{"sabra", "Brazil"},      {"euruf", "Russia"},
    {"afzaf", "South Africa"},{"assgp", "Singapore"},   {"asmys", "Malaysia"},
};
static constexpr int COUNTRY_COUNT = 15;

static const int YEARS[] = {2019, 2020, 2021, 2022, 2023};
static constexpr int YEAR_COUNT = 5;

static const int TOP_N_VALUES[] = {10, 15, 20, 30};
static const char* TOP_N_LABELS[] = {"Top 10", "Top 15", "Top 20", "Top 30"};
static constexpr int TOP_N_COUNT = 4;

// ============================================================================
// Dummy data generation — realistic trade values
// ============================================================================

void TradeVizScreen::load_dummy_data() {
    auto& c = COUNTRIES[country_idx_];
    int year = YEARS[year_idx_];
    int top_n = TOP_N_VALUES[top_n_idx_];

    data_.country_name = c.name;
    data_.country_id   = c.id;
    data_.year         = year;
    data_.hs_code      = "All Products";
    data_.partners.clear();

    // Realistic partner data based on country
    struct PartnerSeed { const char* name; const char* id; double exp_b; double imp_b; };

    // Base partners — top trading nations (values in billions)
    PartnerSeed seeds[] = {
        {"China",           "sachn", 145.2, 182.4},
        {"United States",   "nausa", 132.8, 98.6},
        {"Japan",           "asjpn", 67.3,  71.2},
        {"Germany",         "eudeu", 58.1,  82.3},
        {"South Korea",     "askor", 45.7,  52.8},
        {"United Kingdom",  "eugbr", 42.3,  35.1},
        {"Singapore",       "assgp", 38.9,  41.2},
        {"India",           "asind", 35.6,  28.4},
        {"France",          "eufra", 28.7,  34.5},
        {"Australia",       "ocaus", 26.1,  18.9},
        {"Brazil",          "sabra", 22.4,  15.8},
        {"Netherlands",     "eunld", 21.3,  28.7},
        {"Canada",          "nacan", 19.8,  14.2},
        {"Mexico",          "namex", 17.6,  21.3},
        {"Indonesia",       "asidn", 16.2,  19.8},
        {"Thailand",        "astha", 14.8,  17.1},
        {"Vietnam",         "asvnm", 13.5,  22.6},
        {"Italy",           "euita", 12.9,  18.4},
        {"Malaysia",        "asmys", 11.7,  14.3},
        {"Taiwan",          "astwn", 10.8,  16.9},
        {"Saudi Arabia",    "assau",  9.4,  24.1},
        {"UAE",             "asare",  8.7,  12.3},
        {"Russia",          "euruf",  7.2,  15.8},
        {"Spain",           "euesp",  6.8,   9.1},
        {"Switzerland",     "euche",  5.9,  11.4},
        {"South Africa",    "afzaf",  5.3,   4.7},
        {"Belgium",         "eubel",  4.8,   7.2},
        {"Hong Kong",       "ashkg",  4.2,   3.8},
        {"Poland",          "eupol",  3.7,   5.1},
        {"Chile",           "sachl",  3.1,   2.4},
    };

    // Remove self from partner list & scale based on country+year
    double scale = 1.0 + (year - 2019) * 0.05; // slight growth per year
    // Different countries have different trade volumes
    double country_scale = 1.0;
    if (std::string(c.id) == "sachn") country_scale = 3.2;
    else if (std::string(c.id) == "nausa") country_scale = 2.8;
    else if (std::string(c.id) == "eudeu") country_scale = 1.8;
    else if (std::string(c.id) == "asjpn") country_scale = 1.5;
    else if (std::string(c.id) == "asind") country_scale = 0.9;

    int count = 0;
    for (auto& s : seeds) {
        if (count >= top_n) break;
        if (std::string(s.id) == c.id) continue; // skip self

        TradePartner p;
        p.name = s.name;
        p.id   = s.id;
        p.export_value = s.exp_b * 1e9 * scale * country_scale;
        p.import_value = s.imp_b * 1e9 * scale * country_scale;
        p.total_trade  = p.export_value + p.import_value;
        data_.partners.push_back(std::move(p));
        count++;
    }

    // Sort by total trade descending
    std::sort(data_.partners.begin(), data_.partners.end(),
        [](const TradePartner& a, const TradePartner& b) { return a.total_trade > b.total_trade; });

    // Compute totals
    data_.total_export = 0;
    data_.total_import = 0;
    data_.total_trade  = 0;
    for (auto& p : data_.partners) {
        data_.total_export += p.export_value;
        data_.total_import += p.import_value;
        data_.total_trade  += p.total_trade;
    }

    has_data_ = true;
}

// ============================================================================
// Format helpers
// ============================================================================

std::string TradeVizScreen::format_currency(double val) {
    char buf[32];
    if (std::abs(val) >= 1e12)
        std::snprintf(buf, sizeof(buf), "$%.2fT", val / 1e12);
    else if (std::abs(val) >= 1e9)
        std::snprintf(buf, sizeof(buf), "$%.2fB", val / 1e9);
    else if (std::abs(val) >= 1e6)
        std::snprintf(buf, sizeof(buf), "$%.2fM", val / 1e6);
    else
        std::snprintf(buf, sizeof(buf), "$%.0f", val);
    return buf;
}

// ============================================================================
// Init
// ============================================================================

void TradeVizScreen::init() {
    load_dummy_data();
}

// ============================================================================
// Main render
// ============================================================================

void TradeVizScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }

    ui::ScreenFrame frame("##tradeviz_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);
    render_controls(w);

    float content_h = h - 120.0f; // header + controls + status
    float info_w = has_data_ ? 280.0f : 0.0f;
    float main_w = w - info_w - (has_data_ ? 2.0f : 0.0f);

    ImGui::BeginGroup();
    {
        ImGui::BeginChild("##tv_main", ImVec2(main_w, content_h), false);
        switch (active_view_) {
            case View::Chart: render_chart_view(main_w, content_h); break;
            case View::Table: render_table_view(main_w, content_h); break;
            case View::Stats: render_stats_view(main_w, content_h); break;
        }
        ImGui::EndChild();

        if (has_data_) {
            ImGui::SameLine(0, 2);
            ImGui::BeginChild("##tv_info", ImVec2(info_w, content_h), false);
            render_info_panel(info_w, content_h);
            ImGui::EndChild();
        }
    }
    ImGui::EndGroup();

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void TradeVizScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##tv_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::TextColored(ACCENT, "GLOBAL TRADE ANALYSIS");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| Real-time Trade Flow Visualization");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Control bar
// ============================================================================

void TradeVizScreen::render_controls(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(BG_PANEL.x + 0.01f, BG_PANEL.y + 0.01f, BG_PANEL.z + 0.01f, 1));
    ImGui::BeginChild("##tv_controls", ImVec2(w, 32), false);
    ImGui::SetCursorPos(ImVec2(4, 4));

    bool changed = false;

    // Country
    ImGui::Text("Country:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    if (ImGui::BeginCombo("##tv_country", COUNTRIES[country_idx_].name)) {
        for (int i = 0; i < COUNTRY_COUNT; i++) {
            if (ImGui::Selectable(COUNTRIES[i].name, i == country_idx_)) {
                country_idx_ = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 12);
    ImGui::Text("Year:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::BeginCombo("##tv_year", std::to_string(YEARS[year_idx_]).c_str())) {
        for (int i = 0; i < YEAR_COUNT; i++) {
            if (ImGui::Selectable(std::to_string(YEARS[i]).c_str(), i == year_idx_)) {
                year_idx_ = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 12);
    ImGui::Text("Partners:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("##tv_topn", TOP_N_LABELS[top_n_idx_])) {
        for (int i = 0; i < TOP_N_COUNT; i++) {
            if (ImGui::Selectable(TOP_N_LABELS[i], i == top_n_idx_)) {
                top_n_idx_ = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    if (changed) load_dummy_data();

    // View toggles
    ImGui::SameLine(0, 24);
    auto view_btn = [&](const char* label, View v) {
        bool active = (active_view_ == v);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0,0,0,1) : TEXT_DIM);
        if (ImGui::SmallButton(label)) active_view_ = v;
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    };
    view_btn("CHART", View::Chart);
    view_btn("TABLE", View::Table);
    view_btn("STATS", View::Stats);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Chart view — ImPlot horizontal bar chart (exports vs imports per partner)
// ============================================================================

void TradeVizScreen::render_chart_view(float w, float h) {
    if (!has_data_ || data_.partners.empty()) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 100, h / 2));
        ImGui::TextColored(TEXT_DIM, "No trade data available");
        return;
    }

    int n = (int)data_.partners.size();

    // Build label + value arrays
    std::vector<const char*> labels(n);
    std::vector<double> exports_b(n);
    std::vector<double> imports_b(n);
    std::vector<double> positions(n);

    for (int i = 0; i < n; i++) {
        labels[i]   = data_.partners[i].name.c_str();
        exports_b[i] = data_.partners[i].export_value / 1e9;
        imports_b[i] = data_.partners[i].import_value / 1e9;
        positions[i] = (double)i;
    }

    // Title
    ImGui::TextColored(ACCENT, "%s — Trade Partners (%d)", data_.country_name.c_str(), data_.year);

    if (ImPlot::BeginPlot("##trade_bars", ImVec2(w - 8, h - 30),
                           ImPlotFlags_NoTitle)) {

        ImPlot::SetupAxes("Trade Value ($B)", "Partner",
                          ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Set tick labels on Y axis
        ImPlot::SetupAxisTicks(ImAxis_Y1, positions.data(), n, labels.data());

        // Exports — green bars going right
        ImPlot::SetNextFillStyle(ImVec4(0.05f, 0.85f, 0.42f, 0.8f));
        ImPlot::PlotBars("Exports", exports_b.data(), n, 0.35, -0.18, ImPlotBarsFlags_Horizontal);

        // Imports — red bars going right (stacked visually below)
        ImPlot::SetNextFillStyle(ImVec4(0.96f, 0.25f, 0.25f, 0.8f));
        ImPlot::PlotBars("Imports", imports_b.data(), n, 0.35, 0.18, ImPlotBarsFlags_Horizontal);

        ImPlot::EndPlot();
    }
}

// ============================================================================
// Table view
// ============================================================================

void TradeVizScreen::render_table_view(float w, float h) {
    if (!has_data_ || data_.partners.empty()) return;

    ImGui::TextColored(ACCENT, "%s — Trading Partners (%d)", data_.country_name.c_str(), data_.year);

    int n_cols = 5;
    if (ImGui::BeginTable("##tv_table", n_cols,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable,
            ImVec2(w - 8, h - 30))) {

        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Trading Partner", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Exports", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Imports", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Total Trade", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)data_.partners.size(); i++) {
            auto& p = data_.partners[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_DIM, "%d", i + 1);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(TEXT_PRIMARY, "%s", p.name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(SUCCESS, "%s", format_currency(p.export_value).c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(ERROR_RED, "%s", format_currency(p.import_value).c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(WARNING, "%s", format_currency(p.total_trade).c_str());
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Stats view — KPI cards
// ============================================================================

void TradeVizScreen::render_stats_view(float w, float h) {
    if (!has_data_) return;

    ImGui::BeginChild("##tv_stats", ImVec2(w, h), false);

    float card_w = (w - 48) / 3.0f;
    float card_h = 90.0f;

    auto kpi_card = [&](const char* label, const std::string& value, ImVec4 color,
                        const char* sub = nullptr) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
        ImGui::BeginChild(label, ImVec2(card_w, card_h), true);
        ImGui::TextColored(TEXT_DIM, "%s", label);
        ImGui::Spacing();
        ImGui::TextColored(color, "%s", value.c_str());
        if (sub) { ImGui::Spacing(); ImGui::TextColored(TEXT_DISABLED, "%s", sub); }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    // Row 1
    ImGui::SetCursorPos(ImVec2(8, 8));
    kpi_card("TOTAL TRADE", format_currency(data_.total_trade), WARNING,
             (std::to_string(data_.year) + " | " + std::to_string(data_.partners.size()) + " partners").c_str());
    ImGui::SameLine(0, 8);
    kpi_card("TOTAL EXPORTS", format_currency(data_.total_export), SUCCESS, "Outbound trade flows");
    ImGui::SameLine(0, 8);
    kpi_card("TOTAL IMPORTS", format_currency(data_.total_import), ERROR_RED, "Inbound trade flows");

    ImGui::Spacing();

    // Row 2
    double balance = data_.total_export - data_.total_import;
    bool surplus = balance >= 0;
    kpi_card("TRADE BALANCE",
             format_currency(std::abs(balance)),
             surplus ? SUCCESS : ERROR_RED,
             surplus ? "Surplus" : "Deficit");
    ImGui::SameLine(0, 8);

    if (!data_.partners.empty()) {
        std::string top_info = data_.partners[0].name + " — " + format_currency(data_.partners[0].total_trade);
        kpi_card("TOP PARTNER", top_info, ACCENT, "Largest bilateral trade");
        ImGui::SameLine(0, 8);
    }

    double avg = data_.partners.empty() ? 0 : data_.total_trade / data_.partners.size();
    kpi_card("AVG PER PARTNER", format_currency(avg),
             ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "Mean trade value");

    // Separator + bar chart showing top 5 breakdown
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ACCENT, "Top 5 Partners — Export vs Import ($B)");

    int n = std::min(5, (int)data_.partners.size());
    if (n > 0) {
        std::vector<const char*> labels(n);
        std::vector<double> exp_vals(n), imp_vals(n);
        std::vector<double> positions(n);
        for (int i = 0; i < n; i++) {
            labels[i] = data_.partners[i].name.c_str();
            exp_vals[i] = data_.partners[i].export_value / 1e9;
            imp_vals[i] = data_.partners[i].import_value / 1e9;
            positions[i] = (double)i;
        }

        float chart_h = h - 280;
        if (chart_h > 100 && ImPlot::BeginPlot("##stats_bars", ImVec2(w - 24, chart_h))) {
            ImPlot::SetupAxes("Trade ($B)", "");
            ImPlot::SetupAxisTicks(ImAxis_Y1, positions.data(), n, labels.data());
            ImPlot::SetNextFillStyle(ImVec4(0.05f, 0.85f, 0.42f, 0.8f));
            ImPlot::PlotBars("Exports", exp_vals.data(), n, 0.35, -0.18, ImPlotBarsFlags_Horizontal);
            ImPlot::SetNextFillStyle(ImVec4(0.96f, 0.25f, 0.25f, 0.8f));
            ImPlot::PlotBars("Imports", imp_vals.data(), n, 0.35, 0.18, ImPlotBarsFlags_Horizontal);
            ImPlot::EndPlot();
        }
    }

    ImGui::EndChild();
}

// ============================================================================
// Info panel (right sidebar)
// ============================================================================

void TradeVizScreen::render_info_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    // Country info
    ImGui::TextColored(ACCENT, "INFORMATION");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##tv_cinfo", ImVec2(w - 8, 70), true);
    ImGui::TextColored(TEXT_DIM, "SELECTED COUNTRY");
    ImGui::TextColored(ACCENT, "%s", data_.country_name.c_str());
    ImGui::TextColored(TEXT_DISABLED, "ID: %s", data_.country_id.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Data details
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##tv_dinfo", ImVec2(w - 8, 80), true);
    ImGui::TextColored(TEXT_DIM, "DATA DETAILS");
    ImGui::TextColored(TEXT_PRIMARY, "Year: ");
    ImGui::SameLine();
    ImGui::TextColored(WARNING, "%d", data_.year);
    ImGui::TextColored(TEXT_PRIMARY, "HS Code: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "%s", data_.hs_code.c_str());
    ImGui::TextColored(TEXT_PRIMARY, "Partners: ");
    ImGui::SameLine();
    ImGui::TextColored(SUCCESS, "%d", (int)data_.partners.size());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Legend
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##tv_legend", ImVec2(w - 8, 70), true);
    ImGui::TextColored(TEXT_DIM, "LEGEND");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(p.x, p.y + 4), ImVec2(p.x + 16, p.y + 7),
        ImGui::ColorConvertFloat4ToU32(SUCCESS));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 22);
    ImGui::TextColored(TEXT_PRIMARY, "Exports (Outbound)");

    p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(p.x, p.y + 4), ImVec2(p.x + 16, p.y + 7),
        ImGui::ColorConvertFloat4ToU32(ERROR_RED));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 22);
    ImGui::TextColored(TEXT_PRIMARY, "Imports (Inbound)");

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleColor();
}

// ============================================================================
// Status bar
// ============================================================================

void TradeVizScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##tv_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "Fincept Trade API");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "%d Partners | Year: %d",
        (int)data_.partners.size(), data_.year);

    ImGui::SameLine(w - 280);
    ImGui::TextColored(TEXT_DIM, "Fincept Terminal | Real-time Trade Intelligence");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::trade_viz
