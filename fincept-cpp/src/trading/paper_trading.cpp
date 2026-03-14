// Paper Trading Engine — Direct 1:1 port of Rust paper_trading.rs
// Transactional order fill with position management, margin, fees

#include "paper_trading.h"
#include "storage/database.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace fincept::trading {

// ============================================================================
// Helpers
// ============================================================================

static std::string now_rfc3339() {
    // Use database utility for timestamp, format as ISO string
    auto t = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(t);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

static PtPortfolio read_portfolio_row(db::Statement& stmt) {
    return PtPortfolio{
        stmt.col_text(0), stmt.col_text(1),
        stmt.col_double(2), stmt.col_double(3), stmt.col_text(4),
        stmt.col_double(5), stmt.col_text(6), stmt.col_double(7),
        stmt.col_text(8)
    };
}

static PtOrder read_order_row(db::Statement& stmt) {
    PtOrder o;
    o.id = stmt.col_text(0);
    o.portfolio_id = stmt.col_text(1);
    o.symbol = stmt.col_text(2);
    o.side = stmt.col_text(3);
    o.order_type = stmt.col_text(4);
    o.quantity = stmt.col_double(5);
    auto price_opt = stmt.col_text_opt(6);
    o.price = price_opt ? std::optional<double>(std::stod(*price_opt)) : std::nullopt;
    auto stop_opt = stmt.col_text_opt(7);
    o.stop_price = stop_opt ? std::optional<double>(std::stod(*stop_opt)) : std::nullopt;
    o.filled_qty = stmt.col_double(8);
    auto avg_opt = stmt.col_text_opt(9);
    o.avg_price = avg_opt ? std::optional<double>(std::stod(*avg_opt)) : std::nullopt;
    o.status = stmt.col_text(10);
    o.reduce_only = stmt.col_bool(11);
    o.created_at = stmt.col_text(12);
    o.filled_at = stmt.col_text_opt(13);
    return o;
}

static PtPosition read_position_row(db::Statement& stmt) {
    PtPosition p;
    p.id = stmt.col_text(0);
    p.portfolio_id = stmt.col_text(1);
    p.symbol = stmt.col_text(2);
    p.side = stmt.col_text(3);
    p.quantity = stmt.col_double(4);
    p.entry_price = stmt.col_double(5);
    p.current_price = stmt.col_double(6);
    p.unrealized_pnl = stmt.col_double(7);
    p.realized_pnl = stmt.col_double(8);
    p.leverage = stmt.col_double(9);
    auto liq_opt = stmt.col_text_opt(10);
    p.liquidation_price = liq_opt ? std::optional<double>(std::stod(*liq_opt)) : std::nullopt;
    p.opened_at = stmt.col_text(11);
    return p;
}

static PtTrade read_trade_row(db::Statement& stmt) {
    return PtTrade{
        stmt.col_text(0), stmt.col_text(1), stmt.col_text(2),
        stmt.col_text(3), stmt.col_text(4),
        stmt.col_double(5), stmt.col_double(6), stmt.col_double(7),
        stmt.col_double(8), stmt.col_text(9)
    };
}

// ============================================================================
// Portfolio Operations
// ============================================================================

PtPortfolio pt_create_portfolio(const std::string& name, double balance,
                                 const std::string& currency, double leverage,
                                 const std::string& margin_mode, double fee_rate) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    std::string id = db::generate_uuid();
    std::string now = now_rfc3339();

    auto stmt = db.prepare(
        "INSERT INTO pt_portfolios (id, name, initial_balance, balance, currency, leverage, "
        "margin_mode, fee_rate, created_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, name);
    stmt.bind_double(3, balance);
    stmt.bind_double(4, balance);
    stmt.bind_text(5, currency);
    stmt.bind_double(6, leverage);
    stmt.bind_text(7, margin_mode);
    stmt.bind_double(8, fee_rate);
    stmt.bind_text(9, now);
    stmt.execute();

    return PtPortfolio{id, name, balance, balance, currency, leverage, margin_mode, fee_rate, now};
}

PtPortfolio pt_get_portfolio(const std::string& id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, "
        "created_at FROM pt_portfolios WHERE id = ?1");
    stmt.bind_text(1, id);
    if (!stmt.step()) throw std::runtime_error("Portfolio not found: " + id);
    return read_portfolio_row(stmt);
}

