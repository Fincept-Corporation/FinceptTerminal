// Risk Management Panel — Port of RiskManagementPanel.tsx
// Sub-tabs: Overview | Stress & Decay | Fortitudo Advanced | Efficient Frontiers

#include "risk_view.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace fincept::portfolio {

// ============================================================================
// Main render
// ============================================================================

void RiskView::render(const PortfolioSummary& summary, const ComputedMetrics& metrics) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        overview_loaded_ = stress_loaded_ = fortitudo_loaded_ = frontier_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    // Sub-tab selector
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    const char* tabs[] = {"Risk Overview", "Stress & Decay", "Fortitudo Advanced", "Efficient Frontiers"};
    for (int i = 0; i < 4; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(150, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    // Run button
    ImGui::SameLine(0, 20);
    if (loading_) {
        theme::LoadingSpinner("Running...");
    } else {
        if (theme::AccentButton("Run Analysis")) {
            switch (sub_tab_) {
                case 0: fetch_overview(summary); break;
                case 1: fetch_stress(summary); break;
                case 2: fetch_fortitudo(summary); break;
                case 3: fetch_frontiers(summary); break;
            }
        }
    }

    ImGui::Separator();

    // Error display
    if (!error_.empty()) {
        ImGui::TextColored(theme::colors::MARKET_RED, "[Error] %s", error_.c_str());
        ImGui::Spacing();
    }

    // Content
    switch (sub_tab_) {
        case 0:
            if (!overview_loaded_) {
                ImGui::TextColored(theme::colors::TEXT_DIM,
                    "Click Run to compute comprehensive risk analysis");
            } else {
                render_json_section("Comprehensive Risk Analysis", overview_result_);
            }
            break;

        case 1:
            if (!stress_loaded_) {
                ImGui::TextColored(theme::colors::TEXT_DIM,
                    "Click Run to compute exponential decay weighting and covariance analysis");
            } else {
                render_json_section("Stress & Decay Analysis", stress_result_);
            }
            break;

        case 2:
            if (!fortitudo_loaded_) {
                ImGui::TextColored(theme::colors::TEXT_DIM,
                    "Click Run to compute entropy pooling and exposure stacking");
            } else {
                render_json_section("Fortitudo Advanced", fortitudo_result_);
            }
            break;

        case 3:
            if (!frontier_loaded_) {
                ImGui::TextColored(theme::colors::TEXT_DIM,
                    "Click Run to compute Mean-Variance and Mean-CVaR efficient frontiers");
            } else {
                render_json_section("Efficient Frontiers", frontier_result_);
            }
            break;
    }
}

// ============================================================================
// Python fetchers
// ============================================================================

static std::string build_symbols_csv(const PortfolioSummary& summary) {
    std::string csv;
    for (const auto& h : summary.holdings) {
        if (!csv.empty()) csv += ",";
        csv += h.asset.symbol;
    }
    return csv;
}

static nlohmann::json build_weights_array(const PortfolioSummary& summary) {
    auto arr = nlohmann::json::array();
    for (const auto& h : summary.holdings)
        arr.push_back(h.weight / 100.0);
    return arr;
}

void RiskView::fetch_overview(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    try {
        nlohmann::json req;
        req["symbols"] = build_symbols_csv(summary);
        req["weights"] = build_weights_array(summary);
        req["portfolio_value"] = summary.total_market_value;
        req["function"] = "comprehensive_risk_analysis";

        auto result = python::execute_with_stdin(
            "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
            {"comprehensive_risk_analysis"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                overview_result_ = j;
                overview_loaded_ = true;
            } else {
                error_ = "Failed to parse analysis results";
            }
        } else {
            error_ = result.error.empty() ? "Python script failed" : result.error;
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        LOG_ERROR("RiskView", "Overview fetch failed: %s", e.what());
    }
    loading_ = false;
}

void RiskView::fetch_stress(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    try {
        nlohmann::json req;
        req["symbols"] = build_symbols_csv(summary);
        req["half_life"] = 252;

        auto result = python::execute_with_stdin(
            "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
            {"exp_decay_and_covariance"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                stress_result_ = j;
                stress_loaded_ = true;
            }
        } else {
            error_ = result.error.empty() ? "Python script failed" : result.error;
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        LOG_ERROR("RiskView", "Stress fetch failed: %s", e.what());
    }
    loading_ = false;
}

void RiskView::fetch_fortitudo(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    try {
        nlohmann::json req;
        req["weights"] = build_weights_array(summary);
        req["n_scenarios"] = 1000;

        auto result = python::execute_with_stdin(
            "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
            {"entropy_pooling_and_stacking"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                fortitudo_result_ = j;
                fortitudo_loaded_ = true;
            }
        } else {
            error_ = result.error.empty() ? "Python script failed" : result.error;
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        LOG_ERROR("RiskView", "Fortitudo fetch failed: %s", e.what());
    }
    loading_ = false;
}

void RiskView::fetch_frontiers(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    try {
        nlohmann::json req;
        req["symbols"] = build_symbols_csv(summary);
        req["n_points"] = 20;
        req["alpha"] = 0.05;

        auto result = python::execute_with_stdin(
            "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
            {"efficient_frontiers"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                frontier_result_ = j;
                frontier_loaded_ = true;
            }
        } else {
            error_ = result.error.empty() ? "Python script failed" : result.error;
        }
    } catch (const std::exception& e) {
        error_ = e.what();
        LOG_ERROR("RiskView", "Frontier fetch failed: %s", e.what());
    }
    loading_ = false;
}

// ============================================================================
// Renderers
// ============================================================================

void RiskView::render_kv_table(const char* title, const nlohmann::json& data) {
    if (!data.is_object()) return;

    theme::SectionHeader(title);
    if (ImGui::BeginTable("##kv", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 250);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (auto& [key, val] : data.items()) {
            if (val.is_object() || val.is_array()) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(theme::colors::TEXT_DIM, "%s", key.c_str());
            ImGui::TableNextColumn();
            if (val.is_number()) {
                double v = val.get<double>();
                ImVec4 col = v < 0 ? theme::colors::MARKET_RED : theme::colors::ACCENT;
                ImGui::TextColored(col, "%.6g", v);
            } else if (val.is_string()) {
                ImGui::TextUnformatted(val.get<std::string>().c_str());
            } else if (val.is_boolean()) {
                ImGui::Text("%s", val.get<bool>() ? "Yes" : "No");
            } else {
                ImGui::TextUnformatted(val.dump().c_str());
            }
        }
        ImGui::EndTable();
    }
}

void RiskView::render_matrix_table(const char* title, const nlohmann::json& data) {
    if (!data.is_object()) return;

    // Extract unique asset names from "AAPL / MSFT" style keys
    std::vector<std::string> assets;
    for (auto& [key, _] : data.items()) {
        auto sep = key.find(" / ");
        if (sep != std::string::npos) {
            std::string a = key.substr(0, sep);
            std::string b = key.substr(sep + 3);
            if (std::find(assets.begin(), assets.end(), a) == assets.end()) assets.push_back(a);
            if (std::find(assets.begin(), assets.end(), b) == assets.end()) assets.push_back(b);
        }
    }
    if (assets.empty()) return;

    theme::SectionHeader(title);

    int ncols = static_cast<int>(assets.size()) + 1;
    if (ImGui::BeginTable("##matrix", ncols, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70);
        for (const auto& a : assets)
            ImGui::TableSetupColumn(a.c_str(), ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (const auto& row : assets) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(theme::colors::ACCENT, "%s", row.c_str());

            for (const auto& col : assets) {
                ImGui::TableNextColumn();
                std::string key1 = row + " / " + col;
                std::string key2 = col + " / " + row;
                double v = 0;
                if (data.contains(key1) && data[key1].is_number())
                    v = data[key1].get<double>();
                else if (data.contains(key2) && data[key2].is_number())
                    v = data[key2].get<double>();

                ImVec4 c = v < 0 ? theme::colors::MARKET_RED : theme::colors::MARKET_GREEN;
                if (row == col) c = ImVec4(1, 1, 1, 1);
                ImGui::TextColored(c, "%.4f", v);
            }
        }
        ImGui::EndTable();
    }
}

void RiskView::render_json_section(const char* title, const nlohmann::json& data) {
    if (!data.is_object()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No data");
        return;
    }

    // Check for matrix sub-objects
    if (data.contains("covariance_matrix") && data["covariance_matrix"].is_object()) {
        render_matrix_table("Covariance Matrix", data["covariance_matrix"]);
    }
    if (data.contains("correlation_matrix") && data["correlation_matrix"].is_object()) {
        render_matrix_table("Correlation Matrix", data["correlation_matrix"]);
    }

    // Check for weights sub-object
    if (data.contains("weights") && data["weights"].is_object()) {
        theme::SectionHeader("Portfolio Weights");
        if (ImGui::BeginTable("##weights", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Weight");
            ImGui::TableHeadersRow();

            for (auto& [sym, w] : data["weights"].items()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::ACCENT, "%s", sym.c_str());
                ImGui::TableNextColumn();
                double wv = w.is_number() ? w.get<double>() : 0;
                ImGui::Text("%.1f%%", wv * 100);
                // Simple bar
                ImGui::SameLine();
                float bar_w = static_cast<float>(wv) * 200.0f;
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    p, ImVec2(p.x + bar_w, p.y + ImGui::GetTextLineHeight()),
                    IM_COL32(0, 180, 220, 120));
                ImGui::Dummy(ImVec2(bar_w, 0));
            }
            ImGui::EndTable();
        }
    }

    // Check for frontier array
    if (data.contains("frontier") && data["frontier"].is_array()) {
        theme::SectionHeader("Efficient Frontier");
        auto& frontier = data["frontier"];
        if (ImGui::BeginTable("##frontier", 4,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                ImVec2(0, 250))) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("Exp Return");
            ImGui::TableSetupColumn("Risk");
            ImGui::TableSetupColumn("Sharpe");
            ImGui::TableHeadersRow();

            int idx = 1;
            for (const auto& pt : frontier) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d", idx++);
                ImGui::TableNextColumn();
                double ret = pt.value("expected_return", 0.0);
                ImGui::TextColored(ret >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED,
                    "%.6g", ret);
                ImGui::TableNextColumn();
                double vol = pt.value("volatility", pt.value("cvar", 0.0));
                ImGui::Text("%.6g", vol);
                ImGui::TableNextColumn();
                double sharpe = pt.value("sharpe_ratio", 0.0);
                ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "%.4f", sharpe);
            }
            ImGui::EndTable();
        }
    }

    // Render top-level scalar fields as KV table
    bool has_scalars = false;
    for (auto& [k, v] : data.items()) {
        if (k == "covariance_matrix" || k == "correlation_matrix" || k == "weights"
            || k == "frontier" || k == "moments" || k == "assets") continue;
        if (!v.is_object() && !v.is_array()) { has_scalars = true; break; }
    }

    if (has_scalars) {
        nlohmann::json scalars;
        for (auto& [k, v] : data.items()) {
            if (k == "covariance_matrix" || k == "correlation_matrix" || k == "weights"
                || k == "frontier" || k == "moments" || k == "assets") continue;
            if (!v.is_object() && !v.is_array()) scalars[k] = v;
        }
        render_kv_table(title, scalars);
    }

    // Render nested objects recursively (one level)
    for (auto& [k, v] : data.items()) {
        if (k == "covariance_matrix" || k == "correlation_matrix" || k == "weights"
            || k == "frontier" || k == "moments" || k == "assets") continue;
        if (v.is_object()) {
            render_kv_table(k.c_str(), v);
        }
    }
}

} // namespace fincept::portfolio
