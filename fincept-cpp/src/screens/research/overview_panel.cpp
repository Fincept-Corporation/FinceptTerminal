#include "overview_panel.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::research {

using namespace theme::colors;

// ─── Formatting helpers ─────────────────────────────────────────────────────
// NaN sentinel: we treat exactly 0 as valid (many fields can be 0)
// Only show N/A for NaN
static bool is_missing(double v) { return std::isnan(v); }

static void fmt_number(char* buf, size_t sz, double v, int decimals = 2) {
    if (is_missing(v)) { std::snprintf(buf, sz, "N/A"); return; }
    std::snprintf(buf, sz, "%.*f", decimals, v);
}

static void fmt_large(char* buf, size_t sz, double v) {
    if (is_missing(v)) { std::snprintf(buf, sz, "N/A"); return; }
    double av = std::abs(v);
    const char* sign = v < 0 ? "-" : "";
    if (av >= 1e12)      std::snprintf(buf, sz, "%s$%.2fT", sign, av / 1e12);
    else if (av >= 1e9)  std::snprintf(buf, sz, "%s$%.2fB", sign, av / 1e9);
    else if (av >= 1e6)  std::snprintf(buf, sz, "%s$%.2fM", sign, av / 1e6);
    else if (av >= 1e3)  std::snprintf(buf, sz, "%s$%.2fK", sign, av / 1e3);
    else                 std::snprintf(buf, sz, "%s$%.2f", sign, av);
}

// Format ratio as percentage (multiply by 100): e.g. 0.27 -> "27.00%"
static void fmt_percent(char* buf, size_t sz, double v) {
    if (is_missing(v)) { std::snprintf(buf, sz, "N/A"); return; }
    std::snprintf(buf, sz, "%.2f%%", v * 100.0);
}

// Format value that's already a percentage: e.g. 0.41 -> "0.41%"
static void fmt_pct_raw(char* buf, size_t sz, double v) {
    if (is_missing(v)) { std::snprintf(buf, sz, "N/A"); return; }
    std::snprintf(buf, sz, "%.2f%%", v);
}

static void data_row(const char* label, const char* value, ImVec4 color) {
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::SameLine(w - ImGui::CalcTextSize(value).x);
    ImGui::TextColored(color, "%s", value);
}

