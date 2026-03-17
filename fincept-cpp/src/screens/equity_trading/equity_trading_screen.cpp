#include "equity_trading_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include "core/notification.h"
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
static constexpr float NAV_HEIGHT          = 38.0f;
static constexpr float TICKER_HEIGHT       = 54.0f;
static constexpr float STATUS_HEIGHT       = 24.0f;
static constexpr float LEFT_PANEL_WIDTH    = 240.0f;
static constexpr float RIGHT_PANEL_WIDTH   = 290.0f;
static constexpr float BOTTOM_PANEL_FRAC   = 0.32f;
static constexpr int   MAX_DEPTH_LEVELS    = 10;
static constexpr int   MAX_CHART_CANDLES   = 200;

// UI helper colors
static constexpr ImVec4 BUY_GREEN        = {0.05f, 0.65f, 0.30f, 1.0f};
static constexpr ImVec4 BUY_GREEN_HOVER  = {0.05f, 0.75f, 0.35f, 1.0f};
static constexpr ImVec4 BUY_GREEN_BG     = {0.05f, 0.65f, 0.30f, 0.15f};
static constexpr ImVec4 SELL_RED         = {0.75f, 0.15f, 0.15f, 1.0f};
static constexpr ImVec4 SELL_RED_HOVER   = {0.85f, 0.20f, 0.20f, 1.0f};
static constexpr ImVec4 SELL_RED_BG      = {0.75f, 0.15f, 0.15f, 0.15f};
static constexpr ImVec4 PILL_CONNECTED   = {0.10f, 0.55f, 0.25f, 1.0f};
static constexpr ImVec4 PILL_PAPER       = {0.70f, 0.55f, 0.05f, 1.0f};
static constexpr ImVec4 PILL_LIVE        = {0.75f, 0.15f, 0.15f, 1.0f};
static constexpr ImVec4 DEPTH_BID_BAR    = {0.05f, 0.65f, 0.30f, 0.20f};
static constexpr ImVec4 DEPTH_ASK_BAR    = {0.75f, 0.15f, 0.15f, 0.20f};
static constexpr ImVec4 REGION_INDIA     = {1.0f, 0.55f, 0.10f, 1.0f};
static constexpr ImVec4 REGION_US        = {0.30f, 0.60f, 0.96f, 1.0f};
static constexpr ImVec4 REGION_EU        = {0.55f, 0.40f, 0.90f, 1.0f};

// ============================================================================
// UI Helpers
// ============================================================================

static void eq_draw_pill(const char* text, ImVec4 bg_color, ImVec4 text_color = TEXT_PRIMARY) {
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();

    const float pad_x = 8.0f;
    const float pad_y = 2.0f;
    const ImVec2 rect_min = {cursor.x, cursor.y + 1.0f};
    const ImVec2 rect_max = {cursor.x + text_size.x + pad_x * 2, cursor.y + text_size.y + pad_y * 2 + 1.0f};

    draw->AddRectFilled(rect_min, rect_max,
        ImGui::ColorConvertFloat4ToU32(bg_color), 3.0f);
    draw->AddText({rect_min.x + pad_x, rect_min.y + pad_y},
        ImGui::ColorConvertFloat4ToU32(text_color), text);

    ImGui::Dummy({rect_max.x - rect_min.x, rect_max.y - rect_min.y});
}

static void eq_draw_separator_line(float width) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddLine({p.x, p.y}, {p.x + width, p.y},
        ImGui::ColorConvertFloat4ToU32(BORDER_DIM));
    ImGui::Dummy({width, 1.0f});
}

static const char* eq_format_volume(double vol, char* buf, size_t buf_size) {
    if (vol >= 1e9)
        std::snprintf(buf, buf_size, "%.1fB", vol / 1e9);
    else if (vol >= 1e6)
        std::snprintf(buf, buf_size, "%.1fM", vol / 1e6);
    else if (vol >= 1e3)
        std::snprintf(buf, buf_size, "%.1fK", vol / 1e3);
    else
        std::snprintf(buf, buf_size, "%.0f", vol);
    return buf;
}

static ImVec4 eq_region_color(const char* region) {
    if (std::strcmp(region, "India") == 0) return REGION_INDIA;
    if (std::strcmp(region, "US") == 0) return REGION_US;
    if (std::strcmp(region, "EU") == 0) return REGION_EU;
    return TEXT_DIM;
}

// ============================================================================
// Initialization
// ============================================================================

void EquityTradingScreen::init() {
    LOG_INFO(TAG, "Initializing equity trading screen");

    watchlist_.clear();
    for (int i = 0; i < DEFAULT_WATCHLIST_COUNT; ++i) {
        WatchlistEntry e;
        e.symbol = DEFAULT_WATCHLIST[i];
        e.exchange = "NSE";
        watchlist_.push_back(e);
    }

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

    // Full-screen frame — matches dashboard/markets pattern (no debug window)
    ui::ScreenFrame frame("##equity_trading", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    const float w = frame.width();
    const float total_h = frame.height();

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

        render_watchlist_panel(0.0f, body_y, left_w, body_h);
        render_chart_panel(left_w, body_y, center_w, chart_h);
        if (!bottom_minimized_) {
            render_bottom_panel(left_w, body_y + chart_h, center_w, bottom_h);
        }

        const float order_h = body_h * 0.55f;
        const float depth_h = body_h - order_h;
        render_order_entry(left_w + center_w, body_y, right_w, order_h);
        render_order_book(left_w + center_w, body_y + order_h, right_w, depth_h);
    }

    render_status_bar(w, total_h);

    frame.end();
}

