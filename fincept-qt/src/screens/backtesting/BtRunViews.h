// src/screens/backtesting/BtRunViews.h
//
// The six run views of the Backtesting tab — OVERVIEW, PERFORMANCE, RISK, TRADES, EXPOSURE, COSTS
// (one sub-tab each; layouts: backtesting_tab_redesign_spec 2.6). Every view builds its widgets ONCE
// in its constructor (no data, no Python, no file I/O) and only refills them in show_run(); a null run
// clears. Views talk to each other only through the screen's BtSelection bus (BtTabKit.h) and ignore
// signals whose origin is the view itself or one of its children.
//
// PUBLIC API beyond BtRunView (no view declares signals of its own):
//   BtTradesView::focus_filter()
//       Ctrl+F target: focus the trades text filter and select its text (the view also binds Ctrl+F
//       itself, WidgetWithChildrenShortcut).
//   BtTradesView::trade_window(run, trade, &from_ms, &to_ms)            [static]
//       Zoom range of a trade: entry − max(20 bars, 50 % of the holding period) … exit (the last bar
//       for an open position) + the same margin. False when the trade has no entry time.
//   BtRiskView::drawdown_window(run, drawdown, &from_ms, &to_ms)       [static]
//       Zoom range of a drawdown: peak − 10 % of its span … recovery (the last bar while ongoing) + 10 %.
//   BtRiskView::select_drawdown(bus, run, rank, origin)                [static]
//       Emits drawdown_selected(rank) + time_range_selected(drawdown_window) +
//       trades_filter_requested(peak … end, "during drawdown #n") on `bus` with `origin`.
//   bt_view_owns_origin(view, origin)                                   [inline]
//       True when a bus signal came from `view` or one of its children (the echo test).
//
// Styling: only the object names / "tone" properties of BtTabKit.h's STYLING CONTRACT (nothing here
// calls setStyleSheet). Item-model colours come from tone_color() at data() time; the custom-painted
// charts read the theme at paint time; the MAE scatter (QtCharts) is re-themed on
// ThemeManager::appearance_settled. Numbers repaint on a format change.
#pragma once

#include "screens/backtesting/BtTabKit.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVector>

class QAbstractTableModel;
class QChart;
class QChartView;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QScatterSeries;
class QScrollArea;
class QShowEvent;
class QSortFilterProxyModel;
class QSplitter;
class QStackedWidget;
class QTableView;
class QValueAxis;

namespace fincept::ui {
class BarCellDelegate;
class BarChart;
class CandleChart;
class DistributionChart;
class EmptyState;
class KpiStrip;
class MonthlyReturnsHeatmap;
class StatTile;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::bt {

/// True when `origin` is `view` or one of its descendants (a bus signal's own echo).
inline bool bt_view_owns_origin(const QObject* view, const QObject* origin) {
    for (const QObject* o = origin; o; o = o->parent())
        if (o == view)
            return true;
    return false;
}

// ── OVERVIEW ────────────────────────────────────────────────────────────────
/// Notice strip, equity vs benchmark (drawdown pane, top-5 drawdown bands, trade markers), snapshot
/// panels (returns, risk, trades, vs benchmark, exposure / costs) and the monthly / top drawdowns /
/// last trades row. The snapshot column moves under the chart when the view is narrow.
class BtOverviewView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtOverviewView(BtSelection* selection, QWidget* parent = nullptr);
    void show_run(const BtRunPtr& run) override;

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void render();
    void render_notice();
    void render_chart();
    void rebuild_annotations();
    void render_snapshot();
    void render_bottom();
    void relayout(int width, bool force);
    void fit_to_font();
    void open_month(int year, int month);
    void open_drawdown(int rank);
    void open_trade(const QString& trade_id);
    void zoom_to_trade(const QString& trade_id);

