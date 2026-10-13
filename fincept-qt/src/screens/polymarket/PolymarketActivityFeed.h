#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QEvent>
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
    /// Prepend a live trade (e.g. from a WebSocket feed). Keeps the cap at
    /// 200 rows; older trades fall off the bottom. Flashes the new row
    /// briefly in the exchange accent to draw the eye.
    void append_trade(const fincept::services::prediction::PredictionTrade& trade);
    void clear();

    /// Per-exchange number formatting (decimals on the price column).
    void set_presentation(const ExchangePresentation& p);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();
    /// Shows `trades` exactly as "write rows in list order, then stable-sort by
    /// the header indicator" would. With `allow_incremental`, a refresh that
    /// only adds trades on top shifts the existing rows (insert at the top, trim
    /// the tail) instead of rewriting all 200 rows; cells are reused either way.
    void render_trades(const QVector<fincept::services::prediction::PredictionTrade>& trades, bool allow_incremental);
    /// True when a stable sort by the current header indicator would leave the
    /// rows where they are.
    bool rows_sorted_by_indicator() const;

    QTableWidget* table_ = nullptr;
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();

    // Cached last trades so set_presentation() can re-render with the new
    // decimal places.
    QVector<fincept::services::prediction::PredictionTrade> last_trades_;
    /// Table row i currently shows last_trades_[i] (no sort permuted the rows).
    bool rows_in_list_order_ = true;
};

} // namespace fincept::screens::polymarket
