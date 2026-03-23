#include "storage/repositories/PaperTradingRepository.h"

namespace fincept {

using namespace trading;

PaperTradingRepository& PaperTradingRepository::instance() {
    static PaperTradingRepository s;
    return s;
}

// ── Row Mappers (ported from PaperTrading.cpp read_*_row functions) ──────────

PtPortfolio PaperTradingRepository::map_portfolio(QSqlQuery& q) {
    PtPortfolio p;
    p.id = q.value(0).toString();
    p.name = q.value(1).toString();
    p.initial_balance = q.value(2).toDouble();
    p.balance = q.value(3).toDouble();
    p.currency = q.value(4).toString();
    p.leverage = q.value(5).toDouble();
    p.margin_mode = q.value(6).toString();
    p.fee_rate = q.value(7).toDouble();
    p.exchange = q.value(8).toString();
    p.created_at = q.value(9).toString();
    return p;
}

PtOrder PaperTradingRepository::map_order(QSqlQuery& q) {
    PtOrder o;
    o.id = q.value(0).toString();
    o.portfolio_id = q.value(1).toString();
    o.symbol = q.value(2).toString();
    o.side = q.value(3).toString();
    o.order_type = q.value(4).toString();
    o.quantity = q.value(5).toDouble();
    o.price = q.value(6).isNull() ? std::nullopt : std::optional<double>(q.value(6).toDouble());
    o.stop_price = q.value(7).isNull() ? std::nullopt : std::optional<double>(q.value(7).toDouble());
    o.filled_qty = q.value(8).toDouble();
    o.avg_price = q.value(9).isNull() ? std::nullopt : std::optional<double>(q.value(9).toDouble());
    o.status = q.value(10).toString();
    o.reduce_only = q.value(11).toBool();
    o.created_at = q.value(12).toString();
    o.filled_at = q.value(13).isNull() ? std::nullopt : std::optional<QString>(q.value(13).toString());
    return o;
}

PtPosition PaperTradingRepository::map_position(QSqlQuery& q) {
    PtPosition p;
    p.id = q.value(0).toString();
    p.portfolio_id = q.value(1).toString();
    p.symbol = q.value(2).toString();
    p.side = q.value(3).toString();
    p.quantity = q.value(4).toDouble();
    p.entry_price = q.value(5).toDouble();
    p.current_price = q.value(6).toDouble();
    p.unrealized_pnl = q.value(7).toDouble();
    p.realized_pnl = q.value(8).toDouble();
    p.leverage = q.value(9).toDouble();
    p.liquidation_price = q.value(10).isNull() ? std::nullopt : std::optional<double>(q.value(10).toDouble());
    p.opened_at = q.value(11).toString();
    return p;
}

PtTrade PaperTradingRepository::map_trade(QSqlQuery& q) {
    PtTrade t;
    t.id = q.value(0).toString();
    t.portfolio_id = q.value(1).toString();
    t.order_id = q.value(2).toString();
    t.symbol = q.value(3).toString();
    t.side = q.value(4).toString();
    t.price = q.value(5).toDouble();
    t.quantity = q.value(6).toDouble();
    t.fee = q.value(7).toDouble();
    t.pnl = q.value(8).toDouble();
    t.timestamp = q.value(9).toString();
    return t;
}

// ── Column lists ─────────────────────────────────────────────────────────────

static const char* kPortfolioCols =
    "id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, exchange, created_at";

static const char* kOrderCols = "id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, "
                                "filled_qty, avg_price, status, reduce_only, created_at, filled_at";

static const char* kPositionCols = "id, portfolio_id, symbol, side, quantity, entry_price, current_price, "
                                   "unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at";

static const char* kTradeCols = "id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp";

// ── Portfolios ───────────────────────────────────────────────────────────────

Result<void> PaperTradingRepository::insert_portfolio(const PtPortfolio& p) {
    return exec_write("INSERT INTO pt_portfolios (id, name, initial_balance, balance, currency, "
                      "leverage, margin_mode, fee_rate, exchange, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                      {p.id, p.name, p.initial_balance, p.balance, p.currency, p.leverage, p.margin_mode, p.fee_rate,
                       p.exchange, p.created_at});
}

Result<PtPortfolio> PaperTradingRepository::get_portfolio(const QString& id) {
    return query_one(QString("SELECT %1 FROM pt_portfolios WHERE id = ?").arg(kPortfolioCols), {id}, map_portfolio);
}

std::optional<PtPortfolio> PaperTradingRepository::find_portfolio(const QString& name, const QString& exchange) {
    return query_optional(QString("SELECT %1 FROM pt_portfolios WHERE name = ? AND exchange = ?").arg(kPortfolioCols),
                          {name, exchange}, map_portfolio);
}

