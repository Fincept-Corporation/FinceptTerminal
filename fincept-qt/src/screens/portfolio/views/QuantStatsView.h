// src/screens/portfolio/views/QuantStatsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAnalyticsService.h"

#include <QComboBox>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

#include <functional>

class QScrollArea;

namespace fincept::ui {
class AutoLoadPane;
class DistributionChart;
class EmptyState;
class FanChart;
class InputMemory;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens {

class PortfolioSharedRun;

/// QuantStats analytics view — 5 tabs: METRICS, RETURNS, DRAWDOWN, ROLLING, MONTE CARLO.
/// METRICS / RETURNS / DRAWDOWN / ROLLING show one quantstats_analysis.py run
/// (PortfolioSharedRun: whichever of them is shown first starts it, the others
/// join it or open on its result); MONTE CARLO has its own run
/// (quantstats_monte_carlo.py). Both run on first show (class B: free, local
/// Python + a price download, a few seconds), are remembered per portfolio and
/// holdings ("as of"), and refresh quietly when older than
/// kPortfolioScriptMaxAgeS. All Python calls go through PortfolioAnalyticsService.
class QuantStatsView : public QWidget {
    Q_OBJECT
  public:
    explicit QuantStatsView(QWidget* parent = nullptr);
    ~QuantStatsView() override;

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  signals:
    /// The empty-book state's BUY action.
    void add_position_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// One result tab: its AutoLoadPane, and the empty-book state beside it.
    struct ResultTab {
        QStackedWidget* stack = nullptr; ///< pane | empty-book state
        ui::AutoLoadPane* pane = nullptr;
        ui::EmptyState* empty = nullptr; ///< created on first need
        std::function<QString()> empty_text;
        std::function<void(const QJsonValue&)> render;
    };

    void build_ui();
    QWidget* build_mc_page(QWidget* parent);
    void retranslateUi();
    /// Re-renders the visible tab after an appearance change (table items and
    /// built charts carry their colours); the others re-render on their next show.
    void on_appearance_settled();
    void rerender_if_stale(ResultTab& tab);

    void configure_pane(ResultTab& tab, int shape, const std::function<void(const QJsonValue&)>& render);
    void set_empty_book(bool empty);
    void on_tab_changed(int index);
    ResultTab* tab_for_index(int index);
    /// The QuantStats pane of the current tab (METRICS when MONTE CARLO is current).
    ui::AutoLoadPane* current_qs_pane();

    // Per-tab renderers — read qs_data_ / mc_data_
    void update_metrics();
    void update_returns();
    void update_drawdown();
    void update_rolling();
    void update_monte_carlo_chart();

    // Runs
    void launch_quantstats(quint64 run_id);
    void start_monte_carlo(quint64 request_id);
    void on_period_activated();
    QString period_key() const;
    QString qs_loading_text() const;
    QString mc_loading_text() const;
    QString qs_signature() const;
    QString mc_signature() const;

    // ── Tab container ─────────────────────────────────────────────────────────
    QTabWidget* tabs_ = nullptr;
    int metrics_tab_index_ = -1;
    int returns_tab_index_ = -1;
    int drawdown_tab_index_ = -1;
    int rolling_tab_index_ = -1;
    int mc_tab_index_ = -1;

    // ── Header controls ───────────────────────────────────────────────────────
    QLabel* title_lbl_ = nullptr;
    QLabel* sibling_lbl_ = nullptr;
    QPushButton* qs_run_btn_ = nullptr;
    QComboBox* qs_period_cb_ = nullptr; ///< history window of the analysis (6mo … max)
    QLabel* qs_status_ = nullptr;       ///< "as of …" of the QuantStats tab on screen
    ui::AutoLoadPane* header_pane_ = nullptr; ///< the QuantStats pane qs_status_ follows
    QLabel* metrics_section_lbl_ = nullptr;
    QLabel* mc_title_ = nullptr;
    QLabel* mc_desc_ = nullptr;
    QLabel* paths_lbl_ = nullptr;

    // ── Result tabs ───────────────────────────────────────────────────────────
    ResultTab metrics_tab_;
    ResultTab returns_tab_;
    ResultTab drawdown_tab_;
    ResultTab rolling_tab_;
    ResultTab mc_tab_;

    QTableWidget* metrics_table_ = nullptr;
    QScrollArea* returns_scroll_ = nullptr;
    QScrollArea* drawdown_scroll_ = nullptr;
    QScrollArea* rolling_scroll_ = nullptr;

    // ── MONTE CARLO tab ───────────────────────────────────────────────────────
    QPushButton* mc_run_btn_ = nullptr;
    QSpinBox* mc_paths_spin_ = nullptr; ///< number of simulated paths
    QComboBox* mc_method_cb_ = nullptr; ///< bootstrap / gbm
    QLabel* mc_status_ = nullptr;       ///< "as of …" of the simulation
    ui::KpiStrip* mc_kpis_ = nullptr;
    ui::FanChart* mc_fan_ = nullptr;
    ui::DistributionChart* mc_dist_ = nullptr; ///< terminal returns, when the script sends them
    QLabel* mc_caption_ = nullptr;

    // ── Runs ──────────────────────────────────────────────────────────────────
    PortfolioSharedRun* qs_shared_ = nullptr;
    QHash<quint64, services::AnalyticsRequest> qs_requests_; ///< shared-run id -> script request
    QHash<quint64, services::AnalyticsRequest> mc_requests_; ///< pane request id -> script request
    ui::InputMemory* qs_inputs_ = nullptr;
    ui::InputMemory* mc_inputs_ = nullptr;
    QSet<ui::AutoLoadPane*> theme_stale_;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString currency_;
    QJsonObject qs_data_; // the quantstats_analysis.py result being rendered
    QJsonObject mc_data_; // the quantstats_monte_carlo.py result being rendered
    bool has_data_ = false;
    bool empty_book_ = false;
};

} // namespace fincept::screens
