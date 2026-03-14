#include "technicals_panel.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

namespace fincept::research {

using namespace theme::colors;

static ImVec4 signal_color(Signal s) {
    switch (s) {
        case Signal::BUY:     return MARKET_GREEN;
        case Signal::SELL:    return MARKET_RED;
        case Signal::NEUTRAL: return TEXT_DIM;
    }
    return TEXT_DIM;
}

static const char* signal_text(Signal s) {
    switch (s) {
        case Signal::BUY:     return "BUY";
        case Signal::SELL:    return "SELL";
        case Signal::NEUTRAL: return "HOLD";
    }
    return "HOLD";
}

static const char* signal_arrow(Signal s) {
    switch (s) {
        case Signal::BUY:     return "^";
        case Signal::SELL:    return "v";
        case Signal::NEUTRAL: return "-";
    }
    return "-";
}

void TechnicalsPanel::render(ResearchData& data) {
    std::lock_guard<std::mutex> lock(data.mutex());
    auto& tech = data.technicals();

    if (tech.indicators.empty()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(avail.x * 0.35f, avail.y * 0.4f));
        if (data.is_loading()) {
            theme::LoadingSpinner("Computing technical indicators...");
        } else {
            ImGui::TextColored(MARKET_RED, "NO TECHNICAL DATA AVAILABLE");
            ImGui::SetCursorPosX(avail.x * 0.35f);
            ImGui::TextColored(TEXT_DIM, "Waiting for historical data");
        }
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // ─── Top: Summary Dashboard ───
    float summary_h = std::min(120.0f, avail.y * 0.25f);
    // Yoga: signal panel (side) + key indicators (main)
    auto summary_panels = ui::two_panel_layout(avail.x, summary_h, true, 25, 200, 280);
    float left_w = summary_panels.side_w;
    float right_w = summary_panels.main_w;

    // Overall Signal Panel
    ImGui::BeginChild("##tech_signal", ImVec2(left_w, summary_h), true);
    {
        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        ImGui::TextUnformatted("TECHNICAL RATING");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        // Recommendation
        ImVec4 rec_col = WARNING;
        if (tech.recommendation.find("BUY") != std::string::npos) rec_col = MARKET_GREEN;
        else if (tech.recommendation.find("SELL") != std::string::npos) rec_col = MARKET_RED;

        ImGui::SetWindowFontScale(1.3f);
        float text_w = ImGui::CalcTextSize(tech.recommendation.c_str()).x;
        ImGui::SetCursorPosX((left_w - text_w) * 0.5f);
        ImGui::TextColored(rec_col, "%s", tech.recommendation.c_str());
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();

        // Signal gauge bar
        int total = tech.buy_count + tech.sell_count + tech.neutral_count;
        if (total > 0) {
            float bar_w = ImGui::GetContentRegionAvail().x;
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            float sell_w = bar_w * (float)tech.sell_count / total;
            float neut_w = bar_w * (float)tech.neutral_count / total;
            float buy_w  = bar_w * (float)tech.buy_count / total;

            dl->AddRectFilled(pos, ImVec2(pos.x + sell_w, pos.y + 6), ImGui::GetColorU32(MARKET_RED));
            dl->AddRectFilled(ImVec2(pos.x + sell_w, pos.y),
                              ImVec2(pos.x + sell_w + neut_w, pos.y + 6), IM_COL32(100, 100, 100, 255));
            dl->AddRectFilled(ImVec2(pos.x + sell_w + neut_w, pos.y),
                              ImVec2(pos.x + bar_w, pos.y + 6), ImGui::GetColorU32(MARKET_GREEN));
            ImGui::Dummy(ImVec2(bar_w, 10));
        }

        // Counts
        float third_w = (ImGui::GetContentRegionAvail().x - 8) / 3.0f;
        ImGui::BeginChild("##buy_c", ImVec2(third_w, 0), false);
        ImGui::TextColored(MARKET_GREEN, "%d", tech.buy_count);
        ImGui::TextColored(TEXT_DIM, "BUY");
        ImGui::EndChild();
        ImGui::SameLine(0, 4);
        ImGui::BeginChild("##hold_c", ImVec2(third_w, 0), false);
        ImGui::TextColored(TEXT_PRIMARY, "%d", tech.neutral_count);
        ImGui::TextColored(TEXT_DIM, "HOLD");
        ImGui::EndChild();
        ImGui::SameLine(0, 4);
        ImGui::BeginChild("##sell_c", ImVec2(third_w, 0), false);
        ImGui::TextColored(MARKET_RED, "%d", tech.sell_count);
        ImGui::TextColored(TEXT_DIM, "SELL");
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 8);

    // Key Indicators Quick View
    ImGui::BeginChild("##tech_key", ImVec2(right_w, summary_h), true);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, INFO);
        ImGui::TextUnformatted("KEY INDICATORS");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        // Grid of key indicators — responsive card width
        auto grid = ui::responsive_grid(right_w, 120, 4);
        float card_w = grid.panel_w;
        int cols = grid.columns;
        int col = 0;

