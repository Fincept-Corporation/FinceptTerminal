#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Activity feed showing recent trades for a market.
/// Consumes unified prediction::PredictionTrade so both Polymarket and Kalshi
/// render through the same table.
class PolymarketActivityFeed : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketActivityFeed(QWidget* parent = nullptr);

    void set_trades(const QVector<fincept::services::prediction::PredictionTrade>& trades);
    void clear();

    /// Per-exchange number formatting (decimals on the price column).
    void set_presentation(const ExchangePresentation& p);

  private:
    QTableWidget* table_ = nullptr;
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();

    // Cached last trades so set_presentation() can re-render with the new
    // decimal places.
    QVector<fincept::services::prediction::PredictionTrade> last_trades_;
};

} // namespace fincept::screens::polymarket
