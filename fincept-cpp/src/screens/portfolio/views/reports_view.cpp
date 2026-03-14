// Reports & PME Panel — Port of ReportsPMEPanel.tsx
// Sub-tabs: Tax Report | PME Analysis

#include "reports_view.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::portfolio {

void ReportsView::render(const PortfolioSummary& summary) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        tax_loaded_ = pme_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"Tax Report", "PME Analysis"};
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
            if (sub_tab_ == 0) fetch_tax_report(summary);
            else fetch_pme(summary);
        }
    }

    ImGui::Separator();

    if (sub_tab_ == 0) {
        if (!tax_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to generate tax report (capital gains, wash sales, tax-loss harvesting)");
        } else if (tax_result_.is_object()) {
            theme::SectionHeader("Capital Gains Summary");
            if (ImGui::BeginTable("##tax", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, 250);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (auto& [k, v] : tax_result_.items()) {
                    if (v.is_object() || v.is_array()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                    ImGui::TableNextColumn();
                    if (v.is_number())
                        ImGui::Text("%.2f", v.get<double>());
                    else if (v.is_string())
                        ImGui::TextUnformatted(v.get<std::string>().c_str());
                    else
                        ImGui::TextUnformatted(v.dump().c_str());
                }
                ImGui::EndTable();
            }
        }
    } else {
        if (!pme_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to compute Public Market Equivalent (PME) analysis via pypme");
        } else if (pme_result_.is_object()) {
            theme::SectionHeader("PME Analysis Results");
            if (ImGui::BeginTable("##pme", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (auto& [k, v] : pme_result_.items()) {
                    if (v.is_object() || v.is_array()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                    ImGui::TableNextColumn();
                    if (v.is_number())
                        ImGui::Text("%.4f", v.get<double>());
                    else
                        ImGui::TextUnformatted(v.dump().c_str());
                }
                ImGui::EndTable();
            }
        }
    }
}

void ReportsView::fetch_tax_report(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    try {
        nlohmann::json req;
        auto txns = nlohmann::json::array();
        for (const auto& h : summary.holdings) {
            txns.push_back({
                {"symbol", h.asset.symbol},
                {"quantity", h.asset.quantity},
                {"avg_price", h.asset.avg_buy_price},
                {"current_price", h.current_price}
            });
        }
        req["holdings"] = txns;
        req["portfolio_name"] = summary.portfolio.name;

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/portfolio_analytics.py",
            {"tax_report"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) { tax_result_ = j; tax_loaded_ = true; }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ReportsView", "Tax report failed: %s", e.what());
    }
    loading_ = false;
}

void ReportsView::fetch_pme(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    try {
        nlohmann::json req;
        std::vector<std::string> symbols;
        for (const auto& h : summary.holdings)
            symbols.push_back(h.asset.symbol);
        req["symbols"] = symbols;
        req["portfolio_name"] = summary.portfolio.name;

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/portfolio_analytics.py",
            {"pme_analysis"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) { pme_result_ = j; pme_loaded_ = true; }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ReportsView", "PME analysis failed: %s", e.what());
    }
    loading_ = false;
}

} // namespace fincept::portfolio
