// src/screens/backtesting/BtAnalysisViews.h
//
// The four analysis views of the Backtesting tab (namespace fincept::screens::bt): OPTIMISE,
// WALK-FWD, MONTE CARLO and COMPARE. Each one only RENDERS a parsed result (BacktestResultModel.h);
// the screen owns the settings strips above them, the runs and the run controller.
//
// Every widget is built once in the constructor; show_result() / show_runs() only update data and
// stay well under 100 ms on the GUI thread (2,000 optimisation rows, 20,000-bar equity curves):
// model/view tables (ui::LiveTableModel + a sort proxy), series thinned to <= 5,000 points, no
// resize-to-contents. Styling follows the contract in BtTabKit.h (object names + the "tone"
// property, styled by the screen's single root stylesheet); charts read the theme at paint time.
// A format change re-renders from the held result; a font change re-fits heights on
// ThemeManager::appearance_settled.
//
//   BtOptimizeView.cpp     OPTIMISE (+ the shared helpers declared at the end of this header)
//   BtWalkForwardView.cpp  WALK-FWD
//   BtMonteCarloView.cpp   MONTE CARLO
//   BtCompareView.cpp      COMPARE
#pragma once

#include "screens/backtesting/BtTabKit.h"
#include "services/backtesting/BacktestResultModel.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QBoxLayout;
class QComboBox;
class QEvent;
class QLabel;
class QMenu;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QVBoxLayout;

namespace fincept::ui {
class BarCellDelegate;
class BarChart;
class DistributionChart;
class EmptyState;
class FanChart;
class KpiStrip;
class SensitivityTable;
class StatTile;
class TimeSeriesChart;
template <typename Row>
class LiveTableModel;
} // namespace fincept::ui

namespace fincept::screens::bt {

// Table rows (defined in the .cpp files).
struct BtOptTopRow;
struct BtOptHoldRow;
struct BtWfParamRow;
struct BtMcPctRow;
struct BtCmpMetricRow;
struct BtCmpDiffRow;
class BtAnalysisReflow;
class BtAnalysisNotices;

// ── OPTIMISE ────────────────────────────────────────────────────────────────

class BtOptimizeView : public QWidget {
    Q_OBJECT
  public:
    explicit BtOptimizeView(QWidget* parent = nullptr);
    ~BtOptimizeView() override;
    /// engine_label e.g. "VectorBT"; ccy = ISO code of money values. A null result shows the empty state.
    void show_result(const services::backtest::BtOptimizePtr& result, const QString& ccy, const QString& engine_label);

  signals:
    void apply_requested(const QJsonObject& params); ///< APPLY: copy a parameter set into the setup
    void run_requested(const QJsonObject& params);   ///< RUN: backtest that set (becomes the active run)

  private:
    void build_ui();
    void load_axes();
    /// `fresh` = a new result (TOP SETS is rebuilt and its selection reset); otherwise a re-render
    /// after a format change keeps the selection.
    void render(bool fresh);
    void render_kpis();
    void render_surface();
    void render_top_sets(int pinned_index = -1);
    void render_overfitting();
    void render_holdout();
    void render_stability();
    void render_notices();
    void select_result_row(int index);
    void select_surface_cell(int row, int col);
    void act_on_selection(bool run);
    void update_actions();
    int selected_index() const;

    services::backtest::BtOptimizePtr result_;
    QString ccy_;
    QString engine_;
    bool loading_ = false; ///< combos being refilled: no re-render per change
    QStringList axis_params_;   ///< numeric varying parameters (surface axes)
    QStringList table_params_;  ///< parameter columns of TOP SETS
    services::backtest::BtSurface surface_;
    QString surface_metric_;
    bool surface_higher_better_ = true;

    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    BtAnalysisNotices* notices_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    ui::StatTile* t_best_ = nullptr;
    ui::StatTile* t_set_ = nullptr;
    ui::StatTile* t_runs_ = nullptr;
    ui::StatTile* t_dsr_ = nullptr;
    ui::StatTile* t_emax_ = nullptr;
    ui::StatTile* t_pbo_ = nullptr;
    ui::StatTile* t_hold_ = nullptr;
    ui::StatTile* t_drop_ = nullptr;

    BtPanel surface_panel_;
    QComboBox* x_combo_ = nullptr;
    QComboBox* y_combo_ = nullptr;
    QComboBox* metric_combo_ = nullptr;
    QStackedWidget* surface_stack_ = nullptr;
    ui::SensitivityTable* surface_table_ = nullptr;
    ui::BarChart* surface_bars_ = nullptr;
    QLabel* surface_none_ = nullptr;
    QLabel* surface_note_ = nullptr;

    BtPanel over_panel_;
    QLabel* over_chip_ = nullptr;
    BtKvGrid* over_kv_ = nullptr;
    QLabel* over_reading_ = nullptr;
    QLabel* over_method_ = nullptr;