// ============================================================================
// Top Navigation Bar — broker status pill, exchange selector, symbol input
// ============================================================================

void EquityTradingScreen::render_top_nav(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
    ImGui::BeginChild("##eq_nav", ImVec2(w, NAV_HEIGHT), false);

    // Broker status pill
    if (is_connected_ && active_broker_.has_value()) {
        char pill[64];
        std::snprintf(pill, sizeof(pill), " %s ", trading::broker_id_str(*active_broker_));
        eq_draw_pill(pill, PILL_CONNECTED, ImVec4(1, 1, 1, 1));
    } else {
        eq_draw_pill(" DISCONNECTED ", {0.3f, 0.3f, 0.3f, 0.6f}, TEXT_DIM);
    }

    ImGui::SameLine(0.0f, 12.0f);

    // Exchange selector with region info
    ImGui::SetNextItemWidth(130.0f);
    char combo_label[64];
    std::snprintf(combo_label, sizeof(combo_label), "%s (%s)",
                  EXCHANGES[exchange_idx_].code, EXCHANGES[exchange_idx_].currency);
    if (ImGui::BeginCombo("##exchange", combo_label)) {
        const char* last_region = nullptr;
        for (int i = 0; i < EXCHANGE_COUNT; ++i) {
            // Region separator
            if (!last_region || std::strcmp(last_region, EXCHANGES[i].region) != 0) {
                if (last_region) ImGui::Separator();
                ImGui::TextColored(eq_region_color(EXCHANGES[i].region), " %s", EXCHANGES[i].region);
                last_region = EXCHANGES[i].region;
            }

            const bool selected = (i == exchange_idx_);
            char label[128];
            std::snprintf(label, sizeof(label), "  %s  %s  (%s)",
                          EXCHANGES[i].code, EXCHANGES[i].name, EXCHANGES[i].currency);
            if (ImGui::Selectable(label, selected)) {
                exchange_idx_ = i;
                selected_exchange_ = EXCHANGES[i].code;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, 8.0f);

    // Symbol input with accent border on focus
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.2f, 0.15f, 0.1f, 1.0f));
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::InputText("##sym_input", symbol_buf_, sizeof(symbol_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase)) {
        select_symbol(symbol_buf_, selected_exchange_);
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    if (ImGui::Button("GO", ImVec2(32, 0))) {
        select_symbol(symbol_buf_, selected_exchange_);
    }
    ImGui::PopStyleColor(3);

    // Right side controls
    ImGui::SameLine(w - 260.0f);

    // Paper/Live mode pill
    if (is_paper_mode_) {
        ImGui::PushStyleColor(ImGuiCol_Button, PILL_PAPER);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.65f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::SmallButton(" PAPER ")) { is_paper_mode_ = false; }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, PILL_LIVE);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::SmallButton(" LIVE ")) { is_paper_mode_ = true; }
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0.0f, 8.0f);

    // Connect/Disconnect button
    if (!is_connected_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::SmallButton(" Connect Broker ")) {
            bottom_tab_ = BottomTab::brokers;
            bottom_minimized_ = false;
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.15f, 0.15f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, SELL_RED);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::SmallButton(" Disconnect ")) {
            is_connected_ = false;
            active_broker_ = std::nullopt;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Ticker Bar — current symbol quote strip with spread & range indicators
// ============================================================================

void EquityTradingScreen::render_ticker_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
    ImGui::BeginChild("##eq_ticker", ImVec2(w, TICKER_HEIGHT), false);

    // Symbol name (large)
    ImGui::TextColored(ACCENT, "%s", selected_symbol_.c_str());
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(TEXT_DIM, "%s", selected_exchange_.c_str());

    if (has_quote_) {
        ImGui::SameLine(0.0f, 20.0f);

        // LTP — large text style
        ImGui::TextColored(TEXT_PRIMARY, "%.2f", current_quote_.ltp);
        ImGui::SameLine(0.0f, 8.0f);

        // Change with arrow
        const bool positive = (current_quote_.change >= 0.0);
        const ImVec4& chg_color = positive ? MARKET_GREEN : MARKET_RED;
        const char* arrow = positive ? "\xe2\x96\xb2" : "\xe2\x96\xbc";  // ▲ or ▼
        char chg_buf[64];
        std::snprintf(chg_buf, sizeof(chg_buf), "%s %+.2f (%+.2f%%)",
                      arrow, current_quote_.change, current_quote_.change_pct);
        ImGui::TextColored(chg_color, "%s", chg_buf);

        // OHLV strip
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::TextColored(TEXT_DIM, "O");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", current_quote_.open);

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::TextColored(TEXT_DIM, "H");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(MARKET_GREEN, "%.2f", current_quote_.high);

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::TextColored(TEXT_DIM, "L");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(MARKET_RED, "%.2f", current_quote_.low);

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::TextColored(TEXT_DIM, "V");
        ImGui::SameLine(0.0f, 2.0f);
        char vol_buf[32];
        ImGui::TextColored(TEXT_SECONDARY, "%s", eq_format_volume(current_quote_.volume, vol_buf, sizeof(vol_buf)));

        // Bid/Ask with spread
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::TextColored(MARKET_GREEN, "%.2f", current_quote_.bid);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(TEXT_DIM, "/");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextColored(MARKET_RED, "%.2f", current_quote_.ask);

        if (current_quote_.bid > 0.0 && current_quote_.ask > 0.0) {
            ImGui::SameLine(0.0f, 6.0f);
            const double spread = current_quote_.ask - current_quote_.bid;
            char spread_buf[32];
            std::snprintf(spread_buf, sizeof(spread_buf), "(%.2f)", spread);
            ImGui::TextColored(TEXT_DIM, "%s", spread_buf);
        }

        // Day range bar (visual indicator between L and H)
        if (current_quote_.high > current_quote_.low) {
            ImGui::SameLine(0.0f, 16.0f);
            const ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            const float bar_w = 80.0f;
            const float bar_h = 4.0f;
            auto* draw = ImGui::GetWindowDrawList();

            const float bar_y = bar_pos.y + 6.0f;
            draw->AddRectFilled({bar_pos.x, bar_y}, {bar_pos.x + bar_w, bar_y + bar_h},
                                IM_COL32(60, 60, 65, 200), 2.0f);

            // LTP position within range
            const double range = current_quote_.high - current_quote_.low;
            const float ltp_pct = static_cast<float>((current_quote_.ltp - current_quote_.low) / range);
            const float ltp_x = bar_pos.x + ltp_pct * bar_w;

            // Filled portion
            const ImU32 fill_col = positive ? IM_COL32(13, 180, 100, 200) : IM_COL32(220, 60, 60, 200);
            draw->AddRectFilled({bar_pos.x, bar_y}, {ltp_x, bar_y + bar_h}, fill_col, 2.0f);

            // LTP marker
            draw->AddCircleFilled({ltp_x, bar_y + bar_h * 0.5f}, 3.0f, IM_COL32(255, 255, 255, 220));

            ImGui::Dummy({bar_w + 4.0f, 14.0f});
        }

    } else if (loading_quote_) {
        ImGui::SameLine(0.0f, 24.0f);
        theme::LoadingSpinner("Fetching quote...");
    } else {
        ImGui::SameLine(0.0f, 24.0f);
        ImGui::TextColored(TEXT_DIM, "Connect a broker to view live data");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Watchlist Panel — left sidebar with search, stocks, indices
// ============================================================================

void EquityTradingScreen::render_watchlist_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::BeginChild("##eq_watchlist", ImVec2(w, h), true);

    // Tab-style toggle
    const float tab_w = (w - 20.0f) * 0.5f;
    {
        const bool stocks_active = (sidebar_view_ == SidebarView::watchlist);

        if (stocks_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::Button("STOCKS", ImVec2(tab_w, 24.0f))) {
            sidebar_view_ = SidebarView::watchlist;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0.0f, 2.0f);

        const bool idx_active = (sidebar_view_ == SidebarView::indices);
        if (idx_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::Button("INDICES", ImVec2(tab_w, 24.0f))) {
            sidebar_view_ = SidebarView::indices;
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    if (sidebar_view_ == SidebarView::watchlist) {
        // Search with icon hint
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::SetNextItemWidth(w - 14.0f);
        ImGui::InputTextWithHint("##wl_search", "Search symbols...", watchlist_search_,
                                 sizeof(watchlist_search_));
        ImGui::PopStyleColor();

        const std::string filter(watchlist_search_);

        ImGui::Spacing();

        // Watchlist table with improved visuals
        if (ImGui::BeginTable("##wl_tbl", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_PadOuterX,
                ImVec2(0, h - 90.0f))) {
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.40f);
            ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_None, 0.30f);
            ImGui::TableSetupColumn("Chg%", ImGuiTableColumnFlags_None, 0.30f);
            ImGui::TableHeadersRow();

            int row_idx = 0;
            for (const auto& entry : watchlist_) {
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

                // Highlight selected row
                const bool is_sel = (entry.symbol == selected_symbol_);
                if (is_sel) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                        ImGui::ColorConvertFloat4ToU32(ACCENT_BG));
                }

                ImGui::TableNextColumn();
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
                    const ImVec4& c = (entry.change_pct >= 0.0) ? MARKET_GREEN : MARKET_RED;
                    // Draw small change bar behind text
                    const ImVec2 cell_min = ImGui::GetCursorScreenPos();
                    const float bar_frac = std::min(1.0f, static_cast<float>(std::abs(entry.change_pct)) / 5.0f);
                    auto* draw = ImGui::GetWindowDrawList();
                    const ImVec4& bar_col = (entry.change_pct >= 0.0) ? BUY_GREEN_BG : SELL_RED_BG;
                    draw->AddRectFilled(cell_min,
                        {cell_min.x + bar_frac * 60.0f, cell_min.y + ImGui::GetTextLineHeight()},
                        ImGui::ColorConvertFloat4ToU32(bar_col));

                    ImGui::TextColored(c, "%+.2f%%", entry.change_pct);
                } else {
                    ImGui::TextColored(TEXT_DIM, "0.00%%");
                }

                ++row_idx;
            }
            ImGui::EndTable();
        }

        // Bottom actions
        ImGui::Spacing();
        if (loading_watchlist_) {
            theme::LoadingSpinner("Refreshing...");
        } else {
            const float btn_w = w - 14.0f;
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            if (ImGui::Button("Refresh Watchlist", ImVec2(btn_w, 24.0f))) {
                fetch_watchlist_quotes();
            }
            ImGui::PopStyleColor(2);
        }

    } else {
        // Market Indices view
        ImGui::TextColored(ACCENT, "MARKET INDICES");
        ImGui::Spacing();
        eq_draw_separator_line(w - 14.0f);
        ImGui::Spacing();

        for (int i = 0; i < MARKET_INDICES_COUNT; ++i) {
            const auto& idx = MARKET_INDICES[i];
            ImGui::PushStyleColor(ImGuiCol_Header, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);
            if (ImGui::Selectable(idx.symbol, false, 0, ImVec2(0, 24.0f))) {
                select_symbol(idx.symbol, idx.exchange);
            }
            ImGui::PopStyleColor(2);

            if (i < MARKET_INDICES_COUNT - 1) {
                ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_DIM);
                ImGui::Separator();
                ImGui::PopStyleColor();
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Chart Panel — candlestick chart with timeframe selector
// ============================================================================

void EquityTradingScreen::render_chart_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("##eq_chart", ImVec2(w, h), true);

    // Timeframe buttons — pill style
    static constexpr TimeFrame tfs[] = {
        TimeFrame::m1, TimeFrame::m5, TimeFrame::m15, TimeFrame::m30,
        TimeFrame::h1, TimeFrame::h4, TimeFrame::d1, TimeFrame::w1
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 3));
    for (const auto tf : tfs) {
        const bool active = (tf == chart_timeframe_);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::SmallButton(timeframe_label(tf))) {
            chart_timeframe_ = tf;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0.0f, 2.0f);
    }
    ImGui::PopStyleVar();

    // Hide/Show bottom panel toggle
    ImGui::SameLine(w - 70.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    if (ImGui::SmallButton(bottom_minimized_ ? "Show" : "Hide")) {
        bottom_minimized_ = !bottom_minimized_;
    }
    ImGui::PopStyleColor(2);

    eq_draw_separator_line(w - 16.0f);

    // Chart area
    if (loading_candles_) {
        const float cx = w * 0.4f;
        const float cy = h * 0.4f;
        ImGui::SetCursorPos(ImVec2(cx, cy));
        theme::LoadingSpinner("Loading chart...");
    } else if (candles_.empty()) {
        // Empty state with centered message
        const float cx = w * 0.25f;
        const float cy = h * 0.35f;
        ImGui::SetCursorPos(ImVec2(cx, cy));
        ImGui::TextColored(TEXT_DIM, "No chart data available");
        ImGui::SetCursorPosX(cx - 20.0f);
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DISABLED, "Connect a broker and select a symbol");
        ImGui::TextColored(TEXT_DISABLED, "to view candlestick charts");
    } else {
        render_candle_chart(w - 16.0f, h - 40.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Candle Chart Renderer — improved with volume bars and crosshair
// ============================================================================

void EquityTradingScreen::render_candle_chart(float w, float h) {
    if (candles_.empty() || w < 20.0f || h < 20.0f) return;

    const ImVec2 p = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();

    // Reserve 20% height for volume
    const float vol_h = h * 0.15f;
    const float price_h = h - vol_h - 4.0f;

    // Background
    draw->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(15, 15, 17, 255));

    // Find price min/max
    double min_price = 1e18;
    double max_price = -1e18;
    double max_vol = 0.0;
    for (const auto& c : candles_) {
        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
        max_vol = std::max(max_vol, c.volume);
    }
    if (max_price <= min_price) max_price = min_price + 1.0;
    if (max_vol <= 0.0) max_vol = 1.0;

    const double price_range = max_price - min_price;
    const float padding = price_h * 0.04f;
    const float chart_h = price_h - 2.0f * padding;
    const int n = static_cast<int>(candles_.size());
    const float label_w = 50.0f;
    const float chart_w = w - label_w;
    const float candle_w = std::max(1.0f, chart_w / static_cast<float>(n));
    const float gap = std::max(1.0f, candle_w * 0.15f);
    const float body_w = candle_w - gap;

    // Price grid lines (5 levels) with labels
    for (int i = 0; i <= 4; ++i) {
        const float fy = p.y + padding + chart_h * static_cast<float>(i) / 4.0f;
        draw->AddLine(ImVec2(p.x + label_w, fy), ImVec2(p.x + w, fy),
                      IM_COL32(40, 40, 48, 120));
        const double price_at = max_price - price_range * static_cast<double>(i) / 4.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", price_at);
        draw->AddText(ImVec2(p.x + 2.0f, fy - 6.0f), IM_COL32(120, 120, 130, 200), buf);
    }

    // Volume separator line
    const float vol_top = p.y + price_h + 2.0f;
    draw->AddLine(ImVec2(p.x + label_w, vol_top), ImVec2(p.x + w, vol_top),
                  IM_COL32(50, 50, 58, 150));

    // Draw candles + volume
    for (int i = 0; i < n; ++i) {
        const auto& c = candles_[i];
        const float cx = p.x + label_w + static_cast<float>(i) * candle_w + candle_w * 0.5f;

        auto price_to_y = [&](double price) -> float {
            return p.y + padding + chart_h * static_cast<float>((max_price - price) / price_range);
        };

        const float y_open = price_to_y(c.open);
        const float y_close = price_to_y(c.close);
        const float y_high = price_to_y(c.high);
        const float y_low = price_to_y(c.low);

        const bool bullish = (c.close >= c.open);
        const ImU32 col = bullish ? IM_COL32(13, 217, 107, 255) : IM_COL32(245, 64, 64, 255);
        const ImU32 col_dim = bullish ? IM_COL32(13, 217, 107, 80) : IM_COL32(245, 64, 64, 80);

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

        // Volume bar
        const float vol_bar_h = static_cast<float>(c.volume / max_vol) * (vol_h - 4.0f);
        const float vol_bot = p.y + h;
        draw->AddRectFilled(
            ImVec2(cx - body_w * 0.4f, vol_bot - vol_bar_h),
            ImVec2(cx + body_w * 0.4f, vol_bot), col_dim);
    }

    ImGui::Dummy(ImVec2(w, h));
}

// ============================================================================
// Bottom Panel — tabbed: positions, holdings, orders, history, brokers
// ============================================================================

void EquityTradingScreen::render_bottom_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("##eq_bottom", ImVec2(w, h), true);

    // Tab bar with underline-style indicators and item counts
    static constexpr BottomTab tabs[] = {
        BottomTab::positions, BottomTab::holdings, BottomTab::orders,
        BottomTab::history, BottomTab::brokers
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
    for (const auto t : tabs) {
        const bool active = (t == bottom_tab_);

        // Get count for badge
        int count = 0;
        switch (t) {
            case BottomTab::positions: count = static_cast<int>(positions_.size()); break;
            case BottomTab::holdings:  count = static_cast<int>(holdings_.size()); break;
            case BottomTab::orders:    count = static_cast<int>(orders_.size()); break;
            default: break;
        }

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }

        char tab_label[64];
        if (count > 0)
            std::snprintf(tab_label, sizeof(tab_label), "%s (%d)", bottom_tab_label(t), count);
        else
            std::snprintf(tab_label, sizeof(tab_label), "%s", bottom_tab_label(t));

        if (ImGui::SmallButton(tab_label)) {
            bottom_tab_ = t;
        }

        // Active underline
        if (active) {
            const ImVec2 btn_min = ImGui::GetItemRectMin();
            const ImVec2 btn_max = ImGui::GetItemRectMax();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddLine({btn_min.x, btn_max.y}, {btn_max.x, btn_max.y},
                ImGui::ColorConvertFloat4ToU32(ACCENT), 2.0f);
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine(0.0f, 2.0f);
    }
    ImGui::PopStyleVar();

    // Refresh button (right-aligned)
    ImGui::SameLine(w - 80.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    if (ImGui::SmallButton("Refresh")) {
        refresh_portfolio();
    }
    ImGui::PopStyleColor(2);

    eq_draw_separator_line(w - 16.0f);

    const float content_h = h - 50.0f;

    switch (bottom_tab_) {
        case BottomTab::positions: render_positions_table(w, content_h); break;
        case BottomTab::holdings:  render_holdings_table(w, content_h);  break;
        case BottomTab::orders:    render_orders_table(w, content_h);    break;
        case BottomTab::history:   render_orders_table(w, content_h);    break;
        case BottomTab::brokers:   render_broker_config(w, content_h);   break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Positions Table — with P&L summary row
// ============================================================================

void EquityTradingScreen::render_positions_table(float w, float h) {
    if (loading_portfolio_) {
        theme::LoadingSpinner("Loading positions...");
        return;
    }
    if (positions_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "  No open positions");
        ImGui::TextColored(TEXT_DISABLED, "  Place an order to see positions here");
        return;
    }

    // P&L summary bar
    double total_pnl = 0.0;
    double total_day = 0.0;
    for (const auto& p : positions_) {
        total_pnl += p.pnl;
        total_day += p.day_pnl;
    }

    {
        const ImVec4& pnl_col = (total_pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
        const ImVec4& day_col = (total_day >= 0.0) ? MARKET_GREEN : MARKET_RED;
        ImGui::TextColored(TEXT_DIM, "Total P&L:");
        ImGui::SameLine();
        ImGui::TextColored(pnl_col, "%+.2f", total_pnl);
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::TextColored(TEXT_DIM, "Day:");
        ImGui::SameLine();
        ImGui::TextColored(day_col, "%+.2f", total_day);
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::TextColored(TEXT_DIM, "%d positions", static_cast<int>(positions_.size()));
    }

    ImGui::Spacing();

    if (ImGui::BeginTable("##pos_tbl", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable, ImVec2(0, h - 28.0f))) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_None, 0.08f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_None, 0.10f);
        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("P&L", ImGuiTableColumnFlags_DefaultSort, 0.18f);
        ImGui::TableSetupColumn("Day P&L", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableHeadersRow();

        for (const auto& pos : positions_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_PRIMARY, "%s", pos.symbol.c_str());

            ImGui::TableNextColumn();
            const ImVec4& side_col = (pos.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", pos.side == "buy" ? "BUY" : "SELL");

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", pos.quantity);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", pos.avg_price);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", pos.ltp);

            ImGui::TableNextColumn();
            const ImVec4& pnl_col = (pos.pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(pnl_col, "%+.2f", pos.pnl);

            ImGui::TableNextColumn();
            const ImVec4& day_col = (pos.day_pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(day_col, "%+.2f", pos.day_pnl);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Holdings Table — with investment summary
// ============================================================================

void EquityTradingScreen::render_holdings_table(float w, float h) {
    if (loading_portfolio_) {
        theme::LoadingSpinner("Loading holdings...");
        return;
    }
    if (holdings_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "  No holdings");
        ImGui::TextColored(TEXT_DISABLED, "  Delivery positions will appear here");
        return;
    }

    // Summary bar
    double total_invested = 0.0;
    double total_current = 0.0;
    double total_pnl = 0.0;
    for (const auto& h : holdings_) {
        total_invested += h.invested_value;
        total_current += h.current_value;
        total_pnl += h.pnl;
    }
    const double total_pct = (total_invested > 0.0) ? (total_pnl / total_invested) * 100.0 : 0.0;

    {
        const ImVec4& col = (total_pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
        ImGui::TextColored(TEXT_DIM, "Invested:");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", total_invested);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(TEXT_DIM, "Current:");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", total_current);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(TEXT_DIM, "P&L:");
        ImGui::SameLine();
        ImGui::TextColored(col, "%+.2f (%+.2f%%)", total_pnl, total_pct);
    }

    ImGui::Spacing();

    if (ImGui::BeginTable("##hold_tbl", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, h - 28.0f))) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_None, 0.08f);
        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableSetupColumn("Invested", ImGuiTableColumnFlags_None, 0.16f);
        ImGui::TableSetupColumn("P&L", ImGuiTableColumnFlags_None, 0.16f);
        ImGui::TableSetupColumn("P&L%", ImGuiTableColumnFlags_None, 0.14f);
        ImGui::TableHeadersRow();

        for (const auto& hld : holdings_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_PRIMARY, "%s", hld.symbol.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", hld.quantity);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", hld.avg_price);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", hld.ltp);

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%.2f", hld.invested_value);

            ImGui::TableNextColumn();
            const ImVec4& c = (hld.pnl >= 0.0) ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(c, "%+.2f", hld.pnl);

            ImGui::TableNextColumn();
            ImGui::TextColored(c, "%+.2f%%", hld.pnl_pct);
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Orders Table — with status badges
// ============================================================================

void EquityTradingScreen::render_orders_table(float w, float h) {
    if (loading_portfolio_) {
        theme::LoadingSpinner("Loading orders...");
        return;
    }
    if (orders_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "  No orders");
        ImGui::TextColored(TEXT_DISABLED, "  Place an order to see it here");
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
            ImGui::TextColored(TEXT_PRIMARY, "%s", ord.symbol.c_str());

            ImGui::TableNextColumn();
            const ImVec4& sc = (ord.side == "buy" || ord.side == "BUY") ? MARKET_GREEN : MARKET_RED;
            const char* side_str = (ord.side == "buy" || ord.side == "BUY") ? "BUY" : "SELL";
            ImGui::TextColored(sc, "%s", side_str);

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%s", ord.order_type.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.0f/%.0f", ord.filled_qty, ord.quantity);

            ImGui::TableNextColumn();
            if (ord.avg_price > 0.0)
                ImGui::Text("%.2f", ord.avg_price);
            else
                ImGui::Text("%.2f", ord.price);

            ImGui::TableNextColumn();
            // Status badge
            ImVec4 status_col = TEXT_SECONDARY;
            if (ord.status == "filled" || ord.status == "complete") status_col = MARKET_GREEN;
            else if (ord.status == "pending" || ord.status == "open") status_col = WARNING;
            else if (ord.status == "cancelled" || ord.status == "rejected") status_col = MARKET_RED;
            else if (ord.status == "partially_filled") status_col = INFO;
            ImGui::TextColored(status_col, "%s", ord.status.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", ord.timestamp.c_str());
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Broker Configuration Panel — card-style grid with regions
// ============================================================================

void EquityTradingScreen::render_broker_config(float w, float h) {
    // Header
    ImGui::TextColored(ACCENT, "BROKER CONNECTIONS");
    ImGui::SameLine(w - 160.0f);
    ImGui::TextColored(TEXT_DIM, "16 brokers available");

    eq_draw_separator_line(w - 16.0f);
    ImGui::Spacing();

    struct BrokerInfo { trading::BrokerId id; const char* name; const char* region; };
    static constexpr BrokerInfo known_brokers[] = {
        // Indian brokers (12)
        {trading::BrokerId::Fyers,     "Fyers",      "India"},
        {trading::BrokerId::Zerodha,   "Zerodha",    "India"},
        {trading::BrokerId::AngelOne,  "Angel One",  "India"},
        {trading::BrokerId::Upstox,    "Upstox",     "India"},
        {trading::BrokerId::Dhan,      "Dhan",       "India"},
        {trading::BrokerId::Kotak,     "Kotak",      "India"},
        {trading::BrokerId::Groww,     "Groww",      "India"},
        {trading::BrokerId::AliceBlue, "AliceBlue",  "India"},
        {trading::BrokerId::FivePaisa, "5Paisa",     "India"},
        {trading::BrokerId::IIFL,      "IIFL",       "India"},
        {trading::BrokerId::Motilal,   "Motilal",    "India"},
        {trading::BrokerId::Shoonya,   "Shoonya",    "India"},
        // US brokers (3)
        {trading::BrokerId::Alpaca,    "Alpaca",     "US"},
        {trading::BrokerId::IBKR,      "IBKR",       "US"},
        {trading::BrokerId::Tradier,   "Tradier",    "US"},
        // EU brokers (1)
        {trading::BrokerId::SaxoBank,  "Saxo Bank",  "EU"},
    };

    if (ImGui::BeginTable("##broker_grid", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
            ImVec2(0, h - 30.0f))) {
        ImGui::TableSetupColumn("Broker", ImGuiTableColumnFlags_None, 0.22f);
        ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_None, 0.12f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableSetupColumn("Credentials", ImGuiTableColumnFlags_None, 0.18f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_None, 0.30f);
        ImGui::TableHeadersRow();

        const char* last_region = nullptr;

        for (const auto& b : known_brokers) {
            // Region separator row
            if (!last_region || std::strcmp(last_region, b.region) != 0) {
                if (last_region) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Spacing();
                }
                last_region = b.region;
            }

            ImGui::TableNextRow();

            // Broker name
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_PRIMARY, "%s", b.name);

            // Region pill
            ImGui::TableNextColumn();
            ImGui::TextColored(eq_region_color(b.region), "%s", b.region);

            // Status
            ImGui::TableNextColumn();
            const bool connected = (active_broker_.has_value() && *active_broker_ == b.id && is_connected_);
            if (connected) {
                ImGui::TextColored(MARKET_GREEN, "Connected");
            } else {
                ImGui::TextColored(TEXT_DISABLED, "Offline");
            }

            // Credential status
            ImGui::TableNextColumn();
            {
                auto* broker = trading::BrokerRegistry::instance().get(b.id);
                if (broker) {
                    auto creds = broker->load_credentials();
                    if (!creds.api_key.empty()) {
                        ImGui::TextColored(SUCCESS, "Configured");
                    } else {
                        ImGui::TextColored(TEXT_DISABLED, "Not set");
                    }
                }
            }

            // Action buttons
            ImGui::TableNextColumn();
            char btn_id[64];
            if (!connected) {
                std::snprintf(btn_id, sizeof(btn_id), "Connect##%s", b.name);
                ImGui::PushStyleColor(ImGuiCol_Button, BUY_GREEN);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BUY_GREEN_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                if (ImGui::SmallButton(btn_id)) {
                    connect_broker(b.id);
                }
                ImGui::PopStyleColor(3);
            } else {
                std::snprintf(btn_id, sizeof(btn_id), "Disconnect##%s", b.name);
                ImGui::PushStyleColor(ImGuiCol_Button, SELL_RED);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, SELL_RED_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                if (ImGui::SmallButton(btn_id)) {
                    is_connected_ = false;
                    active_broker_ = std::nullopt;
                }
                ImGui::PopStyleColor(3);
            }
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Order Entry Panel — improved buy/sell toggle, margin bar
// ============================================================================

void EquityTradingScreen::render_order_entry(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::BeginChild("##eq_order", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "ORDER ENTRY");
    eq_draw_separator_line(w - 20.0f);
    ImGui::Spacing();

    const float input_w = w - 28.0f;
    const bool is_buy = (order_form_.side == trading::OrderSide::Buy);

    // Buy / Sell toggle — full width, colored backgrounds
    {
        const float half_w = input_w * 0.49f;
        const float btn_h = 32.0f;

        // BUY button
        if (is_buy) {
            ImGui::PushStyleColor(ImGuiCol_Button, BUY_GREEN);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BUY_GREEN_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::Button("BUY", ImVec2(half_w, btn_h))) {
            order_form_.side = trading::OrderSide::Buy;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, input_w * 0.02f);

        // SELL button
        if (!is_buy) {
            ImGui::PushStyleColor(ImGuiCol_Button, SELL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, SELL_RED_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::Button("SELL", ImVec2(half_w, btn_h))) {
            order_form_.side = trading::OrderSide::Sell;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();

    // Symbol
    ImGui::TextColored(TEXT_DIM, "Symbol");
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputText("##ord_sym", order_form_.symbol_buf, sizeof(order_form_.symbol_buf),
                     ImGuiInputTextFlags_CharsUppercase);

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
    eq_draw_separator_line(input_w);
    ImGui::Spacing();

    // Estimated value & available funds
    const double est_value = order_form_.quantity *
        (order_form_.type == trading::OrderType::Market ? current_quote_.ltp : order_form_.price);

    if (est_value > 0.0) {
        ImGui::TextColored(TEXT_DIM, "Est. Value");
        ImGui::SameLine(input_w * 0.5f);
        ImGui::TextColored(TEXT_PRIMARY, "%.2f", est_value);
    }

    if (funds_.has_value()) {
        ImGui::TextColored(TEXT_DIM, "Available");
        ImGui::SameLine(input_w * 0.5f);
        ImGui::TextColored(MARKET_GREEN, "%.2f", funds_->available_balance);

        // Margin usage bar
        if (funds_->total_balance > 0.0) {
            const float usage = static_cast<float>(funds_->used_margin / funds_->total_balance);
            ImGui::Spacing();

            const ImVec2 bar_pos = ImGui::GetCursorScreenPos();
            const float bar_h = 4.0f;
            auto* draw = ImGui::GetWindowDrawList();

            // Background
            draw->AddRectFilled(bar_pos, {bar_pos.x + input_w, bar_pos.y + bar_h},
                                IM_COL32(40, 40, 48, 200), 2.0f);

            // Used portion
            const ImU32 usage_col = (usage < 0.7f) ? IM_COL32(13, 180, 100, 200) :
                                    (usage < 0.9f) ? IM_COL32(255, 199, 20, 200) :
                                                     IM_COL32(245, 64, 64, 200);
            draw->AddRectFilled(bar_pos, {bar_pos.x + input_w * usage, bar_pos.y + bar_h},
                                usage_col, 2.0f);

            ImGui::Dummy({input_w, bar_h + 2.0f});
            ImGui::TextColored(TEXT_DIM, "Margin: %.0f%%", usage * 100.0f);
        }
    }

    ImGui::Spacing();

    // Place order button — full width, colored
    const bool can_order = is_connected_ || is_paper_mode_;
    if (!can_order) ImGui::BeginDisabled();

    const ImVec4& btn_col = is_buy ? BUY_GREEN : SELL_RED;
    const ImVec4& btn_hover = is_buy ? BUY_GREEN_HOVER : SELL_RED_HOVER;
    ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_hover);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

    const char* btn_text = is_buy ? "PLACE BUY ORDER" : "PLACE SELL ORDER";
    if (ImGui::Button(btn_text, ImVec2(input_w, 36.0f))) {
        place_order();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (!can_order) ImGui::EndDisabled();

    if (!can_order) {
        ImGui::Spacing();
        ImGui::TextColored(WARNING, "Connect broker or enable paper mode");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Market Depth Panel — with visual depth bars
// ============================================================================

void EquityTradingScreen::render_order_book(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("##eq_depth", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "MARKET DEPTH");
    eq_draw_separator_line(w - 16.0f);

    if (ImGui::BeginTable("##depth_tbl", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_SizingStretchProp, ImVec2(0, h - 40.0f))) {
        ImGui::TableSetupColumn("Bid Qty", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableSetupColumn("Bid", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableSetupColumn("Ask", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableSetupColumn("Ask Qty", ImGuiTableColumnFlags_None, 0.25f);
        ImGui::TableHeadersRow();

        if (has_quote_ && current_quote_.ltp > 0.0) {
            const double mid = current_quote_.ltp;
            const double tick = std::max(0.05, mid * 0.001);
            const int max_qty = (MAX_DEPTH_LEVELS) * 100;

            for (int i = 0; i < MAX_DEPTH_LEVELS; ++i) {
                ImGui::TableNextRow();
                const double bid_price = mid - tick * static_cast<double>(i + 1);
                const double ask_price = mid + tick * static_cast<double>(i + 1);
                const int bid_qty = (MAX_DEPTH_LEVELS - i) * 100;
                const int ask_qty = (i + 1) * 80;

                // Bid Qty with depth bar
                ImGui::TableNextColumn();
                {
                    const ImVec2 cell_min = ImGui::GetCursorScreenPos();
                    const float cell_w = ImGui::GetContentRegionAvail().x;
                    const float bar_frac = static_cast<float>(bid_qty) / static_cast<float>(max_qty);
                    auto* draw = ImGui::GetWindowDrawList();
                    // Bar grows from right to left
                    draw->AddRectFilled(
                        {cell_min.x + cell_w * (1.0f - bar_frac), cell_min.y},
                        {cell_min.x + cell_w, cell_min.y + ImGui::GetTextLineHeight()},
                        ImGui::ColorConvertFloat4ToU32(DEPTH_BID_BAR));
                    ImGui::TextColored(MARKET_GREEN, "%d", bid_qty);
                }

                // Bid Price
                ImGui::TableNextColumn();
                ImGui::TextColored(MARKET_GREEN, "%.2f", bid_price);

                // Ask Price
                ImGui::TableNextColumn();
                ImGui::TextColored(MARKET_RED, "%.2f", ask_price);

                // Ask Qty with depth bar
                ImGui::TableNextColumn();
                {
                    const ImVec2 cell_min = ImGui::GetCursorScreenPos();
                    const float cell_w = ImGui::GetContentRegionAvail().x;
                    const float bar_frac = static_cast<float>(ask_qty) / static_cast<float>(max_qty);
                    auto* draw = ImGui::GetWindowDrawList();
                    // Bar grows from left to right
                    draw->AddRectFilled(
                        cell_min,
                        {cell_min.x + cell_w * bar_frac, cell_min.y + ImGui::GetTextLineHeight()},
                        ImGui::ColorConvertFloat4ToU32(DEPTH_ASK_BAR));
                    ImGui::TextColored(MARKET_RED, "%d", ask_qty);
                }
            }
        } else {
            for (int i = 0; i < 5; ++i) {
                ImGui::TableNextRow();
                for (int j = 0; j < 4; ++j) {
                    ImGui::TableNextColumn();
                    ImGui::TextColored(TEXT_DISABLED, "--");
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status Bar — richer with clock, position count, balance
// ============================================================================

void EquityTradingScreen::render_status_bar(float w, float total_h) {
    ImGui::SetCursorPos(ImVec2(0.0f, total_h - STATUS_HEIGHT));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 3));
    ImGui::BeginChild("##eq_status", ImVec2(w, STATUS_HEIGHT), false);

    // Connection status pill
    if (is_connected_ && active_broker_.has_value()) {
        ImGui::TextColored(MARKET_GREEN, "\xe2\x97\x8f");  // ● filled circle
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(TEXT_SECONDARY, "%s", trading::broker_id_str(*active_broker_));
    } else {
        ImGui::TextColored(TEXT_DISABLED, "\xe2\x97\x8b");  // ○ empty circle
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(TEXT_DISABLED, "No broker");
    }

    ImGui::SameLine(0.0f, 16.0f);

    // Mode
    if (is_paper_mode_) {
        ImGui::TextColored(PILL_PAPER, "PAPER");
    } else {
        ImGui::TextColored(PILL_LIVE, "LIVE");
    }

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextColored(TEXT_DIM, "%s:%s",
                       selected_exchange_.c_str(), selected_symbol_.c_str());

    // Position count
    if (!positions_.empty()) {
        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextColored(TEXT_DIM, "%d pos", static_cast<int>(positions_.size()));
    }

    // Funds on the right
    if (funds_.has_value()) {
        ImGui::SameLine(w - 250.0f);
        ImGui::TextColored(TEXT_DIM, "Bal:");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(MARKET_GREEN, "%.2f", funds_->available_balance);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored(TEXT_DIM, "Margin:");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(TEXT_SECONDARY, "%.2f", funds_->used_margin);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
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
        LOG_INFO(TAG, "Paper order placed");
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
            core::notify::send("Equity Order Placed",
                std::string(trading::order_side_str(order.side)) + " " + order.symbol,
                core::NotifyLevel::Success);
        } else {
            LOG_WARN(TAG, "Order failed: %s", resp.error.c_str());
            core::notify::send("Equity Order Failed", resp.error, core::NotifyLevel::Error);
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

    active_creds_ = broker->load_credentials();
    if (active_creds_.api_key.empty()) {
        LOG_WARN(TAG, "No credentials found for %s — configure in Settings",
                 trading::broker_id_str(broker_id));
        return;
    }

    active_broker_ = broker_id;
    is_connected_ = true;
    LOG_INFO(TAG, "Connected to %s", trading::broker_id_str(broker_id));

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
