#pragma once
// AutomationRunsWidget — read-only status of algo deployments and workflow runs.
//
// Sources (no polling timer, D3):
//   * Algo deployments — AlgoEngine::list_deployments() → deployments_loaded
//     (algo_deployments + algo_metrics tables), re-listed on deployment_started /
//     deployment_stopped; live P&L and state patched in place from the hub's
//     push-only `algo:metrics:*` / `algo:state:*` topics.
//   * Workflow runs — WorkflowService::list_recent_runs() → recent_runs_listed
//     (latest run per workflow from workflow_audit_log), re-listed on
//     execution_started / execution_finished.
//
// Deliberately has no start / stop / order controls. Double-click a row to open
// Algo Trading or the Node Editor.

#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "services/workflow/WorkflowService.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVector>

class QTableWidget;

namespace fincept::screens::widgets {

class AutomationRunsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit AutomationRunsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;

  private:
    enum class Kind { Algo, Workflow };

    /// One row — raw data only; text and tooltips are built in render() so a
    /// language switch re-translates them.
    struct RunRow {
        Kind kind = Kind::Algo;
        QString id;    ///< deployment id / workflow id
        QString name;  ///< strategy name / workflow name (empty = unsaved or deleted workflow)
        QString state; ///< running | starting | paused | stopped | succeeded | failed
        bool live = false;
        QDateTime last_run;       ///< local time; invalid when unknown
        QString note;             ///< algo: error message; workflow: audit details
        bool interrupted = false; ///< workflow started but never finished
        // Algo only
        QString symbol;
        QString timeframe;
        bool has_pnl = false;
        double realized = 0;
        double unrealized = 0;
        int trades = 0;
        double win_rate = 0; ///< percent
        QString currency;    ///< broker's ISO currency; empty → global currency
    };

    void apply_styles();
    void request_all();
    void hub_subscribe();
    void render();

    void on_deployments_loaded(const QVector<fincept::services::algo::AlgoDeployment>& deployments);
    void on_workflow_runs(const QVector<fincept::workflow::WorkflowService::RunSummary>& runs);
    void on_algo_metrics(const QString& topic, const QVariant& value);
    void on_algo_state(const QString& topic, const QVariant& value);
    void finish_loading_if_done();

    QString state_label(const QString& state) const;
    QString state_color(const QString& state) const;

    QWidget* root_ = nullptr;
    QLabel* status_label_ = nullptr;
    QTableWidget* table_ = nullptr;

    QVector<RunRow> algo_rows_;
    QVector<RunRow> workflow_rows_;
    QHash<QString, int> algo_index_; ///< deployment id → index in algo_rows_

    bool algo_pending_ = false;
    bool workflow_pending_ = false;
    bool have_algo_ = false;
    bool have_workflows_ = false;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
