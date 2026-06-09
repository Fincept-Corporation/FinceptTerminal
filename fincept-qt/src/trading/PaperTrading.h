#pragma once
// Paper Trading Engine — Asset-agnostic simulated trading
// Qt-adapted port from paper_trading.h/.cpp

#include "trading/TradingTypes.h"

#include <QDate>

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

// Set a paper portfolio's available cash directly (fake money). Used by the
// portfolio-replication "top up" action so an exact 1:1 copy of a large source
// portfolio always has enough simulated cash to fill. Persists immediately.
// Throws std::runtime_error on a non-finite/negative amount or DB error.
void pt_set_balance(const QString& portfolio_id, double new_balance);

// --- Leverage / Margin config (Phase 3 §4) ---
// Per-portfolio leverage rules + market-hours enforcement live in an in-memory
// config map (the existing pt_portfolios schema has no columns for them, and we
// must not add a migration). Defaults match OpenAlgo: equity MIS 5x, CNC 1x,
// futures 10x, options buy/sell 1x. enforce_market_hours defaults to FALSE so
// existing behavior is unchanged.
void pt_set_leverage_config(const QString& portfolio_id, const PtLeverageConfig& cfg);
PtLeverageConfig pt_get_leverage_config(const QString& portfolio_id);
void pt_set_enforce_market_hours(const QString& portfolio_id, bool enforce);
bool pt_get_enforce_market_hours(const QString& portfolio_id);

// --- Instrument-type helpers (Phase 3 §4) ---
// Detect derivative type from the canonical symbol suffix (CE/PE = option,
// FUT = future). Mirrors OpenAlgo's utils/symbol_utils.py.
bool pt_is_option(const QString& symbol);
bool pt_is_future(const QString& symbol);

// --- Margin calculation (Phase 3 §4) ---
// trade_value = quantity * price; required margin = trade_value / leverage,
// where leverage is selected by product/instrument-type/exchange/side.
double pt_calculate_required_margin(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                    const QString& product, double quantity, double price, const QString& side);

// --- Exchange hours (Phase 3 §4) ---
// IST-aware (UTC+5:30) market-open check. NSE/BSE/NFO/BFO 09:15–15:30,
// MCX 09:00–23:30, CDS 09:00–17:00; all other/unknown exchanges (incl. crypto)
// are treated as always open. This is a free function — enforcement is opt-in
// per portfolio via pt_set_enforce_market_hours().
bool pt_is_market_open(const QString& exchange);

// --- Orders ---
// NOTE (Phase 3 §4): `exchange` and `product` are NEW trailing params with
// defaults — existing call sites (which pass up to `reduce_only`) compile
// unchanged. When provided, they drive per-product leverage and (if the
// portfolio opts in) market-hours enforcement.
//
// Rejection contract: to preserve the existing exception-based contract that
// all current callers rely on (they wrap pt_place_order in try/catch and treat
// a throw as failure), an order that fails the margin or market-hours check
// still THROWS std::runtime_error — but BEFORE throwing it persists a
// status == "rejected" order record (with margin_blocked = 0) so the rejection
// is visible in the order book. Successfully placed orders that increase
// exposure have their required margin blocked and recorded on the order
// (order.margin_blocked) and in pt_margin_blocks.
PtOrder pt_place_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                       const QString& order_type, double quantity, std::optional<double> price = std::nullopt,
                       std::optional<double> stop_price = std::nullopt, bool reduce_only = false,
                       const QString& exchange = "", const QString& product = "");
void pt_cancel_order(const QString& order_id);
QVector<PtOrder> pt_get_orders(const QString& portfolio_id, const QString& status = "");

// --- Day-scoped order/trade queries (v040) ---
// Orders / executions whose timestamp falls on the given IST calendar day. Backs
// the order book's per-day view (default today, date-selector for history), so the
// book starts empty each session instead of accumulating every past order.
QVector<PtOrder> pt_get_orders_for_day(const QString& portfolio_id, const QDate& ist_day);
QVector<PtTrade> pt_get_trades_for_day(const QString& portfolio_id, const QDate& ist_day);

// --- Product conversion (v040) ---
// Convert an open position's broker product in place (e.g. MIS -> CNC). Re-prices
// the locked margin to the new product (CNC/delivery needs full notional vs MIS
// notional/leverage) and locks the extra from available balance; throws
// std::runtime_error if the balance is insufficient. After an MIS->CNC convert the
// position carries overnight (skipped by the 15:30 auto-square) and the Equity
// screen renders it in the Holdings tab.
void pt_convert_position_product(const QString& position_id, const QString& new_product);

// --- Intraday settlement / auto square-off (v040) ---
// Square off every open MIS (intraday) position at its last known price when the
// IST clock is at/after 15:30, OR when the position was opened on an earlier IST
// day (catch-up for when the app was closed at the cutoff). Also cancels stale
// pending DAY orders left from prior sessions. CNC/NRML carry-forward positions are
// untouched. Returns the number of positions squared off.
int pt_settle_intraday(const QString& portfolio_id);
int pt_settle_intraday_all();

// --- Fill (core engine logic) ---
// `fill_time` (ISO UTC) overrides the trade/fill timestamp; empty = now. Used by
// intraday settlement to stamp an auto-square at the session close it belongs to
// (so a catch-up square-off lands in that day's book, not today's).
PtTrade pt_fill_order(const QString& order_id, double fill_price, std::optional<double> fill_qty = std::nullopt,
                      const QString& fill_time = {});

// --- Positions ---
QVector<PtPosition> pt_get_positions(const QString& portfolio_id);
void pt_update_position_price(const QString& portfolio_id, const QString& symbol, double price);

// --- Trades & Stats ---
QVector<PtTrade> pt_get_trades(const QString& portfolio_id, int64_t limit = 100);
PtStats pt_get_stats(const QString& portfolio_id);

} // namespace fincept::trading
