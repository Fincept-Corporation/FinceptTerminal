#pragma once
// MaFootballFieldMath.h — the valuation ranges of the M&A football field, built from the user's own runs
// (Qt Core only, header-inline, so the rules are unit-tested: tests/tst_ma_football_field.cpp).
//
//   DCF               min - max of the WACC x terminal-growth grid (point = the base case)
//   Trading comps     Q1 - Q3 of the target value implied by the peers' EV/EBITDA and EV/Revenue
//   Precedents        Q1 - Q3 of the value implied by the precedent deals' EV/EBITDA and EV/Revenue
//   LBO               the entry enterprise value a buyer can pay for a target IRR band (interpolated along the
//                     entry multiples of the LBO sensitivity grid at the base exit multiple); left out when the
//                     band is outside the grid
//   52-week           low - high of the share price
//   Manual            a range the user typed
//
// Values are in one basis: enterprise value, or value per share = (EV - debt + cash) / shares. A range that needs
// the bridge (shares, debt, cash) when none is known is left out with a note — never estimated.

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::ma_ff {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

enum class Basis { PerShare, EnterpriseValue };

/// Shares / debt / cash that turn an enterprise value into a value per share.
struct Bridge {
    double shares = kNaN;
    double debt = kNaN;
    double cash = kNaN;
    bool valid() const { return std::isfinite(shares) && shares > 0 && std::isfinite(debt) && std::isfinite(cash); }
};

inline double ev_to_per_share(double ev, const Bridge& b) {
    return (b.valid() && std::isfinite(ev)) ? (ev - b.debt + b.cash) / b.shares : kNaN;
}
inline double per_share_to_ev(double p, const Bridge& b) {
    return (b.valid() && std::isfinite(p)) ? p * b.shares + b.debt - b.cash : kNaN;
}

/// One bar. `basis_ev` = the source values are enterprise values (else values per share).
struct Range {
    QString key;   ///< "dcf", "comps_ev_ebitda", ...
    QString label; ///< "DCF", "Trading comps · EV/EBITDA"
    double low = kNaN;
    double high = kNaN;
    double mid = kNaN; ///< the central case (median / base cell), NaN when there is none
    bool basis_ev = true;
    QString note; ///< what the range is ("WACC 8.9-10.9%, growth 1.5-3.5%")

    bool valid() const { return std::isfinite(low) && std::isfinite(high) && high >= low; }
};

inline double num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : kNaN;
}

/// Finite min / max of a JSON matrix ([[...], ...]); false when nothing is finite.
inline bool matrix_range(const QJsonArray& m, double* lo, double* hi) {
    double mn = std::numeric_limits<double>::max(), mx = std::numeric_limits<double>::lowest();
    for (const QJsonValue& row : m)
        for (const QJsonValue& c : row.toArray()) {
            const double v = num(c);
            if (std::isfinite(v)) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
        }
    if (mn > mx)
        return false;
    *lo = mn;
    *hi = mx;
    return true;
}

inline double matrix_at(const QJsonArray& m, int r, int c) {
    return (r >= 0 && r < m.size()) ? num(m.at(r).toArray().at(c)) : kNaN;
}

/// DCF: the result of dcf_model.py calculate (flattened data). EV basis from the grid's enterprise values (the
/// per-share grid is used directly in per-share basis); without a grid the base case is a point.
inline Range dcf_range(const QJsonObject& p, Basis basis) {
    Range r;
    r.key = QStringLiteral("dcf");
    r.label = QStringLiteral("DCF");
    const QJsonObject sens = p.value(QStringLiteral("sensitivity")).toObject();
    const QJsonObject summary = p.value(QStringLiteral("summary")).toObject();
    const bool per_share = basis == Basis::PerShare;
    r.basis_ev = !per_share;
    const QString key = per_share ? QStringLiteral("price_per_share") : QStringLiteral("enterprise_value");
    const QJsonArray m = sens.value(key).toArray();
    double lo = kNaN, hi = kNaN;
    if (!m.isEmpty() && matrix_range(m, &lo, &hi)) {
        r.low = lo;
        r.high = hi;
        r.mid = matrix_at(m, sens.value(QStringLiteral("base_row")).toInt(-1), sens.value(QStringLiteral("base_col")).toInt(-1));
        const QJsonArray w = sens.value(QStringLiteral("wacc_values")).toArray();
        const QJsonArray g = sens.value(QStringLiteral("terminal_growth_values")).toArray();
        if (!w.isEmpty() && !g.isEmpty())
            r.note = QStringLiteral("WACC %1-%2%, terminal growth %3-%4%")
                         .arg(num(w.first()), 0, 'f', 1)
                         .arg(num(w.last()), 0, 'f', 1)
                         .arg(num(g.first()), 0, 'f', 1)
                         .arg(num(g.last()), 0, 'f', 1);
    } else {
        const double v = num(summary.value(key));
        r.low = r.high = r.mid = v;
        r.note = QStringLiteral("base case only (no sensitivity grid)");
    }
    return r;
}

