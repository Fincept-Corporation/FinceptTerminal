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
        case WidgetType::VideoPlayer:        return FC_RED;
        case WidgetType::Watchlist:          return FC_CYAN;
        case WidgetType::Portfolio:          return FC_GREEN;
        case WidgetType::StockQuote:         return FC_ORANGE;
        case WidgetType::EconomicCalendar:   return FC_YELLOW;
        case WidgetType::MarketSentiment:    return FC_PURPLE;
        case WidgetType::QuickTrade:         return FC_GREEN;
        case WidgetType::Notes:              return FC_ORANGE;
        case WidgetType::Screener:           return FC_CYAN;
        case WidgetType::RiskMetrics:        return FC_RED;
        case WidgetType::AlgoStatus:         return FC_GREEN;
        case WidgetType::AlphaArena:         return FC_PURPLE;
        case WidgetType::BacktestSummary:    return FC_BLUE;
        case WidgetType::WatchlistAlerts:    return FC_YELLOW;
        case WidgetType::LiveSignals:        return FC_GREEN;
        case WidgetType::DBnomics:           return FC_BLUE;
        case WidgetType::AkShare:            return FC_RED;
        case WidgetType::Maritime:           return FC_CYAN;
        case WidgetType::Polymarket:         return FC_PURPLE;
        case WidgetType::Forum:              return FC_ORANGE;
        case WidgetType::DataSource:         return FC_BLUE;
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
    refresh_live_panels();
    initialized_ = true;
}

void DashboardScreen::refresh_live_panels() {
    // Economic indicators — derived from real MarketPulse quotes (^TNX, ^VIX, DXY, etc.)
    auto pulse = DashboardData::instance().get_quotes(DataCategory::MarketPulse);
    econ_data_.clear();
    for (auto& q : pulse) {
        econ_data_.push_back({q.label, (float)q.price, (float)q.change});
    }
    if (econ_data_.empty()) {
        // Fallback: show loading state
        econ_data_.push_back({"Waiting for market data...", 0, 0});
    }

    // Geopolitics risk — derived from real regional index volatility
    // Uses actual index change% as a risk proxy (bigger move = higher risk)
    auto indices = DashboardData::instance().get_quotes(DataCategory::Indices);
    geo_data_.clear();
    // Map regional indices to regions
    struct RegionMap { const char* symbol; const char* region; };
    RegionMap regions[] = {
        {"^GSPC", "North America"}, {"^DJI", "US Large Cap"},
        {"^FTSE", "UK / Europe"}, {"^GDAXI", "Germany"},
        {"^FCHI", "France"}, {"^N225", "Japan"},
        {"^HSI", "Hong Kong"}, {"000001.SS", "China"},
        {"^BSESN", "India"}, {"^NSEI", "India (Nifty)"},
        {"^STOXX50E", "Euro Zone"}, {"^AXJO", "Australia"},
    };
    for (auto& rm : regions) {
        for (auto& q : indices) {
            if (q.symbol == rm.symbol) {
                // Risk = abs(change%) scaled: 0-1% = low (20-40), 1-3% = medium (40-70), >3% = high (70-100)
                float abs_chg = std::abs((float)q.change_percent);
                float risk = std::min(100.0f, 20.0f + abs_chg * 25.0f);
                geo_data_.push_back({rm.region, risk, (float)q.change_percent});
                break;
            }
        }
    }
    if (geo_data_.empty()) {
        geo_data_.push_back({"Waiting for index data...", 0, 0});
    }

    // Performance — derived from real portfolio index returns
    perf_data_.clear();
    for (auto& q : indices) {
        if (q.symbol == "^GSPC") {
            perf_data_.push_back({"S&P 500 Return", (float)q.price, (float)q.change_percent});
        } else if (q.symbol == "^IXIC") {
            perf_data_.push_back({"NASDAQ Return", (float)q.price, (float)q.change_percent});
        } else if (q.symbol == "^DJI") {
            perf_data_.push_back({"DOW Return", (float)q.price, (float)q.change_percent});
        }
    }
    for (auto& q : pulse) {
        if (q.symbol == "^VIX") {
            perf_data_.push_back({"Volatility (VIX)", (float)q.price, (float)q.change});
        } else if (q.symbol == "^TNX") {
            perf_data_.push_back({"10Y Yield", (float)q.price, (float)q.change});
        }
    }
    if (perf_data_.empty()) {
        perf_data_.push_back({"Waiting for data...", 0, 0});
    }
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

    // Sanitize widget positions: clamp to grid bounds
    const int gc = std::max(1, layout_.grid_cols);
    const int gr = std::max(1, layout_.grid_rows);
    for (auto& w : layout_.widgets) {
        w.col_span = std::clamp(w.col_span, 1, gc);
        w.row_span = std::clamp(w.row_span, 1, gr);
        w.col = std::clamp(w.col, 0, gc - w.col_span);
        w.row = std::clamp(w.row, 0, gr - w.row_span);
    }
}

