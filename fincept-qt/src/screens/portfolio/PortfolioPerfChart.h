// src/screens/portfolio/PortfolioPerfChart.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

namespace fincept::ui {
class TimeSeriesChart;
}

namespace fincept::screens {

/// Main-view performance panel on the shared TimeSeriesChart (crosshair, zoom,
/// range presets, export).
///
///   VALUE  — NAV in the base currency (deposits / withdrawals included) with the
///            cost basis as a reference line;
///   RETURN — the time-weighted return index against the benchmark, both rebased
///            to the first visible day, so every zoom / preset reads as that
///            period's flow-adjusted return.
/// The info line quotes the visible period's TWR, the benchmark's and the NAV.
class PortfolioPerfChart : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioPerfChart(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    void set_currency(const QString& currency);
    /// Benchmark level in the base currency (ascending dates).
    void set_benchmark_history(const QString& symbol, const QStringList& dates, const QVector<double>& closes);
    /// Flow-adjusted (time-weighted) return series of the whole history.
    void set_twr_series(const QVector<portfolio::TwrPoint>& series);
    void refresh_theme();

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    /// The user picked a range that needs more stored history than exists
    /// ("1y" / "5y"); the owner calls PortfolioService::backfill_history.
    void backfill_period_requested(QString period);

  private:
    enum class Mode { Value, Return };
    void build_ui();
    void rebuild_series();
    void update_info();
    void set_mode(Mode m);
    void apply_styles();
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QPushButton* value_btn_ = nullptr;
    QPushButton* return_btn_ = nullptr;
    QPushButton* bench_btn_ = nullptr;
    QLabel* info_label_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;

    Mode mode_ = Mode::Value;
    bool show_benchmark_ = true;
    portfolio::PortfolioSummary summary_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    QVector<portfolio::TwrPoint> twr_;
    QString currency_; ///< portfolio base currency; empty (no unit printed) until set_currency()
    QString bench_symbol_;
    QStringList bench_dates_;
    QVector<double> bench_closes_;
    int nav_series_ = -1;
    int twr_series_id_ = -1;
    int bench_series_ = -1;
};

} // namespace fincept::screens
