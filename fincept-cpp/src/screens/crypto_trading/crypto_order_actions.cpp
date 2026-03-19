#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/theme.h"
#include "trading/exchange_service.h"
#include "trading/paper_trading.h"
#include "trading/order_matcher.h"
#include "core/logger.h"
#include "core/notification.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <thread>

namespace fincept::crypto {

using namespace theme::colors;

void CryptoTradingScreen::submit_order() {
    order_form_.error.clear();
    order_form_.success.clear();

    bool is_live = (trading_mode_ == TradingMode::Live);
    LOG_INFO(TAG, "Submitting %s order: symbol=%s side=%d type=%d qty='%s' price='%s'",
             is_live ? "LIVE" : "PAPER",
             selected_symbol_.c_str(), order_form_.side_idx, order_form_.type_idx,
             order_form_.quantity_buf, order_form_.price_buf);

    // Live mode guard — require credentials
    if (is_live) {
        if (!has_credentials_) {
            order_form_.error = "API credentials required. Click 'API' button to configure.";
            order_form_.msg_timer = 5.0f;
            return;
        }
        LOG_INFO(TAG, "Live order being routed to %s exchange API", exchange_id_.c_str());
    }

    // Parse quantity
    double qty = 0;
    try {
        qty = std::stod(order_form_.quantity_buf);
    } catch (...) {
        order_form_.error = "Invalid quantity";
        order_form_.msg_timer = 5.0f;
        LOG_ERROR(TAG, "Invalid quantity: '%s'", order_form_.quantity_buf);
        return;
    }
    if (qty <= 0) {
        order_form_.error = "Quantity must be > 0";
        order_form_.msg_timer = 5.0f;
        return;
    }

    static const char* type_strs[] = {"market", "limit", "stop", "stop_limit"};
    std::string order_type = type_strs[order_form_.type_idx];
    std::string side = order_form_.side_idx == 0 ? "buy" : "sell";

    std::optional<double> price;
    std::optional<double> stop_price;

    // For market orders, use current ticker price as reference for margin calculation
    if (order_form_.type_idx == 0) {
        double ref = (side == "buy") ? current_ticker_.ask : current_ticker_.bid;
        if (ref <= 0) ref = current_ticker_.last;
        if (ref <= 0) {
            order_form_.error = "No market price available yet";
            order_form_.msg_timer = 5.0f;
            LOG_ERROR(TAG, "Market order rejected: no price data");
            return;
        }
        price = ref;
    }

    if (order_form_.type_idx == 1 || order_form_.type_idx == 3) {
        try {
            double p = std::stod(order_form_.price_buf);
            if (p <= 0) {
                order_form_.error = "Price must be > 0";
                order_form_.msg_timer = 5.0f;
                return;
            }
            price = p;
        } catch (...) {
            order_form_.error = "Invalid price";
            order_form_.msg_timer = 5.0f;
            return;
        }
    }

    if (order_form_.type_idx >= 2) {
        try {
            double sp = std::stod(order_form_.stop_price_buf);
            if (sp <= 0) {
                order_form_.error = "Stop price must be > 0";
                order_form_.msg_timer = 5.0f;
                return;
            }
            stop_price = sp;
        } catch (...) {
            order_form_.error = "Invalid stop price";
            order_form_.msg_timer = 5.0f;
            return;
        }
    }

    // Fee-inclusive balance check for paper BUY orders (ES.45: named values not magic)
    if (trading_mode_ == TradingMode::Paper && side == "buy" && price.has_value()) {
        const bool   is_maker      = (order_form_.type_idx == 1); // limit → maker
        const double effective_fee = market_info_.has_data
                                     ? (is_maker ? market_info_.maker_fee : market_info_.taker_fee)
                                     : 0.001; // fallback 0.1% if fees not yet loaded
        const double notional      = qty * price.value();
        const double total_incl    = notional * (1.0 + effective_fee);
        if (total_incl > portfolio_.balance) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "Insufficient balance ($%.2f avail, $%.2f needed incl. fee)",
                          portfolio_.balance, total_incl);
            order_form_.error = buf;
            order_form_.msg_timer = 5.0f;
            return;
        }
    }

    try {
        if (trading_mode_ == TradingMode::Live) {
            // Live trading — route through exchange API on background thread
            order_submitting_.store(true);
            const std::string sym = selected_symbol_;
            std::string pid = portfolio_id_;
            launch_async(std::thread([this, sym, side, order_type, qty, price, pid]() {
                try {
                    auto& svc = trading::ExchangeService::instance();
                    auto result = svc.place_order(sym, side, order_type, qty,
                                                   price.value_or(0.0));
                    LOG_INFO(TAG, "Live order placed: %s", result.dump(2).c_str());
                    core::notify::send("Order Placed",
                        sym + " " + side + " " + std::to_string(qty),
                        core::NotifyLevel::Success);
                    {
                        std::lock_guard<std::mutex> lock(data_mutex_);
                        order_form_.success = "Live order sent!";
                        order_form_.msg_timer = 3.0f;
                    }
                    trading::ExchangeService::instance().watch_symbol(sym, pid);
                    refresh_portfolio_data();
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    order_form_.error = e.what();
                    order_form_.msg_timer = 5.0f;
                    LOG_ERROR(TAG, "Live order FAILED: %s", e.what());
                    core::notify::send("Order Failed", e.what(), core::NotifyLevel::Error);
                }
                order_submitting_.store(false);
            }));
            // Clear form immediately
            order_form_.quantity_buf[0]   = '\0';
            order_form_.price_buf[0]      = '\0';
            order_form_.stop_price_buf[0] = '\0';
            order_form_.sl_price_buf[0]   = '\0';
            order_form_.tp_price_buf[0]   = '\0';
            return; // skip the rest — thread handles result
        } else {
            // Paper trading
            auto order = trading::pt_place_order(
                portfolio_id_, selected_symbol_, side, order_type,
                qty, price, stop_price, order_form_.reduce_only);

            LOG_INFO(TAG, "Order created: id=%s type=%s side=%s qty=%.6f",
                     order.id.c_str(), order_type.c_str(), side.c_str(), qty);

            // For market orders, immediately fill at current price
            if (order_type == "market" && current_ticker_.last > 0) {
                double fill_price = side == "buy" ? current_ticker_.ask : current_ticker_.bid;
                if (fill_price <= 0) fill_price = current_ticker_.last;
                LOG_INFO(TAG, "Market order fill at %.2f", fill_price);
                trading::pt_fill_order(order.id, fill_price);
                {
                    char msg[128];
                    std::snprintf(msg, sizeof(msg), "%s %s %.4f @ %.2f",
                                  side == "buy" ? "BUY" : "SELL",
                                  selected_symbol_.c_str(), qty, fill_price);
                    core::notify::send("Order Filled", msg, core::NotifyLevel::Success);
                }
            } else {
                trading::OrderMatcher::instance().add_order(order);
                LOG_INFO(TAG, "Order added to matcher");
            }

            // Register SL/TP triggers if provided
            {
                const double sl = (order_form_.sl_price_buf[0] != '\0')
                                  ? std::atof(order_form_.sl_price_buf) : 0.0;
                const double tp = (order_form_.tp_price_buf[0] != '\0')
                                  ? std::atof(order_form_.tp_price_buf) : 0.0;
                if ((sl > 0.0 || tp > 0.0) && !order.id.empty()) {
                    trading::OrderMatcher::instance().set_sl_tp(
                        portfolio_id_, selected_symbol_, order.id, sl, tp);
                }
            }

            order_form_.success = "Order placed: " + order.id.substr(0, 8);
            order_form_.msg_timer = 3.0f;
        }

        // Ensure the traded symbol is watched for price updates
        trading::ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);

        // Clear form
        order_form_.quantity_buf[0]   = '\0';
        order_form_.price_buf[0]      = '\0';
        order_form_.stop_price_buf[0] = '\0';
        order_form_.sl_price_buf[0]   = '\0';
        order_form_.tp_price_buf[0]   = '\0';

        // Refresh portfolio data immediately
        refresh_portfolio_data();
    } catch (const std::exception& e) {
        order_form_.error = e.what();
        order_form_.msg_timer = 5.0f;
        LOG_ERROR(TAG, "Order FAILED: %s", e.what());
    }
}

