// src/services/ai_quant_lab/AIQuantLabService.cpp
#include "services/ai_quant_lab/AIQuantLabService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

namespace fincept::services::quant {

// ── Singleton ────────────────────────────────────────────────────────────────
AIQuantLabService& AIQuantLabService::instance() {
    static AIQuantLabService inst;
    return inst;
}

AIQuantLabService::AIQuantLabService(QObject* parent) : QObject(parent) {
    // Build script map from module definitions
    for (const auto& mod : all_quant_modules())
        script_map_[mod.id] = mod.script;
}

// ── Python helper ────────────────────────────────────────────────────────────
void AIQuantLabService::run_python(const QString& script, const QStringList& args, const QString& module_id,
                                   const QString& command) {
    QPointer<AIQuantLabService> self = this;
    python::PythonRunner::instance().run(script, args, [self, module_id, command](python::PythonResult result) {
        if (!self)
            return;
        if (!result.success) {
            LOG_ERROR("AIQuantLab", QString("[%1/%2] Failed: %3").arg(module_id, command, result.error));
            emit self->error_occurred(module_id, result.error);
            return;
        }
        auto json_str = python::extract_json(result.output);
        auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (doc.isNull()) {
            LOG_ERROR("AIQuantLab", QString("[%1/%2] Invalid JSON").arg(module_id, command));
            emit self->error_occurred(module_id, "Invalid JSON response");
            return;
        }
        LOG_INFO("AIQuantLab", QString("[%1/%2] Result ready").arg(module_id, command));
        emit self->result_ready(module_id, command, doc.object());
    });
}

void AIQuantLabService::run_python_cached(const QString& script, const QStringList& args, const QString& module_id,
                                          const QString& command, int ttl_sec) {
    const QString cache_key = "aiquant:" + module_id + ":" + command;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull()) {
            LOG_DEBUG("AIQuantLab", QString("[%1/%2] Cache hit").arg(module_id, command));
            emit result_ready(module_id, command, doc.object());
            return;
        }
    }

    QPointer<AIQuantLabService> self = this;
    python::PythonRunner::instance().run(
        script, args, [self, module_id, command, cache_key, ttl_sec](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_ERROR("AIQuantLab", QString("[%1/%2] Failed: %3").arg(module_id, command, result.error));
                emit self->error_occurred(module_id, result.error);
                return;
            }
            auto json_str = python::extract_json(result.output);
            auto doc = QJsonDocument::fromJson(json_str.toUtf8());
            if (doc.isNull()) {
                LOG_ERROR("AIQuantLab", QString("[%1/%2] Invalid JSON").arg(module_id, command));
                emit self->error_occurred(module_id, "Invalid JSON response");
                return;
            }
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(doc.object()).toJson(QJsonDocument::Compact))),
                ttl_sec, "ai_quant_lab");
            LOG_INFO("AIQuantLab", QString("[%1/%2] Result ready").arg(module_id, command));
            emit self->result_ready(module_id, command, doc.object());
        });
}

// ── Generic module execution ─────────────────────────────────────────────────
void AIQuantLabService::run_module(const QString& module_id, const QString& command, const QJsonObject& params) {
    auto it = script_map_.find(module_id);
    if (it == script_map_.end()) {
        emit error_occurred(module_id, "Unknown module: " + module_id);
        return;
    }
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(*it, {command, json}, module_id, command);
}

// ── Qlib core ────────────────────────────────────────────────────────────────
void AIQuantLabService::list_models() {
    run_python_cached("ai_quant_lab/qlib_service.py", {"list_models"}, "model_library", "list_models", kListTtlSec);
}

void AIQuantLabService::get_factor_library() {
    run_python_cached("ai_quant_lab/qlib_service.py", {"get_factor_library"}, "factor_discovery", "get_factor_library",
                      kListTtlSec);
}

void AIQuantLabService::train_model(const QJsonObject& params) {
    run_module("model_library", "train_model", params);
}

void AIQuantLabService::run_backtest(const QJsonObject& params) {
    run_module("backtesting", "run_backtest", params);
}

void AIQuantLabService::optimize_portfolio(const QJsonObject& params) {
    run_module("backtesting", "optimize_portfolio", params);
}

// ── RL Trading ───────────────────────────────────────────────────────────────
void AIQuantLabService::train_rl_agent(const QJsonObject& params) {
    run_module("rl_trading", "train", params);
}

void AIQuantLabService::evaluate_rl_agent(const QJsonObject& params) {
    run_module("rl_trading", "evaluate", params);
}

// ── GS Quant ─────────────────────────────────────────────────────────────────
void AIQuantLabService::gs_risk_metrics(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"risk_metrics", json}, "gs_quant", "risk_metrics");
}

