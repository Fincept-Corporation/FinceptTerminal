#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

#include <QLineEdit>

namespace fincept::screens::widgets {

/// Watchlist widget — user provides comma-separated symbols, fetches live data via yfinance.
class WatchlistWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit WatchlistWidget(QWidget* parent = nullptr);

  private:
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    QLineEdit* symbols_input_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QStringList symbols_;
};

} // namespace fincept::screens::widgets
