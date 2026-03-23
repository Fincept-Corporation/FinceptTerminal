#pragma once
// Paper Trading Engine — Asset-agnostic simulated trading
// Qt-adapted port from paper_trading.h/.cpp

#include "trading/TradingTypes.h"

#include <optional>
#include <vector>

namespace fincept::trading {

// --- Portfolio ---
PtPortfolio pt_create_portfolio(const QString& name, double balance, const QString& currency = "USD",
                                double leverage = 1.0, const QString& margin_mode = "cross", double fee_rate = 0.001,
                                const QString& exchange = "");
PtPortfolio pt_get_portfolio(const QString& id);
std::optional<PtPortfolio> pt_find_portfolio(const QString& name, const QString& exchange);
QVector<PtPortfolio> pt_list_portfolios(const QString& exchange = "");
void pt_delete_portfolio(const QString& id);
PtPortfolio pt_reset_portfolio(const QString& id);

// --- Orders ---
PtOrder pt_place_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                       const QString& order_type, double quantity, std::optional<double> price = std::nullopt,
                       std::optional<double> stop_price = std::nullopt, bool reduce_only = false);
void pt_cancel_order(const QString& order_id);
QVector<PtOrder> pt_get_orders(const QString& portfolio_id, const QString& status = "");

// --- Fill (core engine logic) ---
PtTrade pt_fill_order(const QString& order_id, double fill_price, std::optional<double> fill_qty = std::nullopt);

// --- Positions ---
QVector<PtPosition> pt_get_positions(const QString& portfolio_id);
void pt_update_position_price(const QString& portfolio_id, const QString& symbol, double price);

// --- Trades & Stats ---
QVector<PtTrade> pt_get_trades(const QString& portfolio_id, int64_t limit = 100);
PtStats pt_get_stats(const QString& portfolio_id);

} // namespace fincept::trading
