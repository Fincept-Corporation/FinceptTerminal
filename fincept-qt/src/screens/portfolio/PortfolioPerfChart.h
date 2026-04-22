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

    QVector<QPointF> pts_;
    QString currency_;
    QGraphicsLineItem* v_line_ = nullptr;
    QLabel* tooltip_ = nullptr;
};

/// NAV performance chart with period selector and info bar.
class PortfolioPerfChart : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioPerfChart(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_snapshots(const QVector<portfolio::PortfolioSnapshot>& snapshots);
    void set_currency(const QString& currency);
    /// Feed real benchmark closes for the overlay. The symbol is shown on the
    /// toggle button and used to disambiguate currencies in indexed mode.
    void set_benchmark_history(const QString& symbol, const QStringList& dates,
                               const QVector<double>& closes);
    /// Legacy shim — same as set_benchmark_history("SPY", dates, closes).
    void set_spy_history(const QStringList& dates, const QVector<double>& closes);
    void refresh_theme();

  signals:
    /// Emitted when the user clicks a period button that requires backfilling
    /// (e.g. 5Y when only 1Y is cached). Owner can call
    /// PortfolioService::backfill_history(portfolio_id, period) in response.
    void backfill_period_requested(QString period);

  private:
    void build_ui();
    void update_chart();
    void set_period(const QString& period);
    void update_period_buttons_enabled();
    QColor chart_color() const;
    /// Convert a UTC ISO date string to ms since epoch (UTC midnight).
    /// Centralised so portfolio and benchmark series share the same time axis.
    static qint64 iso_date_to_ms_utc(const QString& iso_date);

    // Period selector buttons
    QVector<QPushButton*> period_btns_;
    QString current_period_ = "1M";

    // Benchmark toggle + indexed-mode toggle
    QPushButton* benchmark_btn_ = nullptr;
    QPushButton* indexed_btn_ = nullptr;
    bool show_benchmark_ = false;
    bool indexed_mode_ = false; // false = currency value, true = base-100 indexed

    // Info bar
    QLabel* period_change_label_ = nullptr;
    QLabel* total_return_label_ = nullptr;
    QLabel* nav_label_ = nullptr;
    QLabel* cost_basis_label_ = nullptr;

    // Chart
    CrosshairChartView* chart_view_ = nullptr;

    // Data
    portfolio::PortfolioSummary summary_;
    QVector<portfolio::PortfolioSnapshot> snapshots_;
    QString currency_ = "USD";

    // Benchmark history — symbol may differ from "SPY" depending on currency
    QString benchmark_symbol_ = "SPY";
    QStringList spy_dates_;
    QVector<double> spy_closes_;
};

} // namespace fincept::screens
