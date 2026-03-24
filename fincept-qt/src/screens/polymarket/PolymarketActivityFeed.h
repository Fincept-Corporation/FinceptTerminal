#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Activity feed showing trades/splits/merges for a market.
class PolymarketActivityFeed : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketActivityFeed(QWidget* parent = nullptr);

    void set_activities(const QVector<services::polymarket::Activity>& activities);
    void set_trades(const QVector<services::polymarket::Trade>& trades);
    void clear();

  private:
    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens::polymarket
