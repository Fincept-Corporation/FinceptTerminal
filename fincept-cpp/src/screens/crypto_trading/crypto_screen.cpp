#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "trading/exchange_service.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

namespace fincept::crypto {

using namespace theme::colors;

void CryptoTradingScreen::render() {
    if (!initialized_) init();

    // Handle deferred symbol switch (set from search results while holding data_mutex_)
    if (!pending_symbol_switch_.empty()) {
        std::string sym = pending_symbol_switch_;
        pending_symbol_switch_.clear();
        switch_symbol(sym);
    }

    ui::ScreenFrame frame("##crypto_trading", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    float W = frame.width();
    float H = frame.height();

    // Vertical stack: top_nav(28) + ticker_bar(36) + main_area(flex) + status(22)
    auto vstack = ui::vstack_layout(W, H, {28, 36, -1, 22});

    render_top_nav(W, vstack.heights[0]);
    render_ticker_bar(W, vstack.heights[1]);

    // Main area — three-panel horizontal layout
    float main_h = vstack.heights[2];
    {
        auto panels = ui::three_panel_layout(W, main_h, 15, 22, 180, 280, 300, 4);

        ImGui::BeginChild("##crypto_left", ImVec2(panels.left_w, panels.left_h), false);
        render_watchlist(panels.left_w, panels.left_h);
        ImGui::EndChild();

        ImGui::SameLine(0, panels.gap);

        ImGui::BeginChild("##crypto_center", ImVec2(panels.center_w, panels.center_h), false);
        {
            float chart_h = panels.center_h * 0.55f;
            float bottom_h = panels.center_h - chart_h - 4;

            render_chart_area(panels.center_w, chart_h);
            ImGui::Dummy(ImVec2(0, 4));
            render_bottom_panel(panels.center_w, bottom_h);
        }
        ImGui::EndChild();

        if (panels.right_w > 0) {
            ImGui::SameLine(0, panels.gap);

            ImGui::BeginChild("##crypto_right", ImVec2(panels.right_w, panels.right_h), false);
            {
                float entry_h = panels.right_h * 0.45f;
                float book_h = panels.right_h - entry_h - 4;

                render_order_entry(panels.right_w, entry_h);
                ImGui::Dummy(ImVec2(0, 4));
                render_orderbook(panels.right_w, book_h);
            }
            ImGui::EndChild();
        }
    }

    render_status_bar(W, vstack.heights[3]);

    // Credentials popup (rendered outside frame to overlay properly)
    render_credentials_popup();

    frame.end();

    // Periodic refreshes — WS handles ticker/orderbook/candles in real-time,
    // polling is fallback only (long intervals)
    float dt = ImGui::GetIO().DeltaTime;
    portfolio_timer_ += dt;

    bool ws_ok = trading::ExchangeService::instance().is_ws_connected();
    if (!ws_ok) {
        // WS not connected — fall back to faster REST polling
        ticker_timer_ += dt;
        ob_timer_ += dt;
        wl_timer_ += dt;

        if (ticker_timer_ >= TICKER_POLL_INTERVAL) {
            ticker_timer_ = 0;
            async_refresh_ticker();
        }
        if (ob_timer_ >= ORDERBOOK_POLL_INTERVAL) {
            ob_timer_ = 0;
            async_refresh_orderbook();
        }
        if (wl_timer_ >= WATCHLIST_POLL_INTERVAL) {
            wl_timer_ = 0;
            async_refresh_watchlist_prices();
        }
    }

    if (portfolio_timer_ >= PORTFOLIO_REFRESH_INTERVAL) {
        portfolio_timer_ = 0;
        refresh_portfolio_data();
    }

    // Live data refresh (positions, orders, balance from exchange API)
    if (trading_mode_ == TradingMode::Live && has_credentials_) {
        live_data_timer_ += dt;
        if (live_data_timer_ >= LIVE_DATA_REFRESH_INTERVAL) {
            live_data_timer_ = 0;
            async_fetch_live_positions();
            async_fetch_live_orders();
            async_fetch_live_balance();
        }
    }

    // Market info refresh (every 30s)
    market_info_timer_ += dt;
    if (market_info_timer_ >= MARKET_INFO_INTERVAL) {
        market_info_timer_ = 0;
        async_fetch_market_info();
    }

    // Clear success/error messages after timeout
    if (order_form_.msg_timer > 0) {
        order_form_.msg_timer -= dt;
        if (order_form_.msg_timer <= 0) {
            order_form_.error.clear();
            order_form_.success.clear();
        }
    }
}

// ============================================================================
// Top Navigation Bar
// ============================================================================
void CryptoTradingScreen::render_top_nav(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##crypto_topnav", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "CRYPTO");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_PRIMARY, "TRADING");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Exchange selector — dynamic list from ccxt (with search filter)
    ImGui::TextColored(TEXT_DIM, "Exchange:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(140);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);

    if (exchanges_loaded_) {
        if (ImGui::BeginCombo("##exchange", exchange_id_.c_str())) {
            ImGui::PushItemWidth(-1);
            ImGui::InputTextWithHint("##ex_filter", "Filter...",
                                     exchange_filter_, sizeof(exchange_filter_));
            ImGui::PopItemWidth();
            ImGui::Separator();

            std::string filter(exchange_filter_);
            for (auto& c : filter) c = (char)std::tolower((unsigned char)c);

            std::lock_guard<std::mutex> lock(data_mutex_);
            for (const auto& ex_id : available_exchanges_) {
                if (!filter.empty() && ex_id.find(filter) == std::string::npos)
                    continue;
                bool selected = (ex_id == exchange_id_);
                if (ImGui::Selectable(ex_id.c_str(), selected)) {
                    switch_exchange(ex_id);
                    exchange_filter_[0] = '\0';
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::BeginDisabled();
        if (ImGui::BeginCombo("##exchange",
                exchanges_fetching_ ? "Loading..." : exchange_id_.c_str())) {
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    }

    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // API key indicator + settings button
    ImGui::SameLine(0, 6);
    if (has_credentials_) {
        ImGui::TextColored(MARKET_GREEN, "[KEY]");
    } else {
        ImGui::TextColored(TEXT_DIM, "[NO KEY]");
    }
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    if (ImGui::SmallButton("API")) {
        show_credentials_popup_ = true;
        // Pre-fill from DB if we have stored credentials
        load_exchange_credentials();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Trading mode toggle
    bool is_paper = (trading_mode_ == TradingMode::Paper);
    ImGui::PushStyleColor(ImGuiCol_Button, is_paper ? ImVec4(0.0f, 0.5f, 0.2f, 1.0f) : BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Text, is_paper ? TEXT_PRIMARY : TEXT_DIM);
    if (ImGui::SmallButton("PAPER")) trading_mode_ = TradingMode::Paper;
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 4);

    bool is_live = (trading_mode_ == TradingMode::Live);
    ImGui::PushStyleColor(ImGuiCol_Button, is_live ? ImVec4(0.7f, 0.1f, 0.1f, 1.0f) : BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Text, is_live ? TEXT_PRIMARY : TEXT_DIM);
    if (ImGui::SmallButton("LIVE")) {
        trading_mode_ = TradingMode::Live;
        // Trigger initial live data fetch when switching to live mode
        if (has_credentials_ && !live_data_loaded_) {
            async_fetch_live_balance();
            async_fetch_live_positions();
            async_fetch_live_orders();
        }
    }
    ImGui::PopStyleColor(2);

    // Right side: mode badge + balance
    float balance_w = 280;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > balance_w + 20) {
        ImGui::SameLine(ImGui::GetCursorPosX() + avail - balance_w);
        if (is_paper) {
            ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.3f, 1.0f), "PAPER");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "LIVE");
        }
        ImGui::SameLine(0, 8);
        ImGui::TextColored(TEXT_DIM, "Balance:");
        ImGui::SameLine(0, 4);
        char bal_buf[32];
        if (is_paper) {
            std::snprintf(bal_buf, sizeof(bal_buf), "%.2f %s", portfolio_.balance, portfolio_.currency.c_str());
        } else if (has_credentials_ && live_equity_ > 0.0) {
            std::snprintf(bal_buf, sizeof(bal_buf), "%.2f USDT", live_equity_);
        } else {
            std::snprintf(bal_buf, sizeof(bal_buf), "-- (Live)");
        }
        ImGui::TextColored(MARKET_GREEN, "%s", bal_buf);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Ticker Bar — selected symbol price info
// ============================================================================
void CryptoTradingScreen::render_ticker_bar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_ticker", ImVec2(w, h), true);

    // Symbol
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextColored(TEXT_PRIMARY, "%s", selected_symbol_.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(0, 16);

    double price = current_ticker_.last;

    if (price <= 0 && ticker_loading_) {
        theme::LoadingSpinner("Loading...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (price <= 0) {
        ImGui::TextColored(TEXT_DIM, "Waiting for data...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    // Price
    char buf[64];
    bool positive = current_ticker_.change >= 0;
    ImVec4 chg_col = positive ? MARKET_GREEN : MARKET_RED;

    if (price > 1.0)
        std::snprintf(buf, sizeof(buf), "%.2f", price);
    else
        std::snprintf(buf, sizeof(buf), "%.6f", price);

    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextColored(chg_col, "%s", buf);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(0, 16);

    // Change
    std::snprintf(buf, sizeof(buf), "%s%.2f (%s%.2f%%)",
        positive ? "+" : "", current_ticker_.change,
        positive ? "+" : "", current_ticker_.percentage);
    ImGui::TextColored(chg_col, "%s", buf);

    ImGui::SameLine(0, 24);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // High / Low
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "H:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.high);
    ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "L:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.low);
    ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // Volume
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "Vol:");
    ImGui::SameLine(0, 4);
    double vol = current_ticker_.base_volume;
    if (vol >= 1e9)
        std::snprintf(buf, sizeof(buf), "%.2fB", vol / 1e9);
    else if (vol >= 1e6)
        std::snprintf(buf, sizeof(buf), "%.2fM", vol / 1e6);
    else if (vol >= 1e3)
        std::snprintf(buf, sizeof(buf), "%.2fK", vol / 1e3);
    else
        std::snprintf(buf, sizeof(buf), "%.2f", vol);
    ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // Bid / Ask
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "Bid:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.bid);
    ImGui::TextColored(MARKET_GREEN, "%s", buf);

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "Ask:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.ask);
    ImGui::TextColored(MARKET_RED, "%s", buf);

    // Loading indicator
    if (ticker_fetching_) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(WARNING, "[*]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CryptoTradingScreen::render_status_bar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##crypto_status", ImVec2(w, h), true);

    // Connection status
    bool ws_live = trading::ExchangeService::instance().is_ws_connected();
    bool has_data = (current_ticker_.last > 0);
    ImGui::TextColored(ws_live ? MARKET_GREEN : (has_data ? WARNING : MARKET_RED), "[*]");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_DIM, "%s", ws_live ? "WS LIVE" : (has_data ? "REST" : "NO DATA"));

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Exchange
    ImGui::TextColored(TEXT_DIM, "Exchange:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_SECONDARY, "%s", exchange_id_.c_str());

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Mode
    bool is_paper = (trading_mode_ == TradingMode::Paper);
    ImGui::TextColored(TEXT_DIM, "Mode:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(is_paper ? WARNING : MARKET_RED, "%s", is_paper ? "PAPER" : "LIVE");

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Stats
    ImGui::TextColored(TEXT_DIM, "OK:%d ERR:%d", success_count_, error_count_);

    // Async status
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    if (ticker_fetching_ || ob_fetching_ || wl_fetching_) {
        ImGui::TextColored(WARNING, "FETCHING");
        if (ticker_fetching_) { ImGui::SameLine(0, 4); ImGui::TextColored(TEXT_DIM, "[T]"); }
        if (ob_fetching_) { ImGui::SameLine(0, 4); ImGui::TextColored(TEXT_DIM, "[OB]"); }
        if (wl_fetching_) { ImGui::SameLine(0, 4); ImGui::TextColored(TEXT_DIM, "[WL]"); }
    } else {
        ImGui::TextColored(TEXT_DIM, "IDLE");
    }

    // Last error (if any)
    if (!last_error_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 12);
        // Truncate error for status bar
        std::string err_short = last_error_.substr(0, 60);
        ImGui::TextColored(MARKET_RED, "%s", err_short.c_str());
    }

    // Right side: last update time
    if (last_data_update_ > 0) {
        time_t now = time(nullptr);
        int elapsed = (int)difftime(now, last_data_update_);
        char time_buf[32];
        std::snprintf(time_buf, sizeof(time_buf), "Updated %ds ago", elapsed);
        float time_w = ImGui::CalcTextSize(time_buf).x + 8;
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > time_w) {
            ImGui::SameLine(ImGui::GetCursorPosX() + avail - time_w);
            ImGui::TextColored(TEXT_DIM, "%s", time_buf);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::crypto
