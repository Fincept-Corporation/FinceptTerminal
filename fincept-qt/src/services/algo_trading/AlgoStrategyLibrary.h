// src/services/algo_trading/AlgoStrategyLibrary.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

namespace fincept::services::algo {

// Curated, fully-editable C++ DSL strategy library seeded into the algo_strategies
// table (fixed LIB-* ids). Replaces the old Python/LEAN QC registry. Each entry is
// expressed in the same condition schema the Builder and ConditionEvaluator use.
inline QVector<AlgoStrategy> algo_library_strategies() {
    // value-comparison leaf: <indicator>.<field> <op> <constant>
    auto cv = [](const QString& ind, const QJsonObject& p, const QString& field,
                 const QString& op, double value) {
        QJsonObject c;
        c["indicator"] = ind;
        c["params"] = p;
        c["field"] = field;
        c["operator"] = op;
        c["compare_mode"] = "value";
        c["value"] = value;
        return c;
    };
    // indicator-vs-indicator leaf
    auto ci = [](const QString& ind, const QJsonObject& p, const QString& field, const QString& op,
                 const QString& cmpInd, const QJsonObject& cmpP, const QString& cmpField) {
        QJsonObject c;
        c["indicator"] = ind;
        c["params"] = p;
        c["field"] = field;
        c["operator"] = op;
        c["compare_mode"] = "indicator";
        c["compare_indicator"] = cmpInd;
        c["compare_params"] = cmpP;
        c["compare_field"] = cmpField;
        return c;
    };
    // indicator-vs-indicator leaf comparing against the RHS band N bars back
    // (needed for true channel breakouts: close vs the *prior* bar's extreme).
    auto cio = [&ci](const QString& ind, const QJsonObject& p, const QString& field, const QString& op,
                     const QString& cmpInd, const QJsonObject& cmpP, const QString& cmpField, int cmpOffset) {
        QJsonObject c = ci(ind, p, field, op, cmpInd, cmpP, cmpField);
        c["compare_offset"] = cmpOffset;
        return c;
    };
    auto mk = [](const QString& id, const QString& name, const QString& category, const QString& tf,
                 const QString& entry_logic, const QString& exit_logic, double sl, double tp,
                 const QJsonArray& entry, const QJsonArray& exit) {
        AlgoStrategy s;
        s.id = id;
        s.name = name;
        s.description = category; // shown in the CATEGORY column / used for filtering
        s.timeframe = tf;
        s.entry_logic = entry_logic;
        s.exit_logic = exit_logic;
        s.stop_loss = sl;
        s.take_profit = tp;
        s.entry_conditions = entry;
        s.exit_conditions = exit;
        return s;
    };

    QVector<AlgoStrategy> v;

    // ── Trend Following ──────────────────────────────────────────────────────
    v << mk("LIB-GOLDEN-CROSS", "Golden Cross", "Trend Following", "1d", "AND", "AND", 5, 15,
            {ci("SMA", {{"period", 50}}, "value", "crosses_above", "SMA", {{"period", 200}}, "value")},
            {ci("SMA", {{"period", 50}}, "value", "crosses_below", "SMA", {{"period", 200}}, "value")});
    v << mk("LIB-EMA-CROSS", "EMA 9/21 Cross", "Trend Following", "15m", "AND", "AND", 2, 5,
            {ci("EMA", {{"period", 9}}, "value", "crosses_above", "EMA", {{"period", 21}}, "value")},
            {ci("EMA", {{"period", 9}}, "value", "crosses_below", "EMA", {{"period", 21}}, "value")});
    v << mk("LIB-TRIPLE-EMA", "Triple EMA Stack", "Trend Following", "15m", "AND", "AND", 2, 6,
            {ci("EMA", {{"period", 9}}, "value", ">", "EMA", {{"period", 21}}, "value"),
             ci("EMA", {{"period", 21}}, "value", ">", "EMA", {{"period", 50}}, "value")},
            {ci("EMA", {{"period", 9}}, "value", "crosses_below", "EMA", {{"period", 21}}, "value")});
    v << mk("LIB-SUPERTREND", "SuperTrend Follow", "Trend Following", "15m", "AND", "AND", 3, 9,
            {ci("CLOSE", {}, "value", "crosses_above", "SUPERTREND", {{"period", 10}, {"multiplier", 3}}, "value")},
            {ci("CLOSE", {}, "value", "crosses_below", "SUPERTREND", {{"period", 10}, {"multiplier", 3}}, "value")});
    v << mk("LIB-ADX-TREND", "ADX Trend Filter", "Trend Following", "1h", "AND", "OR", 3, 9,
            {cv("ADX", {{"period", 14}}, "value", ">", 25),
             ci("EMA", {{"period", 9}}, "value", ">", "EMA", {{"period", 21}}, "value")},
            {cv("ADX", {{"period", 14}}, "value", "<", 20),
             ci("EMA", {{"period", 9}}, "value", "crosses_below", "EMA", {{"period", 21}}, "value")});
    v << mk("LIB-ICHIMOKU", "Ichimoku Cloud Break", "Trend Following", "1d", "AND", "AND", 4, 12,
            {ci("CLOSE", {}, "value", "crosses_above", "ICHIMOKU",
                {{"tenkan", 9}, {"kijun", 26}, {"senkou", 52}}, "senkou_a")},
            {ci("CLOSE", {}, "value", "crosses_below", "ICHIMOKU",
                {{"tenkan", 9}, {"kijun", 26}, {"senkou", 52}}, "kijun_sen")});
    // Breakout vs the PRIOR bar's 20-bar channel (compare_offset=1); comparing
    // against the current bar's band is impossible since close is inside it.
    v << mk("LIB-DONCHIAN-BREAK", "Donchian Breakout", "Trend Following", "1h", "AND", "AND", 3, 9,
            {cio("CLOSE", {}, "value", "crosses_above", "DONCHIAN", {{"period", 20}}, "upper", 1)},
            {cio("CLOSE", {}, "value", "crosses_below", "DONCHIAN", {{"period", 20}}, "lower", 1)});

    // ── Momentum ─────────────────────────────────────────────────────────────
    v << mk("LIB-RSI-REVERSAL", "RSI Reversal", "Momentum", "1h", "AND", "AND", 3, 8,
            {cv("RSI", {{"period", 14}}, "value", "<", 30)},
            {cv("RSI", {{"period", 14}}, "value", ">", 60)});
    v << mk("LIB-RSI-BREAKOUT", "RSI Momentum Breakout", "Momentum", "15m", "AND", "AND", 2, 6,
            {cv("RSI", {{"period", 14}}, "value", "crosses_above", 60)},
            {cv("RSI", {{"period", 14}}, "value", "crosses_below", 40)});
    v << mk("LIB-MACD-CROSS", "MACD Signal Cross", "Momentum", "15m", "AND", "AND", 2, 6,
            {ci("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "line", "crosses_above", "MACD",
                {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line")},
            {ci("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "line", "crosses_below", "MACD",
                {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line")});
    v << mk("LIB-STOCH-CROSS", "Stochastic Cross", "Momentum", "15m", "AND", "AND", 2, 6,
            {ci("STOCHASTIC", {{"k_period", 14}, {"d_period", 3}}, "k", "crosses_above", "STOCHASTIC",
                {{"k_period", 14}, {"d_period", 3}}, "d")},
            {ci("STOCHASTIC", {{"k_period", 14}, {"d_period", 3}}, "k", "crosses_below", "STOCHASTIC",
                {{"k_period", 14}, {"d_period", 3}}, "d")});
    v << mk("LIB-CCI", "CCI Zero-Line", "Momentum", "1h", "AND", "AND", 3, 8,
            {cv("CCI", {{"period", 20}}, "value", "crosses_above", -100)},
            {cv("CCI", {{"period", 20}}, "value", "crosses_below", 100)});
    v << mk("LIB-WILLIAMS-R", "Williams %R Reversal", "Momentum", "1h", "AND", "AND", 3, 8,
            {cv("WILLIAMS_R", {{"period", 14}}, "value", "crosses_above", -80)},
            {cv("WILLIAMS_R", {{"period", 14}}, "value", "crosses_below", -20)});
    v << mk("LIB-ROC-MOMENTUM", "Rate-of-Change Momentum", "Momentum", "1d", "AND", "AND", 4, 12,
            {cv("ROC", {{"period", 12}}, "value", ">", 0), cv("ROC", {{"period", 12}}, "value", "rising", 0)},
            {cv("ROC", {{"period", 12}}, "value", "<", 0)});

    // ── Mean Reversion / Volatility ──────────────────────────────────────────
    v << mk("LIB-BB-BREAKOUT", "Bollinger Breakout", "Volatility", "1h", "AND", "AND", 3, 9,
            {ci("CLOSE", {}, "value", "crosses_above", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "upper")},
            {ci("CLOSE", {}, "value", "crosses_below", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "middle")});
    v << mk("LIB-BB-BOUNCE", "Bollinger Bounce", "Mean Reversion", "1h", "AND", "AND", 3, 6,
            {ci("CLOSE", {}, "value", "<", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "lower")},
            {ci("CLOSE", {}, "value", ">", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "middle")});
    v << mk("LIB-KELTNER-BREAK", "Keltner Breakout", "Volatility", "1h", "AND", "AND", 3, 9,
            {ci("CLOSE", {}, "value", "crosses_above", "KELTNER", {{"period", 20}, {"multiplier", 2}}, "upper")},
            {ci("CLOSE", {}, "value", "crosses_below", "KELTNER", {{"period", 20}, {"multiplier", 2}}, "middle")});

    // ── Volume ───────────────────────────────────────────────────────────────
    v << mk("LIB-OBV-TREND", "OBV Trend Confirm", "Volume", "1d", "AND", "OR", 4, 12,
            {cv("OBV", {}, "value", "rising", 0),
             ci("CLOSE", {}, "value", ">", "EMA", {{"period", 21}}, "value")},
            {cv("OBV", {}, "value", "falling", 0)});
    v << mk("LIB-CMF", "Chaikin Money Flow", "Volume", "1d", "AND", "AND", 4, 10,
            {cv("CMF", {{"period", 20}}, "value", "crosses_above", 0)},
            {cv("CMF", {{"period", 20}}, "value", "crosses_below", 0)});
    v << mk("LIB-MFI", "Money Flow Index", "Volume", "1h", "AND", "AND", 3, 8,
            {cv("MFI", {{"period", 14}}, "value", "<", 20)},
            {cv("MFI", {{"period", 14}}, "value", ">", 80)});

    // ── Multi-Indicator combos ───────────────────────────────────────────────
    v << mk("LIB-RSI-MACD", "RSI + MACD Confluence", "Multi-Indicator", "15m", "AND", "OR", 3, 9,
            {cv("RSI", {{"period", 14}}, "value", "<", 45),
             ci("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "line", "crosses_above", "MACD",
                {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line")},
            {cv("RSI", {{"period", 14}}, "value", ">", 70),
             ci("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "line", "crosses_below", "MACD",
                {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line")});
    v << mk("LIB-TREND-PULLBACK", "Trend Pullback", "Multi-Indicator", "1h", "AND", "AND", 3, 9,
            {ci("EMA", {{"period", 50}}, "value", ">", "EMA", {{"period", 200}}, "value"),
             cv("RSI", {{"period", 14}}, "value", "<", 40)},
            {cv("RSI", {{"period", 14}}, "value", ">", 65)});
    v << mk("LIB-BB-RSI", "Bollinger + RSI Oversold", "Multi-Indicator", "1h", "AND", "AND", 3, 7,
            {ci("CLOSE", {}, "value", "<", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "lower"),
             cv("RSI", {{"period", 14}}, "value", "<", 30)},
            {ci("CLOSE", {}, "value", ">", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "middle")});
    v << mk("LIB-VWAP-RECLAIM", "VWAP Reclaim", "Multi-Indicator", "5m", "AND", "AND", 1, 3,
            {ci("CLOSE", {}, "value", "crosses_above", "VWAP", {}, "value")},
            {ci("CLOSE", {}, "value", "crosses_below", "VWAP", {}, "value")});

    // ── Simple / Easy-Trigger ────────────────────────────────────────────────
    // State-based (>, <) NOT crossover events, so the entry is true for many bars
    // and fires on the next bar after deploy whenever the condition already holds —
    // unlike crosses_above presets that wait for a one-bar crossing. Single-condition
    // entries with tight take-profit make these the easiest starting points to edit
    // and deploy live. Adjust the threshold / SL / TP in the Builder to taste.

    // Price holds above the 20 EMA → ride it; exit when it slips below.
    v << mk("LIB-SIMPLE-EMA20", "Price Above EMA 20", "Simple", "15m", "AND", "AND", 1.5, 1.5,
            {ci("CLOSE", {}, "value", ">", "EMA", {{"period", 20}}, "value")},
            {ci("CLOSE", {}, "value", "<", "EMA", {{"period", 20}}, "value")});
    // Buy any RSI dip under 45, take profit on the bounce past 55 (or TP/SL first).
    v << mk("LIB-SIMPLE-RSI-DIP", "Simple RSI Dip", "Simple", "15m", "AND", "AND", 1.5, 1.5,
            {cv("RSI", {{"period", 14}}, "value", "<", 45)},
            {cv("RSI", {{"period", 14}}, "value", ">", 55)});
    // Fast scalp: above the 9 EMA, grab ~0.8% then out (mirrors a tight fixed-target plan).
    v << mk("LIB-SIMPLE-SCALP", "Quick EMA Scalp", "Simple", "5m", "AND", "AND", 0.5, 0.8,
            {ci("CLOSE", {}, "value", ">", "EMA", {{"period", 9}}, "value")},
            {ci("CLOSE", {}, "value", "<", "EMA", {{"period", 9}}, "value")});
    // Intraday long bias: trade only while price is above the session VWAP.
    v << mk("LIB-SIMPLE-VWAP", "Price Above VWAP", "Simple", "5m", "AND", "AND", 1, 1,
            {ci("CLOSE", {}, "value", ">", "VWAP", {}, "value")},
            {ci("CLOSE", {}, "value", "<", "VWAP", {}, "value")});
    // Momentum on: MACD histogram positive; off when it turns negative.
    v << mk("LIB-SIMPLE-MACD-HIST", "MACD Histogram Bull", "Simple", "15m", "AND", "AND", 1.5, 2,
            {cv("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "histogram", ">", 0)},
            {cv("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "histogram", "<", 0)});
    // Hold the uptrend: fast EMA above slow EMA; exit when it flips.
    v << mk("LIB-SIMPLE-TREND", "EMA Trend Hold", "Simple", "1h", "AND", "AND", 2, 4,
            {ci("EMA", {{"period", 20}}, "value", ">", "EMA", {{"period", 50}}, "value")},
            {ci("EMA", {{"period", 20}}, "value", "<", "EMA", {{"period", 50}}, "value")});

    return v;
}

} // namespace fincept::services::algo
