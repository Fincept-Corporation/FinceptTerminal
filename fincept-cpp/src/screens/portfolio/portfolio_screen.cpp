// Portfolio Screen — Investment tracking + Paper Trading UI
// Dense Bloomberg-style ImGui layout

#include "portfolio_screen.h"
#include "portfolio/portfolio_service.h"
#include "trading/paper_trading.h"
#include "trading/order_matcher.h"
#include "python/python_runner.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <fstream>

namespace fincept::portfolio {

// ============================================================================
// Main render
// ============================================================================

void PortfolioScreen::render() {
    // Auto-refresh timer
    if (!paper_trading_mode_) {
        price_fetch_timer_ += ImGui::GetIO().DeltaTime;
        if (price_fetch_timer_ >= auto_refresh_interval_) {
            price_fetch_timer_ = 0.0f;
            needs_refresh_ = true;
        }
    }

    if (needs_refresh_ || pt_needs_refresh_) {
        if (!paper_trading_mode_) refresh_data();
        else refresh_pt_data();
    }

    // Use ScreenFrame like other tabs — positions window in viewport work area
    ui::ScreenFrame frame("##portfolio", ImVec2(8, 8), theme::colors::BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    // Yoga vertical stack: command_bar(36) + stats(48) + content(flex) + status_bar(frame_height)
    float status_h = ImGui::GetFrameHeightWithSpacing();
    auto vstack = ui::vstack_layout(frame.width(), frame.height(), {36, 48, -1, status_h});

    render_command_bar();
    render_stats_ribbon();

    // Main content area — Yoga-computed height
    ImGui::BeginChild("##portfolio_content", ImVec2(0, vstack.heights[2]), ImGuiChildFlags_None);

    if (!paper_trading_mode_) {
        if (detail_view_ != DetailView::None) {
            render_detail_header();
            switch (detail_view_) {
                case DetailView::AnalyticsSectors:  render_analytics_view(); break;
                case DetailView::PerformanceRisk:   render_performance_view(); break;
                case DetailView::Optimization:      render_optimization_view(); break;
                case DetailView::QuantStats:        render_quantstats_view(); break;
                case DetailView::ReportsPME:        render_reports_view(); break;
                case DetailView::CustomIndices:     render_indices_view(); break;
                case DetailView::RiskManagement:    render_risk_view(); break;
                case DetailView::Planning:          render_planning_view(); break;
                case DetailView::Economics:         render_economics_view(); break;
                default: break;
            }
        } else {
            render_holdings_table();
            ImGui::Separator();
            render_detail_buttons();
            ImGui::Separator();
            render_transactions_table();
        }
    } else {
        // Paper trading: Yoga two-panel layout instead of hardcoded 65%
        float w = ImGui::GetContentRegionAvail().x;
        float h = ImGui::GetContentRegionAvail().y;
        auto panels = ui::two_panel_layout(w, h, true, 35, 250, 380);

        ImGui::BeginChild("##pt_left", ImVec2(panels.main_w, panels.main_h), ImGuiChildFlags_Borders);
        render_pt_positions();
        ImGui::Separator();
        render_pt_orders();
        ImGui::Separator();
        render_pt_trades();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##pt_right", ImVec2(panels.side_w, panels.side_h), ImGuiChildFlags_Borders);
        render_pt_order_entry();
        ImGui::Separator();
        render_pt_stats();
        ImGui::EndChild();
    }

    ImGui::EndChild();
    render_status_bar();

    frame.end();

    // Modals (rendered outside the frame so they overlay properly)
    render_create_portfolio_modal();
    render_add_asset_modal();
    render_sell_asset_modal();
    render_delete_confirm_modal();
    render_pt_create_modal();
    render_import_modal();
    render_edit_transaction_modal();
}

// ============================================================================
// Data refresh
// ============================================================================

void PortfolioScreen::refresh_data() {
    try {
        portfolios_ = get_all_portfolios();
        if (selected_portfolio_idx_ < 0 && !portfolios_.empty()) {
            selected_portfolio_idx_ = 0;
        }
        if (selected_portfolio_idx_ >= 0 && selected_portfolio_idx_ < (int)portfolios_.size()) {
            const auto& pf = portfolios_[selected_portfolio_idx_];
            assets_ = get_assets(pf.id);
            transactions_ = get_transactions(pf.id, 50);

            // Fetch live prices via yfinance Python script
            std::map<std::string, std::pair<double, double>> prices;

            if (!assets_.empty()) {
                nlohmann::json req;
                std::vector<std::string> symbols;
                for (const auto& a : assets_) symbols.push_back(a.symbol);
                req["symbols"] = symbols;

                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/fetch_quotes.py", {}, req.dump());

                if (result.success && !result.output.empty()) {
                    auto j = nlohmann::json::parse(result.output, nullptr, false);
                    if (!j.is_discarded() && j.is_object() && !j.contains("error")) {
                        for (auto& [sym, data] : j.items()) {
                            double cur = data.value("price", 0.0);
                            double prev = data.value("prev_close", 0.0);
                            if (cur > 0) prices[sym] = {cur, prev > 0 ? prev : cur};
                        }
                    }
                }
            }

            // Fallback: use avg_buy_price for symbols without live data
            for (const auto& a : assets_) {
                if (prices.find(a.symbol) == prices.end()) {
                    prices[a.symbol] = {a.avg_buy_price, a.avg_buy_price};
                }
            }

            price_cache_ = prices;
            summary_ = compute_summary(pf, assets_, prices);

            // Compute risk metrics from historical data
            if (!assets_.empty()) {
                compute_portfolio_metrics();
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Portfolio", "Refresh failed: %s", e.what());
    }
    needs_refresh_ = false;
}

void PortfolioScreen::compute_portfolio_metrics() {
    try {
        // Fetch 6mo historical data for all symbols + SPY
        nlohmann::json req;
        std::vector<std::string> symbols;
        for (const auto& h : summary_.holdings) symbols.push_back(h.asset.symbol);
        // Add SPY for beta calculation
        if (std::find(symbols.begin(), symbols.end(), "SPY") == symbols.end())
            symbols.push_back("SPY");
        req["symbols"] = symbols;
        req["period"] = "6mo";
        req["interval"] = "1d";

        auto result = python::execute_with_stdin(
            "Analytics/portfolioManagement/fetch_historical.py", {}, req.dump());

        if (!result.success || result.output.empty()) {
            metrics_loaded_ = false;
            return;
        }

        auto j = nlohmann::json::parse(result.output, nullptr, false);
        if (j.is_discarded() || !j.is_object() || j.contains("error")) {
            metrics_loaded_ = false;
            return;
        }

        // Parse into historical_cache_
        historical_cache_.clear();
        for (auto& [sym, data] : j.items()) {
            if (!data.is_array()) continue;
            std::vector<std::pair<std::string, double>> series;
            for (const auto& pt : data) {
                series.emplace_back(
                    pt.value("date", ""),
                    pt.value("close", 0.0));
            }
            historical_cache_[sym] = std::move(series);
        }

        // Build weights map from summary
        std::map<std::string, double> weights;
        for (const auto& h : summary_.holdings) {
            weights[h.asset.symbol] = h.weight / 100.0; // weight is 0-100, need 0-1
        }

        metrics_ = compute_metrics(historical_cache_, weights);
        metrics_loaded_ = true;
    } catch (const std::exception& e) {
        LOG_ERROR("Portfolio", "Metrics computation failed: %s", e.what());
        metrics_loaded_ = false;
    }
}

void PortfolioScreen::refresh_pt_data() {
    try {
        pt_portfolios_ = trading::pt_list_portfolios();
        if (pt_selected_idx_ < 0 && !pt_portfolios_.empty()) {
            pt_selected_idx_ = 0;
        }
        if (pt_selected_idx_ >= 0 && pt_selected_idx_ < (int)pt_portfolios_.size()) {
            const auto& pf = pt_portfolios_[pt_selected_idx_];
            pt_orders_ = trading::pt_get_orders(pf.id);
            pt_positions_ = trading::pt_get_positions(pf.id);
            pt_trades_ = trading::pt_get_trades(pf.id, 100);
            pt_stats_ = trading::pt_get_stats(pf.id);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Portfolio", "Paper trading refresh failed: %s", e.what());
    }
    pt_needs_refresh_ = false;
}

// ============================================================================
// Command Bar
// ============================================================================

void PortfolioScreen::render_command_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_WIDGET);
    ImGui::BeginChild("##portfolio_cmd", ImVec2(0, 36), ImGuiChildFlags_None);

    // Mode toggle
    if (ImGui::RadioButton("Investment", !paper_trading_mode_)) {
        paper_trading_mode_ = false;
        needs_refresh_ = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Paper Trading", paper_trading_mode_)) {
        paper_trading_mode_ = true;
        pt_needs_refresh_ = true;
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    if (!paper_trading_mode_) {
        // Portfolio selector
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##pf_sel", selected_portfolio_idx_ >= 0 && selected_portfolio_idx_ < (int)portfolios_.size()
                              ? portfolios_[selected_portfolio_idx_].name.c_str() : "Select...")) {
            for (int i = 0; i < (int)portfolios_.size(); i++) {
                if (ImGui::Selectable(portfolios_[i].name.c_str(), i == selected_portfolio_idx_)) {
                    selected_portfolio_idx_ = i;
                    needs_refresh_ = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (theme::AccentButton("+ New")) show_create_modal_ = true;
        ImGui::SameLine();
        if (theme::SecondaryButton("Buy")) { show_add_asset_modal_ = true; is_buy_ = true; }
        ImGui::SameLine();
        if (theme::SecondaryButton("Sell")) show_sell_asset_modal_ = true;
        ImGui::SameLine();
        if (theme::SecondaryButton("Import")) show_import_modal_ = true;
        ImGui::SameLine();
        if (ImGui::BeginMenu("Export##menu")) {
            if (ImGui::MenuItem("JSON")) {
                if (selected_portfolio_idx_ >= 0) {
                    try {
                        auto json = export_portfolio_json(portfolios_[selected_portfolio_idx_].id);
                        std::string filename = portfolios_[selected_portfolio_idx_].name + "_export.json";
                        std::ofstream f(filename);
                        if (f) { f << json; f.close(); }
                    } catch (...) {}
                }
            }
            if (ImGui::MenuItem("CSV")) {
                if (selected_portfolio_idx_ >= 0) {
                    try {
                        auto csv = export_portfolio_csv(portfolios_[selected_portfolio_idx_].id);
                        std::string filename = portfolios_[selected_portfolio_idx_].name + "_export.csv";
                        std::ofstream f(filename);
                        if (f) { f << csv; f.close(); }
                    } catch (...) {}
                }
            }
            ImGui::EndMenu();
        }
    } else {
        // PT portfolio selector
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##pt_sel", pt_selected_idx_ >= 0 && pt_selected_idx_ < (int)pt_portfolios_.size()
                              ? pt_portfolios_[pt_selected_idx_].name.c_str() : "Select...")) {
            for (int i = 0; i < (int)pt_portfolios_.size(); i++) {
                if (ImGui::Selectable(pt_portfolios_[i].name.c_str(), i == pt_selected_idx_)) {
                    pt_selected_idx_ = i;
                    pt_needs_refresh_ = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (theme::AccentButton("+ New")) show_pt_create_modal_ = true;
    }

    ImGui::SameLine();
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - 180);

    // Refresh interval selector
    if (!paper_trading_mode_) {
        static const char* intervals[] = {"1m", "5m", "10m", "30m", "1h", "3h", "1d"};
        static const float intervals_sec[] = {60, 300, 600, 1800, 3600, 10800, 86400};
        ImGui::SetNextItemWidth(50);
        if (ImGui::Combo("##refresh_int", &refresh_interval_idx_, intervals, 7)) {
            auto_refresh_interval_ = intervals_sec[refresh_interval_idx_];
            price_fetch_timer_ = 0.0f;
        }
        ImGui::SameLine();
    }

    if (theme::SecondaryButton("Refresh")) {
        if (!paper_trading_mode_) { needs_refresh_ = true; price_fetch_timer_ = 0.0f; }
        else pt_needs_refresh_ = true;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Stats Ribbon
// ============================================================================

void PortfolioScreen::render_stats_ribbon() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_PANEL);
    ImGui::BeginChild("##stats_ribbon", ImVec2(0, 48), ImGuiChildFlags_Borders);

    auto stat_item = [](const char* label, const char* value, ImVec4 color = theme::colors::TEXT_PRIMARY) {
        ImGui::BeginGroup();
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
        ImGui::TextColored(color, "%s", value);
        ImGui::EndGroup();
    };

    if (!paper_trading_mode_) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "$%.2f", summary_.total_market_value);
        stat_item("NAV", buf);
        ImGui::SameLine(0, 40);

        std::snprintf(buf, sizeof(buf), "%+.2f (%.1f%%)", summary_.total_unrealized_pnl, summary_.total_unrealized_pnl_percent);
        ImVec4 pnl_color = summary_.total_unrealized_pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
        stat_item("Unrealized P&L", buf, pnl_color);
        ImGui::SameLine(0, 40);

        std::snprintf(buf, sizeof(buf), "$%.2f", summary_.total_cost_basis);
        stat_item("Cost Basis", buf);
        ImGui::SameLine(0, 40);

        std::snprintf(buf, sizeof(buf), "%d", summary_.total_positions);
        stat_item("Positions", buf);

        // Risk metrics (if computed)
        if (metrics_loaded_) {
            ImGui::SameLine(0, 40);
            std::snprintf(buf, sizeof(buf), "%.2f", metrics_.sharpe_ratio);
            ImVec4 sharpe_col = metrics_.sharpe_ratio > 1.0 ? theme::colors::MARKET_GREEN
                : metrics_.sharpe_ratio > 0 ? theme::colors::WARNING : theme::colors::MARKET_RED;
            stat_item("Sharpe", buf, sharpe_col);
            ImGui::SameLine(0, 40);

            std::snprintf(buf, sizeof(buf), "%.2f", metrics_.beta);
            stat_item("Beta", buf);
            ImGui::SameLine(0, 40);

            std::snprintf(buf, sizeof(buf), "%.1f%%", metrics_.volatility);
            ImVec4 vol_col = metrics_.volatility < 15 ? theme::colors::MARKET_GREEN
                : metrics_.volatility < 25 ? theme::colors::WARNING : theme::colors::MARKET_RED;
            stat_item("Vol", buf, vol_col);
        }
    } else if (pt_selected_idx_ >= 0 && pt_selected_idx_ < (int)pt_portfolios_.size()) {
        const auto& pf = pt_portfolios_[pt_selected_idx_];
        char buf[64];
        std::snprintf(buf, sizeof(buf), "$%.2f", pf.balance);
        stat_item("Balance", buf);
        ImGui::SameLine(0, 40);

        double pnl = pf.balance - pf.initial_balance;
        double pnl_pct = (pf.initial_balance > 0) ? (pnl / pf.initial_balance) * 100.0 : 0.0;
        std::snprintf(buf, sizeof(buf), "%+.2f (%.1f%%)", pnl, pnl_pct);
        ImVec4 color = pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
        stat_item("Total P&L", buf, color);
        ImGui::SameLine(0, 40);

        std::snprintf(buf, sizeof(buf), "%zu", pt_positions_.size());
        stat_item("Positions", buf);
        ImGui::SameLine(0, 40);

        std::snprintf(buf, sizeof(buf), "%.0f%%", pt_stats_.win_rate);
        stat_item("Win Rate", buf);
        ImGui::SameLine(0, 40);

        std::snprintf(buf, sizeof(buf), "%lld", (long long)pt_stats_.total_trades);
        stat_item("Trades", buf);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Detail View Navigation
// ============================================================================

void PortfolioScreen::render_detail_buttons() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));

    struct ViewBtn { const char* label; DetailView view; };
    static const ViewBtn btns[] = {
        {"Analytics",    DetailView::AnalyticsSectors},
        {"Performance",  DetailView::PerformanceRisk},
        {"Optimization", DetailView::Optimization},
        {"QuantStats",   DetailView::QuantStats},
        {"Reports",      DetailView::ReportsPME},
        {"Indices",      DetailView::CustomIndices},
        {"Risk",         DetailView::RiskManagement},
        {"Planning",     DetailView::Planning},
        {"Economics",    DetailView::Economics},
    };

    for (int i = 0; i < 9; i++) {
        if (i > 0) ImGui::SameLine();
        if (theme::SecondaryButton(btns[i].label)) {
            detail_view_ = btns[i].view;
            detail_sub_tab_ = 0;
        }
    }

    ImGui::PopStyleVar();
}

void PortfolioScreen::render_detail_header() {
    if (theme::SecondaryButton("<< Back")) {
        detail_view_ = DetailView::None;
        detail_sub_tab_ = 0;
    }
    ImGui::SameLine();

    static const char* titles[] = {
        "", "Analytics & Sectors", "Performance & Risk",
        "Optimization", "QuantStats", "Reports & PME",
        "Custom Indices", "Risk Management", "Planning", "Economics"
    };
    int idx = static_cast<int>(detail_view_);
    const char* pf_name = (selected_portfolio_idx_ >= 0 && selected_portfolio_idx_ < (int)portfolios_.size())
        ? portfolios_[selected_portfolio_idx_].name.c_str() : "";
    ImGui::Text("%s  |  %s", pf_name, titles[idx]);
    ImGui::Separator();
}

// Detail view stubs — each will be replaced with full implementations
void PortfolioScreen::render_analytics_view() {
    analytics_view_.render(summary_, metrics_);
}

void PortfolioScreen::render_performance_view() {
    performance_view_.render(summary_, metrics_);
}

void PortfolioScreen::render_optimization_view() {
    optimization_view_.render(summary_);
}

void PortfolioScreen::render_quantstats_view() {
    quantstats_view_.render(summary_);
}

void PortfolioScreen::render_reports_view() {
    reports_view_.render(summary_);
}

void PortfolioScreen::render_indices_view() {
    indices_view_.render(summary_);
}

void PortfolioScreen::render_risk_view() {
    risk_view_.render(summary_, metrics_);
}

void PortfolioScreen::render_planning_view() {
    planning_view_.render(summary_);
}

void PortfolioScreen::render_economics_view() {
    economics_view_.render(summary_);
}

// ============================================================================
// Holdings Table (Investment mode)
// ============================================================================

void PortfolioScreen::render_holdings_table() {
    // Toggle between table and heatmap
    theme::SectionHeader("Holdings");
    ImGui::SameLine();
    static bool show_heatmap = false;
    if (ImGui::SmallButton(show_heatmap ? "Table" : "Heatmap")) show_heatmap = !show_heatmap;

    if (summary_.holdings.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No holdings. Click 'Buy' to add assets.");
        return;
    }

    if (show_heatmap) {
        // Treemap-style heatmap using colored rectangles
        float avail_w = ImGui::GetContentRegionAvail().x;
        float heatmap_h = 200;
        ImVec2 origin = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();

        // Sort by weight descending for treemap layout
        auto sorted = summary_.holdings;
        std::sort(sorted.begin(), sorted.end(), [](const HoldingWithQuote& a, const HoldingWithQuote& b) {
            return a.weight > b.weight;
        });

        float x = 0;
        for (const auto& h : sorted) {
            float w = static_cast<float>(h.weight / 100.0) * avail_w;
            if (w < 2) w = 2;

            // Color based on P&L%
            float intensity = static_cast<float>(std::min(std::abs(h.unrealized_pnl_percent) / 10.0, 1.0));
            ImU32 col;
            if (h.unrealized_pnl_percent >= 0)
                col = IM_COL32(0, static_cast<int>(80 + 175 * intensity), 0, 200);
            else
                col = IM_COL32(static_cast<int>(80 + 175 * intensity), 0, 0, 200);

            ImVec2 p0(origin.x + x, origin.y);
            ImVec2 p1(origin.x + x + w - 1, origin.y + heatmap_h);
            draw->AddRectFilled(p0, p1, col);
            draw->AddRect(p0, p1, IM_COL32(40, 40, 40, 255));

            // Label if wide enough
            if (w > 30) {
                char lbl[64];
                std::snprintf(lbl, sizeof(lbl), "%s\n%+.1f%%", h.asset.symbol.c_str(), h.unrealized_pnl_percent);
                float text_x = origin.x + x + 3;
                float text_y = origin.y + 4;
                draw->AddText(ImVec2(text_x, text_y), IM_COL32(255, 255, 255, 230), lbl);
            }

            x += w;
        }

        ImGui::Dummy(ImVec2(avail_w, heatmap_h));
        return;
    }

    // Sorted holdings for table view
    auto sorted = summary_.holdings;

    constexpr int col_count = 8;
    if (ImGui::BeginTable("##holdings", col_count,
            ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY, ImVec2(0, 250))) {

        ImGui::TableSetupColumn("Symbol",     ImGuiTableColumnFlags_DefaultSort, 0, 0);
        ImGui::TableSetupColumn("Qty",        ImGuiTableColumnFlags_None, 0, 1);
        ImGui::TableSetupColumn("Avg Price",  ImGuiTableColumnFlags_None, 0, 2);
        ImGui::TableSetupColumn("Cur Price",  ImGuiTableColumnFlags_None, 0, 3);
        ImGui::TableSetupColumn("Mkt Value",  ImGuiTableColumnFlags_None, 0, 4);
        ImGui::TableSetupColumn("P&L",        ImGuiTableColumnFlags_None, 0, 5);
        ImGui::TableSetupColumn("P&L %",      ImGuiTableColumnFlags_None, 0, 6);
        ImGui::TableSetupColumn("Weight",     ImGuiTableColumnFlags_PreferSortDescending, 0, 7);
        ImGui::TableHeadersRow();

        // Handle sorting
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsDirty && specs->SpecsCount > 0) {
                const auto& spec = specs->Specs[0];
                bool asc = (spec.SortDirection == ImGuiSortDirection_Ascending);
                std::sort(sorted.begin(), sorted.end(),
                    [&](const HoldingWithQuote& a, const HoldingWithQuote& b) {
                        int cmp = 0;
                        switch (spec.ColumnUserID) {
                            case 0: cmp = a.asset.symbol.compare(b.asset.symbol); break;
                            case 1: cmp = (a.asset.quantity < b.asset.quantity) ? -1 : (a.asset.quantity > b.asset.quantity) ? 1 : 0; break;
                            case 2: cmp = (a.asset.avg_buy_price < b.asset.avg_buy_price) ? -1 : 1; break;
                            case 3: cmp = (a.current_price < b.current_price) ? -1 : 1; break;
                            case 4: cmp = (a.market_value < b.market_value) ? -1 : 1; break;
                            case 5: cmp = (a.unrealized_pnl < b.unrealized_pnl) ? -1 : 1; break;
                            case 6: cmp = (a.unrealized_pnl_percent < b.unrealized_pnl_percent) ? -1 : 1; break;
                            case 7: cmp = (a.weight < b.weight) ? -1 : 1; break;
                        }
                        return asc ? (cmp < 0) : (cmp > 0);
                    });
                specs->SpecsDirty = false;
            }
        }

        for (const auto& h : sorted) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(h.asset.symbol.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.4f", h.asset.quantity);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", h.asset.avg_buy_price);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", h.current_price);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", h.market_value);

