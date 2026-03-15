#include "dashboard_screen.h"
#include "dashboard_helpers.h"
#include "http/http_client.h"
#include "storage/cache_service.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <thread>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::dashboard {

// ============================================================================
// Generic quote table renderer
// ============================================================================
void DashboardScreen::render_quote_table(
    const char* table_id,
    const std::vector<QuoteEntry>& quotes,
    const char* col1, const char* col2, const char* col3,
    bool show_volume, int price_decimals)
{
    // Add a "Range" column for intraday high/low bar visualization
    int num_cols = (show_volume ? 4 : 3) + 1; // +1 for range bar
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable(table_id, num_cols, flags,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn(col1, 0, 2.0f);
        ImGui::TableSetupColumn(col2, 0, 1.5f);
        ImGui::TableSetupColumn(col3, 0, 1.0f);
        ImGui::TableSetupColumn("Range", 0, 1.2f);
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

            // Range bar: shows intraday high-low range with current price marker
            ImGui::TableNextColumn();
            if (q.high > 0 && q.low > 0 && q.high >= q.low) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                float bar_w = ImGui::GetContentRegionAvail().x - 4;
                float bar_h = 6.0f;
                float y_center = p.y + ImGui::GetTextLineHeight() * 0.5f - bar_h * 0.5f;
                ImDrawList* dl = ImGui::GetWindowDrawList();

                // Background track
                dl->AddRectFilled(
                    ImVec2(p.x, y_center),
                    ImVec2(p.x + bar_w, y_center + bar_h),
                    IM_COL32(40, 40, 40, 200), 2.0f);

                // Filled range from open to current
                float range = q.high - q.low;
                if (range > 0 && bar_w > 4) {
                    float open_frac = (q.open > 0)
                        ? (float)((q.open - q.low) / range) : 0.0f;
                    float cur_frac = (float)((q.price - q.low) / range);
                    open_frac = std::clamp(open_frac, 0.0f, 1.0f);
                    cur_frac = std::clamp(cur_frac, 0.0f, 1.0f);

                    float x1 = p.x + std::min(open_frac, cur_frac) * bar_w;
                    float x2 = p.x + std::max(open_frac, cur_frac) * bar_w;
                    bool up = q.price >= q.open;
                    ImU32 fill_col = up ? IM_COL32(0, 180, 100, 200) : IM_COL32(220, 50, 50, 200);
                    dl->AddRectFilled(ImVec2(x1, y_center), ImVec2(x2, y_center + bar_h),
                        fill_col, 2.0f);

                    // Current price marker (white triangle/line)
                    float cx = p.x + cur_frac * bar_w;
                    dl->AddRectFilled(
                        ImVec2(cx - 1, y_center - 1),
                        ImVec2(cx + 1, y_center + bar_h + 1),
                        IM_COL32(255, 255, 255, 220));
                }
                ImGui::Dummy(ImVec2(bar_w, ImGui::GetTextLineHeight()));
            } else {
                // No high/low data — show a change-direction bar
                float bar_w = ImGui::GetContentRegionAvail().x - 4;
                ImVec2 p = ImGui::GetCursorScreenPos();
                float bar_h = 6.0f;
                float y_center = p.y + ImGui::GetTextLineHeight() * 0.5f - bar_h * 0.5f;
                ImDrawList* dl = ImGui::GetWindowDrawList();

                dl->AddRectFilled(ImVec2(p.x, y_center), ImVec2(p.x + bar_w, y_center + bar_h),
                    IM_COL32(40, 40, 40, 200), 2.0f);

                // Change magnitude bar from center
                float mid = p.x + bar_w * 0.5f;
                float mag = std::clamp(std::abs((float)q.change_percent) / 5.0f, 0.0f, 1.0f);
                float extent = mag * bar_w * 0.5f;
                bool up = q.change_percent >= 0;
                ImU32 col = up ? IM_COL32(0, 180, 100, 200) : IM_COL32(220, 50, 50, 200);
                if (up)
                    dl->AddRectFilled(ImVec2(mid, y_center), ImVec2(mid + extent, y_center + bar_h), col, 2.0f);
                else
                    dl->AddRectFilled(ImVec2(mid - extent, y_center), ImVec2(mid, y_center + bar_h), col, 2.0f);

                // Center line
                dl->AddLine(ImVec2(mid, y_center - 1), ImVec2(mid, y_center + bar_h + 1),
                    IM_COL32(100, 100, 100, 180));

                ImGui::Dummy(ImVec2(bar_w, ImGui::GetTextLineHeight()));
            }

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
        cols = std::min(n, std::max(3, (int)(avail.x / 90.0f)));
        rows = (n + cols - 1) / cols;
    } else {
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

        float pct = (float)q.change_percent;
        float intensity = std::min(std::abs(pct) / 3.5f, 1.0f);
        ImU32 bg;
        if (pct > 0) {
            int g_val = (int)(40 + 170 * intensity);
            int r_val = (int)(10 * (1.0f - intensity));
            bg = IM_COL32(r_val, g_val, (int)(15 * intensity), 230);
        } else if (pct < 0) {
            int r_val = (int)(50 + 170 * intensity);
            int g_val = (int)(10 * (1.0f - intensity));
            bg = IM_COL32(r_val, g_val, 0, 230);
        } else {
            bg = IM_COL32(45, 45, 45, 230);
        }

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell_w, y + cell_h), bg, 3.0f);

        ImU32 border_col = pct > 0 ? IM_COL32(0, 180, 80, 60) :
                          (pct < 0 ? IM_COL32(180, 40, 40, 60) : IM_COL32(60, 60, 60, 80));
        dl->AddRect(ImVec2(x, y), ImVec2(x + cell_w, y + cell_h), border_col, 3.0f);

        float default_fs = ImGui::GetFontSize();
        float max_text_w = cell_w - 6.0f;

        const char* lbl = q.label.c_str();
        ImVec2 ts_full = ImGui::CalcTextSize(lbl);
        float lbl_fs = default_fs;
        if (ts_full.x > max_text_w && ts_full.x > 0) {
            lbl_fs = default_fs * (max_text_w / ts_full.x);
            if (lbl_fs < 8.0f) lbl_fs = 8.0f;
        }
        float lbl_h = lbl_fs;

        char val[16];
        std::snprintf(val, sizeof(val), "%+.2f%%", pct);
        ImVec2 vs_full = ImGui::CalcTextSize(val);
        float val_fs = default_fs;
        if (vs_full.x > max_text_w && vs_full.x > 0) {
            val_fs = default_fs * (max_text_w / vs_full.x);
            if (val_fs < 8.0f) val_fs = 8.0f;
        }
        float val_h = val_fs;

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
            show_price = cell_h >= (lbl_h + val_h + price_h + 8.0f);
        }

        float gap_between = 2.0f;
        float total_h = lbl_h + gap_between + val_h;
        if (show_price) total_h += gap_between + price_h;
        float text_y = y + std::max(2.0f, (cell_h - total_h) / 2.0f);

        ImFont* font = ImGui::GetFont();
        float lbl_w = ts_full.x * (lbl_fs / default_fs);
        float tx = x + std::max(3.0f, (cell_w - lbl_w) / 2.0f);
        dl->AddText(font, lbl_fs, ImVec2(tx, text_y),
                    IM_COL32(255, 255, 255, 240), lbl);

        float val_w = vs_full.x * (val_fs / default_fs);
        float vx = x + std::max(3.0f, (cell_w - val_w) / 2.0f);
        ImU32 tc = pct > 0 ? IM_COL32(100, 255, 160, 255) :
                   (pct < 0 ? IM_COL32(255, 120, 120, 255) : IM_COL32(140, 140, 140, 255));
        dl->AddText(font, val_fs, ImVec2(vx, text_y + lbl_h + gap_between), tc, val);

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

            ImGui::TableNextColumn();
            ImGui::TextColored(FC_MUTED, "%s", item.time_str.c_str());

            ImGui::TableNextColumn();
            if (item.priority <= 1) {
                ImGui::TextColored(FC_RED, "[!]");
                ImGui::SameLine(0, 2);
            }
            ImGui::TextColored(FC_ORANGE, "%s", item.source.c_str());

            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextColored(FC_WHITE, "%s", item.headline.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_economic(float w, float h) {
    if (ImGui::BeginTable("##eco", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Indicator", 0, 2.5f);
        ImGui::TableSetupColumn("Value", 0, 1.0f);
        ImGui::TableSetupColumn("Chg", 0, 1.0f);
        ImGui::TableSetupColumn("", 0, 1.2f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto& e : econ_data_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", e.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.2f", e.value);
            ImGui::TableNextColumn(); ImGui::TextColored(chg_col(e.change), "%+.2f", e.change);
            // Change bar
            ImGui::TableNextColumn();
            float bar_w = ImGui::GetContentRegionAvail().x - 4;
            if (bar_w > 8) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                float bar_h = 6.0f;
                float yc = p.y + ImGui::GetTextLineHeight() * 0.5f - bar_h * 0.5f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(ImVec2(p.x, yc), ImVec2(p.x + bar_w, yc + bar_h),
                    IM_COL32(40, 40, 40, 200), 2.0f);
                float mid = p.x + bar_w * 0.5f;
                float mag = std::clamp(std::abs(e.change) / 3.0f, 0.0f, 1.0f);
                float ext = mag * bar_w * 0.5f;
                ImU32 col = e.change >= 0 ? IM_COL32(0, 180, 100, 200) : IM_COL32(220, 50, 50, 200);
                if (e.change >= 0)
                    dl->AddRectFilled(ImVec2(mid, yc), ImVec2(mid + ext, yc + bar_h), col, 2.0f);
                else
                    dl->AddRectFilled(ImVec2(mid - ext, yc), ImVec2(mid, yc + bar_h), col, 2.0f);
                dl->AddLine(ImVec2(mid, yc - 1), ImVec2(mid, yc + bar_h + 1), IM_COL32(100, 100, 100, 180));
                ImGui::Dummy(ImVec2(bar_w, ImGui::GetTextLineHeight()));
            }
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_geopolitics(float w, float h) {
    if (ImGui::BeginTable("##geo", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Region", 0, 1.8f);
        ImGui::TableSetupColumn("Risk", 0, 0.7f);
        ImGui::TableSetupColumn("Level", 0, 1.4f);
        ImGui::TableSetupColumn("Trend", 0, 0.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto& g : geo_data_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(FC_WHITE, "%s", g.region.c_str());
            ImGui::TableNextColumn();
            ImVec4 rc = g.risk > 70 ? FC_RED : (g.risk > 50 ? FC_YELLOW : FC_GREEN);
            ImGui::TextColored(rc, "%.0f", g.risk);
            // Risk level bar
            ImGui::TableNextColumn();
            {
                float bar_w = ImGui::GetContentRegionAvail().x - 4;
                if (bar_w > 8) {
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float bar_h = 8.0f;
                    float yc = p.y + ImGui::GetTextLineHeight() * 0.5f - bar_h * 0.5f;
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    // Background
                    dl->AddRectFilled(ImVec2(p.x, yc), ImVec2(p.x + bar_w, yc + bar_h),
                        IM_COL32(30, 30, 30, 220), 3.0f);
                    // Filled portion based on risk 0-100
                    float frac = std::clamp(g.risk / 100.0f, 0.0f, 1.0f);
                    // Color gradient: green -> yellow -> red
                    int r_val, g_val;
                    if (frac < 0.5f) {
                        r_val = (int)(frac * 2 * 220);
                        g_val = 180;
                    } else {
                        r_val = 220;
                        g_val = (int)((1.0f - frac) * 2 * 180);
                    }
                    dl->AddRectFilled(ImVec2(p.x, yc), ImVec2(p.x + frac * bar_w, yc + bar_h),
                        IM_COL32(r_val, g_val, 0, 220), 3.0f);
                    ImGui::Dummy(ImVec2(bar_w, ImGui::GetTextLineHeight()));
                }
            }
            ImGui::TableNextColumn();
            ImGui::TextColored(chg_col(g.trend), "%+.1f", g.trend);
        }
        ImGui::EndTable();
    }
}

void DashboardScreen::widget_performance(float w, float h) {
    if (ImGui::BeginTable("##perf", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Metric", 0, 2.0f);
        ImGui::TableSetupColumn("Value", 0, 1.5f);
        ImGui::TableSetupColumn("", 0, 1.0f);
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
            // Change bar
            ImGui::TableNextColumn();
            float bar_w = ImGui::GetContentRegionAvail().x - 4;
            if (bar_w > 8) {
                ImVec2 bp = ImGui::GetCursorScreenPos();
                float bar_h = 6.0f;
                float yc = bp.y + ImGui::GetTextLineHeight() * 0.5f - bar_h * 0.5f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(ImVec2(bp.x, yc), ImVec2(bp.x + bar_w, yc + bar_h),
                    IM_COL32(40, 40, 40, 200), 2.0f);
                float mid = bp.x + bar_w * 0.5f;
                float mag = std::clamp(std::abs(p.change) / 5.0f, 0.0f, 1.0f);
                float ext = mag * bar_w * 0.5f;
                ImU32 col = p.change >= 0 ? IM_COL32(0, 180, 100, 200) : IM_COL32(220, 50, 50, 200);
                if (p.change >= 0)
                    dl->AddRectFilled(ImVec2(mid, yc), ImVec2(mid + ext, yc + bar_h), col, 2.0f);
                else
                    dl->AddRectFilled(ImVec2(mid - ext, yc), ImVec2(mid, yc + bar_h), col, 2.0f);
                dl->AddLine(ImVec2(mid, yc - 1), ImVec2(mid, yc + bar_h + 1), IM_COL32(100, 100, 100, 180));
                ImGui::Dummy(ImVec2(bar_w, ImGui::GetTextLineHeight()));
            }
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
// YouTube Stream Widget — link launcher with metadata fetch
// ============================================================================
static void yt_open_url(const char* url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = std::string("open ") + url;
    system(cmd.c_str());
#else
    std::string cmd = std::string("xdg-open ") + url;
    system(cmd.c_str());
#endif
}

static std::string extract_video_id(const std::string& url) {
    // Match youtube.com/watch?v=ID, youtu.be/ID, youtube.com/live/ID
    std::regex patterns[] = {
        std::regex(R"((?:youtube\.com/watch\?.*v=)([a-zA-Z0-9_-]{11}))"),
        std::regex(R"((?:youtu\.be/)([a-zA-Z0-9_-]{11}))"),
        std::regex(R"((?:youtube\.com/live/)([a-zA-Z0-9_-]{11}))"),
    };
    for (auto& pat : patterns) {
        std::smatch m;
        if (std::regex_search(url, m, pat) && m.size() > 1) return m[1].str();
    }
    return {};
}

void DashboardScreen::yt_fetch_metadata(int index) {
    if (index < 0 || index >= (int)yt_entries_.size()) return;
    auto& entry = yt_entries_[index];
    if (entry.fetching || entry.fetched) return;
    entry.fetching = true;

    std::string url = entry.url;
    std::thread([this, index, url]() {
        // Use YouTube oEmbed API (no API key needed)
        auto& cache = CacheService::instance();
        std::string oembed_ck = CacheService::make_key("api-response", "yt-oembed", url);

        std::string body;
        auto cached_oembed = cache.get(oembed_ck);
        if (cached_oembed && !cached_oembed->empty()) {
            body = *cached_oembed;
        } else {
            std::string oembed_url = "https://www.youtube.com/oembed?url=" + url + "&format=json";
            http::Headers headers;
            headers["accept"] = "application/json";
            auto resp = http::HttpClient::instance().get(oembed_url, headers);
            if (resp.success && !resp.body.empty()) {
                body = resp.body;
                cache.set(oembed_ck, body, "api-response", CacheTTL::ONE_DAY);
            }
        }

        if (index < (int)yt_entries_.size()) {
            auto& e = yt_entries_[index];
            if (!body.empty()) {
                try {
                    auto j = nlohmann::json::parse(body);
                    e.title = j.value("title", "");
                    e.author = j.value("author_name", "");
                } catch (...) {
                    e.title = "(Could not load metadata)";
                }
            } else {
                e.title = "(Could not load metadata)";
            }
            e.fetched = true;
            e.fetching = false;
        }
    }).detach();
}

void DashboardScreen::widget_youtube_stream(float w, float h) {
    float avail_w = ImGui::GetContentRegionAvail().x;

    // ---- Video player area (when playing) ----
#ifdef FINCEPT_HAS_MPV
    if (yt_active_player_ >= 0 && yt_active_player_ < (int)yt_entries_.size() && yt_player_) {
        auto& active = yt_entries_[yt_active_player_];
        auto state = yt_player_->state();

        // Video display area — fill all available space minus controls
        float controls_h = 54.0f;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float video_w = avail.x;
        float video_h = avail.y - controls_h;
        if (video_h < 60) video_h = 60;
        if (video_w < 60) video_w = 60;

        // Render mpv frame into texture
        unsigned int tex = yt_player_->render_frame((int)video_w, (int)video_h);
        if (tex) {
            ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(video_w, video_h));
        } else {
            // Loading / error state
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::BeginChild("##yt_vid_placeholder", ImVec2(video_w, video_h), false);
            float cy = video_h * 0.4f;
            ImGui::Dummy(ImVec2(0, cy));
            const char* status_text = "Loading video...";
            if (state == media::PlayerState::Error)
                status_text = yt_player_->error_msg().c_str();
            else if (state == media::PlayerState::Stopped)
                status_text = "Stopped";
            float tw = ImGui::CalcTextSize(status_text).x;
            ImGui::SetCursorPosX((video_w - tw) * 0.5f);
            ImGui::TextColored(state == media::PlayerState::Error ? FC_RED : FC_YELLOW, "%s", status_text);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // Controls bar
        ImGui::Spacing();

        // Title
        ImGui::TextColored(FC_WHITE, "%s", active.title.empty() ? active.url.c_str() : active.title.c_str());
        if (!active.author.empty()) {
            ImGui::SameLine(0, 8);
            ImGui::TextColored(FC_GRAY, "by %s", active.author.c_str());
        }

        // Playback controls
        bool is_playing = (state == media::PlayerState::Playing);
        bool is_paused = (state == media::PlayerState::Paused);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));

        if (ImGui::SmallButton(is_paused ? "[>] PLAY" : "[||] PAUSE")) {
            yt_player_->toggle_pause();
        }
        ImGui::SameLine(0, 6);
        if (ImGui::SmallButton("[x] STOP")) {
            yt_player_->stop();
            yt_active_player_ = -1;
        }
        ImGui::SameLine(0, 6);

        // Volume slider
        ImGui::PushItemWidth(60);
        if (ImGui::SliderInt("##yt_vol", &yt_volume_, 0, 100, "Vol:%d")) {
            yt_player_->set_volume(yt_volume_);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0, 6);
        if (ImGui::SmallButton("[^] OPEN")) {
            yt_open_url(active.url.c_str());
        }

        ImGui::PopStyleColor(2);

        // Progress bar
        double dur = yt_player_->duration();
        double pos = yt_player_->position();
        if (dur > 0) {
            float frac = (float)(pos / dur);
            char time_buf[32];
            int pm = (int)pos / 60, ps = (int)pos % 60;
            int dm = (int)dur / 60, ds = (int)dur % 60;
            std::snprintf(time_buf, sizeof(time_buf), "%d:%02d / %d:%02d", pm, ps, dm, ds);
            ImGui::ProgressBar(frac, ImVec2(-1, 14), time_buf);
        }

        return;
    }
#endif

    // ---- URL input row ----
    ImGui::TextColored(FC_RED, ">>"); ImGui::SameLine(0, 4);
    ImGui::TextColored(FC_WHITE, "Add YouTube Link:");

    ImGui::PushItemWidth(avail_w - 70);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, FC_WHITE);
    bool entered = ImGui::InputText("##yt_url", yt_url_buf_, sizeof(yt_url_buf_),
                                     ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 4);
    bool add_clicked = false;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, FC_WHITE);
    add_clicked = ImGui::Button("ADD##yt", ImVec2(54, 0));
    ImGui::PopStyleColor(3);

    if ((entered || add_clicked) && yt_url_buf_[0] != '\0') {
        std::string url_str(yt_url_buf_);
        auto vid_id = extract_video_id(url_str);
        if (!vid_id.empty()) {
            YouTubeEntry entry;
            entry.url = url_str;
            entry.title = "Loading...";
            yt_entries_.push_back(std::move(entry));
            yt_fetch_metadata((int)yt_entries_.size() - 1);
            yt_url_buf_[0] = '\0';
        } else {
            std::strncpy(yt_url_buf_, "Invalid YouTube URL!", sizeof(yt_url_buf_) - 1);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---- Stream list ----
    if (yt_entries_.empty()) {
        float center_y = ImGui::GetContentRegionAvail().y * 0.3f;
        ImGui::Dummy(ImVec2(0, center_y));
        float text_w = ImGui::CalcTextSize("Paste a YouTube link above to get started").x;
        ImGui::SetCursorPosX((avail_w - text_w) * 0.5f);
        ImGui::TextColored(FC_MUTED, "Paste a YouTube link above to get started");

#ifdef FINCEPT_HAS_MPV
        ImGui::Spacing();
        float note_w = ImGui::CalcTextSize("Inline video playback enabled (mpv)").x;
        ImGui::SetCursorPosX((avail_w - note_w) * 0.5f);
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.3f, 1.0f), "Inline video playback enabled (mpv)");
#endif
        return;
    }

    ImGui::BeginChild("##yt_list", ImVec2(0, 0), false);
    int remove_idx = -1;
    for (int i = 0; i < (int)yt_entries_.size(); i++) {
        auto& e = yt_entries_[i];
        ImGui::PushID(i);

        // Card background
        bool is_active = (i == yt_active_player_);
        ImVec2 card_start = ImGui::GetCursorScreenPos();
        float card_h = 52;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(card_start, ImVec2(card_start.x + avail_w - 4, card_start.y + card_h),
            is_active ? IM_COL32(30, 15, 15, 255) : IM_COL32(20, 20, 20, 255), 3.0f);
        dl->AddRect(card_start, ImVec2(card_start.x + avail_w - 4, card_start.y + card_h),
            is_active ? IM_COL32(180, 40, 40, 200) : IM_COL32(60, 20, 20, 180), 3.0f);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

        // YouTube play icon
        ImGui::TextColored(is_active ? FC_GREEN : FC_RED, is_active ? "[=]" : "[>]");
        ImGui::SameLine(0, 6);

        // Title + author
        if (e.fetching) {
            ImGui::TextColored(FC_YELLOW, "Loading metadata...");
        } else {
            ImGui::TextColored(FC_WHITE, "%s", e.title.empty() ? e.url.c_str() : e.title.c_str());
        }
        if (!e.author.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 32);
            ImGui::TextColored(FC_GRAY, "by %s", e.author.c_str());
        }

        // Buttons row
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);

#ifdef FINCEPT_HAS_MPV
        // PLAY button — inline playback
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, FC_WHITE);
        if (ImGui::SmallButton("PLAY")) {
            if (!yt_player_) {
                yt_player_ = std::make_unique<media::MpvPlayer>();
                yt_player_->init();
                yt_player_->set_volume(yt_volume_);
            }
            yt_player_->play(e.url);
            yt_active_player_ = i;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 4);
#endif

        // WATCH button — open in browser
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, FC_WHITE);
        if (ImGui::SmallButton("WATCH")) {
            yt_open_url(e.url.c_str());
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, FC_MUTED);
        if (ImGui::SmallButton("REMOVE")) {
            remove_idx = i;
        }
        ImGui::PopStyleColor(3);

        // Ensure card height
        float cur_y = ImGui::GetCursorPosY();
        float expected_end = card_start.y - ImGui::GetWindowPos().y + card_h + 4;
        if (cur_y < expected_end) ImGui::SetCursorPosY(expected_end);

        ImGui::PopID();
        ImGui::Spacing();
    }
    if (remove_idx >= 0) {
        if (remove_idx == yt_active_player_) {
            if (yt_player_) yt_player_->stop();
            yt_active_player_ = -1;
        } else if (remove_idx < yt_active_player_) {
            yt_active_player_--;
        }
        yt_entries_.erase(yt_entries_.begin() + remove_idx);
    }
    ImGui::EndChild();
}