Result<QVector<PtPortfolio>> PaperTradingRepository::list_portfolios(const QString& exchange) {
    if (exchange.isEmpty()) {
        return query_list(QString("SELECT %1 FROM pt_portfolios ORDER BY created_at DESC").arg(kPortfolioCols), {},
                          map_portfolio);
    }
    return query_list(
        QString("SELECT %1 FROM pt_portfolios WHERE exchange = ? ORDER BY created_at DESC").arg(kPortfolioCols),
        {exchange}, map_portfolio);
}

Result<void> PaperTradingRepository::update_balance(const QString& id, double new_balance) {
    return exec_write("UPDATE pt_portfolios SET balance = ? WHERE id = ?", {new_balance, id});
}

Result<void> PaperTradingRepository::delete_portfolio(const QString& id) {
    // Cascade via foreign keys, but be explicit for safety
    exec_write("DELETE FROM pt_trades WHERE portfolio_id = ?", {id});
    exec_write("DELETE FROM pt_positions WHERE portfolio_id = ?", {id});
    exec_write("DELETE FROM pt_orders WHERE portfolio_id = ?", {id});
    exec_write("DELETE FROM pt_margin_blocks WHERE portfolio_id = ?", {id});
    exec_write("DELETE FROM pt_holdings WHERE portfolio_id = ?", {id});
    return exec_write("DELETE FROM pt_portfolios WHERE id = ?", {id});
}

// ── Orders ───────────────────────────────────────────────────────────────────

Result<void> PaperTradingRepository::insert_order(const PtOrder& o) {
    return exec_write("INSERT INTO pt_orders (id, portfolio_id, symbol, side, order_type, quantity, "
                      "price, stop_price, filled_qty, avg_price, status, reduce_only, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                      {o.id, o.portfolio_id, o.symbol, o.side, o.order_type, o.quantity,
                       o.price.has_value() ? QVariant(o.price.value()) : QVariant(),
                       o.stop_price.has_value() ? QVariant(o.stop_price.value()) : QVariant(), o.filled_qty,
                       o.avg_price.has_value() ? QVariant(o.avg_price.value()) : QVariant(), o.status,
                       o.reduce_only ? 1 : 0, o.created_at});
}

Result<PtOrder> PaperTradingRepository::get_order(const QString& id) {
    auto r = db().execute(QString("SELECT %1 FROM pt_orders WHERE id = ?").arg(kOrderCols), {id});
    if (r.is_err())
        return Result<PtOrder>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<PtOrder>::err("Not found");
    return Result<PtOrder>::ok(map_order(q));
}

Result<QVector<PtOrder>> PaperTradingRepository::get_orders(const QString& portfolio_id, const QString& status) {
    QString sql = QString("SELECT %1 FROM pt_orders WHERE portfolio_id = ?").arg(kOrderCols);
    QVariantList params = {portfolio_id};
    if (!status.isEmpty()) {
        sql += " AND status = ?";
        params.append(status);
    }
    sql += " ORDER BY created_at DESC";
    auto r = db().execute(sql, params);
    if (r.is_err())
        return Result<QVector<PtOrder>>::err(r.error());
    QVector<PtOrder> result;
    auto& q = r.value();
    while (q.next())
        result.append(map_order(q));
    return Result<QVector<PtOrder>>::ok(std::move(result));
}

Result<void> PaperTradingRepository::update_order_fill(const QString& id, double filled_qty, double avg_price,
                                                       const QString& status, const QString& filled_at) {
    return exec_write("UPDATE pt_orders SET filled_qty = ?, avg_price = ?, status = ?, filled_at = ? WHERE id = ?",
                      {filled_qty, avg_price, status, filled_at, id});
}

Result<void> PaperTradingRepository::cancel_order(const QString& id) {
    return exec_write("UPDATE pt_orders SET status = 'cancelled' WHERE id = ?", {id});
}

Result<void> PaperTradingRepository::cancel_all_orders(const QString& portfolio_id) {
    return exec_write("UPDATE pt_orders SET status = 'cancelled' WHERE portfolio_id = ? AND status = 'pending'",
                      {portfolio_id});
}

// ── Positions ────────────────────────────────────────────────────────────────

Result<void> PaperTradingRepository::insert_position(const PtPosition& p) {
    return exec_write("INSERT INTO pt_positions (id, portfolio_id, symbol, side, quantity, entry_price, "
                      "current_price, unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                      {p.id, p.portfolio_id, p.symbol, p.side, p.quantity, p.entry_price, p.current_price,
                       p.unrealized_pnl, p.realized_pnl, p.leverage,
                       p.liquidation_price.has_value() ? QVariant(p.liquidation_price.value()) : QVariant(),
                       p.opened_at});
}

