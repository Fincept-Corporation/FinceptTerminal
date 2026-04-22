// src/storage/repositories/PortfolioRepository.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept {

class PortfolioRepository : public BaseRepository<portfolio::Portfolio> {
  public:
    static PortfolioRepository& instance();

    // ── Portfolios CRUD ──────────────────────────────────────────────────────
    Result<QVector<portfolio::Portfolio>> list_portfolios();
    Result<portfolio::Portfolio> get_portfolio(const QString& id);
    Result<QString> create_portfolio(const QString& name, const QString& owner, const QString& currency,
                                     const QString& description = {});
    Result<void> update_portfolio(const QString& id, const QString& name, const QString& owner, const QString& currency,
                                  const QString& description = {});
    Result<void> delete_portfolio(const QString& id);

    // ── Assets CRUD ──────────────────────────────────────────────────────────
    Result<QVector<portfolio::PortfolioAsset>> get_assets(const QString& portfolio_id);
    Result<qint64> add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                             const QString& date = {}, const QString& sector = {});
    Result<void> update_asset(const QString& portfolio_id, const QString& symbol, double qty, double avg_price);
    Result<void> set_asset_sector(const QString& portfolio_id, const QString& symbol, const QString& sector);
    Result<void> remove_asset(const QString& portfolio_id, const QString& symbol);

    // ── Transactions ─────────────────────────────────────────────────────────
    Result<QVector<portfolio::Transaction>> get_transactions(const QString& portfolio_id, int limit = 50);
    Result<QVector<portfolio::Transaction>> get_symbol_transactions(const QString& portfolio_id, const QString& symbol);
    Result<QString> add_transaction(const QString& portfolio_id, const QString& symbol, const QString& type, double qty,
                                    double price, const QString& date, const QString& notes = {});
    Result<void> update_transaction(const QString& id, double qty, double price, const QString& date,
                                    const QString& notes = {});
    Result<void> delete_transaction(const QString& id);

    // ── Snapshots ────────────────────────────────────────────────────────────
    Result<void> save_snapshot(const QString& portfolio_id, double value, double cost_basis, double pnl, double pnl_pct,
                               const QString& date);
    Result<QVector<portfolio::PortfolioSnapshot>> get_snapshots(const QString& portfolio_id, int days = 365);

  private:
    PortfolioRepository() = default;

    static portfolio::Portfolio map_portfolio(QSqlQuery& q);
    static portfolio::PortfolioAsset map_asset(QSqlQuery& q);
    static portfolio::Transaction map_transaction(QSqlQuery& q);
    static portfolio::PortfolioSnapshot map_snapshot(QSqlQuery& q);
};

} // namespace fincept
