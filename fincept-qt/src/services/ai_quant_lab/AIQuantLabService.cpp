// src/services/ai_quant_lab/AIQuantLabService.cpp
#include "services/ai_quant_lab/AIQuantLabService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/LlmProfileRepository.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QUuid>

#include <algorithm>
#include <memory>

namespace fincept::services::quant {

namespace {
// Cache key prefix for a module's cached read commands (see run_python_cached).
QString aiq_cache_prefix(const QString& module_id) {
    return QStringLiteral("aiquant:") + module_id + QLatin1Char(':');
}

// {"task_id": "<id>"} built with a JSON encoder -- the old QString("{\"task_id\":\"%1\"}") produced
// invalid JSON (or an injectable key) for any id containing a quote or backslash.
QString aiq_task_id_json(const QString& task_id) {
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"task_id", task_id}}).toJson(QJsonDocument::Compact));
}
} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────
AIQuantLabService& AIQuantLabService::instance() {
    static AIQuantLabService inst;
    return inst;
}

AIQuantLabService::AIQuantLabService(QObject* parent) : QObject(parent) {
    // Build script map from module definitions
    for (const auto& mod : all_quant_modules())
        script_map_[mod.id] = mod.script;

    // RD-Agent children must not outlive the terminal (an hour-long LLM loop
    // spending tokens with nobody to report to). Killed while the event loop
    // still exists; their finished() handlers only touch QPointer-guarded state.
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
            for (auto it = rd_tasks_.begin(); it != rd_tasks_.end(); ++it) {
                if (it->proc && it->proc->state() != QProcess::NotRunning) {
                    it->stop_requested = true;
                    it->proc->kill();
                }
            }
        });
    }
}