void CryptoTradingScreen::cancel_order(const std::string& order_id) {
    if (trading_mode_ == TradingMode::Live) {
        // Live cancel — route through exchange API
        LOG_INFO(TAG, "Cancelling live order: %s on %s", order_id.c_str(), exchange_id_.c_str());
        const std::string sym = selected_symbol_;
        launch_async(std::thread([this, order_id, sym]() {
            try {
                auto& svc = trading::ExchangeService::instance();
                auto result = svc.cancel_order(order_id, sym);
                LOG_INFO(TAG, "Live order cancelled: %s", result.dump(2).c_str());
                // Refresh live orders
                async_fetch_live_orders();
            } catch (const std::exception& e) {
                LOG_ERROR(TAG, "Live cancel failed: %s", e.what());
                std::lock_guard<std::mutex> lock(data_mutex_);
                order_form_.error = std::string("Cancel failed: ") + e.what();
                order_form_.msg_timer = 5.0f;
            }
        }));
        return;
    }

    // Paper cancel
    LOG_INFO(TAG, "Cancelling paper order: %s", order_id.c_str());
    try {
        trading::pt_cancel_order(order_id);
        trading::OrderMatcher::instance().remove_order(order_id);
        refresh_portfolio_data();
        LOG_INFO(TAG, "Order cancelled: %s", order_id.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Cancel failed: %s", e.what());
    }
}

// ============================================================================
// Live trading data fetchers — call exchange API via ExchangeService
// ============================================================================

void CryptoTradingScreen::async_fetch_live_positions() {
    if (live_positions_fetching_ || !has_credentials_) return;
    live_positions_fetching_ = true;

    launch_async(std::thread([this]() {
        try {
            auto& svc = trading::ExchangeService::instance();
            auto result = svc.fetch_positions_live();
            if (result.value("success", false) && result.contains("data")) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                live_positions_.clear();
                auto& positions = result["data"]["positions"];
                if (positions.is_array()) {
                    for (const auto& p : positions) {
                        LivePosition pos;
                        pos.symbol = p.value("symbol", "");
                        pos.side = p.value("side", "long");
                        pos.quantity = std::abs(p.value("contracts", p.value("contractSize", 0.0)));
                        if (pos.quantity == 0.0) pos.quantity = std::abs(p.value("amount", 0.0));
                        pos.entry_price = p.value("entryPrice", 0.0);
                        pos.current_price = p.value("markPrice", p.value("liquidationPrice", 0.0));
                        pos.unrealized_pnl = p.value("unrealizedPnl", 0.0);
                        pos.leverage = p.value("leverage", 1.0);
                        if (pos.quantity > 0.0) {
                            live_positions_.push_back(pos);
                        }
                    }
                }
                live_data_loaded_ = true;
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "Live positions fetch failed: %s", e.what());
        }
        live_positions_fetching_ = false;
    }));
}

