#include "dashboard_screen.h"
#include "dashboard_helpers.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <ctime>

namespace fincept::dashboard {

// ============================================================================
// Helper: get market open/close status based on UTC time
// Matches original Tauri MarketPulsePanel getMarketStatus()
// ============================================================================
static const char* get_market_status(const char* region) {
    // Cache UTC time — only recompute once per second
    static time_t cached_mkt_time = 0;
    static int cached_h = 0, cached_wd = 0;
    time_t now = time(nullptr);
    if (now != cached_mkt_time) {
        cached_mkt_time = now;
        struct tm utc_tm;
#ifdef _WIN32
        gmtime_s(&utc_tm, &now);
#else
        gmtime_r(&now, &utc_tm);
#endif
        cached_h = utc_tm.tm_hour;
        cached_wd = utc_tm.tm_wday;
    }
    int h = cached_h;
    int wd = cached_wd; // 0=Sun, 6=Sat

    // Weekend — all closed
    if (wd == 0 || wd == 6) return "CLOSED";

    if (strcmp(region, "US") == 0) {
        if (h >= 13 && h < 14) return "PRE";
        if (h >= 14 && h < 21) return "OPEN";
        return "CLOSED";
    }
    if (strcmp(region, "UK") == 0) {
        if (h >= 7 && h < 8) return "PRE";
        if (h >= 8 && h < 17) return "OPEN";
        return "CLOSED";
    }
    if (strcmp(region, "JP") == 0) {
        if (h >= 0 && h < 6) return "OPEN";
        return "CLOSED";
    }
    if (strcmp(region, "CN") == 0) {
        if (h >= 1 && h < 7) return "OPEN";
        return "CLOSED";
    }
    if (strcmp(region, "IN") == 0) {
        if (h >= 3 && h < 10) return "OPEN";
        return "CLOSED";
    }
    return "CLOSED";
}

// Helper: draw a section header bar matching the original SectionHeader component
static void section_header(float pad, float inner_w, const char* title, ImVec4 icon_col) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.102f, 0.102f, 0.102f, 1.0f));
    ImGui::BeginChild(title, ImVec2(inner_w + pad * 2, 20), false);
    ImGui::SetCursorPos(ImVec2(pad, 3));
    ImGui::TextColored(icon_col, "*");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.47f, 1.0f), "%s", title);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Helper: draw a breadth bar (label, advancing/declining counts, colored bar)
static void breadth_bar(float pad, float inner_w, const char* label, int advancing, int declining) {
    ImGui::SetCursorPosX(pad);

    // Label and counts on same line
    ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.47f, 1.0f), "%s", label);
    ImGui::SameLine(inner_w - 20);
    ImGui::TextColored(ImVec4(0.0f, 0.84f, 0.435f, 1.0f), "%d", advancing);
    ImGui::SameLine(0, 2);
    ImGui::TextColored(ImVec4(0.29f, 0.29f, 0.29f, 1.0f), "/");
    ImGui::SameLine(0, 2);
    ImGui::TextColored(ImVec4(1.0f, 0.231f, 0.231f, 1.0f), "%d", declining);

    // Bar
    int total = advancing + declining;
    float adv_pct = total > 0 ? (float)advancing / total : 0.5f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.x += pad;
    float bw = inner_w;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + bw * adv_pct, p.y + 4),
        IM_COL32(0, 214, 111, 255), 2.0f);
    dl->AddRectFilled(ImVec2(p.x + bw * adv_pct, p.y), ImVec2(p.x + bw, p.y + 4),
        IM_COL32(255, 59, 59, 255), 2.0f);
    ImGui::Dummy(ImVec2(bw, 8));
}