// ─── Section with colored header ────────────────────────────────────────────
static void section_header(const char* label, ImVec4 color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_DIM);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ─────────────────────────────────────────────────────────────────────────────
void OverviewPanel::render(ResearchData& data, ChartPeriod& period) {
    std::lock_guard<std::mutex> lock(data.mutex());
    auto& si = data.stock_info();
    auto& q  = data.quote_data();
    auto& hist = data.historical();

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Yoga: vertical split — top 3-col row (flex 1.8) + bottom overview (flex 1.0)
    auto vstack = ui::vstack_layout(avail.x, avail.y, {-1.8f, -1.0f}, 4);
    float top_h = vstack.heights[0];

    // Yoga: horizontal 3-panel split for top row
    auto sizes = ui::three_panel_layout(avail.x, top_h, 20, 20, 160, 160, 280, 4);

    // ── Top row: 3 columns ──
    ImGui::BeginChild("##ov_col1", ImVec2(sizes.left_w, sizes.left_h), true);
    render_trading_valuation(q, si, sizes.left_w);
    ImGui::EndChild();

    ImGui::SameLine(0, sizes.gap);

    ImGui::BeginChild("##ov_chart", ImVec2(sizes.center_w, sizes.center_h), true);
    render_chart(hist, period, sizes.center_w, sizes.center_h - 40);
    ImGui::EndChild();

    ImGui::SameLine(0, sizes.gap);

    ImGui::BeginChild("##ov_col4", ImVec2(sizes.right_w, sizes.right_h), true);
    render_analyst_performance(si, sizes.right_w);
    ImGui::EndChild();

    // ── Bottom: Company overview ──
    ImGui::Spacing();
    float bottom_h = vstack.heights[1];
    ImGui::BeginChild("##ov_bottom", ImVec2(avail.x, bottom_h), true);
    render_company_overview(si);
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
void OverviewPanel::render_trading_valuation(const QuoteData& q, const StockInfo& si, float width) {
    char buf[64];

    section_header("TODAY'S TRADING", ACCENT);

    fmt_number(buf, sizeof(buf), q.open);
    data_row("OPEN", buf, INFO);

    fmt_number(buf, sizeof(buf), q.high);
    data_row("HIGH", buf, MARKET_GREEN);

    fmt_number(buf, sizeof(buf), q.low);
    data_row("LOW", buf, MARKET_RED);

    fmt_number(buf, sizeof(buf), q.previous_close);
    data_row("PREV CLOSE", buf, TEXT_PRIMARY);

    fmt_number(buf, sizeof(buf), q.volume, 0);
    data_row("VOLUME", buf, WARNING);

    ImGui::Spacing(); ImGui::Spacing();
    section_header("VALUATION", INFO);

    fmt_large(buf, sizeof(buf), si.market_cap);
    data_row("MARKET CAP", buf, INFO);

    fmt_number(buf, sizeof(buf), si.pe_ratio);
    data_row("P/E RATIO", buf, WARNING);

    fmt_number(buf, sizeof(buf), si.forward_pe);
    data_row("FWD P/E", buf, WARNING);

    fmt_number(buf, sizeof(buf), si.peg_ratio);
    data_row("PEG RATIO", buf, WARNING);

    fmt_number(buf, sizeof(buf), si.price_to_book);
    data_row("P/B RATIO", buf, INFO);

    fmt_pct_raw(buf, sizeof(buf), si.dividend_yield);
    data_row("DIV YIELD", buf, MARKET_GREEN);

    fmt_number(buf, sizeof(buf), si.beta);
    data_row("BETA", buf, TEXT_PRIMARY);

    ImGui::Spacing(); ImGui::Spacing();
    section_header("SHARE STATS", ImVec4(0.7f, 0.3f, 0.9f, 1.0f));

    fmt_large(buf, sizeof(buf), si.shares_outstanding);
    data_row("SHARES OUT", buf, INFO);

    fmt_large(buf, sizeof(buf), si.float_shares);
    data_row("FLOAT", buf, INFO);

    fmt_percent(buf, sizeof(buf), si.held_percent_insiders);
    data_row("INSIDERS", buf, WARNING);

    fmt_percent(buf, sizeof(buf), si.held_percent_institutions);
    data_row("INSTITUTIONS", buf, WARNING);

    fmt_percent(buf, sizeof(buf), si.short_percent_of_float);
    data_row("SHORT %", buf, MARKET_RED);
}

// ─────────────────────────────────────────────────────────────────────────────
void OverviewPanel::render_chart(const std::vector<HistoricalDataPoint>& hist,
                                  ChartPeriod& period, float width, float height) {
    // Period selector
    ImGui::TextColored(TEXT_DIM, "PERIOD");
    ImGui::SameLine(0, 8);

    static const ChartPeriod periods[] = {
        ChartPeriod::M1, ChartPeriod::M3, ChartPeriod::M6, ChartPeriod::Y1, ChartPeriod::Y5
    };
    for (auto p : periods) {
        bool active = (p == period);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        }
        if (ImGui::SmallButton(chart_period_label(p))) {
            period = p;
        }
        if (active) ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }
    ImGui::NewLine();
    ImGui::Spacing();

    if (hist.empty()) {
        ImVec2 center(ImGui::GetCursorPosX() + width * 0.5f, ImGui::GetCursorPosY() + height * 0.3f);
        ImGui::SetCursorPos(ImVec2(center.x - 60, center.y));
        ImGui::TextColored(TEXT_DIM, "No chart data available");
        return;
    }

    // Find price range
    double min_price = 1e18, max_price = -1e18;
    double max_vol = 0;
    for (auto& dp : hist) {
        if (dp.low > 0) min_price = std::min(min_price, dp.low);
        max_price = std::max(max_price, dp.high);
        max_vol = std::max(max_vol, dp.volume);
    }
    double price_range = max_price - min_price;
    if (price_range < 0.01) price_range = 1;

    float chart_h = height * 0.75f;
    float vol_h = height * 0.20f;
    ImVec2 chart_origin = ImGui::GetCursorScreenPos();
    float chart_w = width - 16;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(chart_origin,
        ImVec2(chart_origin.x + chart_w, chart_origin.y + chart_h),
        IM_COL32(10, 10, 10, 255));

    // Grid lines (5 horizontal)
    for (int i = 0; i <= 4; i++) {
        float y = chart_origin.y + chart_h * i / 4.0f;
        dl->AddLine(ImVec2(chart_origin.x, y),
                    ImVec2(chart_origin.x + chart_w, y),
                    IM_COL32(40, 40, 40, 255));
        // Price label
        char lbl[32];
        double p = max_price - price_range * i / 4.0;
        std::snprintf(lbl, sizeof(lbl), "$%.2f", p);
        dl->AddText(ImVec2(chart_origin.x + 2, y + 1), IM_COL32(100, 100, 100, 255), lbl);
    }

    // Candlestick rendering
    float bar_w = std::max(1.0f, chart_w / (float)hist.size() - 1);
    float half_bar = bar_w * 0.4f;

    for (size_t i = 0; i < hist.size(); i++) {
        float x = chart_origin.x + (float)i / hist.size() * chart_w;

        float y_open  = chart_origin.y + (float)((max_price - hist[i].open) / price_range) * chart_h;
        float y_close = chart_origin.y + (float)((max_price - hist[i].close) / price_range) * chart_h;
        float y_high  = chart_origin.y + (float)((max_price - hist[i].high) / price_range) * chart_h;
        float y_low   = chart_origin.y + (float)((max_price - hist[i].low) / price_range) * chart_h;

        bool bullish = hist[i].close >= hist[i].open;
        ImU32 col = bullish ? IM_COL32(0, 200, 90, 255) : IM_COL32(240, 50, 50, 255);

        // Wick
        dl->AddLine(ImVec2(x + half_bar, y_high), ImVec2(x + half_bar, y_low), col);
        // Body
        float body_top = std::min(y_open, y_close);
        float body_bot = std::max(y_open, y_close);
        if (body_bot - body_top < 1) body_bot = body_top + 1;
        dl->AddRectFilled(ImVec2(x, body_top), ImVec2(x + bar_w, body_bot), col);
    }

    // Volume bars below chart
    ImVec2 vol_origin(chart_origin.x, chart_origin.y + chart_h + 4);
    dl->AddRectFilled(vol_origin,
        ImVec2(vol_origin.x + chart_w, vol_origin.y + vol_h),
        IM_COL32(10, 10, 10, 255));

    if (max_vol > 0) {
        for (size_t i = 0; i < hist.size(); i++) {
            float x = vol_origin.x + (float)i / hist.size() * chart_w;
            float h = (float)(hist[i].volume / max_vol) * vol_h;
            bool bullish = hist[i].close >= hist[i].open;
            ImU32 col = bullish ? IM_COL32(0, 200, 90, 80) : IM_COL32(240, 50, 50, 80);
            dl->AddRectFilled(
                ImVec2(x, vol_origin.y + vol_h - h),
                ImVec2(x + bar_w, vol_origin.y + vol_h),
                col);
        }
    }

    // Advance cursor past chart
    ImGui::Dummy(ImVec2(chart_w, chart_h + vol_h + 8));
}

