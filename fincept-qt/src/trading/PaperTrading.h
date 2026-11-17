#pragma once
// Paper Trading Engine — Asset-agnostic simulated trading
// Qt-adapted port from paper_trading.h/.cpp

#include "trading/TradingTypes.h"

#include <QDate>
#include <QDateTime>

#include <functional>
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
//
// `source`: who created the order (UnifiedOrder::source format: ticket, basket,
// algo:<id>, workflow:<id>, agent:<client>, …) — written as field "source" on the
// order's audit lines (accepted / rejected), omitted when empty. Not stored.
PtOrder pt_place_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                       const QString& order_type, double quantity, std::optional<double> price = std::nullopt,
                       std::optional<double> stop_price = std::nullopt, bool reduce_only = false,
                       const QString& exchange = "", const QString& product = "", const QString& source = "");
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

// ============================================================================
// Additions for the built-in paper account (no broker, delayed public prices).
// Everything below is opt-in: a portfolio that never calls these behaves exactly
// as before (FX factor 1, IST settlement, no delayed-price rules).
// ============================================================================

// --- Multi-currency (cash in the portfolio currency, prices in the instrument's) ---
// Units of the PORTFOLIO currency per one PRICE unit of `symbol` (minor units
// included: a GBp-quoted symbol in an INR portfolio gets INR-per-pence). Every
// cash amount the engine books for that symbol — margin blocked / held / released,
// fee, realized P&L (on the trade row and the position) — is multiplied by it,
// while prices, quantities and position marks stay in the instrument's own
// currency. In-memory, like the leverage config; unset = 1.0 (single-currency).
// Realized P&L is converted at the factor current at the closing fill (no FX
// gain/loss on the principal is simulated).
void pt_set_symbol_fx(const QString& portfolio_id, const QString& symbol, double fx_to_portfolio_ccy);
double pt_get_symbol_fx(const QString& portfolio_id, const QString& symbol); // 1.0 when unset
bool pt_has_symbol_fx(const QString& portfolio_id, const QString& symbol);

// True for portfolios settled per venue (the built-in paper account): the IST
// 15:30 intraday sweep (pt_settle_intraday) skips them; pt_settle_delayed_sessions
// settles them at each instrument's own session close instead.
bool pt_portfolio_settles_by_venue(const PtPortfolio& portfolio);

// --- Delayed-price simulation ---
// The newest delayed public quote of an instrument, as the simulator sees it.
struct PtDelayedQuote {
    double price = 0.0;        // last price, instrument currency
    qint64 received_ms = 0;    // when this app received it (epoch ms)
    qint64 printed_ms = 0;     // exchange time of that last trade (epoch ms); 0 = unknown
    bool session_open = false; // the instrument's venue is in its regular session now
    bool valid() const;
};

// A quote received longer ago than this is too old to fill a MARKET order while
// the market is open (the public feed refreshes every ~30 s; three minutes of
// silence means it has stalled).
inline constexpr qint64 kPtDelayedQuoteMaxAgeMs = 3 * 60 * 1000;

struct PtSimRequest {
    QString portfolio_id;
    QString symbol;     // canonical (Yahoo-form) symbol
    QString side;       // "buy" | "sell"
    QString order_type; // "market" | "limit" | "stop" | "stop_limit" (or "stop_loss" / "stop_loss_limit")
    double quantity = 0.0;
    std::optional<double> price;      // limit price
    std::optional<double> stop_price; // trigger
    QString exchange;                 // the instrument's venue id (leverage rules)
    QString product;                  // "MIS" | "CNC"
    bool reduce_only = false;
    double fx_to_account = 1.0;       // see pt_set_symbol_fx
    QString source;                   // see pt_place_order (audit field "source")
};

enum class PtSimOutcome { Filled, Resting, QueuedForOpen, Rejected };

struct PtSimResult {
    PtSimOutcome outcome = PtSimOutcome::Rejected;
    PtOrder order;                // the order as stored (empty id when nothing was stored)
    std::optional<PtTrade> trade; // the simulated fill, when Filled
    QString message;              // plain language, says what happened and why
};

// Place an order on delayed prices. Rules:
//   MARKET, session open : fills at the latest delayed price — rejected when no
//                          price has arrived yet, or when it was received more
//                          than `max_quote_age_ms` ago (the feed has stalled).
//   MARKET, session not open: queued (pending "market" order in the matcher); it
//                          fills at the first delayed price printed after the open.
//                          Rejected when there is no price at all (no margin
//                          reference).
//   LIMIT / STOP / STOP-LIMIT: stored and handed to OrderMatcher; while the session
//                          is open and the latest delayed price is fresh, the
//                          matcher checks the new order against it at once (a
//                          marketable limit fills at its limit, a triggered stop at
//                          the delayed price — OrderMatcher's own rules); else it
//                          rests until a delayed price printed after it crosses.
//   Margin / validation failures from pt_place_order come back as Rejected.
PtSimResult pt_submit_delayed_order(const PtSimRequest& request, const PtDelayedQuote& quote, qint64 now_ms,
                                    qint64 max_quote_age_ms = kPtDelayedQuoteMaxAgeMs);

// Run the matcher for one symbol of a delayed-price portfolio on a new delayed
// quote: resting orders (and queued MARKET orders) fill only while the session is
// open and only on a price printed at or after the order was placed; SL / TP
// brackets fire on the same prices. Returns the number of orders filled. Does not
// mark positions (callers coalesce pt_update_position_price).
int pt_match_delayed_quote(const QString& portfolio_id, const QString& symbol, const PtDelayedQuote& quote);

// Session-close settlement for a venue-settled portfolio. `close_after(symbol, t)`
// returns the close (UTC) of the first regular session of the symbol's venue that
// ends after `t`, or an invalid QDateTime for a venue that never closes (crypto).
//   * an intraday (MIS) position is squared off at its last mark, stamped at the
//     close of the session it was opened in, once that close has passed;
//   * a resting DAY order expires (cancelled, margin released) at the close of the
//     first session ending after it was placed — so an order queued while the market
//     was closed lives through the next session.
// Returns the number of positions squared off + orders expired.
int pt_settle_delayed_sessions(const QString& portfolio_id,
                               const std::function<QDateTime(const QString& symbol, const QDateTime& after_utc)>& close_after,
                               const QDateTime& now_utc);

// Change a resting paper order's quantity and/or limit price (and trigger, when
// > 0): the order is cancelled and re-placed with the new values (paper has no
// in-place modify), keeping side, type, product, exchange and reduce-only. The new
// order replaces the old one in the matcher (and, like a real modify, loses its
// time priority). Throws std::runtime_error when the order is not resting, or when
// the replacement is refused (e.g. insufficient margin) — the original is then
// re-placed unchanged and the message says so. For an SL-M ("stop") order
// `new_price` is its trigger.
PtOrder pt_modify_order(const QString& order_id, double new_quantity, double new_price, double new_stop_price = 0.0);

} // namespace fincept::trading
