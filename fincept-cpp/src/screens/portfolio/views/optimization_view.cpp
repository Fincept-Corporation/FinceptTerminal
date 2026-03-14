// Portfolio Optimization — PyPortfolioOpt integration
// 9 optimization strategies via Python

#include "optimization_view.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::portfolio {

void OptimizationView::render(const PortfolioSummary& summary) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        for (int i = 0; i < NUM_TABS; i++) loaded_[i] = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    // Tab bar — scrollable since we have 9 tabs
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    for (int i = 0; i < NUM_TABS; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::SmallButton(TAB_LABELS[i])) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 20);
    if (loading_) {
        theme::LoadingSpinner("Optimizing...");
    } else {
        if (theme::AccentButton("Run")) {
            fetch_optimization(summary, sub_tab_);
        }
    }

    ImGui::Separator();

    if (!error_.empty()) {
        ImGui::TextColored(theme::colors::MARKET_RED, "[Error] %s", error_.c_str());
        ImGui::Spacing();
    }

    if (!loaded_[sub_tab_]) {
        ImGui::TextColored(theme::colors::TEXT_DIM,
            "Click Run to optimize portfolio using %s strategy", TAB_LABELS[sub_tab_]);
    } else {
        render_result(results_[sub_tab_]);
    }
}

void OptimizationView::fetch_optimization(const PortfolioSummary& summary, int tab) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    try {
        nlohmann::json req;
        std::vector<std::string> symbols;
        for (const auto& h : summary.holdings)
            symbols.push_back(h.asset.symbol);
        req["symbols"] = symbols;

        const char* strategies[] = {
            "max_sharpe", "min_volatility", "efficient_risk", "efficient_return",
            "max_quadratic_utility", "risk_parity", "black_litterman",
            "hrp", "custom_constraints"
        };
        req["strategy"] = strategies[tab];

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/portfolio_optimization.py",
            {"optimize"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                results_[tab] = j;
                loaded_[tab] = true;
            } else {
                error_ = "Failed to parse optimization results";
            }
        } else {
            error_ = result.error.empty() ? "Optimization failed" : result.error;
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        LOG_ERROR("OptimizationView", "Optimization failed: %s", e.what());
    }
    loading_ = false;
}

void OptimizationView::render_result(const nlohmann::json& result) {
    if (!result.is_object()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No results");
        return;
    }

    // Weights section
    if (result.contains("weights") && result["weights"].is_object()) {
        theme::SectionHeader("Optimal Weights");
        if (ImGui::BeginTable("##opt_weights", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Allocation");
            ImGui::TableHeadersRow();

            for (auto& [sym, w] : result["weights"].items()) {
                double wv = w.is_number() ? w.get<double>() : 0;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::ACCENT, "%s", sym.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.1f%%", wv * 100);
                ImGui::TableNextColumn();
                ImGui::ProgressBar(static_cast<float>(wv), ImVec2(-1, 14), "");
            }
            ImGui::EndTable();
        }
    }

    // Performance metrics
    theme::SectionHeader("Performance Metrics");
    if (ImGui::BeginTable("##opt_metrics", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (auto& [k, v] : result.items()) {
            if (k == "weights" || v.is_object() || v.is_array()) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
            ImGui::TableNextColumn();
            if (v.is_number()) {
                double val = v.get<double>();
                ImVec4 col = (k.find("return") != std::string::npos || k.find("sharpe") != std::string::npos)
                    ? (val >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED)
                    : ImVec4(1, 1, 1, 1);
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

} // namespace fincept::portfolio
