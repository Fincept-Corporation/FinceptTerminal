// src/services/equity/EquityTechnicalMath.h
//
// Pure, synchronous price-series math for the Equity Research screen: indicator series
// for chart overlays, the two-group technical rating (moving averages vs oscillators),
// bar resampling and performance / risk statistics. No Qt event loop, no I/O; safe on
// a worker thread. Every series is aligned with its input bars, oldest first; NaN marks
// "no value" (warm-up, missing input).
#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QString>
#include <QVector>

#include <limits>

namespace fincept::services::equity::tmath {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// OHLCV columns + bar open times (epoch seconds), oldest first.
struct Bars {
    QVector<qint64> time;
    QVector<double> open, high, low, close, volume;
    int size() const { return static_cast<int>(close.size()); }
};

/// Candles (any order, duplicates allowed) → sorted, de-duplicated Bars. Bars with a
/// non-finite or non-positive close are dropped.
Bars bars_from_candles(const QVector<Candle>& candles);

/// A daily bar's timestamp (the exchange's local midnight as epoch seconds — 15:00 UTC the
/// day before for Tokyo, 23:00 UTC the day before for London in summer) → UTC midnight of
/// that trading date, so a chart in UTC shows the right day on every market.
qint64 daily_utc_midnight(qint64 epoch_secs);
/// Daily candles with daily_utc_midnight() timestamps, sorted.
QVector<Candle> normalize_daily(QVector<Candle> candles);

enum class Resample { Daily, Weekly, Monthly };
/// Weekly (ISO week) / monthly bars built from daily bars; Daily returns a copy.
Bars resample(const Bars& daily, Resample to);

// ── Indicator series ─────────────────────────────────────────────────────────
QVector<double> sma(const QVector<double>& v, int n);
QVector<double> ema(const QVector<double>& v, int n);
QVector<double> wma(const QVector<double>& v, int n);
QVector<double> hma(const QVector<double>& v, int n);
QVector<double> vwma(const QVector<double>& close, const QVector<double>& volume, int n);
QVector<double> rsi(const QVector<double>& close, int n = 14);
struct Macd {
    QVector<double> line, signal, hist;
};
Macd macd(const QVector<double>& close, int fast = 12, int slow = 26, int signal = 9);
struct Bands {
    QVector<double> mid, upper, lower;
};
Bands bollinger(const QVector<double>& close, int n = 20, double k = 2.0);
struct Stoch {
    QVector<double> k, d;
};
Stoch stochastic(const Bars& b, int n = 14, int smooth_k = 3, int smooth_d = 3);
Stoch stoch_rsi(const QVector<double>& close, int rsi_n = 14, int stoch_n = 14, int smooth_k = 3, int smooth_d = 3);
QVector<double> cci(const Bars& b, int n = 20);
struct Adx {
    QVector<double> adx, plus_di, minus_di;
};
Adx adx(const Bars& b, int n = 14);
QVector<double> atr(const Bars& b, int n = 14);
QVector<double> awesome_oscillator(const Bars& b);
QVector<double> momentum(const QVector<double>& close, int n = 10);
QVector<double> williams_r(const Bars& b, int n = 14);
QVector<double> ultimate_oscillator(const Bars& b, int s = 7, int m = 14, int l = 28);
QVector<double> bull_bear_power(const Bars& b, int n = 13);
QVector<double> ichimoku_base(const Bars& b, int n = 26);
QVector<double> obv(const Bars& b);

// ── Technical rating ─────────────────────────────────────────────────────────
/// Moving averages vote price vs average (above = Buy, below = Sell); oscillators vote
/// their own buy / sell zones with a direction test. Each group's score is
/// (buys − sells) / voters in −1..1 — only indicators that actually vote are counted, so
/// informational readings (ATR, band width, OBV …) can no longer dilute the rating — and
/// the summary is the mean of the two group scores.
struct Rating {
    RatingGroup ma;
    RatingGroup osc;
    double summary = kNaN;
    TechSignal summary_signal = TechSignal::Neutral;
    bool ok = false;
};
Rating technical_rating(const Bars& b);
/// −1..1 score → signal: > 0.5 strong buy, > 0.1 buy, ≥ −0.1 neutral, ≥ −0.5 sell, else strong sell.
TechSignal signal_for_score(double score);

// ── Performance / risk ───────────────────────────────────────────────────────
/// Simple return from the last close at or before `from_secs` to the last close; NaN when
/// the series does not reach back that far.
double return_since(const Bars& b, qint64 from_secs);
/// Annualised volatility of log returns over the last `window` bars (all bars when ≤ 0).
double realized_vol(const Bars& b, int window, double periods_per_year = 252.0);
/// Drawdown from the running peak at every bar (≤ 0, fraction).
QVector<double> drawdown(const QVector<double>& close);
/// Largest peak-to-trough fall over the bars since `from_secs` (≤ 0, fraction).
double max_drawdown_since(const Bars& b, qint64 from_secs);
/// OLS beta of daily log returns of `b` against `bench` over the last `window` common days.
double beta_vs(const Bars& b, const Bars& bench, int window = 252);

} // namespace fincept::services::equity::tmath
