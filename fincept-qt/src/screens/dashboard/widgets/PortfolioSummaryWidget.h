#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Portfolio Summary Widget — reads holdings from SQLite (portfolio table),
/// fetches live prices via yfinance, computes P&L and portfolio value.
/// Falls back to a demo portfolio if no DB holdings are found.
class PortfolioSummaryWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PortfolioSummaryWidget(QWidget* parent = nullptr);

    struct Holding {
        QString symbol;
        double shares = 0;
        double avg_cost = 0;
    };

    void load_holdings();
    void fetch_prices(const QVector<Holding>& holdings);
    void render(const QVector<Holding>& holdings, const QVector<services::QuoteData>& quotes);

    // Summary labels
    QLabel* total_value_lbl_ = nullptr;
    QLabel* day_pnl_lbl_ = nullptr;
    QLabel* total_pnl_lbl_ = nullptr;
    QLabel* num_holdings_lbl_ = nullptr;

    // Holdings list layout
    QVBoxLayout* list_layout_ = nullptr;
};

} // namespace fincept::screens::widgets
