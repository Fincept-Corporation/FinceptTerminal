// src/services/ai_quant_lab/AIQuantLabService.h
#pragma once
#include "python/PythonRunner.h"
#include "services/ai_quant_lab/AIQuantLabTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QProcess>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::quant {

/// Singleton service for AI Quant Lab — dispatches to 18+ Python modules.
///
/// Request-scoped runs: while a RequestScope lives, every script the calls
/// below start is tagged with its request id and cancel token. A tagged run
/// answers through request_finished / request_failed only (never through
/// result_ready / error_occurred), so a reply reaches exactly the view that
/// asked for it, however late it arrives; raising the token stops the script
/// (queued: never spawned; running: killed). Untagged calls behave as before.
///
///   const quint64 id = AIQuantLabService::next_request_id();
///   auto cancel = std::make_shared<std::atomic<bool>>(false);
///   { AIQuantLabService::RequestScope scope(id, cancel); svc.gs_greeks(params); }
///   // later: request_finished(id, "gs_quant", "greeks", data)
class AIQuantLabService : public QObject {
    Q_OBJECT
  public:
    static AIQuantLabService& instance();

    /// A fresh, app-wide unique request id (never 0).
    static quint64 next_request_id();

    /// Tags the runs started on the GUI thread while it lives (see above).
    /// Scopes do not nest: an inner scope replaces the outer one until it ends.
    class RequestScope {
      public:
        RequestScope(quint64 request_id, std::shared_ptr<std::atomic<bool>> cancel);
        ~RequestScope();
        RequestScope(const RequestScope&) = delete;
        RequestScope& operator=(const RequestScope&) = delete;
        /// Scripts started (or answered from cache) under this scope so far.
        int dispatched() const;

      private:
        quint64 prev_id_ = 0;
        std::shared_ptr<std::atomic<bool>> prev_cancel_;
        int prev_dispatched_ = 0;
    };

    // ── Generic module execution ────────────────────────────────────────────
    /// Run any quant module by ID with JSON params
    void run_module(const QString& module_id, const QString& command, const QJsonObject& params);

    // ── Qlib core ───────────────────────────────────────────────────────────
    void run_backtest(const QJsonObject& params);

    // ── RL Trading ──────────────────────────────────────────────────────────
    void train_rl_agent(const QJsonObject& params);

    // ── GS Quant ────────────────────────────────────────────────────────────
    void gs_risk_metrics(const QJsonObject& params);
    void gs_portfolio_analytics(const QJsonObject& params);
    void gs_greeks(const QJsonObject& params);
    void gs_var_analysis(const QJsonObject& params);
    void gs_stress_test(const QJsonObject& params);
    void gs_backtest(const QJsonObject& params);
    void gs_statistics(const QJsonObject& params);
    void gs_component_var(const QJsonObject& params);

    // ── Functime ────────────────────────────────────────────────────────────
    void functime_forecast(const QJsonObject& params);
    void functime_anomaly_detection(const QJsonObject& params);
    void functime_seasonality(const QJsonObject& params);
    void functime_metrics(const QJsonObject& params);
    void functime_confidence_intervals(const QJsonObject& params);
    void functime_stationarity(const QJsonObject& params);

    // ── Statsmodels ─────────────────────────────────────────────────────────
    void sm_ols(const QJsonObject& params);
    void sm_arima(const QJsonObject& params);
    void sm_stationarity_tests(const QJsonObject& params);
    void sm_acf_pacf(const QJsonObject& params);
    void sm_granger_causality(const QJsonObject& params);
    void sm_descriptive(const QJsonObject& params);
    void sm_cointegration(const QJsonObject& params);
    void sm_rolling_ols(const QJsonObject& params);
    void sm_regime_switching(const QJsonObject& params);
    void sm_pca(const QJsonObject& params);
    void sm_decompose(const QJsonObject& params);

    // ── Fortitudo ───────────────────────────────────────────────────────────
    void fort_portfolio_metrics(const QJsonObject& params);
    void fort_covariance_matrix(const QJsonObject& params);
    void fort_mean_variance_optimize(const QJsonObject& params);
    void fort_mean_cvar_optimize(const QJsonObject& params);
    void fort_efficient_frontier(const QJsonObject& params);
    void fort_exp_decay_probabilities(const QJsonObject& params);

    // ── GluonTS ─────────────────────────────────────────────────────────────
    void gluon_probabilistic_forecast(const QJsonObject& params);
    void gluon_quantile_forecast(const QJsonObject& params);
    void gluon_distribution_fit(const QJsonObject& params);
    void gluon_evaluate_forecast(const QJsonObject& params);
    void gluon_seasonal_naive(const QJsonObject& params);

