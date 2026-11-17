// src/screens/portfolio/views/StressTestPanel.h
#pragma once
#include "screens/portfolio/PortfolioStress.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAnalyticsService.h"

#include <QJsonObject>
#include <QWidget>

class QDateEdit;
class QLabel;
class QPushButton;
class QTableWidget;

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

/// Stress testing on the current book (PortfolioStress.h engine):
///   * every preset and user-defined scenario, sorted by impact, with a bar per
///     scenario and the FX part of the loss;
///   * the selected scenario per holding (local / FX / total move, P&L bars);
///   * user scenarios: create, duplicate a preset, edit, delete (persisted in settings);
///   * historical replay of a scenario's real window, or any window, on actual
///     prices (PortfolioService::historical_stress) with the path chart.
///
/// The replay is never a hidden strip: on open the last-used scenario of the book
/// is selected again (else the first preset with a historical window) and its
/// replay runs by itself (ui::AutoLoadPane, AutoRun — a past window is
/// deterministic, so a remembered replay is shown at once and never re-run);
/// a hypothetical scenario says it has no window to replay.
class StressTestPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StressTestPanel(QWidget* parent = nullptr);
    ~StressTestPanel() override;

    void set_data(const portfolio::PortfolioSummary& summary);
    void set_analytics(const portfolio::MarketAnalytics& analytics);

  private:
    void build_ui();
    void load_user_scenarios();
    void save_user_scenarios() const;
    void render();
    void render_detail();
    void edit_scenario(int index, bool duplicate);
    void delete_scenario(int index);
    int selected_index() const;

    /// Point the replay pane at the selected scenario (or the custom window).
    void update_replay_view();
    /// The replay pane's StartFn: historical_stress for the current view's window.
    void start_replay(quint64 request_id);
    void render_replay(const QJsonObject& d);
    /// Last scenario the user looked at for this book (settings), else the default.
    QString remembered_scenario(const QString& portfolio_id) const;
    void remember_scenario() const;

    QTableWidget* table_ = nullptr;
    ui::BarChart* impact_chart_ = nullptr;
    QLabel* detail_title_ = nullptr;
    ui::BarChart* holding_chart_ = nullptr;
    QTableWidget* detail_table_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* dup_btn_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* replay_btn_ = nullptr;
    QDateEdit* from_edit_ = nullptr;
    QDateEdit* to_edit_ = nullptr;
    QPushButton* run_window_btn_ = nullptr;

    // ── Historical replay strip ──
    QLabel* replay_title_ = nullptr;
    QLabel* replay_status_ = nullptr; ///< "as of …" kept by the pane
    ui::AutoLoadPane* replay_pane_ = nullptr;
    ui::KpiStrip* replay_kpis_ = nullptr;
    ui::TimeSeriesChart* hist_chart_ = nullptr;
    QLabel* hist_summary_ = nullptr;
    QString replay_start_;   ///< window of the pane's current view (YYYY-MM-DD)
    QString replay_end_;
    QString replay_label_;   ///< scenario name / window text of the current view
    bool custom_window_ = false; ///< the "Any window" RUN is on display
    QString custom_start_;
    QString custom_end_;
    services::AnalyticsRequest replay_req_; ///< replay in flight
    quint64 replay_req_for_ = 0;            ///< pane request id it serves

    portfolio::PortfolioSummary summary_;
    portfolio::MarketAnalytics analytics_;
    QVector<portfolio_stress::Scenario> scenarios_; ///< presets, then user scenarios
    int n_presets_ = 0;
    QVector<portfolio_stress::Result> results_;
    QString selected_id_;
    QString loaded_for_; ///< portfolio whose remembered scenario was restored
    bool has_data_ = false;
    /// A currency / number-format re-render: re-format only, leave the replay pane as it is.
    bool reformat_only_ = false;
};

} // namespace fincept::screens