    QFrame* notice_ = nullptr;
    QLabel* notice_chip_ = nullptr;
    QLabel* notice_text_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;
    QGridLayout* grid_ = nullptr;
    BtPanel chart_panel_;
    ui::TimeSeriesChart* chart_ = nullptr;
    QPushButton* trades_btn_ = nullptr;
    int dd_pane_ = 1;
    int eq_series_ = -1;
    QWidget* snapshot_ = nullptr;
    QGridLayout* snapshot_grid_ = nullptr;
    QVector<BtPanel> snap_panels_;
    QVector<BtKvGrid*> snap_kv_;
    QWidget* bottom_ = nullptr;
    QGridLayout* bottom_grid_ = nullptr;
    QVector<BtPanel> bottom_panels_;
    ui::MonthlyReturnsHeatmap* mini_heat_ = nullptr;
    QTableView* dd_table_ = nullptr;
    QAbstractTableModel* dd_model_ = nullptr;
    QTableView* last_table_ = nullptr;
    QAbstractTableModel* last_model_ = nullptr;
    QHash<int, int> marker_trade_; ///< chart annotation id -> index into run_->trades
    QVector<int> marker_order_;    ///< trade indexes, newest entry first (built when markers need it)
    QString sel_trade_;
    int sel_dd_ = 0;
    int layout_mode_ = -1;
    int snapshot_cols_ = -1;
    int bottom_cols_ = -1;
    int kv_cols_ = -1;
};

// ── PERFORMANCE ─────────────────────────────────────────────────────────────
/// Monthly heat map (strategy / benchmark / excess), yearly bars, rolling Sharpe / volatility / beta,
/// return distribution (daily / weekly / monthly) and the period statistics table.
class BtPerformanceView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtPerformanceView(BtSelection* selection, QWidget* parent = nullptr);
    void show_run(const BtRunPtr& run) override;

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void render();
    void render_heatmap();
    void render_yearly();
    void render_rolling();
    void render_distribution();
    void render_stats();
    void relayout(int width, bool force);
    void fit_to_font();
    void open_period(int year, int month, bool to_overview);

    QScrollArea* scroll_ = nullptr;
    QGridLayout* grid_ = nullptr;
    BtPanel heat_panel_;
    QWidget* heat_seg_ = nullptr;
    ui::MonthlyReturnsHeatmap* heat_ = nullptr;
    int heat_mode_ = 0;
    BtPanel year_panel_;
    ui::BarChart* years_chart_ = nullptr;
    QVector<int> years_;
    BtPanel roll_panel_;
    ui::TimeSeriesChart* rolling_ = nullptr;
    QLabel* roll_note_ = nullptr;
    int roll_vol_pane_ = 1;
    int roll_third_pane_ = 2;
    BtPanel dist_panel_;
    QWidget* dist_seg_ = nullptr;
    ui::DistributionChart* dist_ = nullptr;
    QLabel* dist_note_ = nullptr;
    int dist_mode_ = 0;
    QVector<double> dist_samples_[3]; ///< percent units: bar, weekly, monthly returns
    bool dist_ready_[3] = {false, false, false};
    BtPanel stats_panel_;
    QTableView* stats_table_ = nullptr;
    QAbstractTableModel* stats_model_ = nullptr;
    int grid_cols_ = -1;
};

// ── RISK ────────────────────────────────────────────────────────────────────
/// Tail / downside KPIs, the underwater curve, the top-10 drawdowns table and the tail distribution.
class BtRiskView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtRiskView(BtSelection* selection, QWidget* parent = nullptr);
    void show_run(const BtRunPtr& run) override;

    /// Zoom range of `dd`: peak − 10 % of its span … recovery (last bar while ongoing) + 10 %.
    static bool drawdown_window(const BtRun& run, const services::backtest::BtDrawdown& dd, double* from_ms,
                                double* to_ms);
    /// Emit the drawdown selection set for `rank` (see the header comment) on `bus`.
    static void select_drawdown(BtSelection* bus, const BtRun& run, int rank, QObject* origin);

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void render();
    void render_kpis();
    void render_underwater();
    void render_table();
    void render_tail();
    void mark_drawdown(int rank, bool zoom);
    void relayout(int width, bool force);
    void fit_to_font();

    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;
    QGridLayout* grid_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    BtPanel uw_panel_;
    ui::TimeSeriesChart* underwater_ = nullptr;
    BtPanel dd_panel_;
    QTableView* dd_table_ = nullptr;
    QAbstractTableModel* dd_model_ = nullptr;
    QSortFilterProxyModel* dd_proxy_ = nullptr;
    ui::BarCellDelegate* depth_delegate_ = nullptr;
    QLabel* dd_empty_ = nullptr;
    BtPanel tail_panel_;
    ui::DistributionChart* tail_ = nullptr;
    QLabel* tail_note_ = nullptr;
    BtKvGrid* tail_kv_ = nullptr;
    QVector<double> sorted_returns_; ///< finite bar returns of the run, ascending
    int sel_rank_ = 0;
    bool syncing_ = false;
    int layout_mode_ = -1;
};

