// src/services/portfolio/PortfolioPerformance.cpp — see the header for the rules.
#include "services/portfolio/PortfolioPerformance.h"

#include <QMap>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::portfolio {

namespace portfolio_perf_detail {

bool perf_finite(double v) {
    return std::isfinite(v);
}

QDate perf_date(const QString& s) {
    return QDate::fromString(s.left(10), Qt::ISODate);
}

/// Level on or before `d` from ascending parallel vectors.
std::optional<double> perf_level_as_of(const QStringList& dates, const QVector<double>& levels, const QDate& d) {
    if (dates.isEmpty() || dates.size() != levels.size() || !d.isValid())
        return std::nullopt;
    const QString key = d.toString(Qt::ISODate);
    const auto it = std::upper_bound(dates.begin(), dates.end(), key);
    if (it == dates.begin())
        return std::nullopt;
    const qsizetype idx = std::distance(dates.begin(), it) - 1;
    const double v = levels[idx];
    if (!perf_finite(v) || v <= 0)
        return std::nullopt;
    // A level more than 7 days stale is no longer "as of" this date.
    const QDate ld = perf_date(dates[idx]);
    if (ld.isValid() && ld.daysTo(d) > 7)
        return std::nullopt;
    return v;
}

double perf_mean(const QVector<double>& v) {
    return v.isEmpty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

double perf_sd(const QVector<double>& v) {
    if (v.size() < 2)
        return 0.0;
    const double m = perf_mean(v);
    double ss = 0;
    for (double x : v)
        ss += (x - m) * (x - m);
    return std::sqrt(ss / (v.size() - 1));
}

struct Obs {
    QDate start; // previous observation (or the base date)
    QDate end;
    double ret = 0;
};

} // namespace portfolio_perf_detail

namespace ppd = portfolio_perf_detail;

QString perf_window_key(PerfWindow w) {
    switch (w) {
        case PerfWindow::MTD:
            return QStringLiteral("MTD");
        case PerfWindow::QTD:
            return QStringLiteral("QTD");
        case PerfWindow::YTD:
            return QStringLiteral("YTD");
        case PerfWindow::Y1:
            return QStringLiteral("1Y");
        case PerfWindow::Y3:
            return QStringLiteral("3Y");
        case PerfWindow::Y5:
            return QStringLiteral("5Y");
        case PerfWindow::All:
            break;
    }
    return QStringLiteral("ALL");
}

PerfWindow perf_window_from_key(const QString& key) {
    const QString k = key.trimmed().toUpper();
    if (k == QLatin1String("MTD"))
        return PerfWindow::MTD;
    if (k == QLatin1String("QTD"))
        return PerfWindow::QTD;
    if (k == QLatin1String("YTD"))
        return PerfWindow::YTD;
    if (k == QLatin1String("1Y"))
        return PerfWindow::Y1;
    if (k == QLatin1String("3Y"))
        return PerfWindow::Y3;
    if (k == QLatin1String("5Y"))
        return PerfWindow::Y5;
    return PerfWindow::All;
}

QDate perf_window_base_date(PerfWindow w, const QDate& as_of) {
    if (!as_of.isValid())
        return {};
    switch (w) {
        case PerfWindow::MTD:
            return QDate(as_of.year(), as_of.month(), 1).addDays(-1);
        case PerfWindow::QTD: {
            const int q_first_month = ((as_of.month() - 1) / 3) * 3 + 1;
            return QDate(as_of.year(), q_first_month, 1).addDays(-1);
        }
        case PerfWindow::YTD:
            return QDate(as_of.year() - 1, 12, 31);
        case PerfWindow::Y1:
            return as_of.addYears(-1);
        case PerfWindow::Y3:
            return as_of.addYears(-3);
        case PerfWindow::Y5:
            return as_of.addYears(-5);
        case PerfWindow::All:
            break;
    }
    return {};
}

QVector<TwrPoint> build_twr(const QVector<PortfolioSnapshot>& snapshots, const QVector<ExternalFlow>& flows) {
    // One snapshot per date, ascending (last write for a date wins).
    QMap<QDate, double> nav;
    for (const auto& s : snapshots) {
        const QDate d = ppd::perf_date(s.snapshot_date);
        if (d.isValid() && ppd::perf_finite(s.total_value))
            nav.insert(d, s.total_value);
    }
    // Flows by date: inflows and outflows kept apart (different timing).
    struct DayFlows {
        double in = 0;
        double out = 0; // negative
        bool unknown = false;
    };
    QMap<QDate, DayFlows> by_day;
    for (const auto& f : flows) {
        if (!f.date.isValid())
            continue;
        DayFlows& df = by_day[f.date];
        if (!ppd::perf_finite(f.amount_base)) {
            df.unknown = true;
            continue;
        }
        if (f.amount_base >= 0)
            df.in += f.amount_base;
        else
            df.out += f.amount_base;
    }

    QVector<TwrPoint> out;
    out.reserve(nav.size());
    double cum = 0.0;
    QDate prev;
    double prev_nav = 0;
    for (auto it = nav.constBegin(); it != nav.constEnd(); ++it) {
        TwrPoint p;
        p.date = it.key().toString(Qt::ISODate);
        p.nav = it.value();
        if (!prev.isValid()) {
            p.valid = false; // the base point carries no return
            out.append(p);
            prev = it.key();
            prev_nav = it.value();
            continue;
        }
        const QDate d0 = prev, d1 = it.key();
        const double L = static_cast<double>(std::max<qint64>(1, d0.daysTo(d1)));
        double net = 0, weighted = 0;
        bool unknown = false;
        for (auto fit = by_day.upperBound(d0); fit != by_day.end() && fit.key() <= d1; ++fit) {
            const double days_left = static_cast<double>(fit.key().daysTo(d1));
            const DayFlows& df = fit.value();
            unknown = unknown || df.unknown;
            net += df.in + df.out;
            weighted += df.in * std::min(1.0, (days_left + 1.0) / L);
            weighted += df.out * (days_left / L);
        }
        p.net_flow = net;
        const double denom = prev_nav + weighted;
        if (unknown) {
            p.valid = false; // a flow of unknown size: the period's return cannot be measured
        } else if (denom > 1e-9 && ppd::perf_finite(p.nav)) {
            const double r = (p.nav - prev_nav - net) / denom;
            if (ppd::perf_finite(r) && r > -1.0 + 1e-12) {
                p.period_return = r;
                p.suspect = std::abs(r) > kPerfSuspectReturn;
                cum = (1.0 + cum) * (1.0 + r) - 1.0;
            } else {
                p.valid = false;
            }
        } else {
            p.valid = false; // no capital at risk
        }
        p.cumulative = cum;
        out.append(p);
        prev = d1;
        prev_nav = p.nav;
    }
    return out;
}

std::optional<double> xirr(const QVector<QPair<QDate, double>>& flows_in) {
    QVector<QPair<QDate, double>> flows;
    for (const auto& f : flows_in)
        if (f.first.isValid() && ppd::perf_finite(f.second) && std::abs(f.second) > 0)
            flows.append(f);
    if (flows.size() < 2)
        return std::nullopt;
    bool pos = false, neg = false;
    for (const auto& f : flows) {
        pos = pos || f.second > 0;
        neg = neg || f.second < 0;
    }
    if (!pos || !neg)
        return std::nullopt;
    QDate t0 = flows.first().first;
    for (const auto& f : flows)
        t0 = std::min(t0, f.first);
    auto npv = [&](double r) {
        double s = 0;
        for (const auto& f : flows)
            s += f.second / std::pow(1.0 + r, t0.daysTo(f.first) / 365.25);
        return s;
    };
    auto dnpv = [&](double r) {
        double s = 0;
        for (const auto& f : flows) {
            const double t = t0.daysTo(f.first) / 365.25;
            s += -t * f.second / std::pow(1.0 + r, t + 1.0);
        }
        return s;
    };
    // Newton from 10 %, then bisection as the safety net.
    double r = 0.1;
    for (int i = 0; i < 60; ++i) {
        const double f = npv(r), d = dnpv(r);
        if (!ppd::perf_finite(f) || !ppd::perf_finite(d) || std::abs(d) < 1e-14)
            break;
        const double nr = r - f / d;
        if (!ppd::perf_finite(nr) || nr <= -0.9999)
            break;
        if (std::abs(nr - r) < 1e-10) {
            if (std::abs(npv(nr)) < 1e-6 * std::max(1.0, std::abs(flows.first().second)))
                return nr;
            break;
        }
        r = nr;
    }
    double lo = -0.9999, hi = 10.0;
    double flo = npv(lo), fhi = npv(hi);
    while (ppd::perf_finite(fhi) && flo * fhi > 0 && hi < 1e4) {
        hi *= 4.0;
        fhi = npv(hi);
    }
    if (!ppd::perf_finite(flo) || !ppd::perf_finite(fhi) || flo * fhi > 0)
        return std::nullopt;
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = npv(mid);
        if (!ppd::perf_finite(fm))
            return std::nullopt;
        if ((fm > 0) == (flo > 0)) {
            lo = mid;
            flo = fm;
        } else {
            hi = mid;
        }
        if (hi - lo < 1e-12)
            break;
    }
    return 0.5 * (lo + hi);
}

