// src/services/backtesting/BacktestingTypes.h
#pragma once
#include <QColor>
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

inline QVector<Strategy> default_strategies() {
    return {
        // ── Trend Following ─────────────────────────────────────────────
        {"sma_crossover",
         "SMA Crossover",
         "trend",
         {
             {"fastPeriod", "Fast Period", 10, 2, 100, 1},
             {"slowPeriod", "Slow Period", 20, 5, 200, 1},
         }},
        {"ema_crossover",
         "EMA Crossover",
         "trend",
         {
             {"fastPeriod", "Fast Period", 12, 2, 100, 1},
             {"slowPeriod", "Slow Period", 26, 5, 200, 1},
         }},
        {"macd",
         "MACD",
         "trend",
         {
             {"fast", "Fast Period", 12, 2, 50, 1},
             {"slow", "Slow Period", 26, 5, 100, 1},
             {"signal", "Signal Period", 9, 2, 50, 1},
         }},
        {"adx_trend",
         "ADX Trend",
         "trend",
         {
             {"period", "Period", 14, 5, 50, 1},
             {"threshold", "Threshold", 25, 10, 50, 1},
         }},
        {"triple_ma",
         "Triple MA",
         "trend",
         {
             {"fast", "Fast Period", 5, 2, 50, 1},
             {"medium", "Medium Period", 20, 5, 100, 1},
             {"slow", "Slow Period", 50, 10, 200, 1},
         }},
        {"keltner",
         "Keltner Channel",
         "trend",
         {
             {"period", "Period", 20, 5, 50, 1},
             {"multiplier", "Multiplier", 2.0, 0.5, 5.0, 0.1},
         }},
        {"williams_r",
         "Williams %R",
         "trend",
         {
             {"period", "Period", 14, 5, 50, 1},
             {"overbought", "Overbought", -20, -50, -5, 1},
             {"oversold", "Oversold", -80, -95, -50, 1},
         }},
        {"cci_trend",
         "CCI Trend",
         "trend",
         {
             {"period", "Period", 20, 5, 50, 1},
             {"threshold", "Threshold", 100, 50, 200, 10},
         }},
        {"obv_trend",
         "OBV Trend",
         "trend",
         {
             {"maPeriod", "MA Period", 20, 5, 50, 1},
         }},

        // ── Mean Reversion ──────────────────────────────────────────────
        {"zscore",
         "Z-Score",
         "mean_reversion",
         {
             {"period", "Period", 20, 5, 100, 1},
             {"entry_z", "Entry Z", 2.0, 0.5, 4.0, 0.1},
             {"exit_z", "Exit Z", 0.5, 0.0, 2.0, 0.1},
         }},
        {"bollinger",
         "Bollinger Bands",
         "mean_reversion",
         {
             {"period", "Period", 20, 5, 50, 1},
             {"std_dev", "Std Dev", 2.0, 0.5, 4.0, 0.1},
         }},
        {"rsi",
         "RSI",
         "mean_reversion",
         {
             {"period", "Period", 14, 2, 50, 1},
             {"oversold", "Oversold", 30, 10, 40, 1},
             {"overbought", "Overbought", 70, 60, 90, 1},
         }},
        {"stochastic",
         "Stochastic",
         "mean_reversion",
         {
             {"k_period", "K Period", 14, 5, 50, 1},
             {"d_period", "D Period", 3, 2, 20, 1},
         }},
        {"mfi",
         "Money Flow Index",
         "mean_reversion",
         {
             {"period", "Period", 14, 5, 50, 1},
             {"oversold", "Oversold", 20, 10, 40, 1},
             {"overbought", "Overbought", 80, 60, 90, 1},
         }},

        // ── Momentum ────────────────────────────────────────────────────
        {"momentum",
         "Momentum (ROC)",
         "momentum",
         {
             {"period", "Period", 14, 2, 50, 1},
         }},
        {"dual_momentum",
         "Dual Momentum",
         "momentum",
         {
             {"period", "Period", 12, 2, 50, 1},
             {"lookback", "Lookback", 6, 1, 24, 1},
         }},
        {"macd_zero_cross",
         "MACD Zero Cross",
         "momentum",
         {
             {"fast", "Fast Period", 12, 2, 50, 1},
             {"slow", "Slow Period", 26, 5, 100, 1},
             {"signal", "Signal Period", 9, 2, 50, 1},
         }},

        // ── Breakout ────────────────────────────────────────────────────
        {"donchian",
         "Donchian Breakout",
         "breakout",
         {
             {"period", "Period", 20, 5, 100, 1},
         }},
        {"volatility_bo",
         "Volatility Breakout",
         "breakout",
         {
             {"period", "Period", 20, 5, 50, 1},
             {"multiplier", "Multiplier", 2.0, 0.5, 5.0, 0.1},
         }},
        {"atr_trailing",
         "ATR Trailing Stop",
         "breakout",
         {
             {"period", "Period", 14, 5, 50, 1},
             {"multiplier", "Multiplier", 3.0, 1.0, 6.0, 0.5},
         }},
        {"keltner_breakout",
         "Keltner Breakout",
         "breakout",
         {
             {"period", "Period", 20, 5, 50, 1},
             {"multiplier", "Multiplier", 1.5, 0.5, 4.0, 0.1},
         }},

        // ── Multi-Indicator ─────────────────────────────────────────────
        {"rsi_macd",
         "RSI + MACD",
         "multi",
         {
             {"rsi_period", "RSI Period", 14, 5, 50, 1},
             {"macd_fast", "MACD Fast", 12, 2, 50, 1},
             {"macd_slow", "MACD Slow", 26, 5, 100, 1},
         }},
        {"macd_adx",
         "MACD + ADX",
         "multi",
         {
             {"macd_fast", "MACD Fast", 12, 2, 50, 1},
             {"adx_period", "ADX Period", 14, 5, 50, 1},
         }},
        {"bollinger_rsi",
         "Bollinger + RSI",
         "multi",
         {
             {"bb_period", "BB Period", 20, 5, 50, 1},
             {"rsi_period", "RSI Period", 14, 5, 50, 1},
         }},
        {"trend_momentum",
         "Trend + Momentum",
         "multi",
         {
             {"ma_period", "MA Period", 50, 10, 200, 5},
             {"rsi_period", "RSI Period", 14, 5, 50, 1},
             {"adx_period", "ADX Period", 14, 5, 50, 1},
         }},

        // ── Portfolio Allocation (BT provider) ──────────────────────────
        {"equal_weight", "Equal Weight", "portfolio", {}},
        {"inverse_vol",
         "Inverse Volatility",
         "portfolio",
         {
             {"lookback", "Lookback Days", 60, 20, 252, 5},
         }},
        {"mean_variance",
         "Mean-Variance",
         "portfolio",
         {
             {"lookback", "Lookback Days", 252, 60, 504, 20},
         }},
        {"risk_parity",
         "Risk Parity",
         "portfolio",
         {
             {"lookback", "Lookback Days", 60, 20, 252, 5},
         }},
        {"target_vol",
         "Target Volatility",
         "portfolio",
         {
             {"targetVol", "Target Vol %", 15, 5, 30, 1},
             {"lookback", "Lookback Days", 60, 20, 252, 5},
         }},
        {"min_variance",
         "Min Variance",
         "portfolio",
         {
             {"lookback", "Lookback Days", 252, 60, 504, 20},
         }},
    };
}

