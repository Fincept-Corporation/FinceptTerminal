#include "dashboard_screen.h"
#include "auth/auth_manager.h"
#include "theme/bloomberg_theme.h"
#include "storage/sqlite_store.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstring>

namespace fincept::dashboard {

// ============================================================================
// Bloomberg color palette
// ============================================================================
static const ImVec4 FC_ORANGE  = ImVec4(1.0f, 0.533f, 0.0f, 1.0f);
static const ImVec4 FC_GREEN   = ImVec4(0.0f, 0.84f, 0.435f, 1.0f);
static const ImVec4 FC_RED     = ImVec4(1.0f, 0.231f, 0.231f, 1.0f);
static const ImVec4 FC_CYAN    = ImVec4(0.0f, 0.898f, 1.0f, 1.0f);
static const ImVec4 FC_YELLOW  = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);
static const ImVec4 FC_GRAY    = ImVec4(0.47f, 0.47f, 0.47f, 1.0f);
static const ImVec4 FC_MUTED   = ImVec4(0.29f, 0.29f, 0.29f, 1.0f);
static const ImVec4 FC_WHITE   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
static const ImVec4 FC_PANEL   = ImVec4(0.059f, 0.059f, 0.059f, 1.0f);
static const ImVec4 FC_HEADER  = ImVec4(0.102f, 0.102f, 0.102f, 1.0f);
static const ImVec4 FC_BORDER  = ImVec4(0.165f, 0.165f, 0.165f, 1.0f);
static const ImVec4 FC_DARK    = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
static const ImVec4 FC_BLUE    = ImVec4(0.0f, 0.533f, 1.0f, 1.0f);
static const ImVec4 FC_PURPLE  = ImVec4(0.616f, 0.306f, 0.867f, 1.0f);

static ImVec4 chg_col(double v) { return v > 0 ? FC_GREEN : (v < 0 ? FC_RED : FC_GRAY); }

static std::string fmt_price(double v, int decimals = 2) {
    char b[32];
    if (v == 0) return "--";
    if (std::abs(v) >= 1e6) std::snprintf(b, sizeof(b), "%.2fM", v / 1e6);
    else if (std::abs(v) >= 1e4) std::snprintf(b, sizeof(b), "%.*fK", decimals > 0 ? 1 : 0, v / 1e3);
    else std::snprintf(b, sizeof(b), "%.*f", decimals, v);
    return b;
}

static std::string fmt_volume(double v) {
    char b[32];
    if (v >= 1e9) std::snprintf(b, sizeof(b), "%.1fB", v / 1e9);
    else if (v >= 1e6) std::snprintf(b, sizeof(b), "%.1fM", v / 1e6);
    else if (v >= 1e3) std::snprintf(b, sizeof(b), "%.1fK", v / 1e3);
    else std::snprintf(b, sizeof(b), "%.0f", v);
    return b;
}

// Helper: small styled button
static bool mini_button(const char* label, ImVec4 text_col) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.12f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.08f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, text_col);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    bool clicked = ImGui::SmallButton(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

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
        fincept::storage::SqliteStore::instance().save_setting("dashboard_layout", j.dump(), "dashboard");
    } catch (...) {}
}