        for (auto& ind : tech.indicators) {
            if (col > 0) ImGui::SameLine(0, 4);

            ImGui::BeginChild(("##ki_" + ind.name).c_str(), ImVec2(card_w, 50), true);
            ImVec4 sc = signal_color(ind.signal);

            // Left border color
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetWindowPos();
            dl->AddRectFilled(p, ImVec2(p.x + 2, p.y + 50), ImGui::GetColorU32(sc));

            ImGui::TextColored(TEXT_DIM, "%s", ind.name.c_str());
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", ind.value);
            ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
            ImGui::SameLine();
            ImGui::TextColored(sc, "%s %s", signal_arrow(ind.signal), signal_text(ind.signal));
            ImGui::EndChild();

            col++;
            if (col >= cols) col = 0;
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ─── Category Sections ───
    auto render_category = [&](const char* title, const char* cat, ImVec4 color, bool& expanded) {
        // Count signals in this category
        int buy = 0, sell = 0, neutral = 0;
        for (auto& ind : tech.indicators) {
            if (ind.category == cat) {
                if (ind.signal == Signal::BUY) buy++;
                else if (ind.signal == Signal::SELL) sell++;
                else neutral++;
            }
        }
        int cat_total = buy + sell + neutral;
        if (cat_total == 0) return;

        // Header
        ImGui::PushStyleColor(ImGuiCol_Header, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

        // Build header label with counts
        char hdr[128];
        std::snprintf(hdr, sizeof(hdr), "%s %s  [%d INDICATORS]", expanded ? "v" : ">", title, cat_total);

        if (ImGui::Selectable(hdr, false, 0, ImVec2(0, 24))) {
            expanded = !expanded;
        }

        // Signal counts on the same line (draw after selectable)
        {
            // Compute dynamic right-align offset for signal count badges
            float badge_w = 0;
            if (buy > 0) badge_w += ImGui::CalcTextSize("99 BUY").x + 12;
            if (sell > 0) badge_w += ImGui::CalcTextSize("99 SELL").x + 12;
            if (neutral > 0) badge_w += ImGui::CalcTextSize("99 HOLD").x + 12;
            float signal_pos = std::max(ImGui::GetCursorPosX() + 20.0f,
                                        ImGui::GetContentRegionAvail().x - badge_w);
            ImGui::SameLine(signal_pos);
            if (buy > 0) {
                ImGui::TextColored(MARKET_GREEN, "%d BUY", buy);
                ImGui::SameLine(0, 8);
            }
            if (sell > 0) {
                ImGui::TextColored(MARKET_RED, "%d SELL", sell);
                ImGui::SameLine(0, 8);
            }
            if (neutral > 0) {
                ImGui::TextColored(TEXT_DIM, "%d HOLD", neutral);
            }
        }

        ImGui::PopStyleColor(2);

        // Left color border
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 last_pos = ImGui::GetItemRectMin();
        dl->AddRectFilled(last_pos, ImVec2(last_pos.x + 2, last_pos.y + 24), ImGui::GetColorU32(color));

        if (expanded) {
            ImGui::Indent(8);

            // Table for indicators in this category
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable(cat, 6, flags)) {
                ImGui::TableSetupColumn("INDICATOR", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                ImGui::TableSetupColumn("VALUE", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("PREV", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("MIN", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("MAX", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("SIGNAL", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();

                for (auto& ind : tech.indicators) {
                    if (ind.category != cat) continue;
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(TEXT_SECONDARY, "%s", ind.name.c_str());

                    char buf[32];

                    ImGui::TableSetColumnIndex(1);
                    std::snprintf(buf, sizeof(buf), "%.2f", ind.value);
                    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

                    ImGui::TableSetColumnIndex(2);
                    std::snprintf(buf, sizeof(buf), "%.2f", ind.prev_value);
                    ImGui::TextColored(TEXT_DIM, "%s", buf);

                    ImGui::TableSetColumnIndex(3);
                    std::snprintf(buf, sizeof(buf), "%.2f", ind.min_val);
                    ImGui::TextColored(TEXT_DIM, "%s", buf);

                    ImGui::TableSetColumnIndex(4);
                    std::snprintf(buf, sizeof(buf), "%.2f", ind.max_val);
                    ImGui::TextColored(TEXT_DIM, "%s", buf);

                    ImGui::TableSetColumnIndex(5);
                    ImVec4 sc = signal_color(ind.signal);
                    ImGui::TextColored(sc, "%s %s", signal_arrow(ind.signal), signal_text(ind.signal));
                }

                ImGui::EndTable();
            }

            ImGui::Unindent(8);
            ImGui::Spacing();
        }
    };

    render_category("TREND INDICATORS", "trend", INFO, show_trend_);
    render_category("MOMENTUM INDICATORS", "momentum", MARKET_GREEN, show_momentum_);
    render_category("VOLATILITY INDICATORS", "volatility", WARNING, show_volatility_);

    // Trading notes
    ImGui::Spacing();
    ImGui::BeginChild("##trade_notes", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
    ImGui::TextUnformatted("TRADING NOTES");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::TextColored(TEXT_DIM, "* Technical indicators should be used with fundamental analysis and risk management.");
    ImGui::TextColored(TEXT_DIM, "* Signal strength increases when multiple indicators align (confluence).");
    ImGui::TextColored(TEXT_DIM, "* Overbought/oversold conditions can persist in strong trends.");
    ImGui::TextColored(TEXT_DIM, "* Past performance does not guarantee future results. Always use stop-losses.");
    ImGui::EndChild();
}

} // namespace fincept::research