// ============================================================================
// Widget Frame — wraps any widget with header, accent, close button
//               + drag handle (header) + resize grip (bottom-right corner)
// ============================================================================
void DashboardScreen::render_widget_frame(const char* title, const ImVec4& accent,
                                           float w, float h, WidgetType type,
                                           const std::string& widget_id) {
    if (w < 20 || h < 20) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, FC_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, FC_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.0f);

    std::string id = std::string("##wf_") + widget_id;
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

    // Drag handle icon (left side of header)
    ImGui::SetCursorPosX(4);
    ImGui::SetCursorPosY(4);
    ImGui::TextColored(FC_MUTED, "::"); // grip dots
    ImGui::SameLine(0, 4);

    // Title
    ImGui::TextColored(accent, "%s", title);

    // --- Drag-to-rearrange: click header to drag ---
    {
        ImVec2 hdr_min = hdr_start;
        ImVec2 hdr_max = ImVec2(hdr_start.x + w - 22, hdr_start.y + hdr_h);
        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool hover_hdr = (mouse.x >= hdr_min.x && mouse.x <= hdr_max.x &&
                          mouse.y >= hdr_min.y && mouse.y <= hdr_max.y);

        if (hover_hdr && ImGui::IsMouseClicked(0) && dragging_widget_id_.empty()
            && resizing_widget_id_.empty()) {
            dragging_widget_id_ = widget_id;
            drag_start_mouse_ = mouse;
            for (auto& wi : layout_.widgets) {
                if (wi.id == widget_id) {
                    drag_start_col_ = wi.col;
                    drag_start_row_ = wi.row;
                    break;
                }
            }
        }

        // Cursor hint
        if (hover_hdr || dragging_widget_id_ == widget_id) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }

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
        std::string close_id = std::string("x##c_") + widget_id;
        if (ImGui::SmallButton(close_id.c_str())) {
            remove_widget(widget_id);
        }
        ImGui::PopStyleColor(3);
    }

    // Content
    ImGui::SetCursorPosY(hdr_h + 2);
    float content_h = h - hdr_h - 4;
    if (content_h < 10) content_h = 10;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::BeginChild((std::string("##wc_") + widget_id).c_str(), ImVec2(w - 2, content_h), false);

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
        case WidgetType::YouTubeStream:     widget_youtube_stream(w, content_h); break;
        default: ImGui::TextColored(FC_MUTED, "Unknown widget"); break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // --- Resize grip (bottom-right corner) ---
    {
        float grip_sz = 14.0f;
        ImVec2 widget_screen_br = ImVec2(hdr_start.x + w, hdr_start.y + h);
        ImVec2 grip_min = ImVec2(widget_screen_br.x - grip_sz, widget_screen_br.y - grip_sz);
        ImVec2 grip_max = widget_screen_br;
        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool hover_grip = (mouse.x >= grip_min.x && mouse.x <= grip_max.x &&
                           mouse.y >= grip_min.y && mouse.y <= grip_max.y);

        // Draw resize triangle
        ImU32 grip_col = hover_grip || resizing_widget_id_ == widget_id
            ? IM_COL32(255, 136, 0, 200) : IM_COL32(120, 120, 120, 100);
        dl->AddTriangleFilled(
            ImVec2(grip_max.x, grip_max.y - grip_sz),
            ImVec2(grip_max.x - grip_sz, grip_max.y),
            grip_max, grip_col);

        if (hover_grip && ImGui::IsMouseClicked(0) && resizing_widget_id_.empty()
            && dragging_widget_id_.empty()) {
            resizing_widget_id_ = widget_id;
            drag_start_mouse_ = mouse;
            for (auto& wi : layout_.widgets) {
                if (wi.id == widget_id) {
                    resize_start_col_span_ = wi.col_span;
                    resize_start_row_span_ = wi.row_span;
                    break;
                }
            }
        }

        if (hover_grip || resizing_widget_id_ == widget_id) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace fincept::dashboard
