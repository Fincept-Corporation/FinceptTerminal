#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QAbstractListModel>
#include <QStyledItemDelegate>

namespace fincept::screens::polymarket {

/// Model for market/event list items. Consumes unified prediction::*
/// types so Polymarket and Kalshi rows render identically.
class PolymarketMarketCardModel : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles { MarketRole = Qt::UserRole + 1, EventRole, ViewModeRole };

    explicit PolymarketMarketCardModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void set_markets(const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void set_events(const QVector<fincept::services::prediction::PredictionEvent>& events);
    void set_view_mode(const QString& mode); // "markets" or "events"
    void set_presentation(const ExchangePresentation& p);

    /// Replace a single market by key.market_id without rebuilding the
    /// whole list. Returns true if the row existed and was updated.
    bool update_market(const fincept::services::prediction::PredictionMarket& market);

    const fincept::services::prediction::PredictionMarket* market_at(int row) const;
    const fincept::services::prediction::PredictionEvent* event_at(int row) const;
    const ExchangePresentation& presentation() const { return presentation_; }

  private:
    QVector<fincept::services::prediction::PredictionMarket> markets_;
    QVector<fincept::services::prediction::PredictionEvent> events_;
    QString view_mode_ = "markets";
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();
};

/// Delegate that paints rich market cards with probability bars.
/// Outcome names are read from the prediction::Outcome vector at paint time,
/// so Polymarket "Yes"/"No", Kalshi "yes"/"no", or custom multi-outcome
/// markets all render without code changes.
class PolymarketMarketCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit PolymarketMarketCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace fincept::screens::polymarket
