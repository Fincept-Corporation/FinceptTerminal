// Paper Trading Engine — Business logic layer
// Delegates all DB access to PaperTradingRepository.
// Contains: validation, margin checks, fill engine, position management.

#include "trading/PaperTrading.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/PaperTradingRepository.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
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
    // Reset balance to initial
    auto p = pt_get_portfolio(id);
    repo().update_balance(id, p.initial_balance);
    return pt_get_portfolio(id);
}

// ============================================================================
// Order Operations
// ============================================================================

PtOrder pt_place_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                       const QString& order_type, double quantity, std::optional<double> price,
                       std::optional<double> stop_price, bool reduce_only) {
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

    // Margin check — skip for reduce_only orders and orders that close/reduce existing positions
    auto portfolio = pt_get_portfolio(portfolio_id);
    if (!reduce_only) {
        // Determine if this order closes an existing opposite position
        QString opposite_side = (side == "buy") ? "short" : "long";
        auto opposite_pos = repo().find_position(portfolio_id, symbol, opposite_side);
        double net_new_qty = quantity;
        if (opposite_pos) {
            // Subtract the quantity that will close the existing position
            net_new_qty = std::max(0.0, quantity - opposite_pos->quantity);
        }

        // Only check margin for the net new exposure
        if (net_new_qty > 0.0) {
            double ref = price.value_or(stop_price.value_or(0.0));
            if (ref <= 0.0)
                throw std::runtime_error("Market orders require a reference price for margin calculation");
            double required = net_new_qty * ref / portfolio.leverage;
            if (required > portfolio.balance)
                throw std::runtime_error("Insufficient margin");
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
    order.created_at = now_rfc3339();

    auto r = repo().insert_order(order);
    if (r.is_err())
        throw std::runtime_error(r.error());
    return order;
}

void pt_cancel_order(const QString& order_id) {
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

        // 7. Update balance
        double balance_change = pnl - fee;
        repo().update_balance(order.portfolio_id, portfolio.balance + balance_change);

        // 8. Update order
        double new_filled = order.filled_qty + qty;
        QString new_status = (new_filled >= order.quantity) ? "filled" : "partial";
        double prev_avg = order.avg_price.value_or(0.0);
        double new_avg = (new_filled > 0.0) ? (prev_avg * order.filled_qty + fill_price * qty) / new_filled
                                             : fill_price;
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
        EventBus::instance().publish("paper_trading.order_filled",
                                     {{"trade_id", trade.id},
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