void AIQuantLabService::gs_portfolio_analytics(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"portfolio_analytics", json}, "gs_quant",
               "portfolio_analytics");
}

void AIQuantLabService::gs_greeks(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"greeks", json}, "gs_quant", "greeks");
}

void AIQuantLabService::gs_var_analysis(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"var_analysis", json}, "gs_quant", "var_analysis");
}

void AIQuantLabService::gs_stress_test(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"stress_test", json}, "gs_quant", "stress_test");
}

void AIQuantLabService::gs_backtest(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"backtest", json}, "gs_quant", "backtest");
}

void AIQuantLabService::gs_statistics(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("Analytics/gs_quant_wrapper/gs_quant_service.py", {"statistics", json}, "gs_quant", "statistics");
}

// ── CFA Quant ────────────────────────────────────────────────────────────────
void AIQuantLabService::cfa_trend_analysis(const QJsonObject& params) {
    run_module("cfa_quant", "trend_analysis", params);
}

void AIQuantLabService::cfa_stationarity_test(const QJsonObject& params) {
    run_module("cfa_quant", "stationarity_test", params);
}

void AIQuantLabService::cfa_arima(const QJsonObject& params) {
    run_module("cfa_quant", "arima_model", params);
}

void AIQuantLabService::cfa_ml_prediction(const QJsonObject& params) {
    run_module("cfa_quant", "supervised_learning", params);
}

void AIQuantLabService::cfa_pattern_discovery(const QJsonObject& params) {
    run_module("cfa_quant", "unsupervised_learning", params);
}

void AIQuantLabService::cfa_bootstrap(const QJsonObject& params) {
    run_module("cfa_quant", "resampling_methods", params);
}

void AIQuantLabService::cfa_data_quality(const QJsonObject& params) {
    run_module("cfa_quant", "validate_data", params);
}

// ── Advanced Models ──────────────────────────────────────────────────────────
void AIQuantLabService::list_advanced_models() {
    run_python_cached("ai_quant_lab/qlib_advanced_models.py", {"list_models"}, "advanced_models", "list_models",
                      kListTtlSec);
}

void AIQuantLabService::create_advanced_model(const QJsonObject& params) {
    run_module("advanced_models", "create_model", params);
}

void AIQuantLabService::train_advanced_model(const QJsonObject& params) {
    run_module("advanced_models", "train_model", params);
}

// ── Factor Discovery ─────────────────────────────────────────────────────────
void AIQuantLabService::factor_get_library() {
    run_python_cached("ai_quant_lab/qlib_service.py", {"get_factor_library"}, "factor_discovery", "get_factor_library",
                      kListTtlSec);
}
void AIQuantLabService::factor_get_data(const QJsonObject& params) {
    run_module("factor_discovery", "get_data", params);
}
void AIQuantLabService::factor_get_calendar(const QJsonObject& params) {
    run_module("factor_discovery", "get_calendar", params);
}
void AIQuantLabService::factor_get_instruments() {
    run_python_cached("ai_quant_lab/qlib_service.py", {"get_instruments"}, "factor_discovery", "get_instruments",
                      kListTtlSec);
}

// ── Model Library ─────────────────────────────────────────────────────────────
void AIQuantLabService::model_list() {
    run_python_cached("ai_quant_lab/qlib_service.py", {"list_models"}, "model_library", "list_models", kListTtlSec);
}
void AIQuantLabService::model_train(const QJsonObject& params) {
    run_module("model_library", "train_model", params);
}
void AIQuantLabService::model_backtest(const QJsonObject& params) {
    run_module("model_library", "run_backtest", params);
}
void AIQuantLabService::model_check_status() {
    run_python_cached("ai_quant_lab/qlib_service.py", {"check_status"}, "model_library", "check_status", kListTtlSec);
}

// ── Live Signals ──────────────────────────────────────────────────────────────
void AIQuantLabService::signals_get_data(const QJsonObject& params) {
    run_module("live_signals", "get_data", params);
}
void AIQuantLabService::signals_get_factor_analysis(const QJsonObject& params) {
    run_module("live_signals", "get_factor_analysis", params);
}
void AIQuantLabService::signals_get_feature_importance(const QJsonObject& params) {
    run_module("live_signals", "get_feature_importance", params);
}

// ── HFT ──────────────────────────────────────────────────────────────────────
void AIQuantLabService::hft_create_orderbook(const QJsonObject& params) {
    run_module("hft", "create_orderbook", params);
}
void AIQuantLabService::hft_snapshot(const QJsonObject& params) {
    run_module("hft", "snapshot", params);
}
void AIQuantLabService::hft_market_making_quotes(const QJsonObject& params) {
    run_module("hft", "market_making_quotes", params);
}
void AIQuantLabService::hft_detect_toxic(const QJsonObject& params) {
    run_module("hft", "detect_toxic", params);
}
void AIQuantLabService::hft_execute_order(const QJsonObject& params) {
    run_module("hft", "execute_order", params);
}

