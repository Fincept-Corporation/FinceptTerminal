// src/screens/portfolio/views/PerformanceRiskView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHash>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace fincept::ui {
class DistributionChart;
class KpiStrip;
class MonthlyReturnsHeatmap;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

/// PERFORMANCE & RISK — realised performance of the book over a selectable
/// window (MTD … since inception):
///   * KPI tiles that FOLLOW the window: TWR and MWR side by side, annualised
///     (windows ≥ 1 year only), benchmark and excess, volatility (realised /
///     ex-ante), Sharpe, Sortino, drawdowns, beta, alpha, tracking error,
///     information ratio, up / down capture, Calmar, win rate, VaR / CVaR;
///   * chart (TimeSeriesChart): TWR index vs benchmark or NAV, underwater pane,
///     rolling volatility / Sharpe / beta pane;
///   * period table MTD / QTD / YTD / 1Y / 3Y / 5Y / inception;
///   * monthly / yearly return heatmap and the return distribution.
class PerformanceRiskView : public QWidget {
    Q_OBJECT
  public:
    explicit PerformanceRiskView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    /// Metrics of any window for this portfolio (others are ignored).
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    /// Benchmark level (base currency, ascending dates).
    void set_benchmark(const QString& symbol, const QStringList& dates, const QVector<double>& closes);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    enum class ChartMode { Return, Nav };
    enum class Rolling { Volatility, Sharpe, Beta };

    void build_ui();
    void apply_styles();
    void request_windows(bool all);
    void render_tiles();
    void render_chart();
    void render_periods();
    void render_calendar();
    void render_distribution();
    void render_notice();
    void export_csv();
    const portfolio::ComputedMetrics* window_metrics() const;
    const portfolio::ComputedMetrics* all_metrics() const;

    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QButtonGroup* window_group_ = nullptr;
    QPushButton* return_btn_ = nullptr;
    QPushButton* nav_btn_ = nullptr;
    QComboBox* rolling_cb_ = nullptr;
    QComboBox* rolling_len_cb_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* basis_label_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QTableWidget* periods_table_ = nullptr;
    ui::MonthlyReturnsHeatmap* heatmap_ = nullptr;
    ui::DistributionChart* dist_ = nullptr;
    QLabel* dist_note_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    QHash<int, portfolio::ComputedMetrics> metrics_; // PerfWindow -> metrics
    QHash<int, bool> pending_;                       // requested, not yet delivered
    portfolio::PerfWindow window_ = portfolio::PerfWindow::Y1;
    ChartMode chart_mode_ = ChartMode::Return;
    QString currency_;
    QString bench_symbol_;
    QStringList bench_dates_;
    QVector<double> bench_closes_;
    bool has_data_ = false;
};

} // namespace fincept::screens
