// Performance & Risk Panel — Port of PerformanceRiskPanel.tsx
// NAV chart (ImPlot), risk metric cards, daily returns table, risk recommendations

#include "performance_view.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <implot.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <set>
#include <thread>

namespace fincept::portfolio {

static constexpr double RISK_FREE_RATE = 0.04;

void PerformanceView::render(const PortfolioSummary& summary, const ComputedMetrics& metrics) {
    // Cache invalidation
    if (loaded_for_portfolio_ != summary.portfolio.id || loaded_for_period_ != period_idx_) {
        data_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
        loaded_for_period_ = period_idx_;
    }

    // Auto-fetch on first render
    if (!data_loaded_ && !data_loading_ && !summary.holdings.empty()) {
        fetch_historical(summary);
    }

    render_period_bar(metrics);
    ImGui::Separator();

    // Two-column layout: left (chart + risk cards) | right (daily returns)
    float avail_w = ImGui::GetContentRegionAvail().x;
    float left_w = avail_w * 0.55f - 4;
    float right_w = avail_w - left_w - 8;

    ImGui::BeginChild("##perf_left", ImVec2(left_w, 0), ImGuiChildFlags_None);
    render_nav_chart();
    ImGui::Spacing();
    render_risk_cards(metrics);
    ImGui::Spacing();
    render_recommendations(metrics);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##perf_right", ImVec2(right_w, 0), ImGuiChildFlags_Borders);
    render_daily_returns();
    ImGui::EndChild();
}

// ============================================================================
// Period selector bar with key metrics
// ============================================================================

void PerformanceView::render_period_bar(const ComputedMetrics& metrics) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(theme::colors::TEXT_DIM, "PERIOD");
    ImGui::SameLine();

    for (int i = 0; i < 5; i++) {
        ImGui::SameLine();
        bool active = (period_idx_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::SmallButton(PERIOD_LABELS[i])) {
            if (period_idx_ != i) {
                period_idx_ = i;
                data_loaded_ = false;
            }
        }
        if (active) ImGui::PopStyleColor();
    }

    if (data_loading_) {
        ImGui::SameLine();
        theme::LoadingSpinner("Loading...");
    }

    ImGui::PopStyleVar();

    // Key metrics chips on same line
    if (data_loaded_ && nav_values_.size() >= 2) {
        ImGui::SameLine(0, 20);
        ImVec4 ret_col = period_return_ >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
        ImGui::TextColored(theme::colors::TEXT_DIM, "RTN");
        ImGui::SameLine();
        ImGui::TextColored(ret_col, "%+.2f%%", period_return_);

        ImGui::SameLine(0, 12);
        ImGui::TextColored(theme::colors::TEXT_DIM, "SHARPE");
        ImGui::SameLine();
        ImVec4 sharpe_col = metrics.sharpe_ratio > 1 ? theme::colors::MARKET_GREEN
                          : metrics.sharpe_ratio > 0 ? ImVec4(1, 0.8f, 0, 1) : theme::colors::MARKET_RED;
        ImGui::TextColored(sharpe_col, "%.2f", metrics.sharpe_ratio);

        ImGui::SameLine(0, 12);
        ImGui::TextColored(theme::colors::TEXT_DIM, "BETA");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "%.2f", metrics.beta);

        ImGui::SameLine(0, 12);
        ImGui::TextColored(theme::colors::TEXT_DIM, "VOL");
        ImGui::SameLine();
        ImVec4 vol_col = metrics.volatility < 15 ? theme::colors::MARKET_GREEN
                       : metrics.volatility < 25 ? ImVec4(1, 0.8f, 0, 1) : theme::colors::MARKET_RED;
        ImGui::TextColored(vol_col, "%.1f%%", metrics.volatility);

        ImGui::SameLine(0, 12);
        ImGui::TextColored(theme::colors::TEXT_DIM, "DD");
        ImGui::SameLine();
        ImGui::TextColored(metrics.max_drawdown > 10 ? theme::colors::MARKET_RED : theme::colors::TEXT_DIM,
            "-%.1f%%", metrics.max_drawdown);

        ImGui::SameLine(0, 12);
        ImGui::TextColored(theme::colors::TEXT_DIM, "%d days", trading_days_);
    }
}

