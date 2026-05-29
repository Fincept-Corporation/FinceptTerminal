// Paper Trading Engine — Business logic layer
// Delegates all DB access to PaperTradingRepository.
// Contains: validation, margin checks, fill engine, position management.

#include "trading/PaperTrading.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/PaperTradingRepository.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>
#include <QUuid>

#include <cmath>
#include <stdexcept>

namespace fincept::trading {

static QString generate_uuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

static QString now_rfc3339() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

static auto& repo() {
    return PaperTradingRepository::instance();
}

// Mutex protecting fill engine — prevents concurrent fills on the same portfolio
// from corrupting balance or positions.
static QMutex s_fill_mutex;

// ============================================================================
// Leverage / Market-hours Config (Phase 3 §4)
// ============================================================================
//
// Per-portfolio leverage rules and the market-hours enforcement flag are held
// in memory rather than in the pt_portfolios table. The existing schema has no
// columns for them and the task forbids adding a migration / editing CMake, so
// an in-memory map (rebuilt at runtime by callers via the setters) is the
// source of truth. Defaults match OpenAlgo's sandbox.

namespace {

struct PtRuntimeConfig {
    PtLeverageConfig leverage;
    bool enforce_market_hours = false;
};

QMutex s_config_mutex;
QHash<QString, PtRuntimeConfig>& config_map() {
    static QHash<QString, PtRuntimeConfig> m;
    return m;
}

PtRuntimeConfig config_for(const QString& portfolio_id) {
    QMutexLocker lock(&s_config_mutex);
    return config_map().value(portfolio_id, PtRuntimeConfig{});
}

} // namespace

void pt_set_leverage_config(const QString& portfolio_id, const PtLeverageConfig& cfg) {
    QMutexLocker lock(&s_config_mutex);
    config_map()[portfolio_id].leverage = cfg;
}

PtLeverageConfig pt_get_leverage_config(const QString& portfolio_id) {
    return config_for(portfolio_id).leverage;
}

void pt_set_enforce_market_hours(const QString& portfolio_id, bool enforce) {
    QMutexLocker lock(&s_config_mutex);
    config_map()[portfolio_id].enforce_market_hours = enforce;
}

bool pt_get_enforce_market_hours(const QString& portfolio_id) {
    return config_for(portfolio_id).enforce_market_hours;
}

// ============================================================================
// Instrument-type helpers (Phase 3 §4)
// ============================================================================

bool pt_is_option(const QString& symbol) {
    // Canonical option symbols end with CE (call) or PE (put), e.g.
    // "NIFTY28FEB2520000CE". Match case-insensitively on the suffix.
    return symbol.endsWith("CE", Qt::CaseInsensitive) || symbol.endsWith("PE", Qt::CaseInsensitive);
}

bool pt_is_future(const QString& symbol) {
    // Canonical future symbols end with FUT, e.g. "NIFTY28FEB25FUT".
    return symbol.endsWith("FUT", Qt::CaseInsensitive);
}

// ============================================================================
// Margin calculation (Phase 3 §4)
// ============================================================================

namespace {

// Select leverage for a trade based on instrument type, product, exchange and
// side. Mirrors OpenAlgo fund_manager.py::_get_leverage.
double select_leverage(const PtLeverageConfig& cfg, const QString& symbol, const QString& exchange,
                       const QString& product, const QString& side) {
    // Futures take priority (FUT suffix), then options (CE/PE suffix).
    if (pt_is_future(symbol))
        return cfg.futures;
    if (pt_is_option(symbol))
        return (side.compare("buy", Qt::CaseInsensitive) == 0) ? cfg.options_buy : cfg.options_sell;

    // Equity: leverage depends on product. MIS (intraday) gets equity_mis,
    // everything else (CNC/NRML/delivery/blank) gets equity_cnc.
    const QString ex = exchange.toUpper();
    if (ex == "NSE" || ex == "BSE" || ex.isEmpty()) {
        if (product.compare("MIS", Qt::CaseInsensitive) == 0 || product.compare("intraday", Qt::CaseInsensitive) == 0)
            return cfg.equity_mis;
        return cfg.equity_cnc;
    }

    // Unknown exchange: fall back to 1x (full margin) — safest default.
    return 1.0;
}

} // namespace

double pt_calculate_required_margin(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                    const QString& product, double quantity, double price, const QString& side) {
    if (!std::isfinite(quantity) || quantity <= 0.0 || !std::isfinite(price) || price <= 0.0)
        return 0.0;

    PtLeverageConfig cfg = pt_get_leverage_config(portfolio_id);
    double leverage = select_leverage(cfg, symbol, exchange, product, side);
    if (!std::isfinite(leverage) || leverage <= 0.0)
        leverage = 1.0;

    double trade_value = quantity * price;
    return trade_value / leverage;
}

// ============================================================================
// Exchange hours (Phase 3 §4)
// ============================================================================

bool pt_is_market_open(const QString& exchange) {
    // IST = UTC + 5:30.
    QDateTime ist = QDateTime::currentDateTimeUtc().addSecs(5 * 3600 + 30 * 60);
    QTime t = ist.time();
    const QString ex = exchange.toUpper();

    if (ex == "NSE" || ex == "BSE" || ex == "NFO" || ex == "BFO") {
        // Equity / equity derivatives: 09:15 – 15:30 IST.
        return t >= QTime(9, 15) && t <= QTime(15, 30);
    }
    if (ex == "MCX") {
        // Commodities: 09:00 – 23:30 IST.
        return t >= QTime(9, 0) && t <= QTime(23, 30);
    }
    if (ex == "CDS" || ex == "BCD") {
        // Currency derivatives: 09:00 – 17:00 IST.
        return t >= QTime(9, 0) && t <= QTime(17, 0);
    }

    // Unknown / crypto / international: always open (24/7).
    return true;
}

// ============================================================================
// Portfolio Operations
// ============================================================================

PtPortfolio pt_create_portfolio(const QString& name, double balance, const QString& currency, double leverage,
                                const QString& margin_mode, double fee_rate, const QString& exchange) {
    if (name.isEmpty())
        throw std::runtime_error("Portfolio name cannot be empty");
    if (!std::isfinite(balance) || balance <= 0.0)
        throw std::runtime_error("Invalid balance: must be positive");
    if (!std::isfinite(leverage) || leverage <= 0.0)
        throw std::runtime_error("Invalid leverage: must be positive");
    if (!std::isfinite(fee_rate) || fee_rate < 0.0 || fee_rate > 1.0)
        throw std::runtime_error("Invalid fee_rate: must be between 0 and 1");

    PtPortfolio p;
    p.id = generate_uuid();
    p.name = name;
    p.initial_balance = balance;
    p.balance = balance;
    p.currency = currency;
    p.leverage = leverage;
    p.margin_mode = margin_mode;
    p.fee_rate = fee_rate;
    p.exchange = exchange;
    p.created_at = now_rfc3339();

    auto r = repo().insert_portfolio(p);
    if (r.is_err())
        throw std::runtime_error(r.error());
    return p;
}

PtPortfolio pt_get_portfolio(const QString& id) {
    auto r = repo().get_portfolio(id);
    if (r.is_err())
        throw std::runtime_error(r.error());
    return r.value();
}

std::optional<PtPortfolio> pt_find_portfolio(const QString& name, const QString& exchange) {
    return repo().find_portfolio(name, exchange);
}

QVector<PtPortfolio> pt_list_portfolios(const QString& exchange) {
    auto r = repo().list_portfolios(exchange);
    if (r.is_err())
        return {};
    return r.value();
}

void pt_delete_portfolio(const QString& id) {
    repo().delete_all_trades(id);
    repo().cancel_all_orders(id);
    repo().delete_all_positions(id);
    repo().delete_portfolio(id);
}

PtPortfolio pt_reset_portfolio(const QString& id) {
    pt_get_portfolio(id); // verify exists
    repo().delete_all_trades(id);
    repo().cancel_all_orders(id);
    repo().delete_all_positions(id);
    repo().delete_all_margin_blocks(id); // clear any blocked margin (Phase 3 §4)
    // Reset balance to initial
    auto p = pt_get_portfolio(id);
    repo().update_balance(id, p.initial_balance);
    return pt_get_portfolio(id);
}

// ============================================================================
// Order Operations
// ============================================================================

// Persist a rejected order record (best-effort) then throw, so the rejection is
// both visible in the order book and surfaced to callers via the existing
// exception-based contract.
[[noreturn]] static void reject_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                                      const QString& order_type, double quantity, std::optional<double> price,
                                      std::optional<double> stop_price, bool reduce_only, const QString& product,
                                      const QString& exchange, const std::string& reason) {
    PtOrder rej;
    rej.id = generate_uuid();
    rej.portfolio_id = portfolio_id;
    rej.symbol = symbol;
    rej.side = side;
    rej.order_type = order_type;
    rej.quantity = quantity;
    rej.price = price;
    rej.stop_price = stop_price;
    rej.filled_qty = 0.0;
    rej.status = "rejected";
    rej.reduce_only = reduce_only;
    rej.margin_blocked = 0.0;
    rej.product = product;
    rej.exchange = exchange;
    rej.created_at = now_rfc3339();
    repo().insert_order(rej); // best-effort; ignore failure
    throw std::runtime_error(reason);
}