// ── Meta Learning ────────────────────────────────────────────────────────────
void AIQuantLabService::meta_list_models() {
    run_python_cached("ai_quant_lab/qlib_meta_learning.py", {"list_models"}, "meta_learning", "list_models",
                      kListTtlSec);
}
void AIQuantLabService::meta_run_selection(const QJsonObject& params) {
    run_module("meta_learning", "run_selection", params);
}
void AIQuantLabService::meta_create_ensemble(const QJsonObject& params) {
    run_module("meta_learning", "create_ensemble", params);
}
void AIQuantLabService::meta_tune_hyperparameters(const QJsonObject& params) {
    run_module("meta_learning", "tune_hyperparameters", params);
}
void AIQuantLabService::meta_get_results() {
    run_python_cached("ai_quant_lab/qlib_meta_learning.py", {"get_results"}, "meta_learning", "get_results",
                      kListTtlSec);
}

// ── Online Learning ──────────────────────────────────────────────────────────
void AIQuantLabService::online_list_models() {
    run_python_cached("ai_quant_lab/qlib_online_learning.py", {"list_models"}, "online_learning", "list_models",
                      kListTtlSec);
}
void AIQuantLabService::online_create_model(const QJsonObject& params) {
    run_module("online_learning", "create_model", params);
}
void AIQuantLabService::online_train(const QJsonObject& params) {
    run_module("online_learning", "train", params);
}
void AIQuantLabService::online_predict(const QJsonObject& params) {
    run_module("online_learning", "predict", params);
}
void AIQuantLabService::online_performance(const QJsonObject& params) {
    run_module("online_learning", "performance", params);
}

// ── Rolling Retraining ───────────────────────────────────────────────────────
void AIQuantLabService::rolling_create_schedule(const QJsonObject& params) {
    run_module("rolling_retraining", "create", params);
}
void AIQuantLabService::rolling_execute_retrain(const QJsonObject& params) {
    run_module("rolling_retraining", "retrain", params);
}
void AIQuantLabService::rolling_list_schedules() {
    run_python_cached("ai_quant_lab/qlib_rolling_retraining.py", {"list"}, "rolling_retraining", "list", kListTtlSec);
}

// ── Deep Agent (LangGraph multi-agent) ───────────────────────────────────────
void AIQuantLabService::run_deep_agent(const QJsonObject& params) {
    // Build the CLI payload: execute_task with task, agent_type, thread_id
    QJsonObject payload;
    payload["task"] = params["task"];
    payload["agent_type"] = params.contains("agent_type") ? params["agent_type"] : QJsonValue("general");
    if (params.contains("thread_id"))
        payload["thread_id"] = params["thread_id"];

    auto json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    run_python("agents/deepagents/cli.py", {"execute_task", json}, "deep_agent", "execute_task");
}

// ── RD-Agent (autonomous factor/model research) ──────────────────────────────
void AIQuantLabService::rd_agent_check_status() {
    run_python("agents/rdagents/cli.py", {"check_status"}, "deep_agent", "check_status");
}

void AIQuantLabService::rd_agent_start_factor_mining(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("agents/rdagents/cli.py", {"start_factor_mining", json}, "deep_agent", "start_factor_mining");
}

void AIQuantLabService::rd_agent_start_model_optimization(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("agents/rdagents/cli.py", {"start_model_optimization", json}, "deep_agent", "start_model_optimization");
}

void AIQuantLabService::rd_agent_start_quant_research(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("agents/rdagents/cli.py", {"start_quant_research", json}, "deep_agent", "start_quant_research");
}

void AIQuantLabService::rd_agent_get_task_status(const QString& task_id) {
    auto json = QString("{\"task_id\":\"%1\"}").arg(task_id);
    run_python("agents/rdagents/cli.py", {"get_task_status", json}, "deep_agent", "get_task_status");
}

void AIQuantLabService::rd_agent_get_discovered_factors(const QString& task_id) {
    auto json = QString("{\"task_id\":\"%1\"}").arg(task_id);
    run_python("agents/rdagents/cli.py", {"get_discovered_factors", json}, "deep_agent", "get_discovered_factors");
}

void AIQuantLabService::rd_agent_get_optimized_model(const QString& task_id) {
    auto json = QString("{\"task_id\":\"%1\"}").arg(task_id);
    run_python("agents/rdagents/cli.py", {"get_optimized_model", json}, "deep_agent", "get_optimized_model");
}

