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

  private:
    void build_ui();
    void update_chart();
    void update_metrics();
    void set_period(const QString& period);

    // Period selector
    QVector<QPushButton*> period_btns_;
    QString current_period_ = "1Y";

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

    MetricCard add_metric_card(QLayout* layout, const QString& title, const QString& desc, const char* color);

    // Data
    portfolio::PortfolioSummary summary_;
    QString currency_;
};

} // namespace fincept::screens
