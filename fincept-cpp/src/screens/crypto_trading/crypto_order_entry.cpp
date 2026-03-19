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

void CryptoTradingScreen::render_order_entry(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_order_entry", ImVec2(w, h), true);

    // Mode indicator
    bool is_paper = (trading_mode_ == TradingMode::Paper);
    ImGui::TextColored(ACCENT, "ORDER ENTRY");
    ImGui::SameLine(0, 6);
    if (is_paper) {
        ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.3f, 1.0f), "[PAPER]");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "[LIVE]");
    }
    ImGui::Separator();

    float input_w = w - 24;

    // Side: Buy / Sell
    ImGui::TextColored(TEXT_DIM, "Side");
    {
        float btn_w = (input_w - 4) / 2;
        bool is_buy = (order_form_.side_idx == 0);

        ImGui::PushStyleColor(ImGuiCol_Button, is_buy ? ImVec4(0.0f, 0.55f, 0.25f, 1.0f) : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, is_buy ? TEXT_PRIMARY : TEXT_DIM);
        if (ImGui::Button("BUY", ImVec2(btn_w, 0))) order_form_.side_idx = 0;
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 4);

        bool is_sell = (order_form_.side_idx == 1);
        ImGui::PushStyleColor(ImGuiCol_Button, is_sell ? ImVec4(0.65f, 0.1f, 0.1f, 1.0f) : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, is_sell ? TEXT_PRIMARY : TEXT_DIM);
        if (ImGui::Button("SELL", ImVec2(btn_w, 0))) order_form_.side_idx = 1;
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // Order type
    ImGui::TextColored(TEXT_DIM, "Type");
    ImGui::PushItemWidth(input_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    static const char* types[] = {"Market", "Limit", "Stop", "Stop Limit"};
    ImGui::Combo("##order_type", &order_form_.type_idx, types, 4);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Quantity
    ImGui::TextColored(TEXT_DIM, "Quantity");
    ImGui::PushItemWidth(input_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputText("##qty", order_form_.quantity_buf, sizeof(order_form_.quantity_buf),
                     ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Quick quantity buttons
    {
        float btn_w = (input_w - 12) / 4;
        const char* pcts[] = {"25%", "50%", "75%", "100%"};
        float pct_vals[] = {0.25f, 0.50f, 0.75f, 1.00f};
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            if (ImGui::Button(pcts[i], ImVec2(btn_w, 0))) {
                if (current_ticker_.last > 0 && portfolio_.balance > 0) {
                    double max_qty = (portfolio_.balance * pct_vals[i]) / current_ticker_.last;
                    std::snprintf(order_form_.quantity_buf, sizeof(order_form_.quantity_buf),
                                  "%.6f", max_qty);
                }
            }
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::Spacing();

    // Price (for limit/stop orders)
    if (order_form_.type_idx >= 1) {
        ImGui::TextColored(TEXT_DIM, "Price");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##price", order_form_.price_buf, sizeof(order_form_.price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    // Stop price (for stop/stoplimit)
    if (order_form_.type_idx >= 2) {
        ImGui::TextColored(TEXT_DIM, "Stop Price");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##stop_price", order_form_.stop_price_buf, sizeof(order_form_.stop_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    // Stop Loss / Take Profit — optional risk management, non-Market orders only
    if (order_form_.type_idx != 0) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("-- Risk Mgmt (optional) --");
        ImGui::PopStyleColor();

        ImGui::TextColored(TEXT_DIM, "Stop Loss");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##sl_price", order_form_.sl_price_buf, sizeof(order_form_.sl_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::TextColored(TEXT_DIM, "Take Profit");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##tp_price", order_form_.tp_price_buf, sizeof(order_form_.tp_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    ImGui::Spacing();

    // Reduce only checkbox
    ImGui::Checkbox("Reduce Only", &order_form_.reduce_only);

    ImGui::Spacing();

    // --- Fee Estimation (shown when market info has loaded and notional > 0) ---
    if (market_info_.has_data) {
        const double qty   = std::atof(order_form_.quantity_buf);
        // Market orders use last ticker price; limit/stop orders use entered price
        const double price = (order_form_.type_idx == 0)
                             ? current_ticker_.last
                             : std::atof(order_form_.price_buf);

        if (qty > 0.0 && price > 0.0) {
            // Limit orders go on the book → maker fee; all others fill immediately → taker fee
            const bool   is_maker      = (order_form_.type_idx == 1);
            const double effective_fee = is_maker ? market_info_.maker_fee : market_info_.taker_fee;
            const double notional      = qty * price;
            const double fee_amount    = notional * effective_fee;
            const double total_incl    = notional + fee_amount;

            ImGui::Separator();
            {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Fee(%.2f%% %s)",
                              effective_fee * 100.0, is_maker ? "mkr" : "tkr");
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                ImGui::Text("%s", buf);
                ImGui::SameLine(input_w - 72);
                std::snprintf(buf, sizeof(buf), "%.4f", fee_amount);
                ImGui::Text("%s", buf);

                ImGui::Text("Total w/fee");
                ImGui::SameLine(input_w - 72);
                std::snprintf(buf, sizeof(buf), "%.2f", total_incl);
                ImGui::Text("%s", buf);
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
        }
    }

    ImGui::Spacing();

    // Submit button — clearly labeled with mode
    {
        bool is_buy = (order_form_.side_idx == 0);
        ImVec4 btn_col = is_buy ? ImVec4(0.0f, 0.55f, 0.25f, 1.0f) : ImVec4(0.65f, 0.1f, 0.1f, 1.0f);

        // Show base symbol without quote (e.g., "BTC" not "BTC/USDT")
        std::string base_sym = selected_symbol_;
        auto slash_pos = base_sym.find('/');
        if (slash_pos != std::string::npos) base_sym = base_sym.substr(0, slash_pos);

        char label[64];
        if (is_paper) {
            std::snprintf(label, sizeof(label), "%s %s (Paper)",
                          is_buy ? "BUY" : "SELL", base_sym.c_str());
        } else {
            std::snprintf(label, sizeof(label), "%s %s (LIVE)",
                          is_buy ? "BUY" : "SELL", base_sym.c_str());
            // Add red border for live orders as a safety indicator
            btn_col = is_buy ? ImVec4(0.0f, 0.4f, 0.2f, 1.0f) : ImVec4(0.6f, 0.05f, 0.05f, 1.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(btn_col.x + 0.1f, btn_col.y + 0.1f, btn_col.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        if (order_submitting_.load()) {
            ImGui::Button("Submitting...", ImVec2(input_w, 28));
        } else if (ImGui::Button(label, ImVec2(input_w, 28))) {
            submit_order();
        }
        ImGui::PopStyleColor(3);

        // Warning for live mode
        if (!is_paper) {
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.1f, 1.0f), "Real money at risk!");
        }
    }

    // Error / Success messages
    if (!order_form_.error.empty()) {
        ImGui::TextColored(MARKET_RED, "%s", order_form_.error.c_str());
    }
    if (!order_form_.success.empty()) {
        ImGui::TextColored(MARKET_GREEN, "%s", order_form_.success.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Order Book — right panel bottom
// ============================================================================
void CryptoTradingScreen::render_orderbook(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_orderbook", ImVec2(w, h), true);

    // Title + spread
    ImGui::TextColored(ACCENT, "ORDER BOOK");
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (ob_spread_ > 0) {
            ImGui::SameLine();
            char spread_buf[32];
            std::snprintf(spread_buf, sizeof(spread_buf), "Spread:%.2f(%.3f%%)", ob_spread_, ob_spread_pct_);
            ImGui::TextColored(TEXT_DIM, "%s", spread_buf);
        }
    }
    if (ob_fetching_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "[*]");
    }

    // Mode switcher — 4 segmented buttons
    {
        struct ModeBtn { const char* label; ObViewMode mode; };
        static constexpr ModeBtn k_modes[] = {
            {"OB",    ObViewMode::Book},
            {"VOL",   ObViewMode::Volume},
            {"IMBAL", ObViewMode::Imbalance},
            {"SIG",   ObViewMode::Signals},
        };
        const float btn_w = (w - 28.0f) / 4.0f;
        ImGui::SameLine(0, 6);
        for (int i = 0; i < 4; ++i) {
            if (i > 0) ImGui::SameLine(0, 2);
            const bool active = (ob_view_mode_ == k_modes[i].mode);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(0.2f, 0.4f, 0.7f, 1.0f) : BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text,
                active ? TEXT_PRIMARY : TEXT_DIM);
            if (ImGui::SmallButton(k_modes[i].label))
                ob_view_mode_ = k_modes[i].mode;
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::Separator();

    // Empty state guard
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (ob_asks_.empty() && ob_bids_.empty()) {
            if (ob_loading_) {
                theme::LoadingSpinner("Loading order book...");
            } else {
                ImGui::TextColored(TEXT_DIM, "No order book data");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }
    }

    // Dispatch to active mode renderer
    switch (ob_view_mode_) {
        case ObViewMode::Book:      render_ob_mode_book(w);      break;
        case ObViewMode::Volume:    render_ob_mode_volume(w);    break;
        case ObViewMode::Imbalance: render_ob_mode_imbalance(w); break;
        case ObViewMode::Signals:   render_ob_mode_signals(w);   break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ----------------------------------------------------------------------------
// OB Mode: Book — price / amount / cumulative depth bars (default view)
// ----------------------------------------------------------------------------
void CryptoTradingScreen::render_ob_mode_book(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float half_h  = avail_h / 2.0f;
    const float col_w   = (w - 24.0f) / 3.0f;

    double max_cum = 1.0;
    if (!ob_bids_.empty()) max_cum = std::max(max_cum, ob_bids_.back().cumulative);
    if (!ob_asks_.empty()) max_cum = std::max(max_cum, ob_asks_.back().cumulative);

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("PRICE");
    ImGui::SameLine(col_w + 4); ImGui::Text("AMOUNT");
    ImGui::SameLine(col_w * 2 + 8); ImGui::Text("TOTAL");
    ImGui::PopStyleColor();

    // Asks — lowest ask nearest spread
    ImGui::BeginChild("##ob_asks_book", ImVec2(0, half_h - 12), false);
    {
        char buf[32];
        const int start = std::max(0, static_cast<int>(ob_asks_.size()) - OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = start; i < static_cast<int>(ob_asks_.size()); ++i) {
            const auto& lvl = ob_asks_[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = static_cast<float>(lvl.cumulative / max_cum) * row_w;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x + row_w - bar_w, p.y),
                ImVec2(p.x + row_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.1f, 0.1f, 0.15f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_RED, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.cumulative);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();

    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_BRIGHT);
    ImGui::Separator();
    ImGui::PopStyleColor();

    // Bids — highest to lowest
    ImGui::BeginChild("##ob_bids_book", ImVec2(0, 0), false);
    {
        char buf[32];
        const int count = std::min(static_cast<int>(ob_bids_.size()), OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = 0; i < count; ++i) {
            const auto& lvl = ob_bids_[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = static_cast<float>(lvl.cumulative / max_cum) * row_w;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x + row_w - bar_w, p.y),
                ImVec2(p.x + row_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.2f, 0.15f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_GREEN, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.cumulative);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();
}

// ----------------------------------------------------------------------------
// OB Mode: Volume — price / amount / % share of total book
// ----------------------------------------------------------------------------
void CryptoTradingScreen::render_ob_mode_volume(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    const float col_w = (w - 24.0f) / 3.0f;

    double total_vol = 0.0;
    for (const auto& lvl : ob_asks_) total_vol += lvl.amount;
    for (const auto& lvl : ob_bids_) total_vol += lvl.amount;
    if (total_vol <= 0.0) total_vol = 1.0;

    // Normaliser: each bar fills to its share of the avg-level volume
    const double avg_lvl_vol = total_vol / static_cast<double>(OB_MAX_OB_DISPLAY_LEVELS * 2);

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("PRICE");
    ImGui::SameLine(col_w + 4); ImGui::Text("AMOUNT");
    ImGui::SameLine(col_w * 2 + 8); ImGui::Text("VOL%");
    ImGui::PopStyleColor();

    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float half_h  = avail_h / 2.0f;

    ImGui::BeginChild("##ob_asks_vol", ImVec2(0, half_h - 12), false);
    {
        char buf[32];
        const int start = std::max(0, static_cast<int>(ob_asks_.size()) - OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = start; i < static_cast<int>(ob_asks_.size()); ++i) {
            const auto& lvl = ob_asks_[i];
            const double vol_pct = (lvl.amount / total_vol) * 100.0;
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = std::min(static_cast<float>(lvl.amount / (avg_lvl_vol * 2.0)) * row_w, row_w);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y),
                ImVec2(p.x + bar_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.1f, 0.1f, 0.20f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_RED, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.2f%%", vol_pct);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();

    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_BRIGHT);
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::BeginChild("##ob_bids_vol", ImVec2(0, 0), false);
    {
        char buf[32];
        const int count = std::min(static_cast<int>(ob_bids_.size()), OB_MAX_OB_DISPLAY_LEVELS);
        for (int i = 0; i < count; ++i) {
            const auto& lvl = ob_bids_[i];
            const double vol_pct = (lvl.amount / total_vol) * 100.0;
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float row_w = ImGui::GetContentRegionAvail().x;
            const float bar_w = std::min(static_cast<float>(lvl.amount / (avg_lvl_vol * 2.0)) * row_w, row_w);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y),
                ImVec2(p.x + bar_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.2f, 0.20f)));
            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_GREEN, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.2f%%", vol_pct);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();
}

// ----------------------------------------------------------------------------
// OB Mode: Imbalance — weighted bid/ask ratio gauge + per-level table
// ----------------------------------------------------------------------------
void CryptoTradingScreen::render_ob_mode_imbalance(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (tick_history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "Collecting data...");
        return;
    }

    const auto& latest    = tick_history_.back();
    const double imbalance = latest.imbalance;

    // Imbalance value + colour
    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Imbalance: %+.3f", imbalance);
        const ImVec4 col = (imbalance > OB_IMBALANCE_BUY_THRESHOLD)  ? MARKET_GREEN :
                           (imbalance < OB_IMBALANCE_SELL_THRESHOLD) ? MARKET_RED   :
                           TEXT_SECONDARY;
        ImGui::TextColored(col, "%s", buf);
    }

    // Horizontal bid/ask weight bar
    {
        const float bar_total_w = w - 24.0f;
        const float bid_frac    = static_cast<float>((imbalance + 1.0) / 2.0);
        ImVec2 p = ImGui::GetCursorScreenPos();
        constexpr float bar_h = 8.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + bar_total_w * bid_frac, p.y + bar_h),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.55f, 0.25f, 0.8f)));
        dl->AddRectFilled(ImVec2(p.x + bar_total_w * bid_frac, p.y),
                          ImVec2(p.x + bar_total_w, p.y + bar_h),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(0.65f, 0.1f, 0.1f, 0.8f)));
        ImGui::Dummy(ImVec2(bar_total_w, bar_h + 2.0f));
    }

    ImGui::Spacing();

    // Per-level breakdown
    static constexpr float k_weights[3] = {50.0f, 30.0f, 20.0f};
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("LVL  WT   BID QTY    ASK QTY    IMBAL");
    ImGui::PopStyleColor();
    ImGui::Separator();

    for (int i = 0; i < 3; ++i) {
        const double bq        = latest.bid_qty[i];
        const double aq        = latest.ask_qty[i];
        const double wB        = k_weights[i] * bq;
        const double wA        = k_weights[i] * aq;
        const double lvl_imbal = (wB + wA > 0.0) ? (wB - wA) / (wB + wA) : 0.0;
        const ImVec4 col       = (lvl_imbal > 0.1)  ? MARKET_GREEN :
                                 (lvl_imbal < -0.1) ? MARKET_RED   : TEXT_SECONDARY;
        char row[96];
        std::snprintf(row, sizeof(row), "  L%d  %3.0f%%  %9.4f  %9.4f  %+.3f",
                      i + 1, k_weights[i], bq, aq, lvl_imbal);
        ImGui::TextColored(col, "%s", row);
    }

    ImGui::Spacing();
    ImGui::Separator();

    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "60s rise: %+.3f%%", latest.rise_ratio_60);
        ImGui::TextColored(latest.rise_ratio_60 >= 0.0 ? MARKET_GREEN : MARKET_RED, "%s", buf);
    }
}

// ----------------------------------------------------------------------------
// OB Mode: Signals — BUY/SELL/NEUTRAL badge from imbalance + rise ratio
// ----------------------------------------------------------------------------
void CryptoTradingScreen::render_ob_mode_signals(float w) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (tick_history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "Collecting data... (~5s)");
        return;
    }

    const auto& latest    = tick_history_.back();
    const double imbalance = latest.imbalance;
    const double rise_60   = latest.rise_ratio_60;

    const bool is_buy  = (imbalance > OB_IMBALANCE_BUY_THRESHOLD  && rise_60 > OB_RISE_BUY_THRESHOLD);
    const bool is_sell = (imbalance < OB_IMBALANCE_SELL_THRESHOLD && rise_60 < OB_RISE_SELL_THRESHOLD);

    // Signal badge (styled as non-interactive button for visual weight)
    {
        const char* label  = is_buy ? "  BUY  " : (is_sell ? "  SELL  " : " NEUTRAL ");
        const ImVec4 bg    = is_buy  ? ImVec4(0.0f, 0.55f, 0.25f, 1.0f) :
                             is_sell ? ImVec4(0.65f, 0.1f, 0.1f, 1.0f) :
                                       ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Button(label, ImVec2(w - 24.0f, 28.0f));
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();

    // Metrics table
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("Imbalance    Rise 60s    Thresholds");
    ImGui::PopStyleColor();
    ImGui::Separator();

    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%+.3f", imbalance);
        ImGui::TextColored(imbalance >= 0.0 ? MARKET_GREEN : MARKET_RED, "%s", buf);
        ImGui::SameLine(80);
        std::snprintf(buf, sizeof(buf), "%+.3f%%", rise_60);
        ImGui::TextColored(rise_60 >= 0.0 ? MARKET_GREEN : MARKET_RED, "%s", buf);
        ImGui::SameLine(160);
        ImGui::TextColored(TEXT_DIM, "B>%.2f|S<%.2f",
                           OB_IMBALANCE_BUY_THRESHOLD, OB_IMBALANCE_SELL_THRESHOLD);
    }

    ImGui::Spacing();
    ImGui::Separator();

    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "History: %d/%d ticks",
                      static_cast<int>(tick_history_.size()), OB_MAX_TICK_HISTORY);
        ImGui::TextColored(TEXT_DIM, "%s", buf);
    }
}

} // namespace fincept::crypto
