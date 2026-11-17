// src/algo_engine/ScanSignal.h
#pragma once
// ScanSignal — what a matched scan rule SAYS about direction, from the rule's
// semantics rather than its operator. `RSI < 30` is an oversold (bullish
// reversal) reading, not "bearish because of <"; `SMA(50) crosses above
// SMA(200)` is bullish, `SMA(200) > SMA(50)` bearish; volume and volatility
// filters say nothing about direction.
//
// Header-only, Qt Core (unit-tested in tests/tst_algo_scan_signal.cpp).

#include <QJsonObject>
#include <QString>

namespace fincept::algo::scan_signal {

enum class Bias { Neutral, Bullish, Bearish };

struct RuleBias {
    Bias bias = Bias::Neutral;
    QString reason; ///< short English key: "oversold", "overbought", "above average", "momentum up", ...
};

namespace detail {

inline bool is_price_like(const QString& ind) {
    return ind == QLatin1String("CLOSE") || ind == QLatin1String("OPEN") || ind == QLatin1String("HIGH") ||
           ind == QLatin1String("LOW") || ind == QLatin1String("VWAP");
}

inline bool is_average_like(const QString& ind) {
    return ind == QLatin1String("SMA") || ind == QLatin1String("EMA") || ind == QLatin1String("WMA") ||
           ind == QLatin1String("DEMA") || ind == QLatin1String("TEMA") || ind == QLatin1String("VWAP") ||
           ind == QLatin1String("SUPERTREND") || ind == QLatin1String("ICHIMOKU") || ind == QLatin1String("KELTNER") ||
           ind == QLatin1String("BOLLINGER") || ind == QLatin1String("DONCHIAN");
}

inline int period_of(const QJsonObject& params) {
    return params.value(QStringLiteral("period")).toInt(0);
}

// Oscillator zones: (oversold below, overbought above, midline).
inline bool oscillator_zones(const QString& ind, const QString& field, double& lo, double& hi, double& mid) {
    if (ind == QLatin1String("RSI") || ind == QLatin1String("MFI")) {
        lo = ind == QLatin1String("RSI") ? 30 : 20;
        hi = ind == QLatin1String("RSI") ? 70 : 80;
        mid = 50;
        return true;
    }
    if (ind == QLatin1String("STOCHASTIC")) {
        lo = 20;
        hi = 80;
        mid = 50;
        return true;
    }
    if (ind == QLatin1String("WILLIAMS_R")) {
        lo = -80;
        hi = -20;
        mid = -50;
        return true;
    }
    if (ind == QLatin1String("CCI")) {
        lo = -100;
        hi = 100;
        mid = 0;
        return true;
    }
    if (ind == QLatin1String("BOLLINGER") && field == QLatin1String("pct_b")) {
        lo = 0;
        hi = 1;
        mid = 0.5;
        return true;
    }
    return false;
}

inline bool greater_op(const QString& op) {
    return op == QLatin1String(">") || op == QLatin1String(">=") || op == QLatin1String("crosses_above");
}
inline bool less_op(const QString& op) {
    return op == QLatin1String("<") || op == QLatin1String("<=") || op == QLatin1String("crosses_below");
}

} // namespace detail

/// Bias of one rule leaf as the scanner / condition JSON stores it
/// ({indicator, params, field, operator, compare_mode, value | compare_indicator, compare_params, compare_field}).
inline RuleBias classify(const QJsonObject& c) {
    using namespace detail;
    const QString ind = c.value(QStringLiteral("indicator")).toString().toUpper();
    const QString field = c.value(QStringLiteral("field")).toString(QStringLiteral("value"));
    const QString op = c.value(QStringLiteral("operator")).toString();
    const bool vs_indicator = c.value(QStringLiteral("compare_mode")).toString() == QLatin1String("indicator");
    const double value = c.value(QStringLiteral("value")).toDouble();

    // Volume / volatility / trend-strength filters carry no direction.
    if (ind == QLatin1String("VOLUME") || ind == QLatin1String("VOL_WIN_CHG") || ind == QLatin1String("ATR") ||
        ind == QLatin1String("OBV") ||
        (ind == QLatin1String("BOLLINGER") && field == QLatin1String("width")) ||
        (ind == QLatin1String("ADX") && field == QLatin1String("value")))
        return {Bias::Neutral, QStringLiteral("filter")};

    if (op == QLatin1String("rising"))
        return {Bias::Bullish, QStringLiteral("rising")};
    if (op == QLatin1String("falling"))
        return {Bias::Bearish, QStringLiteral("falling")};

    // ── Indicator vs indicator ─────────────────────────────────────────────
    if (vs_indicator) {
        const QString rhs = c.value(QStringLiteral("compare_indicator")).toString().toUpper();
        const QString rhs_field = c.value(QStringLiteral("compare_field")).toString(QStringLiteral("value"));
        const bool up = greater_op(op);
        const bool down = less_op(op);
        if (!up && !down)
            return {};
        // MACD line vs its signal line; +DI vs -DI; Aroon up vs down; %K vs %D.
        if ((ind == QLatin1String("MACD") && field == QLatin1String("line") && rhs_field == QLatin1String("signal_line")) ||
            (ind == QLatin1String("ADX") && field == QLatin1String("plus_di") && rhs_field == QLatin1String("minus_di")) ||
            (ind == QLatin1String("AROON") && field == QLatin1String("up") && rhs_field == QLatin1String("down")) ||
            (ind == QLatin1String("STOCHASTIC") && field == QLatin1String("k") && rhs_field == QLatin1String("d")))
            return {up ? Bias::Bullish : Bias::Bearish, QStringLiteral("momentum crossover")};
        if ((ind == QLatin1String("MACD") && field == QLatin1String("signal_line") && rhs_field == QLatin1String("line")) ||
            (ind == QLatin1String("ADX") && field == QLatin1String("minus_di") && rhs_field == QLatin1String("plus_di")) ||
            (ind == QLatin1String("AROON") && field == QLatin1String("down") && rhs_field == QLatin1String("up")))
            return {up ? Bias::Bearish : Bias::Bullish, QStringLiteral("momentum crossover")};
        // Price vs an average / band / trend line.
        if (is_price_like(ind) && is_average_like(rhs)) {
            // Closing below a LOWER band is an oversold reading; above an UPPER band overbought.
            if (rhs_field == QLatin1String("lower") && down)
                return {Bias::Bullish, QStringLiteral("oversold")};
            if (rhs_field == QLatin1String("upper") && up)
                return {Bias::Bearish, QStringLiteral("overbought")};
            return {up ? Bias::Bullish : Bias::Bearish, up ? QStringLiteral("above average") : QStringLiteral("below average")};
        }
        if (is_average_like(ind) && is_price_like(rhs))
            return {up ? Bias::Bearish : Bias::Bullish, up ? QStringLiteral("below average") : QStringLiteral("above average")};
        // Average vs average: the shorter period is the fast line.
        if (is_average_like(ind) && is_average_like(rhs)) {
            const int lp = period_of(c.value(QStringLiteral("params")).toObject());
            const int rp = period_of(c.value(QStringLiteral("compare_params")).toObject());
            if (lp > 0 && rp > 0 && lp != rp) {
                const bool fast_on_left = lp < rp;
                const bool bull = fast_on_left ? up : down;
                return {bull ? Bias::Bullish : Bias::Bearish, QStringLiteral("trend crossover")};
            }
            return {};
        }
        return {};
    }

    // ── Indicator vs a fixed value ─────────────────────────────────────────
    double lo = 0, hi = 0, mid = 0;
    if (oscillator_zones(ind, field, lo, hi, mid)) {
        if (op == QLatin1String("between")) {
            const double v2 = c.value(QStringLiteral("value2")).toDouble();
            if (v2 <= lo)
                return {Bias::Bullish, QStringLiteral("oversold")};
            if (value >= hi)
                return {Bias::Bearish, QStringLiteral("overbought")};
            return {};
        }
        if (less_op(op)) {
            // Crossing DOWN out of the overbought zone is the classic sell trigger.
            if (op == QLatin1String("crosses_below") && value >= hi)
                return {Bias::Bearish, QStringLiteral("leaving overbought")};
            if (value <= lo)
                return {Bias::Bullish, QStringLiteral("oversold")};
            if (value <= mid)
                return {Bias::Bearish, QStringLiteral("momentum down")};
            return {};
        }
        if (greater_op(op)) {
            // Crossing UP out of the oversold zone is the classic buy trigger.
            if (op == QLatin1String("crosses_above") && value <= lo)
                return {Bias::Bullish, QStringLiteral("leaving oversold")};
            if (value >= hi)
                return op == QLatin1String("crosses_above") ? RuleBias{Bias::Bullish, QStringLiteral("breakout")}
                                                            : RuleBias{Bias::Bearish, QStringLiteral("overbought")};
            if (value >= mid)
                return {Bias::Bullish, QStringLiteral("momentum up")};
            return {};
        }
        return {};
    }
    // Zero-line momentum: MACD line / histogram, ROC.
    if (ind == QLatin1String("MACD") || ind == QLatin1String("ROC") || ind == QLatin1String("CMF")) {
        if (greater_op(op) && value >= 0)
            return {Bias::Bullish, QStringLiteral("momentum up")};
        if (less_op(op) && value <= 0)
            return {Bias::Bearish, QStringLiteral("momentum down")};
        return {};
    }
    if (ind == QLatin1String("SUPERTREND") && field == QLatin1String("direction")) {
        if (greater_op(op) && value >= 0)
            return {Bias::Bullish, QStringLiteral("uptrend")};
        if (less_op(op) && value <= 0)
            return {Bias::Bearish, QStringLiteral("downtrend")};
        return {};
    }
    // A price level: breaking above is bullish, below bearish.
    if (is_price_like(ind) || is_average_like(ind)) {
        if (greater_op(op))
            return {Bias::Bullish, QStringLiteral("above level")};
        if (less_op(op))
            return {Bias::Bearish, QStringLiteral("below level")};
    }
    return {};
}

inline RuleBias classify(const QJsonValue& v) {
    return classify(v.toObject());
}

/// Net bias of the MATCHED rules: bullish when every directional rule says up,
/// bearish when every one says down, neutral otherwise (mixed or none).
template <typename Range>
inline Bias combine(const Range& matched_rules) {
    int up = 0, down = 0;
    for (const auto& r : matched_rules) {
        const RuleBias b = classify(r);
        if (b.bias == Bias::Bullish)
            ++up;
        else if (b.bias == Bias::Bearish)
            ++down;
    }
    if (up > 0 && down == 0)
        return Bias::Bullish;
    if (down > 0 && up == 0)
        return Bias::Bearish;
    return Bias::Neutral;
}

} // namespace fincept::algo::scan_signal
