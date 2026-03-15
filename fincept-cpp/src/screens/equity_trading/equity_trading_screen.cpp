#include "equity_trading_screen.h"
#include "ui/theme.h"
#include "core/logger.h"
#include "trading/broker_registry.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <future>

namespace fincept::equity_trading {

using namespace theme::colors;
static constexpr const char* TAG = "EquityTrading";

// Named constants (ES.45)
static constexpr float NAV_HEIGHT          = 32.0f;
static constexpr float TICKER_HEIGHT       = 48.0f;
static constexpr float STATUS_HEIGHT       = 22.0f;
static constexpr float LEFT_PANEL_WIDTH    = 220.0f;
static constexpr float RIGHT_PANEL_WIDTH   = 280.0f;
static constexpr float BOTTOM_PANEL_FRAC   = 0.32f;
static constexpr int   MAX_DEPTH_LEVELS    = 10;
static constexpr int   MAX_CHART_CANDLES   = 200;

// ============================================================================
// Initialization
// ============================================================================

void EquityTradingScreen::init() {
    LOG_INFO(TAG, "Initializing equity trading screen");

    // Build default watchlist
    watchlist_.clear();
    for (int i = 0; i < DEFAULT_WATCHLIST_COUNT; ++i) {
        WatchlistEntry e;
        e.symbol = DEFAULT_WATCHLIST[i];
        e.exchange = "NSE";
        watchlist_.push_back(e);
    }

    // Copy selected symbol to order form
    std::strncpy(order_form_.symbol_buf, selected_symbol_.c_str(),
                 sizeof(order_form_.symbol_buf) - 1);

    initialized_ = true;
    LOG_INFO(TAG, "Initialized with %d watchlist entries", static_cast<int>(watchlist_.size()));
}

// ============================================================================
// Main render — polls futures, lays out multi-panel Bloomberg-style UI
// ============================================================================

void EquityTradingScreen::render() {
    if (!initialized_) init();

    // Poll async futures
    if (quote_future_.valid()) {
        if (quote_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                current_quote_ = quote_future_.get();
                has_quote_ = true;
            } catch (...) {
                LOG_WARN(TAG, "Quote fetch failed");
            }
            loading_quote_ = false;
        }
    }

    if (candle_future_.valid()) {
        if (candle_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                candles_ = candle_future_.get();
            } catch (...) {
                LOG_WARN(TAG, "Candle fetch failed");
            }
            loading_candles_ = false;
        }
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float w = avail.x;
    const float total_h = avail.y;

    // Layout: top_nav | ticker_bar | [watchlist | chart+bottom | order+depth] | status
    render_top_nav(w);
    render_ticker_bar(w);

    const float body_y = ImGui::GetCursorPosY();
    const float body_h = total_h - body_y - STATUS_HEIGHT;

    if (body_h > 50.0f) {
        const float left_w = LEFT_PANEL_WIDTH;
        const float right_w = RIGHT_PANEL_WIDTH;
        const float center_w = w - left_w - right_w;

        const float bottom_h = bottom_minimized_ ? 0.0f : body_h * BOTTOM_PANEL_FRAC;
        const float chart_h = body_h - bottom_h;

        // Left panel — watchlist
        render_watchlist_panel(0.0f, body_y, left_w, body_h);

        // Center — chart + bottom
        render_chart_panel(left_w, body_y, center_w, chart_h);
        if (!bottom_minimized_) {
            render_bottom_panel(left_w, body_y + chart_h, center_w, bottom_h);
        }

        // Right — order entry + order book
        const float order_h = body_h * 0.55f;
        const float depth_h = body_h - order_h;
        render_order_entry(left_w + center_w, body_y, right_w, order_h);
        render_order_book(left_w + center_w, body_y + order_h, right_w, depth_h);
    }

    render_status_bar(w, total_h);
}

// ============================================================================
// Top Navigation Bar
// ============================================================================