    // ── Deep Agent (LangGraph multi-agent) ──────────────────────────────────
    /// `params`: task, agent_type, thread_id, config (LLM block). The payload —
    /// LLM key included — goes to Python on stdin, never argv. Without a
    /// `config` that names a provider the ai_quant_lab LLM profile is used.
    void run_deep_agent(const QJsonObject& params);

    /// Deep Analysis tab probes: deepagents/cli.py check_status (is the library
    /// importable) and list_capabilities (agent types → subagents). No LLM, no
    /// key. Answered through `on_done` while `context` lives — never through
    /// result_ready / error_occurred, whose deep_agent handlers would paint the
    /// reply into the results pane and re-enable a run. `on_done` receives the
    /// script's JSON object, or {"success": false, "error": …} when the run
    /// failed. An unavailable library is a valid check_status answer:
    /// {"available": false, "error": "<why>"}.
    void deep_agent_check_status(QObject* context, std::function<void(const QJsonObject&)> on_done);
    void deep_agent_list_capabilities(QObject* context, std::function<void(const QJsonObject&)> on_done);

    // ── RD-Agent (autonomous factor/model research) ──────────────────────────
    // Long tasks (start_* / resume) run as one child process per task, owned by
    // this service: progress streams back as JSON lines, status/results are
    // served from memory, stop = kill. They live for this app session only.
    void rd_agent_check_status();
    void rd_agent_start_factor_mining(const QJsonObject& params);
    void rd_agent_start_model_optimization(const QJsonObject& params);
    void rd_agent_start_quant_research(const QJsonObject& params);
    void rd_agent_get_task_status(const QString& task_id);
    void rd_agent_get_discovered_factors(const QString& task_id);
    void rd_agent_get_optimized_model(const QString& task_id);
    void rd_agent_list_tasks(const QString& status_filter = {});
    void rd_agent_stop_task(const QString& task_id);
    void rd_agent_resume_task(const QString& task_id, const QJsonObject& config = {});
    void rd_agent_start_ui();
    void rd_agent_start_mcp_server(int port = 18765);
    void rd_agent_mcp_status();

    // ── Advanced ────────────────────────────────────────────────────────────
    void train_advanced_model(const QJsonObject& params);

    // ── Factor Discovery ────────────────────────────────────────────────────
    void factor_get_library();
    void factor_get_data(const QJsonObject& params);
    void factor_get_calendar(const QJsonObject& params);
    void factor_get_instruments();

    // ── Model Library ───────────────────────────────────────────────────────
    void model_list();
    void model_train(const QJsonObject& params);
    void model_backtest(const QJsonObject& params);
    void model_check_status();

    // ── Live Signals ────────────────────────────────────────────────────────
    void signals_get_data(const QJsonObject& params);
    void signals_get_factor_analysis(const QJsonObject& params);
    void signals_get_feature_importance(const QJsonObject& params);

    // ── HFT ─────────────────────────────────────────────────────────────────
    void hft_create_orderbook(const QJsonObject& params);
    void hft_snapshot(const QJsonObject& params);
    void hft_market_making_quotes(const QJsonObject& params);
    void hft_detect_toxic(const QJsonObject& params);
    void hft_execute_order(const QJsonObject& params);

    // ── Meta Learning ───────────────────────────────────────────────────────
    void meta_list_models();
    void meta_run_selection(const QJsonObject& params);
    void meta_create_ensemble(const QJsonObject& params);
    void meta_tune_hyperparameters(const QJsonObject& params);
    void meta_get_results();

    // ── Online Learning ─────────────────────────────────────────────────────
    void online_list_models();
    void online_create_model(const QJsonObject& params);
    void online_train(const QJsonObject& params);
    void online_predict(const QJsonObject& params);
    void online_performance(const QJsonObject& params);

    // ── Rolling Retraining ──────────────────────────────────────────────────
    void rolling_create_schedule(const QJsonObject& params);
    void rolling_execute_retrain(const QJsonObject& params);
    void rolling_list_schedules();
    void rolling_preview_tasks(const QJsonObject& params);
    void rolling_delete_schedule(const QJsonObject& params);

    // ── Feature Engineering ─────────────────────────────────────────────────
    void feature_compute(const QJsonObject& params);
    void feature_select_by_ic(const QJsonObject& params);
    void feature_evaluate_expression(const QJsonObject& params);

    // ── Portfolio Optimization ──────────────────────────────────────────────
    void portopt_run(const QString& method, const QJsonObject& params);

    // ── Factor Evaluation ───────────────────────────────────────────────────
    void evaluation_ic(const QJsonObject& params);
    void evaluation_report(const QJsonObject& params);
    void evaluation_risk_metrics(const QJsonObject& params);

