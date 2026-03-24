// src/services/ai_quant_lab/AIQuantLabService.cpp
#include "services/ai_quant_lab/AIQuantLabService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDateTime>
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
    run_python("ai_quant_lab/qlib_service.py", {"list_models"}, "model_library", "list_models");
}

void AIQuantLabService::get_factor_library() {
    run_python("ai_quant_lab/qlib_service.py", {"get_factor_library"}, "factor_discovery", "get_factor_library");
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
    run_python("ai_quant_lab/qlib_advanced_models.py", {"list_models"}, "advanced_models", "list_models");
}

void AIQuantLabService::create_advanced_model(const QJsonObject& params) {
    run_module("advanced_models", "create_model", params);
}

void AIQuantLabService::train_advanced_model(const QJsonObject& params) {
    run_module("advanced_models", "train_model", params);
}

// ── HFT ──────────────────────────────────────────────────────────────────────
void AIQuantLabService::hft_analyze(const QJsonObject& params) {
    run_module("hft", "analyze", params);
}

// ── Meta Learning ────────────────────────────────────────────────────────────
void AIQuantLabService::meta_learn(const QJsonObject& params) {
    run_module("meta_learning", "learn", params);
}

// ── Online Learning ──────────────────────────────────────────────────────────
void AIQuantLabService::online_learn(const QJsonObject& params) {
    run_module("online_learning", "learn", params);
}

// ── Rolling Retraining ───────────────────────────────────────────────────────
void AIQuantLabService::rolling_retrain(const QJsonObject& params) {
    run_module("rolling_retraining", "retrain", params);
}

} // namespace fincept::services::quant