// ── Python helper ────────────────────────────────────────────────────────────
void AIQuantLabService::run_python(const QString& script, const QStringList& args, const QString& module_id,
                                   const QString& command, const python::PythonRunner::RunOptions& opts) {
    QPointer<AIQuantLabService> self = this;
    python::PythonRunner::instance().run_with_options(
        script, args, opts, [self, module_id, command](python::PythonResult result) {
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
            // A successful command may have created / deleted / trained something that a
            // cached list command (list_models, list, get_results, ...) reports -- drop
            // those entries so the next LIST reflects it instead of serving up to
            // kListTtlSec-old data (a deleted schedule kept showing for five minutes).
            fincept::CacheManager::instance().remove_prefix(aiq_cache_prefix(module_id));
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
    // "deep_agent" spans two implementations (LangGraph deep agent + RD-Agent),
    // neither of which is "<script> <command> <json>": route explicitly. The
    // generic path below sent execute_task to rdagents/cli.py, which has no such
    // command, so quant_deep_agent_execute could never work.
    if (module_id == QLatin1String("deep_agent")) {
        run_deep_agent_command(command, params);
        return;
    }
    auto it = script_map_.find(module_id);
    if (it == script_map_.end()) {
        emit error_occurred(module_id, "Unknown module: " + module_id);
        return;
    }
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(*it, {command, json}, module_id, command);
}

void AIQuantLabService::run_deep_agent_command(const QString& command, const QJsonObject& params) {
    const QString task_id = params.value(QStringLiteral("task_id")).toString();
    if (command == QLatin1String("execute_task"))
        run_deep_agent(params);
    else if (command == QLatin1String("check_status"))
        rd_agent_check_status();
    else if (command == QLatin1String("start_factor_mining"))
        rd_agent_start_factor_mining(params);
    else if (command == QLatin1String("start_model_optimization"))
        rd_agent_start_model_optimization(params);
    else if (command == QLatin1String("start_quant_research"))
        rd_agent_start_quant_research(params);
    else if (command == QLatin1String("get_task_status"))
        rd_agent_get_task_status(task_id);
    else if (command == QLatin1String("get_discovered_factors"))
        rd_agent_get_discovered_factors(task_id);
    else if (command == QLatin1String("get_optimized_model"))
        rd_agent_get_optimized_model(task_id);
    else if (command == QLatin1String("list_tasks"))
        rd_agent_list_tasks(params.value(QStringLiteral("status")).toString());
    else if (command == QLatin1String("stop_task"))
        rd_agent_stop_task(task_id);
    else if (command == QLatin1String("resume_task"))
        rd_agent_resume_task(task_id, params.value(QStringLiteral("config")).toObject());
    else if (command == QLatin1String("start_ui"))
        rd_agent_start_ui();
    else if (command == QLatin1String("start_mcp_server"))
        rd_agent_start_mcp_server(params.value(QStringLiteral("port")).toInt(18765));
    else if (command == QLatin1String("mcp_status"))
        rd_agent_mcp_status();
    else {
        // Queued like every other result so a caller that connects after
        // calling still hears it.
        QPointer<AIQuantLabService> self = this;
        QMetaObject::invokeMethod(
            this,
            [self, command]() {
                if (self)
                    emit self->error_occurred(QStringLiteral("deep_agent"),
                                              QStringLiteral("Unknown deep_agent command: ") + command);
            },
            Qt::QueuedConnection);
    }
}

QJsonObject AIQuantLabService::agent_llm_config(const QJsonObject& params) const {
    const QJsonObject given = params.value(QStringLiteral("config")).toObject();
    if (!given.value(QStringLiteral("llm_provider")).toString().isEmpty())
        return given;
    // MCP / LLM-originated calls carry no credentials (and must not): use the
    // profile assigned to AI Quant Lab, same keys QuantModulePanel builds.
    const ResolvedLlmProfile p = LlmProfileRepository::instance().resolve_for_context(QStringLiteral("ai_quant_lab"));
    if (p.provider.isEmpty())
        return {};
    return QJsonObject{{QStringLiteral("llm_provider"), p.provider},
                       {QStringLiteral("llm_api_key"), p.api_key},
                       {QStringLiteral("llm_model"), p.model_id},
                       {QStringLiteral("llm_base_url"), p.base_url}};
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
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    QPointer<AIQuantLabService> self = this;

    // Called per stdout/stderr line while the process runs — for live UX only.
    auto on_line = [self](QString line, bool is_stderr) {
        if (!self)
            return;
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith('{')) {
            auto doc = QJsonDocument::fromJson(trimmed.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                auto obj = doc.object();
                const auto event = obj.value("event").toString();
                if (event == "progress") {
                    emit self->training_progress("rl_trading", obj.value("step").toInt(), obj.value("total").toInt(),
                                                 obj.value("reward_mean").toDouble(), obj.value("loss").toDouble());
                    return;
                }
                if (event == "log") {
                    emit self->training_log("rl_trading", obj.value("msg").toString(), is_stderr);
                    return;
                }
                // "result" events are handled authoritatively in the finished callback.
                if (event == "result")
                    return;
            }
        }
        // Non-JSON or untagged line → raw log entry (SB3 warnings, stack traces, etc).
        emit self->training_log("rl_trading", line, is_stderr);
    };

    // Called once when the process ends — authoritative parse for the final result.
    auto on_finished = [self](python::PythonResult result) {
        if (!self)
            return;
        QJsonObject final_result;
        bool found_result = false;
        const auto lines = result.output.split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            const auto trimmed = line.trimmed();
            if (!trimmed.startsWith('{'))
                continue;
            auto doc = QJsonDocument::fromJson(trimmed.toUtf8());
            if (doc.isNull() || !doc.isObject())
                continue;
            auto obj = doc.object();
            if (obj.value("event").toString() == "result") {
                obj.remove("event");
                final_result = obj;
                found_result = true;
            }
        }
        // Only trust a result event if the process also exited cleanly. A crash
        // after the script emitted a result line would otherwise surface stale
        // data as success — unsafe for a finance-critical training path.
        if (found_result && result.success) {
            LOG_INFO("AIQuantLab", "[rl_trading/train] Result ready");
            emit self->result_ready("rl_trading", "train", final_result);
        } else {
            QString msg;
            // The script itself reported the failure ({"event":"result","success":false,
            // "error":...}) and exited 0: surface its message, not "exited abnormally".
            const QString script_error = final_result.value("error").toString().trimmed();
            if (found_result && !final_result.value("success").toBool(true) && !script_error.isEmpty()) {
                msg = script_error;
            } else if (found_result && !result.success) {
                msg =
                    QString("Training process exited abnormally (exit=%1) after emitting result").arg(result.exit_code);
                if (!result.error.isEmpty())
                    msg += QStringLiteral(": ") + result.error;
            } else if (!result.error.isEmpty()) {
                msg = result.error;
            } else {
                msg = QStringLiteral("Training ended without result");
            }
            LOG_ERROR("AIQuantLab", QString("[rl_trading/train] Failed: %1").arg(msg));
            emit self->error_occurred("rl_trading", msg);
        }
    };

    python::PythonRunner::instance().run("ai_quant_lab/qlib_rl.py", {"train", json}, on_finished, on_line);
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

// ── Functime ─────────────────────────────────────────────────────────────────
namespace {
constexpr const char* kFunctimeScript = "Analytics/functime_wrapper/functime_service.py";
} // namespace

void AIQuantLabService::functime_check_status() {
    run_python(kFunctimeScript, {"check_status", "{}"}, "functime", "check_status");
}

void AIQuantLabService::functime_forecast(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFunctimeScript, {"forecast", json}, "functime", "forecast");
}

void AIQuantLabService::functime_anomaly_detection(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFunctimeScript, {"anomaly_detection", json}, "functime", "anomaly_detection");
}

void AIQuantLabService::functime_seasonality(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFunctimeScript, {"seasonality", json}, "functime", "seasonality");
}

