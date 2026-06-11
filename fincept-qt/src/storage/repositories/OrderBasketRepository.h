#pragma once
// OrderBasketRepository — persistence for Zerodha-style saved order baskets.
// A basket is a named group of order legs; legs are stored as a JSON array in
// the ActionCenter::serialize_unified_order shape so the same encoding handles
// queueing, baskets, and any future order persistence.

#include "storage/repositories/BaseRepository.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QVector>

namespace fincept {

struct OrderBasket {
    QString id;   // UUID
    QString name; // user label, e.g. "Nifty Core 5"
    QVector<trading::UnifiedOrder> legs;
};

class OrderBasketRepository : public BaseRepository<OrderBasket> {
  public:
    static OrderBasketRepository& instance();

    QVector<OrderBasket> list_all() const;
    /// Insert when basket.id is empty (a UUID is assigned), update otherwise.
    /// Returns the persisted basket (with id set).
    OrderBasket save(OrderBasket basket);
    void remove(const QString& id);

  private:
    OrderBasketRepository() = default;
};

} // namespace fincept
