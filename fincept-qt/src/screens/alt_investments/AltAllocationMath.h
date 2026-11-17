#pragma once
// AltAllocationMath.h — the arithmetic of the ALLOCATION & LIQUIDITY view (A9), Qt Core only and
// unit-tested (tests/tst_alt_allocation_math.cpp).
//
// Positions are the portfolio's listed holdings (sellable within a day), private-market funds
// (locked; their unfunded commitments are future calls) and alternative holdings entered by hand
// with their own liquidity. The view answers: how much is alternative, how quickly could cash be
// raised, how many times do assets liquid within 90 days cover the unfunded commitments, and what
// a public-market fall does to all of that — the "denominator effect": listed assets are marked
// down at once, private NAVs lag (held unchanged), distributions stop and calls continue.

#include <QHash>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::alt_alloc {

/// Liquidity tiers, fastest first; the days each tier needs to raise cash.
enum Tier { Daily = 0, Monthly = 1, Quarterly = 2, Annual = 3, Locked = 4 };
constexpr int kTierCount = 5;
inline int tier_days(int tier) {
    static const int kDays[kTierCount] = {1, 31, 92, 366, std::numeric_limits<int>::max()};
    return tier >= 0 && tier < kTierCount ? kDays[tier] : std::numeric_limits<int>::max();
}
inline int tier_from_key(const QString& key) {
    if (key == QLatin1String("daily"))
        return Daily;
    if (key == QLatin1String("monthly"))
        return Monthly;
    if (key == QLatin1String("quarterly"))
        return Quarterly;
    if (key == QLatin1String("annual"))
        return Annual;
    return Locked;
}

struct Position {
    QString name;
    QString asset_class;   ///< "equity", "bond", "private_equity", "real_estate", ...
    double value = 0;      ///< base currency
    int tier = Daily;
    double unfunded = 0;   ///< commitments not yet called (funds)
    bool alternative = false;
};

inline double total_value(const QVector<Position>& ps) {
    double t = 0;
    for (const Position& p : ps)
        t += std::max(0.0, p.value);
    return t;
}

inline double alternative_value(const QVector<Position>& ps) {
    double t = 0;
    for (const Position& p : ps)
        if (p.alternative)
            t += std::max(0.0, p.value);
    return t;
}

inline double total_unfunded(const QVector<Position>& ps) {
    double t = 0;
    for (const Position& p : ps)
        t += std::max(0.0, p.unfunded);
    return t;
}

/// Value that can be turned into cash within `days`.
inline double liquid_within(const QVector<Position>& ps, int days) {
    double t = 0;
    for (const Position& p : ps)
        if (tier_days(p.tier) <= days)
            t += std::max(0.0, p.value);
    return t;
}

/// Cumulative value available by the end of each tier (the liquidity ladder).
inline QVector<double> ladder(const QVector<Position>& ps) {
    QVector<double> by(kTierCount, 0.0);
    for (const Position& p : ps)
        by[std::clamp(p.tier, 0, kTierCount - 1)] += std::max(0.0, p.value);
    for (int i = 1; i < kTierCount; ++i)
        by[i] += by[i - 1];
    return by;
}

/// Value by asset class.
inline QHash<QString, double> by_class(const QVector<Position>& ps) {
    QHash<QString, double> out;
    for (const Position& p : ps)
        out[p.asset_class] += std::max(0.0, p.value);
    return out;
}

/// Times the assets liquid within 90 days cover the unfunded commitments (NaN when none unfunded).
inline double unfunded_cover(const QVector<Position>& ps) {
    const double u = total_unfunded(ps);
    if (!(u > 0))
        return std::numeric_limits<double>::quiet_NaN();
    return liquid_within(ps, 92) / u;
}

struct StressResult {
    double public_shock_pct = 0;
    double calls_pct = 0;
    double total = 0;
    double alternatives = 0;
    double alt_weight_pct = 0;
    double liquid_90d = 0;
    double unfunded = 0;
    double cover = std::numeric_limits<double>::quiet_NaN();
};

/// Listed (non-alternative, daily) assets move by `public_shock_pct`; private NAVs are held; then
/// `calls_pct` of every unfunded commitment is called and paid from the daily-liquid assets
/// (raising those funds' NAV by the same amount); no distributions arrive.
inline StressResult stress(const QVector<Position>& ps, double public_shock_pct, double calls_pct) {
    QVector<Position> s = ps;
    for (Position& p : s)
        if (!p.alternative && p.tier == Daily)
            p.value *= (1.0 + public_shock_pct / 100.0);
    double calls = 0;
    for (Position& p : s) {
        if (p.unfunded > 0) {
            const double c = p.unfunded * std::clamp(calls_pct, 0.0, 100.0) / 100.0;
            p.unfunded -= c;
            p.value += c;
            calls += c;
        }
    }
    // Pay the calls out of daily-liquid, non-alternative assets, pro rata.
    double liquid = 0;
    for (const Position& p : s)
        if (!p.alternative && p.tier == Daily)
            liquid += std::max(0.0, p.value);
    if (liquid > 0) {
        const double keep = std::max(0.0, 1.0 - calls / liquid);
        for (Position& p : s)
            if (!p.alternative && p.tier == Daily)
                p.value *= keep;
    }
    StressResult r;
    r.public_shock_pct = public_shock_pct;
    r.calls_pct = calls_pct;
    r.total = total_value(s);
    r.alternatives = alternative_value(s);
    r.alt_weight_pct = r.total > 0 ? r.alternatives / r.total * 100.0 : 0.0;
    r.liquid_90d = liquid_within(s, 92);
    r.unfunded = total_unfunded(s);
    r.cover = unfunded_cover(s);
    return r;
}

} // namespace fincept::screens::alt_alloc