// ============================================================================
// Fetch historical data via Python
// ============================================================================

void PerformanceView::fetch_historical(const PortfolioSummary& summary) {
    if (data_loading_) return;
    data_loading_ = true;
    error_msg_.clear();

    // Capture holdings data by value for the thread
    struct HoldingInfo { std::string symbol; double quantity; };
    std::vector<HoldingInfo> holdings_copy;
    std::vector<std::string> symbols;
    for (const auto& h : summary.holdings) {
        holdings_copy.push_back({h.asset.symbol, h.asset.quantity});
        symbols.push_back(h.asset.symbol);
    }
    symbols.push_back("SPY");
    int period = period_idx_;

    std::thread([this, holdings_copy, symbols, period]() {
        try {
            nlohmann::json req;
            req["symbols"] = symbols;

            const char* yf_periods[] = {"1mo", "3mo", "6mo", "1y", "5y"};
            req["period"] = yf_periods[period];
            req["interval"] = "1d";

            auto& cache = CacheService::instance();
            std::string sym_key;
            for (const auto& s : symbols) sym_key += s + "_";
            std::string cache_key = CacheService::make_key("market-quotes", "perf-hist",
                sym_key + yf_periods[period]);

            std::string output;
            auto cached = cache.get(cache_key);
            if (cached && !cached->empty()) {
                output = *cached;
            } else {
                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/fetch_historical.py",
                    {}, req.dump());
                if (result.success && !result.output.empty()) {
                    output = result.output;
                    // Only cache if valid JSON and no error
                    auto check = nlohmann::json::parse(output, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache.set(cache_key, output, "market-quotes", CacheTTL::FIFTEEN_MIN);
                } else if (!result.error.empty()) {
                    error_msg_ = result.error;
                    data_loading_ = false;
                    return;
                }
            }

            if (!output.empty()) {
                auto j = nlohmann::json::parse(output, nullptr, false);
                if (!j.is_discarded() && j.is_object()) {
                    if (j.contains("error")) {
                        error_msg_ = j["error"].get<std::string>();
                        data_loading_ = false;
                        return;
                    }
                    std::map<std::string, std::map<std::string, double>> sym_data;

                    for (const auto& sym : symbols) {
                        if (!j.contains(sym)) continue;
                        auto& sd = j[sym];
                        if (sd.is_array()) {
                            // Format: [{"date": "2024-01-01", "close": 150.0}, ...]
                            for (const auto& rec : sd) {
                                if (rec.contains("date") && rec.contains("close")) {
                                    sym_data[sym][rec["date"].get<std::string>()] = rec["close"].get<double>();
                                }
                            }
                        } else if (sd.is_object() && sd.contains("dates") && sd.contains("closes")) {
                            // Legacy format: {"dates": [...], "closes": [...]}
                            auto& dates = sd["dates"];
                            auto& closes = sd["closes"];
                            for (size_t i = 0; i < dates.size() && i < closes.size(); i++) {
                                sym_data[sym][dates[i].get<std::string>()] = closes[i].get<double>();
                            }
                        }
                    }

                    std::set<std::string> all_dates;
                    for (const auto& [sym, dmap] : sym_data) {
                        if (sym == "SPY") continue;
                        for (const auto& [d, _] : dmap) all_dates.insert(d);
                    }

                    std::vector<std::string> sorted_dates(all_dates.begin(), all_dates.end());
                    std::sort(sorted_dates.begin(), sorted_dates.end());

                    nav_values_.clear();
                    nav_times_.clear();
                    nav_dates_.clear();

                    for (size_t di = 0; di < sorted_dates.size(); di++) {
                        const auto& date = sorted_dates[di];
                        double nav = 0;
                        bool has_any = false;

                        for (const auto& h : holdings_copy) {
                            auto sit = sym_data.find(h.symbol);
                            if (sit == sym_data.end()) continue;
                            auto dit = sit->second.find(date);
                            if (dit != sit->second.end()) {
                                nav += h.quantity * dit->second;
                                has_any = true;
                            }
                        }

                        if (has_any) {
                            nav_values_.push_back(nav);
                            nav_times_.push_back(static_cast<double>(di));
                            nav_dates_.push_back(date);
                        }
                    }

                    bench_returns_.clear();
                    auto spy_it = sym_data.find("SPY");
                    if (spy_it != sym_data.end()) {
                        std::vector<double> spy_closes;
                        for (const auto& date : sorted_dates) {
                            auto dit = spy_it->second.find(date);
                            if (dit != spy_it->second.end())
                                spy_closes.push_back(dit->second);
                        }
                        for (size_t i = 1; i < spy_closes.size(); i++) {
                            if (spy_closes[i - 1] > 0)
                                bench_returns_.push_back((spy_closes[i] - spy_closes[i - 1]) / spy_closes[i - 1]);
                        }
                    }

                    if (nav_values_.size() >= 2) {
                        start_nav_ = nav_values_.front();
                        current_nav_ = nav_values_.back();
                        period_return_ = start_nav_ > 0 ? ((current_nav_ - start_nav_) / start_nav_) * 100.0 : 0.0;
                        trading_days_ = static_cast<int>(nav_values_.size());
                    }

                    data_loaded_ = true;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("PerformanceView", "Failed to fetch historical: %s", e.what());
        }
        data_loading_ = false;
    }).detach();
}

// ============================================================================
// NAV Chart using ImPlot
// ============================================================================

void PerformanceView::render_nav_chart() {
    theme::SectionHeader("Portfolio NAV");

    if (!data_loaded_ || nav_values_.size() < 2) {
        if (data_loading_) {
            theme::LoadingSpinner("Loading historical data...");
        } else if (!error_msg_.empty()) {
            ImGui::TextColored(theme::colors::MARKET_RED, "Error: %s", error_msg_.c_str());
        } else {
            ImGui::TextColored(theme::colors::TEXT_DIM, "No historical data available.");
        }
        return;
    }

    // Stats above chart
    ImVec4 ret_col = period_return_ >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
    ImGui::TextColored(theme::colors::TEXT_DIM, "CURRENT NAV");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "$%.2f", current_nav_);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(theme::colors::TEXT_DIM, "START NAV");
    ImGui::SameLine();
    ImGui::Text("$%.2f", start_nav_);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(theme::colors::TEXT_DIM, "RETURN");
    ImGui::SameLine();
    ImGui::TextColored(ret_col, "%+.2f%%", period_return_);

    // ImPlot line chart
    float chart_h = 180;
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.05f, 0.1f));

    if (ImPlot::BeginPlot("##nav_chart", ImVec2(-1, chart_h), ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Day", "NAV ($)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

        // Color based on return
        ImVec4 line_col = period_return_ >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
        ImVec4 fill_col = ImVec4(line_col.x, line_col.y, line_col.z, 0.15f);

        ImPlot::PushStyleColor(ImPlotCol_Line, line_col);
        ImPlot::PushStyleColor(ImPlotCol_Fill, fill_col);
        ImPlot::PlotShaded("NAV", nav_times_.data(), nav_values_.data(),
            static_cast<int>(nav_values_.size()));
        ImPlot::PlotLine("NAV", nav_times_.data(), nav_values_.data(),
            static_cast<int>(nav_values_.size()));
        ImPlot::PopStyleColor(2);

        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
}

// ============================================================================
// Risk metric cards
// ============================================================================

void PerformanceView::render_risk_cards(const ComputedMetrics& metrics) {
    theme::SectionHeader("Risk Metrics");

    if (ImGui::BeginTable("##risk_cards", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Description");
        ImGui::TableHeadersRow();

        auto risk_row = [](const char* label, const char* value, ImVec4 color, const char* desc) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "%s", label);
            ImGui::TableNextColumn();
            ImGui::TextColored(color, "%s", value);
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "%s", desc);
        };

        char buf[64];
        ImVec4 col;

        // Sharpe
        std::snprintf(buf, sizeof(buf), "%.2f", metrics.sharpe_ratio);
        col = metrics.sharpe_ratio > 1 ? theme::colors::MARKET_GREEN
            : metrics.sharpe_ratio > 0 ? ImVec4(1, 0.8f, 0, 1) : theme::colors::MARKET_RED;
        risk_row("SHARPE RATIO", buf, col, "Risk-adj. return (ann.)");

        // Beta
        std::snprintf(buf, sizeof(buf), "%.2f", metrics.beta);
        col = metrics.beta < 0.8 ? ImVec4(0.3f, 0.5f, 1, 1)
            : metrics.beta < 1.2 ? ImVec4(0, 0.8f, 1, 1) : ImVec4(1, 0.6f, 0, 1);
        risk_row("BETA vs SPY", buf, col, "Market sensitivity");

        // Volatility
        std::snprintf(buf, sizeof(buf), "%.1f%%", metrics.volatility);
        col = metrics.volatility < 15 ? theme::colors::MARKET_GREEN
            : metrics.volatility < 25 ? ImVec4(1, 0.8f, 0, 1) : theme::colors::MARKET_RED;
        risk_row("VOLATILITY", buf, col, "Annualized std dev");

        // Max Drawdown
        std::snprintf(buf, sizeof(buf), "-%.1f%%", metrics.max_drawdown);
        col = metrics.max_drawdown > 20 ? theme::colors::MARKET_RED
            : metrics.max_drawdown > 10 ? ImVec4(1, 0.6f, 0, 1) : theme::colors::MARKET_GREEN;
        risk_row("MAX DRAWDOWN", buf, col, "Peak-to-trough");

        // VaR
        std::snprintf(buf, sizeof(buf), "%.1f%%", metrics.var_95);
        risk_row("VaR (95%)", buf, ImVec4(1, 0.8f, 0, 1), "1-day 95% confidence");

        // Risk Score
        std::snprintf(buf, sizeof(buf), "%.0f / 100", metrics.risk_score);
        col = metrics.risk_score > 70 ? theme::colors::MARKET_RED
            : metrics.risk_score > 40 ? ImVec4(1, 0.8f, 0, 1) : theme::colors::MARKET_GREEN;
        risk_row("RISK SCORE", buf, col, "Vol + concentration");

        ImGui::EndTable();
    }
}

