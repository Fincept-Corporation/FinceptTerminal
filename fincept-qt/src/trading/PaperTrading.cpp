// Paper Trading Engine — Business logic layer
// Delegates all DB access to PaperTradingRepository.
// Contains: validation, margin checks, fill engine, position management.

#include "trading/PaperTrading.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/PaperTradingRepository.h"
#include "storage/sqlite/Database.h"

#include <QDate>
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

void pt_set_balance(const QString& portfolio_id, double new_balance) {
    if (!std::isfinite(new_balance) || new_balance < 0.0)
        throw std::runtime_error("Invalid balance: must be finite and non-negative");
    auto r = repo().update_balance(portfolio_id, new_balance);
    if (r.is_err())
        throw std::runtime_error(r.error());
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

PtTrade pt_fill_order(const QString& order_id, double fill_price, std::optional<double> fill_qty,
                      const QString& fill_time) {
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
    QString now = fill_time.isEmpty() ? now_rfc3339() : fill_time;

    QString position_side = (order.side == "buy") ? "long" : "short";
    QString opposite_side = (order.side == "buy") ? "short" : "long";
    double pnl = 0.0;
    double new_filled = order.filled_qty + qty;
    bool fully_filled = (new_filled >= order.quantity);

    // ── Begin DB transaction for atomicity ──────────────────────────────
    auto& db = Database::instance();
    db.begin_transaction();

    try {
        // Net change to available cash from this fill. Margin model (v040):
        //   • Opening exposure KEEPS its margin locked on the position (held_margin)
        //     — it is NOT released to cash on fill. This is what finally makes
        //     available balance drop when a position is opened (the old code released
        //     the block on fill, so an open position locked no cash — "stuck at 10L").
        //   • Closing/reducing a position RELEASES that position's held margin back
        //     to cash, proportionally, plus realized P&L.
        //   • Brokerage (fee) is charged on every fill.
        double balance_delta = -fee;
        const double order_blocked = repo().get_margin_block(order_id);
        double block_consumed = 0.0; // portion of the order's block turned into position margin

        // ── Closing leg: net against an opposite-side position ──
        double close_qty = 0.0;
        auto opp = repo().find_position(order.portfolio_id, order.symbol, opposite_side);
        if (opp) {
            PtPosition pos = *opp;
            close_qty = std::min(qty, pos.quantity);
            pnl = (pos.side == "long") ? (fill_price - pos.entry_price) * close_qty
                                       : (pos.entry_price - fill_price) * close_qty;
            const double frac_close = pos.quantity > 0.0 ? (close_qty / pos.quantity) : 1.0;
            const double margin_released = pos.held_margin * frac_close;
            if (close_qty >= pos.quantity) {
                repo().delete_position(pos.id);
            } else {
                repo().update_position(pos.id, pos.quantity - close_qty, pos.entry_price);
                repo().set_position_margin(pos.id, pos.held_margin - margin_released);
                repo().add_realized_pnl(pos.id, pnl);
            }
            balance_delta += margin_released + pnl;
        }

        // ── Opening leg: net-new exposure on the order's own side ──
        const double open_qty = qty - close_qty;
        if (open_qty > 0.0) {
            // Margin the new exposure must lock, per product / leverage at fill price.
            double open_margin = pt_calculate_required_margin(order.portfolio_id, order.symbol, order.exchange,
                                                              order.product, open_qty, fill_price, order.side);
            // That margin was pre-blocked on the order at placement: convert the
            // block into position margin (release the block, re-lock the actual).
            block_consumed = fully_filled ? order_blocked : std::min(order_blocked, open_margin);
            balance_delta += block_consumed - open_margin;

            const QString product = order.product.isEmpty() ? QStringLiteral("MIS") : order.product;
            auto same = repo().find_position(order.portfolio_id, order.symbol, position_side);
            if (same) {
                PtPosition pos = *same;
                double new_qty = pos.quantity + open_qty;
                if (new_qty <= 0.0)
                    throw std::runtime_error("Invalid position quantity after averaging");
                double new_entry = (pos.entry_price * pos.quantity + fill_price * open_qty) / new_qty;
                repo().update_position(pos.id, new_qty, new_entry);
                repo().set_position_margin(pos.id, pos.held_margin + open_margin);
            } else {
                PtPosition np;
                np.id = generate_uuid();
                np.portfolio_id = order.portfolio_id;
                np.symbol = order.symbol;
                np.side = position_side;
                np.quantity = open_qty;
                np.entry_price = fill_price;
                np.current_price = fill_price;
                np.leverage = open_margin > 0.0 ? (open_qty * fill_price) / open_margin : portfolio.leverage;
                np.opened_at = now;
                np.product = product;
                np.held_margin = open_margin;
                repo().insert_position(np);
            }
        }

        // ── Settle the order's margin block ──
        if (order_blocked > 0.0) {
            if (fully_filled) {
                repo().delete_margin_block(order_id);
            } else {
                double remaining = std::max(0.0, order_blocked - block_consumed);
                repo().insert_margin_block(generate_uuid(), order.portfolio_id, order_id, order.symbol, remaining);
            }
        }

        repo().update_balance(order.portfolio_id, portfolio.balance + balance_delta);

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

// ============================================================================
// Day-scoped queries / Settlement / Product conversion (v040)
// ============================================================================

namespace {

// IST = UTC + 5:30. The UTC instant at which the given IST calendar day begins.
QDateTime ist_day_start_utc(const QDate& ist_day) {
    QDateTime utc_midnight(ist_day, QTime(0, 0, 0), QTimeZone::UTC);
    return utc_midnight.addSecs(-330 * 60); // shift IST-midnight back to its UTC instant
}

// The IST calendar date of a stored (naive/Z UTC) ISO timestamp.
QDate ist_date_of(const QString& iso_utc, const QDate& fallback) {
    QDateTime dt = QDateTime::fromString(iso_utc, Qt::ISODate);
    if (!dt.isValid())
        return fallback;
    dt.setTimeZone(QTimeZone::UTC); // timestamps are UTC; reinterpret if no zone was parsed
    return dt.addSecs(330 * 60).date();
}

} // namespace

QVector<PtOrder> pt_get_orders_for_day(const QString& portfolio_id, const QDate& ist_day) {
    const QDateTime start = ist_day_start_utc(ist_day);
    auto r = repo().get_orders_between(portfolio_id, start.toString(Qt::ISODate),
                                       start.addDays(1).toString(Qt::ISODate));
    return r.is_ok() ? r.value() : QVector<PtOrder>{};
}

QVector<PtTrade> pt_get_trades_for_day(const QString& portfolio_id, const QDate& ist_day) {
    const QDateTime start = ist_day_start_utc(ist_day);
    auto r = repo().get_trades_between(portfolio_id, start.toString(Qt::ISODate),
                                       start.addDays(1).toString(Qt::ISODate));
    return r.is_ok() ? r.value() : QVector<PtTrade>{};
}

void pt_convert_position_product(const QString& position_id, const QString& new_product) {
    QMutexLocker lock(&s_fill_mutex);

    auto pr = repo().get_position(position_id);
    if (pr.is_err())
        throw std::runtime_error(pr.error());
    PtPosition pos = pr.value();
    if (pos.product.compare(new_product, Qt::CaseInsensitive) == 0)
        return; // already that product — no-op

    auto portfolio = pt_get_portfolio(pos.portfolio_id);

    // Margin the position would lock under the NEW product, at its entry basis.
    // (CNC/delivery = full notional at 1x; MIS = notional/leverage.)
    const QString side = (pos.side == "long") ? "buy" : "sell";
    const double new_margin = pt_calculate_required_margin(pos.portfolio_id, pos.symbol, QString(), new_product,
                                                           pos.quantity, pos.entry_price, side);
    const double extra = new_margin - pos.held_margin; // additional cash to lock (CNC needs more than MIS)
    if (extra > portfolio.balance + 1e-6)
        throw std::runtime_error(("Insufficient balance to convert to " + new_product).toStdString());

    auto& db = Database::instance();
    db.begin_transaction();
    try {
        repo().update_balance(pos.portfolio_id, portfolio.balance - extra);
        repo().set_position_margin(position_id, new_margin);
        repo().set_position_product(position_id, new_product);
        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}

int pt_settle_intraday(const QString& portfolio_id) {
    // Intraday auto-square applies ONLY to Indian (INR) equity portfolios. Crypto
    // and US (USD) paper portfolios trade on other schedules and must never be
    // squared at the IST 15:30 cutoff (this previously closed month-old crypto
    // positions because their product had defaulted to MIS).
    PtPortfolio portfolio;
    try {
        portfolio = pt_get_portfolio(portfolio_id);
    } catch (...) {
        return 0;
    }
    if (portfolio.currency.compare("INR", Qt::CaseInsensitive) != 0)
        return 0;

    const QDateTime ist_now = QDateTime::currentDateTimeUtc().addSecs(330 * 60);
    const QDate ist_today = ist_now.date();
    const bool past_close = ist_now.time() >= QTime(15, 30);

    int squared = 0;
    const auto positions = pt_get_positions(portfolio_id);
    for (const auto& pos : positions) {
        if (!product_is_intraday(pos.product))
            continue; // CNC / NRML carry forward
        const QDate opened_ist = ist_date_of(pos.opened_at, ist_today);
        const bool from_prior_day = opened_ist < ist_today;
        if (!past_close && !from_prior_day)
            continue; // still within today's session and not yet 15:30

        const double price = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
        if (price <= 0.0)
            continue; // can't price the square-off

        // Stamp the square-off at the session close it actually belongs to: 15:30
        // IST on the day the position was opened. For a same-day position past the
        // cutoff that's today's 15:30; for a carried-over position it's that prior
        // day's close — so a catch-up auto-square lands in THAT day's book, never
        // polluting today's order list with trades the user didn't place.
        const QDateTime close_utc = QDateTime(opened_ist, QTime(15, 30, 0), QTimeZone::UTC).addSecs(-330 * 60);
        const QString close_iso = close_utc.toString(Qt::ISODate);

        // Square off = insert a reduce-only market order on the opposite side and
        // fill it at the last price. Reuses the fill engine's closing leg (margin
        // release + realized P&L + trade record), and leaves an auto-square order
        // in the book just like a real broker's 3:30 cutoff.
        try {
            PtOrder o;
            o.id = generate_uuid();
            o.portfolio_id = portfolio_id;
            o.symbol = pos.symbol;
            o.side = (pos.side == "long") ? QStringLiteral("sell") : QStringLiteral("buy");
            o.order_type = "market";
            o.quantity = pos.quantity;
            o.price = price;
            o.filled_qty = 0.0;
            o.status = "pending";
            o.reduce_only = true;
            o.margin_blocked = 0.0;
            o.product = pos.product;
            o.created_at = close_iso;
            auto ir = repo().insert_order(o);
            if (ir.is_err())
                continue;
            pt_fill_order(o.id, price, std::nullopt, close_iso);
            ++squared;
        } catch (const std::exception& e) {
            LOG_WARN("PaperTrading", QString("Auto-square failed for %1: %2").arg(pos.symbol, e.what()));
        }
    }

    // Stale DAY orders from earlier IST sessions expire at the close — cancel them
    // so they don't linger as pending or fire on a later day.
    const auto pending = pt_get_orders(portfolio_id, "pending");
    for (const auto& o : pending) {
        if (ist_date_of(o.created_at, ist_today) < ist_today) {
            try {
                pt_cancel_order(o.id);
            } catch (...) {
            }
        }
    }
    return squared;
}

int pt_settle_intraday_all() {
    int total = 0;
    for (const auto& p : pt_list_portfolios())
        total += pt_settle_intraday(p.id);
    return total;
}

} // namespace fincept::trading
