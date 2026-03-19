#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/theme.h"
#include "trading/exchange_service.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>

namespace fincept::crypto {

using namespace theme::colors;

void CryptoTradingScreen::render_chart_area(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##crypto_chart", ImVec2(w, h), true);

    // Header: CHART label + symbol + WS indicator
    ImGui::TextColored(ACCENT, "CHART");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "%s", selected_symbol_.c_str());

    ImGui::SameLine(0, 8);
    bool ws_ok = trading::ExchangeService::instance().is_ws_connected();
    ImGui::TextColored(ws_ok ? MARKET_GREEN : WARNING, ws_ok ? "[WS LIVE]" : "[REST]");

    ImGui::SameLine(0, 16);

    // Timeframe buttons
    static const char* timeframes[] = {"1m", "5m", "15m", "1h", "4h", "1d"};
    for (int i = 0; i < 6; i++) {
        if (i > 0) ImGui::SameLine(0, 2);
        bool active = (i == selected_timeframe_);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT_DIM : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? TEXT_PRIMARY : TEXT_DIM);
        if (ImGui::SmallButton(timeframes[i])) {
            if (selected_timeframe_ != i) {
                selected_timeframe_ = i;
                candle_timeframe_ = timeframes[i];
                // Re-fetch candles for new timeframe
                candles_loading_ = true;
                candles_fetching_ = true;
                const std::string sym = selected_symbol_;
                std::string tf = candle_timeframe_;
                launch_async(std::thread([this, sym, tf]() {
                    try {
                        auto ohlcv = trading::ExchangeService::instance().fetch_ohlcv(sym, tf, OHLCV_FETCH_COUNT);
                        LOG_INFO(TAG, "<<< OHLCV %s received: %d candles", tf.c_str(), (int)ohlcv.size());
                        {
                            std::lock_guard<std::mutex> lock(data_mutex_);
                            candles_ = std::move(ohlcv);
                        }
                        candles_loading_ = false;
                        candles_fetching_ = false;
                        success_count_++;
                    } catch (const std::exception& e) {
                        LOG_ERROR(TAG, "<<< OHLCV fetch failed: %s", e.what());
                        candles_loading_ = false;
                        candles_fetching_ = false;
                        error_count_++;
                    }
                }));
            }
        }
        ImGui::PopStyleColor(2);
    }

    // Candle count
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, "[%d candles]", (int)candles_.size());
    }

    ImGui::Separator();

    float chart_w = ImGui::GetContentRegionAvail().x;
    float chart_h = ImGui::GetContentRegionAvail().y;

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (chart_h > 60 && !candles_.empty()) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        render_candlestick_chart(p, chart_w, chart_h);
        ImGui::Dummy(ImVec2(chart_w, chart_h));
    } else if (candles_loading_) {
        ImGui::Dummy(ImVec2(0, 20));
        theme::LoadingSpinner("Loading OHLCV data...");
    } else if (chart_h > 60 && current_ticker_.last > 0) {
        // No candle data yet but have ticker — show price placeholder
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        char price_text[64];
        std::snprintf(price_text, sizeof(price_text), "%.2f", current_ticker_.last);
        ImVec2 text_size = ImGui::CalcTextSize(price_text);
        ImVec2 text_pos(p.x + (chart_w - text_size.x) / 2, p.y + (chart_h - text_size.y) / 2);
        dl->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), price_text);
        ImGui::Dummy(ImVec2(chart_w, chart_h));
    } else {
        ImGui::Dummy(ImVec2(0, 20));
        theme::LoadingSpinner("Loading chart data...");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Candlestick Chart — renders OHLCV candles via ImDrawList
// ============================================================================
void CryptoTradingScreen::render_candlestick_chart(ImVec2 origin, float chart_w, float chart_h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Colors
    ImU32 grid_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
    ImU32 green = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.8f, 0.4f, 1.0f));
    ImU32 red = ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImU32 green_dim = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.6f, 0.3f, 0.6f));
    ImU32 red_dim = ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.15f, 0.15f, 0.6f));
    ImU32 text_col = ImGui::ColorConvertFloat4ToU32(TEXT_DIM);
    ImU32 crosshair_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.5f, 0.5f));

    // Reserve right margin for price axis
    float price_axis_w = 70.0f;
    float plot_w = chart_w - price_axis_w;
    if (plot_w < 50) plot_w = chart_w;

    // Determine how many candles fit
    float candle_width = 6.0f;
    float candle_gap = 2.0f;
    float candle_step = candle_width + candle_gap;
    int max_visible = (int)(plot_w / candle_step);
    if (max_visible < 5) max_visible = 5;

    // Get visible range (show most recent candles)
    int total = (int)candles_.size();
    int start_idx = total > max_visible ? total - max_visible : 0;
    int visible_count = total - start_idx;
    if (visible_count <= 0) return;

    // Find price range
    double price_high = -1e18;
    double price_low = 1e18;
    for (int i = start_idx; i < total; i++) {
        const auto& c = candles_[i];
        if (c.high > price_high) price_high = c.high;
        if (c.low < price_low) price_low = c.low;
    }

    // Add 2% padding
    double range = price_high - price_low;
    if (range < 0.01) range = 1.0;
    double padding = range * 0.02;
    price_high += padding;
    price_low -= padding;
    range = price_high - price_low;

    // Helper: price to Y coordinate
    auto price_to_y = [&](double price) -> float {
        return origin.y + chart_h * (1.0f - (float)((price - price_low) / range));
    };

    // Draw grid lines (horizontal)
    int grid_lines = 5;
    for (int i = 0; i <= grid_lines; i++) {
        float y = origin.y + chart_h * i / (float)grid_lines;
        dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + plot_w, y), grid_col);

        // Price labels on right axis
        if (plot_w < chart_w) {
            double price = price_high - (range * i / grid_lines);
            char label[32];
            if (price >= 1000)
                std::snprintf(label, sizeof(label), "%.1f", price);
            else if (price >= 1)
                std::snprintf(label, sizeof(label), "%.2f", price);
            else
                std::snprintf(label, sizeof(label), "%.4f", price);
            dl->AddText(ImVec2(origin.x + plot_w + 4, y - 6), text_col, label);
        }
    }

    // Draw vertical grid lines
    int v_grid = 6;
    for (int i = 1; i < v_grid; i++) {
        float x = origin.x + plot_w * i / (float)v_grid;
        dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + chart_h), grid_col);
    }

    // Draw candles
    float x_start = origin.x + plot_w - visible_count * candle_step;

    for (int i = 0; i < visible_count; i++) {
        const auto& c = candles_[start_idx + i];
        float cx = x_start + i * candle_step + candle_width * 0.5f;

        bool bullish = c.close >= c.open;
        ImU32 body_col = bullish ? green : red;
        ImU32 wick_col = bullish ? green_dim : red_dim;

        // Wick (high-low line)
        float wick_x = cx;
        float wick_top = price_to_y(c.high);
        float wick_bot = price_to_y(c.low);
        dl->AddLine(ImVec2(wick_x, wick_top), ImVec2(wick_x, wick_bot), wick_col, 1.0f);

        // Body (open-close rect)
        float body_top = price_to_y(bullish ? c.close : c.open);
        float body_bot = price_to_y(bullish ? c.open : c.close);
        float body_min_h = 1.0f;
        if (body_bot - body_top < body_min_h) {
            body_bot = body_top + body_min_h;
        }

        float body_left = cx - candle_width * 0.5f;
        float body_right = cx + candle_width * 0.5f;
        dl->AddRectFilled(ImVec2(body_left, body_top), ImVec2(body_right, body_bot), body_col);
    }

    // Volume bars at bottom (subtle, 15% of chart height)
    double vol_max = 0;
    for (int i = start_idx; i < total; i++) {
        if (candles_[i].volume > vol_max) vol_max = candles_[i].volume;
    }

    if (vol_max > 0) {
        float vol_zone = chart_h * 0.15f;
        float vol_base = origin.y + chart_h;

        for (int i = 0; i < visible_count; i++) {
            const auto& c = candles_[start_idx + i];
            float cx = x_start + i * candle_step + candle_width * 0.5f;
            float vol_h = (float)(c.volume / vol_max) * vol_zone;
            bool bullish = c.close >= c.open;

            ImU32 vol_col = bullish
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.25f, 0.3f))
                : ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.1f, 0.1f, 0.3f));

            float body_left = cx - candle_width * 0.5f;
            float body_right = cx + candle_width * 0.5f;
            dl->AddRectFilled(ImVec2(body_left, vol_base - vol_h),
                              ImVec2(body_right, vol_base), vol_col);
        }
    }

    // Current price line
    if (current_ticker_.last > 0 && current_ticker_.last >= price_low && current_ticker_.last <= price_high) {
        float y = price_to_y(current_ticker_.last);
        ImU32 price_line_col = ImGui::ColorConvertFloat4ToU32(
            current_ticker_.change >= 0 ? MARKET_GREEN : MARKET_RED);

        // Dashed line effect
        float dash_len = 6.0f;
        for (float x = origin.x; x < origin.x + plot_w; x += dash_len * 2) {
            float end = x + dash_len;
            if (end > origin.x + plot_w) end = origin.x + plot_w;
            dl->AddLine(ImVec2(x, y), ImVec2(end, y), price_line_col, 1.0f);
        }

        // Price tag on right
        if (plot_w < chart_w) {
            char tag[32];
            if (current_ticker_.last >= 1000)
                std::snprintf(tag, sizeof(tag), "%.1f", current_ticker_.last);
            else
                std::snprintf(tag, sizeof(tag), "%.2f", current_ticker_.last);

            ImVec2 tag_size = ImGui::CalcTextSize(tag);
            float tag_x = origin.x + plot_w + 2;
            float tag_y = y - tag_size.y * 0.5f;
            dl->AddRectFilled(ImVec2(tag_x, tag_y - 1),
                              ImVec2(tag_x + tag_size.x + 6, tag_y + tag_size.y + 1),
                              price_line_col);
            dl->AddText(ImVec2(tag_x + 3, tag_y),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1)), tag);
        }
    }

    // Crosshair on hover
    ImVec2 mouse = ImGui::GetMousePos();
    if (mouse.x >= origin.x && mouse.x <= origin.x + plot_w &&
        mouse.y >= origin.y && mouse.y <= origin.y + chart_h) {

        dl->AddLine(ImVec2(origin.x, mouse.y), ImVec2(origin.x + plot_w, mouse.y), crosshair_col);
        dl->AddLine(ImVec2(mouse.x, origin.y), ImVec2(mouse.x, origin.y + chart_h), crosshair_col);

        // Price at cursor Y
        double hover_price = price_high - range * ((mouse.y - origin.y) / chart_h);
        char hover_label[32];
        std::snprintf(hover_label, sizeof(hover_label), "%.2f", hover_price);
        dl->AddText(ImVec2(origin.x + plot_w + 4, mouse.y - 6),
                    ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), hover_label);
    }
}