void AIQuantLabService::functime_metrics(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFunctimeScript, {"metrics", json}, "functime", "metrics");
}

void AIQuantLabService::functime_confidence_intervals(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFunctimeScript, {"confidence_intervals", json}, "functime", "confidence_intervals");
}

void AIQuantLabService::functime_stationarity(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFunctimeScript, {"stationarity", json}, "functime", "stationarity");
}

// ── Statsmodels ──────────────────────────────────────────────────────────────
namespace {
constexpr const char* kStatsmodelsScript = "Analytics/statsmodels_wrapper/statsmodels_service.py";
} // namespace

void AIQuantLabService::sm_check_status() {
    run_python(kStatsmodelsScript, {"check_status", "{}"}, "statsmodels", "check_status");
}

void AIQuantLabService::sm_ols(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kStatsmodelsScript, {"ols", json}, "statsmodels", "ols");
}

void AIQuantLabService::sm_arima(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kStatsmodelsScript, {"arima", json}, "statsmodels", "arima");
}

void AIQuantLabService::sm_stationarity_tests(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kStatsmodelsScript, {"stationarity_tests", json}, "statsmodels", "stationarity_tests");
}

void AIQuantLabService::sm_acf_pacf(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kStatsmodelsScript, {"acf_pacf", json}, "statsmodels", "acf_pacf");
}

void AIQuantLabService::sm_granger_causality(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kStatsmodelsScript, {"granger_causality", json}, "statsmodels", "granger_causality");
}

void AIQuantLabService::sm_descriptive(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kStatsmodelsScript, {"descriptive", json}, "statsmodels", "descriptive");
}

// ── Fortitudo ────────────────────────────────────────────────────────────────
namespace {
constexpr const char* kFortitudoScript = "Analytics/fortitudo_tech_wrapper/fortitudo_service.py";
} // namespace

void AIQuantLabService::fort_check_status() {
    run_python(kFortitudoScript, {"check_status", "{}"}, "fortitudo", "check_status");
}

void AIQuantLabService::fort_portfolio_metrics(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFortitudoScript, {"portfolio_metrics", json}, "fortitudo", "portfolio_metrics");
}

void AIQuantLabService::fort_covariance_matrix(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFortitudoScript, {"covariance_matrix", json}, "fortitudo", "covariance_matrix");
}

void AIQuantLabService::fort_mean_variance_optimize(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFortitudoScript, {"mean_variance_optimize", json}, "fortitudo", "mean_variance_optimize");
}

void AIQuantLabService::fort_mean_cvar_optimize(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFortitudoScript, {"mean_cvar_optimize", json}, "fortitudo", "mean_cvar_optimize");
}

void AIQuantLabService::fort_efficient_frontier(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFortitudoScript, {"efficient_frontier", json}, "fortitudo", "efficient_frontier");
}

void AIQuantLabService::fort_exp_decay_probabilities(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kFortitudoScript, {"exp_decay_probabilities", json}, "fortitudo", "exp_decay_probabilities");
}

// ── GluonTS ──────────────────────────────────────────────────────────────────
namespace {
constexpr const char* kGluonTsScript = "Analytics/gluonts_wrapper/gluonts_service.py";
} // namespace

void AIQuantLabService::gluon_check_status() {
    run_python(kGluonTsScript, {"check_status", "{}"}, "gluonts", "check_status");
}

void AIQuantLabService::gluon_probabilistic_forecast(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kGluonTsScript, {"probabilistic_forecast", json}, "gluonts", "probabilistic_forecast");
}

void AIQuantLabService::gluon_quantile_forecast(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kGluonTsScript, {"quantile_forecast", json}, "gluonts", "quantile_forecast");
}

void AIQuantLabService::gluon_distribution_fit(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kGluonTsScript, {"distribution_fit", json}, "gluonts", "distribution_fit");
}

void AIQuantLabService::gluon_evaluate_forecast(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kGluonTsScript, {"evaluate_forecast", json}, "gluonts", "evaluate_forecast");
}

