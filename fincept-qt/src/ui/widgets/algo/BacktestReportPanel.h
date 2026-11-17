// src/ui/widgets/algo/BacktestReportPanel.h
#pragma once
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QAbstractTableModel;
class QLabel;
class QProgressBar;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTableView;

namespace fincept::ui {
class TimeSeriesChart;
class CandleChart;
class MonthlyReturnsHeatmap;
class DistributionChart;
class EmptyState;
class KpiStrip;
class KeySortProxy;
} // namespace fincept::ui

namespace fincept::ui::algo {

/// Renders a BacktestEngine result in the Algo tab's dense style (object names of the Algo kit's
/// root stylesheet; no setStyleSheet):
///
///   SPY · NYSE · 1d · long · 2023-11-18 → 2026-11-16 · 750 bars · net of costs   [⚠ 2 WARNINGS]
///   [NET RETURN][CAGR][SHARPE][MAX DD][WIN RATE][PROFIT FACTOR][TRADES][EXPOSURE][VS BUY & HOLD]
///   ┌ equity (net) vs buy & hold, trade markers ─────────────────────────────────────────────┐
///   └ underwater (drawdown %) pane ─────────────────────────────────────────────────────────┘
///   TRADES · MONTHLY · COSTS · ASSUMPTIONS / WARNINGS · PRICE · DISTRIBUTION
///
/// The trade list is a model/view table (sortable, CSV export with plain numbers, ISO times in
/// the venue's zone and a currency column). KPI tiles open the tab that explains them.
///
/// States: idle (an EmptyState with a RUN action → run_requested()), queued (waiting for its
/// turn — no progress bar), loading (an indeterminate bar until the first progress report,
/// then the percentage, + CANCEL — the previous report is hidden, never shown as if it were the
/// new one), error (the message) and the report itself, whose header says when the run happened
/// and how long it took.
class BacktestReportPanel : public QWidget {
    Q_OBJECT
  public:
    explicit BacktestReportPanel(QWidget* parent = nullptr);

    /// A fresh result: its run time is measured from the last set_loading().
    void set_result(const QJsonObject& payload);
    /// A result kept from an earlier run (restored with its draft): shown like a fresh
    /// one, with "as of `ran_at`" and the run time it took (`duration_ms` < 0 = unknown).
    void set_remembered_result(const QJsonObject& payload, const QDateTime& ran_at, qint64 duration_ms);
    /// Back to the idle page (EmptyState with a RUN action).
    void clear();
    /// A run is waiting for its turn (e.g. behind another automatic load): `what`,
    /// without a progress bar. The previous report is hidden.
    void set_queued(const QString& what);
    /// A run started: hides the previous report and shows `what` with an
    /// indeterminate bar until set_progress() reports a percentage above 0.
    void set_loading(const QString& what);
    void set_progress(int percent, const QString& stage);
    /// The run failed: hides the previous report and shows the reason.
    void set_error(const QString& message);
    /// The run was cancelled.
    void set_cancelled();

    bool has_result() const { return has_result_; }
    const QJsonObject& result() const { return last_payload_; }
    /// When the result on screen was produced (local time; invalid = none).
    QDateTime ran_at() const { return ran_at_; }
    /// Wall-clock time from set_loading() to set_result() of the result on screen; -1 = unknown.
    qint64 run_duration_ms() const { return run_duration_ms_; }
    /// The result on screen came from set_remembered_result().
    bool is_remembered() const { return remembered_; }
    /// "Ran 14:02 · took 1.2 s" / "As of … (remembered)" — empty without a result.
    QString run_info_text() const;

  signals:
    void cancel_requested();
    /// The idle page's RUN action: the host starts a run through its usual path.
    void run_requested();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    QWidget* build_state_page();
    QWidget* build_report_page();
    QWidget* build_idle_page();
    QWidget* build_trades_page(QWidget* parent);
    QWidget* build_costs_page(QWidget* parent);
    QWidget* build_assumptions_page(QWidget* parent);
    QWidget* build_distribution_page(QWidget* parent);
    void show_state(const QString& title, const QString& detail, bool busy, bool error);
    void apply_idle_spec();
    void render_result();
    void populate_header();
    void populate_kpis();
    void populate_equity();
    void populate_price();
    void populate_monthly();
    void populate_distribution();
    void populate_trades();
    void populate_costs();
    void populate_assumptions();
    void export_trades_csv();
    /// The trades as machine-readable CSV: plain numbers, ISO 8601 times in the venue's
    /// zone and a currency column (never the on-screen text).
    void trades_export(QStringList* headers, QVector<QStringList>* rows) const;
    void retranslateUi();
    void show_tab(int index);
    void fit_to_font();

    QString money(double v, bool signed_value = false) const;
    QString price_text(double v) const;
    QString time_text(double epoch_ms) const;
    QString reason_text(const QString& reason) const;

    QStackedWidget* stack_ = nullptr;

    // Idle page
    fincept::ui::EmptyState* idle_ = nullptr;

    // State page
    QLabel* state_title_ = nullptr;
    QLabel* state_detail_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    bool state_is_error_ = false;
    bool busy_ = false;        // loading: progress bar + Cancel shown
    bool determinate_ = false; // the bar shows a percentage (a report above 0 arrived)

    // Run time of the result on screen
    QElapsedTimer run_clock_; // started by set_loading()
    QDateTime ran_at_;
    qint64 run_duration_ms_ = -1;
    bool remembered_ = false;

    // Report page
    QLabel* header_ = nullptr;
    QLabel* run_info_ = nullptr;
    QPushButton* warnings_btn_ = nullptr;
    fincept::ui::KpiStrip* kpis_ = nullptr;
    QSplitter* split_ = nullptr;
    fincept::ui::TimeSeriesChart* equity_chart_ = nullptr;
    QTabBar* tabs_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    fincept::ui::CandleChart* price_chart_ = nullptr;
    fincept::ui::MonthlyReturnsHeatmap* heatmap_ = nullptr;
    QLabel* dist_caption_ = nullptr;
    fincept::ui::DistributionChart* dist_ = nullptr;
    QLabel* trades_count_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QTableView* trades_ = nullptr;
    QAbstractTableModel* trades_model_ = nullptr; ///< ui::LiveTableModel<row> (defined in the .cpp)
    fincept::ui::KeySortProxy* trades_proxy_ = nullptr;
    QWidget* costs_grid_ = nullptr;
    QWidget* assumptions_grid_ = nullptr;
    QLabel* warnings_list_ = nullptr;
    QLabel* assumptions_note_ = nullptr;
    bool shown_once_ = false;

    QJsonObject last_payload_; // the result on screen
    bool has_result_ = false;
    QString currency_; // the run's quote currency ("GBp" for LSE); empty = unknown: no unit printed
    bool daily_bars_ = true;
};

} // namespace fincept::ui::algo
