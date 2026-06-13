#include "ui/charts/ChartIndicators.h"

#include <cmath>
#include <limits>

namespace fincept::ui::indicators {

QVector<double> sma(const QVector<double>& values, int period) {
    const int n = values.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (period < 1 || n < period)
        return out;

    double sum = 0;
    for (int i = 0; i < period; ++i)
        sum += values[i];
    out[period - 1] = sum / period;

    for (int i = period; i < n; ++i) {
        sum += values[i] - values[i - period];
        out[i] = sum / period;
    }
    return out;
}

QVector<double> ema(const QVector<double>& values, int period) {
    const int n = values.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (period < 1 || n < period)
        return out;

    // Seed with SMA of the first `period` values, exactly as pandas-ta does.
    double sum = 0;
    for (int i = 0; i < period; ++i)
        sum += values[i];
    out[period - 1] = sum / period;

    const double alpha = 2.0 / (period + 1);
    for (int i = period; i < n; ++i)
        out[i] = alpha * values[i] + (1.0 - alpha) * out[i - 1];
    return out;
}

QVector<double> vwap(const QVector<double>& highs,
                     const QVector<double>& lows,
                     const QVector<double>& closes,
                     const QVector<double>& volumes) {
    const int n = closes.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (n == 0)
        return out;

    double cum_tp_vol = 0;
    double cum_vol = 0;

    for (int i = 0; i < n; ++i) {
        const double tp = (highs[i] + lows[i] + closes[i]) / 3.0;
        cum_tp_vol += tp * volumes[i];
        cum_vol += volumes[i];
        out[i] = (cum_vol > 0) ? cum_tp_vol / cum_vol : std::numeric_limits<double>::quiet_NaN();
    }
    return out;
}

QVector<double> vwap_std_dev(const QVector<double>& highs,
                             const QVector<double>& lows,
                             const QVector<double>& closes,
                             const QVector<double>& volumes,
                             const QVector<double>& vwap_values) {
    const int n = closes.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (n == 0)
        return out;

    double cum_vol = 0;
    double cum_sq_diff_vol = 0;

    for (int i = 0; i < n; ++i) {
        const double tp = (highs[i] + lows[i] + closes[i]) / 3.0;
        cum_vol += volumes[i];
        const double diff = tp - vwap_values[i];
        cum_sq_diff_vol += diff * diff * volumes[i];
        out[i] = (cum_vol > 0) ? std::sqrt(cum_sq_diff_vol / cum_vol) : 0.0;
    }
    return out;
}

BollingerResult bollinger(const QVector<double>& closes, int period, double num_std) {
    const int n = closes.size();
    BollingerResult result;
    result.upper.resize(n, std::numeric_limits<double>::quiet_NaN());
    result.middle = sma(closes, period);
    result.lower.resize(n, std::numeric_limits<double>::quiet_NaN());

    for (int i = period - 1; i < n; ++i) {
        double sum_sq = 0;
        for (int j = i - period + 1; j <= i; ++j) {
            const double diff = closes[j] - result.middle[i];
            sum_sq += diff * diff;
        }
        const double sd = std::sqrt(sum_sq / period);
        result.upper[i] = result.middle[i] + num_std * sd;
        result.lower[i] = result.middle[i] - num_std * sd;
    }
    return result;
}

PivotResult pivot_points(double high, double low, double close, PivotType type) {
    PivotResult r;
    switch (type) {
    case PivotType::Standard:
        r.pp = (high + low + close) / 3.0;
        r.r1 = 2 * r.pp - low;
        r.s1 = 2 * r.pp - high;
        r.r2 = r.pp + (high - low);
        r.s2 = r.pp - (high - low);
        r.r3 = high + 2 * (r.pp - low);
        r.s3 = low - 2 * (high - r.pp);
        break;
    case PivotType::Camarilla:
        r.pp = (high + low + close) / 3.0;
        r.r1 = close + (high - low) * 1.1 / 12;
        r.s1 = close - (high - low) * 1.1 / 12;
        r.r2 = close + (high - low) * 1.1 / 6;
        r.s2 = close - (high - low) * 1.1 / 6;
        r.r3 = close + (high - low) * 1.1 / 4;
        r.s3 = close - (high - low) * 1.1 / 4;
        break;
    case PivotType::Woodie:
        r.pp = (high + low + 2 * close) / 4.0;
        r.r1 = 2 * r.pp - low;
        r.s1 = 2 * r.pp - high;
        r.r2 = r.pp + (high - low);
        r.s2 = r.pp - (high - low);
        r.r3 = high + 2 * (r.pp - low);
        r.s3 = low - 2 * (high - r.pp);
        break;
    case PivotType::Fibonacci:
        r.pp = (high + low + close) / 3.0;
        r.r1 = r.pp + 0.382 * (high - low);
        r.s1 = r.pp - 0.382 * (high - low);
        r.r2 = r.pp + 0.618 * (high - low);
        r.s2 = r.pp - 0.618 * (high - low);
        r.r3 = r.pp + 1.000 * (high - low);
        r.s3 = r.pp - 1.000 * (high - low);
        break;
    }
    return r;
}

} // namespace fincept::ui::indicators
