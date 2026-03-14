#include "dashboard_screen.h"
#include "dashboard_helpers.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

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
// Widget Frame — wraps any widget with header, accent, close button
// ============================================================================
void DashboardScreen::render_widget_frame(const char* title, const ImVec4& accent,
                                           float w, float h, WidgetType type) {
    if (w < 20 || h < 20) return;

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

} // namespace fincept::dashboard
