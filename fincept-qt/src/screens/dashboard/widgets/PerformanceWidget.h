#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QHash>
#include <QLabel>

namespace fincept::screens::widgets {

/// Performance tracker widget — fetches a basket of symbols (SPY, QQQ, DIA, IWM, etc.)
/// and computes real YTD/daily performance metrics from yfinance data.
///
/// Subscribes to `market:quote:<sym>` on the DataHub for the benchmark
/// basket and re-derives the metrics from cached rows on every delivery.
class PerformanceWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PerformanceWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    void rebuild_from_cache();

    struct MetricRow {
        QWidget* row_widget = nullptr;
        QLabel* label = nullptr;
        QLabel* value = nullptr;
        QLabel* period = nullptr;
    };
    QVector<MetricRow> rows_;

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