// ============================================================================
// Market Pulse Panel (Right Sidebar)
// Matches original Tauri MarketPulsePanel.tsx:
//   Fear & Greed → Market Breadth → Top Gainers → Top Losers
//   → Global Snapshot → Market Hours
// ============================================================================
void DashboardScreen::render_market_pulse(float width, float height) {
    float pad = 8.0f;
    float inner_w = ImGui::GetContentRegionAvail().x - pad * 2;

    // ── Panel Header with live pulse dot ──
    ImGui::SetCursorPos(ImVec2(pad, 4));
    ImGui::TextColored(FC_ORANGE, "*"); ImGui::SameLine(0, 4);
    ImGui::TextColored(FC_ORANGE, "MARKET PULSE");

    // Pulsing green dot (right side)
    {
        float dot_x = width - pad - 6;
        float dot_y = ImGui::GetCursorScreenPos().y - 10;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 sp = ImGui::GetWindowPos();
        float pulse = (float)(0.5 + 0.5 * sin(ImGui::GetTime() * 3.14159));
        dl->AddCircleFilled(ImVec2(sp.x + dot_x, dot_y),
            3.0f, IM_COL32(0, 214, 111, (int)(120 + 135 * pulse)));
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 0.533f, 0.0f, 0.25f));
    ImGui::Separator();
    ImGui::PopStyleColor();

    // ════════════════════════════════════════════════════════════════════════
    // 1. FEAR & GREED INDEX (VIX-derived, 5-level, matches original)
    // ════════════════════════════════════════════════════════════════════════
    ImGui::Spacing();
    ImGui::SetCursorPosX(pad);
    ImGui::TextColored(FC_GRAY, "FEAR & GREED INDEX");
    ImGui::SameLine(inner_w);
    ImGui::TextColored(FC_ORANGE, "*"); // gauge icon

    // Assemble global snapshot from MarketPulse (VIX, TNX, DXY) + Commodities (Gold, Oil) + Crypto (BTC)
    auto pulse_quotes = DashboardData::instance().get_quotes(DataCategory::MarketPulse);
    for (auto& q : DashboardData::instance().get_quotes(DataCategory::Commodities)) {
        if (q.symbol == "GC=F" || q.symbol == "CL=F") pulse_quotes.push_back(q);
    }
    for (auto& q : DashboardData::instance().get_quotes(DataCategory::Crypto)) {
        if (q.symbol == "BTC-USD") pulse_quotes.push_back(q);
    }
    float vix_val = 0;
    for (auto& q : pulse_quotes) {
        if (q.symbol == "^VIX") { vix_val = (float)q.price; break; }
    }

    // VIX → Fear/Greed mapping (matches original exactly)
    float fear_greed = 50.0f;
    if (vix_val > 0) {
        if (vix_val <= 12.0f)      fear_greed = 95.0f;
        else if (vix_val <= 15.0f) fear_greed = 80.0f + (15.0f - vix_val) * 5.0f;
        else if (vix_val <= 20.0f) fear_greed = 60.0f + (20.0f - vix_val) * 4.0f;
        else if (vix_val <= 25.0f) fear_greed = 40.0f + (25.0f - vix_val) * 4.0f;
        else if (vix_val <= 35.0f) fear_greed = 20.0f + (35.0f - vix_val) * 2.0f;
        else                        fear_greed = std::max(5.0f, 20.0f - (vix_val - 35.0f));
        fear_greed = std::round(fear_greed);
    }

    // Gradient bar: Red → Orange → Yellow → LightGreen → Green
    ImVec2 bar_start = ImGui::GetCursorScreenPos();
    bar_start.x += pad;
    float bar_w = inner_w;
    float bar_h = 6.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)bar_w; i++) {
        float t = (float)i / bar_w;
        int r, g, b = 0;
        if (t < 0.25f) {
            // Red → Orange
            float s = t / 0.25f;
            r = 255; g = (int)(102 * s); b = (int)(68 * (1.0f - s));
        } else if (t < 0.5f) {
            // Orange → Yellow
            float s = (t - 0.25f) / 0.25f;
            r = 255; g = 102 + (int)(153 * s); b = 0;
        } else if (t < 0.75f) {
            // Yellow → Light green
            float s = (t - 0.5f) / 0.25f;
            r = 255 - (int)(119 * s); g = 255 - (int)(51 * s); b = 0 + (int)(68 * s);
        } else {
            // Light green → Green
            float s = (t - 0.75f) / 0.25f;
            r = 136 - (int)(136 * s); g = 204 + (int)(10 * s); b = 68 + (int)(43 * s);
        }
        dl->AddRectFilled(
            ImVec2(bar_start.x + i, bar_start.y),
            ImVec2(bar_start.x + i + 1, bar_start.y + bar_h),
            IM_COL32(r, g, b, 220));
    }

    // Pointer circle on gauge
    float ptr_x = bar_start.x + (fear_greed / 100.0f) * bar_w;

    // 5-level label & color (matches original)
    const char* fg_label; ImVec4 fg_color;
    if (fear_greed <= 20.0f)      { fg_label = "EXTREME FEAR";  fg_color = FC_RED; }
    else if (fear_greed <= 40.0f) { fg_label = "FEAR";          fg_color = ImVec4(1.0f, 0.4f, 0.267f, 1.0f); }
    else if (fear_greed <= 60.0f) { fg_label = "NEUTRAL";       fg_color = FC_YELLOW; }
    else if (fear_greed <= 80.0f) { fg_label = "GREED";         fg_color = ImVec4(0.533f, 0.8f, 0.267f, 1.0f); }
    else                          { fg_label = "EXTREME GREED"; fg_color = FC_GREEN; }

    // Draw pointer with glow
    ImU32 ptr_col = ImGui::ColorConvertFloat4ToU32(fg_color);
    dl->AddCircleFilled(ImVec2(ptr_x, bar_start.y + bar_h / 2), 6.0f, IM_COL32(255, 255, 255, 240));
    dl->AddCircle(ImVec2(ptr_x, bar_start.y + bar_h / 2), 6.0f, ptr_col, 0, 2.0f);

    ImGui::Dummy(ImVec2(bar_w, bar_h + 8));

    // Value and label
    ImGui::SetCursorPosX(pad);
    ImGui::PushStyleColor(ImGuiCol_Text, fg_color);
    ImGui::Text("%.0f", fear_greed);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);
    ImGui::TextColored(FC_MUTED, "/100");

    // Right-align the label
    {
        ImVec2 label_size = ImGui::CalcTextSize(fg_label);
        float label_x = pad + inner_w - label_size.x;
        ImGui::SameLine(label_x);
        ImGui::TextColored(fg_color, "%s", fg_label);
    }

    ImGui::Spacing();

    // ════════════════════════════════════════════════════════════════════════
    // 2. MARKET BREADTH (NYSE / NASDAQ / S&P 500 — derived from movers)
    // ════════════════════════════════════════════════════════════════════════
    section_header(pad, inner_w, "MARKET BREADTH", FC_CYAN);
    ImGui::Spacing();

    auto movers = DashboardData::instance().get_quotes(DataCategory::TopMovers);
    if (!movers.empty()) {
        int adv = 0, dec = 0;
        for (auto& q : movers) {
            if (q.change_percent > 0) adv++;
            else if (q.change_percent < 0) dec++;
        }
        int total = (int)movers.size();
        if (total == 0) total = 1;

        // Scale to approximate real market breadth (like original)
        int nyse_adv = (int)((float)adv / total * 3000) + (int)(fmod(ImGui::GetTime() * 7.3, 50));
        int nyse_dec = (int)((float)dec / total * 3000) + (int)(fmod(ImGui::GetTime() * 3.1, 50));
        int nas_adv  = (int)((float)adv / total * 3500) + (int)(fmod(ImGui::GetTime() * 5.7, 50));
        int nas_dec  = (int)((float)dec / total * 3500) + (int)(fmod(ImGui::GetTime() * 2.3, 50));
        int sp_adv   = (int)((float)adv / total * 500);
        int sp_dec   = (int)((float)dec / total * 500);

        breadth_bar(pad, inner_w, "NYSE",    nyse_adv, nyse_dec);
        breadth_bar(pad, inner_w, "NASDAQ",  nas_adv,  nas_dec);
        breadth_bar(pad, inner_w, "S&P 500", sp_adv,   sp_dec);
    } else {
        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_MUTED, "Loading...");
    }

    ImGui::Spacing();

    // ════════════════════════════════════════════════════════════════════════
    // 3. TOP GAINERS
    // ════════════════════════════════════════════════════════════════════════
    section_header(pad, inner_w, "TOP GAINERS", FC_GREEN);

    if (!movers.empty()) {
        auto sorted = movers;
        std::sort(sorted.begin(), sorted.end(), [](const QuoteEntry& a, const QuoteEntry& b) {
            return a.change_percent > b.change_percent;
        });

        int shown = 0;
        if (ImGui::BeginTable("##pulse_gainers", 3, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Sym", 0, 1.0f);
            ImGui::TableSetupColumn("Vol", 0, 0.8f);
            ImGui::TableSetupColumn("Chg", 0, 0.8f);
            for (auto& q : sorted) {
                if (q.change_percent <= 0) break;
                if (shown >= 3) break;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_WHITE, "%s", q.symbol.c_str());
                ImGui::TextColored(FC_MUTED, "%s", q.label.c_str());
                ImGui::TableNextColumn();
                if (q.volume > 0)
                    ImGui::TextColored(FC_MUTED, "VOL: %s", fmt_volume(q.volume).c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_GREEN, "+%.2f%%", q.change_percent);
                shown++;
            }
            ImGui::EndTable();
        }
        if (shown == 0) {
            ImGui::SetCursorPosX(pad);
            ImGui::TextColored(FC_MUTED, "No gainers data");
        }
    } else {
        // Skeleton loading
        for (int i = 0; i < 3; i++) {
            ImGui::SetCursorPosX(pad);
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(sp, ImVec2(sp.x + 40, sp.y + 10), IM_COL32(42, 42, 42, 255), 2.0f);
            dl->AddRectFilled(ImVec2(sp.x + inner_w - 50, sp.y), ImVec2(sp.x + inner_w, sp.y + 10), IM_COL32(42, 42, 42, 255), 2.0f);
            ImGui::Dummy(ImVec2(inner_w, 16));
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // 4. TOP LOSERS
    // ════════════════════════════════════════════════════════════════════════
    section_header(pad, inner_w, "TOP LOSERS", FC_RED);

    if (!movers.empty()) {
        auto sorted = movers;
        std::sort(sorted.begin(), sorted.end(), [](const QuoteEntry& a, const QuoteEntry& b) {
            return a.change_percent < b.change_percent;
        });

        int shown = 0;
        if (ImGui::BeginTable("##pulse_losers", 3, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Sym", 0, 1.0f);
            ImGui::TableSetupColumn("Vol", 0, 0.8f);
            ImGui::TableSetupColumn("Chg", 0, 0.8f);
            for (auto& q : sorted) {
                if (q.change_percent >= 0) break;
                if (shown >= 3) break;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_WHITE, "%s", q.symbol.c_str());
                ImGui::TextColored(FC_MUTED, "%s", q.label.c_str());
                ImGui::TableNextColumn();
                if (q.volume > 0)
                    ImGui::TextColored(FC_MUTED, "VOL: %s", fmt_volume(q.volume).c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_RED, "%.2f%%", q.change_percent);
                shown++;
            }
            ImGui::EndTable();
        }
        if (shown == 0) {
            ImGui::SetCursorPosX(pad);
            ImGui::TextColored(FC_MUTED, "No losers data");
        }
    } else {
        for (int i = 0; i < 3; i++) {
            ImGui::SetCursorPosX(pad);
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(sp, ImVec2(sp.x + 40, sp.y + 10), IM_COL32(42, 42, 42, 255), 2.0f);
            dl->AddRectFilled(ImVec2(sp.x + inner_w - 50, sp.y), ImVec2(sp.x + inner_w, sp.y + 10), IM_COL32(42, 42, 42, 255), 2.0f);
            ImGui::Dummy(ImVec2(inner_w, 16));
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // 5. GLOBAL SNAPSHOT (formatted like original: VIX, 10Y, DXY, GOLD, OIL, BTC)
    // ════════════════════════════════════════════════════════════════════════
    section_header(pad, inner_w, "GLOBAL SNAPSHOT", FC_BLUE);

    struct SnapItem {
        const char* label;
        const char* symbol;
        ImVec4 color;
        bool is_yield;   // TNX special handling
        bool is_btc;     // BTC formatting
        bool is_gold;    // no decimals
    };
    SnapItem snaps[] = {
        {"VIX",     "^VIX",     FC_YELLOW, false, false, false},
        {"US 10Y",  "^TNX",     FC_CYAN,   true,  false, false},
        {"DXY",     "DX-Y.NYB", FC_CYAN,   false, false, false},
        {"GOLD",    "GC=F",     FC_YELLOW, false, false, true},
        {"OIL WTI", "CL=F",     FC_CYAN,   false, false, false},
        {"BTC",     "BTC-USD",  FC_ORANGE, false, true,  false},
    };

    bool has_global = !pulse_quotes.empty();
    for (auto& s : snaps) {
        ImGui::SetCursorPosX(pad);

        // Label
        ImGui::TextColored(FC_GRAY, "%s", s.label);

        // Value + change (right aligned)
        if (!has_global) {
            // Loading skeleton
            ImVec2 sp = ImGui::GetCursorScreenPos();
            float rx = sp.x + inner_w - 60;
            dl->AddRectFilled(ImVec2(rx, sp.y - 14), ImVec2(rx + 60, sp.y - 4), IM_COL32(42, 42, 42, 255), 2.0f);
        } else {
            // Find the quote
            const QuoteEntry* found = nullptr;
            for (auto& q : pulse_quotes) {
                if (q.symbol == s.symbol) { found = &q; break; }
            }

            char val_buf[32] = "--";
            char chg_buf[32] = "";
            if (found && found->price > 0) {
                if (s.is_yield) {
                    std::snprintf(val_buf, sizeof(val_buf), "%.2f%%", found->price / 10.0);
                } else if (s.is_btc) {
                    std::snprintf(val_buf, sizeof(val_buf), "$%.1fK", found->price / 1000.0);
                } else if (s.is_gold) {
                    std::snprintf(val_buf, sizeof(val_buf), "$%.0f", found->price);
                } else {
                    std::snprintf(val_buf, sizeof(val_buf), "%.2f", found->price);
                }
                std::snprintf(chg_buf, sizeof(chg_buf), "%+.2f%%", found->change_percent);
            }

            // Right-align value
            ImVec2 val_size = ImGui::CalcTextSize(val_buf);
            ImVec2 chg_size = ImGui::CalcTextSize(chg_buf);
            float total_w = val_size.x + (chg_buf[0] ? chg_size.x + 6 : 0);
            float rx = pad + inner_w - total_w;

            ImGui::SameLine(rx);
            ImGui::TextColored(s.color, "%s", val_buf);
            if (chg_buf[0]) {
                ImGui::SameLine(0, 6);
                ImVec4 cc = (chg_buf[0] == '+') ? FC_GREEN :
                            (chg_buf[0] == '-') ? FC_RED : FC_MUTED;
                ImGui::TextColored(cc, "%s", chg_buf);
            }
        }

        // Separator line
        ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // ════════════════════════════════════════════════════════════════════════
    // 6. MARKET HOURS (matches original: NYSE, LSE, TSE, SSE, NSE)
    // ════════════════════════════════════════════════════════════════════════
    section_header(pad, inner_w, "MARKET HOURS", FC_YELLOW);
    ImGui::Spacing();

    struct ExchangeInfo {
        const char* name;
        const char* region;
        ImVec4 accent;
    };
    ExchangeInfo exchanges[] = {
        {"NYSE/NASDAQ",   "US", FC_GREEN},
        {"LSE",           "UK", FC_BLUE},
        {"TSE (TOKYO)",   "JP", FC_PURPLE},
        {"SSE (SHANGHAI)","CN", FC_RED},
        {"NSE (INDIA)",   "IN", FC_ORANGE},
    };

    for (auto& ex : exchanges) {
        const char* status = get_market_status(ex.region);
        ImVec4 status_col;
        if (strcmp(status, "OPEN") == 0)       status_col = FC_GREEN;
        else if (strcmp(status, "PRE") == 0)    status_col = FC_YELLOW;
        else                                    status_col = FC_RED;

        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(FC_GRAY, "%s", ex.name);

        // Right-align status with dot
        {
            ImVec2 st_size = ImGui::CalcTextSize(status);
            float rx = pad + inner_w - st_size.x - 10;
            ImGui::SameLine(rx);

            // Status dot
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddCircleFilled(ImVec2(sp.x, sp.y + 6), 2.5f,
                ImGui::ColorConvertFloat4ToU32(status_col));
            ImGui::Dummy(ImVec2(8, 0));
            ImGui::SameLine(0, 0);
            ImGui::TextColored(status_col, "%s", status);
        }

        ImGui::PushStyleColor(ImGuiCol_Separator, FC_BORDER);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

}

// ============================================================================
// Widget Grid — scrollable, drag-to-rearrange, resizable widgets
// ============================================================================

// Check if placing a widget at (col, row) with (cs, rs) span would overlap
// any other widget. skip_id is the widget being moved/resized (excluded).
static bool would_overlap(const std::vector<WidgetInstance>& widgets,
                          const std::string& skip_id,
                          int col, int row, int cs, int rs) {
    for (const auto& other : widgets) {
        if (!other.visible || other.id == skip_id) continue;
        // Axis-aligned rect overlap test
        bool h_overlap = col < other.col + other.col_span && col + cs > other.col;
        bool v_overlap = row < other.row + other.row_span && row + rs > other.row;
        if (h_overlap && v_overlap) return true;
    }
    return false;
}

// Find the maximum col_span a widget can grow to without overlapping others
// or exceeding grid bounds.
static int max_col_span_for(const std::vector<WidgetInstance>& widgets,
                            const std::string& wid, int col, int row,
                            int row_span, int grid_cols) {
    int max_cs = grid_cols - col;
    for (const auto& other : widgets) {
        if (!other.visible || other.id == wid) continue;
        bool v_overlap = row < other.row + other.row_span && row + row_span > other.row;
        if (v_overlap && other.col >= col) {
            max_cs = std::min(max_cs, other.col - col);
        }
    }
    return std::max(1, max_cs);
}

// Find the maximum row_span a widget can grow to without overlapping others
// or exceeding grid bounds.
static int max_row_span_for(const std::vector<WidgetInstance>& widgets,
                            const std::string& wid, int col, int row,
                            int col_span, int grid_rows) {
    int max_rs = grid_rows - row;
    for (const auto& other : widgets) {
        if (!other.visible || other.id == wid) continue;
        bool h_overlap = col < other.col + other.col_span && col + col_span > other.col;
        if (h_overlap && other.row >= row) {
            max_rs = std::min(max_rs, other.row - row);
        }
    }
    return std::max(1, max_rs);
}

void DashboardScreen::render_widget_grid(float width, float height) {
    float gap = 4.0f;
    grid_gap_ = gap;

    const int grid_cols = layout_.grid_cols;
    const int grid_rows = layout_.grid_rows;

    int max_col = 0, max_row = 0;
    for (auto& w : layout_.widgets) {
        if (!w.visible) continue;
        max_col = std::max(max_col, w.col + w.col_span);
        max_row = std::max(max_row, w.row + w.row_span);
    }
    if (max_col == 0) max_col = grid_cols;
    if (max_row == 0) max_row = grid_rows;
    // Ensure computed max doesn't exceed configured grid
    max_col = std::max(max_col, grid_cols);
    max_row = std::max(max_row, grid_rows);

    float cell_w = (width - (max_col + 1) * gap) / max_col;
    cell_w = std::max(100.0f, cell_w);

    // Fixed cell height based on layout_.grid_rows (the *configured* rows, not actual).
    // When user adds more widgets beyond grid_rows, extra rows scroll — they don't squish.
    float cell_h = (height - (layout_.grid_rows + 1) * gap) / layout_.grid_rows;
    cell_h = std::max(180.0f, cell_h);

    grid_cell_w_ = cell_w;
    grid_cell_h_ = cell_h;

    // Content height based on actual rows used — may exceed viewport → scrollbar
    float content_h = max_row * (cell_h + gap) + gap;
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysVerticalScrollbar;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, FC_PANEL);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.03f, 0.03f, 0.03f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(1.0f, 0.533f, 0.0f, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(1.0f, 0.533f, 0.0f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(1.0f, 0.533f, 0.0f, 0.75f));
    ImGui::BeginChild("##grid", ImVec2(width, height), false, flags);

    // Reserve content space so scroll range is correct
    ImGui::Dummy(ImVec2(width - 16, content_h));
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImVec2 grid_origin = ImGui::GetCursorScreenPos();
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    bool mouse_down = ImGui::GetIO().MouseDown[0];
    bool mouse_released = ImGui::IsMouseReleased(0);

    // --- Handle drag/resize: snap to grid on release, smooth pixel offset while dragging ---
    // Track pixel offsets for smooth visual feedback during drag/resize
    float drag_pixel_dx = 0, drag_pixel_dy = 0;
    float resize_pixel_dw = 0, resize_pixel_dh = 0;

    if (mouse_released) {
        if (!dragging_widget_id_.empty()) {
            // Snap final position to grid on release
            ImVec2 delta = ImVec2(mouse_pos.x - drag_start_mouse_.x,
                                  mouse_pos.y - drag_start_mouse_.y);
            int col_delta = (int)std::round(delta.x / (cell_w + gap));
            int row_delta = (int)std::round(delta.y / (cell_h + gap));

            // Find the widget being dragged
            for (auto& w : layout_.widgets) {
                if (w.id == dragging_widget_id_) {
                    int new_col = drag_start_col_ + col_delta;
                    int new_row = drag_start_row_ + row_delta;
                    // Clamp col to grid, row to actual used rows (not configured rows)
                    new_col = std::clamp(new_col, 0, grid_cols - w.col_span);
                    new_row = std::max(0, new_row);
                    // Only apply if no overlap at new position
                    if (!would_overlap(layout_.widgets, w.id,
                                       new_col, new_row, w.col_span, w.row_span)) {
                        w.col = new_col;
                        w.row = new_row;
                    }
                    break;
                }
            }
            save_layout();
            dragging_widget_id_.clear();
        }
        if (!resizing_widget_id_.empty()) {
            // Snap final span to grid on release
            ImVec2 delta = ImVec2(mouse_pos.x - drag_start_mouse_.x,
                                  mouse_pos.y - drag_start_mouse_.y);
            int cs_delta = (int)std::round(delta.x / (cell_w + gap));
            int rs_delta = (int)std::round(delta.y / (cell_h + gap));

            for (auto& w : layout_.widgets) {
                if (w.id == resizing_widget_id_) {
                    // Clamp col_span to grid boundary, row_span unbounded downward
                    int new_cs = std::clamp(resize_start_col_span_ + cs_delta,
                                            1, grid_cols - w.col);
                    int new_rs = std::max(1, resize_start_row_span_ + rs_delta);
                    // Further clamp to prevent overlap with other widgets
                    int limit_cs = max_col_span_for(layout_.widgets, w.id,
                                                     w.col, w.row, new_rs, grid_cols);
                    new_cs = std::min(new_cs, limit_cs);
                    int limit_rs = max_row_span_for(layout_.widgets, w.id,
                                                     w.col, w.row, new_cs, grid_rows);
                    new_rs = std::min(new_rs, limit_rs);

                    w.col_span = new_cs;
                    w.row_span = new_rs;
                    break;
                }
            }
            save_layout();
            resizing_widget_id_.clear();
        }
    }

    // --- Compute smooth pixel offsets while dragging (don't modify grid pos yet) ---
    if (!dragging_widget_id_.empty() && mouse_down) {
        drag_pixel_dx = mouse_pos.x - drag_start_mouse_.x;
        drag_pixel_dy = mouse_pos.y - drag_start_mouse_.y;
    }

    // --- Compute smooth pixel offsets while resizing (don't modify span yet) ---
    // Clamp visual resize to grid boundaries and overlap limits
    if (!resizing_widget_id_.empty() && mouse_down) {
        resize_pixel_dw = mouse_pos.x - drag_start_mouse_.x;
        resize_pixel_dh = mouse_pos.y - drag_start_mouse_.y;

        // Find the widget being resized and compute clamped visual size
        for (const auto& w : layout_.widgets) {
            if (w.id == resizing_widget_id_) {
                // Compute what the snapped span would be
                int preview_cs = std::clamp(
                    resize_start_col_span_ + (int)std::round(resize_pixel_dw / (cell_w + gap)),
                    1, grid_cols - w.col);
                int preview_rs = std::clamp(
                    resize_start_row_span_ + (int)std::round(resize_pixel_dh / (cell_h + gap)),
                    1, grid_rows - w.row);
                // Clamp to overlap limits
                int limit_cs = max_col_span_for(layout_.widgets, w.id,
                                                 w.col, w.row, preview_rs, grid_cols);
                preview_cs = std::min(preview_cs, limit_cs);
                int limit_rs = max_row_span_for(layout_.widgets, w.id,
                                                 w.col, w.row, preview_cs, grid_rows);
                preview_rs = std::min(preview_rs, limit_rs);
                // Convert clamped span back to pixel delta
                float max_w = preview_cs * cell_w + (preview_cs - 1) * gap;
                float max_h = preview_rs * cell_h + (preview_rs - 1) * gap;
                float base_w = w.col_span * cell_w + (w.col_span - 1) * gap;
                float base_h = w.row_span * cell_h + (w.row_span - 1) * gap;
                resize_pixel_dw = std::clamp(resize_pixel_dw,
                    cell_w - base_w, max_w - base_w);
                resize_pixel_dh = std::clamp(resize_pixel_dh,
                    cell_h - base_h, max_h - base_h);
                break;
            }
        }
    }

    // --- Render widgets ---
    for (auto& w : layout_.widgets) {
        if (!w.visible) continue;

        float wx = gap + w.col * (cell_w + gap);
        float wy = gap + w.row * (cell_h + gap);
        float ww = w.col_span * cell_w + (w.col_span - 1) * gap;
        float wh = w.row_span * cell_h + (w.row_span - 1) * gap;

        bool is_dragging = (w.id == dragging_widget_id_);
        bool is_resizing = (w.id == resizing_widget_id_);

        // Apply smooth pixel offset during drag (visual only)
        if (is_dragging) {
            wx += drag_pixel_dx;
            wy += drag_pixel_dy;
        }

        // Apply smooth pixel offset during resize (visual only, already clamped above)
        if (is_resizing) {
            ww = std::max(cell_w, ww + resize_pixel_dw);
            wh = std::max(cell_h, wh + resize_pixel_dh);
        }

        ImGui::SetCursorPos(ImVec2(wx, wy));

        // Draw drop-target ghost showing snapped position while dragging
        if (is_dragging) {
            int snap_col = drag_start_col_ + (int)std::round(drag_pixel_dx / (cell_w + gap));
            int snap_row = drag_start_row_ + (int)std::round(drag_pixel_dy / (cell_h + gap));
            // Clamp ghost to grid bounds
            snap_col = std::clamp(snap_col, 0, grid_cols - w.col_span);
            snap_row = std::max(0, snap_row);

            float ghost_x = gap + snap_col * (cell_w + gap);
            float ghost_y = gap + snap_row * (cell_h + gap);
            float ghost_w = w.col_span * cell_w + (w.col_span - 1) * gap;
            float ghost_h = w.row_span * cell_h + (w.row_span - 1) * gap;
            ImVec2 go = ImGui::GetWindowPos();
            float scroll_y = ImGui::GetScrollY();

            // Color ghost red if it would overlap, orange if valid
            bool overlap = would_overlap(layout_.widgets, w.id,
                                          snap_col, snap_row, w.col_span, w.row_span);
            ImU32 fill_col = overlap ? IM_COL32(255, 50, 50, 30) : IM_COL32(255, 136, 0, 30);
            ImU32 border_col = overlap ? IM_COL32(255, 50, 50, 120) : IM_COL32(255, 136, 0, 120);

            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(go.x + ghost_x, go.y + ghost_y - scroll_y),
                ImVec2(go.x + ghost_x + ghost_w, go.y + ghost_y + ghost_h - scroll_y),
                fill_col, 3.0f);
            ImGui::GetWindowDrawList()->AddRect(
                ImVec2(go.x + ghost_x, go.y + ghost_y - scroll_y),
                ImVec2(go.x + ghost_x + ghost_w, go.y + ghost_y + ghost_h - scroll_y),
                border_col, 3.0f, 0, 1.5f);
        }

        render_widget_frame(w.title.c_str(), accent_for(w.type), ww, wh, w.type, w.id);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(5);
}

// ============================================================================
// Add Widget Modal
// ============================================================================
void DashboardScreen::render_add_widget_modal() {
    if (!show_add_widget_modal_) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 520), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, FC_HEADER);

    if (ImGui::Begin("Add Widget##modal", &show_add_widget_modal_,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {

        ImGui::TextColored(FC_ORANGE, "Select widget to add:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Group widgets by category
        for (int cat = 0; cat <= static_cast<int>(WidgetCategory::Tools); cat++) {
            auto category = static_cast<WidgetCategory>(cat);
            bool has_any = false;
            for (int i = 0; i < static_cast<int>(WidgetType::Count); i++) {
                if (widget_category_for(static_cast<WidgetType>(i)) == category) {
                    has_any = true; break;
                }
            }
            if (!has_any) continue;

            // Category header
            ImGui::TextColored(FC_GRAY, "%s", widget_category_name(category));
            ImGui::Separator();

            for (int i = 0; i < static_cast<int>(WidgetType::Count); i++) {
                auto type = static_cast<WidgetType>(i);
                if (widget_category_for(type) != category) continue;

                ImVec4 accent = accent_for(type);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, accent);

                if (ImGui::Button(widget_type_name(type), ImVec2(-1, 24))) {
                    add_widget(type);
                    show_add_widget_modal_ = false;
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::Spacing();
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
    ImGui::SetNextWindowSize(ImVec2(520, 560), ImGuiCond_FirstUseEver);

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

    ImGui::TextColored(FC_ORANGE, "v4.0.0");
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

} // namespace fincept::dashboard
