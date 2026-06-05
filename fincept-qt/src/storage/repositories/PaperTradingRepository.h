#pragma once
#include "storage/repositories/BaseRepository.h"
#include "trading/TradingTypes.h"

#include <optional>

namespace fincept {

/// Data-access layer for all paper trading tables.
/// Business logic (fill engine, margin checks) stays in PaperTrading.cpp.
class PaperTradingRepository : public BaseRepository<trading::PtPortfolio> {
  public:
    static PaperTradingRepository& instance();

    // ── Portfolios ───────────────────────────────────────────────────────────
    Result<void> insert_portfolio(const trading::PtPortfolio& p);
    Result<trading::PtPortfolio> get_portfolio(const QString& id);
    std::optional<trading::PtPortfolio> find_portfolio(const QString& name, const QString& exchange);
    Result<QVector<trading::PtPortfolio>> list_portfolios(const QString& exchange = {});
    Result<void> update_balance(const QString& id, double new_balance);
    Result<void> delete_portfolio(const QString& id);

    // ── Orders ───────────────────────────────────────────────────────────────
    Result<void> insert_order(const trading::PtOrder& o);
    Result<trading::PtOrder> get_order(const QString& id);
    Result<QVector<trading::PtOrder>> get_orders(const QString& portfolio_id, const QString& status = {});
    /// Orders whose created_at falls in [from_iso, to_iso) (UTC ISO strings). Used
    /// by the order book's per-day view so each day starts empty.
    Result<QVector<trading::PtOrder>> get_orders_between(const QString& portfolio_id, const QString& from_iso,
                                                         const QString& to_iso);
    Result<void> update_order_fill(const QString& id, double filled_qty, double avg_price, const QString& status,
                                   const QString& filled_at);
    Result<void> cancel_order(const QString& id);
    Result<void> cancel_all_orders(const QString& portfolio_id);

    // ── Positions ────────────────────────────────────────────────────────────
    Result<void> insert_position(const trading::PtPosition& p);
    Result<trading::PtPosition> get_position(const QString& id);
    Result<QVector<trading::PtPosition>> get_positions(const QString& portfolio_id);
    std::optional<trading::PtPosition> find_position(const QString& portfolio_id, const QString& symbol,
                                                     const QString& side);
    Result<void> update_position(const QString& id, double quantity, double entry_price);
    Result<void> update_position_price(const QString& portfolio_id, const QString& symbol, double price);
    Result<void> add_realized_pnl(const QString& id, double pnl);
    /// Set the margin currently locked by an open position (v040). Released to
    /// available balance, proportionally, when the position is closed/reduced.
    Result<void> set_position_margin(const QString& id, double held_margin);
    /// Change a position's broker product (e.g. MIS -> CNC on conversion, v040).
    Result<void> set_position_product(const QString& id, const QString& product);
    Result<void> delete_position(const QString& id);
    Result<void> delete_all_positions(const QString& portfolio_id);

    // ── Margin Blocks (Phase 3 §4) ──────────────────────────────────────────
    // Tracks margin blocked from available balance per order, so the exact
    // amount can be released on fill/cancel. Backed by the existing
    // pt_margin_blocks table (created in v001_initial). order_id is UNIQUE.
    Result<void> insert_margin_block(const QString& id, const QString& portfolio_id, const QString& order_id,
                                     const QString& symbol, double blocked_amount);
    /// Returns the blocked amount for an order, or 0.0 if none recorded.
    double get_margin_block(const QString& order_id);
    Result<void> delete_margin_block(const QString& order_id);
    Result<void> delete_all_margin_blocks(const QString& portfolio_id);

    // ── Trades ───────────────────────────────────────────────────────────────
    Result<void> insert_trade(const trading::PtTrade& t);
    Result<QVector<trading::PtTrade>> get_trades(const QString& portfolio_id, int64_t limit = 100);
    /// Trades whose timestamp falls in [from_iso, to_iso) (UTC ISO strings).
    Result<QVector<trading::PtTrade>> get_trades_between(const QString& portfolio_id, const QString& from_iso,
                                                         const QString& to_iso);
    Result<void> delete_all_trades(const QString& portfolio_id);

    // ── Stats ────────────────────────────────────────────────────────────────
    Result<trading::PtStats> get_stats(const QString& portfolio_id);

    // ── Row mappers (public for reuse) ───────────────────────────────────────
    static trading::PtPortfolio map_portfolio(QSqlQuery& q);
    static trading::PtOrder map_order(QSqlQuery& q);
    static trading::PtPosition map_position(QSqlQuery& q);
    static trading::PtTrade map_trade(QSqlQuery& q);

  private:
    PaperTradingRepository() = default;
};

} // namespace fincept