void AIQuantLabService::rd_agent_list_tasks(const QString& status_filter) {
    QString json = status_filter.isEmpty() ? QStringLiteral("{}") : QString("{\"status\":\"%1\"}").arg(status_filter);
    run_python("agents/rdagents/cli.py", {"list_tasks", json}, "deep_agent", "list_tasks");
}

void AIQuantLabService::rd_agent_stop_task(const QString& task_id) {
    auto json = QString("{\"task_id\":\"%1\"}").arg(task_id);
    run_python("agents/rdagents/cli.py", {"stop_task", json}, "deep_agent", "stop_task");
}

void AIQuantLabService::rd_agent_resume_task(const QString& task_id, const QJsonObject& config) {
    QJsonObject params;
    params["task_id"] = task_id;
    if (!config.isEmpty())
        params["config"] = config;
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python("agents/rdagents/cli.py", {"resume_task", json}, "deep_agent", "resume_task");
}

void AIQuantLabService::rd_agent_start_ui() {
    run_python("agents/rdagents/cli.py", {"start_ui"}, "deep_agent", "start_ui");
}

void AIQuantLabService::rd_agent_start_mcp_server(int port) {
    auto json = QString("{\"port\":%1}").arg(port);
    run_python("agents/rdagents/cli.py", {"start_mcp_server", json}, "deep_agent", "start_mcp_server");
}

void AIQuantLabService::rd_agent_mcp_status() {
    run_python("agents/rdagents/cli.py", {"mcp_status"}, "deep_agent", "mcp_status");
}

// ── Feature Engineering ──────────────────────────────────────────────────────
void AIQuantLabService::feature_compute(const QJsonObject& params) {
    auto cmd = params["indicator"].toString("moving_average");
    run_module("feature_engineering", cmd, params);
}

void AIQuantLabService::feature_select_by_ic(const QJsonObject& params) {
    run_module("feature_engineering", "select_features_by_ic", params);
}

void AIQuantLabService::feature_evaluate_expression(const QJsonObject& params) {
    run_module("feature_engineering", "evaluate_expression", params);
}

// ── Portfolio Optimization ────────────────────────────────────────────────────
void AIQuantLabService::portopt_run(const QString& method, const QJsonObject& params) {
    run_module("portfolio_opt", method, params);
}

// ── Factor Evaluation ─────────────────────────────────────────────────────────
void AIQuantLabService::evaluation_ic(const QJsonObject& params) {
    run_module("factor_evaluation", "calculate_ic", params);
}

void AIQuantLabService::evaluation_report(const QJsonObject& params) {
    run_module("factor_evaluation", "generate_evaluation_report", params);
}

void AIQuantLabService::evaluation_risk_metrics(const QJsonObject& params) {
    run_module("factor_evaluation", "calculate_risk_metrics", params);
}

// ── Strategy Builder ──────────────────────────────────────────────────────────
void AIQuantLabService::strategy_create(const QString& type, const QJsonObject& params) {
    run_module("strategy_builder", type, params);
}

void AIQuantLabService::strategy_portfolio_metrics(const QJsonObject& params) {
    run_module("strategy_builder", "portfolio_metrics", params);
}

void AIQuantLabService::strategy_list() {
    auto it = script_map_.find("strategy_builder");
    if (it == script_map_.end()) {
        emit error_occurred("strategy_builder", "Unknown module");
        return;
    }
    run_python_cached(*it, {"list_strategies", "{}"}, "strategy_builder", "list_strategies", kListTtlSec);
}

// ── Data Processors ───────────────────────────────────────────────────────────
void AIQuantLabService::dataproc_list_processors() {
    auto it = script_map_.find("data_processors");
    if (it == script_map_.end()) {
        emit error_occurred("data_processors", "Unknown module");
        return;
    }
    run_python_cached(*it, {"list_processors", "{}"}, "data_processors", "list_processors", kListTtlSec);
}

void AIQuantLabService::dataproc_create_pipeline(const QJsonObject& params) {
    run_module("data_processors", "create_pipeline", params);
}

void AIQuantLabService::dataproc_process_data(const QJsonObject& params) {
    run_module("data_processors", "process_data", params);
}

// ── Quant Reporting ───────────────────────────────────────────────────────────
void AIQuantLabService::reporting_ic_analysis(const QJsonObject& params) {
    run_module("quant_reporting", "ic_analysis", params);
}

void AIQuantLabService::reporting_model_performance(const QJsonObject& params) {
    run_module("quant_reporting", "model_performance", params);
}

void AIQuantLabService::reporting_cumulative_returns(const QJsonObject& params) {
    run_module("quant_reporting", "cumulative_return_graph", params);
}

} // namespace fincept::services::quant