// ── TRADES ──────────────────────────────────────────────────────────────────
/// Filter bar, price + trades chart (or the equity chart when priceData is absent), P&L per trade
/// distribution, MAE / MFE scatter, the trades table with totals over the filtered rows, and the
/// BY SYMBOL attribution table. Zero trades -> an EmptyState in place of the table area.
class BtTradesView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtTradesView(BtSelection* selection, QWidget* parent = nullptr);
    void show_run(const BtRunPtr& run) override;
    /// Focus the text filter and select its text (Ctrl+F).
    void focus_filter();
    /// Zoom range of `trade` (see the header comment). False when it has no entry time.
    static bool trade_window(const BtRun& run, const services::backtest::BtTrade& trade, double* from_ms,
                             double* to_ms);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

  private:
    void build_filter_bar();
    void build_top();
    void build_tables();
    void render();
    void refresh_filter();
    void render_attribution();
    void render_distribution();
    void render_scatter();
    void theme_scatter();
    void render_price_fallback();
    void request_price_chart(const QString& symbol);
    void load_price_chart();
    void mark_selected_trade(bool follow_symbol);
    void on_current_trade(int source_row);
    void on_activated_trade(int source_row);
    void select_trade_by_id(const QString& trade_id, bool scroll);
    void reset_filters();
    void set_view_page(int page);
    void relayout(int width, bool force);
    void fit_to_font();
    int source_row_of(const QString& trade_id) const;

    // filter bar
    QWidget* bar_ = nullptr;
    QHBoxLayout* bar_row_a_ = nullptr;
    QHBoxLayout* bar_row_b_ = nullptr;
    QLineEdit* filter_edit_ = nullptr;
    QWidget* outcome_seg_ = nullptr;
    QWidget* side_seg_ = nullptr;
    QComboBox* symbol_combo_ = nullptr;
    QComboBox* reason_combo_ = nullptr;
    QLabel* ext_chip_ = nullptr;
    QPushButton* ext_clear_ = nullptr;
    QLabel* count_label_ = nullptr;
    QWidget* page_seg_ = nullptr;
    int outcome_ = 0; ///< 0 all, 1 winners, 2 losers
    int side_ = 2;    ///< 0 long, 1 short, 2 both
    double ext_from_ = kBtNaN;
    double ext_to_ = kBtNaN;
    QString ext_label_;
    bool bar_compact_ = false;

    // top charts
    QSplitter* splitter_ = nullptr;
    QWidget* top_ = nullptr;
    QGridLayout* top_grid_ = nullptr;
    BtPanel price_panel_;
    QComboBox* price_combo_ = nullptr;
    QStackedWidget* price_stack_ = nullptr;
    ui::CandleChart* candle_ = nullptr;
    ui::TimeSeriesChart* fallback_ = nullptr;
    QString chart_symbol_;     ///< symbol loaded in candle_
    QString pending_symbol_;   ///< symbol of a deferred load
    bool load_pending_ = false;
    bool load_on_show_ = false; ///< a load was requested while hidden
    QVector<int> candle_lines_;      ///< the selected trade's band on candle_ (annotation ids)
    bool candle_zoom_pending_ = false; ///< a selection asked for another symbol: zoom to it once loaded
    int fallback_band_ = -1;
    BtPanel dist_panel_;
    ui::DistributionChart* pnl_dist_ = nullptr;
    BtPanel scatter_panel_;
    QStackedWidget* scatter_stack_ = nullptr;
    QChartView* scatter_view_ = nullptr;
    QChart* scatter_ = nullptr;
    QScatterSeries* win_series_ = nullptr;
    QScatterSeries* loss_series_ = nullptr;
    QValueAxis* scatter_x_ = nullptr;
    QValueAxis* scatter_y_ = nullptr;
    QLabel* scatter_empty_ = nullptr;
    QList<QPointF> win_points_;
    QList<QPointF> loss_points_;
    QVector<int> win_rows_;  ///< trade index per point of win_series_
    QVector<int> loss_rows_; ///< trade index per point of loss_series_
    bool scatter_hold_x_ = false;
    int top_cols_ = -1;

    // tables
    QStackedWidget* table_stack_ = nullptr;
    QWidget* trades_page_ = nullptr;
    QTableView* table_ = nullptr;
    QAbstractTableModel* trades_model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    ui::BarCellDelegate* pnl_delegate_ = nullptr;
    ui::BarCellDelegate* mae_delegate_ = nullptr;
    ui::BarCellDelegate* mfe_delegate_ = nullptr;
    BtKvGrid* totals_ = nullptr;
    QTableView* attr_table_ = nullptr;
    QAbstractTableModel* attr_model_ = nullptr;
    QSortFilterProxyModel* attr_proxy_ = nullptr;
    ui::BarCellDelegate* attr_pnl_delegate_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    int page_ = 0; ///< 0 trades, 1 by symbol

    QString ccy_;
    QString sel_trade_;
    QString zoomed_trade_;
    bool syncing_ = false;
    quint64 gen_ = 0;
};