Result<QVector<PtPosition>> PaperTradingRepository::get_positions(const QString& portfolio_id) {
    auto r = db().execute(
        QString("SELECT %1 FROM pt_positions WHERE portfolio_id = ? ORDER BY opened_at DESC").arg(kPositionCols),
        {portfolio_id});
    if (r.is_err())
        return Result<QVector<PtPosition>>::err(r.error());
    QVector<PtPosition> result;
    auto& q = r.value();
    while (q.next())
        result.append(map_position(q));
    return Result<QVector<PtPosition>>::ok(std::move(result));
}

std::optional<PtPosition> PaperTradingRepository::find_position(const QString& portfolio_id, const QString& symbol,
                                                                const QString& side) {
    auto r = db().execute(
        QString("SELECT %1 FROM pt_positions WHERE portfolio_id = ? AND symbol = ? AND side = ?").arg(kPositionCols),
        {portfolio_id, symbol, side});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return map_position(q);
}

Result<void> PaperTradingRepository::update_position(const QString& id, double quantity, double entry_price) {
    return exec_write("UPDATE pt_positions SET quantity = ?, entry_price = ? WHERE id = ?",
                      {quantity, entry_price, id});
}

Result<void> PaperTradingRepository::update_position_price(const QString& portfolio_id, const QString& symbol,
                                                           double price) {
    // Update current_price AND recompute unrealized_pnl atomically
    // long: (price - entry) * qty,  short: (entry - price) * qty
    return exec_write(
        "UPDATE pt_positions SET current_price = ?, "
        "unrealized_pnl = CASE WHEN side = 'long' THEN (? - entry_price) * quantity "
        "ELSE (entry_price - ?) * quantity END "
        "WHERE portfolio_id = ? AND symbol = ?",
        {price, price, price, portfolio_id, symbol});
}

Result<void> PaperTradingRepository::add_realized_pnl(const QString& id, double pnl) {
    return exec_write("UPDATE pt_positions SET realized_pnl = realized_pnl + ? WHERE id = ?", {pnl, id});
}

Result<void> PaperTradingRepository::delete_position(const QString& id) {
    return exec_write("DELETE FROM pt_positions WHERE id = ?", {id});
}

Result<void> PaperTradingRepository::delete_all_positions(const QString& portfolio_id) {
    return exec_write("DELETE FROM pt_positions WHERE portfolio_id = ?", {portfolio_id});
}

// ── Trades ───────────────────────────────────────────────────────────────────

Result<void> PaperTradingRepository::insert_trade(const PtTrade& t) {
    return exec_write(
        "INSERT INTO pt_trades (id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        {t.id, t.portfolio_id, t.order_id, t.symbol, t.side, t.price, t.quantity, t.fee, t.pnl, t.timestamp});
}

Result<QVector<PtTrade>> PaperTradingRepository::get_trades(const QString& portfolio_id, int64_t limit) {
    auto r = db().execute(QString("SELECT %1 FROM pt_trades WHERE portfolio_id = ? ORDER BY timestamp DESC LIMIT %2")
                              .arg(kTradeCols)
                              .arg(limit),
                          {portfolio_id});
    if (r.is_err())
        return Result<QVector<PtTrade>>::err(r.error());
    QVector<PtTrade> result;
    auto& q = r.value();
    while (q.next())
        result.append(map_trade(q));
    return Result<QVector<PtTrade>>::ok(std::move(result));
}

Result<void> PaperTradingRepository::delete_all_trades(const QString& portfolio_id) {
    return exec_write("DELETE FROM pt_trades WHERE portfolio_id = ?", {portfolio_id});
}

// ── Stats ────────────────────────────────────────────────────────────────────

Result<PtStats> PaperTradingRepository::get_stats(const QString& portfolio_id) {
    auto r = db().execute("SELECT "
                          "  COALESCE(SUM(pnl), 0),"
                          "  COUNT(*),"
                          "  SUM(CASE WHEN pnl > 0 THEN 1 ELSE 0 END),"
                          "  SUM(CASE WHEN pnl < 0 THEN 1 ELSE 0 END),"
                          "  COALESCE(MAX(pnl), 0),"
                          "  COALESCE(MIN(pnl), 0)"
                          " FROM pt_trades WHERE portfolio_id = ?",
                          {portfolio_id});

    if (r.is_err())
        return Result<PtStats>::err(r.error());

    auto& q = r.value();
    if (!q.next())
        return Result<PtStats>::ok(PtStats{});

    PtStats s;
    s.total_pnl = q.value(0).toDouble();
    s.total_trades = q.value(1).toLongLong();
    s.winning_trades = q.value(2).toLongLong();
    s.losing_trades = q.value(3).toLongLong();
    s.largest_win = q.value(4).toDouble();
    s.largest_loss = q.value(5).toDouble();
    s.win_rate = s.total_trades > 0 ? static_cast<double>(s.winning_trades) / static_cast<double>(s.total_trades) : 0.0;
    return Result<PtStats>::ok(s);
}

} // namespace fincept