    BtPanel hold_panel_;
    BtKvGrid* hold_kv_ = nullptr;
    QTableView* hold_view_ = nullptr;
    ui::LiveTableModel<BtOptHoldRow>* hold_model_ = nullptr;
    QLabel* hold_note_ = nullptr;

    BtPanel top_panel_;
    QTableView* top_view_ = nullptr;
    ui::LiveTableModel<BtOptTopRow>* top_model_ = nullptr;
    QSortFilterProxyModel* top_proxy_ = nullptr;
    ui::BarCellDelegate* top_bar_ = nullptr;
    int top_bar_column_ = -1;
    QPushButton* apply_btn_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* top_note_ = nullptr;

    BtPanel stab_panel_;
    ui::BarChart* spread_chart_ = nullptr;
    QLabel* plateau_hero_ = nullptr;
    QLabel* plateau_text_ = nullptr;
    BtKvGrid* sens_kv_ = nullptr;
    QLabel* stab_chip_ = nullptr;
};

// ── WALK-FWD ────────────────────────────────────────────────────────────────

class BtWalkForwardView : public QWidget {
    Q_OBJECT
  public:
    explicit BtWalkForwardView(QWidget* parent = nullptr);
    ~BtWalkForwardView() override;
    void show_result(const services::backtest::BtWalkForwardPtr& result, const QString& ccy, const QString& engine_label);

  private:
    void build_ui();
    void render();
    void render_kpis();
    void render_folds();
    void render_equity();
    void render_windows();
    void render_stability();
    void render_param_chart();
    void window_selected();
    void refit();

    services::backtest::BtWalkForwardPtr result_;
    QString ccy_;
    QString engine_;

    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    BtAnalysisNotices* notices_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    ui::StatTile* t_ret_ = nullptr;
    ui::StatTile* t_sharpe_ = nullptr;
    ui::StatTile* t_prof_ = nullptr;
    ui::StatTile* t_eff_ = nullptr;
    ui::StatTile* t_drop_ = nullptr;
    ui::StatTile* t_cov_ = nullptr;
    ui::StatTile* t_mode_ = nullptr;

    BtPanel folds_panel_;
    ui::TimeSeriesChart* folds_ = nullptr;
    BtPanel equity_panel_;
    ui::TimeSeriesChart* equity_ = nullptr;
    BtPanel inout_panel_;
    ui::BarChart* inout_ = nullptr;

    BtPanel windows_panel_;
    QTableView* windows_view_ = nullptr;
    ui::LiveTableModel<services::backtest::BtWfWindow>* windows_model_ = nullptr;
    QSortFilterProxyModel* windows_proxy_ = nullptr;

    BtPanel stab_panel_;
    QLabel* stab_chip_ = nullptr;
    QTableView* stab_view_ = nullptr;
    ui::LiveTableModel<BtWfParamRow>* stab_model_ = nullptr;
    ui::BarChart* param_chart_ = nullptr;
    QLabel* param_title_ = nullptr;
    QLabel* stab_note_ = nullptr;
};

// ── MONTE CARLO ─────────────────────────────────────────────────────────────

class BtMonteCarloView : public QWidget {
    Q_OBJECT
  public:
    explicit BtMonteCarloView(QWidget* parent = nullptr);
    ~BtMonteCarloView() override;
    void show_result(const services::backtest::BtMonteCarloPtr& result, const QString& ccy, const QString& engine_label);

  private:
    void build_ui();
    void render();
    void render_kpis();
    void render_fan();
    void render_paths();
    void render_percentiles();
    void render_distributions();
    void render_notices();
    /// Calendar years of the simulated horizon (x_dates, else the provenance bars); NaN when unknown.
    double years() const;

    services::backtest::BtMonteCarloPtr result_;
    QString ccy_;
    QString engine_;
    int fan_mode_ = 0; ///< 0 = FAN, 1 = PATHS

    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    BtAnalysisNotices* notices_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    ui::StatTile* t_med_ = nullptr;
    ui::StatTile* t_p5_ = nullptr;
    ui::StatTile* t_p95_ = nullptr;
    ui::StatTile* t_loss_ = nullptr;
    ui::StatTile* t_ruin_ = nullptr;
    ui::StatTile* t_dd50_ = nullptr;
    ui::StatTile* t_dd95_ = nullptr;
    ui::StatTile* t_orig_ = nullptr;

    BtPanel fan_panel_;
    QWidget* fan_seg_ = nullptr;
    QStackedWidget* fan_stack_ = nullptr;
    ui::FanChart* fan_ = nullptr;
    ui::TimeSeriesChart* paths_ = nullptr;
    QLabel* fan_note_ = nullptr;

    BtPanel pct_panel_;
    QTableView* pct_view_ = nullptr;
    ui::LiveTableModel<BtMcPctRow>* pct_model_ = nullptr;
    BtKvGrid* sim_kv_ = nullptr;

