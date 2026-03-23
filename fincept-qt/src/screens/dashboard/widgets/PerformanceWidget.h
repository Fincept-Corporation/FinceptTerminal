#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QLabel>

namespace fincept::screens::widgets {

/// Performance tracker widget — fetches a basket of symbols (SPY, QQQ, DIA, IWM, etc.)
/// and computes real YTD/daily performance metrics from yfinance data.
class PerformanceWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PerformanceWidget(QWidget* parent = nullptr);

  private:
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    struct MetricRow {
        QLabel* label = nullptr;
        QLabel* value = nullptr;
        QLabel* period = nullptr;
    };
    QVector<MetricRow> rows_;
};

} // namespace fincept::screens::widgets