// ── EXPOSURE ────────────────────────────────────────────────────────────────
/// Exposure KPIs, gross / net exposure over time with cash (or, without an exposure series, the
/// equity curve with the in-market spans shaded) and the positions at the end of the run + cash.
class BtExposureView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtExposureView(BtSelection* selection, QWidget* parent = nullptr);
    void show_run(const BtRunPtr& run) override;

  private:
    void render();
    void render_kpis();
    void render_chart();
    void render_positions();
    void fit_to_font();

    QScrollArea* scroll_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QFrame* notice_ = nullptr;
    QLabel* notice_text_ = nullptr;
    BtPanel chart_panel_;
    ui::TimeSeriesChart* chart_ = nullptr;
    int pane2_ = 1;
    BtPanel pos_panel_;
    QTableView* pos_table_ = nullptr;
    QAbstractTableModel* pos_model_ = nullptr;
    QSortFilterProxyModel* pos_proxy_ = nullptr;
    ui::BarCellDelegate* weight_delegate_ = nullptr;
    QLabel* pos_note_ = nullptr;
};

// ── COSTS ───────────────────────────────────────────────────────────────────
/// Cost KPIs, the gross -> net waterfall, costs / turnover / notional by year and the cost model.
class BtCostsView : public BtRunView {
    Q_OBJECT
  public:
    explicit BtCostsView(BtSelection* selection, QWidget* parent = nullptr);
    void show_run(const BtRunPtr& run) override;

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void render();
    void render_kpis();
    void render_waterfall();
    void render_by_year();
    void render_model();
    void relayout(int width, bool force);
    void fit_to_font();

    QScrollArea* scroll_ = nullptr;
    QWidget* content_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QFrame* notice_ = nullptr;
    QLabel* notice_text_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    BtPanel wf_panel_;
    ui::BarChart* waterfall_ = nullptr;
    QLabel* wf_note_ = nullptr;
    BtPanel year_panel_;
    QWidget* year_seg_ = nullptr;
    ui::BarChart* by_year_ = nullptr;
    QLabel* year_note_ = nullptr;
    int year_mode_ = 0;
    BtPanel model_panel_;
    BtKvGrid* model_kv_ = nullptr;
    QLabel* model_source_ = nullptr;
    QLabel* not_modelled_title_ = nullptr;
    QLabel* not_modelled_ = nullptr;
    QVector<services::backtest::BtCostYear> years_; ///< costs.byYear, or computed from the trades
    bool years_computed_ = false;
    int layout_mode_ = -1;
};

} // namespace fincept::screens::bt
