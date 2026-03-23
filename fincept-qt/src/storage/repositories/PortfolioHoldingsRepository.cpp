#include "storage/repositories/PortfolioHoldingsRepository.h"

namespace fincept {

PortfolioHoldingsRepository& PortfolioHoldingsRepository::instance() {
    static PortfolioHoldingsRepository s;
    return s;
}

PortfolioHolding PortfolioHoldingsRepository::map_row(QSqlQuery& q) {
    return {
        q.value(0).toInt(),    q.value(1).toString(), q.value(2).toString(), q.value(3).toDouble(),
        q.value(4).toDouble(), q.value(5).toBool(),   q.value(6).toString(), q.value(7).toString(),
    };
}

Result<QVector<PortfolioHolding>> PortfolioHoldingsRepository::get_active() {
    return query_list("SELECT id, symbol, name, shares, avg_cost, active, added_at, updated_at "
                      "FROM portfolio_holdings WHERE active = 1 ORDER BY symbol",
                      {}, map_row);
}

Result<QVector<PortfolioHolding>> PortfolioHoldingsRepository::list_all() {
    return query_list("SELECT id, symbol, name, shares, avg_cost, active, added_at, updated_at "
                      "FROM portfolio_holdings ORDER BY symbol",
                      {}, map_row);
}

Result<qint64> PortfolioHoldingsRepository::add(const QString& symbol, double shares, double avg_cost,
                                                const QString& name) {
    return exec_insert("INSERT INTO portfolio_holdings (symbol, name, shares, avg_cost) VALUES (?, ?, ?, ?)",
                       {symbol.toUpper(), name, shares, avg_cost});
}

Result<void> PortfolioHoldingsRepository::update(int id, double shares, double avg_cost) {
    return exec_write("UPDATE portfolio_holdings SET shares = ?, avg_cost = ?, updated_at = datetime('now') "
                      "WHERE id = ?",
                      {shares, avg_cost, id});
}

Result<void> PortfolioHoldingsRepository::deactivate(int id) {
    return exec_write("UPDATE portfolio_holdings SET active = 0, updated_at = datetime('now') WHERE id = ?", {id});
}

Result<void> PortfolioHoldingsRepository::remove(int id) {
    return exec_write("DELETE FROM portfolio_holdings WHERE id = ?", {id});
}

} // namespace fincept