void EquityTradingScreen::render_top_nav(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##eq_nav", ImVec2(w, NAV_HEIGHT), false);

    // Broker status
    if (is_connected_ && active_broker_.has_value()) {
        ImGui::TextColored(SUCCESS, " CONNECTED");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_SECONDARY, "| %s", trading::broker_id_str(*active_broker_));
    } else {
        ImGui::TextColored(TEXT_DIM, " DISCONNECTED");
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(w * 0.3f);

    // Exchange selector
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("##exchange", EXCHANGES[exchange_idx_].code)) {
        for (int i = 0; i < EXCHANGE_COUNT; ++i) {
            const bool selected = (i == exchange_idx_);
            char label[128];
            std::snprintf(label, sizeof(label), "%s - %s", EXCHANGES[i].code, EXCHANGES[i].name);
            if (ImGui::Selectable(label, selected)) {
                exchange_idx_ = i;
                selected_exchange_ = EXCHANGES[i].code;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Symbol input
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputText("##sym_input", symbol_buf_, sizeof(symbol_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        select_symbol(symbol_buf_, selected_exchange_);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("GO")) {
        select_symbol(symbol_buf_, selected_exchange_);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(w - 200.0f);

    // Paper/Live toggle
    if (is_paper_mode_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.1f, 1.0f));
        if (ImGui::SmallButton("PAPER")) { is_paper_mode_ = false; }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        if (ImGui::SmallButton("LIVE")) { is_paper_mode_ = true; }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Broker connect buttons
    if (!is_connected_) {
        if (ImGui::SmallButton("Connect Broker")) {
            bottom_tab_ = BottomTab::brokers;
        }
    } else {
        if (ImGui::SmallButton("Disconnect")) {
            is_connected_ = false;
            active_broker_ = std::nullopt;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Ticker Bar — current symbol quote strip
// ============================================================================

void EquityTradingScreen::render_ticker_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##eq_ticker", ImVec2(w, TICKER_HEIGHT), false);

    // Symbol name
    ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));
    ImGui::PushFont(nullptr);  // default font
    ImGui::TextColored(ACCENT, "%s", selected_symbol_.c_str());
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, " %s", selected_exchange_.c_str());

    if (has_quote_) {
        ImGui::SameLine(0.0f, 24.0f);

        // LTP
        ImGui::TextColored(TEXT_PRIMARY, "%.2f", current_quote_.ltp);
        ImGui::SameLine();

        // Change
        const auto& chg_color = (current_quote_.change >= 0.0) ? MARKET_GREEN : MARKET_RED;
        const char* arrow = (current_quote_.change >= 0.0) ? "+" : "";
        ImGui::TextColored(chg_color, "%s%.2f (%.2f%%)", arrow,
                           current_quote_.change, current_quote_.change_pct);

        // OHLV strip
        ImGui::SameLine(0.0f, 32.0f);
        ImGui::TextColored(TEXT_DIM, "O:");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", current_quote_.open);

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(TEXT_DIM, "H:");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", current_quote_.high);

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(TEXT_DIM, "L:");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", current_quote_.low);

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(TEXT_DIM, "V:");
        ImGui::SameLine(0.0f, 2.0f);

        // Format volume with K/M/B
        if (current_quote_.volume >= 1e9)
            ImGui::TextColored(TEXT_SECONDARY, "%.1fB", current_quote_.volume / 1e9);
        else if (current_quote_.volume >= 1e6)
            ImGui::TextColored(TEXT_SECONDARY, "%.1fM", current_quote_.volume / 1e6);
        else if (current_quote_.volume >= 1e3)
            ImGui::TextColored(TEXT_SECONDARY, "%.1fK", current_quote_.volume / 1e3);
        else
            ImGui::TextColored(TEXT_SECONDARY, "%.0f", current_quote_.volume);

        // Bid/Ask
        ImGui::SameLine(0.0f, 32.0f);
        ImGui::TextColored(MARKET_GREEN, "Bid: %.2f", current_quote_.bid);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(MARKET_RED, "Ask: %.2f", current_quote_.ask);
    } else if (loading_quote_) {
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::TextColored(TEXT_DIM, "Loading...");
    } else {
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::TextColored(TEXT_DIM, "No quote data — connect a broker or fetch data");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Watchlist Panel (left sidebar)
// ============================================================================

void EquityTradingScreen::render_watchlist_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##eq_watchlist", ImVec2(w, h), true);

    // Sidebar tabs
    if (ImGui::SmallButton(sidebar_view_ == SidebarView::watchlist ? "[STOCKS]" : "STOCKS")) {
        sidebar_view_ = SidebarView::watchlist;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(sidebar_view_ == SidebarView::indices ? "[INDICES]" : "INDICES")) {
        sidebar_view_ = SidebarView::indices;
    }

    ImGui::Separator();

    if (sidebar_view_ == SidebarView::watchlist) {
        // Search filter
        ImGui::SetNextItemWidth(w - 16.0f);
        ImGui::InputTextWithHint("##wl_search", "Search...", watchlist_search_,
                                 sizeof(watchlist_search_));

        const std::string filter(watchlist_search_);

        // Watchlist table
        if (ImGui::BeginTable("##wl_tbl", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                ImVec2(0, h - 70.0f))) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.4f);
            ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_None, 0.3f);
            ImGui::TableSetupColumn("Chg%", ImGuiTableColumnFlags_None, 0.3f);
            ImGui::TableHeadersRow();

            for (const auto& entry : watchlist_) {
                // Filter
                if (!filter.empty()) {
                    bool match = false;
                    for (size_t i = 0; i < entry.symbol.size() && i < filter.size(); ++i) {
                        if (std::tolower(static_cast<unsigned char>(entry.symbol[i])) !=
                            std::tolower(static_cast<unsigned char>(filter[i]))) break;
                        if (i + 1 == filter.size()) match = true;
                    }
                    if (!match && entry.symbol.find(filter) == std::string::npos) continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Selectable row
                const bool is_sel = (entry.symbol == selected_symbol_);
                if (ImGui::Selectable(entry.symbol.c_str(), is_sel,
                        ImGuiSelectableFlags_SpanAllColumns)) {
                    select_symbol(entry.symbol, entry.exchange);
                }

                ImGui::TableNextColumn();
                if (entry.ltp > 0.0) {
                    ImGui::Text("%.2f", entry.ltp);
                } else {
                    ImGui::TextColored(TEXT_DIM, "--");
                }

                ImGui::TableNextColumn();
                if (std::abs(entry.change_pct) > 0.001) {
                    const auto& c = (entry.change_pct >= 0.0) ? MARKET_GREEN : MARKET_RED;
                    ImGui::TextColored(c, "%+.2f%%", entry.change_pct);
                } else {
                    ImGui::TextColored(TEXT_DIM, "0.00%%");
                }
            }
            ImGui::EndTable();
        }

        // Refresh button
        if (loading_watchlist_) {
            ImGui::TextColored(TEXT_DIM, "Refreshing...");
        } else {
            if (ImGui::SmallButton("Refresh Watchlist")) {
                fetch_watchlist_quotes();
            }
        }

    } else {
        // Market Indices view
        ImGui::TextColored(ACCENT, "MARKET INDICES");
        ImGui::Separator();
        for (int i = 0; i < MARKET_INDICES_COUNT; ++i) {
            const auto& idx = MARKET_INDICES[i];
            if (ImGui::Selectable(idx.symbol, false)) {
                select_symbol(idx.symbol, idx.exchange);
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Chart Panel — candlestick chart via ImGui DrawList
// ============================================================================

void EquityTradingScreen::render_chart_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##eq_chart", ImVec2(w, h), true);

    // Timeframe buttons
    static constexpr TimeFrame tfs[] = {
        TimeFrame::m1, TimeFrame::m5, TimeFrame::m15, TimeFrame::m30,
        TimeFrame::h1, TimeFrame::h4, TimeFrame::d1, TimeFrame::w1
    };
    for (const auto tf : tfs) {
        const bool active = (tf == chart_timeframe_);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
        if (ImGui::SmallButton(timeframe_label(tf))) {
            chart_timeframe_ = tf;
            // Would trigger candle fetch here
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    // Toggle bottom panel
    ImGui::SameLine(w - 80.0f);
    if (ImGui::SmallButton(bottom_minimized_ ? "Show" : "Hide")) {
        bottom_minimized_ = !bottom_minimized_;
    }

    ImGui::Separator();

    // Chart area
    if (loading_candles_) {
        ImGui::TextColored(TEXT_DIM, "Loading chart data...");
    } else if (candles_.empty()) {
        const float cw = w - 16.0f;
        const float ch = h - 60.0f;
        ImGui::SetCursorPos(ImVec2(cw * 0.3f, ch * 0.4f));
        ImGui::TextColored(TEXT_DIM, "No chart data available");
        ImGui::SetCursorPosX(cw * 0.2f);
        ImGui::TextColored(TEXT_DIM, "Connect a broker to load candle data");
    } else {
        render_candle_chart(w - 16.0f, h - 60.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Candle Chart Renderer (ImGui DrawList)
// ============================================================================

void EquityTradingScreen::render_candle_chart(float w, float h) {
    if (candles_.empty() || w < 20.0f || h < 20.0f) return;

    const ImVec2 p = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();

    // Background
    draw->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(17, 17, 18, 255));

    // Find min/max
    double min_price = 1e18;
    double max_price = -1e18;
    for (const auto& c : candles_) {
        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
    }
    if (max_price <= min_price) max_price = min_price + 1.0;

    const double price_range = max_price - min_price;
    const float padding = h * 0.05f;
    const float chart_h = h - 2.0f * padding;
    const int n = static_cast<int>(candles_.size());
    const float candle_w = std::max(1.0f, (w - 40.0f) / static_cast<float>(n));
    const float gap = std::max(1.0f, candle_w * 0.15f);
    const float body_w = candle_w - gap;

    // Grid lines (5 horizontal)
    for (int i = 0; i <= 4; ++i) {
        const float fy = p.y + padding + chart_h * static_cast<float>(i) / 4.0f;
        draw->AddLine(ImVec2(p.x + 40.0f, fy), ImVec2(p.x + w, fy),
                      IM_COL32(50, 50, 55, 100));
        const double price_at = max_price - price_range * static_cast<double>(i) / 4.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", price_at);
        draw->AddText(ImVec2(p.x + 2.0f, fy - 6.0f), IM_COL32(136, 136, 145, 200), buf);
    }

    // Draw candles
    for (int i = 0; i < n; ++i) {
        const auto& c = candles_[i];
        const float cx = p.x + 40.0f + static_cast<float>(i) * candle_w + candle_w * 0.5f;

        auto price_to_y = [&](double price) -> float {
            return p.y + padding + chart_h * static_cast<float>((max_price - price) / price_range);
        };

        const float y_open = price_to_y(c.open);
        const float y_close = price_to_y(c.close);
        const float y_high = price_to_y(c.high);
        const float y_low = price_to_y(c.low);

        const bool bullish = (c.close >= c.open);
        const ImU32 col = bullish ? IM_COL32(13, 217, 107, 255) : IM_COL32(245, 64, 64, 255);

        // Wick
        draw->AddLine(ImVec2(cx, y_high), ImVec2(cx, y_low), col, 1.0f);

        // Body
        const float body_top = std::min(y_open, y_close);
        const float body_bot = std::max(y_open, y_close);
        if (body_bot - body_top < 1.0f) {
            draw->AddLine(ImVec2(cx - body_w * 0.5f, body_top),
                          ImVec2(cx + body_w * 0.5f, body_top), col, 1.0f);
        } else {
            draw->AddRectFilled(
                ImVec2(cx - body_w * 0.5f, body_top),
                ImVec2(cx + body_w * 0.5f, body_bot), col);
        }
    }

    // Advance cursor
    ImGui::Dummy(ImVec2(w, h));
}

// ============================================================================
// Bottom Panel — positions, holdings, orders, history, brokers tabs
// ============================================================================

void EquityTradingScreen::render_bottom_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##eq_bottom", ImVec2(w, h), true);

    // Tab bar
    static constexpr BottomTab tabs[] = {
        BottomTab::positions, BottomTab::holdings, BottomTab::orders,
        BottomTab::history, BottomTab::brokers
    };
    for (const auto t : tabs) {
        const bool active = (t == bottom_tab_);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
        if (ImGui::SmallButton(bottom_tab_label(t))) {
            bottom_tab_ = t;
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    // Refresh button
    ImGui::SameLine(w - 70.0f);
    if (ImGui::SmallButton("Refresh")) {
        refresh_portfolio();
    }

    ImGui::Separator();

    const float content_h = h - 50.0f;

    switch (bottom_tab_) {
        case BottomTab::positions: render_positions_table(w, content_h); break;
        case BottomTab::holdings:  render_holdings_table(w, content_h);  break;
        case BottomTab::orders:    render_orders_table(w, content_h);    break;
        case BottomTab::history:   render_orders_table(w, content_h);    break;  // reuse
        case BottomTab::brokers:   render_broker_config(w, content_h);   break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Positions Table
// ============================================================================

void EquityTradingScreen::render_positions_table(float w, float h) {
    if (loading_portfolio_) {
        ImGui::TextColored(TEXT_DIM, "Loading positions...");
        return;
    }
    if (positions_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No open positions");
        return;
    }

    if (ImGui::BeginTable("##pos_tbl", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, h))) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_None, 0.08f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_None, 0.10f);
        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("P&L", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableSetupColumn("Day P&L", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableHeadersRow();

        for (const auto& pos : positions_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ACCENT, "%s", pos.symbol.c_str());

            ImGui::TableNextColumn();
            const auto& side_col = (pos.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", pos.side.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", pos.quantity);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", pos.avg_price);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", pos.ltp);

            ImGui::TableNextColumn();
            const auto& pnl_col = (pos.pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(pnl_col, "%+.2f", pos.pnl);

            ImGui::TableNextColumn();
            const auto& day_col = (pos.day_pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(day_col, "%+.2f", pos.day_pnl);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Holdings Table
// ============================================================================

void EquityTradingScreen::render_holdings_table(float w, float h) {
    if (loading_portfolio_) {
        ImGui::TextColored(TEXT_DIM, "Loading holdings...");
        return;
    }
    if (holdings_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No holdings");
        return;
    }

    if (ImGui::BeginTable("##hold_tbl", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, h))) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.20f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_None, 0.10f);
        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_None, 0.15f);
        ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_None, 0.15f);
        ImGui::TableSetupColumn("P&L", ImGuiTableColumnFlags_None, 0.20f);
        ImGui::TableSetupColumn("P&L%", ImGuiTableColumnFlags_None, 0.20f);
        ImGui::TableHeadersRow();

        for (const auto& hld : holdings_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ACCENT, "%s", hld.symbol.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", hld.quantity);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", hld.avg_price);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", hld.ltp);

            ImGui::TableNextColumn();
            const auto& c = (hld.pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(c, "%+.2f", hld.pnl);

            ImGui::TableNextColumn();
            ImGui::TextColored(c, "%+.2f%%", hld.pnl_pct);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Orders Table
// ============================================================================

void EquityTradingScreen::render_orders_table(float w, float h) {
    if (loading_portfolio_) {
        ImGui::TextColored(TEXT_DIM, "Loading orders...");
        return;
    }
    if (orders_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No orders");
        return;
    }

    if (ImGui::BeginTable("##ord_tbl", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, h))) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.15f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_None, 0.08f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 0.10f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_None, 0.10f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_None, 0.13f);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_None, 0.30f);
        ImGui::TableHeadersRow();

        for (const auto& ord : orders_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ACCENT, "%s", ord.symbol.c_str());

            ImGui::TableNextColumn();
            const auto& sc = (ord.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(sc, "%s", ord.side.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", ord.order_type.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", ord.quantity);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", ord.price);

            ImGui::TableNextColumn();
            // Color by status
            ImVec4 status_col = TEXT_SECONDARY;
            if (ord.status == "filled") status_col = SUCCESS;
            else if (ord.status == "pending") status_col = WARNING;
            else if (ord.status == "cancelled" || ord.status == "rejected") status_col = ERROR_RED;
            ImGui::TextColored(status_col, "%s", ord.status.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", ord.timestamp.c_str());
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Broker Configuration Panel
// ============================================================================

void EquityTradingScreen::render_broker_config(float w, float h) {
    ImGui::TextColored(ACCENT, "BROKER CONNECTIONS");
    ImGui::Separator();
    ImGui::Spacing();

    // List available brokers in a grid
    const auto& reg = trading::BrokerRegistry::instance();
    const auto broker_ids = reg.list_brokers();

    if (ImGui::BeginTable("##broker_grid", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame,
            ImVec2(0, h - 40.0f))) {
        ImGui::TableSetupColumn("Broker");
        ImGui::TableSetupColumn("Region");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        // Show all known brokers from BrokerId enum
        struct BrokerInfo { trading::BrokerId id; const char* name; const char* region; };
        static constexpr BrokerInfo known_brokers[] = {
            {trading::BrokerId::Fyers,     "Fyers",      "India"},
            {trading::BrokerId::Zerodha,   "Zerodha",    "India"},
            {trading::BrokerId::Upstox,    "Upstox",     "India"},
            {trading::BrokerId::Dhan,      "Dhan",       "India"},
            {trading::BrokerId::AngelOne,  "Angel One",  "India"},
            {trading::BrokerId::Groww,     "Groww",      "India"},
            {trading::BrokerId::Kotak,     "Kotak",      "India"},
            {trading::BrokerId::AliceBlue, "AliceBlue",  "India"},
            {trading::BrokerId::Alpaca,    "Alpaca",     "US"},
            {trading::BrokerId::IBKR,      "IBKR",       "US"},
            {trading::BrokerId::Tradier,   "Tradier",    "US"},
            {trading::BrokerId::SaxoBank,  "Saxo Bank",  "EU"},
        };

        for (const auto& b : known_brokers) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", b.name);

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", b.region);

            ImGui::TableNextColumn();
            const bool connected = (active_broker_.has_value() && *active_broker_ == b.id && is_connected_);
            if (connected) {
                ImGui::TextColored(SUCCESS, "Connected");
            } else {
                ImGui::TextColored(TEXT_DIM, "Offline");
            }

            ImGui::TableNextColumn();
            char btn_id[64];
            std::snprintf(btn_id, sizeof(btn_id), "Connect##%s", b.name);
            if (!connected) {
                if (ImGui::SmallButton(btn_id)) {
                    connect_broker(b.id);
                }
            } else {
                std::snprintf(btn_id, sizeof(btn_id), "Disconnect##%s", b.name);
                if (ImGui::SmallButton(btn_id)) {
                    is_connected_ = false;
                    active_broker_ = std::nullopt;
                }
            }
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Order Entry Panel (right side, top)
// ============================================================================

void EquityTradingScreen::render_order_entry(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##eq_order", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "ORDER ENTRY");
    ImGui::Separator();

    const float input_w = w - 24.0f;

    // Buy / Sell toggle
    {
        const bool is_buy = (order_form_.side == trading::OrderSide::Buy);
        if (is_buy) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.2f, 1.0f));
        else        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        if (ImGui::Button("BUY", ImVec2(input_w * 0.48f, 28.0f))) {
            order_form_.side = trading::OrderSide::Buy;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        if (!is_buy) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        else         ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        if (ImGui::Button("SELL", ImVec2(input_w * 0.48f, 28.0f))) {
            order_form_.side = trading::OrderSide::Sell;
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // Symbol
    ImGui::TextColored(TEXT_DIM, "Symbol");
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputText("##ord_sym", order_form_.symbol_buf, sizeof(order_form_.symbol_buf));

    // Order type
    ImGui::TextColored(TEXT_DIM, "Type");
    ImGui::SetNextItemWidth(input_w);
    static constexpr const char* type_labels[] = {"Market", "Limit", "Stop Loss", "SL Limit"};
    int type_idx = static_cast<int>(order_form_.type);
    if (ImGui::Combo("##ord_type", &type_idx, type_labels, 4)) {
        order_form_.type = static_cast<trading::OrderType>(type_idx);
    }

    // Product type
    ImGui::TextColored(TEXT_DIM, "Product");
    ImGui::SetNextItemWidth(input_w);
    static constexpr const char* prod_labels[] = {"Intraday", "Delivery", "Margin", "Cover", "Bracket"};
    int prod_idx = static_cast<int>(order_form_.product);
    if (ImGui::Combo("##ord_prod", &prod_idx, prod_labels, 5)) {
        order_form_.product = static_cast<trading::ProductType>(prod_idx);
    }

    // Quantity
    ImGui::TextColored(TEXT_DIM, "Quantity");
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputDouble("##ord_qty", &order_form_.quantity, 1.0, 10.0, "%.0f");

    // Price (for limit/SL orders)
    if (order_form_.type != trading::OrderType::Market) {
        ImGui::TextColored(TEXT_DIM, "Price");
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputDouble("##ord_price", &order_form_.price, 0.05, 1.0, "%.2f");
    }

    // Stop price
    if (order_form_.type == trading::OrderType::StopLoss ||
        order_form_.type == trading::OrderType::StopLossLimit) {
        ImGui::TextColored(TEXT_DIM, "Trigger Price");
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputDouble("##ord_trigger", &order_form_.stop_price, 0.05, 1.0, "%.2f");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Estimated value
    const double est_value = order_form_.quantity *
        (order_form_.type == trading::OrderType::Market ? current_quote_.ltp : order_form_.price);
    if (est_value > 0.0) {
        ImGui::TextColored(TEXT_DIM, "Est. Value:");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%.2f", est_value);
    }

    // Available funds
    if (funds_.has_value()) {
        ImGui::TextColored(TEXT_DIM, "Available:");
        ImGui::SameLine();
        ImGui::TextColored(MARKET_GREEN, "%.2f", funds_->available_balance);
    }

    ImGui::Spacing();

    // Place order button
    const bool can_order = is_connected_ || is_paper_mode_;
    if (!can_order) ImGui::BeginDisabled();

    const bool is_buy = (order_form_.side == trading::OrderSide::Buy);
    const ImVec4 btn_col = is_buy ? ImVec4(0.0f, 0.6f, 0.25f, 1.0f)
                                  : ImVec4(0.7f, 0.15f, 0.15f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
    const char* btn_text = is_buy ? "PLACE BUY ORDER" : "PLACE SELL ORDER";
    if (ImGui::Button(btn_text, ImVec2(input_w, 32.0f))) {
        place_order();
    }
    ImGui::PopStyleColor();

    if (!can_order) ImGui::EndDisabled();

    if (!can_order) {
        ImGui::TextColored(WARNING, "Connect broker or enable paper mode");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Order Book / Market Depth Panel (right side, bottom)
// ============================================================================

void EquityTradingScreen::render_order_book(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##eq_depth", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "MARKET DEPTH");
    ImGui::Separator();

    // Placeholder depth — in a real implementation, broker.get_quotes() or WS feed populates this
    if (ImGui::BeginTable("##depth_tbl", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, h - 40.0f))) {
        ImGui::TableSetupColumn("Bid Qty", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableSetupColumn("Bid", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableSetupColumn("Ask", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableSetupColumn("Ask Qty", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableHeadersRow();

        // If we have a quote, generate synthetic depth levels
        if (has_quote_ && current_quote_.ltp > 0.0) {
            const double mid = current_quote_.ltp;
            const double tick = std::max(0.05, mid * 0.001);

            for (int i = 0; i < MAX_DEPTH_LEVELS; ++i) {
                ImGui::TableNextRow();
                const double bid_price = mid - tick * static_cast<double>(i + 1);
                const double ask_price = mid + tick * static_cast<double>(i + 1);

                ImGui::TableNextColumn();
                ImGui::TextColored(MARKET_GREEN, "%d", (MAX_DEPTH_LEVELS - i) * 100);

                ImGui::TableNextColumn();
                ImGui::TextColored(MARKET_GREEN, "%.2f", bid_price);

                ImGui::TableNextColumn();
                ImGui::TextColored(MARKET_RED, "%.2f", ask_price);

                ImGui::TableNextColumn();
                ImGui::TextColored(MARKET_RED, "%d", (i + 1) * 80);
            }
        } else {
            for (int i = 0; i < 5; ++i) {
                ImGui::TableNextRow();
                for (int j = 0; j < 4; ++j) {
                    ImGui::TableNextColumn();
                    ImGui::TextColored(TEXT_DIM, "--");
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status Bar
// ============================================================================

void EquityTradingScreen::render_status_bar(float w, float total_h) {
    ImGui::SetCursorPos(ImVec2(0.0f, total_h - STATUS_HEIGHT));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##eq_status", ImVec2(w, STATUS_HEIGHT), false);

    ImGui::SetCursorPos(ImVec2(8.0f, 2.0f));

    // Connection status
    if (is_connected_ && active_broker_.has_value()) {
        ImGui::TextColored(SUCCESS, "CONNECTED");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "| %s", trading::broker_id_str(*active_broker_));
    } else {
        ImGui::TextColored(TEXT_DIM, "DISCONNECTED");
    }

    ImGui::SameLine(0.0f, 20.0f);

    // Mode
    if (is_paper_mode_) {
        ImGui::TextColored(WARNING, "PAPER MODE");
    } else {
        ImGui::TextColored(MARKET_RED, "LIVE MODE");
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::TextColored(TEXT_DIM, "%s:%s",
                       selected_exchange_.c_str(), selected_symbol_.c_str());

    // Funds on the right
    if (funds_.has_value()) {
        ImGui::SameLine(w - 200.0f);
        ImGui::TextColored(TEXT_DIM, "Balance:");
        ImGui::SameLine();
        ImGui::TextColored(MARKET_GREEN, "%.2f", funds_->available_balance);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Actions
// ============================================================================

void EquityTradingScreen::select_symbol(const std::string& symbol, const std::string& exchange) {
    selected_symbol_ = symbol;
    selected_exchange_ = exchange;
    std::strncpy(symbol_buf_, symbol.c_str(), sizeof(symbol_buf_) - 1);
    std::strncpy(order_form_.symbol_buf, symbol.c_str(), sizeof(order_form_.symbol_buf) - 1);
    has_quote_ = false;
    candles_.clear();

    LOG_INFO(TAG, "Selected %s:%s", exchange.c_str(), symbol.c_str());

    // Fetch quote if broker connected
    if (is_connected_) {
        fetch_quote();
    }
}

void EquityTradingScreen::fetch_quote() {
    if (!active_broker_.has_value() || loading_quote_) return;

    auto* broker = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!broker) return;

    loading_quote_ = true;
    const auto creds = active_creds_;
    const auto sym = selected_symbol_;

    quote_future_ = std::async(std::launch::async, [broker, creds, sym]() {
        auto resp = broker->get_quotes(creds, {sym});
        if (resp.success && resp.data.has_value() && !resp.data->empty()) {
            return resp.data->front();
        }
        return trading::BrokerQuote{};
    });
}

void EquityTradingScreen::fetch_watchlist_quotes() {
    if (!active_broker_.has_value() || loading_watchlist_) return;

    auto* broker = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!broker) return;

    loading_watchlist_ = true;
    const auto creds = active_creds_;
    std::vector<std::string> symbols;
    symbols.reserve(watchlist_.size());
    for (const auto& e : watchlist_) symbols.push_back(e.symbol);

    // Fire async — results polled next frame
    std::thread([this, broker, creds, symbols]() {
        auto resp = broker->get_quotes(creds, symbols);
        if (resp.success && resp.data.has_value()) {
            for (const auto& q : *resp.data) {
                for (auto& e : watchlist_) {
                    if (e.symbol == q.symbol) {
                        e.ltp = q.ltp;
                        e.change = q.change;
                        e.change_pct = q.change_pct;
                        e.volume = q.volume;
                        break;
                    }
                }
            }
        }
        loading_watchlist_ = false;
    }).detach();
}

void EquityTradingScreen::place_order() {
    if (!is_connected_ && !is_paper_mode_) return;

    trading::UnifiedOrder order;
    order.symbol = order_form_.symbol_buf;
    order.exchange = selected_exchange_;
    order.side = order_form_.side;
    order.order_type = order_form_.type;
    order.quantity = order_form_.quantity;
    order.price = order_form_.price;
    order.stop_price = order_form_.stop_price;
    order.product_type = order_form_.product;
    order.stop_loss = order_form_.stop_loss;
    order.take_profit = order_form_.take_profit;

    LOG_INFO(TAG, "Placing %s %s order: %s x%.0f @ %.2f",
             trading::order_side_str(order.side),
             trading::order_type_str(order.order_type),
             order.symbol.c_str(), order.quantity, order.price);

    if (is_paper_mode_) {
        // Paper mode — just log it (paper engine integration would go here)
        LOG_INFO(TAG, "Paper order placed (not connected to paper engine)");
        return;
    }

    if (!active_broker_.has_value()) return;
    auto* broker = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!broker) return;

    const auto creds = active_creds_;
    std::thread([broker, creds, order]() {
        auto resp = broker->place_order(creds, order);
        if (resp.success) {
            LOG_INFO(TAG, "Order placed: %s", resp.order_id.c_str());
        } else {
            LOG_WARN(TAG, "Order failed: %s", resp.error.c_str());
        }
    }).detach();
}

void EquityTradingScreen::connect_broker(trading::BrokerId broker_id) {
    LOG_INFO(TAG, "Connecting to broker: %s", trading::broker_id_str(broker_id));

    auto* broker = trading::BrokerRegistry::instance().get(broker_id);
    if (!broker) {
        LOG_WARN(TAG, "Broker not found in registry");
        return;
    }

    // Load credentials from DB
    active_creds_ = broker->load_credentials();
    if (active_creds_.api_key.empty()) {
        LOG_WARN(TAG, "No credentials found for %s — configure in Settings",
                 trading::broker_id_str(broker_id));
        return;
    }

    active_broker_ = broker_id;
    is_connected_ = true;
    LOG_INFO(TAG, "Connected to %s", trading::broker_id_str(broker_id));

    // Fetch initial data
    fetch_quote();
    refresh_portfolio();
}

void EquityTradingScreen::refresh_portfolio() {
    if (!active_broker_.has_value() || loading_portfolio_) return;

    auto* broker = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!broker) return;

    loading_portfolio_ = true;
    const auto creds = active_creds_;

    std::thread([this, broker, creds]() {
        auto pos_resp = broker->get_positions(creds);
        if (pos_resp.success && pos_resp.data.has_value()) {
            positions_ = *pos_resp.data;
        }

        auto hold_resp = broker->get_holdings(creds);
        if (hold_resp.success && hold_resp.data.has_value()) {
            holdings_ = *hold_resp.data;
        }

        auto ord_resp = broker->get_orders(creds);
        if (ord_resp.success && ord_resp.data.has_value()) {
            orders_ = *ord_resp.data;
        }

        auto funds_resp = broker->get_funds(creds);
        if (funds_resp.success && funds_resp.data.has_value()) {
            funds_ = *funds_resp.data;
        }

        loading_portfolio_ = false;
    }).detach();
}

} // namespace fincept::equity_trading