// ============================================================================
// Bottom Panel — Positions / Orders / History / Stats
// ============================================================================
void CryptoTradingScreen::render_bottom_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_bottom", ImVec2(w, h), true);

    struct TabDef { const char* label; BottomTab tab; };
    static const TabDef tabs[] = {
        {"Positions", BottomTab::Positions},
        {"Orders", BottomTab::Orders},
        {"History", BottomTab::History},
        {"Trades", BottomTab::Trades},
        {"Market Info", BottomTab::MarketInfo},
        {"Stats", BottomTab::Stats},
    };

    for (int i = 0; i < 6; i++) {
        if (i > 0) ImGui::SameLine(0, 2);
        bool active = (bottom_tab_ == tabs[i].tab);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT_BG : ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ACCENT : TEXT_DIM);
        if (ImGui::SmallButton(tabs[i].label)) {
            bottom_tab_ = tabs[i].tab;
        }
        ImGui::PopStyleColor(2);
    }

    // Count badges
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ImGui::SameLine(0, 8);

        if (trading_mode_ == TradingMode::Live && has_credentials_) {
            if (!live_positions_.empty()) {
                char badge[16];
                std::snprintf(badge, sizeof(badge), "[%d pos]", (int)live_positions_.size());
                ImGui::TextColored(ACCENT, "%s", badge);
                ImGui::SameLine(0, 4);
            }
            int open_count = 0;
            for (auto& o : live_orders_) if (o.status == "open") open_count++;
            if (open_count > 0) {
                char badge[16];
                std::snprintf(badge, sizeof(badge), "[%d ord]", open_count);
                ImGui::TextColored(WARNING, "%s", badge);
            }
        } else if (trading_mode_ == TradingMode::Paper) {
            if (!positions_.empty()) {
                char badge[16];
                std::snprintf(badge, sizeof(badge), "[%d pos]", (int)positions_.size());
                ImGui::TextColored(ACCENT, "%s", badge);
                ImGui::SameLine(0, 4);
            }
            int pending = 0;
            for (auto& o : orders_) if (o.status == "pending") pending++;
            if (pending > 0) {
                char badge[16];
                std::snprintf(badge, sizeof(badge), "[%d ord]", pending);
                ImGui::TextColored(WARNING, "%s", badge);
            }
        }
    }

    ImGui::Separator();

    ImGui::BeginChild("##bottom_content", ImVec2(0, 0), false);
    switch (bottom_tab_) {
        case BottomTab::Positions:  render_positions_tab(); break;
        case BottomTab::Orders:     render_orders_tab(); break;
        case BottomTab::History:    render_history_tab(); break;
        case BottomTab::Trades:     render_trades_tab(); break;
        case BottomTab::MarketInfo: render_market_info_tab(); break;
        case BottomTab::Stats:      render_stats_tab(); break;
    }
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}


} // namespace fincept::crypto