void CryptoTradingScreen::async_fetch_live_orders() {
    if (live_orders_fetching_ || !has_credentials_) return;
    live_orders_fetching_ = true;

    launch_async(std::thread([this]() {
        try {
            auto& svc = trading::ExchangeService::instance();
            auto result = svc.fetch_open_orders_live();
            if (result.value("success", false) && result.contains("data")) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                live_orders_.clear();
                auto& orders = result["data"]["orders"];
                if (orders.is_array()) {
                    for (const auto& o : orders) {
                        LiveOrder ord;
                        ord.id = o.value("id", "");
                        ord.symbol = o.value("symbol", "");
                        ord.side = o.value("side", "");
                        ord.type = o.value("type", "");
                        ord.status = o.value("status", "");
                        ord.quantity = o.value("amount", 0.0);
                        ord.price = o.value("price", 0.0);
                        ord.filled_qty = o.value("filled", 0.0);
                        ord.timestamp = o.value("datetime", "");
                        live_orders_.push_back(ord);
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "Live orders fetch failed: %s", e.what());
        }
        live_orders_fetching_ = false;
    }));
}

void CryptoTradingScreen::async_fetch_live_balance() {
    if (live_balance_fetching_ || !has_credentials_) return;
    live_balance_fetching_ = true;

    launch_async(std::thread([this]() {
        try {
            auto& svc = trading::ExchangeService::instance();
            auto result = svc.fetch_balance();
            if (result.value("success", false) && result.contains("data")) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                auto& bal = result["data"];
                // ccxt returns balance as {free: {USDT: x}, used: {USDT: x}, total: {USDT: x}}
                if (bal.contains("total") && bal["total"].is_object()) {
                    live_equity_ = bal["total"].value("USDT", bal["total"].value("USD", 0.0));
                }
                if (bal.contains("free") && bal["free"].is_object()) {
                    live_balance_ = bal["free"].value("USDT", bal["free"].value("USD", 0.0));
                }
                if (bal.contains("used") && bal["used"].is_object()) {
                    live_used_margin_ = bal["used"].value("USDT", bal["used"].value("USD", 0.0));
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "Live balance fetch failed: %s", e.what());
        }
        live_balance_fetching_ = false;
    }));
}

void CryptoTradingScreen::async_fetch_live_trades() {
    if (live_trades_fetching_ || !has_credentials_) return;
    live_trades_fetching_ = true;

    const std::string sym = selected_symbol_;
    launch_async(std::thread([this, sym]() {
        try {
            auto& svc = trading::ExchangeService::instance();
            auto result = svc.fetch_my_trades_live(sym, 50);
            if (result.value("success", false) && result.contains("data")) {
                std::lock_guard<std::mutex> lock(data_mutex_);
                live_trades_.clear();
                auto& trades = result["data"]["trades"];
                if (trades.is_array()) {
                    for (const auto& t : trades) {
                        LiveTrade tr;
                        tr.id = t.value("id", "");
                        tr.symbol = t.value("symbol", sym);
                        tr.side = t.value("side", "");
                        tr.price = t.value("price", 0.0);
                        tr.quantity = t.value("amount", 0.0);
                        // fee can be an object {cost: x, currency: y} or a number
                        if (t.contains("fee") && t["fee"].is_object()) {
                            tr.fee = t["fee"].value("cost", 0.0);
                        } else {
                            tr.fee = t.value("fee", 0.0);
                        }
                        tr.timestamp = t.value("datetime", "");
                        live_trades_.push_back(tr);
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "Live trades fetch failed: %s", e.what());
        }
        live_trades_fetching_ = false;
    }));
}

} // namespace fincept::crypto