std::vector<PtPortfolio> pt_list_portfolios() {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, "
        "created_at FROM pt_portfolios");
    std::vector<PtPortfolio> result;
    while (stmt.step()) result.push_back(read_portfolio_row(stmt));
    return result;
}

void pt_delete_portfolio(const std::string& id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    db.transaction([&]() {
        auto s1 = db.prepare("DELETE FROM pt_trades WHERE portfolio_id = ?1");
        s1.bind_text(1, id); s1.execute();
        auto s2 = db.prepare("DELETE FROM pt_orders WHERE portfolio_id = ?1");
        s2.bind_text(1, id); s2.execute();
        auto s3 = db.prepare("DELETE FROM pt_positions WHERE portfolio_id = ?1");
        s3.bind_text(1, id); s3.execute();
        auto s4 = db.prepare("DELETE FROM pt_portfolios WHERE id = ?1");
        s4.bind_text(1, id); s4.execute();
        return 0;
    });
}

PtPortfolio pt_reset_portfolio(const std::string& id) {
    // Verify exists
    pt_get_portfolio(id);

    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    db.transaction([&]() {
        auto s1 = db.prepare("DELETE FROM pt_trades WHERE portfolio_id = ?1");
        s1.bind_text(1, id); s1.execute();
        auto s2 = db.prepare("DELETE FROM pt_orders WHERE portfolio_id = ?1");
        s2.bind_text(1, id); s2.execute();
        auto s3 = db.prepare("DELETE FROM pt_positions WHERE portfolio_id = ?1");
        s3.bind_text(1, id); s3.execute();
        auto s4 = db.prepare("UPDATE pt_portfolios SET balance = initial_balance WHERE id = ?1");
        s4.bind_text(1, id); s4.execute();
        return 0;
    });

    return pt_get_portfolio(id);
}

// ============================================================================
// Order Operations
// ============================================================================

PtOrder pt_place_order(const std::string& portfolio_id, const std::string& symbol,
                        const std::string& side, const std::string& order_type,
                        double quantity, std::optional<double> price,
                        std::optional<double> stop_price, bool reduce_only) {
    // Validate order_type
    if (order_type != "market" && order_type != "limit" &&
        order_type != "stop" && order_type != "stop_limit") {
        throw std::runtime_error("Invalid order type: " + order_type);
    }

    // Validate side
    if (side != "buy" && side != "sell") {
        throw std::runtime_error("Invalid side: " + side);
    }

    // Validate quantity
    if (!std::isfinite(quantity) || quantity <= 0.0) {
        throw std::runtime_error("Invalid quantity");
    }

    // Validate price fields
    if (price && (!std::isfinite(*price) || *price <= 0.0)) {
        throw std::runtime_error("Invalid price");
    }
    if (stop_price && (!std::isfinite(*stop_price) || *stop_price <= 0.0)) {
        throw std::runtime_error("Invalid stop price");
    }

    if (order_type == "limit" && !price) {
        throw std::runtime_error("Limit order requires price");
    }
    if ((order_type == "stop" || order_type == "stop_limit") && !stop_price) {
        throw std::runtime_error("Stop order requires stop_price");
    }

    // Check margin for new positions
    auto portfolio = pt_get_portfolio(portfolio_id);
    if (!reduce_only) {
        double reference_price = price.value_or(stop_price.value_or(0.0));
        if (reference_price <= 0.0) {
            throw std::runtime_error("Market orders require a reference price for margin calculation");
        }
        double required_margin = quantity * reference_price / portfolio.leverage;
        if (required_margin > portfolio.balance) {
            throw std::runtime_error("Insufficient margin");
        }
    }

    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    std::string id = db::generate_uuid();
    std::string now = now_rfc3339();

    auto stmt = db.prepare(
        "INSERT INTO pt_orders (id, portfolio_id, symbol, side, order_type, quantity, "
        "price, stop_price, reduce_only, created_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, portfolio_id);
    stmt.bind_text(3, symbol);
    stmt.bind_text(4, side);
    stmt.bind_text(5, order_type);
    stmt.bind_double(6, quantity);
    if (price) stmt.bind_double(7, *price); else stmt.bind_null(7);
    if (stop_price) stmt.bind_double(8, *stop_price); else stmt.bind_null(8);
    stmt.bind_int(9, reduce_only ? 1 : 0);
    stmt.bind_text(10, now);
    stmt.execute();

    PtOrder order;
    order.id = id;
    order.portfolio_id = portfolio_id;
    order.symbol = symbol;
    order.side = side;
    order.order_type = order_type;
    order.quantity = quantity;
    order.price = price;
    order.stop_price = stop_price;
    order.filled_qty = 0.0;
    order.avg_price = std::nullopt;
    order.status = "pending";
    order.reduce_only = reduce_only;
    order.created_at = now;
    order.filled_at = std::nullopt;
    return order;
}

void pt_cancel_order(const std::string& order_id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "UPDATE pt_orders SET status = 'cancelled' WHERE id = ?1 AND status = 'pending'");
    stmt.bind_text(1, order_id);
    stmt.execute();
}

