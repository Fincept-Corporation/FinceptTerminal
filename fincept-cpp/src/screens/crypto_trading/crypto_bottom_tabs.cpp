#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/theme.h"
#include "trading/exchange_service.h"
#include "trading/paper_trading.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <thread>

namespace fincept::crypto {

using namespace theme::colors;

void CryptoTradingScreen::render_positions_tab() {
    bool is_live = (trading_mode_ == TradingMode::Live);

    // Mode label
    ImGui::TextColored(is_live ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : ImVec4(0.0f, 0.7f, 0.3f, 1.0f),
                       is_live ? "[LIVE POSITIONS]" : "[PAPER POSITIONS]");

    if (is_live) {
        if (!has_credentials_) {
            ImGui::TextColored(TEXT_DIM, "API credentials required for live trading.");
            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            if (ImGui::SmallButton("Configure API Key")) {
                show_credentials_popup_ = true;
                load_exchange_credentials();
            }
            ImGui::PopStyleColor();
            return;
        }

        std::lock_guard<std::mutex> lock(data_mutex_);

        if (live_positions_fetching_) {
            theme::LoadingSpinner("Fetching positions...");
            return;
        }

        if (live_positions_.empty()) {
            ImGui::TextColored(TEXT_DIM, "No open positions on %s", exchange_id_.c_str());
            return;
        }

        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##live_positions", 7, flags)) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("PnL", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Lev", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            char buf[32];
            for (const auto& pos : live_positions_) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(TEXT_PRIMARY, "%s", pos.symbol.c_str());

                ImGui::TableSetColumnIndex(1);
                ImVec4 side_col = (pos.side == "long") ? MARKET_GREEN : MARKET_RED;
                ImGui::TextColored(side_col, "%s", pos.side == "long" ? "LONG" : "SHORT");

                ImGui::TableSetColumnIndex(2);
                std::snprintf(buf, sizeof(buf), "%.4f", pos.quantity);
                ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

                ImGui::TableSetColumnIndex(3);
                std::snprintf(buf, sizeof(buf), "%.2f", pos.entry_price);
                ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

                ImGui::TableSetColumnIndex(4);
                std::snprintf(buf, sizeof(buf), "%.2f", pos.current_price);
                ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

                ImGui::TableSetColumnIndex(5);
                ImVec4 pnl_col = pos.unrealized_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
                std::snprintf(buf, sizeof(buf), "%s%.2f",
                    pos.unrealized_pnl >= 0 ? "+" : "", pos.unrealized_pnl);
                ImGui::TextColored(pnl_col, "%s", buf);

                ImGui::TableSetColumnIndex(6);
                std::snprintf(buf, sizeof(buf), "%.0fx", pos.leverage);
                ImGui::TextColored(TEXT_DIM, "%s", buf);
            }

            ImGui::EndTable();
        }
        return;
    }

    // Paper mode — show simulated positions
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading portfolio...");
        return;
    }

    if (positions_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No open positions");
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##positions", 7, flags)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("PnL", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Lev", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (auto& pos : positions_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", pos.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImVec4 side_col = (pos.side == "long") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", pos.side == "long" ? "LONG" : "SHORT");

            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.4f", pos.quantity);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.2f", pos.entry_price);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(4);
            std::snprintf(buf, sizeof(buf), "%.2f", pos.current_price);
            ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

            ImGui::TableSetColumnIndex(5);
            ImVec4 pnl_col = pos.unrealized_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
            std::snprintf(buf, sizeof(buf), "%s%.2f",
                pos.unrealized_pnl >= 0 ? "+" : "", pos.unrealized_pnl);
            ImGui::TextColored(pnl_col, "%s", buf);

            ImGui::TableSetColumnIndex(6);
            std::snprintf(buf, sizeof(buf), "%.0fx", pos.leverage);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::render_orders_tab() {
    bool is_live = (trading_mode_ == TradingMode::Live);

    ImGui::TextColored(is_live ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : ImVec4(0.0f, 0.7f, 0.3f, 1.0f),
                       is_live ? "[LIVE ORDERS]" : "[PAPER ORDERS]");

    if (is_live) {
        if (!has_credentials_) {
            ImGui::TextColored(TEXT_DIM, "API credentials required.");
            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            if (ImGui::SmallButton("Configure##orders")) {
                show_credentials_popup_ = true;
                load_exchange_credentials();
            }
            ImGui::PopStyleColor();
            return;
        }

        std::lock_guard<std::mutex> lock(data_mutex_);

        if (live_orders_fetching_) {
            theme::LoadingSpinner("Fetching orders...");
            return;
        }

        if (live_orders_.empty()) {
            ImGui::TextColored(TEXT_DIM, "No open orders on %s", exchange_id_.c_str());
            return;
        }

        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##live_orders", 7, flags)) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            char buf[32];
            for (auto& ord : live_orders_) {
                ImGui::TableNextRow();
                ImGui::PushID(ord.id.c_str());

                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(TEXT_PRIMARY, "%s", ord.symbol.c_str());

                ImGui::TableSetColumnIndex(1);
                ImVec4 side_col = (ord.side == "buy") ? MARKET_GREEN : MARKET_RED;
                ImGui::TextColored(side_col, "%s", ord.side == "buy" ? "BUY" : "SELL");

                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(TEXT_SECONDARY, "%s", ord.type.c_str());

                ImGui::TableSetColumnIndex(3);
                std::snprintf(buf, sizeof(buf), "%.4f", ord.quantity);
                ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

                ImGui::TableSetColumnIndex(4);
                if (ord.price > 0.0) {
                    std::snprintf(buf, sizeof(buf), "%.2f", ord.price);
                    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
                } else {
                    ImGui::TextColored(TEXT_DIM, "MKT");
                }

                ImGui::TableSetColumnIndex(5);
                ImVec4 status_col = TEXT_DIM;
                if (ord.status == "open") status_col = WARNING;
                else if (ord.status == "closed" || ord.status == "filled") status_col = MARKET_GREEN;
                else if (ord.status == "canceled" || ord.status == "cancelled") status_col = MARKET_RED;
                ImGui::TextColored(status_col, "%s", ord.status.c_str());

                ImGui::TableSetColumnIndex(6);
                if (ord.status == "open") {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_Text, MARKET_RED);
                    if (ImGui::SmallButton("Cancel")) {
                        cancel_order(ord.id);
                    }
                    ImGui::PopStyleColor(2);
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        return;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading orders...");
        return;
    }

    if (orders_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No orders");
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##orders", 7, flags)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (auto& ord : orders_) {
            ImGui::TableNextRow();
            ImGui::PushID(ord.id.c_str());

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", ord.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImVec4 side_col = (ord.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", ord.side == "buy" ? "BUY" : "SELL");

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(TEXT_SECONDARY, "%s", ord.order_type.c_str());

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.4f", ord.quantity);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(4);
            if (ord.price.has_value()) {
                std::snprintf(buf, sizeof(buf), "%.2f", *ord.price);
                ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
            } else {
                ImGui::TextColored(TEXT_DIM, "MKT");
            }

            ImGui::TableSetColumnIndex(5);
            ImVec4 status_col = TEXT_DIM;
            if (ord.status == "pending") status_col = WARNING;
            else if (ord.status == "filled") status_col = MARKET_GREEN;
            else if (ord.status == "cancelled") status_col = MARKET_RED;
            ImGui::TextColored(status_col, "%s", ord.status.c_str());

            ImGui::TableSetColumnIndex(6);
            if (ord.status == "pending") {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_Text, MARKET_RED);
                if (ImGui::SmallButton("Cancel")) {
                    cancel_order(ord.id);
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::render_history_tab() {
    bool is_live = (trading_mode_ == TradingMode::Live);
    ImGui::TextColored(is_live ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : ImVec4(0.0f, 0.7f, 0.3f, 1.0f),
                       is_live ? "[LIVE HISTORY]" : "[PAPER HISTORY]");

    if (is_live) {
        if (!has_credentials_) {
            ImGui::TextColored(TEXT_DIM, "API credentials required.");
            return;
        }

        std::lock_guard<std::mutex> lock(data_mutex_);

        if (live_trades_fetching_) {
            theme::LoadingSpinner("Fetching trade history...");
            return;
        }

        if (live_trades_.empty()) {
            ImGui::TextColored(TEXT_DIM, "No trade history on %s", exchange_id_.c_str());
            // Trigger initial fetch
            if (!live_data_loaded_) {
                live_data_loaded_ = true;
                async_fetch_live_trades();
            }
            return;
        }

        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##live_trades_hist", 6, flags)) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Fee", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            char buf[32];
            for (const auto& t : live_trades_) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(TEXT_PRIMARY, "%s", t.symbol.c_str());

                ImGui::TableSetColumnIndex(1);
                ImVec4 side_col = (t.side == "buy") ? MARKET_GREEN : MARKET_RED;
                ImGui::TextColored(side_col, "%s", t.side == "buy" ? "BUY" : "SELL");

                ImGui::TableSetColumnIndex(2);
                std::snprintf(buf, sizeof(buf), "%.2f", t.price);
                ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

                ImGui::TableSetColumnIndex(3);
                std::snprintf(buf, sizeof(buf), "%.4f", t.quantity);
                ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

                ImGui::TableSetColumnIndex(4);
                std::snprintf(buf, sizeof(buf), "%.4f", t.fee);
                ImGui::TextColored(TEXT_DIM, "%s", buf);

                ImGui::TableSetColumnIndex(5);
                ImGui::TextColored(TEXT_DIM, "%s", t.timestamp.c_str());
            }

            ImGui::EndTable();
        }
        return;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading history...");
        return;
    }

    if (trades_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No trade history");
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##history", 6, flags)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Fee", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("PnL", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (auto& t : trades_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", t.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImVec4 side_col = (t.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", t.side == "buy" ? "BUY" : "SELL");

            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.2f", t.price);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.4f", t.quantity);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(4);
            std::snprintf(buf, sizeof(buf), "%.4f", t.fee);
            ImGui::TextColored(TEXT_DIM, "%s", buf);

            ImGui::TableSetColumnIndex(5);
            ImVec4 pnl_col = t.pnl >= 0 ? MARKET_GREEN : MARKET_RED;
            std::snprintf(buf, sizeof(buf), "%s%.2f", t.pnl >= 0 ? "+" : "", t.pnl);
            ImGui::TextColored(pnl_col, "%s", buf);
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::render_stats_tab() {
    bool is_live = (trading_mode_ == TradingMode::Live);
    ImGui::TextColored(is_live ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : ImVec4(0.0f, 0.7f, 0.3f, 1.0f),
                       is_live ? "[LIVE STATS]" : "[PAPER STATS]");

    if (is_live) {
        ImGui::TextColored(TEXT_DIM, "Live statistics not available yet.");
        return;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    char buf[32];

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading stats...");
        return;
    }

    ImGui::Columns(2, "##stats_cols", false);

    // Compute unrealized PnL from open positions
    double unrealized_pnl = 0.0;
    for (const auto& pos : positions_) {
        unrealized_pnl += pos.unrealized_pnl;
    }

    ImGui::TextColored(TEXT_DIM, "Realized PnL");
    ImVec4 pnl_col = stats_.total_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
    std::snprintf(buf, sizeof(buf), "%s%.2f", stats_.total_pnl >= 0 ? "+" : "", stats_.total_pnl);
    ImGui::TextColored(pnl_col, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Unrealized PnL");
    ImVec4 upnl_col = unrealized_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
    std::snprintf(buf, sizeof(buf), "%s%.2f", unrealized_pnl >= 0 ? "+" : "", unrealized_pnl);
    ImGui::TextColored(upnl_col, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Win Rate");
    std::snprintf(buf, sizeof(buf), "%.1f%%", stats_.win_rate);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Total Trades");
    std::snprintf(buf, sizeof(buf), "%lld", (long long)stats_.total_trades);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Balance");
    std::snprintf(buf, sizeof(buf), "%.2f", portfolio_.balance);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

    ImGui::NextColumn();

    ImGui::TextColored(TEXT_DIM, "Winning");
    std::snprintf(buf, sizeof(buf), "%lld", (long long)stats_.winning_trades);
    ImGui::TextColored(MARKET_GREEN, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Losing");
    std::snprintf(buf, sizeof(buf), "%lld", (long long)stats_.losing_trades);
    ImGui::TextColored(MARKET_RED, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Largest Win");
    std::snprintf(buf, sizeof(buf), "+%.2f", stats_.largest_win);
    ImGui::TextColored(MARKET_GREEN, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Largest Loss");
    std::snprintf(buf, sizeof(buf), "%.2f", stats_.largest_loss);
    ImGui::TextColored(MARKET_RED, "%s", buf);

    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Reset Portfolio", ImVec2(-1, 0))) {
        portfolio_ = trading::pt_reset_portfolio(portfolio_id_);
        refresh_portfolio_data();
        LOG_INFO(TAG, "Portfolio reset");
    }
    ImGui::PopStyleColor();
}

// ============================================================================
// Trades tab — Time & Sales feed
// ============================================================================
void CryptoTradingScreen::render_trades_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (recent_trades_.empty()) {
        if (trades_fetching_) {
            ImGui::TextColored(TEXT_DIM, "Loading trades...");
        } else {
            ImGui::TextColored(TEXT_DIM, "No trades yet");
        }
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##trades_feed", 4, flags)) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (const auto& t : recent_trades_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            time_t ts = t.timestamp / 1000;
            struct tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &ts);
#else
            localtime_r(&ts, &tm_buf);
#endif
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
            ImGui::TextColored(TEXT_DIM, "%s", buf);

            ImGui::TableSetColumnIndex(1);
            ImVec4 col = (t.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(col, "%s", t.side == "buy" ? "BUY" : "SELL");

            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.2f", t.price);
            ImGui::TextColored(col, "%s", buf);

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.4f", t.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::async_fetch_trades() {
    if (trades_fetching_) return;
    trades_fetching_ = true;

    const std::string sym = selected_symbol_;
    launch_async(std::thread([this, sym]() {
        try {
            auto trades = trading::ExchangeService::instance().fetch_trades(sym, 100);
            std::lock_guard<std::mutex> lock(data_mutex_);
            recent_trades_.clear();
            for (const auto& t : trades) {
                recent_trades_.push_back({t.id, t.side, t.price, t.amount, t.timestamp});
            }
        } catch (const std::exception& e) {
            LOG_ERROR("CryptoTrading", "Failed to fetch trades: %s", e.what());
        }
        trades_fetching_ = false;
    }));
}

// ============================================================================
// Market Info tab — funding rate, open interest, fees
// ============================================================================
void CryptoTradingScreen::render_market_info_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    char buf[64];

    if (!market_info_.has_data) {
        if (market_info_fetching_) {
            ImGui::TextColored(TEXT_DIM, "Loading market info...");
        } else {
            // Not yet fetched — show a trigger hint
            ImGui::TextColored(TEXT_DIM, "Fetching market info...");
        }
        return;
    }

    ImGui::Columns(2, "##mktinfo_cols", false);

    // --- Fees (always available for any symbol) ---
    ImGui::TextColored(TEXT_DIM, "Maker Fee");
    if (market_info_.maker_fee > 0.0) {
        std::snprintf(buf, sizeof(buf), "%.4f%%", market_info_.maker_fee * 100.0);
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A");
    }
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Taker Fee");
    if (market_info_.taker_fee > 0.0) {
        std::snprintf(buf, sizeof(buf), "%.4f%%", market_info_.taker_fee * 100.0);
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A");
    }
    ImGui::Spacing();

    // --- Perp-only fields (show N/A for spot) ---
    ImGui::TextColored(TEXT_DIM, "Funding Rate");
    if (market_info_.funding_rate != 0.0) {
        std::snprintf(buf, sizeof(buf), "%.4f%%", market_info_.funding_rate * 100.0);
        ImVec4 fr_col = market_info_.funding_rate >= 0 ? MARKET_GREEN : MARKET_RED;
        ImGui::TextColored(fr_col, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A (spot)");
    }
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Mark Price");
    if (market_info_.mark_price > 0.0) {
        std::snprintf(buf, sizeof(buf), "%.4f", market_info_.mark_price);
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A (spot)");
    }
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Index Price");
    if (market_info_.index_price > 0.0) {
        std::snprintf(buf, sizeof(buf), "%.4f", market_info_.index_price);
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A (spot)");
    }

    ImGui::NextColumn();

    ImGui::TextColored(TEXT_DIM, "Open Interest");
    if (market_info_.open_interest > 0.0) {
        std::snprintf(buf, sizeof(buf), "%.2f", market_info_.open_interest);
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A (spot)");
    }
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "OI Value (USD)");
    if (market_info_.open_interest_value > 0.0) {
        const double oi_val = market_info_.open_interest_value;
        if (oi_val > 1e9)      std::snprintf(buf, sizeof(buf), "%.2fB", oi_val / 1e9);
        else if (oi_val > 1e6) std::snprintf(buf, sizeof(buf), "%.2fM", oi_val / 1e6);
        else                   std::snprintf(buf, sizeof(buf), "%.0f",  oi_val);
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    } else {
        ImGui::TextColored(TEXT_DIM, "N/A (spot)");
    }
    ImGui::Spacing();

    if (market_info_.next_funding_time > 0) {
        ImGui::TextColored(TEXT_DIM, "Next Funding");
        const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const int64_t diff_s = (market_info_.next_funding_time - now_ms) / 1000;
        if (diff_s > 0) {
            const int h = static_cast<int>(diff_s / 3600);
            const int m = static_cast<int>((diff_s % 3600) / 60);
            std::snprintf(buf, sizeof(buf), "%dh %dm", h, m);
        } else {
            std::snprintf(buf, sizeof(buf), "Now");
        }
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    }

    ImGui::Columns(1);

    // Refresh hint
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "(refreshes every %.0fs)", MARKET_INFO_INTERVAL);
}

void CryptoTradingScreen::async_fetch_market_info() {
    if (market_info_fetching_) return;
    market_info_fetching_ = true;

    const std::string sym = selected_symbol_;
    launch_async(std::thread([this, sym]() {
        auto& svc = trading::ExchangeService::instance();

        // Fetch funding rate + OI (perp only — scripts return nulls gracefully for spot)
        const auto fr = svc.fetch_funding_rate(sym);
        const auto oi = svc.fetch_open_interest(sym);

        // Fetch trading fees — always available for any symbol on any exchange.
        // fetch_trading_fees.py with a symbol arg returns: {"symbol":..., "maker":..., "taker":...}
        double maker_fee = 0.0;
        double taker_fee = 0.0;
        try {
            const auto fees_j = svc.fetch_trading_fees(sym);
            if (fees_j.contains("data")) {
                const auto& d = fees_j["data"];
                if (d.contains("maker") && !d["maker"].is_null())
                    maker_fee = d["maker"].get<double>();
                if (d.contains("taker") && !d["taker"].is_null())
                    taker_fee = d["taker"].get<double>();
            }
        } catch (const std::exception& e) {
            LOG_DEBUG(TAG, "Trading fees fetch failed: %s", e.what());
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            market_info_.funding_rate       = fr.funding_rate;
            market_info_.mark_price         = fr.mark_price;
            market_info_.index_price        = fr.index_price;
            market_info_.next_funding_time  = fr.next_funding_timestamp;
            market_info_.open_interest      = oi.open_interest;
            market_info_.open_interest_value = oi.open_interest_value;
            market_info_.maker_fee          = maker_fee;
            market_info_.taker_fee          = taker_fee;
            market_info_.has_data           = true;  // always mark ready after fetch attempt
        }
        market_info_fetching_ = false;
    }));
}

} // namespace fincept::crypto
