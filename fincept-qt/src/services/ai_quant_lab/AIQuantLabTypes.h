// src/services/ai_quant_lab/AIQuantLabTypes.h
#pragma once
#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::quant {

// ── Module definitions ──────────────────────────────────────────────────────

struct QuantModule {
    QString id;
    QString label;
    QString short_label;
    QString category; // CORE, AI_ML, ADVANCED, ANALYTICS
    QColor color;
    QString script; // Python script to call
    QString description;
};

inline QVector<QuantModule> all_quant_modules() {
    return {
        // CORE
        {"factor_discovery", "Factor Discovery", "FACTOR", "CORE", QColor("#FF6B35"), "ai_quant_lab/qlib_service.py",
         "Discover alpha factors using Qlib framework"},
        {"model_library", "Model Library", "MODEL", "CORE", QColor("#00E5FF"), "ai_quant_lab/qlib_service.py",
         "30+ ML models: LightGBM, XGBoost, LSTM, Transformer"},
        {"backtesting", "Backtesting", "BTEST", "CORE", QColor("#00D66F"), "ai_quant_lab/qlib_advanced_backtest.py",
         "Multi-strategy backtesting with portfolio optimization"},
        {"live_signals", "Live Signals", "SIGNAL", "CORE", QColor("#FFC400"), "ai_quant_lab/qlib_service.py",
         "Real-time trading signal generation"},

        // AI/ML
        {"deep_agent", "Deep Agent", "DAGENT", "AI_ML", QColor("#9D4EDD"),
         "ai_quant_lab/rdagent/core/rd_agent_service.py", "Autonomous AI research agent (RD-Agent)"},
        {"rl_trading", "RL Trading", "RL", "AI_ML", QColor("#FF3B8E"), "ai_quant_lab/qlib_rl.py",
         "Reinforcement learning: PPO, DQN, A2C, SAC, TD3"},
        {"online_learning", "Online Learning", "ONLINE", "AI_ML", QColor("#00BCD4"),
         "ai_quant_lab/qlib_online_learning.py", "Incremental learning with concept drift detection"},
        {"meta_learning", "Meta Learning", "META", "AI_ML", QColor("#E040FB"), "ai_quant_lab/qlib_meta_learning.py",
         "Few-shot learning, model selection, AutoML"},
        {"pattern_intelligence", "Pattern Intelligence", "PATTERN", "AI_ML", QColor("#FF9100"),
         "Analytics/technical_indicators.py", "Technical pattern recognition and scoring"},

        // ADVANCED
        {"hft", "High Frequency Trading", "HFT", "ADVANCED", QColor("#F44336"), "ai_quant_lab/qlib_high_frequency.py",
         "Order book dynamics, market making, latency optimization"},
        {"rolling_retraining", "Rolling Retraining", "ROLL", "ADVANCED", QColor("#8BC34A"),
         "ai_quant_lab/qlib_rolling_retraining.py", "Automated model retraining with rolling windows"},
        {"advanced_models", "Advanced Models", "ADV", "ADVANCED", QColor("#03A9F4"),
         "ai_quant_lab/qlib_advanced_models.py", "LSTM, GRU, Transformer, Localformer, HIST, GAT"},

        // ANALYTICS
        {"cfa_quant", "CFA Quant", "CFA", "ANALYTICS", QColor("#FF6B35"), "Analytics/quant_analytics_cli.py",
         "Trend, ARIMA, ML prediction, PCA, bootstrap"},
        {"gs_quant", "GS Quant", "GS", "ANALYTICS", QColor("#9D4EDD"), "Analytics/gs_quant_wrapper/gs_quant_service.py",
         "Risk metrics, portfolio analytics, options Greeks, VaR"},
        {"statsmodels", "Statsmodels", "STATS", "ANALYTICS", QColor("#2196F3"), "Analytics/statsmodels_cli.py",
         "Statistical modeling and econometrics"},
        {"functime", "Functime", "FUNC", "ANALYTICS", QColor("#4CAF50"), "Analytics/quantstats_analytics.py",
         "Time-series forecasting and analysis"},
        {"fortitudo", "Fortitudo", "FORT", "ANALYTICS", QColor("#FF5722"), "Analytics/riskfoliolib_wrapper.py",
         "Risk-aware portfolio optimization"},
        {"gluonts", "GluonTS", "GLUON", "ANALYTICS", QColor("#795548"), "Analytics/quantstats_analytics.py",
         "Probabilistic time-series forecasting"},
    };
}

inline QStringList quant_categories() {
    return {"CORE", "AI_ML", "ADVANCED", "ANALYTICS"};
}

} // namespace fincept::services::quant
