// src/screens/portfolio/PortfolioPerfChart.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

/// NAV performance chart with period selector and info bar.
class PortfolioPerfChart : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioPerfChart(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    void set_currency(const QString& currency);

  private:
    void build_ui();
    void update_chart();
    void set_period(const QString& period);
    QColor chart_color() const;

    // Period selector buttons
    QVector<QPushButton*> period_btns_;
    QString current_period_ = "1M";

    // Info bar
    QLabel* period_change_label_ = nullptr;
    QLabel* total_return_label_ = nullptr;
    QLabel* nav_label_ = nullptr;

    // Chart
    QChartView* chart_view_ = nullptr;

    // Data
    portfolio::PortfolioSummary summary_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    QString currency_ = "USD";
};

} // namespace fincept::screens