inline QStringList strategy_categories() {
    return {"trend", "mean_reversion", "momentum", "breakout", "multi", "portfolio", "custom"};
}

// ── Indicator definitions ───────────────────────────────────────────────────

struct Indicator {
    QString id;
    QString label;
    QVector<StrategyParam> params;
};

inline QVector<Indicator> all_indicators() {
    return {
        {"ma", "Moving Average (SMA)", {{"period", "Period", 20, 2, 200, 1}}},
        {"ema", "Moving Average (EMA)", {{"period", "Period", 20, 2, 200, 1}}},
        {"rsi", "RSI", {{"period", "Period", 14, 2, 50, 1}}},
        {"macd",
         "MACD",
         {
             {"fast", "Fast", 12, 2, 50, 1},
             {"slow", "Slow", 26, 5, 100, 1},
             {"signal", "Signal", 9, 2, 50, 1},
         }},
        {"stoch",
         "Stochastic",
         {
             {"k_period", "K Period", 14, 5, 50, 1},
             {"d_period", "D Period", 3, 2, 20, 1},
         }},
        {"cci", "CCI", {{"period", "Period", 20, 5, 50, 1}}},
        {"williams", "Williams %R", {{"period", "Period", 14, 5, 50, 1}}},
        {"adx", "ADX", {{"period", "Period", 14, 5, 50, 1}}},
        {"momentum", "Momentum", {{"period", "Period", 14, 2, 50, 1}}},
        {"atr", "ATR", {{"period", "Period", 14, 5, 50, 1}}},
        {"bbands",
         "Bollinger Bands",
         {
             {"period", "Period", 20, 5, 50, 1},
             {"std_dev", "Std Dev", 2.0, 0.5, 4.0, 0.1},
         }},
        {"obv", "On Balance Volume", {}},
        {"vwap", "VWAP", {}},
        {"ichimoku",
         "Ichimoku Cloud",
         {
             {"tenkan", "Tenkan", 9, 5, 20, 1},
             {"kijun", "Kijun", 26, 10, 52, 1},
             {"senkou", "Senkou", 52, 20, 120, 1},
         }},
    };
}

// ── Position sizing ─────────────────────────────────────────────────────────

inline QStringList position_sizing_methods() {
    return {"percent", "fixed", "kelly", "vol_target", "risk"};
}

// ── Optimize objectives ─────────────────────────────────────────────────────

inline QStringList optimize_objectives() {
    return {"sharpe", "sortino", "calmar", "return"};
}

// ── Optimize methods ────────────────────────────────────────────────────────

inline QStringList optimize_methods() {
    return {"grid", "random"};
}

// ── ML Label types ──────────────────────────────────────────────────────────

inline QStringList label_types() {
    return {"FIXLB", "MEANLB", "LEXLB", "TRENDLB", "BOLB"};
}

// ── Splitter types ──────────────────────────────────────────────────────────

inline QStringList splitter_types() {
    return {"RollingSplitter", "ExpandingSplitter", "PurgedKFold"};
}

// ── Signal generators ───────────────────────────────────────────────────────

inline QStringList signal_generators() {
    return {"RAND", "RANDX", "RANDNX", "RPROB", "RPROBX"};
}

// ── Indicator signal modes ──────────────────────────────────────────────────

inline QStringList indicator_signal_modes() {
    return {"crossover", "threshold", "breakout", "mean_reversion", "filter"};
}

// ── Returns analysis types ──────────────────────────────────────────────────

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