void AIQuantLabService::gluon_seasonal_naive(const QJsonObject& params) {
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    run_python(kGluonTsScript, {"seasonal_naive", json}, "gluonts", "seasonal_naive");
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
    run_module("hft", "fetch_orderbook", params); // live CCXT fetch
}
void AIQuantLabService::hft_snapshot(const QJsonObject& params) {
    run_module("hft", "analyze", params); // full analysis: book + trades + all metrics
}
void AIQuantLabService::hft_market_making_quotes(const QJsonObject& params) {
    run_module("hft", "market_making", params); // live book → A-S quotes
}
void AIQuantLabService::hft_detect_toxic(const QJsonObject& params) {
    run_module("hft", "toxic_flow", params); // live trades → PIN score
}
void AIQuantLabService::hft_execute_order(const QJsonObject& params) {
    run_module("hft", "slippage", params); // order book walk → slippage estimate
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
    // Retrain emits one JSON event per line (start / window / ensemble / done / error), flushed
    // as the run progresses. Forward each event the moment it arrives so the panel's progress
    // bar and log are live (they used to be replayed only after the process exited, i.e. after
    // up to an hour of silence), then make sure a run that never reached a terminal event is
    // reported as a failure instead of leaving the bar on "Starting...".
    struct RetrainState {
        bool saw_event = false; // at least one JSON object was forwarded
        bool terminal = false;  // a done / error event (or a bare failure object) was forwarded
    };
    auto state = std::make_shared<RetrainState>();
    QPointer<AIQuantLabService> self = this;

    auto forward = [self, state](const QString& line) {
        const auto trimmed = line.trimmed();
        if (!self || !trimmed.startsWith('{'))
            return;
        const auto doc = QJsonDocument::fromJson(trimmed.toUtf8());
        if (doc.isNull() || !doc.isObject())
            return;
        const auto obj = doc.object();
        const auto event = obj.value("event").toString();
        state->saw_event = true;
        if (event == "done" || event == "error" || (event.isEmpty() && !obj.value("success").toBool(true)))
            state->terminal = true;
        emit self->result_ready("rolling_retraining", "retrain", obj);
    };

    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    python::PythonRunner::instance().run(
        "ai_quant_lab/qlib_rolling_retraining.py", {"retrain", json},
        [self, state, forward](python::PythonResult result) {
            if (!self)
                return;
            // A retrain updates the schedule's last_run / last_status.
            fincept::CacheManager::instance().remove_prefix(aiq_cache_prefix("rolling_retraining"));
            if (!state->saw_event) {
                // Nothing came through on_line: replay the buffered output once, in case the
                // runner delivered it only with the finished callback.
                for (const auto& line : result.output.split('\n', Qt::SkipEmptyParts))
                    forward(line);
            }
            if (!state->terminal) {
                const QString msg = result.error.trimmed().isEmpty()
                                        ? QStringLiteral("Retrain ended without a result")
                                        : result.error.trimmed();
                LOG_ERROR("AIQuantLab", QString("[rolling_retraining/retrain] Failed: %1").arg(msg));
                emit self->error_occurred("rolling_retraining", msg);
            }
        },
        [forward](QString line, bool is_stderr) {
            if (!is_stderr)
                forward(line);
        });
}
void AIQuantLabService::rolling_list_schedules() {
    run_python_cached("ai_quant_lab/qlib_rolling_retraining.py", {"list"}, "rolling_retraining", "list", kListTtlSec);
}
void AIQuantLabService::rolling_preview_tasks(const QJsonObject& params) {
    run_module("rolling_retraining", "preview", params);
}
void AIQuantLabService::rolling_delete_schedule(const QJsonObject& params) {
    run_module("rolling_retraining", "delete", params);
}

// ── Deep Agent (LangGraph multi-agent) ───────────────────────────────────────
// A multi-agent LangGraph run calls the LLM many times; give it a real budget
// instead of the generic script watchdog.
static constexpr int kAiqDeepAgentTimeoutMs = 20 * 60 * 1000;

void AIQuantLabService::run_deep_agent(const QJsonObject& params) {
    QJsonObject payload;
    payload["task"] = params["task"];
    payload["agent_type"] = params.contains("agent_type") ? params["agent_type"] : QJsonValue("general");
    if (params.contains("thread_id"))
        payload["thread_id"] = params["thread_id"];
    // deepagents/cli.py hands `config` to create_model(); an empty block means
    // "no provider" — resolve it here for callers that have none (MCP tools).
    payload["config"] = agent_llm_config(params);

    // SECURITY: the payload carries the LLM API key. It used to be an argv JSON
    // argument — readable by any same-user process (Win32_Process.CommandLine,
    // /proc/<pid>/cmdline) and captured by crash dumps / EDR. stdin is not.
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kAiqDeepAgentTimeoutMs;
    opts.stdin_data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    run_python("agents/deepagents/cli.py", {"execute_task", "--stdin"}, "deep_agent", "execute_task", opts);
}

// ── RD-Agent (autonomous factor/model research) ──────────────────────────────
//
// Why the RD-Agent long tasks are child processes owned HERE (not PythonRunner):
//   * The old design ran each loop on a daemon thread of a ONE-SHOT process:
//     cli.py start_* returned "started", the process exited, and the thread
//     died with it — every task was dead on arrival, and status / stop / list
//     in later processes looked at an empty in-memory registry.
//   * A loop runs for tens of minutes to hours. Through PythonRunner it would
//     occupy one of its 3 global slots for that long, and PythonRunner has no
//     handle to kill a run (stop = kill is the only stop that frees anything).
// So, like AgentService's streaming runs (the P4 exception precedent), each
// task is one QProcess with the standard Python env: the payload — LLM key
// included — goes in on stdin, `cli.py run_task` runs the loop synchronously
// and prints JSON lines (progress after every iteration, then result / error),
// and status, results and stop are served from the registry below. Tasks live
// for this app session; checkpoints in the per-profile workspace let RESUME
// continue one.