PtOrder pt_place_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                       const QString& order_type, double quantity, std::optional<double> price,
                       std::optional<double> stop_price, bool reduce_only, const QString& exchange,
                       const QString& product) {
    // Validation
    if (order_type != "market" && order_type != "limit" && order_type != "stop" && order_type != "stop_limit")
        throw std::runtime_error("Invalid order type: " + order_type.toStdString());
    if (side != "buy" && side != "sell")
        throw std::runtime_error("Invalid side: " + side.toStdString());
    if (!std::isfinite(quantity) || quantity <= 0.0)
        throw std::runtime_error("Invalid quantity");
    if (price && (!std::isfinite(*price) || *price <= 0.0))
        throw std::runtime_error("Invalid price");
    if (stop_price && (!std::isfinite(*stop_price) || *stop_price <= 0.0))
        throw std::runtime_error("Invalid stop price");
    if (order_type == "limit" && !price)
        throw std::runtime_error("Limit order requires price");
    if ((order_type == "stop" || order_type == "stop_limit") && !stop_price)
        throw std::runtime_error("Stop order requires stop_price");

    auto portfolio = pt_get_portfolio(portfolio_id);

    // Exchange-hours enforcement — opt-in per portfolio (default off preserves
    // existing behavior). Only enforced when an exchange is supplied.
    if (pt_get_enforce_market_hours(portfolio_id) && !exchange.isEmpty() && !pt_is_market_open(exchange)) {
        reject_order(portfolio_id, symbol, side, order_type, quantity, price, stop_price, reduce_only, product,
                     exchange, ("Market closed for exchange " + exchange).toStdString());
    }

    // Margin blocking — skip for reduce_only orders and for the portion of an
    // order that closes/reduces an existing opposite position. Only the net new
    // exposure consumes margin.
    double margin_to_block = 0.0;
    if (!reduce_only) {
        QString opposite_side = (side == "buy") ? "short" : "long";
        auto opposite_pos = repo().find_position(portfolio_id, symbol, opposite_side);
        double net_new_qty = quantity;
        if (opposite_pos) {
            // Subtract the quantity that will close the existing position
            net_new_qty = std::max(0.0, quantity - opposite_pos->quantity);
        }

        if (net_new_qty > 0.0) {
            double ref = price.value_or(stop_price.value_or(0.0));
            if (ref <= 0.0)
                throw std::runtime_error("Market orders require a reference price for margin calculation");

            // Per-product / per-instrument leverage. If exchange/product are not
            // supplied, select_leverage falls back to the equity rules /
            // portfolio default, and for blank exchange the equity CNC path.
            margin_to_block =
                pt_calculate_required_margin(portfolio_id, symbol, exchange, product, net_new_qty, ref, side);

            if (margin_to_block > portfolio.balance) {
                reject_order(portfolio_id, symbol, side, order_type, quantity, price, stop_price, reduce_only, product,
                             exchange, "Insufficient margin");
            }
        }
    }

    PtOrder order;
    order.id = generate_uuid();
    order.portfolio_id = portfolio_id;
    order.symbol = symbol;
    order.side = side;
    order.order_type = order_type;
    order.quantity = quantity;
    order.price = price;
    order.stop_price = stop_price;
    order.filled_qty = 0.0;
    order.status = "pending";
    order.reduce_only = reduce_only;
    order.margin_blocked = margin_to_block;
    order.product = product;
    order.exchange = exchange;
    order.created_at = now_rfc3339();

    auto r = repo().insert_order(order);
    if (r.is_err())
        throw std::runtime_error(r.error());

    // Block the margin from available balance and record it against the order so
    // the exact amount can be released on fill/cancel.
    if (margin_to_block > 0.0) {
        repo().update_balance(portfolio_id, portfolio.balance - margin_to_block);
        repo().insert_margin_block(generate_uuid(), portfolio_id, order.id, symbol, margin_to_block);
    }

    return order;
}

