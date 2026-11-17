#pragma once
#include "screens/dashboard/widgets/PortfolioTileBase.h"

#include <QDate>
#include <QVector>

class QLabel;
class QPushButton;
class QTimer;

namespace fincept::ui {
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::widgets {

/// Portfolio performance tile (type id "performance").
///
/// VALUE and TODAY come from the live summary (PortfolioService::load_summary,
/// base currency). MTD / YTD / 1Y / SINCE-START are time-weighted returns from
/// the NAV snapshots with external flows removed (PortfolioService::load_snapshots
/// + ledger() + build_twr_series()), to the last snapshot date; the chart shows
/// that TWR index against the portfolio's benchmark (daily closes from the hub).
/// With too little history the tile offers to build it (backfill_history).
class PortfolioPerformanceWidget : public PortfolioTileBase {
    Q_OBJECT
  public:
    explicit PortfolioPerformanceWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

  protected:
    void on_portfolio_selected() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;

  private:
    struct IndexPoint {
        QDate date;
        double index = 1.0; ///< 1 + cumulative TWR
    };
    void request_all();
    void on_summary(const portfolio::PortfolioSummary& s);
    void render_summary();
    void on_snapshots(const QVector<portfolio::PortfolioSnapshot>& snaps);
    void subscribe_benchmark();
    void render_returns();
    void render_chart();
    double period_return(const QDate& anchor) const;
    QString benchmark_symbol() const;

    ui::KpiStrip* kpis_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QLabel* footer_ = nullptr;
    QPushButton* backfill_btn_ = nullptr;
    QTimer* refresh_timer_ = nullptr;

    QVector<IndexPoint> twr_;
    /// Currencies whose flows were converted at today's rate (dated rate not
    /// loaded): the returns are approximate ("≈" + note). Empty = exact.
    QStringList twr_approx_;
    portfolio::PortfolioSummary summary_; ///< last summary, re-rendered on a format change
    bool have_summary_ = false;
    QVector<QPointF> bench_;
    QString bench_topic_;
    int snapshot_count_ = 0;
    bool snapshots_loaded_ = false;
    bool backfilling_ = false;
};

} // namespace fincept::screens::widgets
