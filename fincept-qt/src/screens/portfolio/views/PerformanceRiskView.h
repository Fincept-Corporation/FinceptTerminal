// src/screens/portfolio/views/PerformanceRiskView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

/// Performance & Risk detail view with NAV chart, risk metric cards, and period selector.
class PerformanceRiskView : public QWidget {
    Q_OBJECT
  public:
    explicit PerformanceRiskView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    /// Benchmark closes (ascending dates) for the RETURN chart overlay.
    void set_benchmark(const QString& symbol, const QStringList& dates, const QVector<double>& closes);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// What the chart plots: NAV in currency (includes deposits/withdrawals), the
    /// flow-adjusted cumulative return against the benchmark, or the drawdown curve.
    enum class ChartMode { Nav, Return, Drawdown };

    void build_ui();
    void retranslateUi();
    void update_chart();
    void update_nav_chart(const QDate& cutoff);
    void update_return_chart(const QDate& cutoff);
    void update_drawdown_chart(const QDate& cutoff);
    void update_metrics();
    void set_period(const QString& period);
    void set_chart_mode(ChartMode mode);
    QDate period_cutoff() const;
    /// The TWR points inside the period, with the base point (last one on/before the
    /// cutoff) first so period returns cover the whole window.
    QVector<portfolio::TwrPoint> twr_window(const QDate& cutoff) const;

    // Period selector
    QVector<QPushButton*> period_btns_;
    QString current_period_ = "1Y";

    // Chart-mode selector
    QVector<QPushButton*> mode_btns_;
    ChartMode chart_mode_ = ChartMode::Nav;

    // Benchmark history for the RETURN overlay
    QString bench_symbol_;
    QStringList bench_dates_;
    QVector<double> bench_closes_;

    // Header labels
    QLabel* chart_title_ = nullptr;
    QLabel* metrics_header_ = nullptr;
    QLabel* rf_hint_ = nullptr; // basis line: TWR window, risk-free rate + source, risk basis
    QPushButton* export_btn_ = nullptr;

    void update_rf_hint();
    void export_csv();

    // Chart
    QChartView* chart_view_ = nullptr;

    // Metric cards
    struct MetricCard {
        QLabel* title = nullptr;
        QLabel* value = nullptr;
        QLabel* desc = nullptr;
    };
    MetricCard sharpe_card_;
    MetricCard sortino_card_;
    MetricCard beta_card_;
    MetricCard alpha_card_;
    MetricCard vol_card_;
    MetricCard drawdown_card_;
    MetricCard var_card_;
    MetricCard cvar_card_;
    MetricCard twr_card_;
    MetricCard cagr_card_;
    MetricCard calmar_card_;
    MetricCard cur_dd_card_;
    MetricCard excess_card_;
    MetricCard te_card_;
    MetricCard ir_card_;
    MetricCard win_card_;

    MetricCard add_metric_card(QLayout* layout, const QString& title, const QString& desc, const char* color);

    // Data
    portfolio::PortfolioSummary summary_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    portfolio::ComputedMetrics metrics_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