// ============================================================================
// YouTube entries persistence
// ============================================================================
void DashboardScreen::save_yt_entries() {
    try {
        nlohmann::json j = nlohmann::json::array();
        for (auto& e : yt_entries_) {
            j.push_back({{"url", e.url}, {"title", e.title}, {"author", e.author}});
        }
        fincept::db::ops::save_setting("dashboard_yt_entries", j.dump(), "dashboard");
    } catch (...) {}
}

void DashboardScreen::load_yt_entries() {
    try {
        auto opt = fincept::db::ops::get_setting("dashboard_yt_entries");
        if (opt.has_value() && !opt->empty()) {
            auto j = nlohmann::json::parse(*opt);
            if (j.is_array()) {
                for (auto& ej : j) {
                    YouTubeEntry entry;
                    entry.url = ej.value("url", "");
                    entry.title = ej.value("title", "");
                    entry.author = ej.value("author", "");
                    entry.fetched = !entry.title.empty();
                    if (!entry.url.empty()) {
                        yt_entries_.push_back(std::move(entry));
                    }
                }
            }
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

    // Default config for widgets that need it (matches React DEFAULT_WIDGET_CONFIGS)
    switch (type) {
        case WidgetType::StockQuote:       w.config = {{"stockSymbol","AAPL"}}; break;
        case WidgetType::EconomicCalendar: w.config = {{"country","US"},{"limit",10}}; break;
        case WidgetType::Screener:         w.config = {{"screenerPreset","value"}}; break;
        case WidgetType::WatchlistAlerts:  w.config = {{"alertThreshold",3}}; break;
        case WidgetType::Watchlist:        w.config = {{"watchlistName","Default"}}; break;
        case WidgetType::DBnomics:         w.config = {{"dbnomicsSeriesId","FRED/UNRATE"}}; break;
        case WidgetType::Notes:            w.config = {{"notesCategory","all"},{"notesLimit",10}}; break;
        default: break;
    }

    int grid_cols = std::max(layout_.grid_cols, 1);

    // Find the last occupied row and scan that row for an empty col slot.
    int last_row = 0;
    for (auto& existing : layout_.widgets) {
        if (!existing.visible) continue;
        last_row = std::max(last_row, existing.row);
    }

    // Build occupancy for last_row only
    std::vector<bool> row_occupied(grid_cols, false);
    for (auto& existing : layout_.widgets) {
        if (!existing.visible) continue;
        if (existing.row == last_row) {
            for (int c = existing.col; c < existing.col + existing.col_span && c < grid_cols; c++)
                row_occupied[c] = true;
        }
    }

    // Find first free col in last_row
    bool found = false;
    for (int c = 0; c < grid_cols; c++) {
        if (!row_occupied[c]) {
            w.col = c;
            w.row = last_row;
            found = true;
            break;
        }
    }

    // Last row full — start a new row
    if (!found) {
        w.col = 0;
        w.row = last_row + 1;
    }

    w.col_span = 1;
    w.row_span = 1;
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

    // Clock — cached per second to avoid localtime() every frame
    {
        static time_t cached_time = 0;
        static char cached_tb[32] = {};
        time_t now = time(nullptr);
        if (now != cached_time) {
            cached_time = now;
            struct tm t_buf;
#ifdef _WIN32
            localtime_s(&t_buf, &now);
#else
            localtime_r(&now, &t_buf);
#endif
            std::strftime(cached_tb, sizeof(cached_tb), "%H:%M:%S", &t_buf);
        }
        ImGui::TextColored(FC_YELLOW, "%s", cached_tb);
    }

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

    // Rebuild ticker cache only when data version changes (avoids 4 vector copies + CalcTextSize per frame)
    const uint64_t dv = DashboardData::instance().data_version();
    if (dv != ticker_data_version_) {
        ticker_data_version_ = dv;
        ticker_quotes_.clear();
        for (auto cat : {DataCategory::Indices, DataCategory::Crypto, DataCategory::Forex, DataCategory::MarketPulse}) {
            auto q = DashboardData::instance().get_quotes(cat);
            ticker_quotes_.insert(ticker_quotes_.end(), q.begin(), q.end());
        }
        ticker_total_width_ = 0;
        for (auto& q : ticker_quotes_) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s %.2f %+.2f%%  ", q.label.c_str(), q.price, q.change_percent);
            ticker_total_width_ += ImGui::CalcTextSize(buf).x;
        }
    }

    if (ticker_quotes_.empty()) {
        ImGui::SetCursorPos(ImVec2(width / 2 - 60, 2));
        ImGui::TextColored(FC_MUTED, "Loading market data...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ticker_offset_ -= ImGui::GetIO().DeltaTime * 55.0f;

    if (ticker_total_width_ > 0 && ticker_offset_ < -ticker_total_width_) ticker_offset_ = width;

    ImGui::SetCursorPos(ImVec2(0, 2));

    float cur_x = ticker_offset_;
    for (auto& q : ticker_quotes_) {
        if (cur_x > width + 200) break;
        if (cur_x > -200) {
            ImGui::SetCursorPosX(cur_x);
            ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
            ImGui::SameLine(0, 3);
            char pb[32]; fmt_price_buf(pb, sizeof(pb), q.price);
            ImGui::TextColored(FC_GRAY, "%s", pb);
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

    // Only refresh derived panels when upstream data has changed (skip 59/60 frames)
    const uint64_t cur_ver = DashboardData::instance().data_version();
    if (cur_ver != last_data_version_) {
        last_data_version_ = cur_ver;
        refresh_live_panels();
    }

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

    // Main area: widget grid + optional resizable Market Pulse sidebar
    float grid_area_h = vstack.heights[3];
    if (grid_area_h < 100) grid_area_h = 100;

    if (layout_.market_pulse_open) {
        const float min_side = 220.0f;
        const float max_side = 420.0f;
        const float splitter_w = 4.0f;
        pulse_width_ = std::clamp(pulse_width_, min_side, max_side);

        float grid_w = content_w - pulse_width_ - splitter_w;
        if (grid_w < 200.0f) grid_w = 200.0f;

        render_widget_grid(grid_w, grid_area_h);
        ImGui::SameLine(0, 0);

        // Splitter handle — 4px wide, draggable
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f,0.12f,0.12f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f,0.533f,0.0f,0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f,0.533f,0.0f,0.8f));
            ImGui::Button("##splitter", ImVec2(splitter_w, grid_area_h));
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemActive()) {
                pulse_width_ -= ImGui::GetIO().MouseDelta.x;
                pulse_width_ = std::clamp(pulse_width_, min_side, max_side);
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        ImGui::SameLine(0, 0);

        // Sidebar — fixed width, no ResizeX
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f,0.04f,0.04f,1.0f));
        ImGui::BeginChild("##pulse_panel", ImVec2(pulse_width_, grid_area_h),
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        render_market_pulse(pulse_width_, grid_area_h);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else {
        render_widget_grid(content_w, grid_area_h);
    }

    frame.end();

    // Modals (rendered outside main window)
    render_add_widget_modal();
    render_template_picker_modal();
    render_widget_config_popup();
}

} // namespace fincept::dashboard
