// QuantStats + FFN View
// Port of QuantStatsPanel.tsx + FFNView.tsx

#include "quantstats_view.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::portfolio {

void QuantStatsView::render(const PortfolioSummary& summary) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        qs_loaded_ = ffn_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"QuantStats", "FFN Analytics"};
    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(130, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 20);
    if (loading_) {
        theme::LoadingSpinner("Computing...");
    } else {
        if (theme::AccentButton("Run")) {
            if (sub_tab_ == 0) fetch_quantstats(summary);
            else fetch_ffn(summary);
        }
    }

    ImGui::Separator();

    if (sub_tab_ == 0) {
        if (!qs_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to generate comprehensive quantitative statistics");
        } else {
            theme::SectionHeader("QuantStats Report");
            render_stats_table(qs_result_);
        }
    } else {
        if (!ffn_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to compute FFN (Financial Functions) deep analytics");
        } else {
            theme::SectionHeader("FFN Analytics");
            render_stats_table(ffn_result_);
        }
    }
}

void QuantStatsView::render_stats_table(const nlohmann::json& data) {
    if (!data.is_object()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No data");
        return;
    }

    // Render nested objects as separate sections, scalars as KV table
    bool has_scalars = false;
    for (auto& [k, v] : data.items()) {
        if (!v.is_object() && !v.is_array()) { has_scalars = true; break; }
    }

    if (has_scalars) {
        if (ImGui::BeginTable("##stats_kv", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                ImVec2(0, 300))) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 280);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            ImGui::TableSetupScrollFreeze(0, 1);

            for (auto& [k, v] : data.items()) {
                if (v.is_object() || v.is_array()) continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                ImGui::TableNextColumn();
                if (v.is_number()) {
                    double val = v.get<double>();
                    ImVec4 col = val < 0 ? theme::colors::MARKET_RED : ImVec4(1, 1, 1, 1);
                    ImGui::TextColored(col, "%.6g", val);
                } else if (v.is_string()) {
                    ImGui::TextUnformatted(v.get<std::string>().c_str());
                } else {
                    ImGui::TextUnformatted(v.dump().c_str());
                }
            }
            ImGui::EndTable();
        }
    }

    // Render nested objects as subsections
    for (auto& [k, v] : data.items()) {
        if (!v.is_object()) continue;
        ImGui::Spacing();
        theme::SectionHeader(k.c_str());
        if (ImGui::BeginTable(("##" + k).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 280);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            for (auto& [sk, sv] : v.items()) {
                if (sv.is_object() || sv.is_array()) continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_DIM, "%s", sk.c_str());
                ImGui::TableNextColumn();
                if (sv.is_number()) {
                    double val = sv.get<double>();
                    ImVec4 col = val < 0 ? theme::colors::MARKET_RED : ImVec4(1, 1, 1, 1);
                    ImGui::TextColored(col, "%.6g", val);
                } else {
                    ImGui::TextUnformatted(sv.dump().c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

void QuantStatsView::fetch_quantstats(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    try {
        nlohmann::json req;
        std::vector<std::string> symbols;
        std::vector<double> weights;
        for (const auto& h : summary.holdings) {
            symbols.push_back(h.asset.symbol);
            weights.push_back(h.weight / 100.0);
        }
        req["symbols"] = symbols;
        req["weights"] = weights;

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/quantstats_analysis.py",
            {"portfolio_stats"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) { qs_result_ = j; qs_loaded_ = true; }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("QuantStatsView", "QuantStats failed: %s", e.what());
    }
    loading_ = false;
}

void QuantStatsView::fetch_ffn(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    try {
        nlohmann::json req;
        std::vector<std::string> symbols;
        for (const auto& h : summary.holdings)
            symbols.push_back(h.asset.symbol);
        req["symbols"] = symbols;

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/ffn_analysis.py",
            {"run_ffn"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) { ffn_result_ = j; ffn_loaded_ = true; }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("QuantStatsView", "FFN failed: %s", e.what());
    }
    loading_ = false;
}

} // namespace fincept::portfolio
