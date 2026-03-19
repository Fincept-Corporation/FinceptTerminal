#include "backtesting_screen.h"
#include "backtesting_constants.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace fincept::backtesting {

using json = nlohmann::json;

void BacktestingScreen::render_results_panel() {
    using namespace theme::colors;

    // Header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##bt_res_hdr", ImVec2(0, 56));

    ImGui::TextColored(ACCENT, "RESULTS");

    // View tabs (only for backtest with results)
    if (active_command_ == CommandType::Backtest && !result_.empty() && !data_.is_loading()) {
        const char* views[] = {"SUMMARY", "METRICS", "CHARTS", "TRADES", "RAW"};
        ResultView view_enums[] = {ResultView::Summary, ResultView::Metrics, ResultView::Charts, ResultView::Trades, ResultView::Raw};
        for (int i = 0; i < 5; i++) {
            if (i > 0) ImGui::SameLine(0, 2);
            if (ProviderTab(views[i], ACCENT, result_view_ == view_enums[i]))
                result_view_ = view_enums[i];
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Content
    ImGui::BeginChild("##bt_res_content", ImVec2(0, 0));

    // Error state
    if (has_error_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        ImGui::TextWrapped("ERROR: %s", error_msg_.c_str());
        ImGui::PopStyleColor();
    }
    // Loading state
    else if (data_.is_loading()) {
        ImGui::Spacing(); ImGui::Spacing();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w * 0.3f);
        ImGui::TextColored(ACCENT, "Running %s...", command_label(active_command_));
    }
    // Results
    else if (!result_.empty()) {
        // Synthetic data warning
        if (result_.contains("data") && result_["data"].is_object()) {
            auto& data = result_["data"];
            if (data.value("usingSyntheticData", false) || data.contains("syntheticDataWarning")) {
                ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
                ImGui::TextWrapped("SYNTHETIC DATA WARNING: Results use fake data. Install yfinance for real results.");
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }
        }

        if (active_command_ == CommandType::Backtest) {
            switch (result_view_) {
                case ResultView::Summary: render_result_summary(); break;
                case ResultView::Metrics: render_result_metrics(); break;
                case ResultView::Charts:  render_result_charts();  break;
                case ResultView::Trades:  render_result_trades();  break;
                case ResultView::Raw:     render_result_raw();     break;
                default: render_result_summary(); break;
            }
        }
        else if (active_command_ == CommandType::Optimize || active_command_ == CommandType::WalkForward) {
            render_result_metrics();
            ImGui::Spacing();
            render_result_drawdowns();
            ImGui::Spacing();
            render_result_raw();
        }
        else if (active_command_ == CommandType::Indicator) {
            render_result_indicator();
        }
        else if (active_command_ == CommandType::IndicatorSignals) {
            render_result_signals();
        }
        else if (active_command_ == CommandType::Labels) {
            render_result_labels();
        }
        else if (active_command_ == CommandType::Splits) {
            render_result_splitters();
        }
        else if (active_command_ == CommandType::Returns) {
            render_result_returns();
        }
        else if (active_command_ == CommandType::Signals) {
            render_result_signal_generator();
        }
        else {
            render_result_raw();
        }
    }
    // Empty state
    else {
        EmptyState("No results yet", "Configure and run a command");
    }

    ImGui::EndChild();
}

// =============================================================================
// Result summary
// =============================================================================
void BacktestingScreen::render_result_summary() {
    using namespace theme::colors;

    json perf;
    if (result_.contains("data") && result_["data"].is_object() && result_["data"].contains("performance"))
        perf = result_["data"]["performance"];
    else
        perf = result_.value("performance", json());

    if (perf.empty()) {
        ImGui::TextColored(WARNING, "No performance data available.");
        return;
    }

    float total_return = perf.value("totalReturn", 0.0f) * 100.0f;
    bool is_profit = total_return > 0;

    // Status badge
    ImGui::TextColored(SUCCESS, "BACKTEST COMPLETED");
    ImGui::Spacing();

    // Total return card
    ImVec4 ret_color = is_profit ? SUCCESS : ERROR_RED;
    char ret_str[32];
    snprintf(ret_str, sizeof(ret_str), "%+.2f%%", total_return);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_ret_card", ImVec2(0, 80), ImGuiChildFlags_Borders);
    ImGui::TextColored(TEXT_DIM, "TOTAL RETURN");
    ImGui::PushFont(nullptr); // Use default font at larger rendering
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(ret_color, "%s", ret_str);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();

    json stats;
    if (result_.contains("data") && result_["data"].contains("statistics"))
        stats = result_["data"]["statistics"];
    float init_cap = stats.value("initialCapital", 0.0f);
    float final_cap = stats.value("finalCapital", 0.0f);
    if (init_cap > 0) {
        ImGui::TextColored(TEXT_DIM, "$%.0f -> $%.0f", init_cap, final_cap);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Key metrics grid (2x2)
    if (ImGui::BeginTable("##bt_key_metrics", 2, ImGuiTableFlags_None)) {
        float sharpe = perf.value("sharpeRatio", 0.0f);
        float max_dd = perf.value("maxDrawdown", 0.0f) * 100.0f;
        float win_rate = perf.value("winRate", 0.0f) * 100.0f;
        int total_trades = perf.value("totalTrades", 0);

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::BeginChild("##bt_sharpe", ImVec2(0, 52), ImGuiChildFlags_Borders);
        ImGui::TextColored(TEXT_DIM, "SHARPE RATIO");
        ImGui::TextColored(sharpe > 1 ? SUCCESS : INFO, "%.2f", sharpe);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::BeginChild("##bt_maxdd", ImVec2(0, 52), ImGuiChildFlags_Borders);
        ImGui::TextColored(TEXT_DIM, "MAX DRAWDOWN");
        ImGui::TextColored(ERROR_RED, "%.2f%%", max_dd);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::BeginChild("##bt_winrate", ImVec2(0, 52), ImGuiChildFlags_Borders);
        ImGui::TextColored(TEXT_DIM, "WIN RATE");
        ImGui::TextColored(win_rate > 50 ? SUCCESS : WARNING, "%.1f%%", win_rate);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::BeginChild("##bt_trades", ImVec2(0, 52), ImGuiChildFlags_Borders);
        ImGui::TextColored(TEXT_DIM, "TOTAL TRADES");
        ImGui::TextColored(INFO, "%d", total_trades);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Trade breakdown
    int total_trades = perf.value("totalTrades", 0);
    if (total_trades > 0) {
        int winning = perf.value("winningTrades", 0);
        int losing = perf.value("losingTrades", 0);
        float profit_factor = perf.value("profitFactor", 0.0f);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", winning);
        MetricRow("Winning Trades", buf, SUCCESS);
        snprintf(buf, sizeof(buf), "%d", losing);
        MetricRow("Losing Trades", buf, ERROR_RED);
        snprintf(buf, sizeof(buf), "%.2f", profit_factor);
        MetricRow("Profit Factor", buf, profit_factor > 1 ? SUCCESS : ERROR_RED);
    }
}

// =============================================================================
// Result metrics
// =============================================================================
void BacktestingScreen::render_result_metrics() {
    using namespace theme::colors;

    json perf;
    if (result_.contains("data") && result_["data"].is_object() && result_["data"].contains("performance"))
        perf = result_["data"]["performance"];
    else
        perf = result_.value("performance", json());

    if (perf.empty()) {
        ImGui::TextColored(TEXT_DIM, "No metrics available.");
        return;
    }

    struct MetricCategory {
        const char* name;
        std::vector<const char*> keys;
    };

    MetricCategory categories[] = {
        {"RETURNS", {"totalReturn", "annualizedReturn", "CAGR", "expectancy"}},
        {"RISK METRICS", {"sharpeRatio", "sortinoRatio", "calmarRatio", "volatility", "maxDrawdown"}},
        {"TRADE STATISTICS", {"totalTrades", "winningTrades", "losingTrades", "winRate", "profitFactor"}},
        {"TRADE PERFORMANCE", {"averageWin", "averageLoss", "largestWin", "largestLoss"}},
    };

    for (auto& cat : categories) {
        bool has_any = false;
        for (auto& key : cat.keys) {
            if (perf.contains(key)) { has_any = true; break; }
        }
        if (!has_any) continue;

        ImGui::TextColored(ACCENT, "%s", cat.name);
        ImGui::Spacing();

        for (auto& key : cat.keys) {
            if (!perf.contains(key)) continue;
            auto& val = perf[key];

            char value_str[64];
            ImVec4 color = INFO;

            // Format based on key type
            std::string k(key);
            if (k == "totalReturn" || k == "annualizedReturn" || k == "maxDrawdown"
                || k == "winRate" || k == "volatility") {
                float v = val.get<float>() * 100.0f;
                snprintf(value_str, sizeof(value_str), "%.2f%%", v);
                if (k == "maxDrawdown") color = ERROR_RED;
                else if (k == "winRate") color = v > 50 ? SUCCESS : WARNING;
                else color = v > 0 ? SUCCESS : ERROR_RED;
            }
            else if (k == "sharpeRatio" || k == "sortinoRatio" || k == "calmarRatio" || k == "profitFactor") {
                float v = val.get<float>();
                snprintf(value_str, sizeof(value_str), "%.2f", v);
                color = v > 0 ? SUCCESS : ERROR_RED;
            }
            else if (k == "totalTrades" || k == "winningTrades" || k == "losingTrades") {
                snprintf(value_str, sizeof(value_str), "%d", val.get<int>());
                color = INFO;
            }
            else if (k == "averageWin" || k == "averageLoss" || k == "largestWin"
                     || k == "largestLoss" || k == "expectancy") {
                snprintf(value_str, sizeof(value_str), "$%.2f", val.get<float>());
                color = val.get<float>() > 0 ? SUCCESS : ERROR_RED;
            }
            else {
                snprintf(value_str, sizeof(value_str), "%s", val.dump().c_str());
            }

            // Display label with camelCase to readable
            std::string label(key);
            // Insert spaces before capitals
            std::string readable;
            for (size_t i = 0; i < label.size(); i++) {
                if (i > 0 && label[i] >= 'A' && label[i] <= 'Z')
                    readable += ' ';
                readable += label[i];
            }

            MetricRow(readable.c_str(), value_str, color);
        }

        ImGui::Spacing();
    }
}

// =============================================================================
// Result trades
// =============================================================================
void BacktestingScreen::render_result_trades() {
    using namespace theme::colors;

    json trades;
    if (result_.contains("data") && result_["data"].is_object() && result_["data"].contains("trades"))
        trades = result_["data"]["trades"];

    if (!trades.is_array() || trades.empty()) {
        ImGui::TextColored(TEXT_DIM, "No trade data available.");
        return;
    }

    ImGui::TextColored(ACCENT, "TRADES (%zu)", trades.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##bt_trades_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Entry Date");
        ImGui::TableSetupColumn("Exit Date");
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("PnL");
        ImGui::TableSetupColumn("Return");
        ImGui::TableHeadersRow();

        for (auto& trade : trades) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", trade.value("entryDate", trade.value("entry_date", "-")).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", trade.value("exitDate", trade.value("exit_date", "-")).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", trade.value("side", trade.value("direction", "-")).c_str());
            ImGui::TableNextColumn();
            float pnl = trade.value("pnl", trade.value("profit", 0.0f));
            ImGui::TextColored(pnl >= 0 ? SUCCESS : ERROR_RED, "$%.2f", pnl);
            ImGui::TableNextColumn();
            float ret = trade.value("return", trade.value("returnPct", 0.0f));
            ImGui::TextColored(ret >= 0 ? SUCCESS : ERROR_RED, "%.2f%%", ret * 100);
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Result raw JSON
// =============================================================================
void BacktestingScreen::render_result_raw() {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "RAW JSON");
    ImGui::Spacing();

    std::string raw = result_.dump(2);
    // Truncate for display
    if (raw.size() > 8000) raw = raw.substr(0, 8000) + "\n... (truncated)";

    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
    ImGui::InputTextMultiline("##bt_raw_json", const_cast<char*>(raw.c_str()), raw.size() + 1,
        ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
}

// =============================================================================
// Result charts — equity curve + drawdown using ImPlot
// =============================================================================
void BacktestingScreen::render_result_charts() {
    using namespace theme::colors;

    json equity_data;
    if (result_.contains("data") && result_["data"].is_object() && result_["data"].contains("equity"))
        equity_data = result_["data"]["equity"];

    if (!equity_data.is_array() || equity_data.empty()) {
        EmptyState("No equity curve data", "Run a backtest to see charts");
        return;
    }

    // Extract data into vectors
    std::vector<float> equity_vals;
    std::vector<float> drawdown_vals;
    std::vector<std::string> dates;

    for (size_t i = 0; i < equity_data.size(); i++) {
        auto& item = equity_data[i];
        if (item.is_number()) {
            equity_vals.push_back(item.get<float>());
            drawdown_vals.push_back(0.0f);
            char buf[16]; snprintf(buf, sizeof(buf), "Day %zu", i + 1);
            dates.push_back(buf);
        } else if (item.is_object()) {
            equity_vals.push_back(item.value("equity", item.value("value", 0.0f)));
            drawdown_vals.push_back(item.value("drawdown", 0.0f));
            dates.push_back(item.value("date", ""));
        }
    }

    int n = (int)equity_vals.size();
    if (n == 0) {
        EmptyState("No equity data points", "Check raw output");
        return;
    }

    // Equity Curve section
    ImGui::TextColored(SUCCESS, "EQUITY CURVE");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_equity_chart", ImVec2(0, 160), ImGuiChildFlags_Borders);

    // Simple text-based chart representation (ImPlot not always available)
    float min_eq = equity_vals[0], max_eq = equity_vals[0];
    for (float v : equity_vals) {
        if (v < min_eq) min_eq = v;
        if (v > max_eq) max_eq = v;
    }
    float range = max_eq - min_eq;
    if (range < 0.01f) range = 1.0f;

    // Show key data points
    ImGui::TextColored(TEXT_DIM, "Start: $%.0f  |  End: $%.0f  |  Points: %d", equity_vals.front(), equity_vals.back(), n);
    ImGui::TextColored(TEXT_DIM, "Min: $%.0f  |  Max: $%.0f", min_eq, max_eq);
    ImGui::Spacing();

    // Mini sparkline using progress bars
    float avail_w = ImGui::GetContentRegionAvail().x;
    int bars = std::min(n, (int)avail_w / 3);
    int step = std::max(1, n / bars);

    for (int i = 0; i < bars; i++) {
        int idx = std::min(i * step, n - 1);
        float frac = (equity_vals[idx] - min_eq) / range;
        ImVec4 bar_col = equity_vals[idx] >= equity_vals[0] ? SUCCESS : ERROR_RED;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_col);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + i * 3.0f);
        ImGui::ProgressBar(frac, ImVec2(2, 80), "");
        ImGui::PopStyleColor(2);
        if (i < bars - 1) ImGui::SameLine(0, 1);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Drawdown section
    bool has_dd = false;
    for (float v : drawdown_vals) { if (v != 0.0f) { has_dd = true; break; } }

    if (has_dd) {
        ImGui::TextColored(ERROR_RED, "DRAWDOWN");
        ImGui::Spacing();

        float min_dd = 0, max_dd_abs = 0;
        for (float v : drawdown_vals) {
            if (v < min_dd) min_dd = v;
            if (std::abs(v) > max_dd_abs) max_dd_abs = std::abs(v);
        }

        ImGui::TextColored(TEXT_DIM, "Max Drawdown: %.2f%%", min_dd * 100.0f);
        ImGui::Spacing();

        // Drawdown bars
        if (max_dd_abs > 0) {
            int dd_bars = std::min(n, (int)avail_w / 3);
            int dd_step = std::max(1, n / dd_bars);
            for (int i = 0; i < dd_bars; i++) {
                int idx = std::min(i * dd_step, n - 1);
                float frac = std::abs(drawdown_vals[idx]) / max_dd_abs;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ERROR_RED);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
                ImGui::ProgressBar(frac, ImVec2(2, 40), "");
                ImGui::PopStyleColor(2);
                if (i < dd_bars - 1) ImGui::SameLine(0, 1);
            }
        }
    }
}

// =============================================================================
// Result drawdowns — detailed drawdown metrics
// =============================================================================
void BacktestingScreen::render_result_drawdowns() {
    using namespace theme::colors;

    json perf;
    if (result_.contains("data") && result_["data"].is_object()) {
        if (result_["data"].contains("performance"))
            perf = result_["data"]["performance"];
        if (result_["data"].contains("drawdowns")) {
            auto& dd = result_["data"]["drawdowns"];
            for (auto it = dd.begin(); it != dd.end(); ++it)
                perf[it.key()] = it.value();
        }
    }

    if (perf.empty()) {
        ImGui::TextColored(TEXT_DIM, "No drawdown data available.");
        return;
    }

    // Max drawdown highlight
    if (perf.contains("maxDrawdown")) {
        float max_dd = perf["maxDrawdown"].get<float>() * 100.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.1f));
        ImGui::BeginChild("##bt_maxdd_highlight", ImVec2(0, 52), ImGuiChildFlags_Borders);
        ImGui::TextColored(ERROR_RED, "MAX DRAWDOWN");
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ERROR_RED, "%.2f%%", max_dd);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::TextColored(ACCENT, "DRAWDOWN METRICS");
    ImGui::Spacing();

    struct DDMetric { const char* key; const char* label; const char* desc; };
    static const DDMetric dd_metrics[] = {
        {"maxDrawdown",          "Max Drawdown",          "Largest peak-to-trough decline"},
        {"maxDrawdownDuration",  "Max DD Duration",       "Longest time in drawdown (days)"},
        {"avgDrawdown",          "Avg Drawdown",          "Average drawdown depth"},
        {"avgDrawdownDuration",  "Avg DD Duration",       "Average drawdown duration (days)"},
        {"avgRecoveryDuration",  "Avg Recovery",          "Average time to recover (days)"},
        {"activeDrawdown",       "Active Drawdown",       "Current unrecovered drawdown"},
        {"calmarRatio",          "Calmar Ratio",          "Annualized return / max drawdown"},
        {"ulcerIndex",           "Ulcer Index",           "Root mean square of drawdowns"},
        {"painIndex",            "Pain Index",            "Mean drawdown over period"},
        {"painRatio",            "Pain Ratio",            "Return / pain index"},
        {"martinRatio",          "Martin Ratio",          "Return / ulcer index"},
        {"burkeFactor",          "Burke Factor",          "Return adjusted by DD variability"},
        {"maxConsecutiveLosses", "Max Consecutive Losses", "Longest losing streak"},
        {"maxConsecutiveWins",   "Max Consecutive Wins",  "Longest winning streak"},
    };

    for (auto& m : dd_metrics) {
        if (!perf.contains(m.key)) continue;
        auto& val = perf[m.key];
        char buf[64];
        ImVec4 color = INFO;

        std::string k(m.key);
        if (k == "maxDrawdown" || k == "avgDrawdown" || k == "activeDrawdown") {
            snprintf(buf, sizeof(buf), "%.2f%%", val.get<float>() * 100.0f);
            color = ERROR_RED;
        } else if (k == "calmarRatio" || k == "painRatio" || k == "martinRatio" || k == "burkeFactor") {
            snprintf(buf, sizeof(buf), "%.2f", val.get<float>());
            color = val.get<float>() > 0 ? SUCCESS : ERROR_RED;
        } else if (val.is_number_float()) {
            snprintf(buf, sizeof(buf), "%.2f", val.get<float>());
        } else {
            snprintf(buf, sizeof(buf), "%s", val.dump().c_str());
        }

        // Row with description tooltip
        MetricRow(m.label, buf, color);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", m.desc);
    }
}

// =============================================================================
// Result indicator — parameter sweep results
// =============================================================================
void BacktestingScreen::render_result_indicator() {
    using namespace theme::colors;

    json data;
    if (result_.contains("data") && result_["data"].is_object())
        data = result_["data"];
    else {
        render_result_raw();
        return;
    }

    std::string indicator = data.value("indicator", "Unknown");
    int total_combos = data.value("totalCombinations", 0);
    json results = data.value("results", json::array());

    // Status badge
    ImGui::TextColored(SUCCESS, "INDICATOR SWEEP COMPLETED");
    ImGui::Spacing();

    // Summary card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_ind_summary", ImVec2(0, 48), ImGuiChildFlags_Borders);
    std::string title = indicator;
    for (auto& c : title) c = (char)toupper(c);
    ImGui::TextColored(TEXT_PRIMARY, "%s PARAMETER SWEEP", title.c_str());
    ImGui::TextColored(TEXT_DIM, "Total Combinations: %d", total_combos);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (results.is_array() && !results.empty()) {
        ImGui::TextColored(TEXT_DIM, "PARAMETER COMBINATIONS (showing first 20)");
        ImGui::Spacing();

        int shown = 0;
        for (auto& r : results) {
            if (shown >= 20) break;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.06f));
            char child_id[32]; snprintf(child_id, sizeof(child_id), "##bt_ind_r%d", shown);

            // Check if multi-output
            bool is_multi = r.contains("outputs") && r["outputs"].is_object() && !r["outputs"].empty();

            if (is_multi) {
                ImGui::BeginChild(child_id, ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
                // Params
                ImGui::TextColored(TEXT_DIM, "Params:");
                ImGui::SameLine();
                ImGui::TextColored(TEXT_PRIMARY, "%s", r.value("params", json()).dump().c_str());
                ImGui::Separator();

                // Components
                auto& outputs = r["outputs"];
                for (auto it = outputs.begin(); it != outputs.end(); ++it) {
                    auto& comp = it.value();
                    std::string comp_name = it.key();
                    for (auto& c : comp_name) c = (char)toupper(c);
                    ImGui::TextColored(INFO, "%s", comp_name.c_str());
                    if (comp.contains("mean"))
                        ImGui::Text("Mean: %.2f  Std: %.2f  Last: %.2f  Range: %.2f-%.2f",
                            comp.value("mean", 0.0f), comp.value("std", 0.0f),
                            comp.value("last", 0.0f), comp.value("min", 0.0f), comp.value("max", 0.0f));
                }
                ImGui::EndChild();
            } else {
                ImGui::BeginChild(child_id, ImVec2(0, 36), ImGuiChildFlags_Borders);
                ImGui::TextColored(TEXT_DIM, "Params:");
                ImGui::SameLine();
                ImGui::TextColored(TEXT_PRIMARY, "%s", r.value("params", json()).dump().c_str());
                ImGui::SameLine(0, 12);
                ImGui::TextColored(INFO, "Mean: %.2f", r.value("mean", 0.0f));
                ImGui::SameLine(0, 8);
                ImGui::TextColored(ACCENT, "Last: %.2f", r.value("last", 0.0f));
                ImGui::SameLine(0, 8);
                ImGui::TextColored(TEXT_DIM, "Range: %.2f-%.2f", r.value("min", 0.0f), r.value("max", 0.0f));
                ImGui::EndChild();
            }
            ImGui::PopStyleColor();
            shown++;
        }
    } else {
        ImGui::TextColored(WARNING, "No parameter combinations generated. Check parameter ranges.");
    }
}

