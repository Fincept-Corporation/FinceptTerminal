// Analytics & Sectors Panel — Port of AnalyticsSectorsPanel.tsx (585 lines)
// 3 sub-tabs: Overview, Analytics, Correlation

#include "analytics_view.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace fincept::portfolio {

void AnalyticsView::render(const PortfolioSummary& summary, const ComputedMetrics& metrics) {
    // Invalidate cache if portfolio changed
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        analytics_loaded_ = false;
        correlation_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    // Sub-tab selector
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"Overview", "Analytics", "Correlation"};
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(120, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    switch (sub_tab_) {
        case 0: render_overview(summary, metrics); break;
        case 1: render_analytics(summary); break;
        case 2: render_correlation(summary); break;
    }
}

// ============================================================================
// Overview — metrics + gainers/losers
// ============================================================================

void AnalyticsView::render_overview(const PortfolioSummary& summary, const ComputedMetrics& metrics) {
    // Metrics row
    ImGui::Spacing();
    theme::SectionHeader("Risk Metrics");

    if (ImGui::BeginTable("##metrics_grid", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        auto metric_row = [](const char* l1, const char* v1, const char* l2, const char* v2) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", l1);
            ImGui::TableNextColumn(); ImGui::Text("%s", v1);
            ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", l2);
            ImGui::TableNextColumn(); ImGui::Text("%s", v2);
        };

        char buf1[32], buf2[32];
        std::snprintf(buf1, sizeof(buf1), "%.2f", metrics.sharpe_ratio);
        std::snprintf(buf2, sizeof(buf2), "%.2f", metrics.beta);
        metric_row("Sharpe Ratio", buf1, "Beta (vs SPY)", buf2);

        std::snprintf(buf1, sizeof(buf1), "%.1f%%", metrics.volatility);
        std::snprintf(buf2, sizeof(buf2), "%.1f%%", metrics.max_drawdown);
        metric_row("Volatility (ann.)", buf1, "Max Drawdown", buf2);

        std::snprintf(buf1, sizeof(buf1), "%.1f%%", metrics.var_95);
        std::snprintf(buf2, sizeof(buf2), "%.0f / 100", metrics.risk_score);
        metric_row("VaR (95%)", buf1, "Risk Score", buf2);

        std::snprintf(buf1, sizeof(buf1), "%.4f", metrics.concentration);
        metric_row("Concentration (HHI)", buf1, "", "");

        ImGui::EndTable();
    }

    // Top gainers / losers
    ImGui::Spacing();
    float half_w = ImGui::GetContentRegionAvail().x * 0.5f - 4;

    // Sort holdings by P&L%
    auto sorted = summary.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const HoldingWithQuote& a, const HoldingWithQuote& b) {
        return a.unrealized_pnl_percent > b.unrealized_pnl_percent;
    });

    ImGui::BeginChild("##gainers", ImVec2(half_w, 200), ImGuiChildFlags_Borders);
    theme::SectionHeader("Top Gainers");
    for (size_t i = 0; i < std::min<size_t>(5, sorted.size()); i++) {
        if (sorted[i].unrealized_pnl_percent <= 0) break;
        ImGui::TextColored(theme::colors::MARKET_GREEN, "  %s: %+.1f%%",
            sorted[i].asset.symbol.c_str(), sorted[i].unrealized_pnl_percent);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##losers", ImVec2(half_w, 200), ImGuiChildFlags_Borders);
    theme::SectionHeader("Top Losers");
    for (size_t i = sorted.size(); i > 0; i--) {
        if (sorted[i - 1].unrealized_pnl_percent >= 0) break;
        ImGui::TextColored(theme::colors::MARKET_RED, "  %s: %+.1f%%",
            sorted[i - 1].asset.symbol.c_str(), sorted[i - 1].unrealized_pnl_percent);
    }
    ImGui::EndChild();

    // Allocation table
    ImGui::Spacing();
    theme::SectionHeader("Allocation");
    if (ImGui::BeginTable("##allocation", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Weight");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("P&L");
        ImGui::TableHeadersRow();

        for (const auto& h : summary.holdings) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(h.asset.symbol.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.1f%%", h.weight);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", h.market_value);
            ImVec4 col = h.unrealized_pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(col, "%+.2f", h.unrealized_pnl);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Analytics — Python integration
// ============================================================================

void AnalyticsView::fetch_analytics_data(const PortfolioSummary& summary) {
    if (analytics_loading_) return;
    analytics_loading_ = true;

    try {
        // Build holdings JSON for Python scripts
        nlohmann::json holdings = nlohmann::json::array();
        for (const auto& h : summary.holdings) {
            holdings.push_back({
                {"symbol", h.asset.symbol},
                {"quantity", h.asset.quantity},
                {"avg_price", h.asset.avg_buy_price},
                {"current_price", h.current_price},
                {"weight", h.weight / 100.0}
            });
        }

        nlohmann::json req;
        req["holdings"] = holdings;
        req["portfolio_name"] = summary.portfolio.name;

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/portfolio_analytics.py",
            {"calculate_portfolio_metrics"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                analytics_result_ = j;
                analytics_loaded_ = true;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("AnalyticsView", "Failed to fetch analytics: %s", e.what());
    }
    analytics_loading_ = false;
}

void AnalyticsView::render_analytics(const PortfolioSummary& summary) {
    if (!analytics_loaded_ && !analytics_loading_) {
        if (theme::AccentButton("Run Analytics")) {
            fetch_analytics_data(summary);
        }
        ImGui::SameLine();
        ImGui::TextColored(theme::colors::TEXT_DIM,
            "Click to run portfolio analytics via Python");
        return;
    }

    if (analytics_loading_) {
        theme::LoadingSpinner("Running analytics...");
        return;
    }

    // Render analytics results as key-value table
    theme::SectionHeader("Portfolio Analytics Results");

    if (analytics_result_.is_object()) {
        if (ImGui::BeginTable("##analytics_kv", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            for (auto& [key, val] : analytics_result_.items()) {
                if (val.is_object() || val.is_array()) continue; // skip nested
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", key.c_str());
                ImGui::TableNextColumn();
                if (val.is_number())
                    ImGui::Text("%.4f", val.get<double>());
                else if (val.is_string())
                    ImGui::TextUnformatted(val.get<std::string>().c_str());
                else
                    ImGui::TextUnformatted(val.dump().c_str());
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    if (theme::SecondaryButton("Refresh")) {
        analytics_loaded_ = false;
        fetch_analytics_data(summary);
    }
}

// ============================================================================
// Correlation — Python integration
// ============================================================================

void AnalyticsView::fetch_correlation_data(const PortfolioSummary& summary) {
    if (correlation_loading_) return;
    correlation_loading_ = true;

    try {
        nlohmann::json req;
        std::vector<std::string> symbols;
        for (const auto& h : summary.holdings)
            symbols.push_back(h.asset.symbol);
        req["symbols"] = symbols;

        auto result = python::execute_with_stdin(
            "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
            {"covariance_analysis"}, req.dump());

        if (result.success && !result.output.empty()) {
            auto j = nlohmann::json::parse(result.output, nullptr, false);
            if (!j.is_discarded()) {
                correlation_result_ = j;
                correlation_loaded_ = true;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("AnalyticsView", "Failed to fetch correlation: %s", e.what());
    }
    correlation_loading_ = false;
}

void AnalyticsView::render_correlation(const PortfolioSummary& summary) {
    if (!correlation_loaded_ && !correlation_loading_) {
        if (theme::AccentButton("Run Correlation Analysis")) {
            fetch_correlation_data(summary);
        }
        ImGui::SameLine();
        ImGui::TextColored(theme::colors::TEXT_DIM,
            "Click to compute correlation matrix via Fortitudo");
        return;
    }

    if (correlation_loading_) {
        theme::LoadingSpinner("Computing correlations...");
        return;
    }

    theme::SectionHeader("Correlation / Covariance Matrix");

    if (correlation_result_.is_object()) {
        // Render as key-value or matrix depending on shape
        if (ImGui::BeginTable("##corr_kv", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 250);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            for (auto& [key, val] : correlation_result_.items()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", key.c_str());
                ImGui::TableNextColumn();
                if (val.is_number())
                    ImGui::Text("%.6f", val.get<double>());
                else if (val.is_string())
                    ImGui::TextWrapped("%s", val.get<std::string>().c_str());
                else
                    ImGui::TextWrapped("%s", val.dump(2).c_str());
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    if (theme::SecondaryButton("Refresh")) {
        correlation_loaded_ = false;
        fetch_correlation_data(summary);
    }
}

} // namespace fincept::portfolio
