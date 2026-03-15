// Analytics & Sectors Panel — Port of AnalyticsSectorsPanel.tsx (585 lines)
// 3 sub-tabs: Overview, Analytics, Correlation

#include "analytics_view.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <implot.h>

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
    std::string pf_name = summary.portfolio.name;
    std::string sym_key;
    for (const auto& h : summary.holdings) sym_key += h.asset.symbol + "_";

    std::thread([this, holdings, pf_name, sym_key]() {
        try {
            nlohmann::json req;
            req["holdings"] = holdings;
            req["portfolio_name"] = pf_name;

            auto& cache = CacheService::instance();
            std::string ck = CacheService::make_key("reference", "portfolio-analytics",
                sym_key.substr(0, std::min((size_t)64, sym_key.size())));

            std::string output;
            auto cached = cache.get(ck);
            if (cached && !cached->empty()) { output = *cached; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/portfolio_analytics.py",
                    {"calculate_portfolio_metrics"}, req.dump());
                if (result.success && !result.output.empty()) {
                    output = result.output;
                    // Only cache successful results
                    auto check = nlohmann::json::parse(output, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache.set(ck, output, "reference", CacheTTL::FIFTEEN_MIN);
                } else if (!result.output.empty()) {
                    output = result.output; // Still show error to user
                } else if (!result.error.empty()) {
                    output = nlohmann::json({{"error", result.error}}).dump();
                }
            }

            if (!output.empty()) {
                auto j = nlohmann::json::parse(output, nullptr, false);
                if (!j.is_discarded()) {
                    analytics_result_ = j;
                    analytics_loaded_ = true;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("AnalyticsView", "Failed to fetch analytics: %s", e.what());
        }
        analytics_loading_ = false;
    }).detach();
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

    // Check for error
    if (analytics_result_.is_object() && analytics_result_.contains("error")) {
        ImGui::TextColored(theme::colors::MARKET_RED, "Error: %s",
            analytics_result_["error"].get<std::string>().c_str());
        ImGui::Spacing();
        if (theme::SecondaryButton("Retry")) {
            analytics_loaded_ = false;
            fetch_analytics_data(summary);
        }
        return;
    }

    // Render analytics results — handle nested sections
    auto render_section = [](const char* label, const char* table_id,
                             const nlohmann::json& obj) {
        if (!obj.is_object() || obj.empty()) return;
        theme::SectionHeader(label);
        if (ImGui::BeginTable(table_id, 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 280);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            for (auto& [k, v] : obj.items()) {
                if (v.is_object() || v.is_array()) continue; // skip deep nesting
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                ImGui::TableNextColumn();
                if (v.is_number())
                    ImGui::Text("%.6f", v.get<double>());
                else if (v.is_string())
                    ImGui::TextUnformatted(v.get<std::string>().c_str());
                else if (v.is_null())
                    ImGui::TextColored(theme::colors::TEXT_DIM, "N/A");
                else
                    ImGui::TextUnformatted(v.dump().c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    };

    if (analytics_result_.is_object()) {
        // Render known sections
        if (analytics_result_.contains("portfolio_metrics"))
            render_section("Portfolio Metrics", "##pm",
                           analytics_result_["portfolio_metrics"]);

        if (analytics_result_.contains("risk_metrics"))
            render_section("Risk Metrics", "##rm",
                           analytics_result_["risk_metrics"]);

        if (analytics_result_.contains("capm_analysis"))
            render_section("CAPM Analysis", "##capm",
                           analytics_result_["capm_analysis"]);

        if (analytics_result_.contains("systematic_risk_analysis"))
            render_section("Systematic Risk", "##sysrisk",
                           analytics_result_["systematic_risk_analysis"]);

        if (analytics_result_.contains("correlation_analysis"))
            render_section("Correlation Analysis", "##corrfx",
                           analytics_result_["correlation_analysis"]);

        if (analytics_result_.contains("individual_betas"))
            render_section("Individual Betas", "##betas",
                           analytics_result_["individual_betas"]);

        // Render any remaining top-level scalars
        bool has_scalars = false;
        for (auto& [k, v] : analytics_result_.items()) {
            if (!v.is_object() && !v.is_array()) { has_scalars = true; break; }
        }
        if (has_scalars)
            render_section("Summary", "##a_summary", analytics_result_);
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

    std::vector<std::string> symbols;
    for (const auto& h : summary.holdings)
        symbols.push_back(h.asset.symbol);

    std::thread([this, symbols]() {
        try {
            nlohmann::json req;
            // fortitudo_service expects "returns" key — pass as comma-separated tickers
            std::string tickers_csv;
            for (size_t i = 0; i < symbols.size(); i++) {
                if (i > 0) tickers_csv += ",";
                tickers_csv += symbols[i];
            }
            req["returns"] = tickers_csv;

            auto& cache_c = CacheService::instance();
            std::string csym;
            for (const auto& s : symbols) csym += s + "_";
            std::string cck = CacheService::make_key("reference", "portfolio-correlation",
                csym.substr(0, std::min((size_t)64, csym.size())));

            std::string c_out;
            auto cached_c = cache_c.get(cck);
            if (cached_c && !cached_c->empty()) { c_out = *cached_c; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
                    {"covariance_analysis"}, req.dump());
                if (result.success && !result.output.empty()) {
                    c_out = result.output;
                    auto check = nlohmann::json::parse(c_out, nullptr, false);
                    if (!check.is_discarded() && check.value("success", false))
                        cache_c.set(cck, c_out, "reference", CacheTTL::FIFTEEN_MIN);
                } else if (!result.output.empty()) {
                    c_out = result.output;
                } else if (!result.error.empty()) {
                    c_out = nlohmann::json({{"success", false}, {"error", result.error}}).dump();
                }
            }

            if (!c_out.empty()) {
                auto j = nlohmann::json::parse(c_out, nullptr, false);
                if (!j.is_discarded()) {
                    correlation_result_ = j;
                    correlation_loaded_ = true;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("AnalyticsView", "Failed to fetch correlation: %s", e.what());
        }
        correlation_loading_ = false;
    }).detach();
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

    // Check for error
    if (correlation_result_.is_object() &&
        (correlation_result_.contains("error") ||
         (correlation_result_.contains("success") && !correlation_result_["success"].get<bool>()))) {
        std::string err = correlation_result_.contains("error")
            ? correlation_result_["error"].get<std::string>() : "Unknown error";
        ImGui::TextColored(theme::colors::MARKET_RED, "Error: %s", err.c_str());
        ImGui::Spacing();
        if (theme::SecondaryButton("Retry")) {
            correlation_loaded_ = false;
            fetch_correlation_data(summary);
        }
        return;
    }

    // Build asset list from result
    std::vector<std::string> assets;
    if (correlation_result_.contains("assets") && correlation_result_["assets"].is_array()) {
        for (const auto& s : correlation_result_["assets"])
            assets.push_back(s.get<std::string>());
    }
    int n = (int)assets.size();

    // Helper: parse flat "A / B" -> value dict into NxN grid (row-major)
    auto parse_matrix = [&](const nlohmann::json& flat,
                            std::vector<double>& data_flat) {
        data_flat.assign(n * n, 0.0);
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                std::string key = assets[r] + " / " + assets[c];
                if (flat.contains(key) && flat[key].is_number())
                    data_flat[r * n + c] = flat[key].get<double>();
            }
        }
    };

    // Helper: color for correlation value (-1 to +1)
    auto corr_color = [](double v) -> ImVec4 {
        if (v >= 0.7)  return ImVec4(0.1f, 0.85f, 0.3f, 1.0f);
        if (v >= 0.3)  return ImVec4(0.3f, 0.7f, 0.5f, 1.0f);
        if (v >= -0.3) return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        if (v >= -0.7) return ImVec4(0.9f, 0.6f, 0.3f, 1.0f);
        return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
    };

    if (correlation_result_.is_object() && n > 0) {

        // ====== 1. Correlation Heatmap (DrawList-based) ======
        if (correlation_result_.contains("correlation_matrix")) {
            std::vector<double> corr_data;
            parse_matrix(correlation_result_["correlation_matrix"], corr_data);

            theme::SectionHeader("Correlation Heatmap");

            // Draw a custom heatmap using DrawList for pixel-perfect control
            float cell_size = 70.0f;
            float label_w = 60.0f; // space for row/col labels
            float total_w = label_w + cell_size * n;
            float total_h = label_w + cell_size * n;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Column headers
            for (int c = 0; c < n; c++) {
                float cx = origin.x + label_w + c * cell_size + cell_size * 0.5f;
                float cy = origin.y + label_w * 0.5f;
                ImVec2 text_size = ImGui::CalcTextSize(assets[c].c_str());
                dl->AddText(ImVec2(cx - text_size.x * 0.5f, cy - text_size.y * 0.5f),
                    ImGui::ColorConvertFloat4ToU32(theme::colors::ACCENT),
                    assets[c].c_str());
            }

            // Rows
            for (int r = 0; r < n; r++) {
                // Row label
                float ry = origin.y + label_w + r * cell_size + cell_size * 0.5f;
                ImVec2 text_size = ImGui::CalcTextSize(assets[r].c_str());
                dl->AddText(ImVec2(origin.x + label_w - text_size.x - 4,
                                   ry - text_size.y * 0.5f),
                    ImGui::ColorConvertFloat4ToU32(theme::colors::ACCENT),
                    assets[r].c_str());

                for (int c = 0; c < n; c++) {
                    double val = corr_data[r * n + c];
                    float x0 = origin.x + label_w + c * cell_size;
                    float y0 = origin.y + label_w + r * cell_size;
                    float x1 = x0 + cell_size;
                    float y1 = y0 + cell_size;

                    // Color: lerp from red (-1) -> white (0) -> green (+1)
                    float t = (float)(val + 1.0) * 0.5f; // 0..1
                    ImVec4 cell_col;
                    if (t < 0.5f) {
                        float s = t * 2.0f; // 0..1 for red→white
                        cell_col = ImVec4(0.9f, 0.2f + 0.6f * s, 0.2f + 0.6f * s, 0.8f);
                    } else {
                        float s = (t - 0.5f) * 2.0f; // 0..1 for white→green
                        cell_col = ImVec4(0.8f - 0.6f * s, 0.8f, 0.8f - 0.5f * s, 0.8f);
                    }

                    // Filled cell
                    dl->AddRectFilled(ImVec2(x0 + 1, y0 + 1), ImVec2(x1 - 1, y1 - 1),
                        ImGui::ColorConvertFloat4ToU32(cell_col), 3.0f);

                    // Value text centered in cell
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%+.2f", val);
                    ImVec2 ts = ImGui::CalcTextSize(buf);
                    // Dark text for readability
                    dl->AddText(ImVec2(x0 + (cell_size - ts.x) * 0.5f,
                                       y0 + (cell_size - ts.y) * 0.5f),
                        IM_COL32(20, 20, 20, 255), buf);
                }
            }

            // Reserve space
            ImGui::Dummy(ImVec2(total_w, total_h));
            ImGui::Spacing();

            // Correlation table with clean formatting
            theme::SectionHeader("Correlation Values");
            int cols = n + 1;
            if (ImGui::BeginTable("##corr_tbl", cols,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 80);
                for (int c = 0; c < n; c++)
                    ImGui::TableSetupColumn(assets[c].c_str(), ImGuiTableColumnFlags_WidthFixed, 90);
                ImGui::TableHeadersRow();

                for (int r = 0; r < n; r++) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(theme::colors::ACCENT, "%s", assets[r].c_str());
                    for (int c = 0; c < n; c++) {
                        ImGui::TableNextColumn();
                        double val = corr_data[r * n + c];
                        ImVec4 col = corr_color(val);

                        // Color bar background
                        ImVec2 pos = ImGui::GetCursorScreenPos();
                        float col_w = ImGui::GetColumnWidth();
                        float row_h = ImGui::GetTextLineHeightWithSpacing();
                        float fill = (float)std::abs(val);
                        ImGui::GetWindowDrawList()->AddRectFilled(pos,
                            ImVec2(pos.x + col_w * fill, pos.y + row_h),
                            ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, 0.2f)));

                        ImGui::TextColored(col, "%+.3f", val);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
        }

        // ====== 2. Covariance Matrix (clean table, no "e" notation) ======
        if (correlation_result_.contains("covariance_matrix")) {
            std::vector<double> cov_data;
            parse_matrix(correlation_result_["covariance_matrix"], cov_data);

            theme::SectionHeader("Covariance Matrix");
            int cols = n + 1;
            if (ImGui::BeginTable("##cov_tbl", cols,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 80);
                for (int c = 0; c < n; c++)
                    ImGui::TableSetupColumn(assets[c].c_str(), ImGuiTableColumnFlags_WidthFixed, 110);
                ImGui::TableHeadersRow();

                for (int r = 0; r < n; r++) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(theme::colors::ACCENT, "%s", assets[r].c_str());
                    for (int c = 0; c < n; c++) {
                        ImGui::TableNextColumn();
                        double val = cov_data[r * n + c];
                        // Always show as decimal — no scientific notation
                        ImGui::Text("%.6f", val);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
        }
    }

    // ====== 3. Statistical Moments — table + volatility bar chart ======
    if (correlation_result_.contains("moments") && correlation_result_["moments"].is_object() && n > 0) {
        const auto& moments = correlation_result_["moments"];

        // Parse moments data
        std::vector<double> means, vols, skews, kurts;
        for (const auto& sym : assets) {
            auto get = [&](const char* metric) -> double {
                std::string key = sym + " " + metric;
                if (moments.contains(key) && moments[key].is_number())
                    return moments[key].get<double>();
                return 0.0;
            };
            means.push_back(get("Mean"));
            vols.push_back(get("Volatility"));
            skews.push_back(get("Skewness"));
            kurts.push_back(get("Kurtosis"));
        }

        // Volatility bar chart
        theme::SectionHeader("Volatility Comparison");
        std::vector<const char*> labels;
        for (const auto& a : assets) labels.push_back(a.c_str());
        std::vector<double> positions(n);
        for (int i = 0; i < n; i++) positions[i] = i;

        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.1f, 0.1f));
        if (ImPlot::BeginPlot("##vol_bars", ImVec2(-1, 200),
                ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes("", "Daily Volatility",
                ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines,
                ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisTicks(ImAxis_X1, positions.data(), n, labels.data());

            ImPlot::SetNextFillStyle(ImVec4(0.2f, 0.6f, 0.9f, 0.7f));
            ImPlot::PlotBars("Volatility", positions.data(), vols.data(), n, 0.6);

            ImPlot::EndPlot();
        }
        ImPlot::PopStyleVar();
        ImGui::Spacing();

        // Mean return bar chart
        theme::SectionHeader("Mean Daily Return");
        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.1f, 0.1f));
        if (ImPlot::BeginPlot("##mean_bars", ImVec2(-1, 200),
                ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes("", "Daily Return",
                ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines,
                ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisTicks(ImAxis_X1, positions.data(), n, labels.data());

            // Color bars by sign
            for (int i = 0; i < n; i++) {
                ImVec4 bar_col = means[i] >= 0
                    ? ImVec4(0.1f, 0.8f, 0.3f, 0.7f)
                    : ImVec4(0.9f, 0.2f, 0.2f, 0.7f);
                ImPlot::SetNextFillStyle(bar_col);
                char lbl[64];
                std::snprintf(lbl, sizeof(lbl), "##m%d", i);
                ImPlot::PlotBars(lbl, &positions[i], &means[i], 1, 0.6);
            }
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleVar();
        ImGui::Spacing();

        // Moments table
        theme::SectionHeader("Statistical Moments");
        if (ImGui::BeginTable("##moments", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Mean Return", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Volatility", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Skewness", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Kurtosis", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            for (int i = 0; i < n; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(theme::colors::ACCENT, "%s", assets[i].c_str());

                ImGui::TableNextColumn();
                ImGui::TextColored(means[i] >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED,
                    "%+.6f", means[i]);

                ImGui::TableNextColumn();
                ImGui::Text("%.6f", vols[i]);

                ImGui::TableNextColumn();
                ImGui::Text("%.4f", skews[i]);

                ImGui::TableNextColumn();
                ImGui::Text("%.2f", kurts[i]);
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    }

    // Summary
    if (correlation_result_.is_object()) {
        int n_obs = correlation_result_.value("n_observations", 0);
        int n_ast = correlation_result_.value("n_assets", 0);
        ImGui::TextColored(theme::colors::TEXT_DIM,
            "%d assets  |  %d observations  |  1-year lookback", n_ast, n_obs);
    }

    ImGui::Spacing();
    if (theme::SecondaryButton("Refresh")) {
        correlation_loaded_ = false;
        fetch_correlation_data(summary);
    }
}

} // namespace fincept::portfolio
