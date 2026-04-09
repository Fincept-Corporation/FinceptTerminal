// src/screens/portfolio/PortfolioStatsRibbon.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

class PortfolioStatsRibbon : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioStatsRibbon(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    void refresh_theme();

  private:
    struct MetricCell {
        QWidget* container = nullptr;
        QLabel* label = nullptr;
        QLabel* value = nullptr;
        QLabel* sub = nullptr;
    };

    MetricCell add_cell(const QString& label_text, const char* value_color);

    QHBoxLayout* cells_layout_ = nullptr;
    MetricCell total_value_;
    MetricCell pnl_;
    MetricCell day_change_;
    MetricCell cost_basis_;
    MetricCell positions_;
    MetricCell concentration_;
    MetricCell sharpe_;
    MetricCell beta_;
    MetricCell volatility_;
    MetricCell max_drawdown_;
    MetricCell risk_score_;
    MetricCell var95_;
};

} // namespace fincept::screens
