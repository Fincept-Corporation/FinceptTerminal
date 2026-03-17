#include "analysis_panel.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

namespace fincept::research {

using namespace theme::colors;

static bool is_missing(double v) { return std::isnan(v); }

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

static void fmt_percent(char* buf, size_t sz, double v) {
    if (is_missing(v)) { std::snprintf(buf, sz, "N/A"); return; }
    std::snprintf(buf, sz, "%.2f%%", v * 100.0);
}

static void fmt_number(char* buf, size_t sz, double v) {
    if (is_missing(v)) { std::snprintf(buf, sz, "N/A"); return; }
    std::snprintf(buf, sz, "%.2f", v);
}

// Metric card: label on top (dim), value below (large, colored)
static void metric_card(const char* label, const char* value, ImVec4 color) {
    ImGui::TextColored(TEXT_DIM, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted(value);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// Section with colored left bar
static void section_start(const char* title, ImVec4 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(pos, ImVec2(pos.x + 4, pos.y + 16), ImGui::GetColorU32(color));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, color);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void AnalysisPanel::render(ResearchData& data) {
    std::shared_lock<std::shared_mutex> lock(data.mutex());
    auto& si = data.stock_info();
    char buf[64];

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Yoga: 2x2 grid — vertical split into top/bottom, each with two horizontal panels
    auto vstack = ui::vstack_layout(avail.x, avail.y, {-1, -1}, 8);
    float top_h = vstack.heights[0];
    float bot_h = vstack.heights[1];
    auto top_panels = ui::two_panel_layout(avail.x, top_h, true, 50, 200, avail.x * 0.55f);
    auto bot_panels = ui::two_panel_layout(avail.x, bot_h, true, 50, 200, avail.x * 0.55f);

    // ── Top Left: Financial Health ──
    ImGui::BeginChild("##fh", ImVec2(top_panels.main_w, top_h), true);
    section_start("FINANCIAL HEALTH", ACCENT);

    // Inner 2-column grid (each quadrant splits into two columns)
    float col_w = (ImGui::GetContentRegionAvail().x - 8) * 0.5f;
    // col_w is used in all 4 quadrants — recomputed from content region, responsive

    ImGui::BeginChild("##fh_tl", ImVec2(col_w, 0), false);
    fmt_large(buf, sizeof(buf), si.total_cash);
    metric_card("TOTAL CASH", buf, MARKET_GREEN);
    fmt_large(buf, sizeof(buf), si.free_cashflow);
    metric_card("FREE CASHFLOW", buf, INFO);
    ImGui::EndChild();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##fh_tr", ImVec2(col_w, 0), false);
    fmt_large(buf, sizeof(buf), si.total_debt);
    metric_card("TOTAL DEBT", buf, MARKET_RED);
    fmt_large(buf, sizeof(buf), si.operating_cashflow);
    metric_card("OPERATING CF", buf, ImVec4(0.24f, 0.56f, 0.94f, 1.0f));
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::SameLine(0, 8);

    // ── Top Right: Enterprise Value ──
    ImGui::BeginChild("##ev", ImVec2(top_panels.side_w, top_h), true);
    section_start("ENTERPRISE VALUE", INFO);

    ImGui::BeginChild("##ev_tl", ImVec2(col_w, 0), false);
    fmt_large(buf, sizeof(buf), si.enterprise_value);
    metric_card("ENTERPRISE VALUE", buf, WARNING);
    fmt_number(buf, sizeof(buf), si.enterprise_to_ebitda);
    metric_card("EV/EBITDA", buf, INFO);
    ImGui::EndChild();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##ev_tr", ImVec2(col_w, 0), false);
    fmt_number(buf, sizeof(buf), si.enterprise_to_revenue);
    metric_card("EV/REVENUE", buf, INFO);
    if (!is_missing(si.book_value)) std::snprintf(buf, sizeof(buf), "$%.2f", si.book_value);
    else std::snprintf(buf, sizeof(buf), "N/A");
    metric_card("BOOK VALUE", buf, TEXT_PRIMARY);
    ImGui::EndChild();

    ImGui::EndChild();

    // ── Bottom Left: Revenue & Profits ──
    ImGui::BeginChild("##rp", ImVec2(bot_panels.main_w, bot_h), true);
    section_start("REVENUE & PROFITS", MARKET_GREEN);

    ImGui::BeginChild("##rp_tl", ImVec2(col_w, 0), false);
    fmt_large(buf, sizeof(buf), si.total_revenue);
    metric_card("TOTAL REVENUE", buf, MARKET_GREEN);
    fmt_large(buf, sizeof(buf), si.gross_profits);
    metric_card("GROSS PROFITS", buf, MARKET_GREEN);
    ImGui::EndChild();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##rp_tr", ImVec2(col_w, 0), false);
    if (!is_missing(si.revenue_per_share)) std::snprintf(buf, sizeof(buf), "$%.2f", si.revenue_per_share);
    else std::snprintf(buf, sizeof(buf), "N/A");
    metric_card("REVENUE/SHARE", buf, INFO);
    fmt_percent(buf, sizeof(buf), si.ebitda_margins);
    metric_card("EBITDA MARGINS", buf, WARNING);
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::SameLine(0, 8);

    // ── Bottom Right: Key Ratios ──
    ImGui::BeginChild("##kr", ImVec2(bot_panels.side_w, bot_h), true);
    section_start("KEY RATIOS", ImVec4(0.7f, 0.3f, 0.9f, 1.0f));

    ImGui::BeginChild("##kr_tl", ImVec2(col_w, 0), false);
    fmt_number(buf, sizeof(buf), si.pe_ratio);
    metric_card("P/E RATIO", buf, TEXT_PRIMARY);
    fmt_percent(buf, sizeof(buf), si.return_on_equity);
    metric_card("ROE", buf, MARKET_GREEN);
    fmt_number(buf, sizeof(buf), si.beta);
    metric_card("BETA", buf, INFO);
    ImGui::EndChild();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##kr_tr", ImVec2(col_w, 0), false);
    fmt_number(buf, sizeof(buf), si.peg_ratio);
    metric_card("PEG RATIO", buf, TEXT_PRIMARY);
    fmt_percent(buf, sizeof(buf), si.return_on_assets);
    metric_card("ROA", buf, MARKET_GREEN);
    fmt_number(buf, sizeof(buf), si.short_ratio);
    metric_card("SHORT RATIO", buf, MARKET_RED);
    ImGui::EndChild();

    ImGui::EndChild();
}

} // namespace fincept::research