namespace {
QString aiq_rd_elapsed(qint64 started_ms, qint64 finished_ms) {
    if (started_ms <= 0)
        return QStringLiteral("-");
    const qint64 end = finished_ms > 0 ? finished_ms : QDateTime::currentMSecsSinceEpoch();
    const qint64 mins = std::max<qint64>(0, (end - started_ms) / 60000);
    return QStringLiteral("%1 min").arg(mins);
}

QJsonObject aiq_rd_not_found(const QString& task_id) {
    return {{"success", false}, {"error", QStringLiteral("Task '%1' not found in this session").arg(task_id)}};
}
} // namespace

void AIQuantLabService::rd_emit(const QString& command, const QJsonObject& data) {
    QPointer<AIQuantLabService> self = this;
    QMetaObject::invokeMethod(
        this,
        [self, command, data]() {
            if (self)
                emit self->result_ready(QStringLiteral("deep_agent"), command, data);
        },
        Qt::QueuedConnection);
}

int AIQuantLabService::rd_running_count() const {
    int n = 0;
    for (auto it = rd_tasks_.cbegin(); it != rd_tasks_.cend(); ++it) {
        if (it->status == QLatin1String("running"))
            ++n;
    }
    return n;
}

QJsonObject AIQuantLabService::rd_status_json(const RdTask& t) const {
    return QJsonObject{
        {"success", true},
        {"task_id", t.id},
        {"type", t.kind},
        {"status", t.status},
        {"progress", t.progress},
        {"iterations_completed", t.iterations_completed},
        {"max_iterations", t.max_iterations},
        {"current_step", t.current_step},
        {"best_ic", t.best_ic.isUndefined() ? QJsonValue() : t.best_ic},
        {"best_sharpe", t.best_sharpe.isUndefined() ? QJsonValue() : t.best_sharpe},
        {"elapsed_time", aiq_rd_elapsed(t.started_ms, t.finished_ms)},
        {"error", t.error.isEmpty() ? QJsonValue() : QJsonValue(t.error)},
    };
}

void AIQuantLabService::rd_start(const QString& command, const QString& kind, const QJsonObject& params) {
    if (rd_running_count() >= kMaxConcurrentRdTasks) {
        rd_emit(command, {{"success", false},
                          {"error", QStringLiteral("%1 RD-Agent tasks are already running — stop one or wait for it "
                                                   "to finish.")
                                        .arg(kMaxConcurrentRdTasks)}});
        return;
    }
    const QJsonObject llm = agent_llm_config(params);
    if (llm.isEmpty()) {
        rd_emit(command, {{"success", false},
                          {"error", QStringLiteral("No LLM configured for AI Quant Lab — pick an LLM profile "
                                                   "(Settings → LLM Config).")}});
        return;
    }

    RdTask t;
    const QString prefix = kind == QLatin1String("factor_mining")        ? QStringLiteral("factor")
                           : kind == QLatin1String("model_optimization") ? QStringLiteral("model")
                                                                         : QStringLiteral("quant");
    t.id = QStringLiteral("%1_%2_%3")
               .arg(prefix, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")),
                    QUuid::createUuid().toString(QUuid::WithoutBraces).left(6));
    t.kind = kind;
    t.params = params;
    t.params.remove(QStringLiteral("config")); // never keep credentials
    t.max_iterations = std::max(
        1, params.value(QStringLiteral("max_iterations")).toInt(params.value(QStringLiteral("iterations")).toInt(10)));
    // Per-profile, writable workspace (the scripts dir is pruned in dev builds
    // and read-only in installs).
    t.workspace = AppPaths::data() + QStringLiteral("/rdagents/workspaces/") + t.id;
    const QString task_id = t.id;
    rd_tasks_.insert(task_id, t);

    QString err;
    if (!rd_spawn(task_id, llm, {}, &err)) {
        rd_tasks_[task_id].status = QStringLiteral("failed");
        rd_tasks_[task_id].error = err;
        rd_emit(command, {{"success", false}, {"task_id", task_id}, {"error", err}});
        return;
    }
    const int n = rd_tasks_[task_id].max_iterations;
    rd_emit(command, {{"success", true},
                      {"task_id", task_id},
                      {"status", "started"},
                      {"message", QStringLiteral("RD-Agent task started")},
                      {"max_iterations", n},
                      {"estimated_time", QStringLiteral("%1-%2 minutes").arg(n * 3).arg(n * 10)}});
}