            ImVec4 pnl_col = h.unrealized_pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(pnl_col, "%+.2f", h.unrealized_pnl);
            ImGui::TableNextColumn(); ImGui::TextColored(pnl_col, "%+.1f%%", h.unrealized_pnl_percent);
            ImGui::TableNextColumn(); ImGui::Text("%.1f%%", h.weight);
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Transactions Table (Investment mode)
// ============================================================================

void PortfolioScreen::render_transactions_table() {
    theme::SectionHeader("Recent Transactions");

    if (transactions_.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No transactions yet.");
        return;
    }

    if (ImGui::BeginTable("##txns", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY, ImVec2(0, 180))) {

        ImGui::TableSetupColumn("Date");
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Total");
        ImGui::TableHeadersRow();

        for (const auto& t : transactions_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.transaction_date.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.symbol.c_str());

            ImVec4 type_col = (t.transaction_type == "BUY") ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(type_col, "%s", t.transaction_type.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.4f", t.quantity);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", t.price);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", t.total_value);

            // Right-click context menu for edit/delete
            if (ImGui::BeginPopupContextItem(t.id.c_str())) {
                if (ImGui::MenuItem("Edit")) {
                    edit_txn_id_ = t.id;
                    std::snprintf(edit_txn_qty_, sizeof(edit_txn_qty_), "%.4f", t.quantity);
                    std::snprintf(edit_txn_price_, sizeof(edit_txn_price_), "%.2f", t.price);
                    std::strncpy(edit_txn_date_, t.transaction_date.c_str(), sizeof(edit_txn_date_) - 1);
                    std::strncpy(edit_txn_notes_, t.notes.value_or("").c_str(), sizeof(edit_txn_notes_) - 1);
                    show_edit_txn_modal_ = true;
                }
                if (ImGui::MenuItem("Delete")) {
                    try {
                        delete_transaction(t.id);
                        needs_refresh_ = true;
                    } catch (...) {}
                }
                ImGui::EndPopup();
            }
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Paper Trading — Positions
// ============================================================================

void PortfolioScreen::render_pt_positions() {
    theme::SectionHeader("Open Positions");

    if (pt_positions_.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No open positions.");
        return;
    }

    if (ImGui::BeginTable("##pt_pos", 8,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY, ImVec2(0, 150))) {

        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Entry");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Unreal. P&L");
        ImGui::TableSetupColumn("Real. P&L");
        ImGui::TableSetupColumn("Leverage");
        ImGui::TableHeadersRow();

        for (const auto& p : pt_positions_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(p.symbol.c_str());

            ImVec4 side_col = (p.side == "long") ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(side_col, "%s", p.side.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.4f", p.quantity);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", p.entry_price);
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", p.current_price);

            ImVec4 upnl_col = p.unrealized_pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(upnl_col, "%+.2f", p.unrealized_pnl);

            ImVec4 rpnl_col = p.realized_pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(rpnl_col, "%+.2f", p.realized_pnl);
            ImGui::TableNextColumn(); ImGui::Text("%.1fx", p.leverage);
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Paper Trading — Orders
// ============================================================================

void PortfolioScreen::render_pt_orders() {
    theme::SectionHeader("Open Orders");

    // Filter pending/partial only
    std::vector<const trading::PtOrder*> open;
    for (const auto& o : pt_orders_) {
        if (o.status == "pending" || o.status == "partial") open.push_back(&o);
    }

    if (open.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No open orders.");
        return;
    }

    if (ImGui::BeginTable("##pt_orders", 8,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY, ImVec2(0, 120))) {

        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Filled");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (const auto* o : open) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(o->symbol.c_str());

            ImVec4 side_col = (o->side == "buy") ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(side_col, "%s", o->side.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(o->order_type.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.4f", o->quantity);
            ImGui::TableNextColumn();
            if (o->price) ImGui::Text("$%.2f", *o->price);
            else ImGui::TextColored(theme::colors::TEXT_DIM, "MKT");
            ImGui::TableNextColumn(); ImGui::Text("%.4f", o->filled_qty);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(o->status.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(o->id.c_str());
            if (ImGui::SmallButton("Cancel")) {
                try {
                    trading::pt_cancel_order(o->id);
                    pt_needs_refresh_ = true;
                } catch (...) {}
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Paper Trading — Trade History
// ============================================================================

void PortfolioScreen::render_pt_trades() {
    theme::SectionHeader("Trade History");

    if (pt_trades_.empty()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "No trades yet.");
        return;
    }

    if (ImGui::BeginTable("##pt_trades", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY, ImVec2(0, 150))) {

        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("Fee");
        ImGui::TableSetupColumn("P&L");
        ImGui::TableHeadersRow();

        for (const auto& t : pt_trades_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.timestamp.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.symbol.c_str());

            ImVec4 side_col = (t.side == "buy") ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(side_col, "%s", t.side.c_str());
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", t.price);
            ImGui::TableNextColumn(); ImGui::Text("%.4f", t.quantity);
            ImGui::TableNextColumn(); ImGui::Text("$%.4f", t.fee);

            ImVec4 pnl_col = t.pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TableNextColumn(); ImGui::TextColored(pnl_col, "%+.2f", t.pnl);
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Paper Trading — Stats Panel
// ============================================================================

void PortfolioScreen::render_pt_stats() {
    theme::SectionHeader("Performance Stats");

    float stats_label_w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto row = [stats_label_w](const char* label, const char* value, ImVec4 col = theme::colors::TEXT_PRIMARY) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
        ImGui::SameLine(stats_label_w);
        ImGui::TextColored(col, "%s", value);
    };

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.2f", pt_stats_.total_pnl);
    row("Total P&L", buf, pt_stats_.total_pnl >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED);

    std::snprintf(buf, sizeof(buf), "%.1f%%", pt_stats_.win_rate);
    row("Win Rate", buf);

    std::snprintf(buf, sizeof(buf), "%lld", (long long)pt_stats_.total_trades);
    row("Total Trades", buf);

    std::snprintf(buf, sizeof(buf), "%lld / %lld", (long long)pt_stats_.winning_trades, (long long)pt_stats_.losing_trades);
    row("W / L", buf);

    std::snprintf(buf, sizeof(buf), "%+.2f", pt_stats_.largest_win);
    row("Largest Win", buf, theme::colors::MARKET_GREEN);

    std::snprintf(buf, sizeof(buf), "%+.2f", pt_stats_.largest_loss);
    row("Largest Loss", buf, theme::colors::MARKET_RED);
}

// ============================================================================
// Paper Trading — Order Entry
// ============================================================================

void PortfolioScreen::render_pt_order_entry() {
    theme::SectionHeader("Place Order");

    if (pt_selected_idx_ < 0 || pt_selected_idx_ >= (int)pt_portfolios_.size()) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "Select a portfolio first.");
        return;
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##pt_sym", "Symbol (e.g. BTC/USDT)", pt_order_symbol_, sizeof(pt_order_symbol_));

    // Side toggle
    ImGui::PushStyleColor(ImGuiCol_Button, pt_order_is_buy_ ? theme::colors::MARKET_GREEN : theme::colors::BG_WIDGET);
    if (ImGui::Button("BUY", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2, 0))) pt_order_is_buy_ = true;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, !pt_order_is_buy_ ? theme::colors::MARKET_RED : theme::colors::BG_WIDGET);
    if (ImGui::Button("SELL", ImVec2(-1, 0))) pt_order_is_buy_ = false;
    ImGui::PopStyleColor();

    // Order type
    const char* order_types[] = {"market", "limit", "stop", "stop_limit"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##pt_otype", &pt_order_type_idx_, order_types, 4);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##pt_qty", "Quantity", pt_order_qty_, sizeof(pt_order_qty_));

    if (pt_order_type_idx_ != 0) { // not market
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##pt_price", "Price", pt_order_price_, sizeof(pt_order_price_));
    }

    ImGui::Spacing();

    // Submit
    ImVec4 btn_col = pt_order_is_buy_ ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
    ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(btn_col.x * 1.1f, btn_col.y * 1.1f, btn_col.z * 1.1f, 1.0f));

    if (ImGui::Button(pt_order_is_buy_ ? "Place Buy Order" : "Place Sell Order", ImVec2(-1, 32))) {
        try {
            double qty = std::atof(pt_order_qty_);
            std::optional<double> price = std::nullopt;
            std::optional<double> stop_price = std::nullopt;

            if (pt_order_type_idx_ == 1 || pt_order_type_idx_ == 3) { // limit or stop_limit
                price = std::atof(pt_order_price_);
            }
            if (pt_order_type_idx_ == 2 || pt_order_type_idx_ == 3) { // stop or stop_limit
                stop_price = std::atof(pt_order_price_);
            }

            auto order = trading::pt_place_order(
                pt_portfolios_[pt_selected_idx_].id,
                pt_order_symbol_,
                pt_order_is_buy_ ? "buy" : "sell",
                order_types[pt_order_type_idx_],
                qty, price, stop_price);

            // Add to order matcher for non-market orders
            if (pt_order_type_idx_ != 0) {
                trading::OrderMatcher::instance().add_order(order);
            }

            pt_needs_refresh_ = true;
            std::memset(pt_order_qty_, 0, sizeof(pt_order_qty_));
            std::memset(pt_order_price_, 0, sizeof(pt_order_price_));
        } catch (const std::exception& e) {
            LOG_ERROR("Portfolio", "Order placement failed: %s", e.what());
        }
    }

    ImGui::PopStyleColor(2);
}

// ============================================================================
// Status Bar
// ============================================================================

void PortfolioScreen::render_status_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_DARKEST);
    ImGui::BeginChild("##pf_status", ImVec2(0, 0), ImGuiChildFlags_None);

    ImGui::TextColored(theme::colors::TEXT_DIM, "Portfolio");
    ImGui::SameLine();
    ImGui::TextColored(theme::colors::ACCENT, "%s",
        paper_trading_mode_ ? "Paper Trading" : "Investment");

    if (!paper_trading_mode_ && selected_portfolio_idx_ >= 0 && selected_portfolio_idx_ < (int)portfolios_.size()) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(theme::colors::TEXT_DIM, "| %s", portfolios_[selected_portfolio_idx_].name.c_str());
    }
    if (paper_trading_mode_ && pt_selected_idx_ >= 0 && pt_selected_idx_ < (int)pt_portfolios_.size()) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(theme::colors::TEXT_DIM, "| %s", pt_portfolios_[pt_selected_idx_].name.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Modals — Create Portfolio
// ============================================================================

void PortfolioScreen::render_create_portfolio_modal() {
    if (!show_create_modal_) return;

    ImGui::OpenPopup("Create Portfolio");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 250));

    if (ImGui::BeginPopupModal("Create Portfolio", &show_create_modal_, ImGuiWindowFlags_NoResize)) {
        ImGui::InputTextWithHint("Name", "My Portfolio", new_name_, sizeof(new_name_));
        ImGui::InputTextWithHint("Owner", "Your Name", new_owner_, sizeof(new_owner_));

        const char* currencies[] = {"USD", "EUR", "GBP", "JPY", "INR"};
        ImGui::Combo("Currency", &new_currency_idx_, currencies, 5);

        ImGui::Spacing();
        if (theme::AccentButton("Create")) {
            if (new_name_[0] != '\0') {
                try {
                    create_portfolio(new_name_, new_owner_, currencies[new_currency_idx_]);
                    needs_refresh_ = true;
                    show_create_modal_ = false;
                    std::memset(new_name_, 0, sizeof(new_name_));
                    std::memset(new_owner_, 0, sizeof(new_owner_));
                } catch (const std::exception& e) {
                    LOG_ERROR("Portfolio", "Create portfolio failed: %s", e.what());
                }
            }
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_create_modal_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Modals — Add Asset
// ============================================================================

void PortfolioScreen::render_add_asset_modal() {
    if (!show_add_asset_modal_) return;

    ImGui::OpenPopup("Add Asset");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350, 220));

    if (ImGui::BeginPopupModal("Add Asset", &show_add_asset_modal_, ImGuiWindowFlags_NoResize)) {
        ImGui::InputTextWithHint("Symbol", "AAPL", add_symbol_, sizeof(add_symbol_));
        ImGui::InputTextWithHint("Quantity", "10", add_qty_, sizeof(add_qty_));
        ImGui::InputTextWithHint("Price", "150.00", add_price_, sizeof(add_price_));

        ImGui::Spacing();
        if (theme::AccentButton("Add")) {
            if (selected_portfolio_idx_ >= 0 && add_symbol_[0] != '\0') {
                try {
                    add_asset(portfolios_[selected_portfolio_idx_].id,
                              add_symbol_, std::atof(add_qty_), std::atof(add_price_));
                    needs_refresh_ = true;
                    show_add_asset_modal_ = false;
                    std::memset(add_symbol_, 0, sizeof(add_symbol_));
                    std::memset(add_qty_, 0, sizeof(add_qty_));
                    std::memset(add_price_, 0, sizeof(add_price_));
                } catch (const std::exception& e) {
                    LOG_ERROR("Portfolio", "Add asset failed: %s", e.what());
                }
            }
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_add_asset_modal_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Modals — Sell Asset
// ============================================================================

void PortfolioScreen::render_sell_asset_modal() {
    if (!show_sell_asset_modal_) return;

    ImGui::OpenPopup("Sell Asset");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350, 220));

    if (ImGui::BeginPopupModal("Sell Asset", &show_sell_asset_modal_, ImGuiWindowFlags_NoResize)) {
        ImGui::InputTextWithHint("Symbol", "AAPL", sell_symbol_, sizeof(sell_symbol_));
        ImGui::InputTextWithHint("Quantity", "5", sell_qty_, sizeof(sell_qty_));
        ImGui::InputTextWithHint("Price", "155.00", sell_price_, sizeof(sell_price_));

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::MARKET_RED);
        if (ImGui::Button("Sell")) {
            if (selected_portfolio_idx_ >= 0 && sell_symbol_[0] != '\0') {
                try {
                    sell_asset(portfolios_[selected_portfolio_idx_].id,
                               sell_symbol_, std::atof(sell_qty_), std::atof(sell_price_));
                    needs_refresh_ = true;
                    show_sell_asset_modal_ = false;
                    std::memset(sell_symbol_, 0, sizeof(sell_symbol_));
                    std::memset(sell_qty_, 0, sizeof(sell_qty_));
                    std::memset(sell_price_, 0, sizeof(sell_price_));
                } catch (const std::exception& e) {
                    LOG_ERROR("Portfolio", "Sell asset failed: %s", e.what());
                }
            }
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_sell_asset_modal_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Modals — Delete Confirm
// ============================================================================

void PortfolioScreen::render_delete_confirm_modal() {
    if (!show_delete_confirm_) return;

    ImGui::OpenPopup("Delete Portfolio?");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Delete Portfolio?", &show_delete_confirm_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this portfolio?");
        ImGui::Text("This action cannot be undone.");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ERROR_RED);
        if (ImGui::Button("Delete")) {
            if (selected_portfolio_idx_ >= 0) {
                try {
                    delete_portfolio(portfolios_[selected_portfolio_idx_].id);
                    selected_portfolio_idx_ = -1;
                    needs_refresh_ = true;
                } catch (...) {}
            }
            show_delete_confirm_ = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_delete_confirm_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Modals — PT Create
// ============================================================================

void PortfolioScreen::render_pt_create_modal() {
    if (!show_pt_create_modal_) return;

    ImGui::OpenPopup("Create Paper Portfolio");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350, 180));

    if (ImGui::BeginPopupModal("Create Paper Portfolio", &show_pt_create_modal_, ImGuiWindowFlags_NoResize)) {
        ImGui::InputTextWithHint("Name", "My Paper Portfolio", pt_name_, sizeof(pt_name_));
        ImGui::InputTextWithHint("Balance", "100000", pt_balance_, sizeof(pt_balance_));

        ImGui::Spacing();
        if (theme::AccentButton("Create")) {
            if (pt_name_[0] != '\0') {
                try {
                    trading::pt_create_portfolio(pt_name_, std::atof(pt_balance_));
                    pt_needs_refresh_ = true;
                    show_pt_create_modal_ = false;
                    std::memset(pt_name_, 0, sizeof(pt_name_));
                    std::strncpy(pt_balance_, "100000", sizeof(pt_balance_) - 1);
                } catch (const std::exception& e) {
                    LOG_ERROR("Portfolio", "Create PT portfolio failed: %s", e.what());
                }
            }
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_pt_create_modal_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Modals — Import Portfolio
// ============================================================================

void PortfolioScreen::render_import_modal() {
    if (!show_import_modal_) return;

    ImGui::OpenPopup("Import Portfolio");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 280));

    if (ImGui::BeginPopupModal("Import Portfolio", &show_import_modal_, ImGuiWindowFlags_NoResize)) {
        ImGui::InputTextWithHint("File Path", "path/to/portfolio.json", import_path_, sizeof(import_path_));

        ImGui::Spacing();
        ImGui::Text("Import Mode:");
        ImGui::RadioButton("New Portfolio", &import_mode_, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Merge into existing", &import_mode_, 1);

        if (import_mode_ == 1 && !portfolios_.empty()) {
            ImGui::SetNextItemWidth(200);
            if (ImGui::BeginCombo("##merge_target",
                    import_merge_idx_ >= 0 && import_merge_idx_ < (int)portfolios_.size()
                    ? portfolios_[import_merge_idx_].name.c_str() : "Select...")) {
                for (int i = 0; i < (int)portfolios_.size(); i++) {
                    if (ImGui::Selectable(portfolios_[i].name.c_str(), i == import_merge_idx_))
                        import_merge_idx_ = i;
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Spacing();
        if (theme::AccentButton("Import")) {
            if (import_path_[0] != '\0') {
                try {
                    std::ifstream f(import_path_);
                    if (f) {
                        std::string json_data((std::istreambuf_iterator<char>(f)),
                                               std::istreambuf_iterator<char>());
                        f.close();

                        std::string mode = import_mode_ == 0 ? "new" : "merge";
                        std::string merge_id = (import_mode_ == 1 && import_merge_idx_ >= 0
                            && import_merge_idx_ < (int)portfolios_.size())
                            ? portfolios_[import_merge_idx_].id : "";

                        auto result = import_portfolio_json(json_data, mode, merge_id);
                        LOG_INFO("Portfolio", "Imported %d transactions (%d skipped)",
                                 result.transactions_imported, result.transactions_skipped);
                        needs_refresh_ = true;
                        show_import_modal_ = false;
                        std::memset(import_path_, 0, sizeof(import_path_));
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Portfolio", "Import failed: %s", e.what());
                }
            }
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_import_modal_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Modals — Edit Transaction
// ============================================================================

void PortfolioScreen::render_edit_transaction_modal() {
    if (!show_edit_txn_modal_) return;

    ImGui::OpenPopup("Edit Transaction");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 260));

    if (ImGui::BeginPopupModal("Edit Transaction", &show_edit_txn_modal_, ImGuiWindowFlags_NoResize)) {
        ImGui::InputTextWithHint("Quantity", "10", edit_txn_qty_, sizeof(edit_txn_qty_));
        ImGui::InputTextWithHint("Price", "150.00", edit_txn_price_, sizeof(edit_txn_price_));
        ImGui::InputTextWithHint("Date", "2024-01-15", edit_txn_date_, sizeof(edit_txn_date_));
        ImGui::InputTextWithHint("Notes", "Optional notes", edit_txn_notes_, sizeof(edit_txn_notes_));

        ImGui::Spacing();
        if (theme::AccentButton("Save")) {
            if (!edit_txn_id_.empty()) {
                try {
                    update_transaction(edit_txn_id_,
                        std::atof(edit_txn_qty_), std::atof(edit_txn_price_),
                        edit_txn_date_, edit_txn_notes_);
                    needs_refresh_ = true;
                    show_edit_txn_modal_ = false;
                } catch (const std::exception& e) {
                    LOG_ERROR("Portfolio", "Edit transaction failed: %s", e.what());
                }
            }
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) show_edit_txn_modal_ = false;

        ImGui::EndPopup();
    }
}

} // namespace fincept::portfolio
