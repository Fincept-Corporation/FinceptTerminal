#include "market_overview.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>
#include <cmath>

namespace fincept::dashboard::widgets {

void MarketOverview::render() {
    if (ImGui::Begin("Market Overview")) {
        theme::SectionHeader("INDICES");
        ImGui::Spacing();

        if (ImGui::BeginTable("##indices", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("%Chg", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableHeadersRow();

            struct IndexData { const char* name; float last; float change; };
            IndexData indices[] = {
                {"S&P 500",     5234.18f,  +28.72f},
                {"DJIA",        39512.84f, +125.08f},
                {"NASDAQ",      16384.47f, +85.32f},
                {"Russell 2000", 2084.15f, -12.43f},
                {"VIX",         13.42f,    -0.28f},
                {"DXY",         104.28f,   +0.15f},
            };

            for (auto& idx : indices) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", idx.name);

                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%.2f", idx.last);

                ImGui::TableNextColumn();
                bool positive = idx.change >= 0;
                ImGui::TextColored(positive ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED,
                    "%s%.2f", positive ? "+" : "", idx.change);

                ImGui::TableNextColumn();
                float pct = (idx.change / idx.last) * 100.0f;
                ImGui::TextColored(positive ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED,
                    "%s%.2f%%", pct >= 0 ? "+" : "", pct);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing(); ImGui::Spacing();

        theme::SectionHeader("COMMODITIES");
        ImGui::Spacing();

        if (ImGui::BeginTable("##commodities", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

            ImGui::TableSetupColumn("Commodity");
            ImGui::TableSetupColumn("Price");
            ImGui::TableSetupColumn("Change");
            ImGui::TableHeadersRow();

            struct CommodityData { const char* name; const char* price; const char* change; bool pos; };
            CommodityData commodities[] = {
                {"Gold",   "$2,178.30", "+$12.40", true},
                {"Silver", "$24.85",    "-$0.32",  false},
                {"WTI Oil","$78.42",    "+$1.15",  true},
                {"Nat Gas","$1.72",     "-$0.08",  false},
                {"Copper", "$3.92",     "+$0.05",  true},
            };

            for (auto& c : commodities) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", c.name);
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", c.price);
                ImGui::TableNextColumn();
                ImGui::TextColored(c.pos ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED,
                    "%s", c.change);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_DIM, "Sample data - connect to live market feed");
    }
    ImGui::End();
}

} // namespace fincept::dashboard::widgets