bool AIQuantLabService::rd_spawn(const QString& task_id, const QJsonObject& llm_config, const QString& resume_from,
                                 QString* error_out) {
    auto it = rd_tasks_.find(task_id);
    if (it == rd_tasks_.end())
        return false;
    auto& py = python::PythonRunner::instance();
    if (!py.is_available()) {
        if (error_out)
            *error_out = QStringLiteral("Python not available");
        return false;
    }
    QDir().mkpath(it->workspace);

    QJsonObject payload = it->params;
    payload["task_id"] = task_id;
    payload["kind"] = it->kind;
    payload["workspace"] = it->workspace;
    payload["max_iterations"] = it->max_iterations;
    payload["config"] = llm_config;
    if (!resume_from.isEmpty())
        payload["resume_from"] = resume_from;

    auto* proc = new QProcess(this);
    proc->setProcessEnvironment(py.build_python_env());
    proc->setWorkingDirectory(py.scripts_dir());
#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    it->proc = proc;
    it->status = QStringLiteral("running");
    it->stop_requested = false;
    it->error.clear();
    it->started_ms = QDateTime::currentMSecsSinceEpoch();
    it->finished_ms = 0;
    it->current_step = QStringLiteral("starting");

    QPointer<AIQuantLabService> self = this;
    auto stdout_buf = std::make_shared<QByteArray>();
    connect(proc, &QProcess::readyReadStandardOutput, this, [self, proc, task_id, stdout_buf]() {
        if (!self)
            return;
        stdout_buf->append(proc->readAllStandardOutput());
        qsizetype nl_pos = -1;
        while ((nl_pos = stdout_buf->indexOf('\n')) >= 0) {
            const QByteArray line = stdout_buf->left(nl_pos).trimmed();
            stdout_buf->remove(0, nl_pos + 1);
            if (!line.isEmpty())
                self->rd_handle_line(task_id, line);
        }
    });
    // stderr is the loop's own logging; keep only its tail for the error text.
    auto stderr_tail = std::make_shared<QByteArray>();
    connect(proc, &QProcess::readyReadStandardError, this, [proc, stderr_tail]() {
        stderr_tail->append(proc->readAllStandardError());
        if (stderr_tail->size() > 8192)
            stderr_tail->remove(0, stderr_tail->size() - 8192);
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [self, proc, task_id, stdout_buf, stderr_tail](int exit_code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (!self)
                    return;
                const QByteArray rest = *stdout_buf + '\n' + proc->readAllStandardOutput();
                stdout_buf->clear();
                for (const QByteArray& line : rest.split('\n')) {
                    const QByteArray trimmed = line.trimmed();
                    if (!trimmed.isEmpty())
                        self->rd_handle_line(task_id, trimmed);
                }
                auto t = self->rd_tasks_.find(task_id);
                if (t == self->rd_tasks_.end() || t->status != QLatin1String("running"))
                    return; // already settled by a result / error line
                t->finished_ms = QDateTime::currentMSecsSinceEpoch();
                if (t->stop_requested) {
                    t->status = QStringLiteral("stopped");
                    t->current_step = QStringLiteral("stopped");
                } else {
                    t->status = QStringLiteral("failed");
                    const QString tail = QString::fromUtf8(stderr_tail->right(400)).trimmed();
                    t->error = tail.isEmpty()
                                   ? QStringLiteral("RD-Agent process exited (code %1) without a result").arg(exit_code)
                                   : tail;
                    LOG_ERROR("AIQuantLab", QString("[deep_agent/%1] failed: %2").arg(task_id, t->error.left(200)));
                }
            });
    connect(proc, &QProcess::errorOccurred, this, [self, proc, task_id](QProcess::ProcessError e) {
        // Only a failed spawn ends without finished(); a crash is reported there.
        if (e != QProcess::FailedToStart)
            return;
        const QString err = proc->errorString();
        proc->deleteLater();
        if (!self)
            return;
        auto t = self->rd_tasks_.find(task_id);
        if (t == self->rd_tasks_.end())
            return;
        t->status = QStringLiteral("failed");
        t->error = QStringLiteral("Process error: ") + err;
        t->finished_ms = QDateTime::currentMSecsSinceEpoch();
    });

    LOG_INFO("AIQuantLab",
             QString("[deep_agent] RD-Agent task %1 (%2) started%3")
                 .arg(task_id, it->kind, resume_from.isEmpty() ? QString() : QStringLiteral(" (resume)")));
    proc->start(py.python_path(), {py.scripts_dir() + QStringLiteral("/agents/rdagents/cli.py"),
                                   QStringLiteral("run_task"), QStringLiteral("--stdin")});
    proc->write(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    proc->closeWriteChannel();
    return true;
}

void AIQuantLabService::rd_handle_line(const QString& task_id, const QByteArray& line) {
    if (!line.startsWith('{'))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject())
        return;
    auto t = rd_tasks_.find(task_id);
    if (t == rd_tasks_.end())
        return;
    const QJsonObject ev = doc.object();
    const QString event = ev.value(QStringLiteral("event")).toString();
    if (event == QLatin1String("progress")) {
        t->iterations_completed = ev.value(QStringLiteral("iterations_completed")).toInt(t->iterations_completed);
        t->progress = std::clamp(ev.value(QStringLiteral("progress")).toDouble(t->progress), 0.0, 1.0);
        t->current_step = ev.value(QStringLiteral("current_step")).toString(t->current_step);
        if (!ev.value(QStringLiteral("best_ic")).isNull() && !ev.value(QStringLiteral("best_ic")).isUndefined())
            t->best_ic = ev.value(QStringLiteral("best_ic"));
        if (!ev.value(QStringLiteral("best_sharpe")).isNull() && !ev.value(QStringLiteral("best_sharpe")).isUndefined())
            t->best_sharpe = ev.value(QStringLiteral("best_sharpe"));
    } else if (event == QLatin1String("result")) {
        t->finished_ms = QDateTime::currentMSecsSinceEpoch();
        if (ev.value(QStringLiteral("success")).toBool()) {
            t->status = QStringLiteral("completed");
            t->progress = 1.0;
            t->current_step = QStringLiteral("completed");
            t->factors = ev.value(QStringLiteral("factors")).toArray();
            t->models = ev.value(QStringLiteral("models")).toArray();
            if (!ev.value(QStringLiteral("best_ic")).isNull() && !ev.value(QStringLiteral("best_ic")).isUndefined())
                t->best_ic = ev.value(QStringLiteral("best_ic"));
            if (!ev.value(QStringLiteral("best_sharpe")).isNull() &&
                !ev.value(QStringLiteral("best_sharpe")).isUndefined())
                t->best_sharpe = ev.value(QStringLiteral("best_sharpe"));
            LOG_INFO("AIQuantLab", QString("[deep_agent] RD-Agent task %1 completed").arg(task_id));
        } else {
            t->status = QStringLiteral("failed");
            t->error = ev.value(QStringLiteral("error")).toString(QStringLiteral("RD-Agent task failed"));
        }
    } else if (event == QLatin1String("error")) {
        t->finished_ms = QDateTime::currentMSecsSinceEpoch();
        t->status = QStringLiteral("failed");
        t->error = ev.value(QStringLiteral("error")).toString(QStringLiteral("RD-Agent task failed"));
    }
}

void AIQuantLabService::rd_agent_check_status() {
    run_python("agents/rdagents/cli.py", {"check_status"}, "deep_agent", "check_status");
}

void AIQuantLabService::rd_agent_start_factor_mining(const QJsonObject& params) {
    rd_start(QStringLiteral("start_factor_mining"), QStringLiteral("factor_mining"), params);
}

void AIQuantLabService::rd_agent_start_model_optimization(const QJsonObject& params) {
    rd_start(QStringLiteral("start_model_optimization"), QStringLiteral("model_optimization"), params);
}

void AIQuantLabService::rd_agent_start_quant_research(const QJsonObject& params) {
    rd_start(QStringLiteral("start_quant_research"), QStringLiteral("quant_research"), params);
}

void AIQuantLabService::rd_agent_get_task_status(const QString& task_id) {
    const auto it = rd_tasks_.constFind(task_id);
    if (it == rd_tasks_.cend()) {
        rd_emit(QStringLiteral("get_task_status"), aiq_rd_not_found(task_id));
        return;
    }
    rd_emit(QStringLiteral("get_task_status"), rd_status_json(*it));
}

void AIQuantLabService::rd_agent_get_discovered_factors(const QString& task_id) {
    const auto it = rd_tasks_.constFind(task_id);
    if (it == rd_tasks_.cend()) {
        rd_emit(QStringLiteral("get_discovered_factors"), aiq_rd_not_found(task_id));
        return;
    }
    rd_emit(QStringLiteral("get_discovered_factors"),
            {{"success", true},
             {"task_id", task_id},
             {"status", it->status},
             {"total_factors", it->factors.size()},
             {"factors", it->factors},
             {"best_ic", it->best_ic.isUndefined() ? QJsonValue() : it->best_ic},
             {"best_sharpe", it->best_sharpe.isUndefined() ? QJsonValue() : it->best_sharpe}});
}

void AIQuantLabService::rd_agent_get_optimized_model(const QString& task_id) {
    const auto it = rd_tasks_.constFind(task_id);
    if (it == rd_tasks_.cend()) {
        rd_emit(QStringLiteral("get_optimized_model"), aiq_rd_not_found(task_id));
        return;
    }
    rd_emit(QStringLiteral("get_optimized_model"),
            {{"success", true},
             {"task_id", task_id},
             {"status", it->status},
             {"model_type", it->params.value(QStringLiteral("model_type"))},
             {"total_models", it->models.size()},
             {"models", it->models},
             {"best_sharpe", it->best_sharpe.isUndefined() ? QJsonValue() : it->best_sharpe},
             {"best_ic", it->best_ic.isUndefined() ? QJsonValue() : it->best_ic}});
}

void AIQuantLabService::rd_agent_list_tasks(const QString& status_filter) {
    QJsonArray tasks;
    for (auto it = rd_tasks_.cbegin(); it != rd_tasks_.cend(); ++it) {
        if (!status_filter.isEmpty() && it->status != status_filter)
            continue;
        tasks.append(rd_status_json(*it));
    }
    rd_emit(QStringLiteral("list_tasks"), {{"success", true}, {"tasks", tasks}, {"total", tasks.size()}});
}

void AIQuantLabService::rd_agent_stop_task(const QString& task_id) {
    auto it = rd_tasks_.find(task_id);
    if (it == rd_tasks_.end()) {
        rd_emit(QStringLiteral("stop_task"), aiq_rd_not_found(task_id));
        return;
    }
    if (it->status != QLatin1String("running") || !it->proc) {
        rd_emit(
            QStringLiteral("stop_task"),
            {{"success", false}, {"error", QStringLiteral("Task '%1' is not running (%2)").arg(task_id, it->status)}});
        return;
    }
    // A real stop: the loop runs in that process, so killing it frees the CPU,
    // the LLM spend and the slot. The checkpoint from the last finished
    // iteration stays in the workspace for RESUME.
    it->stop_requested = true;
    it->proc->kill();
    rd_emit(QStringLiteral("stop_task"),
            {{"success", true}, {"task_id", task_id}, {"message", QStringLiteral("Task %1 stopped").arg(task_id)}});
}

void AIQuantLabService::rd_agent_resume_task(const QString& task_id, const QJsonObject& config) {
    auto it = rd_tasks_.find(task_id);
    if (it == rd_tasks_.end()) {
        rd_emit(QStringLiteral("resume_task"), aiq_rd_not_found(task_id));
        return;
    }
    if (it->status == QLatin1String("running")) {
        rd_emit(QStringLiteral("resume_task"),
                {{"success", false}, {"error", QStringLiteral("Task '%1' is already running").arg(task_id)}});
        return;
    }
    if (it->kind == QLatin1String("quant_research")) {
        rd_emit(QStringLiteral("resume_task"),
                {{"success", false},
                 {"error", QStringLiteral("Resume is supported for factor mining and model optimization tasks "
                                          "only")}});
        return;
    }
    const QString checkpoint = it->workspace + QStringLiteral("/checkpoint.pkl");
    if (!QFileInfo::exists(checkpoint)) {
        rd_emit(QStringLiteral("resume_task"),
                {{"success", false},
                 {"error", QStringLiteral("No checkpoint for task '%1' yet — it stopped before its first iteration "
                                          "finished")
                               .arg(task_id)}});
        return;
    }
    if (rd_running_count() >= kMaxConcurrentRdTasks) {
        rd_emit(QStringLiteral("resume_task"),
                {{"success", false},
                 {"error", QStringLiteral("%1 RD-Agent tasks are already running").arg(kMaxConcurrentRdTasks)}});
        return;
    }
    QJsonObject call_params;
    if (!config.isEmpty())
        call_params["config"] = config;
    const QJsonObject llm = agent_llm_config(call_params);
    if (llm.isEmpty()) {
        rd_emit(QStringLiteral("resume_task"),
                {{"success", false}, {"error", QStringLiteral("No LLM configured for AI Quant Lab")}});
        return;
    }
    QString err;
    if (!rd_spawn(task_id, llm, checkpoint, &err)) {
        rd_emit(QStringLiteral("resume_task"), {{"success", false}, {"error", err}});
        return;
    }
    rd_emit(QStringLiteral("resume_task"),
            {{"success", true},
             {"task_id", task_id},
             {"message", QStringLiteral("Task %1 resumed from its last checkpoint").arg(task_id)}});
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
void AIQuantLabService::reporting_check_status() {
    run_module("quant_reporting", "check_status", {});
}

void AIQuantLabService::reporting_ic_analysis(const QJsonObject& params) {
    run_module("quant_reporting", "ic_analysis", params);
}

void AIQuantLabService::reporting_model_performance(const QJsonObject& params) {
    run_module("quant_reporting", "model_performance", params);
}

void AIQuantLabService::reporting_cumulative_returns(const QJsonObject& params) {
    run_module("quant_reporting", "cumulative_returns", params);
}

void AIQuantLabService::reporting_risk_report(const QJsonObject& params) {
    run_module("quant_reporting", "risk_report", params);
}

void AIQuantLabService::reporting_factor_quantiles(const QJsonObject& params) {
    run_module("quant_reporting", "factor_quantiles", params);
}

} // namespace fincept::services::quant