// ─────────────────────────────────────────────────────────────────────────────
void OverviewPanel::render_analyst_performance(const StockInfo& si, float width) {
    char buf[64];

    section_header("ANALYST TARGETS", ImVec4(1.0f, 0.0f, 1.0f, 1.0f));

    if (!is_missing(si.target_high_price)) std::snprintf(buf, sizeof(buf), "$%.2f", si.target_high_price);
    else std::snprintf(buf, sizeof(buf), "N/A");
    data_row("HIGH", buf, MARKET_GREEN);

    if (!is_missing(si.target_mean_price)) std::snprintf(buf, sizeof(buf), "$%.2f", si.target_mean_price);
    else std::snprintf(buf, sizeof(buf), "N/A");
    data_row("MEAN", buf, WARNING);

    if (!is_missing(si.target_low_price)) std::snprintf(buf, sizeof(buf), "$%.2f", si.target_low_price);
    else std::snprintf(buf, sizeof(buf), "N/A");
    data_row("LOW", buf, MARKET_RED);

    std::snprintf(buf, sizeof(buf), "%d", si.number_of_analysts);
    data_row("ANALYSTS", buf, INFO);

    // Recommendation badge
    if (!si.recommendation_key.empty()) {
        ImGui::Spacing();
        ImVec4 rec_col = WARNING;
        if (si.recommendation_key == "buy" || si.recommendation_key == "strong_buy")
            rec_col = MARKET_GREEN;
        else if (si.recommendation_key == "sell" || si.recommendation_key == "strong_sell")
            rec_col = MARKET_RED;

        std::string rec_text = si.recommendation_key;
        for (auto& c : rec_text) c = (char)std::toupper(c);
        for (auto& c : rec_text) if (c == '_') c = ' ';
        ImGui::TextColored(rec_col, ">> %s", rec_text.c_str());
    }

    ImGui::Spacing(); ImGui::Spacing();
    section_header("52 WEEK RANGE", WARNING);

    if (!is_missing(si.fifty_two_week_high)) std::snprintf(buf, sizeof(buf), "$%.2f", si.fifty_two_week_high);
    else std::snprintf(buf, sizeof(buf), "N/A");
    data_row("HIGH", buf, MARKET_GREEN);

    if (!is_missing(si.fifty_two_week_low)) std::snprintf(buf, sizeof(buf), "$%.2f", si.fifty_two_week_low);
    else std::snprintf(buf, sizeof(buf), "N/A");
    data_row("LOW", buf, MARKET_RED);

    fmt_number(buf, sizeof(buf), si.average_volume, 0);
    data_row("AVG VOL", buf, INFO);

    ImGui::Spacing(); ImGui::Spacing();
    section_header("PROFITABILITY", MARKET_GREEN);

    fmt_percent(buf, sizeof(buf), si.gross_margins);
    data_row("GROSS MARGIN", buf, MARKET_GREEN);

    fmt_percent(buf, sizeof(buf), si.operating_margins);
    data_row("OPER. MARGIN", buf, MARKET_GREEN);

    fmt_percent(buf, sizeof(buf), si.profit_margins);
    data_row("PROFIT MARGIN", buf, MARKET_GREEN);

    fmt_percent(buf, sizeof(buf), si.return_on_assets);
    data_row("ROA", buf, INFO);

    fmt_percent(buf, sizeof(buf), si.return_on_equity);
    data_row("ROE", buf, INFO);

    ImGui::Spacing(); ImGui::Spacing();
    section_header("GROWTH RATES", ImVec4(0.24f, 0.56f, 0.94f, 1.0f));

    fmt_percent(buf, sizeof(buf), si.revenue_growth);
    data_row("REVENUE", buf, MARKET_GREEN);

    fmt_percent(buf, sizeof(buf), si.earnings_growth);
    data_row("EARNINGS", buf, MARKET_GREEN);
}