    BtPanel final_panel_;
    QStackedWidget* final_stack_ = nullptr;
    ui::DistributionChart* final_chart_ = nullptr;
    QLabel* final_note_ = nullptr;
    BtPanel dd_panel_;
    ui::DistributionChart* dd_chart_ = nullptr;
};

// ── COMPARE ─────────────────────────────────────────────────────────────────

struct BtCompareEntry {
    qint64 id = 0;
    QString label;
    QString as_of;
    QString engine;
    bool baseline = false;
    services::backtest::BtRunPtr run;
    QJsonObject inputs; ///< the request JSON of that run
};

class BtCompareView : public QWidget {
    Q_OBJECT
  public:
    explicit BtCompareView(QWidget* parent = nullptr);
    ~BtCompareView() override;
    /// 2..6 runs; baseline = the flagged one, else the first. Fewer than 2 shows the empty state.
    void show_runs(const QVector<BtCompareEntry>& runs);

  signals:
    void remove_requested(qint64 id);
    void baseline_requested(qint64 id);
    void add_requested();           ///< "+ ADD RUN"
    void open_requested(qint64 id); ///< make that run the active run

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    struct ChipSlot {
        QWidget* unit = nullptr;
        QPushButton* chip = nullptr;
        QMenu* menu = nullptr;
        QLabel* warn = nullptr;
    };

    void build_ui();
    void render();
    void render_chips();
    void render_equity();
    void render_metrics();
    void render_differs();
    void reflow_chips(bool force);
    void refresh_swatches();
    void refit();
    int baseline_index() const;

    QVector<BtCompareEntry> runs_;
    int baseline_ = 0;

    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    BtAnalysisNotices* notices_ = nullptr;

    BtPanel chips_panel_;
    QWidget* chips_box_ = nullptr;
    QVector<ChipSlot> slots_;
    QPushButton* add_btn_ = nullptr;
    int chip_cols_ = -1;

    BtPanel equity_panel_;
    ui::TimeSeriesChart* equity_ = nullptr;
    QLabel* equity_note_ = nullptr;

    BtPanel metrics_panel_;
    QTableView* metrics_view_ = nullptr;
    ui::LiveTableModel<BtCmpMetricRow>* metrics_model_ = nullptr;

    BtPanel diff_panel_;
    QTableView* diff_view_ = nullptr;
    ui::LiveTableModel<BtCmpDiffRow>* diff_model_ = nullptr;
    QLabel* diff_note_ = nullptr;
};

// ── Shared helpers of the four views (defined in BtOptimizeView.cpp) ───────

/// Panels side by side that stack vertically when the row is narrower than `wide_chars` average
/// characters of its font (so a large font re-flows earlier). Not a Q_OBJECT.
class BtAnalysisReflow : public QWidget {
  public:
    explicit BtAnalysisReflow(QWidget* parent = nullptr, int wide_chars = 118);
    void add(QWidget* w, int stretch = 1);
    bool is_wide() const { return wide_ == 1; }

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void reflow(int width, bool force);
    QBoxLayout* box_ = nullptr;
    QVector<int> stretch_;
    int wide_chars_ = 118;
    int wide_ = -1;
};

/// One notice line (warning / caveat) above a view.
struct BtAnalysisNotice {
    QString text;
    Tone tone = Tone::Dim;
};

/// A stack of #btNote lines, labels reused across set_notices calls; hidden when empty. Not a Q_OBJECT.
class BtAnalysisNotices : public QWidget {
  public:
    explicit BtAnalysisNotices(QWidget* parent = nullptr);
    void set_notices(const QVector<BtAnalysisNotice>& notices);

  private:
    QVBoxLayout* box_ = nullptr;
    QVector<QLabel*> labels_;
};

/// Notices from a result's warnings[] (most severe first, as parsed).
QVector<BtAnalysisNotice> bt_analysis_warning_notices(const QVector<services::backtest::BtWarning>& warnings);
/// A vertical #btScroll page; *body receives its content layout (margins 6, spacing 6).
QScrollArea* bt_analysis_page(QWidget* parent, QVBoxLayout** body);
/// Right-click menu (copy image / save PNG / optional CSV) on a custom-painted chart.
void bt_analysis_chart_menu(QWidget* chart, const QString& export_name,
                            std::function<void(QStringList*, QVector<QStringList>*)> csv = {});
/// Minimum height of `view` = header + `rows` rows (from the font); re-applied on appearance_settled.
void bt_analysis_fit_table(QTableView* view, int rows);
/// Minimum height of a chart = `lines` text lines of its font; re-applied on appearance_settled.
void bt_analysis_fit_chart(QWidget* chart, double lines);
/// A parameter value: "10", "0.25", "1.5".
QString bt_analysis_param_text(double v);
/// A JSON value as short text (numbers via bt_analysis_param_text, bools on / off, arrays joined).
QString bt_analysis_json_text(const QJsonValue& v);
/// "fastPeriod 10 · slowPeriod 100" (`order` first, then the remaining keys; `names` = with names).
QString bt_analysis_params_text(const QJsonObject& params, const QStringList& order = {}, bool names = true);

} // namespace fincept::screens::bt
