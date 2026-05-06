#include "services/alpha_arena/Indicators.h"

#include <cmath>
#include <limits>

namespace fincept::services::alpha_arena {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
} // namespace

QVector<double> ema(const QVector<double>& values, int period) {
    const int n = values.size();
    QVector<double> out(n, kNaN);
    if (period <= 0 || n < period) return out;

    // Seed with SMA of the first `period` values, exactly as pandas-ta does.
    double sum = 0.0;
    for (int i = 0; i < period; ++i) sum += values[i];
    out[period - 1] = sum / period;

    const double alpha = 2.0 / (period + 1);
    for (int i = period; i < n; ++i) {
        out[i] = alpha * values[i] + (1.0 - alpha) * out[i - 1];
    }
    return out;
}

QVector<double> rsi_wilder(const QVector<double>& closes, int period) {
    const int n = closes.size();
    QVector<double> out(n, kNaN);
    if (period <= 0 || n <= period) return out;

    // Seed average gain/loss from the first `period` deltas.
    double gain_sum = 0.0;
    double loss_sum = 0.0;
    for (int i = 1; i <= period; ++i) {
        const double d = closes[i] - closes[i - 1];
        if (d > 0)      gain_sum += d;
        else if (d < 0) loss_sum -= d;
    }
    double avg_gain = gain_sum / period;
    double avg_loss = loss_sum / period;

    auto rsi_from = [](double g, double l) {
        if (l == 0.0) return 100.0;
        const double rs = g / l;
        return 100.0 - 100.0 / (1.0 + rs);
    };

    out[period] = rsi_from(avg_gain, avg_loss);

    // Wilder smoothing for the rest.
    const double inv = 1.0 / period;
    for (int i = period + 1; i < n; ++i) {
        const double d = closes[i] - closes[i - 1];
        const double up = (d > 0) ? d : 0.0;
        const double dn = (d < 0) ? -d : 0.0;
        avg_gain = (avg_gain * (period - 1) + up) * inv;
        avg_loss = (avg_loss * (period - 1) + dn) * inv;
        out[i] = rsi_from(avg_gain, avg_loss);
    }
    return out;
}

MacdSeries macd(const QVector<double>& closes,
                int fast_period,
                int slow_period,
                int signal_period) {
    const int n = closes.size();
    MacdSeries s;
    s.macd.resize(n);
    s.signal.resize(n);
    s.hist.resize(n);
    s.macd.fill(kNaN);
    s.signal.fill(kNaN);
    s.hist.fill(kNaN);
    if (fast_period <= 0 || slow_period <= 0 || signal_period <= 0) return s;
    if (n < slow_period) return s;

    const auto fast = ema(closes, fast_period);
    const auto slow = ema(closes, slow_period);
    for (int i = 0; i < n; ++i) {
        if (std::isnan(fast[i]) || std::isnan(slow[i])) continue;
        s.macd[i] = fast[i] - slow[i];
    }

    // Signal is EMA of MACD, but only over the defined region. We seed at the
    // first index where macd is defined and we have `signal_period` macd
    // points; that matches pandas-ta's behaviour.
    int first_macd = -1;
    for (int i = 0; i < n; ++i) {
        if (!std::isnan(s.macd[i])) { first_macd = i; break; }
    }
    if (first_macd < 0 || first_macd + signal_period > n) return s;

    double sum = 0.0;
    for (int i = first_macd; i < first_macd + signal_period; ++i) sum += s.macd[i];
    const int seed = first_macd + signal_period - 1;
    s.signal[seed] = sum / signal_period;
    s.hist[seed] = s.macd[seed] - s.signal[seed];

    const double alpha = 2.0 / (signal_period + 1);
    for (int i = seed + 1; i < n; ++i) {
        s.signal[i] = alpha * s.macd[i] + (1.0 - alpha) * s.signal[i - 1];
        s.hist[i] = s.macd[i] - s.signal[i];
    }
    return s;
}

QVector<double> atr_wilder(const QVector<double>& highs,
                           const QVector<double>& lows,
                           const QVector<double>& closes,
                           int period) {
    const int n = closes.size();
    QVector<double> out(n, kNaN);
    if (period <= 0 || n <= period) return out;
    if (highs.size() != n || lows.size() != n) return out;

    // True range series.
    QVector<double> tr(n, 0.0);
    tr[0] = highs[0] - lows[0];
    for (int i = 1; i < n; ++i) {
        const double a = highs[i] - lows[i];
        const double b = std::fabs(highs[i] - closes[i - 1]);
        const double c = std::fabs(lows[i] - closes[i - 1]);
        tr[i] = std::max(a, std::max(b, c));
    }

    // Seed with simple average of first `period` TRs.
    double seed_sum = 0.0;
    for (int i = 0; i < period; ++i) seed_sum += tr[i];
    out[period - 1] = seed_sum / period;

    // Wilder smoothing.
    for (int i = period; i < n; ++i) {
        out[i] = (out[i - 1] * (period - 1) + tr[i]) / period;
    }
    return out;
}

} // namespace fincept::services::alpha_arena
