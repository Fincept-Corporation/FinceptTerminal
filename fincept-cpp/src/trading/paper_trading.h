#pragma once
// Paper Trading Engine — Asset-agnostic simulated trading
// Direct port of Rust paper_trading.rs (550 lines)

#include "portfolio/portfolio_types.h"
#include <vector>
#include <optional>
#include <string>

namespace fincept::trading {

// --- Portfolio ---
PtPortfolio pt_create_portfolio(const std::string& name, double balance,
                                 const std::string& currency = "USD",
                                 double leverage = 1.0,
                                 const std::string& margin_mode = "cross",
                                 double fee_rate = 0.001,
                                 const std::string& exchange = "");
PtPortfolio pt_get_portfolio(const std::string& id);
std::optional<PtPortfolio> pt_find_portfolio(const std::string& name,
                                              const std::string& exchange);
std::vector<PtPortfolio> pt_list_portfolios(const std::string& exchange = "");
void pt_delete_portfolio(const std::string& id);
PtPortfolio pt_reset_portfolio(const std::string& id);

// --- Orders ---
PtOrder pt_place_order(const std::string& portfolio_id, const std::string& symbol,
                        const std::string& side, const std::string& order_type,
                        double quantity, std::optional<double> price = std::nullopt,
                        std::optional<double> stop_price = std::nullopt,
                        bool reduce_only = false);
void pt_cancel_order(const std::string& order_id);
std::vector<PtOrder> pt_get_orders(const std::string& portfolio_id,
                                    const std::string& status = "");

// --- Fill (core engine logic) ---
PtTrade pt_fill_order(const std::string& order_id, double fill_price,
                       std::optional<double> fill_qty = std::nullopt);

// --- Positions ---
std::vector<PtPosition> pt_get_positions(const std::string& portfolio_id);
void pt_update_position_price(const std::string& portfolio_id,
                               const std::string& symbol, double price);

// --- Trades & Stats ---
std::vector<PtTrade> pt_get_trades(const std::string& portfolio_id, int64_t limit = 100);
PtStats pt_get_stats(const std::string& portfolio_id);

} // namespace fincept::trading