void compute_realised_metrics(const PerfInputs& in, ComputedMetrics& m) {
    m.window = perf_window_key(in.window);
    m.periods_per_year = in.periods_per_year > 0 ? in.periods_per_year : 252.0;
    m.calendar = in.calendar_id;
    const double ppy = m.periods_per_year;
    auto is_obs_day = in.is_trading_day ? in.is_trading_day : [](const QDate& d) { return d.dayOfWeek() <= 5; };

    const QVector<TwrPoint> all = build_twr(in.snapshots, in.flows);
    if (all.isEmpty())
        return;
    const QDate last_date = ppd::perf_date(all.last().date);
    const QDate as_of = in.as_of.isValid() ? std::min(in.as_of, last_date) : last_date;
    const QDate base_date = perf_window_base_date(in.window, as_of);

    // Base point: last snapshot on/before base_date; otherwise the first one.
    int base_idx = 0;
    bool truncated = false;
    if (base_date.isValid()) {
        int found = -1;
        for (int i = 0; i < all.size(); ++i) {
            if (ppd::perf_date(all[i].date) <= base_date)
                found = i;
            else
                break;
        }
        if (found >= 0) {
            base_idx = found;
        } else {
            truncated = true;
        }
    }
    int end_idx = all.size() - 1;
    while (end_idx > base_idx && ppd::perf_date(all[end_idx].date) > as_of)
        --end_idx;
    if (truncated)
        m.notes << QStringLiteral("History starts %1 — shorter than the %2 window").arg(all[base_idx].date, m.window);

    // Window series, cumulative rebased to the base point.
    QVector<TwrPoint> win;
    double cum = 0;
    for (int i = base_idx; i <= end_idx; ++i) {
        TwrPoint p = all[i];
        if (i == base_idx) {
            p.valid = false;
            p.period_return = 0;
            p.net_flow = 0;
        } else if (p.valid) {
            cum = (1.0 + cum) * (1.0 + p.period_return) - 1.0;
        }
        p.cumulative = cum;
        if (i > base_idx) {
            if (!p.valid)
                ++m.perf_excluded;
            if (p.valid && p.suspect)
                ++m.perf_suspect;
        }
        win.append(p);
    }
    m.twr_series = win;
    if (win.isEmpty())
        return;
    m.perf_start = win.first().date;
    m.perf_end = win.last().date;
    if (m.perf_suspect > 0)
        m.notes << QStringLiteral("%1 period(s) moved more than 25 % — kept; check for unrecorded deposits, "
                                  "withdrawals or bad prices")
                       .arg(m.perf_suspect);
    if (m.perf_excluded > 0)
        m.notes << QStringLiteral("%1 period(s) had no capital at risk or a flow of unknown FX — return undefined")
                       .arg(m.perf_excluded);

    // ── Compound the periods onto the calendar's trading days ────────────────
    QVector<ppd::Obs> obs;
    {
        QDate obs_start = ppd::perf_date(win.first().date);
        QDate pending_end;
        double pending = 0;
        bool have_pending = false;
        auto next_obs_day = [&is_obs_day](QDate d) {
            for (int k = 0; k < 10 && !is_obs_day(d); ++k)
                d = d.addDays(1);
            return d;
        };
        for (int i = 1; i < win.size(); ++i) {
            const TwrPoint& p = win[i];
            if (!p.valid)
                continue;
            const QDate od = next_obs_day(ppd::perf_date(p.date));
            if (have_pending && od != pending_end) {
                obs.append({obs_start, pending_end, pending});
                obs_start = pending_end;
                pending = 0;
                have_pending = false;
            }
            pending = (1.0 + pending) * (1.0 + p.period_return) - 1.0;
            pending_end = od;
            have_pending = true;
        }
        if (have_pending)
            obs.append({obs_start, pending_end, pending});
    }
    const int n = static_cast<int>(obs.size());
    m.perf_observations = n;

    QVector<double> rets;
    rets.reserve(n);
    for (const auto& o : obs)
        rets.append(o.ret);

    const QDate d0 = ppd::perf_date(win.first().date);
    const QDate d1 = ppd::perf_date(win.last().date);
    const qint64 span_days = (d0.isValid() && d1.isValid()) ? d0.daysTo(d1) : 0;
    const double years = span_days / 365.25;

    if (win.size() >= 2) {
        m.total_return = win.last().cumulative * 100.0;
        if (span_days >= kPerfMinAnnualiseDays && years > 0)
            m.annualized_return = (std::pow(1.0 + win.last().cumulative, 1.0 / years) - 1.0) * 100.0;
    }

    if (n >= kPerfMinDrawdownObservations) {
        double idx = 1.0, peak = 1.0, mdd = 0.0;
        int wins = 0;
        double best = rets.first(), worst = rets.first();
        for (double r : rets) {
            idx *= (1.0 + r);
            peak = std::max(peak, idx);
            mdd = std::min(mdd, idx / peak - 1.0);
            if (r > 0.0)
                ++wins;
            best = std::max(best, r);
            worst = std::min(worst, r);
        }
        m.max_drawdown = mdd * 100.0;
        m.current_drawdown = (idx / peak - 1.0) * 100.0;
        m.win_rate = 100.0 * wins / n;
        m.best_period = best * 100.0;
        m.worst_period = worst * 100.0;
    }

    const double rf_p = std::pow(1.0 + in.risk_free, 1.0 / ppy) - 1.0;
    if (n >= kPerfMinObservations) {
        const double mean = ppd::perf_mean(rets);
        const double sd = ppd::perf_sd(rets);
        m.realised_volatility = sd * std::sqrt(ppy) * 100.0;
        if (sd > 1e-12)
            m.sharpe = (mean - rf_p) / sd * std::sqrt(ppy);
        double dss = 0;
        for (double r : rets) {
            const double shortfall = std::min(r - rf_p, 0.0);
            dss += shortfall * shortfall;
        }
        const double dd = std::sqrt(dss / n);
        if (dd > 1e-12)
            m.sortino = (mean - rf_p) / dd * std::sqrt(ppy);
        if (m.annualized_return && m.max_drawdown && *m.max_drawdown < -0.1)
            m.calmar = *m.annualized_return / std::abs(*m.max_drawdown);
    }

    // ── Benchmark-relative, same observations ─────────────────────────────────
    if (n >= 2 && in.benchmark_dates.size() >= 2 && in.benchmark_dates.size() == in.benchmark_levels.size()) {
        QVector<double> pr, br;
        for (const auto& o : obs) {
            const auto b0 = ppd::perf_level_as_of(in.benchmark_dates, in.benchmark_levels, o.start);
            const auto b1 = ppd::perf_level_as_of(in.benchmark_dates, in.benchmark_levels, o.end);
            if (!b0 || !b1)
                continue;
            pr.append(o.ret);
            br.append(*b1 / *b0 - 1.0);
        }
        const int mc = static_cast<int>(pr.size());
        if (mc >= 2) {
            double grow_p = 1.0, grow_b = 1.0;
            for (int i = 0; i < mc; ++i) {
                grow_p *= 1.0 + pr[i];
                grow_b *= 1.0 + br[i];
            }
            m.benchmark_return = (grow_b - 1.0) * 100.0;
            m.excess_return = (grow_p - grow_b) * 100.0;
        }
        if (mc >= kPerfMinObservations) {
            const double pm = ppd::perf_mean(pr), bm = ppd::perf_mean(br);
            double cov = 0, var_b = 0, var_p = 0;
            for (int i = 0; i < mc; ++i) {
                cov += (pr[i] - pm) * (br[i] - bm);
                var_b += (br[i] - bm) * (br[i] - bm);
                var_p += (pr[i] - pm) * (pr[i] - pm);
            }
            if (var_b > 1e-14) {
                const double beta = cov / var_b;
                m.realised_beta = beta;
                m.alpha = ((pm - rf_p) - beta * (bm - rf_p)) * ppy * 100.0;
                if (var_p > 1e-14)
                    m.r_squared = (cov * cov) / (var_b * var_p);
                if (std::abs(beta) > 0.1)
                    m.treynor = (pm - rf_p) * ppy / beta * 100.0;
            }
            double am = 0.0;
            for (int i = 0; i < mc; ++i)
                am += pr[i] - br[i];
            am /= mc;
            double ass = 0.0;
            for (int i = 0; i < mc; ++i) {
                const double d = (pr[i] - br[i]) - am;
                ass += d * d;
            }
            const double te = std::sqrt(ass / (mc - 1)) * std::sqrt(ppy);
            if (te > 1e-9) {
                m.tracking_error = te * 100.0;
                m.information_ratio = am * ppy / te;
            }
            // Capture ratios on geometric means of the up / down benchmark periods.
            auto capture = [&](bool up) -> std::optional<double> {
                double gp = 1.0, gb = 1.0;
                int k = 0;
                for (int i = 0; i < mc; ++i) {
                    if ((up && br[i] > 0) || (!up && br[i] < 0)) {
                        gp *= 1.0 + pr[i];
                        gb *= 1.0 + br[i];
                        ++k;
                    }
                }
                if (k < 3 || gp <= 0 || gb <= 0)
                    return std::nullopt;
                const double mp = std::pow(gp, 1.0 / k) - 1.0;
                const double mb = std::pow(gb, 1.0 / k) - 1.0;
                if (std::abs(mb) < 1e-12)
                    return std::nullopt;
                return mp / mb * 100.0;
            };
            m.up_capture = capture(true);
            m.down_capture = capture(false);
        }
    }

    // ── Money-weighted return over the window ─────────────────────────────────
    {
        QVector<QPair<QDate, double>> cf;
        const double v_start = win.first().nav;
        if (v_start > 0)
            cf.append({d0, -v_start});
        bool unknown = false;
        for (const auto& f : in.flows) {
            if (!f.date.isValid() || f.date <= d0 || f.date > d1)
                continue;
            if (!ppd::perf_finite(f.amount_base)) {
                unknown = true;
                break;
            }
            cf.append({f.date, -f.amount_base});
        }
        if (!unknown && ppd::perf_finite(win.last().nav))
            cf.append({d1, win.last().nav});
        if (!unknown && span_days > 0) {
            if (auto irr = xirr(cf)) {
                m.mwr_period = (std::pow(1.0 + *irr, span_days / 365.25) - 1.0) * 100.0;
                if (span_days >= kPerfMinAnnualiseDays)
                    m.mwr = *irr * 100.0;
            }
        }
        if (unknown)
            m.notes << QStringLiteral("Money-weighted return not computed: a flow in the window has no FX rate");
    }
}

} // namespace fincept::portfolio