/// Q1-Q3 rows of a `target_valuation.valuations` object (trading comps / precedents): EV/EBITDA and EV/Revenue.
inline QVector<Range> multiple_ranges(const QJsonObject& p, const QString& key_prefix, const QString& label_prefix) {
    QVector<Range> out;
    const QJsonObject v =
        p.value(QStringLiteral("target_valuation")).toObject().value(QStringLiteral("valuations")).toObject();
    const struct {
        const char* stem;
        const char* name;
    } kinds[] = {{"ev_ebitda", "EV/EBITDA"}, {"ev_revenue", "EV/Revenue"}};
    for (const auto& k : kinds) {
        const QString stem = QString::fromLatin1(k.stem);
        Range r;
        r.key = key_prefix + QLatin1Char('_') + stem;
        r.label = label_prefix + QStringLiteral(" · ") + QString::fromLatin1(k.name);
        r.low = num(v.value(stem + QStringLiteral("_q1")));
        r.high = num(v.value(stem + QStringLiteral("_q3")));
        r.mid = num(v.value(stem + QStringLiteral("_median")));
        r.basis_ev = true;
        r.note = QStringLiteral("interquartile range of the implied value");
        // A multiple the source did not have comes back as 0: not a range.
        if (r.valid() && r.high > 0)
            out << r;
    }
    return out;
}

/// x at which the piecewise-linear y(x) first crosses `target` (xs ascending); NaN when it never does.
inline double crossing(const QVector<double>& xs, const QVector<double>& ys, double target) {
    for (int i = 0; i + 1 < xs.size() && i + 1 < ys.size(); ++i) {
        const double y0 = ys.at(i), y1 = ys.at(i + 1);
        if (!std::isfinite(y0) || !std::isfinite(y1))
            continue;
        if ((y0 - target) * (y1 - target) <= 0 && y0 != y1)
            return xs.at(i) + (target - y0) / (y1 - y0) * (xs.at(i + 1) - xs.at(i));
        if (y0 == target)
            return xs.at(i);
    }
    return kNaN;
}

/// LBO: entry EV for IRRs between `irr_low_pct` and `irr_high_pct` at the base exit multiple of the lbo_model.py
/// sensitivity grid (rows = entry multiples, columns = exit multiples), with `ebitda` the entry EBITDA.
/// The higher IRR gives the lower price. Not valid when either IRR is outside the grid.
inline Range lbo_range(const QJsonObject& p, double ebitda, double irr_low_pct, double irr_high_pct) {
    Range r;
    r.key = QStringLiteral("lbo");
    r.label = QStringLiteral("LBO");
    r.basis_ev = true;
    const QJsonArray entries = p.value(QStringLiteral("entry_multiple_range")).toArray();
    const QJsonArray exits = p.value(QStringLiteral("exit_multiple_range")).toArray();
    const QJsonArray irr = p.value(QStringLiteral("irr_pct_matrix")).toArray();
    if (entries.size() < 2 || exits.isEmpty() || irr.size() != entries.size() || !(ebitda > 0)) {
        r.note = QStringLiteral("needs the LBO sensitivity grid and EBITDA");
        return r;
    }
    const int base_col = static_cast<int>((exits.size() - 1) / 2);
    QVector<double> xs, ys;
    for (int i = 0; i < entries.size(); ++i) {
        xs << num(entries.at(i));
        ys << matrix_at(irr, i, base_col);
    }
    const double m_cheap = crossing(xs, ys, irr_high_pct); // a higher IRR needs a lower entry multiple
    const double m_rich = crossing(xs, ys, irr_low_pct);
    if (!std::isfinite(m_cheap) || !std::isfinite(m_rich)) {
        r.note = QStringLiteral("a %1-%2% IRR is outside the LBO grid").arg(irr_low_pct, 0, 'f', 0).arg(irr_high_pct, 0, 'f', 0);
        return r;
    }
    r.low = std::min(m_cheap, m_rich) * ebitda;
    r.high = std::max(m_cheap, m_rich) * ebitda;
    r.mid = kNaN;
    r.note = QStringLiteral("entry value for a %1-%2% IRR at a %3x exit")
                 .arg(irr_low_pct, 0, 'f', 0)
                 .arg(irr_high_pct, 0, 'f', 0)
                 .arg(num(exits.at(base_col)), 0, 'f', 1);
    return r;
}

/// `r` in `basis`; false (with r.note set) when the bridge it needs is missing.
inline bool to_basis(Range* r, Basis basis, const Bridge& b) {
    const bool want_ev = basis == Basis::EnterpriseValue;
    if (r->basis_ev == want_ev)
        return true;
    if (!b.valid()) {
        r->note = QStringLiteral("needs shares, debt and cash to convert");
        return false;
    }
    auto conv = [&](double v) { return want_ev ? per_share_to_ev(v, b) : ev_to_per_share(v, b); };
    r->low = conv(r->low);
    r->high = conv(r->high);
    r->mid = std::isfinite(r->mid) ? conv(r->mid) : kNaN;
    if (r->low > r->high)
        std::swap(r->low, r->high);
    r->basis_ev = want_ev;
    return true;
}

/// Shared axis for the rows and markers: their span plus `pad` of it on each side.
inline bool axis(const QVector<Range>& rows, const QVector<double>& markers, double pad, double* lo, double* hi) {
    double mn = std::numeric_limits<double>::max(), mx = std::numeric_limits<double>::lowest();
    for (const Range& r : rows)
        if (r.valid()) {
            mn = std::min(mn, r.low);
            mx = std::max(mx, r.high);
        }
    for (double m : markers)
        if (std::isfinite(m)) {
            mn = std::min(mn, m);
            mx = std::max(mx, m);
        }
    if (mn > mx)
        return false;
    const double span = mx > mn ? mx - mn : std::max(std::fabs(mx), 1.0) * 0.1;
    *lo = mn - span * pad;
    *hi = mx + span * pad;
    return true;
}

} // namespace fincept::screens::ma_ff
