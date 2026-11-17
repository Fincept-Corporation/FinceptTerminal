#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataTypes.h"
#include "services/markets/MarketPerformance.h"

#include <QHash>

class QLabel;

namespace fincept::ui {
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::widgets {

/// Market risk tile (type id "market_risk"; the old "risk_metrics" id now shows
/// portfolio risk — PortfolioRiskWidget).
///
/// For the user's MarketProfile region: the region's volatility gauge (level,
/// regime), the benchmark's realised 20-day volatility and its distance from the
/// 52-week high, the implied-minus-realised spread where the gauge measures the
/// same market (VIX / S&P 500, India VIX / Nifty 50), and the VIX term ratio
/// (VIX / VIX3M) where available. The chart shows the gauge's year of daily
/// closes with the 20 and 30 regime lines.
class RiskMetricsWidget : public BaseWidget {
    Q_OBJECT
  public:
    /// Config: {"market_region": "<id>"} pins the tile to one region (RegionPin.h).
    explicit RiskMetricsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    void on_topic_errors_changed() override;

  private:
    void hub_subscribe_all();
    void reload_region();
    void render();
    void render_chart();
    QStringList topics() const;

    ui::KpiStrip* kpis_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QLabel* footer_ = nullptr;

    QString region_pin_; ///< "" = follow the selector
    QString bench_;
    QString gauge_;
    bool gauge_matches_bench_ = false;
    bool has_term_ = false;
    QHash<QString, services::QuoteData> quotes_;
    QHash<QString, services::PerformanceStats> stats_;
    QVector<QPointF> gauge_hist_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
