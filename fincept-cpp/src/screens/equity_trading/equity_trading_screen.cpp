#include "equity_trading_screen.h"
#include "ui/theme.h"
#include "core/logger.h"
#include "trading/broker_registry.h"
#include "trading/paper_trading.h"
#include "trading/equity_stream.h"
#include "python/python_runner.h"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <nlohmann/json.hpp>

namespace fincept::equity_trading {

using namespace theme::colors;
using json = nlohmann::json;

static constexpr ImVec4 C_GRN = {0.10f,0.75f,0.35f,1};
static constexpr ImVec4 C_RED = {0.85f,0.25f,0.25f,1};
static constexpr ImVec4 C_YEL = {0.90f,0.70f,0.10f,1};

// ═══════════════════════════════════════════════════════════════════════
// render
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::render() {
    if (!inited_) {
        watchlist_.clear();
        for (int i = 0; i < DEFAULT_WATCHLIST_COUNT; ++i)
            watchlist_.push_back({DEFAULT_WATCHLIST[i], "NSE"});
        std::strncpy(order_form_.symbol_buf, sel_sym_.c_str(), sizeof(order_form_.symbol_buf)-1);
        if (paper_id_.empty()) {
            auto ex = trading::pt_find_portfolio("Equity Paper", "NSE");
            if (ex) paper_id_ = ex->id;
            else    paper_id_ = trading::pt_create_portfolio("Equity Paper",1000000.0,"INR",1.0,"cross",0.0003,"NSE").id;
        }
        // Restore saved credentials into UI fields
        // Also set active_broker_/active_creds_ for the first broker that has a valid token,
        // so REST quotes work immediately (even in paper mode, even after market hours).
        for (int i = 0; i < BROKER_META_COUNT; ++i) {
            auto* b = trading::BrokerRegistry::instance().get(BROKER_META[i].id);
            if (!b) continue;
            auto creds = b->load_credentials();
            if (creds.api_key.empty()) continue;
            auto& ci = cred_ui_[i];
            std::strncpy(ci.api_key,    creds.api_key.c_str(),    sizeof(ci.api_key)-1);
            std::strncpy(ci.api_secret, creds.api_secret.c_str(), sizeof(ci.api_secret)-1);
            std::strncpy(ci.user_id,    creds.user_id.c_str(),    sizeof(ci.user_id)-1);
            // Restore additional fields from additional_data JSON
            if (!creds.additional_data.empty()) {
                try {
                    auto j = json::parse(creds.additional_data);
                    std::string pw = j.value("password", "");
                    std::string totp = j.value("totp_secret", "");
                    if (!pw.empty())   std::strncpy(ci.password,    pw.c_str(),   sizeof(ci.password)-1);
                    if (!totp.empty()) std::strncpy(ci.totp_secret, totp.c_str(), sizeof(ci.totp_secret)-1);
                } catch (...) {}
            }
            ci.token_valid = !creds.access_token.empty();
            // Use first non-OAuth broker with a saved token for REST price lookups.
            // Skip OAuth brokers (Fyers, Zerodha, Upstox, Dhan…) — their tokens expire
            // and require re-auth; only TOTP/ApiKey brokers have durable tokens.
            // connected_ stays false — user must explicitly click Connect to go live.
            if (!active_broker_ && !creds.access_token.empty()
                && BROKER_META[i].auth_type != BrokerAuthType::OAuth) {
                active_creds_  = creds;
                active_broker_ = BROKER_META[i].id;
            }
        }
        inited_ = true;
        refresh_paper_portfolio();
        if (active_broker_) {
            fetch_quote();
            refresh_watchlist_quotes();
        }
    }

    using namespace std::chrono;
    auto now = steady_clock::now();

    // Poll futures
    if (quote_fut_.valid() && quote_fut_.wait_for(milliseconds(0)) == std::future_status::ready) {
        try { quote_ = quote_fut_.get(); has_quote_ = true; } catch(...) {}
        loading_quote_ = false;
    }
    if (candle_fut_.valid() && candle_fut_.wait_for(milliseconds(0)) == std::future_status::ready) {
        try { candles_ = candle_fut_.get(); } catch(...) {}
        loading_candles_ = false;
    }

    // Poll broker connect future — resolve on main thread
    if (connecting_ && connect_fut_.valid() &&
        connect_fut_.wait_for(milliseconds(0)) == std::future_status::ready) {
        ConnectResult r;
        try { r = connect_fut_.get(); } catch(...) { r.error = "Exception during auth"; }
        connecting_ = false;
        finish_connect(r);
    }

    // Auto-refresh portfolio
    if (!loading_port_ && duration_cast<seconds>(now - last_port_refresh_).count() >= PORT_REFRESH_SECS) {
        last_port_refresh_ = now;
        if (paper_mode_) refresh_paper_portfolio();
        else if (connected_) refresh_portfolio();
    }

    // Auto-refresh quote every 5s when we have an active broker (live or paper mode)
    if (!loading_quote_ && active_broker_ &&
        duration_cast<seconds>(now - last_quote_refresh_).count() >= QUOTE_REFRESH_SECS) {
        last_quote_refresh_ = now;
        fetch_quote();
    }

    // Auto-refresh watchlist quotes every 15s via REST
    if (!loading_wl_quotes_ && active_broker_ &&
        duration_cast<seconds>(now - last_wl_refresh_).count() >= WL_REFRESH_SECS) {
        last_wl_refresh_ = now;
        refresh_watchlist_quotes();
    }

    // Status timeout
    if (!status_msg_.empty() && duration_cast<seconds>(now - status_time_).count() >= 5)
        status_msg_.clear();

    // Window — same pattern as standalone test that works
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float top_off = ImGui::GetFrameHeight() + 4;
    float bot_off = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + top_off});
    ImGui::SetNextWindowSize({vp->WorkSize.x, vp->WorkSize.y - top_off - bot_off});

    ImGui::Begin("##equity_main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Status
    if (!status_msg_.empty()) { ImGui::TextColored(C_GRN, "%s", status_msg_.c_str()); ImGui::Spacing(); }

    // Mode + symbol bar
    if (paper_mode_)         ImGui::TextColored(C_YEL, "[PAPER]");
    else if (connected_ && ws_streaming_ &&
             trading::EquityStreamManager::instance().is_connected())
                             ImGui::TextColored(C_GRN, "[LIVE\xE2\x97\x8F WS]");
    else if (connected_)     ImGui::TextColored(C_GRN, "[LIVE REST]");
    else                     ImGui::TextColored(C_RED, "[DISCONNECTED]");
    ImGui::SameLine(0,12);
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("##exch", &exch_idx_, [](void* d, int i) -> const char* {
        return EXCHANGES[i].code; }, nullptr, EXCHANGE_COUNT)) {
        sel_exch_ = EXCHANGES[exch_idx_].code;
    }
    ImGui::SameLine(0,4);
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText("##sym", sym_buf_, sizeof(sym_buf_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        select_symbol(sym_buf_, EXCHANGES[exch_idx_].code);
    }
    ImGui::SameLine();
    if (ImGui::Button("Go")) select_symbol(sym_buf_, EXCHANGES[exch_idx_].code);
    ImGui::SameLine(0,16);
    if (has_quote_) {
        ImGui::Text("%.2f", quote_.ltp);
        ImGui::SameLine(0,6);
        ImGui::TextColored(quote_.change >= 0 ? C_GRN : C_RED, "%+.2f (%.2f%%)", quote_.change, quote_.change_pct);
    }
    ImGui::SameLine(0,20);
    if (connected_) ImGui::BeginDisabled();
    ImGui::Checkbox("Paper", &paper_mode_);
    if (connected_) ImGui::EndDisabled();
    ImGui::Separator();

    // Navigation buttons
    static const char* nav[] = {"Overview","Watchlist","Order","Positions","Orders","Funds","Brokers"};
    for (int i = 0; i < 7; ++i) {
        if (i > 0) ImGui::SameLine();
        bool active = (section_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        if (ImGui::Button(nav[i])) section_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::Separator();
    ImGui::Spacing();

    // Content
    switch (section_) {
        case 0: section_overview();   break;
        case 1: section_watchlist();  break;
        case 2: section_order();      break;
        case 3: section_positions();  break;
        case 4: section_orders();     break;
        case 5: section_funds();      break;
        case 6: section_brokers();    break;
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════
// Overview
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_overview() {
    ImGui::Text("Equity Trading");
    ImGui::Separator();
    if (connected_ && active_broker_)
        ImGui::TextColored(C_GRN, "Connected: %s", trading::broker_id_str(*active_broker_));
    else if (paper_mode_ && active_broker_)
        ImGui::TextColored(C_YEL, "Paper mode | %s (REST quotes active)", trading::broker_id_str(*active_broker_));
    else if (paper_mode_)
        ImGui::Text("Paper trading active. Connect a broker in Brokers tab to go live.");
    else
        ImGui::TextColored(C_RED, "No broker connected.");

    ImGui::Spacing();
    ImGui::Text("Symbol: %s [%s]", sel_sym_.c_str(), sel_exch_.c_str());

    if (has_quote_) {
        ImGui::Text("LTP: %.2f  |  Open: %.2f  |  High: %.2f  |  Low: %.2f",
                     quote_.ltp, quote_.open, quote_.high, quote_.low);
        ImGui::Text("Volume: %.0f  |  Bid: %.2f  |  Ask: %.2f",
                     quote_.volume, quote_.bid, quote_.ask);
    } else if (loading_quote_) {
        ImGui::TextColored(C_YEL, "Loading quote...");
    } else if (!active_broker_) {
        ImGui::TextColored(TEXT_DIM, "No broker configured — connect one in Brokers tab.");
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Watchlist
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_watchlist() {
    ImGui::Text("Watchlist (%d)", (int)watchlist_.size());
    ImGui::Separator();
    if (ImGui::BeginTable("##wl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("LTP",    ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Chg",    ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Chg%",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (auto& e : watchlist_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            char sl[64]; std::snprintf(sl, sizeof(sl), "%s##wl_%s", e.symbol.c_str(), e.symbol.c_str());
            if (ImGui::Selectable(sl, false, ImGuiSelectableFlags_SpanAllColumns))
                select_symbol(e.symbol, e.exchange);
            ImGui::TableSetColumnIndex(1);
            e.ltp > 0 ? ImGui::Text("%.2f", e.ltp) : ImGui::TextColored(TEXT_DIM, "-");
            ImGui::TableSetColumnIndex(2);
            e.ltp > 0 ? ImGui::TextColored(e.change>=0?C_GRN:C_RED,"%+.2f",e.change) : ImGui::TextColored(TEXT_DIM,"-");
            ImGui::TableSetColumnIndex(3);
            e.ltp > 0 ? ImGui::TextColored(e.change_pct>=0?C_GRN:C_RED,"%.2f%%",e.change_pct) : ImGui::TextColored(TEXT_DIM,"-");
        }
        ImGui::EndTable();
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Order Entry
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_order() {
    ImGui::Text("Place Order");
    ImGui::Separator();
    auto& f = order_form_;

    ImGui::Text("Symbol"); ImGui::SameLine(120); ImGui::SetNextItemWidth(160);
    ImGui::InputText("##osym", f.symbol_buf, sizeof(f.symbol_buf));

    ImGui::Text("Side"); ImGui::SameLine(120);
    if (ImGui::RadioButton("BUY##buy", f.side == trading::OrderSide::Buy)) f.side = trading::OrderSide::Buy;
    ImGui::SameLine();
    if (ImGui::RadioButton("SELL##sell", f.side == trading::OrderSide::Sell)) f.side = trading::OrderSide::Sell;

    ImGui::Text("Type"); ImGui::SameLine(120);
    static const char* OT[] = {"Market","Limit","StopLoss","SL-Limit"};
    int ot = (int)f.type; ImGui::SetNextItemWidth(160);
    if (ImGui::Combo("##ot", &ot, OT, 4)) f.type = (trading::OrderType)ot;

    ImGui::Text("Product"); ImGui::SameLine(120);
    static const char* PT[] = {"Intraday","Delivery","Margin","Cover","Bracket"};
    int pt = (int)f.product; ImGui::SetNextItemWidth(160);
    if (ImGui::Combo("##pt", &pt, PT, 5)) f.product = (trading::ProductType)pt;

    ImGui::Text("Quantity"); ImGui::SameLine(120); ImGui::SetNextItemWidth(160);
    ImGui::InputDouble("##qty", &f.quantity, 1, 10, "%.0f");

    if (f.type != trading::OrderType::Market) {
        ImGui::Text("Price"); ImGui::SameLine(120); ImGui::SetNextItemWidth(160);
        ImGui::InputDouble("##price", &f.price, 0.05, 1.0, "%.2f");
    }
    if (f.type == trading::OrderType::StopLoss || f.type == trading::OrderType::StopLossLimit) {
        ImGui::Text("Stop Price"); ImGui::SameLine(120); ImGui::SetNextItemWidth(160);
        ImGui::InputDouble("##sp", &f.stop_price, 0.05, 1.0, "%.2f");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (has_quote_) ImGui::TextColored(TEXT_DIM, "LTP: %.2f | %s", quote_.ltp, paper_mode_ ? "PAPER" : "LIVE");

    bool is_buy = (f.side == trading::OrderSide::Buy);
    ImGui::PushStyleColor(ImGuiCol_Button, is_buy ? ImVec4(0.05f,0.55f,0.25f,1) : ImVec4(0.65f,0.15f,0.15f,1));
    char ob[48]; std::snprintf(ob, sizeof(ob), "  %s %.0f x %s  ", is_buy?"BUY":"SELL", f.quantity, f.symbol_buf);
    if (ImGui::Button(ob)) place_order();
    ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════
// Positions
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_positions() {
    ImGui::Text("Open Positions");
    ImGui::SameLine(0,20);
    if (ImGui::Button("Refresh##pos")) { paper_mode_ ? refresh_paper_portfolio() : refresh_portfolio(); }
    ImGui::Separator();
    if (positions_.empty()) { ImGui::TextColored(TEXT_DIM, "No open positions."); return; }
    if (ImGui::BeginTable("##pos", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Symbol"); ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Qty"); ImGui::TableSetupColumn("Avg");
        ImGui::TableSetupColumn("LTP"); ImGui::TableSetupColumn("P&L");
        ImGui::TableHeadersRow();
        for (auto& p : positions_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", p.symbol.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(p.side=="buy"?C_GRN:C_RED, "%s", p.side.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.0f", p.quantity);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", p.avg_price);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", p.ltp);
            ImGui::TableSetColumnIndex(5); ImGui::TextColored(p.pnl>=0?C_GRN:C_RED, "%+.2f", p.pnl);
        }
        ImGui::EndTable();
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Orders
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_orders() {
    ImGui::Text("Order Book");
    ImGui::SameLine(0,20);
    if (ImGui::Button("Refresh##ord")) { paper_mode_ ? refresh_paper_portfolio() : refresh_portfolio(); }
    ImGui::Separator();
    if (orders_.empty()) { ImGui::TextColored(TEXT_DIM, "No orders."); return; }
    if (ImGui::BeginTable("##ord", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Symbol",  ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Side",    ImGuiTableColumnFlags_WidthFixed,  50);
        ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed,  70);
        ImGui::TableSetupColumn("Qty",     ImGuiTableColumnFlags_WidthFixed,  60);
        ImGui::TableSetupColumn("Price",   ImGuiTableColumnFlags_WidthFixed,  70);
        ImGui::TableSetupColumn("Status",  ImGuiTableColumnFlags_WidthFixed,  80);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (auto& o : orders_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", o.symbol.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(o.side=="buy"?C_GRN:C_RED, "%s", o.side.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s", o.order_type.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.0f", o.quantity);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", o.price);
            ImGui::TableSetColumnIndex(5);
            ImVec4 sc = TEXT_DIM;
            if (o.status=="filled") sc=C_GRN; else if (o.status=="cancelled") sc=C_RED; else if (o.status=="pending") sc=C_YEL;
            ImGui::TextColored(sc, "%s", o.status.c_str());
            ImGui::TableSetColumnIndex(6);
            bool is_active = (o.status=="pending" || o.status=="open" || o.status=="trigger pending");
            if (is_active) {
                char ml[48]; std::snprintf(ml, sizeof(ml), "Modify##mo_%s", o.order_id.c_str());
                if (ImGui::SmallButton(ml)) {
                    modify_state_.order_id = o.order_id;
                    modify_state_.price    = o.price;
                    modify_state_.qty      = o.quantity;
                    modify_state_.trigger_price = 0.0;
                    modify_state_.open     = true;
                    ImGui::OpenPopup("Modify Order");
                }
                ImGui::SameLine(0, 4);
                char cl[48]; std::snprintf(cl, sizeof(cl), "Cancel##co_%s", o.order_id.c_str());
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f,0.12f,0.12f,1));
                if (ImGui::SmallButton(cl)) cancel_order_async(o.order_id);
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }

    // Modify Order popup
    if (ImGui::BeginPopupModal("Modify Order", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Order: %s", modify_state_.order_id.c_str());
        ImGui::Separator();
        ImGui::Text("Price");      ImGui::SameLine(120); ImGui::SetNextItemWidth(140);
        ImGui::InputDouble("##mp", &modify_state_.price, 0.05, 1.0, "%.2f");
        ImGui::Text("Quantity");   ImGui::SameLine(120); ImGui::SetNextItemWidth(140);
        ImGui::InputDouble("##mq", &modify_state_.qty, 1.0, 10.0, "%.0f");
        ImGui::Text("Stop Price"); ImGui::SameLine(120); ImGui::SetNextItemWidth(140);
        ImGui::InputDouble("##mt", &modify_state_.trigger_price, 0.05, 1.0, "%.2f");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f,0.50f,0.80f,1));
        if (ImGui::Button("Modify##confirm", {120,0})) {
            modify_order_async(modify_state_.order_id,
                               modify_state_.price,
                               modify_state_.qty,
                               modify_state_.trigger_price);
            modify_state_.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0,8);
        if (ImGui::Button("Cancel##modcancel", {80,0})) {
            modify_state_.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Funds
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_funds() {
    ImGui::Text("Funds / Portfolio");
    ImGui::Separator();
    if (paper_mode_ && !paper_id_.empty()) {
        try {
            auto p = trading::pt_get_portfolio(paper_id_);
            ImGui::Text("Portfolio: %s", p.name.c_str());
            ImGui::Text("Balance:   %.2f %s", p.balance, p.currency.c_str());
            ImGui::Text("Initial:   %.2f", p.initial_balance);
            double pnl = p.balance - p.initial_balance;
            ImGui::TextColored(pnl>=0?C_GRN:C_RED, "Net P&L:   %+.2f", pnl);
            ImGui::Spacing();
            if (ImGui::Button("Reset Paper Portfolio")) {
                trading::pt_reset_portfolio(paper_id_);
                refresh_paper_portfolio();
                set_status("Paper portfolio reset.");
            }
        } catch (...) { ImGui::TextColored(C_RED, "Failed to load portfolio."); }
    } else if (funds_) {
        ImGui::Text("Available: %.2f", funds_->available_balance);
        ImGui::Text("Used Margin: %.2f", funds_->used_margin);
        ImGui::Text("Total: %.2f", funds_->total_balance);
    } else {
        ImGui::TextColored(TEXT_DIM, "Connect a broker to see live funds.");
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Brokers
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::section_brokers() {
    ImGui::Text("Broker Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    // Broker radio buttons
    const char* last_rgn = nullptr;
    for (int i = 0; i < BROKER_META_COUNT; ++i) {
        const auto& m = BROKER_META[i];
        if (!last_rgn || std::strcmp(last_rgn, m.region) != 0) {
            if (last_rgn) { ImGui::Spacing(); ImGui::Spacing(); }
            ImGui::TextColored(TEXT_DIM, "%s:", m.region);
            ImGui::SameLine(60);
            last_rgn = m.region;
        } else { ImGui::SameLine(); }
        bool conn = (active_broker_ && *active_broker_ == m.id && connected_);
        char lbl[64]; std::snprintf(lbl, sizeof(lbl), "%s%s", conn?"* ":"", m.name);
        if (ImGui::RadioButton(lbl, broker_sel_ == i)) broker_sel_ = i;
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (broker_sel_ < 0) { ImGui::TextColored(TEXT_DIM, "Select a broker above."); return; }

    int idx = broker_sel_;
    const auto& m = BROKER_META[idx];
    auto& ci = cred_ui_[idx];
    bool conn = (active_broker_ && *active_broker_ == m.id && connected_);

    ImGui::Text("%s (%s)", m.name, m.region);
    ImGui::TextColored(TEXT_DIM, "Auth: %s",
        m.auth_type==BrokerAuthType::OAuth?"OAuth":m.auth_type==BrokerAuthType::TOTP?"TOTP":"API Key");
    ImGui::Spacing();

    // Credential fields
    ImGui::Text("%s", m.api_key_label[0] ? m.api_key_label : "API Key");
    ImGui::SameLine(150); ImGui::SetNextItemWidth(250);
    char kl[32]; std::snprintf(kl,sizeof(kl),"##ak_%d",idx);
    ImGui::InputText(kl, ci.api_key, sizeof(ci.api_key));

    if (m.api_secret_label[0]) {
        ImGui::Text("%s", m.api_secret_label);
        ImGui::SameLine(150); ImGui::SetNextItemWidth(250);
        char sl[32]; std::snprintf(sl,sizeof(sl),"##as_%d",idx);
        ImGui::InputText(sl, ci.api_secret, sizeof(ci.api_secret), ci.show_secret?0:ImGuiInputTextFlags_Password);
        ImGui::SameLine();
        if (ImGui::SmallButton(ci.show_secret?"Hide":"Show")) ci.show_secret = !ci.show_secret;
    }
    if (m.needs_user_id && m.user_id_label[0]) {
        ImGui::Text("%s", m.user_id_label);
        ImGui::SameLine(150); ImGui::SetNextItemWidth(250);
        char ul[32]; std::snprintf(ul,sizeof(ul),"##uid_%d",idx);
        ImGui::InputText(ul, ci.user_id, sizeof(ci.user_id));
    }
    if (m.needs_password && m.password_label[0]) {
        ImGui::Text("%s", m.password_label);
        ImGui::SameLine(150); ImGui::SetNextItemWidth(250);
        char pl[32]; std::snprintf(pl,sizeof(pl),"##pw_%d",idx);
        ImGui::InputText(pl, ci.password, sizeof(ci.password), ci.show_pw?0:ImGuiInputTextFlags_Password);
    }
    if (m.needs_totp) {
        ImGui::Text("TOTP Secret");
        ImGui::SameLine(150); ImGui::SetNextItemWidth(250);
        char tl[32]; std::snprintf(tl,sizeof(tl),"##totp_%d",idx);
        ImGui::InputText(tl, ci.totp_secret, sizeof(ci.totp_secret));
        ImGui::Spacing();
        ImGui::Text("TOTP Code");
        ImGui::SameLine(150); ImGui::SetNextItemWidth(120);
        char tcl[32]; std::snprintf(tcl,sizeof(tcl),"##tcode_%d",idx);
        ImGui::InputText(tcl, ci.auth_code, sizeof(ci.auth_code));
        ImGui::SameLine(); ImGui::TextColored(TEXT_DIM, "(6-digit, refresh each 30s)");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // OAuth
    if (m.auth_type == BrokerAuthType::OAuth) {
        if (!ci.oauth_url.empty()) {
            ImGui::TextWrapped("Step 1: Open URL:");
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            ImGui::TextWrapped("%s", ci.oauth_url.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); ImGui::SetTooltip("Click to open in browser"); }
            if (ImGui::IsItemClicked()) {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", ci.oauth_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
                std::string cmd = "xdg-open \"" + ci.oauth_url + "\"";
                std::system(cmd.c_str());
#endif
            }
            ImGui::Text("Step 2: Paste code:");
            ImGui::SameLine(150); ImGui::SetNextItemWidth(250);
            char al[32]; std::snprintf(al,sizeof(al),"##ac_%d",idx);
            ImGui::InputText(al, ci.auth_code, sizeof(ci.auth_code));
            char el[48]; std::snprintf(el,sizeof(el),"Exchange Token##ex_%d",idx);
            if (ImGui::Button(el)) exchange_token_async(idx);
        } else {
            char gl[48]; std::snprintf(gl,sizeof(gl),"Generate OAuth URL##gen_%d",idx);
            if (ImGui::Button(gl)) {
                ci.oauth_url = generate_oauth_url(m.id, ci.api_key);
                if (ci.oauth_url.empty()) set_status("Enter API key first.");
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) { ci.oauth_url.clear(); ci.auth_code[0]=0; }
    }

    ImGui::Spacing();

    // Save
    char svl[48]; std::snprintf(svl,sizeof(svl),"Save##save_%d",idx);
    if (ImGui::Button(svl)) {
        json extra;
        if (ci.totp_secret[0]) extra["totp_secret"] = ci.totp_secret;
        if (ci.password[0])    extra["password"]     = ci.password;
        if (ci.user_id[0])     extra["user_id"]      = ci.user_id;
        trading::BrokerCredentials creds;
        creds.broker_id = trading::broker_id_str(m.id);
        creds.api_key = ci.api_key; creds.api_secret = ci.api_secret;
        creds.user_id = ci.user_id; creds.additional_data = extra.dump();
        auto* broker = trading::BrokerRegistry::instance().get(m.id);
        if (broker) { broker->save_credentials(creds); set_status(std::string("Saved: ")+m.name); }
    }
    ImGui::SameLine(0,20);

    if (!conn) {
        char cnl[48]; std::snprintf(cnl,sizeof(cnl),"Connect##con_%d",idx);
        if (ImGui::Button(cnl)) connect_broker(m.id);
    } else {
        ImGui::TextColored(C_GRN, "CONNECTED");
        ImGui::SameLine(0,12);
        if (ImGui::Button("Disconnect")) { stop_ws_stream(); connected_=false; active_broker_=std::nullopt; set_status("Disconnected."); }
    }

    if (!ci.err.empty()) ImGui::TextColored(C_RED, "%s", ci.err.c_str());
    if (!ci.ok.empty())  ImGui::TextColored(C_GRN, "%s", ci.ok.c_str());
}

// ═══════════════════════════════════════════════════════════════════════
// Actions
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::select_symbol(const std::string& sym, const std::string& exch) {
    sel_sym_ = sym; sel_exch_ = exch;
    std::strncpy(sym_buf_, sym.c_str(), sizeof(sym_buf_)-1);
    std::strncpy(order_form_.symbol_buf, sym.c_str(), sizeof(order_form_.symbol_buf)-1);
    fetch_quote(); fetch_candles();
}

void EquityTradingScreen::fetch_quote() {
    // If WS is streaming, pull from tick cache instead of making a REST call
    if (ws_streaming_) {
        trading::EquityTick tick;
        if (trading::EquityStreamManager::instance().get_tick(sel_sym_, tick) && tick.ltp > 0) {
            quote_.ltp        = tick.ltp;
            quote_.open       = tick.open;
            quote_.high       = tick.high;
            quote_.low        = tick.low;
            quote_.bid        = tick.bid;
            quote_.ask        = tick.ask;
            quote_.volume     = tick.volume;
            quote_.change     = tick.ltp - tick.close;
            quote_.change_pct = tick.close > 0 ? (quote_.change / tick.close * 100.0) : 0.0;
            has_quote_        = true;
            return;  // got live tick, done
        }
        // WS connected but no tick yet — fall through to REST for initial quote
    }

    if (loading_quote_) return;
    // Fetch quote if broker connected (even in paper mode — price display is always useful)
    auto* b = active_broker_ ? trading::BrokerRegistry::instance().get(*active_broker_) : nullptr;
    if (!b) { has_quote_=false; return; }
    if (active_creds_.api_key.empty()) { has_quote_=false; return; }
    loading_quote_ = true;
    last_quote_refresh_ = std::chrono::steady_clock::now();
    std::string query = sel_exch_ + ":" + sel_sym_;
    quote_fut_ = std::async(std::launch::async, [b, query, sym=sel_sym_, c=active_creds_]() {
        auto r = b->get_quotes(c, {query});
        if (r.data && !r.data->empty()) return r.data->front();
        auto r2 = b->get_quotes(c, {sym});
        return (r2.data && !r2.data->empty()) ? r2.data->front() : trading::BrokerQuote{};
    });
}

void EquityTradingScreen::fetch_candles() {
    if (loading_candles_) return;
    auto* b = connected_ && active_broker_ ? trading::BrokerRegistry::instance().get(*active_broker_) : nullptr;
    if (!b) return;
    loading_candles_ = true;
    candle_fut_ = std::async(std::launch::async,
        [b, s=sel_sym_, res=std::string(timeframe_resolution(timeframe_)), c=active_creds_]() {
            auto r = b->get_history(c, s, res, "2024-01-01", "2099-12-31");
            return r.data.value_or(std::vector<trading::BrokerCandle>{});
        });
}

void EquityTradingScreen::place_order() {
    auto& f = order_form_;
    if (!f.symbol_buf[0] || f.quantity <= 0) { set_status("Invalid order."); return; }
    if (paper_mode_) {
        double fp = has_quote_ ? quote_.ltp : f.price;
        if (fp <= 0) fp = f.price;
        if (fp <= 0) { set_status("No price."); return; }
        try {
            auto ord = trading::pt_place_order(paper_id_, f.symbol_buf,
                f.side==trading::OrderSide::Buy?"buy":"sell",
                f.type==trading::OrderType::Market?"market":"limit",
                f.quantity, f.price>0?f.price:fp, false);
            trading::pt_fill_order(ord.id, fp, f.quantity);
            set_status(std::string("[Paper] Filled: ")+f.symbol_buf);
            refresh_paper_portfolio();
        } catch (const std::exception& e) { set_status(std::string("Paper fail: ")+e.what()); }
    } else {
        if (!connected_ || !active_broker_) { set_status("Not connected."); return; }
        auto* broker = trading::BrokerRegistry::instance().get(*active_broker_);
        if (!broker) { set_status("Broker not found."); return; }
        trading::UnifiedOrder order;
        order.symbol=f.symbol_buf; order.exchange=sel_exch_;
        order.side=f.side; order.order_type=f.type;
        order.quantity=f.quantity; order.price=f.price; order.stop_price=f.stop_price;
        order.product_type=f.product; order.validity=validity_label(f.validity);
        order.amo=f.is_amo;
        std::thread([this,broker,order,creds=active_creds_](){
            auto r = broker->place_order(creds,order);
            if(r.success){set_status("Placed: "+r.order_id);refresh_portfolio();}
            else set_status("Failed: "+r.error);
        }).detach();
    }
}

void EquityTradingScreen::connect_broker(trading::BrokerId id) {
    if (connecting_) return;
    auto* b = trading::BrokerRegistry::instance().get(id);
    if (!b) { set_status("Broker unavailable."); return; }

    int idx = -1;
    for (int i = 0; i < BROKER_META_COUNT; ++i)
        if (BROKER_META[i].id == id) { idx = i; break; }

    auto creds = b->load_credentials();
    if (creds.api_key.empty()) { set_status("Save credentials first."); return; }

    // TOTP brokers: re-authenticate via exchange_token_async, then finalize on main thread
    if (idx >= 0 && BROKER_META[idx].auth_type == BrokerAuthType::TOTP) {
        if (cred_ui_[idx].auth_code[0] == '\0') {
            set_status("Enter TOTP code first."); return;
        }
        exchange_token_async(idx);  // sets connect_fut_, connecting_=true
        // render() will poll connect_fut_ and call finish_connect()
        return;
    }

    // OAuth / ApiKey: need a saved token
    if (creds.access_token.empty()) { set_status("No access token — generate token first."); return; }
    finish_connect({true, id, creds, ""});
}

void EquityTradingScreen::finish_connect(ConnectResult r) {
    int idx = connecting_idx_;
    connecting_idx_ = -1;
    if (!r.success) {
        set_status("Auth failed: " + r.error);
        if (idx >= 0) { cred_ui_[idx].err = r.error; cred_ui_[idx].ok.clear(); }
        return;
    }
    active_creds_  = r.creds;
    active_broker_ = r.broker_id;
    connected_     = true;
    paper_mode_    = false;
    if (idx >= 0) { cred_ui_[idx].ok = "Connected!"; cred_ui_[idx].err.clear(); cred_ui_[idx].token_valid = true; }
    set_status(std::string("Connected: ") + trading::broker_id_str(r.broker_id));
    refresh_portfolio();
    start_ws_stream();
    fetch_quote();
    refresh_watchlist_quotes();
}

void EquityTradingScreen::refresh_portfolio() {
    if (loading_port_||!connected_||!active_broker_) return;
    auto* b = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!b) return;
    loading_port_=true; last_port_refresh_=std::chrono::steady_clock::now();
    std::thread([this,b,c=active_creds_](){
        auto pr=b->get_positions(c); if(pr.data) positions_=*pr.data;
        auto or_=b->get_orders(c);   if(or_.data) orders_=*or_.data;
        auto fr=b->get_funds(c);     if(fr.data) funds_=*fr.data;
        loading_port_=false;
    }).detach();
}

void EquityTradingScreen::refresh_paper_portfolio() {
    if (paper_id_.empty()) return;
    loading_port_=true; last_port_refresh_=std::chrono::steady_clock::now();
    std::thread([this](){
        try {
            auto rp=trading::pt_get_positions(paper_id_);
            positions_.clear();
            for(auto&p:rp){trading::BrokerPosition bp; bp.symbol=p.symbol; bp.exchange=sel_exch_;
                bp.quantity=p.quantity; bp.avg_price=p.entry_price; bp.ltp=p.current_price;
                bp.pnl=p.unrealized_pnl; bp.side=(p.side=="long")?"buy":"sell"; positions_.push_back(bp);}
            auto ro=trading::pt_get_orders(paper_id_,"");
            orders_.clear();
            for(auto&o:ro){trading::BrokerOrderInfo bo; bo.order_id=o.id; bo.symbol=o.symbol;
                bo.side=o.side; bo.order_type=o.order_type; bo.quantity=o.quantity;
                bo.status=o.status; orders_.push_back(bo);}
        } catch(...){}
        loading_port_=false;
    }).detach();
}

// Batch-fetch LTP for every watchlist entry in one REST call.
// AngelOne accepts up to 50 tokens per request — all 47 fit in one shot.
void EquityTradingScreen::refresh_watchlist_quotes() {
    if (loading_wl_quotes_ || !active_broker_) return;
    auto* b = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!b) return;
    loading_wl_quotes_ = true;

    // Build token list: "EXCHANGE:SYMBOL" for each watchlist entry
    std::vector<std::string> tokens;
    tokens.reserve(watchlist_.size());
    for (const auto& w : watchlist_)
        tokens.push_back(w.exchange + ":" + w.symbol);

    std::thread([this, b, tokens, c=active_creds_]() {
        auto r = b->get_quotes(c, tokens);
        if (r.data) {
            for (const auto& q : *r.data) {
                for (auto& w : watchlist_) {
                    if (w.symbol == q.symbol) {
                        w.ltp        = q.ltp;
                        w.change     = q.change;
                        w.change_pct = q.change_pct;
                        w.volume     = q.volume;
                        break;
                    }
                }
            }
        }
        loading_wl_quotes_ = false;
    }).detach();
}

void EquityTradingScreen::modify_order_async(const std::string& oid,
                                               double price, double qty, double trigger_price) {
    if (!connected_ || !active_broker_) { set_status("Not connected."); return; }
    auto* b = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!b) { set_status("Broker not found."); return; }
    json mods;
    if (price > 0)         mods["price"]         = price;
    if (qty > 0)           mods["quantity"]       = qty;
    if (trigger_price > 0) mods["trigger_price"]  = trigger_price;
    std::thread([this, b, oid, mods, c=active_creds_]() {
        auto r = b->modify_order(c, oid, mods);
        if (r.success) { set_status("Modified: " + oid); refresh_portfolio(); }
        else           set_status("Modify failed: " + r.error);
    }).detach();
    set_status("Modifying...");
}

void EquityTradingScreen::cancel_order_async(const std::string& oid) {
    if (paper_mode_) {
        std::thread([oid,this](){ trading::pt_cancel_order(oid); refresh_paper_portfolio(); }).detach();
    } else if (active_broker_) {
        auto* b = trading::BrokerRegistry::instance().get(*active_broker_);
        if (b) std::thread([this,b,oid,c=active_creds_](){ b->cancel_order(c,oid); refresh_portfolio(); }).detach();
    }
    set_status("Cancelling...");
}

void EquityTradingScreen::exchange_token_async(int idx) {
    if (connecting_) return;
    auto* b = trading::BrokerRegistry::instance().get(BROKER_META[idx].id);
    if (!b) { cred_ui_[idx].err = "Broker unavailable."; return; }

    // For TOTP: secret param = "client_code:pin"
    std::string secret_param = (BROKER_META[idx].auth_type == BrokerAuthType::TOTP && cred_ui_[idx].user_id[0])
        ? (std::string(cred_ui_[idx].user_id) + ":" + cred_ui_[idx].api_secret)
        : cred_ui_[idx].api_secret;

    connecting_     = true;
    connecting_idx_ = idx;
    cred_ui_[idx].err.clear();
    cred_ui_[idx].ok.clear();

    // Network-only future — no shared state touched inside
    connect_fut_ = std::async(std::launch::async,
        [b, idx,
         key    = std::string(cred_ui_[idx].api_key),
         secret = secret_param,
         code   = std::string(cred_ui_[idx].auth_code)]() -> ConnectResult {
            auto r = b->exchange_token(key, secret, code);
            if (!r.success) return {false, BROKER_META[idx].id, {}, r.error};
            json extra = r.additional_data.empty() ? json::object() : json::parse(r.additional_data);
            extra["token_date"] = today_ist_string();
            trading::BrokerCredentials creds;
            creds.broker_id       = trading::broker_id_str(BROKER_META[idx].id);
            creds.api_key         = key;
            creds.api_secret      = secret;
            creds.access_token    = r.access_token;
            creds.user_id         = r.user_id;
            creds.additional_data = extra.dump();
            b->save_credentials(creds);
            return {true, BROKER_META[idx].id, creds, ""};
        });
}

std::string EquityTradingScreen::generate_oauth_url(trading::BrokerId id, const char* k) {
    std::string key = k?k:""; if(key.empty()) return "";
    switch(id){
        case trading::BrokerId::Fyers:    return "https://api-t1.fyers.in/api/v3/generate-authcode?client_id="+key+"&redirect_uri=https://fincept.in/callback&response_type=code&state=fincept";
        case trading::BrokerId::Zerodha:  return "https://kite.trade/connect/login?api_key="+key+"&v=3";
        case trading::BrokerId::Upstox:   return "https://api.upstox.com/v2/login/authorization/dialog?response_type=code&client_id="+key+"&redirect_uri=https://fincept.in/callback";
        case trading::BrokerId::Dhan:     return "https://api.dhan.co/oauth2/authorize?client_id="+key+"&response_type=code&redirect_uri=https://fincept.in/callback";
        case trading::BrokerId::IIFL:     return "https://ttblaze.iifl.com/apiflow/CustomerValidation?apikey="+key;
        case trading::BrokerId::Groww:    return "https://api.groww.in/v1/oauth2/authorize?client_id="+key+"&response_type=code&redirect_uri=https://fincept.in/callback";
        case trading::BrokerId::IBKR:     return "https://www.interactivebrokers.com/authorize?client_id="+key;
        case trading::BrokerId::Tradier:  return "https://api.tradier.com/v1/oauth/authorize?client_id="+key+"&scope=read,write,market,trade&state=fincept";
        case trading::BrokerId::SaxoBank: return "https://live.logonvalidation.net/authorize?client_id="+key+"&response_type=code&redirect_uri=https://fincept.in/callback";
        default: return "";
    }
}

std::string EquityTradingScreen::today_ist_string() {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now() + hours(5) + minutes(30));
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[16]; std::snprintf(buf,sizeof(buf),"%04d-%02d-%02d",tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday);
    return buf;
}

// ═══════════════════════════════════════════════════════════════════════
// WebSocket streaming
// ═══════════════════════════════════════════════════════════════════════
void EquityTradingScreen::start_ws_stream() {
    if (!connected_ || !active_broker_) return;

    auto* b = trading::BrokerRegistry::instance().get(*active_broker_);
    if (!b) return;

    const char* adapter = b->ws_adapter_name();
    if (!adapter || adapter[0] == '\0') {
        // Broker has no WS adapter — stay on REST polling
        ws_streaming_ = false;
        return;
    }

    // Build symbol list from watchlist + currently selected symbol
    std::vector<trading::EquitySymbol> syms;
    // Always include selected symbol first
    if (!sel_sym_.empty())
        syms.push_back({sel_sym_, sel_exch_});
    for (const auto& w : watchlist_) {
        bool already = false;
        for (const auto& s : syms) if (s.symbol == w.symbol) { already = true; break; }
        if (!already) syms.push_back({w.symbol, w.exchange});
    }

    auto& esm = trading::EquityStreamManager::instance();
    esm.stop();

    // Register tick callback — updates quote_ if it's for the selected symbol
    ws_tick_cb_id_ = esm.on_tick([this](const trading::EquityTick& t) {
        // Update watchlist entry LTP
        for (auto& w : watchlist_) {
            if (w.symbol == t.symbol) {
                w.ltp        = t.ltp;
                w.change     = t.ltp - t.close;
                w.change_pct = t.close > 0 ? (w.change / t.close * 100.0) : 0.0;
            }
        }
        // Update selected quote
        if (t.symbol == sel_sym_) {
            quote_.ltp        = t.ltp;
            quote_.open       = t.open;
            quote_.high       = t.high;
            quote_.low        = t.low;
            quote_.bid        = t.bid;
            quote_.ask        = t.ask;
            quote_.volume     = t.volume;
            quote_.change     = t.ltp - t.close;
            quote_.change_pct = t.close > 0 ? (quote_.change / t.close * 100.0) : 0.0;
            has_quote_        = true;
        }
    });

    ws_status_cb_id_ = esm.on_status([this](bool conn, const std::string& msg) {
        if (conn) set_status(std::string("[WS] Connected"));
        else      set_status(std::string("[WS] ") + msg);
    });

    esm.start(adapter, active_creds_, syms);
    ws_streaming_ = true;
    set_status(std::string("WS stream starting: ") + adapter);
}

void EquityTradingScreen::stop_ws_stream() {
    if (!ws_streaming_) return;
    auto& esm = trading::EquityStreamManager::instance();
    if (ws_tick_cb_id_   >= 0) { esm.remove_tick_callback(ws_tick_cb_id_);     ws_tick_cb_id_   = -1; }
    if (ws_status_cb_id_ >= 0) { esm.remove_status_callback(ws_status_cb_id_); ws_status_cb_id_ = -1; }
    esm.stop();
    ws_streaming_ = false;
}

} // namespace fincept::equity_trading