    // ── Strategy Builder ────────────────────────────────────────────────────
    void strategy_create(const QString& type, const QJsonObject& params);
    void strategy_portfolio_metrics(const QJsonObject& params);

    // ── Data Processors ─────────────────────────────────────────────────────
    void dataproc_list_processors();
    void dataproc_create_pipeline(const QJsonObject& params);
    void dataproc_process_data(const QJsonObject& params);

    // ── Quant Reporting ─────────────────────────────────────────────────────
    void reporting_ic_analysis(const QJsonObject& params);
    void reporting_model_performance(const QJsonObject& params);
    void reporting_cumulative_returns(const QJsonObject& params);
    void reporting_risk_report(const QJsonObject& params);
    void reporting_factor_quantiles(const QJsonObject& params);

  signals:
    void result_ready(QString module_id, QString command, QJsonObject data);
    void error_occurred(QString module_id, QString message);
    /// Reply of a tagged run (RequestScope). `data` is the script's JSON object.
    void request_finished(quint64 request_id, QString module_id, QString command, QJsonObject data);
    /// A tagged run failed (process error, script error envelope, cancelled, timed out).
    void request_failed(quint64 request_id, QString module_id, QString message);
    /// A local qlib dataset was built, updated or downloaded (tagged or not):
    /// every dataset bar reads the list again.
    void datasets_changed(QString module_id, QString command, QJsonObject data);
    /// Emitted during a streaming training run (currently: rl_trading/train).
    void training_progress(QString module_id, int step, int total, double reward_mean, double loss);
    void training_log(QString module_id, QString line, bool is_stderr);

  private:
    explicit AIQuantLabService(QObject* parent = nullptr);
    Q_DISABLE_COPY(AIQuantLabService)

    void run_python(const QString& script, const QStringList& args, const QString& module_id, const QString& command,
                    const python::PythonRunner::RunOptions& opts = {});
    void run_python_cached(const QString& script, const QStringList& args, const QString& module_id,
                           const QString& command, int ttl_sec);

    /// The request the next run belongs to (0 = untagged), and its cancel token; counts the dispatch.
    struct Tag {
        quint64 id = 0;
        std::shared_ptr<std::atomic<bool>> cancel;
    };
    Tag take_tag();
    /// Completion of a run: tagged → request_finished / request_failed, else result_ready / error_occurred.
    void emit_done(const Tag& tag, const QString& module_id, const QString& command, const QJsonObject& data);
    void emit_failed(const Tag& tag, const QString& module_id, const QString& message);

    quint64 scope_id_ = 0;                          ///< RequestScope in force (GUI thread)
    std::shared_ptr<std::atomic<bool>> scope_cancel_;
    int scope_dispatched_ = 0;

    /// The LLM block for an agent run: the caller's `config` when it carries a
    /// provider, else the ai_quant_lab profile resolved from LlmProfileRepository.
    QJsonObject agent_llm_config(const QJsonObject& params) const;

    /// One deepagents/cli.py read command for the probes above.
    void deep_agent_query(const QString& command, QObject* context, std::function<void(const QJsonObject&)> on_done);

    // ── RD-Agent task registry ──────────────────────────────────────────────
    struct RdTask {
        QString id;
        QString kind;       // factor_mining | model_optimization | quant_research
        QString status;     // running | completed | failed | stopped
        QJsonObject params; // task parameters WITHOUT the LLM credentials
        QString workspace;
        double progress = 0.0; // 0..1
        int iterations_completed = 0;
        int max_iterations = 0;
        QString current_step;
        QJsonValue best_ic;
        QJsonValue best_sharpe;
        QJsonArray factors;
        QJsonArray models;
        QString error;
        qint64 started_ms = 0;
        qint64 finished_ms = 0;
        bool stop_requested = false;
        QPointer<QProcess> proc;
    };

    void rd_start(const QString& command, const QString& kind, const QJsonObject& params);
    /// Spawn `cli.py run_task` for `task_id`; `resume_from` = checkpoint path or empty.
    bool rd_spawn(const QString& task_id, const QJsonObject& llm_config, const QString& resume_from,
                  QString* error_out);
    void rd_handle_line(const QString& task_id, const QByteArray& line);
    QJsonObject rd_status_json(const RdTask& t) const;
    int rd_running_count() const;
    /// result_ready("deep_agent", command, data), queued so callers that connect
    /// after calling (and screens that update their status text after calling)
    /// see it.
    void rd_emit(const QString& command, const QJsonObject& data);

    static constexpr int kListTtlSec = 5 * 60;
    static constexpr int kMaxConcurrentRdTasks = 2;

    // Module ID → script mapping
    QHash<QString, QString> script_map_;
    QHash<QString, RdTask> rd_tasks_;
};

} // namespace fincept::services::quant