// =============================================================================
// Result signals — entry/exit signals from indicator_signals command
// =============================================================================
void BacktestingScreen::render_result_signals() {
    using namespace theme::colors;

    json data;
    if (result_.contains("data") && result_["data"].is_object())
        data = result_["data"];
    else {
        render_result_raw();
        return;
    }

    std::string symbol = data.value("symbol", "");
    ImGui::TextColored(SUCCESS, "SIGNALS GENERATED%s%s", symbol.empty() ? "" : " - ", symbol.c_str());
    ImGui::Spacing();

    // Performance metrics (if available)
    bool has_metrics = data.contains("avgReturn");
    if (has_metrics) {
        ImGui::TextColored(ACCENT, "SIGNAL PERFORMANCE METRICS");
        ImGui::Spacing();

        if (ImGui::BeginTable("##bt_sig_perf", 4, ImGuiTableFlags_None)) {
            auto metric_cell = [](const char* label, const char* fmt, float val, ImVec4 col) {
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", label);
                char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
                ImGui::TextColored(col, "%s", buf);
            };

            float win_rate = data.value("winRate", 0.0f);
            float avg_ret = data.value("avgReturn", 0.0f);
            float pf = data.value("profitFactor", 0.0f);
            int total_sig = data.value("totalSignals", 0);

            metric_cell("WIN RATE", "%.1f%%", win_rate, win_rate >= 50 ? SUCCESS : ERROR_RED);
            metric_cell("AVG RETURN", "%.2f%%", avg_ret, avg_ret >= 0 ? SUCCESS : ERROR_RED);
            metric_cell("PROFIT FACTOR", "%.2f", pf, INFO);

            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SIGNALS");
            ImGui::TextColored(TEXT_PRIMARY, "%d", total_sig);

            // Second row
            float best = data.value("bestTrade", 0.0f);
            float worst = data.value("worstTrade", 0.0f);
            float avg_hold = data.value("avgHoldingPeriod", 0.0f);
            float sig_dens = data.value("signalDensity", 0.0f);

            metric_cell("BEST TRADE", "+%.2f%%", best, SUCCESS);
            metric_cell("WORST TRADE", "%.2f%%", worst, ERROR_RED);
            metric_cell("AVG HOLD (DAYS)", "%.0f", avg_hold, TEXT_PRIMARY);
            metric_cell("SIGNAL DENSITY", "%.1f%%", sig_dens, WARNING);

            ImGui::EndTable();
        }
        ImGui::Spacing();
    }

    // Signal counts
    int entry_count = data.value("entryCount", 0);
    int exit_count = data.value("exitCount", 0);
    int total_bars = data.value("totalBars", 0);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_sig_counts", ImVec2(0, 52), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("##bt_sig_cnt_tbl", 3, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "ENTRY SIGNALS");
        ImGui::TextColored(SUCCESS, "%d", entry_count);

        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "EXIT SIGNALS");
        ImGui::TextColored(ERROR_RED, "%d", exit_count);

        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "TOTAL BARS");
        ImGui::TextColored(TEXT_PRIMARY, "%d", total_bars);

        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Entry signals detail
    json entries = data.value("entrySignals", data.value("entries", json::array()));
    if (entries.is_array() && !entries.empty()) {
        ImGui::TextColored(TEXT_DIM, "ENTRY SIGNALS (showing first 10)");
        ImGui::Spacing();

        int shown = 0;
        for (auto& sig : entries) {
            if (shown >= 10) break;
            char id[32]; snprintf(id, sizeof(id), "##bt_entry_%d", shown);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.08f));
            ImGui::BeginChild(id, ImVec2(0, 20), ImGuiChildFlags_Borders);
            if (sig.is_object()) {
                ImGui::TextColored(TEXT_DIM, "Date:"); ImGui::SameLine();
                ImGui::TextColored(TEXT_PRIMARY, "%s", sig.value("date", "-").c_str());
                ImGui::SameLine(0, 8);
                ImGui::TextColored(TEXT_DIM, "Price:"); ImGui::SameLine();
                ImGui::TextColored(INFO, "$%.2f", sig.value("price", 0.0f));
            } else {
                ImGui::TextColored(TEXT_PRIMARY, "%s", sig.dump().c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            shown++;
        }
        ImGui::Spacing();
    }

    // Exit signals detail
    json exits = data.value("exitSignals", data.value("exits", json::array()));
    if (exits.is_array() && !exits.empty()) {
        ImGui::TextColored(TEXT_DIM, "EXIT SIGNALS (showing first 10)");
        ImGui::Spacing();

        int shown = 0;
        for (auto& sig : exits) {
            if (shown >= 10) break;
            char id[32]; snprintf(id, sizeof(id), "##bt_exit_%d", shown);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.08f));
            ImGui::BeginChild(id, ImVec2(0, 20), ImGuiChildFlags_Borders);
            if (sig.is_object()) {
                ImGui::TextColored(TEXT_DIM, "Date:"); ImGui::SameLine();
                ImGui::TextColored(TEXT_PRIMARY, "%s", sig.value("date", "-").c_str());
                ImGui::SameLine(0, 8);
                ImGui::TextColored(TEXT_DIM, "Price:"); ImGui::SameLine();
                ImGui::TextColored(INFO, "$%.2f", sig.value("price", 0.0f));
            } else {
                ImGui::TextColored(TEXT_PRIMARY, "%s", sig.dump().c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            shown++;
        }
    }
}