void DashboardScreen::load_layout() {
    try {
        auto opt = fincept::storage::SqliteStore::instance().get_setting("dashboard_layout");
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
// F-Key Bar (removed separate toolbar — merged Add Widget into header)
// ============================================================================
void DashboardScreen::render_fkey_bar(float width) {
    // Skip F-key bar if window too narrow
    if (width < 600) return;

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
        if (used + btn_w > width - 4) break; // stop if overflow

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
// Toolbar — now empty stub (merged into header)
// ============================================================================
void DashboardScreen::render_toolbar(float /*width*/) {
    // All toolbar actions moved to header bar
}

// ============================================================================
// Generic quote table renderer
// ============================================================================
void DashboardScreen::render_quote_table(
    const char* table_id,
    const std::vector<QuoteEntry>& quotes,
    const char* col1, const char* col2, const char* col3,
    bool show_volume, int price_decimals)
{
    int num_cols = show_volume ? 4 : 3;
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable(table_id, num_cols, flags,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn(col1, 0, 2.0f);
        ImGui::TableSetupColumn(col2, 0, 1.5f);
        ImGui::TableSetupColumn(col3, 0, 1.0f);
        if (show_volume) ImGui::TableSetupColumn("Vol", 0, 1.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (auto& q : quotes) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", fmt_price(q.price, price_decimals).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
            if (show_volume) {
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_MUTED, "%s", fmt_volume(q.volume).c_str());
            }
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Individual widget content renderers
// ============================================================================
void DashboardScreen::widget_global_indices(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::Indices);
    if (auto* msg = loading_text(DashboardData::instance().is_loading(DataCategory::Indices), !quotes.empty())) {
        ImGui::TextColored(FC_MUTED, "%s", msg); return;
    }
    render_quote_table("##idx", quotes, "Index", "Value", "Chg%", false, 2);
}

void DashboardScreen::widget_forex(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::Forex);
    if (auto* msg = loading_text(DashboardData::instance().is_loading(DataCategory::Forex), !quotes.empty())) {
        ImGui::TextColored(FC_MUTED, "%s", msg); return;
    }
    render_quote_table("##fx", quotes, "Pair", "Rate", "Chg%", false, 4);
}

void DashboardScreen::widget_commodities(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::Commodities);
    if (auto* msg = loading_text(DashboardData::instance().is_loading(DataCategory::Commodities), !quotes.empty())) {
        ImGui::TextColored(FC_MUTED, "%s", msg); return;
    }
    render_quote_table("##cmd", quotes, "Commodity", "Price", "Chg%", false, 2);
}

void DashboardScreen::widget_crypto(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::Crypto);
    if (auto* msg = loading_text(DashboardData::instance().is_loading(DataCategory::Crypto), !quotes.empty())) {
        ImGui::TextColored(FC_MUTED, "%s", msg); return;
    }
    render_quote_table("##cry", quotes, "Token", "Price", "Chg%", true, 2);
}

void DashboardScreen::widget_sector_heatmap(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::SectorETFs);
    bool is_load = DashboardData::instance().is_loading(DataCategory::SectorETFs);
    if (quotes.empty()) {
        ImGui::TextColored(FC_MUTED, is_load ? "Loading sectors..." : "No sector data");
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 20 || avail.y < 20) return;

    int n = (int)quotes.size();

    // Adaptive grid: prefer wider cells on wide widgets, more rows on tall widgets
    int cols, rows;
    if (avail.x > avail.y * 1.5f) {
        // Wide widget — more columns
        cols = std::min(n, std::max(3, (int)(avail.x / 90.0f)));
        rows = (n + cols - 1) / cols;
    } else {
        // Square or tall — balanced
        cols = std::max(2, (int)std::ceil(std::sqrt((double)n * avail.x / std::max(1.0f, avail.y))));
        cols = std::min(cols, n);
        rows = (n + cols - 1) / cols;
    }

    float gap = 2.0f;
    float cell_w = std::max(40.0f, (avail.x - (cols - 1) * gap) / cols);
    float cell_h = std::max(30.0f, (avail.y - (rows - 1) * gap) / rows);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < n; i++) {
        int r = i / cols, c = i % cols;
        float x = origin.x + c * (cell_w + gap);
        float y = origin.y + r * (cell_h + gap);
        auto& q = quotes[i];

        // Bloomberg-style heatmap colors: deeper saturation
        float pct = (float)q.change_percent;
        float intensity = std::min(std::abs(pct) / 3.5f, 1.0f);
        ImU32 bg;
        if (pct > 0) {
            // Green gradient: dark green → bright green
            int g_val = (int)(40 + 170 * intensity);
            int r_val = (int)(10 * (1.0f - intensity));
            bg = IM_COL32(r_val, g_val, (int)(15 * intensity), 230);
        } else if (pct < 0) {
            // Red gradient: dark red → bright red
            int r_val = (int)(50 + 170 * intensity);
            int g_val = (int)(10 * (1.0f - intensity));
            bg = IM_COL32(r_val, g_val, 0, 230);
        } else {
            bg = IM_COL32(45, 45, 45, 230);
        }

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell_w, y + cell_h), bg, 3.0f);

        // Subtle border
        ImU32 border_col = pct > 0 ? IM_COL32(0, 180, 80, 60) :
                          (pct < 0 ? IM_COL32(180, 40, 40, 60) : IM_COL32(60, 60, 60, 80));
        dl->AddRect(ImVec2(x, y), ImVec2(x + cell_w, y + cell_h), border_col, 3.0f);

        // Compute font size so full label text fits the cell width
        float default_fs = ImGui::GetFontSize();
        float max_text_w = cell_w - 6.0f;

        const char* lbl = q.label.c_str();
        ImVec2 ts_full = ImGui::CalcTextSize(lbl);
        // Scale font down if label is wider than cell, minimum 8px
        float lbl_fs = default_fs;
        if (ts_full.x > max_text_w && ts_full.x > 0) {
            lbl_fs = default_fs * (max_text_w / ts_full.x);
            if (lbl_fs < 8.0f) lbl_fs = 8.0f;
        }
        float lbl_h = lbl_fs;  // approx line height at this font size

        // Change percent text — always at default or slightly smaller
        char val[16];
        std::snprintf(val, sizeof(val), "%+.2f%%", pct);
        ImVec2 vs_full = ImGui::CalcTextSize(val);
        float val_fs = default_fs;
        if (vs_full.x > max_text_w && vs_full.x > 0) {
            val_fs = default_fs * (max_text_w / vs_full.x);
            if (val_fs < 8.0f) val_fs = 8.0f;
        }
        float val_h = val_fs;

        // Price line (optional third line)
        char price_buf[24] = {};
        float price_fs = 0, price_h = 0;
        bool show_price = false;
        if (q.price > 0) {
            std::snprintf(price_buf, sizeof(price_buf), "$%s", fmt_price(q.price).c_str());
            ImVec2 ps_full = ImGui::CalcTextSize(price_buf);
            price_fs = default_fs * 0.85f;
            if (ps_full.x * (price_fs / default_fs) > max_text_w && ps_full.x > 0) {
                price_fs = default_fs * (max_text_w / ps_full.x);
                if (price_fs < 7.0f) price_fs = 7.0f;
            }
            price_h = price_fs;
            // Only show price if cell is tall enough for 3 lines
            show_price = cell_h >= (lbl_h + val_h + price_h + 8.0f);
        }

        // Vertically center all lines
        float gap_between = 2.0f;
        float total_h = lbl_h + gap_between + val_h;
        if (show_price) total_h += gap_between + price_h;
        float text_y = y + std::max(2.0f, (cell_h - total_h) / 2.0f);

        // Draw label — scaled font, centered
        ImFont* font = ImGui::GetFont();
        float lbl_w = ts_full.x * (lbl_fs / default_fs);
        float tx = x + std::max(3.0f, (cell_w - lbl_w) / 2.0f);
        dl->AddText(font, lbl_fs, ImVec2(tx, text_y),
                    IM_COL32(255, 255, 255, 240), lbl);

        // Draw change% — scaled font, centered, colored
        float val_w = vs_full.x * (val_fs / default_fs);
        float vx = x + std::max(3.0f, (cell_w - val_w) / 2.0f);
        ImU32 tc = pct > 0 ? IM_COL32(100, 255, 160, 255) :
                   (pct < 0 ? IM_COL32(255, 120, 120, 255) : IM_COL32(140, 140, 140, 255));
        dl->AddText(font, val_fs, ImVec2(vx, text_y + lbl_h + gap_between), tc, val);

        // Draw price if space allows
        if (show_price) {
            ImVec2 ps_full2 = ImGui::CalcTextSize(price_buf);
            float pw = ps_full2.x * (price_fs / default_fs);
            float px = x + std::max(3.0f, (cell_w - pw) / 2.0f);
            dl->AddText(font, price_fs,
                        ImVec2(px, text_y + lbl_h + gap_between + val_h + gap_between),
                        IM_COL32(180, 180, 180, 180), price_buf);
        }
    }
    ImGui::Dummy(avail);
}