std::vector<PtOrder> pt_get_orders(const std::string& portfolio_id,
                                    const std::string& status) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    std::string sql =
        "SELECT id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, "
        "filled_qty, avg_price, status, reduce_only, created_at, filled_at "
        "FROM pt_orders WHERE portfolio_id = ?1";
    if (!status.empty()) sql += " AND status = ?2";

    auto stmt = db.prepare(sql.c_str());
    stmt.bind_text(1, portfolio_id);
    if (!status.empty()) stmt.bind_text(2, status);

    std::vector<PtOrder> result;
    while (stmt.step()) result.push_back(read_order_row(stmt));
    return result;
}

// ============================================================================
// Fill Order — The Core Engine
// ============================================================================

PtTrade pt_fill_order(const std::string& order_id, double fill_price,
                       std::optional<double> fill_qty) {
    if (!std::isfinite(fill_price) || fill_price <= 0.0) {
        throw std::runtime_error("Invalid fill price");
    }
    if (fill_qty && (!std::isfinite(*fill_qty) || *fill_qty <= 0.0)) {
        throw std::runtime_error("Invalid fill quantity");
    }

    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    return db.transaction([&]() -> PtTrade {
        // 1. Get order
        auto order_stmt = db.prepare(
            "SELECT id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, "
            "filled_qty, avg_price, status, reduce_only, created_at, filled_at "
            "FROM pt_orders WHERE id = ?1");
        order_stmt.bind_text(1, order_id);
        if (!order_stmt.step()) throw std::runtime_error("Order not found: " + order_id);
        PtOrder order = read_order_row(order_stmt);

        if (order.status != "pending" && order.status != "partial") {
            throw std::runtime_error("Order not fillable");
        }

        // 2. Get portfolio
        auto pf_stmt = db.prepare(
            "SELECT id, name, initial_balance, balance, currency, leverage, margin_mode, "
            "fee_rate, created_at FROM pt_portfolios WHERE id = ?1");
        pf_stmt.bind_text(1, order.portfolio_id);
        if (!pf_stmt.step()) throw std::runtime_error("Portfolio not found");
        PtPortfolio portfolio = read_portfolio_row(pf_stmt);

        // 3. Determine fill qty and fee
        double qty = fill_qty.value_or(order.quantity - order.filled_qty);
        double fee = qty * fill_price * portfolio.fee_rate;
        std::string now = now_rfc3339();

        std::string position_side = (order.side == "buy") ? "long" : "short";
        std::string opposite_side = (order.side == "buy") ? "short" : "long";
        double pnl = 0.0;

        // 4. Check for opposite-side position (closing)
        auto opp_stmt = db.prepare(
            "SELECT id, portfolio_id, symbol, side, quantity, entry_price, current_price, "
            "unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at "
            "FROM pt_positions WHERE portfolio_id = ?1 AND symbol = ?2 AND side = ?3");
        opp_stmt.bind_text(1, order.portfolio_id);
        opp_stmt.bind_text(2, order.symbol);
        opp_stmt.bind_text(3, opposite_side);

        bool had_opposite = false;
        if (opp_stmt.step()) {
            had_opposite = true;
            PtPosition pos = read_position_row(opp_stmt);

            double close_qty = std::min(qty, pos.quantity);
            pnl = (pos.side == "long")
                ? (fill_price - pos.entry_price) * close_qty
                : (pos.entry_price - fill_price) * close_qty;

            if (close_qty >= pos.quantity) {
                // Fully close
                auto del = db.prepare("DELETE FROM pt_positions WHERE id = ?1");
                del.bind_text(1, pos.id);
                del.execute();
            } else {
                // Partially close
                auto upd = db.prepare(
                    "UPDATE pt_positions SET quantity = quantity - ?1, "
                    "realized_pnl = realized_pnl + ?2 WHERE id = ?3");
                upd.bind_double(1, close_qty);
                upd.bind_double(2, pnl);
                upd.bind_text(3, pos.id);
                upd.execute();
            }

            // Position flip: open remaining qty in new direction
            if (qty > close_qty) {
                double new_qty = qty - close_qty;
                std::string pos_id = db::generate_uuid();
                auto ins = db.prepare(
                    "INSERT INTO pt_positions (id, portfolio_id, symbol, side, quantity, "
                    "entry_price, current_price, leverage, opened_at) "
                    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");
                ins.bind_text(1, pos_id);
                ins.bind_text(2, order.portfolio_id);
                ins.bind_text(3, order.symbol);
                ins.bind_text(4, position_side);
                ins.bind_double(5, new_qty);
                ins.bind_double(6, fill_price);
                ins.bind_double(7, fill_price);
                ins.bind_double(8, portfolio.leverage);
                ins.bind_text(9, now);
                ins.execute();
            }
        }

        if (!had_opposite) {
            // 5. Check for same-side position (averaging)
            auto same_stmt = db.prepare(
                "SELECT id, portfolio_id, symbol, side, quantity, entry_price, current_price, "
                "unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at "
                "FROM pt_positions WHERE portfolio_id = ?1 AND symbol = ?2 AND side = ?3");
            same_stmt.bind_text(1, order.portfolio_id);
            same_stmt.bind_text(2, order.symbol);
            same_stmt.bind_text(3, position_side);

            if (same_stmt.step()) {
                PtPosition pos = read_position_row(same_stmt);
                double new_qty = pos.quantity + qty;
                double new_entry = (pos.entry_price * pos.quantity + fill_price * qty) / new_qty;
                auto upd = db.prepare(
                    "UPDATE pt_positions SET quantity = ?1, entry_price = ?2, "
                    "current_price = ?3 WHERE id = ?4");
                upd.bind_double(1, new_qty);
                upd.bind_double(2, new_entry);
                upd.bind_double(3, fill_price);
                upd.bind_text(4, pos.id);
                upd.execute();
            } else {
                // 6. Brand new position
                std::string pos_id = db::generate_uuid();
                auto ins = db.prepare(
                    "INSERT INTO pt_positions (id, portfolio_id, symbol, side, quantity, "
                    "entry_price, current_price, leverage, opened_at) "
                    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");
                ins.bind_text(1, pos_id);
                ins.bind_text(2, order.portfolio_id);
                ins.bind_text(3, order.symbol);
                ins.bind_text(4, position_side);
                ins.bind_double(5, qty);
                ins.bind_double(6, fill_price);
                ins.bind_double(7, fill_price);
                ins.bind_double(8, portfolio.leverage);
                ins.bind_text(9, now);
                ins.execute();
            }
        }

        // 7. Update balance: realized P&L - fee
        double balance_change = pnl - fee;
        {
            auto bal = db.prepare(
                "UPDATE pt_portfolios SET balance = balance + ?1 WHERE id = ?2");
            bal.bind_double(1, balance_change);
            bal.bind_text(2, order.portfolio_id);
            bal.execute();
        }

        // 8. Update order
        double new_filled = order.filled_qty + qty;
        std::string new_status = (new_filled >= order.quantity) ? "filled" : "partial";
        double prev_avg = order.avg_price.value_or(0.0);
        double new_avg = (prev_avg * order.filled_qty + fill_price * qty) / new_filled;
        {
            auto upd = db.prepare(
                "UPDATE pt_orders SET filled_qty = ?1, avg_price = ?2, status = ?3, "
                "filled_at = ?4 WHERE id = ?5");
            upd.bind_double(1, new_filled);
            upd.bind_double(2, new_avg);
            upd.bind_text(3, new_status);
            upd.bind_text(4, now);
            upd.bind_text(5, order_id);
            upd.execute();
        }

        // 9. Create trade record
        std::string trade_id = db::generate_uuid();
        {
            auto ins = db.prepare(
                "INSERT INTO pt_trades (id, portfolio_id, order_id, symbol, side, price, "
                "quantity, fee, pnl, timestamp) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
            ins.bind_text(1, trade_id);
            ins.bind_text(2, order.portfolio_id);
            ins.bind_text(3, order_id);
            ins.bind_text(4, order.symbol);
            ins.bind_text(5, order.side);
            ins.bind_double(6, fill_price);
            ins.bind_double(7, qty);
            ins.bind_double(8, fee);
            ins.bind_double(9, pnl);
            ins.bind_text(10, now);
            ins.execute();
        }

        PtTrade trade{trade_id, order.portfolio_id, order_id, order.symbol,
                       order.side, fill_price, qty, fee, pnl, now};

        // Emit EventBus event after transaction commits
        core::EventBus::instance().publish("paper_trading.order_filled", {
            {"trade_id", trade.id},
            {"portfolio_id", trade.portfolio_id},
            {"symbol", trade.symbol},
            {"side", trade.side},
            {"price", trade.price},
            {"quantity", trade.quantity},
            {"pnl", trade.pnl},
            {"fee", trade.fee}
        });

        return trade;
    });
}

// ============================================================================
// Position & Trade Queries
// ============================================================================

std::vector<PtPosition> pt_get_positions(const std::string& portfolio_id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, portfolio_id, symbol, side, quantity, entry_price, current_price, "
        "unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at "
        "FROM pt_positions WHERE portfolio_id = ?1");
    stmt.bind_text(1, portfolio_id);

    std::vector<PtPosition> result;
    while (stmt.step()) result.push_back(read_position_row(stmt));
    return result;
}

void pt_update_position_price(const std::string& portfolio_id,
                               const std::string& symbol, double price) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "UPDATE pt_positions SET current_price = ?1, "
        "unrealized_pnl = CASE WHEN side = 'long' THEN (?1 - entry_price) * quantity "
        "ELSE (entry_price - ?1) * quantity END "
        "WHERE portfolio_id = ?2 AND symbol = ?3");
    stmt.bind_double(1, price);
    stmt.bind_text(2, portfolio_id);
    stmt.bind_text(3, symbol);
    stmt.execute();
}