// ============================================================================
// Daily returns table
// ============================================================================

void PerformanceView::render_daily_returns() {
    theme::SectionHeader("Daily Returns");

    if (!data_loaded_ || nav_values_.size() < 2) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No data for period");
        return;
    }

    ImGui::Text("%d trading days", trading_days_);

    if (ImGui::BeginTable("##daily_ret", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("NAV", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Chg %", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();
        ImGui::TableSetupScrollFreeze(0, 1);

        // Show newest first
        for (int i = static_cast<int>(nav_values_.size()) - 1; i >= 1; i--) {
            double change = nav_values_[i] - nav_values_[i - 1];
            double change_pct = nav_values_[i - 1] > 0
                ? (change / nav_values_[i - 1]) * 100.0 : 0.0;

            ImVec4 col = change >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (i < static_cast<int>(nav_dates_.size()))
                ImGui::TextUnformatted(nav_dates_[i].c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "$%.2f", nav_values_[i]);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%+.2f", change);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%+.2f%%", change_pct);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Risk recommendations
// ============================================================================

void PerformanceView::render_recommendations(const ComputedMetrics& metrics) {
    // Only show if we have data
    if (!data_loaded_) return;

    struct Rec { bool warn; const char* text; };
    std::vector<Rec> recs;

    if (metrics.sharpe_ratio < 1 && metrics.sharpe_ratio != 0)
        recs.push_back({true, "Low Sharpe — consider diversifying for better risk-adjusted return"});
    if (metrics.sharpe_ratio > 2)
        recs.push_back({false, "Excellent Sharpe — strong risk-adjusted performance"});
    if (metrics.beta > 1.5)
        recs.push_back({true, "High Beta — more volatile than market, consider defensive assets"});
    if (metrics.volatility > 25)
        recs.push_back({true, "High Volatility — consider adding bonds or low-vol assets"});
    if (metrics.max_drawdown > 20)
        recs.push_back({true, "Large Drawdown — review stop-loss and position sizing"});
    if (metrics.beta >= 0.8 && metrics.beta <= 1.2)
        recs.push_back({false, "Balanced Beta — good market correlation"});

    if (recs.empty()) return;

    theme::SectionHeader("Risk Recommendations");
    for (const auto& r : recs) {
        ImVec4 col = r.warn ? theme::colors::MARKET_RED : theme::colors::MARKET_GREEN;
        const char* icon = r.warn ? "[!]" : "[ok]";
        ImGui::TextColored(col, "%s %s", icon, r.text);
    }
}

} // namespace fincept::portfolio
