#pragma once
// Indicators — minimal, allocation-light technical indicator helpers.
//
// These are the only indicators the Alpha Arena prompt exposes (per Nof1
// Season 1): EMA, MACD, RSI (Wilder), ATR. Implementations are validated
// against pandas-ta within 1e-6 in the test suite (Phase 3 exit criterion).
//
// All functions return a vector of doubles aligned with the input. Positions
// where the indicator is undefined (warmup) are filled with quiet NaN —
// callers should `std::isnan()`-check before formatting into prompts.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §4.4 (Indicators).

#include <QVector>

namespace fincept::services::alpha_arena {

/// Exponential moving average. period must be ≥ 1.
/// Output[i] is NaN for i < period - 1; output[period - 1] = SMA(values[0..period-1]).
QVector<double> ema(const QVector<double>& values, int period);

/// Wilder's RSI. period must be ≥ 1. Returns NaN until period bars are seen.
QVector<double> rsi_wilder(const QVector<double>& closes, int period);

/// Result of macd(): three series of equal length to `closes`.
struct MacdSeries {
    QVector<double> macd;
    QVector<double> signal;
    QVector<double> hist;
};

/// MACD with EMA-based signal line. Defaults match TradingView/pandas-ta.
MacdSeries macd(const QVector<double>& closes,
                int fast_period = 12,
                int slow_period = 26,
                int signal_period = 9);

/// Wilder ATR. highs/lows/closes must have equal length. period must be ≥ 1.
QVector<double> atr_wilder(const QVector<double>& highs,
                           const QVector<double>& lows,
                           const QVector<double>& closes,
                           int period);

} // namespace fincept::services::alpha_arena
