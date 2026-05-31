// src/ui/widgets/algo/BacktestReportPanel.h
#pragma once
#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QTableWidget;
class QGridLayout;
class QChart;
class QLineSeries;
class QValueAxis;

namespace fincept::ui::algo {

/// Renders a BacktestEngine result payload as a professional report: an expanded
/// KPI grid (return, Sharpe, Sortino, max-DD, Calmar, win-rate, profit-factor,
/// trades, expectancy, avg-bars), an equity curve, an underwater/drawdown chart,
/// and a trade-by-trade table. Reads the fields BacktestEngine already emits
/// (equity_curve, trades, sortino, calmar, expectancy, …).
class BacktestReportPanel : public QWidget {
    Q_OBJECT
public:
    explicit BacktestReportPanel(QWidget* parent = nullptr);

    void set_result(const QJsonObject& payload);
    void clear();

private:
    QWidget* build_kpis();
    QWidget* build_charts();
    QWidget* build_heatmap();
    QWidget* build_trades();
    void add_kpi(QGridLayout* grid, int row, int col, const QString& key, const QString& title);
    void populate_heatmap(const QJsonObject& payload);

    QHash<QString, QLabel*> kpi_val_;
    QHash<QString, QLabel*> kpi_sub_;

    QChart* equity_chart_ = nullptr;
    QLineSeries* equity_series_ = nullptr;
    QLineSeries* benchmark_series_ = nullptr;
    QValueAxis* eq_x_ = nullptr;
    QValueAxis* eq_y_ = nullptr;

    QGridLayout* heatmap_grid_ = nullptr;

    QChart* dd_chart_ = nullptr;
    QLineSeries* dd_series_ = nullptr;
    QValueAxis* dd_x_ = nullptr;
    QValueAxis* dd_y_ = nullptr;

    QTableWidget* trades_ = nullptr;

    QLabel* placeholder_ = nullptr;
    QWidget* body_ = nullptr;
};

} // namespace fincept::ui::algo
