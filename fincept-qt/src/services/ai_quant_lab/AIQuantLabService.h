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
    void run_module(const QString& module_id, const QString& command,
                    const QJsonObject& params);

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

    // ── Advanced ────────────────────────────────────────────────────────────
    void list_advanced_models();
    void create_advanced_model(const QJsonObject& params);
    void train_advanced_model(const QJsonObject& params);

    // ── HFT ─────────────────────────────────────────────────────────────────
    void hft_analyze(const QJsonObject& params);

    // ── Meta Learning ───────────────────────────────────────────────────────
    void meta_learn(const QJsonObject& params);

    // ── Online Learning ─────────────────────────────────────────────────────
    void online_learn(const QJsonObject& params);

    // ── Rolling Retraining ──────────────────────────────────────────────────
    void rolling_retrain(const QJsonObject& params);

  signals:
    void result_ready(QString module_id, QString command, QJsonObject data);
    void error_occurred(QString module_id, QString message);

  private:
    explicit AIQuantLabService(QObject* parent = nullptr);
    Q_DISABLE_COPY(AIQuantLabService)

    void run_python(const QString& script, const QStringList& args,
                    const QString& module_id, const QString& command);

    // Module ID → script mapping
    QHash<QString, QString> script_map_;
};

} // namespace fincept::services::quant