void DashboardScreen::widget_news(float w, float h) {
    auto news = DashboardData::instance().get_news();
    if (news.empty()) { ImGui::TextColored(FC_MUTED, "Loading news..."); return; }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;

    float avail_h = ImGui::GetContentRegionAvail().y;
    if (avail_h < 20) avail_h = 20;

    if (ImGui::BeginTable("##news_tbl", 3, flags, ImVec2(0, avail_h))) {
        ImGui::TableSetupColumn("Time",     ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("Source",   ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Headline", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < news.size(); i++) {
            auto& item = news[i];
            ImGui::TableNextRow();

            // Time column
            ImGui::TableNextColumn();
            ImGui::TextColored(FC_MUTED, "%s", item.time_str.c_str());

            // Source column
            ImGui::TableNextColumn();
            if (item.priority <= 1) {
                ImGui::TextColored(FC_RED, "[!]");
                ImGui::SameLine(0, 2);
            }
            ImGui::TextColored(FC_ORANGE, "%s", item.source.c_str());

            // Headline column — wraps to available width
            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextColored(FC_WHITE, "%s", item.headline.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_economic(float w, float h) {
    if (ImGui::BeginTable("##eco", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Indicator", 0, 2.5f);
        ImGui::TableSetupColumn("Value", 0, 1.0f);
        ImGui::TableSetupColumn("Chg", 0, 1.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto& e : econ_data_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", e.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.2f", e.value);
            ImGui::TableNextColumn(); ImGui::TextColored(chg_col(e.change), "%+.2f", e.change);
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_geopolitics(float w, float h) {
    if (ImGui::BeginTable("##geo", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Region", 0, 1.8f);
        ImGui::TableSetupColumn("Risk", 0, 1.0f);
        ImGui::TableSetupColumn("Trend", 0, 1.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto& g : geo_data_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", g.region.c_str());
            ImGui::TableNextColumn();
            ImVec4 rc = g.risk > 70 ? FC_RED : (g.risk > 50 ? FC_YELLOW : FC_GREEN);
            ImGui::TextColored(rc, "%.1f", g.risk);
            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(g.trend), "%+.1f", g.trend);
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_performance(float w, float h) {
    if (ImGui::BeginTable("##perf", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Metric", 0, 2.0f);
        ImGui::TableSetupColumn("Value", 0, 1.5f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto& p : perf_data_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", p.metric.c_str());
            ImGui::TableNextColumn();
            if (p.value != 0)
                ImGui::TextColored(chg_col(p.change), "$%s (%+.2f%%)", fmt_price(p.value).c_str(), p.change);
            else
                ImGui::TextColored(chg_col(p.change), "%+.2f%%", p.change);
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_top_movers(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::TopMovers);
    if (auto* msg = loading_text(DashboardData::instance().is_loading(DataCategory::TopMovers), !quotes.empty())) {
        ImGui::TextColored(FC_MUTED, "%s", msg); return;
    }
    auto sorted = quotes;
    std::sort(sorted.begin(), sorted.end(), [](const QuoteEntry& a, const QuoteEntry& b) {
        return std::abs(a.change_percent) > std::abs(b.change_percent);
    });
    render_quote_table("##movers", sorted, "Stock", "Price", "Chg%", true, 2);
}

void DashboardScreen::widget_market_data(float w, float h) {
    auto quotes = DashboardData::instance().get_quotes(DataCategory::Indices);
    render_quote_table("##mktdata", quotes, "Symbol", "Price", "Chg%", true, 2);
}

// ============================================================================
// Widget Frame
// ============================================================================
void DashboardScreen::render_widget_frame(const char* title, const ImVec4& accent,
                                           float w, float h, WidgetType type) {
    if (w < 20 || h < 20) return; // skip too-small widgets

    ImGui::PushStyleColor(ImGuiCol_ChildBg, FC_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, FC_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.0f);

    std::string id = std::string("##wf_") + title;
    ImGui::BeginChild(id.c_str(), ImVec2(w, h), ImGuiChildFlags_Borders);

    float hdr_h = 22.0f;
    ImVec2 hdr_start = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Header bg
    dl->AddRectFilled(hdr_start, ImVec2(hdr_start.x + w, hdr_start.y + hdr_h),
        ImGui::ColorConvertFloat4ToU32(FC_HEADER));
    // Accent line
    dl->AddRectFilled(hdr_start, ImVec2(hdr_start.x + w, hdr_start.y + 2),
        ImGui::ColorConvertFloat4ToU32(accent));

    // Title
    ImGui::SetCursorPosX(6);
    ImGui::SetCursorPosY(4);
    ImGui::TextColored(accent, "%s", title);

    // Loading indicator
    DataCategory data_cat = DataCategory::Indices;
    switch (type) {
        case WidgetType::GlobalIndices:  data_cat = DataCategory::Indices; break;
        case WidgetType::Forex:          data_cat = DataCategory::Forex; break;
        case WidgetType::Commodities:    data_cat = DataCategory::Commodities; break;
        case WidgetType::Crypto:         data_cat = DataCategory::Crypto; break;
        case WidgetType::SectorHeatmap:  data_cat = DataCategory::SectorETFs; break;
        case WidgetType::TopMovers:      data_cat = DataCategory::TopMovers; break;
        default: break;
    }
    if (DashboardData::instance().is_loading(data_cat)) {
        ImGui::SameLine(0, 6);
        ImGui::TextColored(FC_YELLOW, "(...)");
    }

    // Close button
    float close_x = w - 18;
    if (close_x > ImGui::GetCursorPosX() + 20) {
        ImGui::SameLine(close_x);
        ImGui::SetCursorPosY(3);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 0, 0, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, FC_MUTED);
        std::string close_id = std::string("x##c_") + title;
        if (ImGui::SmallButton(close_id.c_str())) {
            for (auto& wi : layout_.widgets) {
                if (wi.title == title) { remove_widget(wi.id); break; }
            }
        }
        ImGui::PopStyleColor(3);
    }

    // Content
    ImGui::SetCursorPosY(hdr_h + 2);
    float content_h = h - hdr_h - 4;
    if (content_h < 10) content_h = 10;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::BeginChild((std::string("##wc_") + title).c_str(), ImVec2(w - 2, content_h), false);

    switch (type) {
        case WidgetType::GlobalIndices:      widget_global_indices(w, content_h); break;
        case WidgetType::Forex:              widget_forex(w, content_h); break;
        case WidgetType::Commodities:        widget_commodities(w, content_h); break;
        case WidgetType::Crypto:             widget_crypto(w, content_h); break;
        case WidgetType::SectorHeatmap:      widget_sector_heatmap(w, content_h); break;
        case WidgetType::News:               widget_news(w, content_h); break;
        case WidgetType::EconomicIndicators: widget_economic(w, content_h); break;
        case WidgetType::GeopoliticalRisk:   widget_geopolitics(w, content_h); break;
        case WidgetType::Performance:        widget_performance(w, content_h); break;
        case WidgetType::TopMovers:          widget_top_movers(w, content_h); break;
        case WidgetType::MarketData:         widget_market_data(w, content_h); break;
        default: ImGui::TextColored(FC_MUTED, "Unknown widget"); break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Market Pulse Panel (Right Sidebar) — responsive to width
// ============================================================================
void DashboardScreen::render_market_pulse(float width, float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.04f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, FC_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("##market_pulse", ImVec2(width, height), ImGuiChildFlags_Borders);

    float pad = 8.0f;
    float inner_w = width - pad * 2;

    // Title
    ImGui::SetCursorPos(ImVec2(pad, 4));
    ImGui::TextColored(FC_ORANGE, "MARKET PULSE");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER);
    ImGui::Separator();
    ImGui::PopStyleColor();

    // ---- Fear & Greed Gauge ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "FEAR & GREED");

    auto pulse_quotes = DashboardData::instance().get_quotes(DataCategory::MarketPulse);
    float vix_val = 0;
    for (auto& q : pulse_quotes) {
        if (q.symbol == "^VIX") { vix_val = (float)q.price; break; }
    }

    float fear_greed = vix_val > 0 ?
        std::max(0.0f, std::min(100.0f, 100.0f - (vix_val - 10.0f) * 3.0f)) : 55.0f;

    // Gradient bar
    ImVec2 bar_start = ImGui::GetCursorScreenPos();
    bar_start.x += pad;
    float bar_w = inner_w;
    float bar_h = 6.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)bar_w; i++) {
        float t = (float)i / bar_w;
        int r, g;
        if (t < 0.5f) { r = 255; g = (int)(t * 2 * 255); }
        else { r = (int)((1.0f - (t - 0.5f) * 2) * 255); g = 255; }
        dl->AddRectFilled(
            ImVec2(bar_start.x + i, bar_start.y),
            ImVec2(bar_start.x + i + 1, bar_start.y + bar_h),
            IM_COL32(r, g, 0, 180));
    }

    float ptr_x = bar_start.x + (fear_greed / 100.0f) * bar_w;
    dl->AddTriangleFilled(
        ImVec2(ptr_x - 3, bar_start.y + bar_h + 1),
        ImVec2(ptr_x + 3, bar_start.y + bar_h + 1),
        ImVec2(ptr_x, bar_start.y + bar_h - 2),
        IM_COL32(255, 255, 255, 220));

    ImGui::Dummy(ImVec2(bar_w, bar_h + 6));

    const char* fg_label = fear_greed <= 25 ? "FEAR" : fear_greed <= 50 ? "CAUTIOUS" :
                           fear_greed <= 75 ? "NEUTRAL" : "GREED";
    ImVec4 fg_color = fear_greed <= 25 ? FC_RED : fear_greed <= 50 ? FC_ORANGE :
                      fear_greed <= 75 ? FC_YELLOW : FC_GREEN;

    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(fg_color, "%.0f %s", fear_greed, fg_label);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER); ImGui::Separator(); ImGui::PopStyleColor();

    // ---- Global Snapshot ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "GLOBAL SNAPSHOT");
    ImGui::Spacing();

    if (!pulse_quotes.empty()) {
        // Use a table for proper alignment
        if (ImGui::BeginTable("##pulse_snap", 3, ImGuiTableFlags_SizingStretchProp,
                              ImVec2(inner_w, 0))) {
            ImGui::TableSetupColumn("Name", 0, 1.2f);
            ImGui::TableSetupColumn("Price", 0, 1.0f);
            ImGui::TableSetupColumn("Chg", 0, 0.9f);
            for (auto& q : pulse_quotes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%s", fmt_price(q.price).c_str());
                ImGui::TableNextColumn(); ImGui::TextColored(chg_col(q.change_percent), "%+.2f%%", q.change_percent);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_MUTED, "Loading...");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER); ImGui::Separator(); ImGui::PopStyleColor();

    // ---- Top Movers ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "TOP MOVERS");
    ImGui::Spacing();

    auto movers = DashboardData::instance().get_quotes(DataCategory::TopMovers);
    if (!movers.empty()) {
        auto sorted = movers;
        std::sort(sorted.begin(), sorted.end(), [](const QuoteEntry& a, const QuoteEntry& b) {
            return std::abs(a.change_percent) > std::abs(b.change_percent);
        });
        int show = std::min(6, (int)sorted.size());

        if (ImGui::BeginTable("##pulse_movers", 3, ImGuiTableFlags_SizingStretchProp,
                              ImVec2(inner_w, 0))) {
            ImGui::TableSetupColumn("Dir", 0, 0.8f);
            ImGui::TableSetupColumn("Name", 0, 1.5f);
            ImGui::TableSetupColumn("Price", 0, 1.0f);
            for (int i = 0; i < show; i++) {
                auto& q = sorted[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(chg_col(q.change_percent), "%s%+.1f%%",
                    q.change_percent >= 0 ? "+" : "", q.change_percent);
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_GRAY, "$%s", fmt_price(q.price).c_str());
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_MUTED, "Loading...");
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER); ImGui::Separator(); ImGui::PopStyleColor();

    // ---- Market Breadth ----
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "MARKET BREADTH");
    ImGui::Spacing();

    auto indices = DashboardData::instance().get_quotes(DataCategory::Indices);
    if (!indices.empty()) {
        int advancing = 0, declining = 0;
        for (auto& q : indices) {
            if (q.change_percent > 0) advancing++;
            else if (q.change_percent < 0) declining++;
        }
        int total = (int)indices.size();

        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_GREEN, "Adv: %d", advancing);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(FC_RED, "Dec: %d", declining);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(FC_GRAY, "Unch: %d", total - advancing - declining);

        if (total > 0) {
            float adv_pct = (float)advancing / total;
            ImVec2 p = ImGui::GetCursorScreenPos();
            p.x += pad;
            float bw = inner_w;
            dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + bw * adv_pct, p.y + 5),
                ImGui::ColorConvertFloat4ToU32(FC_GREEN), 2.0f);
            dl->AddRectFilled(ImVec2(p.x + bw * adv_pct, p.y), ImVec2(p.x + bw, p.y + 5),
                ImGui::ColorConvertFloat4ToU32(FC_RED), 2.0f);
            ImGui::Dummy(ImVec2(bw, 8));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Widget Grid — positions widgets using absolute cursor positions
// ============================================================================
void DashboardScreen::render_widget_grid(float width, float height) {
    float gap = 4.0f;

    // Find grid dimensions from layout
    int max_col = 0, max_row = 0;
    for (auto& w : layout_.widgets) {
        if (!w.visible) continue;
        max_col = std::max(max_col, w.col + w.col_span);
        max_row = std::max(max_row, w.row + w.row_span);
    }
    if (max_col == 0) max_col = layout_.grid_cols;
    if (max_row == 0) max_row = layout_.grid_rows;

    float cell_w = (width - (max_col + 1) * gap) / max_col;
    float cell_h = (height - (max_row + 1) * gap) / max_row;

    // Ensure minimum sizes
    cell_w = std::max(100.0f, cell_w);
    cell_h = std::max(60.0f, cell_h);

    // Allow scrolling if content overflows
    bool needs_scroll = (max_row * (cell_h + gap) + gap) > height;
    ImGuiWindowFlags flags = needs_scroll ? 0 : ImGuiWindowFlags_NoScrollbar;

    ImGui::BeginChild("##grid", ImVec2(width, height), false, flags);

    for (auto& w : layout_.widgets) {
        if (!w.visible) continue;

        float wx = gap + w.col * (cell_w + gap);
        float wy = gap + w.row * (cell_h + gap);
        float ww = w.col_span * cell_w + (w.col_span - 1) * gap;
        float wh = w.row_span * cell_h + (w.row_span - 1) * gap;

        ImGui::SetCursorPos(ImVec2(wx, wy));
        render_widget_frame(w.title.c_str(), accent_for(w.type), ww, wh, w.type);
    }

    ImGui::EndChild();
}

// ============================================================================
// Add Widget Modal
// ============================================================================
void DashboardScreen::render_add_widget_modal() {
    if (!show_add_widget_modal_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, FC_HEADER);

    if (ImGui::Begin("Add Widget##modal", &show_add_widget_modal_,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {

        ImGui::TextColored(FC_ORANGE, "Select widget to add:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(WidgetType::Count); i++) {
            auto type = static_cast<WidgetType>(i);
            ImVec4 accent = accent_for(type);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, accent);

            if (ImGui::Button(widget_type_name(type), ImVec2(-1, 26))) {
                add_widget(type);
                show_add_widget_modal_ = false;
            }
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Template Picker Modal
// ============================================================================
void DashboardScreen::render_template_picker_modal() {
    if (!show_template_picker_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 340), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, FC_HEADER);

    if (ImGui::Begin("Dashboard Templates##modal", &show_template_picker_,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {

        ImGui::TextColored(FC_ORANGE, "Choose a layout:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (auto& t : get_all_templates()) {
            bool is_current = (layout_.template_id == t.id);
            ImGui::PushStyleColor(ImGuiCol_Button,
                is_current ? ImVec4(0.15f, 0.08f, 0.0f, 1.0f) : ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.12f, 0.0f, 1.0f));

            std::string label = std::string("[") + t.tag + "] " + t.name;
            if (is_current) label += "  (ACTIVE)";

            if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
                apply_template(t.id);
                show_template_picker_ = false;
            }
            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_Text, FC_GRAY);
            ImGui::TextWrapped("  %s", t.description.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Status Bar
// ============================================================================
void DashboardScreen::render_status_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, FC_HEADER);
    ImGui::BeginChild("##dash_status", ImVec2(width, 20), false);

    ImGui::SetCursorPos(ImVec2(8, 3));

    ImGui::TextColored(FC_ORANGE, "v3.3.1");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_GRAY, "LAYOUT:");
    ImGui::SameLine(0, 3);
    ImGui::TextColored(FC_GREEN, "%s", layout_.template_id.c_str());
    ImGui::SameLine(0, 8);
    ImGui::TextColored(FC_BORDER, "|");
    ImGui::SameLine(0, 8);

    // Feed indicators
    struct FeedInfo { const char* label; DataCategory cat; };
    FeedInfo feeds[] = {
        {"EQ", DataCategory::Indices}, {"FX", DataCategory::Forex},
        {"CM", DataCategory::Commodities}, {"CR", DataCategory::Crypto},
        {"SC", DataCategory::SectorETFs}
    };
    for (int i = 0; i < 5; i++) {
        bool has = DashboardData::instance().has_data(feeds[i].cat);
        bool ld = DashboardData::instance().is_loading(feeds[i].cat);
        ImVec4 col = has ? FC_GREEN : (ld ? FC_YELLOW : FC_RED);
        ImGui::TextColored(col, "[*]%s", feeds[i].label);
        if (i < 4) ImGui::SameLine(0, 3);
    }

    // Right: status
    float right_x = width - 60;
    float cur = ImGui::GetCursorPosX();
    if (right_x > cur + 10) {
        ImGui::SameLine(right_x);
        bool ok = DashboardData::instance().has_data(DataCategory::Indices);
        ImGui::TextColored(ok ? FC_GREEN : FC_YELLOW, ok ? "READY" : "LOADING");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// MAIN RENDER
// ============================================================================
void DashboardScreen::render() {
    init();

    DashboardData::instance().update(ImGui::GetIO().DeltaTime);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float work_y = vp->WorkPos.y + tab_bar_h;
    float work_h = vp->WorkSize.y - tab_bar_h - footer_h;
    float work_w = vp->WorkSize.x;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, work_y));
    ImGui::SetNextWindowSize(ImVec2(work_w, work_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, FC_DARK);

    ImGui::Begin("##dash", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    float content_w = ImGui::GetContentRegionAvail().x;

    // Fixed heights
    float header_h = 30;
    float accent_h = 2;
    float fkey_h = 0; // F-key bar removed
    float ticker_h = 20;
    float status_h = 0; // status bar removed — app-level footer handles session/API info

    // Header bar (with Add Widget, Template, Refresh, Pulse toggle)
    render_header_bar(content_w);

    // Orange accent line
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + content_w, p.y + accent_h),
            ImGui::ColorConvertFloat4ToU32(FC_ORANGE));
        ImGui::Dummy(ImVec2(content_w, accent_h));
    }

    // Ticker bar
    render_ticker_bar(content_w);

    // Main area: widget grid + market pulse
    float grid_area_h = ImGui::GetContentRegionAvail().y - status_h;
    if (grid_area_h < 100) grid_area_h = 100;

    float pulse_w = layout_.market_pulse_open ? std::max(280.0f, std::min(340.0f, content_w * 0.25f)) : 0.0f;
    float grid_w = content_w - pulse_w;

    // Use a table layout for proper side-by-side
    if (ImGui::BeginTable("##main_layout", layout_.market_pulse_open ? 2 : 1,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX,
                          ImVec2(content_w, grid_area_h))) {
        if (layout_.market_pulse_open) {
            ImGui::TableSetupColumn("grid", ImGuiTableColumnFlags_WidthFixed, grid_w);
            ImGui::TableSetupColumn("pulse", ImGuiTableColumnFlags_WidthFixed, pulse_w);
        } else {
            ImGui::TableSetupColumn("grid", ImGuiTableColumnFlags_WidthFixed, content_w);
        }

        ImGui::TableNextRow(0, grid_area_h);

        ImGui::TableNextColumn();
        render_widget_grid(grid_w, grid_area_h);

        if (layout_.market_pulse_open) {
            ImGui::TableNextColumn();
            render_market_pulse(pulse_w, grid_area_h);
        }

        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Modals (rendered outside main window)
    render_add_widget_modal();
    render_template_picker_modal();
}

} // namespace fincept::dashboard
