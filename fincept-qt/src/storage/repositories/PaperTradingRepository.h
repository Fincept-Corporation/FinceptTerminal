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
    Result<void> update_order_fill(const QString& id, double filled_qty, double avg_price, const QString& status,
                                   const QString& filled_at);
    Result<void> cancel_order(const QString& id);
    Result<void> cancel_all_orders(const QString& portfolio_id);

    // ── Positions ────────────────────────────────────────────────────────────
    Result<void> insert_position(const trading::PtPosition& p);
    Result<QVector<trading::PtPosition>> get_positions(const QString& portfolio_id);
    std::optional<trading::PtPosition> find_position(const QString& portfolio_id, const QString& symbol,
                                                     const QString& side);
    Result<void> update_position(const QString& id, double quantity, double entry_price);
    Result<void> update_position_price(const QString& portfolio_id, const QString& symbol, double price);
    Result<void> add_realized_pnl(const QString& id, double pnl);
    Result<void> delete_position(const QString& id);
    Result<void> delete_all_positions(const QString& portfolio_id);

    // ── Trades ───────────────────────────────────────────────────────────────
    Result<void> insert_trade(const trading::PtTrade& t);
    Result<QVector<trading::PtTrade>> get_trades(const QString& portfolio_id, int64_t limit = 100);
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
