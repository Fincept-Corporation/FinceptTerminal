#pragma once

#include <QVector>

namespace fincept::ui {

struct BollingerResult {
    QVector<double> upper;
    QVector<double> middle;
    QVector<double> lower;
};

struct PivotResult {
    double pp = 0;
    double r1 = 0, r2 = 0, r3 = 0;
    double s1 = 0, s2 = 0, s3 = 0;
};

enum class PivotType { Standard, Camarilla, Woodie, Fibonacci };

namespace indicators {

QVector<double> sma(const QVector<double>& values, int period);

/// Exponential moving average, seeded with the SMA of the first `period`
/// values (pandas-ta convention). Output[i] is NaN for i < period - 1.
QVector<double> ema(const QVector<double>& values, int period);

QVector<double> vwap(const QVector<double>& highs,
                     const QVector<double>& lows,
                     const QVector<double>& closes,
                     const QVector<double>& volumes);

QVector<double> vwap_std_dev(const QVector<double>& highs,
                             const QVector<double>& lows,
                             const QVector<double>& closes,
                             const QVector<double>& volumes,
                             const QVector<double>& vwap_values);

BollingerResult bollinger(const QVector<double>& closes, int period = 20, double num_std = 2.0);

PivotResult pivot_points(double high, double low, double close, PivotType type = PivotType::Standard);

} // namespace indicators
} // namespace fincept::ui
