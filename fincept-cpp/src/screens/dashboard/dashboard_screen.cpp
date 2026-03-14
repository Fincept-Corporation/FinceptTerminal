#include "dashboard_screen.h"
#include "dashboard_helpers.h"
#include "auth/auth_manager.h"
#include "ui/yoga_helpers.h"
#include "storage/database.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstring>

namespace fincept::dashboard {

// ============================================================================
// Accent color per widget type
// ============================================================================
ImVec4 DashboardScreen::accent_for(WidgetType type) {
    switch (type) {
        case WidgetType::GlobalIndices:      return FC_ORANGE;
        case WidgetType::Forex:              return FC_YELLOW;
        case WidgetType::Commodities:        return FC_ORANGE;
        case WidgetType::Crypto:             return FC_CYAN;
        case WidgetType::SectorHeatmap:      return FC_CYAN;
        case WidgetType::News:               return FC_WHITE;
        case WidgetType::EconomicIndicators: return FC_YELLOW;
        case WidgetType::GeopoliticalRisk:   return FC_RED;
        case WidgetType::Performance:        return FC_GREEN;
        case WidgetType::TopMovers:          return FC_PURPLE;
        case WidgetType::MarketData:         return FC_BLUE;
        default: return FC_ORANGE;
    }
}

const char* DashboardScreen::loading_text(bool loading, bool has_data) {
    if (loading && !has_data) return "Loading...";
    if (loading && has_data) return nullptr;
    if (!has_data) return "No data";
    return nullptr;
}

// ============================================================================
// Init
// ============================================================================
void DashboardScreen::init() {
    if (initialized_) return;
    session_start_ = std::chrono::steady_clock::now();
    DashboardData::instance().initialize();
    load_layout();
    if (layout_.widgets.empty()) layout_ = default_layout();
    init_static_data();
    initialized_ = true;
}

void DashboardScreen::init_static_data() {
    econ_data_ = {
        {"US 10Y Treasury", 4.35f, 0.05f}, {"US GDP Growth", 2.8f, 0.1f},
        {"US Unemployment", 3.6f, -0.1f}, {"US CPI YoY", 3.2f, -0.1f},
        {"Fed Funds Rate", 5.50f, 0.0f}, {"PMI Manufacturing", 50.3f, 0.5f},
        {"Consumer Confidence", 104.7f, 2.3f}, {"Retail Sales MoM", 0.6f, 0.3f},
    };
    geo_data_ = {
        {"Global Risk", 62.5f, 2.3f}, {"US-China", 78.3f, -1.5f},
        {"Europe", 45.2f, 0.8f}, {"Middle East", 82.7f, 5.2f},
        {"Asia-Pacific", 38.5f, -0.3f}, {"Latin America", 52.1f, 1.1f},
        {"Africa", 58.4f, 0.6f}, {"Cyber Risk", 71.2f, 3.8f},
    };
    perf_data_ = {
        {"Total Return", 0.0f, 8.67f}, {"Day P/L", 15243.56f, 1.23f},
        {"Week P/L", 42567.89f, 3.45f}, {"Month P/L", 125678.90f, 8.12f},
        {"Sharpe Ratio", 1.85f, 0.0f}, {"Max Drawdown", -5.23f, 0.0f},
    };
}

// ============================================================================
// Layout persistence
// ============================================================================
void DashboardScreen::save_layout() {
    try {
        auto j = layout_.to_json();
        fincept::db::ops::save_setting("dashboard_layout", j.dump(), "dashboard");
    } catch (...) {}
}

void DashboardScreen::load_layout() {
    try {
        auto opt = fincept::db::ops::get_setting("dashboard_layout");
        if (opt.has_value() && !opt->empty()) {
            auto j = nlohmann::json::parse(*opt);
            layout_ = DashboardLayout::from_json(j);
        }
    } catch (...) {}
}

// ============================================================================
// Widget management
// ============================================================================
void DashboardScreen::add_widget(WidgetType type) {
    WidgetInstance w;
    w.id = generate_widget_id();
    w.type = type;
    w.title = widget_type_name(type);
    int max_row = 0;
    for (auto& existing : layout_.widgets) {
        int end_row = existing.row + existing.row_span;
        if (end_row > max_row) max_row = end_row;
    }
    w.col = 0; w.row = max_row; w.col_span = 1; w.row_span = 1;
    layout_.widgets.push_back(w);
    save_layout();
}

void DashboardScreen::remove_widget(const std::string& id) {
    layout_.widgets.erase(
        std::remove_if(layout_.widgets.begin(), layout_.widgets.end(),
            [&id](const WidgetInstance& w) { return w.id == id; }),
        layout_.widgets.end());
    save_layout();
}

void DashboardScreen::apply_template(const std::string& template_id) {
    for (auto& t : get_all_templates()) {
        if (t.id == template_id) { layout_ = t.create(); save_layout(); return; }
    }
}

// ============================================================================
// HEADER BAR — single row: branding + status + actions + session
// ============================================================================
void DashboardScreen::render_header_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, FC_HEADER);
    ImGui::BeginChild("##dash_header", ImVec2(width, 30), false);

    ImGui::SetCursorPos(ImVec2(8, 6));

    // Left: Branding
    ImGui::TextColored(FC_ORANGE, "FINCEPT");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(FC_MUTED, "TERMINAL");
    ImGui::SameLine(0, 10);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 10);

    // Live status
    bool has_any = DashboardData::instance().has_data(DataCategory::Indices);
    bool loading = DashboardData::instance().is_loading(DataCategory::Indices);
    if (has_any)       ImGui::TextColored(FC_GREEN, "[*] LIVE");
    else if (loading)  ImGui::TextColored(FC_YELLOW, "[~] LOADING");
    else               ImGui::TextColored(FC_RED, "[!] OFFLINE");

    ImGui::SameLine(0, 10);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 10);

    // Clock
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[32];
    std::strftime(tb, sizeof(tb), "%H:%M:%S", t);
    ImGui::TextColored(FC_YELLOW, "%s", tb);

    ImGui::SameLine(0, 10);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 10);

    // Widget count
    int visible = 0;
    for (auto& w : layout_.widgets) if (w.visible) visible++;
    ImGui::TextColored(FC_CYAN, "%d WIDGETS", visible);

    ImGui::SameLine(0, 10);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 6);

    // ---- Actions in the header row ----
    if (mini_button("[+] ADD", FC_ORANGE)) show_add_widget_modal_ = true;
    ImGui::SameLine(0, 4);
    if (mini_button("TEMPLATE", FC_CYAN)) show_template_picker_ = true;
    ImGui::SameLine(0, 4);
    if (mini_button("REFRESH", FC_GREEN)) DashboardData::instance().refresh_all();
    ImGui::SameLine(0, 4);

    // Refresh interval combo
    ImGui::PushItemWidth(48);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, FC_GRAY);
    const char* intervals[] = {"1m", "5m", "10m", "30m"};
    float interval_secs[] = {60.0f, 300.0f, 600.0f, 1800.0f};
    if (ImGui::Combo("##iv", &refresh_interval_idx_, intervals, 4)) {
        DashboardData::instance().set_refresh_interval(interval_secs[refresh_interval_idx_]);
    }
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 4);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 4);

    // Market Pulse toggle
    if (mini_button(layout_.market_pulse_open ? "PULSE [ON]" : "PULSE [OFF]",
                    layout_.market_pulse_open ? FC_ORANGE : FC_GRAY)) {
        layout_.market_pulse_open = !layout_.market_pulse_open;
        save_layout();
    }

    // Right: session uptime (push to far right)
    auto elapsed = std::chrono::steady_clock::now() - session_start_;
    int secs = (int)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    char uptime[16];
    std::snprintf(uptime, sizeof(uptime), "%02d:%02d:%02d", secs / 3600, (secs % 3600) / 60, secs % 60);

    float uptxt_w = ImGui::CalcTextSize("00:00:00").x + ImGui::CalcTextSize("SESSION:").x + 12;
    float cur_x = ImGui::GetCursorPosX();
    float right_x = width - uptxt_w - 8;
    if (right_x > cur_x + 8) {
        ImGui::SameLine(right_x);
        ImGui::TextColored(FC_GRAY, "SESSION:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(FC_CYAN, "%s", uptime);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Ticker Bar — Real scrolling market data
// ============================================================================
void DashboardScreen::render_ticker_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1.0f));
    ImGui::BeginChild("##ticker_bar", ImVec2(width, 20), false);

    auto ticker_quotes = DashboardData::instance().get_quotes(DataCategory::Ticker);

    if (ticker_quotes.empty()) {
        ImGui::SetCursorPos(ImVec2(width / 2 - 60, 2));
        ImGui::TextColored(FC_MUTED, "Loading market data...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ticker_offset_ -= ImGui::GetIO().DeltaTime * 55.0f;

    float total_w = 0;
    for (auto& q : ticker_quotes) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %.2f %+.2f%%  ", q.label.c_str(), q.price, q.change_percent);
        total_w += ImGui::CalcTextSize(buf).x;
    }
    if (total_w > 0 && ticker_offset_ < -total_w) ticker_offset_ = width;

    ImGui::SetCursorPos(ImVec2(0, 2));

    float cur_x = ticker_offset_;
    for (auto& q : ticker_quotes) {
        if (cur_x > width + 200) break;
        if (cur_x > -200) {
            ImGui::SetCursorPosX(cur_x);
            ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
            ImGui::SameLine(0, 3);
            ImGui::TextColored(FC_GRAY, "%s", fmt_price(q.price).c_str());
            ImGui::SameLine(0, 3);
            ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
            ImGui::SameLine(0, 14);
            cur_x = ImGui::GetCursorPosX();
        } else {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s %.2f %+.2f%%", q.label.c_str(), q.price, q.change_percent);
            cur_x += ImGui::CalcTextSize(buf).x + 18;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// F-Key Bar
// ============================================================================
void DashboardScreen::render_fkey_bar(float width) {
    if (width < 600) return;  // Hide on compact screens

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.03f, 0.03f, 1.0f));
    ImGui::BeginChild("##fkey_bar", ImVec2(width, 22), false);
    ImGui::SetCursorPos(ImVec2(4, 2));

    struct FKey { const char* label; ImVec4 color; };
    FKey keys[] = {
        {"F1:HELP", FC_GRAY}, {"F2:MKT", FC_ORANGE}, {"F3:NEWS", FC_ORANGE},
        {"F4:PORT", FC_GREEN}, {"F5:MOV", FC_PURPLE}, {"F6:ECON", FC_YELLOW},
        {"F7:CHT", FC_CYAN}, {"F8:TRD", FC_GREEN}, {"F9:CRY", FC_CYAN},
        {"F10:FX", FC_YELLOW}, {"F11:CMD", FC_ORANGE}, {"F12:SET", FC_GRAY}
    };

    float avail = width - 8;
    float btn_w = avail / 12.0f - 2.0f;
    if (btn_w < 30) btn_w = 30;

    for (int i = 0; i < 12; i++) {
        float used = ImGui::GetCursorPosX();
        if (used + btn_w > width - 4) break;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, keys[i].color);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 1));

        ImGui::Button(keys[i].label, ImVec2(btn_w, 17));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (i < 11) ImGui::SameLine(0, 2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Toolbar — stub (merged into header)
// ============================================================================
void DashboardScreen::render_toolbar(float /*width*/) {
    // All toolbar actions moved to header bar
}

// ============================================================================
// MAIN RENDER — Yoga-based responsive layout
// ============================================================================
void DashboardScreen::render() {
    init();

    DashboardData::instance().update(ImGui::GetIO().DeltaTime);

    // ScreenFrame replaces all manual viewport math
    ui::ScreenFrame frame("##dash", ImVec2(0, 0), FC_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float content_w = frame.width();
    float content_h = frame.height();

    // Yoga vertical stack: header(30) + accent(2) + ticker(20) + content(flex)
    auto vstack = ui::vstack_layout(content_w, content_h, {30, 2, 20, -1});

    // Header bar
    render_header_bar(content_w);

    // Orange accent line
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + content_w, p.y + vstack.heights[1]),
            ImGui::ColorConvertFloat4ToU32(FC_ORANGE));
        ImGui::Dummy(ImVec2(content_w, vstack.heights[1]));
    }

    // Ticker bar
    render_ticker_bar(content_w);

    // Main area: responsive two-panel via Yoga (widget grid + market pulse)
    float grid_area_h = vstack.heights[3];
    if (grid_area_h < 100) grid_area_h = 100;

    auto panels = ui::two_panel_layout(content_w, grid_area_h,
        layout_.market_pulse_open, 25, 280, 340);

    render_widget_grid(panels.main_w, panels.main_h);

    if (layout_.market_pulse_open && panels.side_w > 0) {
        ImGui::SameLine(0, 0);
        render_market_pulse(panels.side_w, panels.side_h);
    }

    frame.end();

    // Modals (rendered outside main window)
    render_add_widget_modal();
    render_template_picker_modal();
}

} // namespace fincept::dashboard