// ─────────────────────────────────────────────────────────────────────────────
void OverviewPanel::render_company_overview(const StockInfo& si) {
    if (si.description.empty()) {
        ImGui::TextColored(TEXT_DIM, "No company description available.");
        return;
    }

    ImVec2 co_avail = ImGui::GetContentRegionAvail();
    // Yoga: description (main) + info sidebar
    auto co_panels = ui::two_panel_layout(co_avail.x, co_avail.y, true, 35, 200, 320);
    float desc_w = co_panels.main_w;
    float info_w = co_panels.side_w;

    // Left: description
    ImGui::BeginChild("##desc", ImVec2(desc_w, 0), false);
    section_header("COMPANY OVERVIEW", INFO);
    ImGui::PushTextWrapPos(desc_w - 8);
    ImGui::TextColored(TEXT_SECONDARY, "%s", si.description.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    ImGui::SameLine(0, 8);

    // Right: key info
    ImGui::BeginChild("##info_right", ImVec2(info_w, 0), false);
    char buf[64];

    section_header("COMPANY INFO", TEXT_PRIMARY);
    fmt_number(buf, sizeof(buf), (double)si.employees, 0);
    data_row("EMPLOYEES", buf, INFO);

    if (!si.website.empty()) {
        std::string site = si.website.substr(0, 30);
        data_row("WEBSITE", site.c_str(), ImVec4(0.24f, 0.56f, 0.94f, 1.0f));
    }
    if (!si.currency.empty())
        data_row("CURRENCY", si.currency.c_str(), INFO);

    ImGui::Spacing(); ImGui::Spacing();
    section_header("FINANCIAL HEALTH", ACCENT);

    fmt_large(buf, sizeof(buf), si.total_cash);
    data_row("CASH", buf, MARKET_GREEN);

    fmt_large(buf, sizeof(buf), si.total_debt);
    data_row("DEBT", buf, MARKET_RED);

    fmt_large(buf, sizeof(buf), si.free_cashflow);
    data_row("FREE CF", buf, INFO);

    ImGui::EndChild();
}

} // namespace fincept::research
