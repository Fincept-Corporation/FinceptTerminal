#pragma once
// AltLiveMath.h — the arithmetic behind the analyzers' LIVE DATA prefill (A4), Qt Core only so it is
// unit-tested (tests/tst_alt_live_math.cpp): a par yield read off the Treasury curve at the bond's
// own maturity, and the futures contract nearest to "n months from now".

#include <QDate>
#include <QVector>

#include <cmath>
#include <limits>

namespace fincept::screens::alt_live {

/// Linear interpolation of `yields` (percent) over `tenors` (years, ascending) at `years`; flat
/// beyond the first / last tenor. Points whose yield is not finite are skipped. NaN when no point.
inline double interpolate(const QVector<double>& tenors, const QVector<double>& yields, double years) {
    QVector<double> t;
    QVector<double> y;
    for (int i = 0; i < tenors.size() && i < yields.size(); ++i)
        if (std::isfinite(tenors.at(i)) && std::isfinite(yields.at(i))) {
            t << tenors.at(i);
            y << yields.at(i);
        }
    if (t.isEmpty() || !std::isfinite(years))
        return std::numeric_limits<double>::quiet_NaN();
    if (years <= t.first())
        return y.first();
    if (years >= t.last())
        return y.last();
    for (int i = 1; i < t.size(); ++i) {
        if (years <= t.at(i)) {
            const double span = t.at(i) - t.at(i - 1);
            if (span <= 0)
                return y.at(i);
            const double w = (years - t.at(i - 1)) / span;
            return y.at(i - 1) + w * (y.at(i) - y.at(i - 1));
        }
    }
    return y.last();
}

/// Index into `months` (first day of each contract month) of the contract nearest to `target`,
/// counting only contracts whose `prices` entry is finite and positive; a tie goes to the later
/// month. -1 when none qualifies.
inline int nearest_contract(const QVector<QDate>& months, const QVector<double>& prices, const QDate& target) {
    int best = -1;
    qint64 best_gap = 0;
    for (int i = 0; i < months.size(); ++i) {
        const double p = i < prices.size() ? prices.at(i) : std::numeric_limits<double>::quiet_NaN();
        if (!months.at(i).isValid() || !std::isfinite(p) || p <= 0)
            continue;
        const qint64 gap = std::llabs(months.at(i).daysTo(target));
        if (best < 0 || gap < best_gap || (gap == best_gap && months.at(i) > months.at(best))) {
            best = i;
            best_gap = gap;
        }
    }
    return best;
}

} // namespace fincept::screens::alt_live
