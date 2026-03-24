#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QAbstractListModel>
#include <QStyledItemDelegate>

namespace fincept::screens::polymarket {

/// Model for market/event list items.
class PolymarketMarketCardModel : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles { MarketRole = Qt::UserRole + 1, EventRole, ViewModeRole };

    explicit PolymarketMarketCardModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void set_markets(const QVector<services::polymarket::Market>& markets);
    void set_events(const QVector<services::polymarket::Event>& events);
    void set_view_mode(const QString& mode); // "markets" or "events"

    const services::polymarket::Market* market_at(int row) const;
    const services::polymarket::Event* event_at(int row) const;

  private:
    QVector<services::polymarket::Market> markets_;
    QVector<services::polymarket::Event> events_;
    QString view_mode_ = "markets";
};

/// Delegate that paints rich market cards with probability bars.
class PolymarketMarketCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit PolymarketMarketCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace fincept::screens::polymarket