std::vector<PtTrade> pt_get_trades(const std::string& portfolio_id, int64_t limit) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp "
        "FROM pt_trades WHERE portfolio_id = ?1 ORDER BY timestamp DESC LIMIT ?2");
    stmt.bind_text(1, portfolio_id);
    stmt.bind_int(2, limit);

    std::vector<PtTrade> result;
    while (stmt.step()) result.push_back(read_trade_row(stmt));
    return result;
}

PtStats pt_get_stats(const std::string& portfolio_id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT COALESCE(SUM(pnl), 0), COUNT(*), "
        "COALESCE(SUM(CASE WHEN pnl > 0 THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN pnl < 0 THEN 1 ELSE 0 END), 0), "
        "COALESCE(MAX(pnl), 0), COALESCE(MIN(pnl), 0) "
        "FROM pt_trades WHERE portfolio_id = ?1");
    stmt.bind_text(1, portfolio_id);

    PtStats stats;
    if (stmt.step()) {
        stats.total_pnl = stmt.col_double(0);
        stats.total_trades = stmt.col_int(1);
        stats.winning_trades = stmt.col_int(2);
        stats.losing_trades = stmt.col_int(3);
        stats.largest_win = stmt.col_double(4);
        stats.largest_loss = stmt.col_double(5);
        stats.win_rate = (stats.total_trades > 0)
            ? (static_cast<double>(stats.winning_trades) / static_cast<double>(stats.total_trades)) * 100.0
            : 0.0;
    }
    return stats;
}

} // namespace fincept::trading
