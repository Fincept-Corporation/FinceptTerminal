// src/services/backtesting/BacktestingTypes.h
#pragma once
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::backtest {

// ── Provider definitions ────────────────────────────────────────────────────

struct Provider {
    QString slug;
    QString display_name;
    QColor color;
    QStringList commands;
};

inline QVector<Provider> all_providers() {
    return {
        {"vectorbt",
         "VectorBT",
         QColor("#00E5FF"),
         {"backtest", "optimize", "walk_forward", "indicator", "indicator_signals", "labels", "splits", "returns",
          "signals"}},
        {"backtestingpy", "Backtesting.py", QColor("#00D66F"), {"backtest", "optimize", "walk_forward", "indicator"}},
        {"fasttrade", "FastTrade", QColor("#FFC400"), {"backtest"}},
        {"zipline",
         "Zipline",
         QColor("#FF3B8E"),
         {"backtest", "optimize", "walk_forward", "indicator", "indicator_signals"}},
        {"bt", "BT", QColor("#FF6B35"), {"backtest", "optimize", "walk_forward", "indicator", "indicator_signals"}},
        {"fincept", "Fincept", QColor("#d97706"), {"backtest", "optimize", "walk_forward"}},
    };
}

// ── Command definitions ─────────────────────────────────────────────────────

struct CommandDef {
    QString id;
    QString label;
    QColor color;
};

inline QVector<CommandDef> all_commands() {
    return {
        {"backtest", "Run Backtest", QColor("#FF6B35")},
        {"optimize", "Optimize", QColor("#00D66F")},
        {"walk_forward", "Walk-Forward", QColor("#0088FF")},
        {"indicator", "Indicators", QColor("#9D4EDD")},
        {"indicator_signals", "Indicator Signals", QColor("#FFC400")},
        {"labels", "ML Labels", QColor("#00E5FF")},
        {"splits", "CV Splits", QColor("#0088FF")},
        {"returns", "Returns Analysis", QColor("#00D66F")},
        {"signals", "Signal Generators", QColor("#FFC400")},
    };
}

// ── Strategy parameter definition ──────────────────────────────────────────

struct StrategyParam {
    QString name;
    QString label;
    double default_val = 14;
    double min_val = 1;
    double max_val = 200;
    double step = 1;
};

// ── Strategy definitions ────────────────────────────────────────────────────

struct Strategy {
    QString id;
    QString name;
    QString category;
    QVector<StrategyParam> params;
};

/// Returns empty — Python is the source of truth. Strategies are loaded dynamically
/// via BacktestingService::load_strategies() → result_ready("get_strategies").
inline QVector<Strategy> default_strategies() {
    return {};
}

/// Derive categories from a loaded strategy list (no hardcoded list needed).
inline QStringList categories_from_strategies(const QVector<Strategy>& strategies) {
    QStringList cats;
    for (const auto& s : strategies)
        if (!cats.contains(s.category))
            cats.append(s.category);
    return cats;
}

/// Parse a single strategy parameter from JSON.
/// Handles both BT shape {name, label, default, min, max, step}
/// and VectorBT shape {id, name, default, min, max, step}.
inline StrategyParam param_from_json(const QJsonObject& p) {
    StrategyParam sp;
    sp.name      = p.contains("name") ? p.value("name").toString() : p.value("id").toString();
    sp.label     = p.contains("label") ? p.value("label").toString() : p.value("name").toString(sp.name);
    sp.default_val = p.value("default").toDouble(14);
    sp.min_val   = p.value("min").toDouble(1);
    sp.max_val   = p.value("max").toDouble(200);
    sp.step      = p.value("step").toDouble(1);
    return sp;
}

/// Parse strategies from a Python get_strategies JSON response.
///
/// Handles two shapes:
///   BT/backtestingpy: {"success":true,"data":{"strategies":{"category":[{id,name,params:[]}]}}}
///   VectorBT:         {"success":true,"data":[{"type","category","parameters":[{id,name,...}]}]}
inline QVector<Strategy> strategies_from_json(const QJsonObject& root) {
    QVector<Strategy> out;
    auto data_val = root.value("data");

    // VectorBT shape: data is an array
    if (data_val.isArray()) {
        for (const auto& item : data_val.toArray()) {
            auto obj = item.toObject();
            Strategy s;
            s.id       = obj.value("type").toString();
            s.name     = obj.value("name").toString(s.id);
            s.category = obj.value("category").toString("other");
            for (const auto& pv : obj.value("parameters").toArray())
                s.params.append(param_from_json(pv.toObject()));
            if (!s.id.isEmpty())
                out.append(s);
        }
        return out;
    }

    // BT/backtestingpy shape: data.strategies is an object keyed by category
    auto data_obj = data_val.isObject() ? data_val.toObject() : root;
    auto strategies_val = data_obj.value("strategies");
    if (!strategies_val.isObject())
        return out;

    auto cats_obj = strategies_val.toObject();
    for (const auto& cat : cats_obj.keys()) {
        for (const auto& sv : cats_obj.value(cat).toArray()) {
            auto obj = sv.toObject();
            Strategy s;
            s.id       = obj.value("id").toString();
            s.name     = obj.value("name").toString(s.id);
            s.category = cat;
            for (const auto& pv : obj.value("params").toArray())
                s.params.append(param_from_json(pv.toObject()));
            if (!s.id.isEmpty())
                out.append(s);
        }
    }
    return out;
}

// ── Indicator definitions ───────────────────────────────────────────────────

struct Indicator {
    QString id;
    QString label;
    QVector<StrategyParam> params;
};

/// Returns empty — Python is the source of truth. Indicators are loaded dynamically
/// via BacktestingService::execute(provider, "get_indicators", {}) → result_ready.
inline QVector<Indicator> all_indicators() {
    return {};
}

// ── Command options ─────────────────────────────────────────────────────────
// These are compile-time fallbacks used ONLY when a provider does not support
// the "get_command_options" command (e.g. FastTrade, Zipline).
// Real providers (bt, vectorbt, backtestingpy, fincept) override these at
// runtime via BacktestingService::execute(provider, "get_command_options", {})
// → result_ready → BacktestingScreen::on_command_options_loaded().

inline QStringList position_sizing_methods() {
    return {"percent", "fixed", "kelly", "vol_target", "risk"};
}

inline QStringList optimize_objectives() {
    return {"sharpe", "sortino", "calmar", "return"};
}

inline QStringList optimize_methods() {
    return {"grid", "random"};
}

inline QStringList label_types() {
    return {"FIXLB", "MEANLB", "LEXLB", "TRENDLB", "BOLB"};
}

inline QStringList splitter_types() {
    return {"RollingSplitter", "ExpandingSplitter", "PurgedKFold"};
}

inline QStringList signal_generators() {
    return {"RAND", "RANDX", "RANDNX", "RPROB", "RPROBX"};
}

inline QStringList indicator_signal_modes() {
    return {"crossover", "threshold", "breakout", "mean_reversion", "filter"};
}

inline QStringList returns_analysis_types() {
    return {"cumulative", "rolling", "drawdown", "distribution", "benchmark_comparison"};
}

// ── Metric display helpers ──────────────────────────────────────────────────

/// Keys whose values are ratios, not percentages
inline QStringList ratio_metric_keys() {
    return {"sharpe_ratio",      "sortino_ratio", "calmar_ratio", "treynor_ratio",
            "information_ratio", "profit_factor", "beta",         "alpha",
            "sharpeRatio",       "sortinoRatio",  "calmarRatio",  "treynorRatio",
            "informationRatio",  "profitFactor"};
}

/// Keys whose values are already percentages (0-100 scale or 0-1 scale)
inline QStringList pct_metric_keys() {
    return {"total_return", "annualized_return", "max_drawdown", "win_rate", "volatility",
            "totalReturn",  "annualizedReturn",  "maxDrawdown",  "winRate"};
}

/// Keys whose values are counts (integers)
inline QStringList count_metric_keys() {
    return {"total_trades", "winning_trades", "losing_trades", "totalTrades",      "winningTrades",
            "losingTrades", "winning_days",   "losing_days",   "consecutive_wins", "consecutive_losses"};
}

} // namespace fincept::services::backtest
