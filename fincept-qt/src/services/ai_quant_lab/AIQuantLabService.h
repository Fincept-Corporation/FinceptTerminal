// src/services/ai_quant_lab/AIQuantLabService.h
#pragma once
#include "services/ai_quant_lab/AIQuantLabTypes.h"

#include <QHash>
#include <QObject>

#include <functional>

namespace fincept::services::quant {

/// Singleton service for AI Quant Lab — dispatches to 18+ Python modules.
class AIQuantLabService : public QObject {
    Q_OBJECT
  public:
    static AIQuantLabService& instance();

    // ── Generic module execution ────────────────────────────────────────────
    /// Run any quant module by ID with JSON params
    void run_module(const QString& module_id, const QString& command, const QJsonObject& params);

    // ── Qlib core ───────────────────────────────────────────────────────────
    void list_models();
    void get_factor_library();
    void train_model(const QJsonObject& params);
    void run_backtest(const QJsonObject& params);
    void optimize_portfolio(const QJsonObject& params);

    // ── RL Trading ──────────────────────────────────────────────────────────
    void train_rl_agent(const QJsonObject& params);
    void evaluate_rl_agent(const QJsonObject& params);

    // ── GS Quant ────────────────────────────────────────────────────────────
    void gs_risk_metrics(const QJsonObject& params);
    void gs_portfolio_analytics(const QJsonObject& params);
    void gs_greeks(const QJsonObject& params);
    void gs_var_analysis(const QJsonObject& params);
    void gs_stress_test(const QJsonObject& params);
    void gs_backtest(const QJsonObject& params);
    void gs_statistics(const QJsonObject& params);

    // ── CFA Quant ───────────────────────────────────────────────────────────
    void cfa_trend_analysis(const QJsonObject& params);
    void cfa_stationarity_test(const QJsonObject& params);
    void cfa_arima(const QJsonObject& params);
    void cfa_ml_prediction(const QJsonObject& params);
    void cfa_pattern_discovery(const QJsonObject& params);
    void cfa_bootstrap(const QJsonObject& params);
    void cfa_data_quality(const QJsonObject& params);

    // ── Deep Agent (LangGraph multi-agent) ──────────────────────────────────
    void run_deep_agent(const QJsonObject& params);

    // ── RD-Agent (autonomous factor/model research) ──────────────────────────
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
    void list_advanced_models();
    void create_advanced_model(const QJsonObject& params);
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
    void strategy_list();

    // ── Data Processors ─────────────────────────────────────────────────────
    void dataproc_list_processors();
    void dataproc_create_pipeline(const QJsonObject& params);
    void dataproc_process_data(const QJsonObject& params);

    // ── Quant Reporting ─────────────────────────────────────────────────────
    void reporting_ic_analysis(const QJsonObject& params);
    void reporting_model_performance(const QJsonObject& params);
    void reporting_cumulative_returns(const QJsonObject& params);

  signals:
    void result_ready(QString module_id, QString command, QJsonObject data);
    void error_occurred(QString module_id, QString message);

  private:
    explicit AIQuantLabService(QObject* parent = nullptr);
    Q_DISABLE_COPY(AIQuantLabService)

    void run_python(const QString& script, const QStringList& args, const QString& module_id, const QString& command);
    void run_python_cached(const QString& script, const QStringList& args, const QString& module_id,
                           const QString& command, int ttl_sec);

    static constexpr int kListTtlSec = 5 * 60;

    // Module ID → script mapping
    QHash<QString, QString> script_map_;
};

} // namespace fincept::services::quant
