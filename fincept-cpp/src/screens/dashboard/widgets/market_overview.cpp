#include "market_overview.h"
#include "../dashboard_data.h"
#include "../dashboard_helpers.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::dashboard::widgets {

void MarketOverview::render() {
    if (ImGui::Begin("Market Overview")) {
        theme::SectionHeader("INDICES");
        ImGui::Spacing();

        auto indices = DashboardData::instance().get_quotes(DataCategory::Indices);
        bool idx_loading = DashboardData::instance().is_loading(DataCategory::Indices);

        if (indices.empty()) {
            ImGui::TextColored(FC_MUTED, idx_loading ? "Loading indices..." : "No index data available");
        } else if (ImGui::BeginTable("##indices", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("%Chg", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableHeadersRow();

            for (auto& q : indices) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());

                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", fmt_price(q.price).c_str());

                ImGui::TableNextColumn();
                ImGui::TextColored(chg_col(q.change), "%s%.2f",
                    q.change >= 0 ? "+" : "", q.change);

                ImGui::TableNextColumn();
                ImGui::TextColored(chg_col(q.change_percent), "%s%.2f%%",
                    q.change_percent >= 0 ? "+" : "", q.change_percent);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing(); ImGui::Spacing();

        theme::SectionHeader("COMMODITIES");
        ImGui::Spacing();

        auto commodities = DashboardData::instance().get_quotes(DataCategory::Commodities);
        bool cmd_loading = DashboardData::instance().is_loading(DataCategory::Commodities);

        if (commodities.empty()) {
            ImGui::TextColored(FC_MUTED, cmd_loading ? "Loading commodities..." : "No commodity data available");
        } else if (ImGui::BeginTable("##commodities", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {

            ImGui::TableSetupColumn("Commodity");
            ImGui::TableSetupColumn("Price");
            ImGui::TableSetupColumn("Change");
            ImGui::TableHeadersRow();

            for (auto& q : commodities) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(FC_WHITE, "%s", q.label.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_SECONDARY, "$%s", fmt_price(q.price).c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(chg_col(q.change_percent), "%s%.2f%%",
                    q.change_percent >= 0 ? "+" : "", q.change_percent);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        auto last_idx = DashboardData::instance().last_refresh_str(DataCategory::Indices);
        if (!last_idx.empty()) {
            ImGui::TextColored(FC_MUTED, "Last updated: %s", last_idx.c_str());
        }
    }
    ImGui::End();
}

} // namespace fincept::dashboard::widgets