// =============================================================================
// Result signal generator — random/generated signals display
// =============================================================================
void BacktestingScreen::render_result_signal_generator() {
    using namespace theme::colors;

    json data;
    if (result_.contains("data") && result_["data"].is_object())
        data = result_["data"];

    if (data.empty()) {
        EmptyState("No signal data available", "");
        return;
    }

    json entries = data.value("entries", data.value("entrySignals", json::array()));
    json exits = data.value("exits", data.value("exitSignals", json::array()));
    int total_entries = data.value("totalEntries", entries.is_array() ? (int)entries.size() : 0);
    int total_exits = data.value("totalExits", exits.is_array() ? (int)exits.size() : 0);

    ImGui::TextColored(ACCENT, "SIGNAL SUMMARY");
    ImGui::Spacing();

    // Summary grid
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_siggen_summary", ImVec2(0, 52), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("##bt_siggen_tbl", 3, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "ENTRIES");
        ImGui::TextColored(SUCCESS, "%d", total_entries);

        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "EXITS");
        ImGui::TextColored(ERROR_RED, "%d", total_exits);

        ImGui::TableNextColumn();
        if (data.contains("signalDensity")) {
            ImGui::TextColored(TEXT_DIM, "DENSITY");
            ImGui::TextColored(INFO, "%.1f%%", data["signalDensity"].get<float>() * 100.0f);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Signal timeline preview
    json signals = data.value("signals", json::array());
    if (signals.is_array() && !signals.empty()) {
        ImGui::TextColored(ACCENT, "SIGNAL TIMELINE (FIRST 20)");
        ImGui::Spacing();

        int shown = 0;
        for (auto& sig : signals) {
            if (shown >= 20) break;
            bool is_entry = false;
            if (sig.is_object()) is_entry = sig.value("type", "") == "entry";
            else if (sig.is_number()) is_entry = sig.get<int>() == 1;
            else if (sig.is_boolean()) is_entry = sig.get<bool>();

            ImVec4 col = is_entry ? SUCCESS : ERROR_RED;
            const char* label = is_entry ? "E" : "X";

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x, col.y, col.z, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            char btn_id[32]; snprintf(btn_id, sizeof(btn_id), "%s##sg%d", label, shown);
            ImGui::SmallButton(btn_id);
            ImGui::PopStyleColor(3);
            if (shown < 19) ImGui::SameLine(0, 2);
            shown++;
        }
    }
    // Entry timestamps fallback
    else if (entries.is_array() && !entries.empty()) {
        ImGui::TextColored(SUCCESS, "ENTRY SIGNALS (FIRST 20)");
        ImGui::Spacing();

        int shown = 0;
        for (auto& e : entries) {
            if (shown >= 20) break;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
            char btn_id[64];
            if (e.is_string())
                snprintf(btn_id, sizeof(btn_id), "%s##se%d", e.get<std::string>().c_str(), shown);
            else
                snprintf(btn_id, sizeof(btn_id), "#%d##se%d", shown + 1, shown);
            ImGui::SmallButton(btn_id);
            ImGui::PopStyleColor(2);
            if (shown < 19) ImGui::SameLine(0, 2);
            shown++;
        }
    }
}

