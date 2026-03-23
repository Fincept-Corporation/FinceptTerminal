#pragma once
#include "storage/repositories/BaseRepository.h"

namespace fincept {

struct PortfolioHolding {
    int id = 0;
    QString symbol;
    QString name;
    double shares = 0;
    double avg_cost = 0;
    bool active = true;
    QString added_at;
    QString updated_at;
};

class PortfolioHoldingsRepository : public BaseRepository<PortfolioHolding> {
  public:
    static PortfolioHoldingsRepository& instance();

    Result<QVector<PortfolioHolding>> get_active();
    Result<QVector<PortfolioHolding>> list_all();
    Result<qint64> add(const QString& symbol, double shares, double avg_cost, const QString& name = {});
    Result<void> update(int id, double shares, double avg_cost);
    Result<void> deactivate(int id);
    Result<void> remove(int id);

  private:
    PortfolioHoldingsRepository() = default;
    static PortfolioHolding map_row(QSqlQuery& q);
};

} // namespace fincept
