// src/screens/portfolio/PortfolioPerfChart.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QGraphicsLineItem>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

/// QChartView subclass that draws a vertical crosshair and floating tooltip.
class CrosshairChartView : public QChartView {
    Q_OBJECT
  public:
    explicit CrosshairChartView(QChart* chart, QWidget* parent = nullptr);

    /// Feed the raw series points so we can snap to the nearest one.
    void set_series_data(const QVector<QPointF>& pts, const QString& currency);

  protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    void update_crosshair(const QPoint& widget_pos);
    void hide_crosshair();

    QVector<QPointF>      pts_;
    QString               currency_;
    QGraphicsLineItem*    v_line_   = nullptr;
    QLabel*               tooltip_  = nullptr;
};

/// NAV performance chart with period selector and info bar.
class PortfolioPerfChart : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioPerfChart(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    void set_currency(const QString& currency);
    /// Feed real SPY close prices for benchmark overlay.
    void set_spy_history(const QStringList& dates, const QVector<double>& closes);

  private:
    void build_ui();
    void update_chart();
    void set_period(const QString& period);
    QColor chart_color() const;

    // Period selector buttons
    QVector<QPushButton*> period_btns_;
    QString current_period_ = "1M";

    // Benchmark toggle button
    QPushButton* benchmark_btn_ = nullptr;
    bool show_benchmark_ = false;

    // Info bar
    QLabel* period_change_label_ = nullptr;
    QLabel* total_return_label_ = nullptr;
    QLabel* nav_label_ = nullptr;

    // Chart
    CrosshairChartView* chart_view_ = nullptr;

    // Data
    portfolio::PortfolioSummary summary_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    QString currency_ = "USD";

    // SPY benchmark history (real prices)
    QStringList     spy_dates_;
    QVector<double> spy_closes_;
};

} // namespace fincept::screens