void pt_cancel_order(const QString& order_id) {
    // Release any margin blocked when the order was placed back to available
    // balance (no realized P&L — the order never filled). Serialized under the
    // fill mutex so a concurrent fill can't double-release the same block.
    QMutexLocker lock(&s_fill_mutex);

    double blocked = repo().get_margin_block(order_id);
    if (blocked > 0.0) {
        auto ord = repo().get_order(order_id);
        if (ord.is_ok()) {
            auto portfolio = pt_get_portfolio(ord.value().portfolio_id);
            repo().update_balance(portfolio.id, portfolio.balance + blocked);
        }
        repo().delete_margin_block(order_id);
    }

    repo().cancel_order(order_id);
}

QVector<PtOrder> pt_get_orders(const QString& portfolio_id, const QString& status) {
    auto r = repo().get_orders(portfolio_id, status);
    if (r.is_err())
        return {};
    return r.value();
}

// ============================================================================
// Fill Order — The Core Engine
// ============================================================================

PtTrade pt_fill_order(const QString& order_id, double fill_price, std::optional<double> fill_qty) {
    if (!std::isfinite(fill_price) || fill_price <= 0.0)
        throw std::runtime_error("Invalid fill price");
    if (fill_qty && (!std::isfinite(*fill_qty) || *fill_qty <= 0.0))
        throw std::runtime_error("Invalid fill quantity");

    // Serialize fills to prevent concurrent corruption of balance/positions
    QMutexLocker fill_lock(&s_fill_mutex);

    // 1. Get order
    auto or1 = repo().get_order(order_id);
    if (or1.is_err())
        throw std::runtime_error(or1.error());
    PtOrder order = or1.value();

    if (order.status != "pending" && order.status != "partial")
        throw std::runtime_error("Order not fillable");

    // 2. Get portfolio
    PtPortfolio portfolio = pt_get_portfolio(order.portfolio_id);

    // 3. Fill qty and fee
    double qty = fill_qty.value_or(order.quantity - order.filled_qty);
    if (qty <= 0.0)
        throw std::runtime_error("Nothing left to fill");
    double fee = qty * fill_price * portfolio.fee_rate;
    QString now = now_rfc3339();

    QString position_side = (order.side == "buy") ? "long" : "short";
    QString opposite_side = (order.side == "buy") ? "short" : "long";
    double pnl = 0.0;

    // ── Begin DB transaction for atomicity ──────────────────────────────
    auto& db = Database::instance();
    db.begin_transaction();

    try {
        // 4. Check opposite-side position (closing)
        auto opp = repo().find_position(order.portfolio_id, order.symbol, opposite_side);
        bool had_opposite = false;

        if (opp) {
            had_opposite = true;
            PtPosition pos = *opp;
            double close_qty = std::min(qty, pos.quantity);
            pnl = (pos.side == "long") ? (fill_price - pos.entry_price) * close_qty
                                       : (pos.entry_price - fill_price) * close_qty;

            if (close_qty >= pos.quantity) {
                repo().delete_position(pos.id);
            } else {
                repo().update_position(pos.id, pos.quantity - close_qty, pos.entry_price);
                repo().add_realized_pnl(pos.id, pnl);
            }

            // Position flip: remaining qty becomes new opposite position
            if (qty > close_qty) {
                double new_qty = qty - close_qty;
                PtPosition np;
                np.id = generate_uuid();
                np.portfolio_id = order.portfolio_id;
                np.symbol = order.symbol;
                np.side = position_side;
                np.quantity = new_qty;
                np.entry_price = fill_price;
                np.current_price = fill_price;
                np.leverage = portfolio.leverage;
                np.opened_at = now;
                repo().insert_position(np);
            }
        }

        if (!had_opposite) {
            // 5. Same-side position (averaging)
            auto same = repo().find_position(order.portfolio_id, order.symbol, position_side);
            if (same) {
                PtPosition pos = *same;
                double new_qty = pos.quantity + qty;
                if (new_qty <= 0.0)
                    throw std::runtime_error("Invalid position quantity after averaging");
                double new_entry = (pos.entry_price * pos.quantity + fill_price * qty) / new_qty;
                repo().update_position(pos.id, new_qty, new_entry);
            } else {
                // 6. Brand new position
                PtPosition np;
                np.id = generate_uuid();
                np.portfolio_id = order.portfolio_id;
                np.symbol = order.symbol;
                np.side = position_side;
                np.quantity = qty;
                np.entry_price = fill_price;
                np.current_price = fill_price;
                np.leverage = portfolio.leverage;
                np.opened_at = now;
                repo().insert_position(np);
            }
        }

        // 7a. Release blocked margin (Phase 3 §4).
        // Margin was blocked from balance at placement for the net-new exposure.
        // Release it proportionally to the filled quantity on each (possibly
        // partial) fill. On the final fill, release whatever remains so rounding
        // never leaves margin stuck.
        double new_filled = order.filled_qty + qty;
        bool fully_filled = (new_filled >= order.quantity);
        double order_blocked = repo().get_margin_block(order_id);
        double margin_release = 0.0;
        if (order_blocked > 0.0) {
            if (fully_filled) {
                margin_release = order_blocked; // release all remaining
                repo().delete_margin_block(order_id);
            } else {
                double frac = (order.quantity > 0.0) ? (qty / order.quantity) : 1.0;
                margin_release = order_blocked * frac;
                double remaining = std::max(0.0, order_blocked - margin_release);
                repo().insert_margin_block(generate_uuid(), order.portfolio_id, order_id, order.symbol, remaining);
            }
        }

        // 7b. Update balance
        // Fee model: deduct fee only on closing fills (when realized PnL is produced).
        // Entry fees are NOT deducted from balance at open — they are included in the
        // exit fee when the position is closed. This matches how most exchanges present
        // unrealized P&L (gross of entry fee) and only settle fees on close.
        // Released margin is credited back to available balance alongside realized P&L.
        double balance_change = (had_opposite ? (pnl - fee) : pnl) + margin_release;
        repo().update_balance(order.portfolio_id, portfolio.balance + balance_change);

        // 8. Update order
        QString new_status = fully_filled ? "filled" : "partial";
        double prev_avg = order.avg_price.value_or(0.0);
        double new_avg =
            (new_filled > 0.0) ? (prev_avg * order.filled_qty + fill_price * qty) / new_filled : fill_price;
        repo().update_order_fill(order_id, new_filled, new_avg, new_status, now);

        // 9. Create trade record
        PtTrade trade;
        trade.id = generate_uuid();
        trade.portfolio_id = order.portfolio_id;
        trade.order_id = order_id;
        trade.symbol = order.symbol;
        trade.side = order.side;
        trade.price = fill_price;
        trade.quantity = qty;
        trade.fee = fee;
        trade.pnl = pnl;
        trade.timestamp = now;
        repo().insert_trade(trade);

        db.commit();

        // Emit event (outside transaction — non-critical)
        EventBus::instance().publish("paper_trading.order_filled", {{"trade_id", trade.id},
                                                                    {"portfolio_id", trade.portfolio_id},
                                                                    {"symbol", trade.symbol},
                                                                    {"side", trade.side},
                                                                    {"price", trade.price},
                                                                    {"quantity", trade.quantity},
                                                                    {"pnl", trade.pnl},
                                                                    {"fee", trade.fee}});

        return trade;
    } catch (...) {
        db.rollback();
        throw;
    }
}

// ============================================================================
// Position & Trade Queries
// ============================================================================

QVector<PtPosition> pt_get_positions(const QString& portfolio_id) {
    auto r = repo().get_positions(portfolio_id);
    if (r.is_err())
        return {};
    return r.value();
}

void pt_update_position_price(const QString& portfolio_id, const QString& symbol, double price) {
    repo().update_position_price(portfolio_id, symbol, price);
}

QVector<PtTrade> pt_get_trades(const QString& portfolio_id, int64_t limit) {
    auto r = repo().get_trades(portfolio_id, limit);
    if (r.is_err())
        return {};
    return r.value();
}

PtStats pt_get_stats(const QString& portfolio_id) {
    auto r = repo().get_stats(portfolio_id);
    if (r.is_err())
        return {};
    return r.value();
}

} // namespace fincept::trading