// =============================================================================
// Result labels — ML label generation results
// =============================================================================
void BacktestingScreen::render_result_labels() {
    using namespace theme::colors;

    json data;
    if (result_.contains("data") && result_["data"].is_object())
        data = result_["data"];

    if (data.empty()) {
        EmptyState("No label data available", "");
        return;
    }

    json distribution = data.value("distribution", json::object());
    json labels = data.value("labels", json::array());
    int total_labels = data.value("totalLabels", labels.is_array() ? (int)labels.size() : 0);

    ImGui::TextColored(ACCENT, "LABEL SUMMARY");
    ImGui::Spacing();

    // Summary
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_lbl_summary", ImVec2(0, 52), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("##bt_lbl_sum_tbl", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "TOTAL");
        ImGui::TextColored(TEXT_PRIMARY, "%d", total_labels);

        ImGui::TableNextColumn();
        if (data.contains("signalDensity")) {
            ImGui::TextColored(TEXT_DIM, "DENSITY");
            ImGui::TextColored(INFO, "%.1f%%", data["signalDensity"].get<float>() * 100.0f);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Distribution
    if (distribution.is_object() && !distribution.empty()) {
        ImGui::TextColored(ACCENT, "LABEL DISTRIBUTION");
        ImGui::Spacing();

        for (auto it = distribution.begin(); it != distribution.end(); ++it) {
            std::string label = it.key();
            int count = it.value().is_number() ? it.value().get<int>() : 0;
            float pct = total_labels > 0 ? (float)count / total_labels * 100.0f : 0;

            ImVec4 col = TEXT_DIM;
            if (label == "1" || label == "buy") col = SUCCESS;
            else if (label == "-1" || label == "sell") col = ERROR_RED;

            char val_buf[64];
            snprintf(val_buf, sizeof(val_buf), "%d (%.1f%%)", count, pct);

            std::string upper_label = label;
            for (auto& c : upper_label) c = (char)toupper(c);

            MetricRow(upper_label.c_str(), val_buf, col);
        }
        ImGui::Spacing();
    }

    // Label preview
    if (labels.is_array() && !labels.empty()) {
        ImGui::TextColored(ACCENT, "LABEL PREVIEW (FIRST 20)");
        ImGui::Spacing();

        int shown = 0;
        for (auto& lbl : labels) {
            if (shown >= 20) break;

            int val = 0;
            if (lbl.is_number()) val = lbl.get<int>();
            else if (lbl.is_object() && lbl.contains("label")) val = lbl["label"].get<int>();

            ImVec4 col = TEXT_DIM;
            if (val == 1) col = SUCCESS;
            else if (val == -1) col = ERROR_RED;

            char btn_id[32]; snprintf(btn_id, sizeof(btn_id), "%d##lb%d", val, shown);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::SmallButton(btn_id);
            ImGui::PopStyleColor(2);
            if (shown < 19) ImGui::SameLine(0, 2);
            shown++;
        }
    }
}

// =============================================================================
// Result splitters — cross-validation split results
// =============================================================================
void BacktestingScreen::render_result_splitters() {
    using namespace theme::colors;

    json data;
    if (result_.contains("data") && result_["data"].is_object())
        data = result_["data"];

    if (data.empty()) {
        EmptyState("No split data available", "");
        return;
    }

    json splits = data.value("splits", json::array());
    int n_splits = data.value("nSplits", splits.is_array() ? (int)splits.size() : 0);

    ImGui::TextColored(ACCENT, "SPLIT SUMMARY");
    ImGui::Spacing();

    // Summary
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##bt_spl_summary", ImVec2(0, 52), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("##bt_spl_sum_tbl", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_DIM, "SPLITS");
        ImGui::TextColored(TEXT_PRIMARY, "%d", n_splits);

        ImGui::TableNextColumn();
        if (data.contains("coverage")) {
            ImGui::TextColored(TEXT_DIM, "COVERAGE");
            auto& cov = data["coverage"];
            if (cov.is_number())
                ImGui::TextColored(INFO, "%.1f%%", cov.get<float>() * 100.0f);
            else
                ImGui::TextColored(INFO, "%s", cov.dump().c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Split ranges
    if (splits.is_array() && !splits.empty()) {
        ImGui::TextColored(ACCENT, "SPLIT RANGES");
        ImGui::Spacing();

        if (ImGui::BeginTable("##bt_split_ranges", 3,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
            ImGui::TableSetupColumn("Train");
            ImGui::TableSetupColumn("Test");
            ImGui::TableHeadersRow();

            int idx = 0;
            for (auto& split : splits) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_DIM, "#%d", idx + 1);

                ImGui::TableNextColumn();
                std::string train_start = split.value("trainStart", split.value("train_start", "?"));
                std::string train_end = split.value("trainEnd", split.value("train_end", "?"));
                ImGui::TextColored(INFO, "%s -> %s", train_start.c_str(), train_end.c_str());

                ImGui::TableNextColumn();
                std::string test_start = split.value("testStart", split.value("test_start", "?"));
                std::string test_end = split.value("testEnd", split.value("test_end", "?"));
                ImGui::TextColored(SUCCESS, "%s -> %s", test_start.c_str(), test_end.c_str());

                idx++;
            }
            ImGui::EndTable();
        }
    }
}

// =============================================================================
// Result returns — returns analysis results
// =============================================================================
void BacktestingScreen::render_result_returns() {
    using namespace theme::colors;

    json data;
    if (result_.contains("data") && result_["data"].is_object())
        data = result_["data"];

    if (data.empty()) {
        EmptyState("No returns data available", "");
        return;
    }

    json metrics = data.value("performance", data.value("returns", data));

    struct RetMetric { const char* key; const char* label; };
    static const RetMetric ret_metrics[] = {
        {"totalReturn",      "Total Return"},
        {"annualizedReturn", "Annualized Return"},
        {"volatility",       "Volatility"},
        {"sharpeRatio",      "Sharpe Ratio"},
        {"sortinoRatio",     "Sortino Ratio"},
        {"calmarRatio",      "Calmar Ratio"},
        {"VaR",              "Value at Risk (95%)"},
        {"CVaR",             "Conditional VaR"},
        {"upCapture",        "Up Capture Ratio"},
        {"downCapture",      "Down Capture Ratio"},
        {"skewness",         "Skewness"},
        {"kurtosis",         "Kurtosis"},
        {"CAGR",             "CAGR"},
        {"tailRatio",        "Tail Ratio"},
        {"commonSenseRatio", "Common Sense Ratio"},
    };

    // Check if any metrics exist
    bool has_any = false;
    for (auto& m : ret_metrics) {
        if (metrics.contains(m.key)) { has_any = true; break; }
    }

    if (has_any) {
        ImGui::TextColored(ACCENT, "RETURNS ANALYSIS");
        ImGui::Spacing();

        for (auto& m : ret_metrics) {
            if (!metrics.contains(m.key)) continue;
            auto& val = metrics[m.key];
            char buf[64];
            ImVec4 color = INFO;

            std::string k(m.key);
            if (k == "totalReturn" || k == "annualizedReturn" || k == "volatility" || k == "CAGR") {
                float v = val.get<float>() * 100.0f;
                snprintf(buf, sizeof(buf), "%.2f%%", v);
                color = v > 0 ? SUCCESS : ERROR_RED;
            } else if (k == "VaR" || k == "CVaR") {
                snprintf(buf, sizeof(buf), "%.4f", val.get<float>());
                color = ERROR_RED;
            } else {
                snprintf(buf, sizeof(buf), "%.4f", val.get<float>());
                color = val.get<float>() > 0 ? SUCCESS : ERROR_RED;
            }

            MetricRow(m.label, buf, color);
        }
        ImGui::Spacing();
    }

    // Rolling metrics
    json rolling = data.value("rolling", json());
    if (rolling.is_object() && !rolling.empty()) {
        ImGui::TextColored(INFO, "ROLLING METRICS");
        ImGui::Spacing();

        for (auto it = rolling.begin(); it != rolling.end(); ++it) {
            char buf[64];
            if (it.value().is_number())
                snprintf(buf, sizeof(buf), "%.4f", it.value().get<float>());
            else
                snprintf(buf, sizeof(buf), "%s", it.value().dump().c_str());

            // camelCase to readable
            std::string label = it.key();
            std::string readable;
            for (size_t i = 0; i < label.size(); i++) {
                if (i > 0 && label[i] >= 'A' && label[i] <= 'Z') readable += ' ';
                readable += label[i];
            }
            MetricRow(readable.c_str(), buf, INFO);
        }
        ImGui::Spacing();
    }

    // Benchmark comparison
    json benchmark = data.value("benchmark", json());
    if (benchmark.is_object() && !benchmark.empty()) {
        ImGui::TextColored(SUCCESS, "BENCHMARK COMPARISON");
        ImGui::Spacing();

        for (auto it = benchmark.begin(); it != benchmark.end(); ++it) {
            char buf[64];
            if (it.value().is_number())
                snprintf(buf, sizeof(buf), "%.4f", it.value().get<float>());
            else
                snprintf(buf, sizeof(buf), "%s", it.value().dump().c_str());

            std::string label = it.key();
            std::string readable;
            for (size_t i = 0; i < label.size(); i++) {
                if (i > 0 && label[i] >= 'A' && label[i] <= 'Z') readable += ' ';
                readable += label[i];
            }
            MetricRow(readable.c_str(), buf, SUCCESS);
        }
    }

    if (!has_any && rolling.empty() && benchmark.empty()) {
        ImGui::TextColored(TEXT_DIM, "Returns analysis returned no displayable metrics. Check raw output.");
    }
}

// =============================================================================
// Status bar
// =============================================================================

} // namespace fincept::backtesting
