// src/algo_engine/PerformanceMetrics.cpp
#include "algo_engine/PerformanceMetrics.h"

#include <QDateTime>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::algo::perf {

namespace {

constexpr double kPerfMsPerDay = 86400000.0;
constexpr double kPerfDaysPerYear = 365.25;

double perf_round(double v, int decimals) {
    if (!std::isfinite(v))
        return v;
    const double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

} // namespace

int day_key(const QDate& d) {
    return d.isValid() ? d.year() * 10000 + d.month() * 100 + d.day() : 0;
}

QDate day_from_key(int key) {
    if (key <= 0)
        return {};
    return QDate(key / 10000, (key / 100) % 100, key % 100);
}

QDate point_date(const Point& p) {
    if (p.day > 0)
        return day_from_key(p.day);
    return QDateTime::fromMSecsSinceEpoch(p.t_ms, QTimeZone::UTC).date();
}

QVector<double> period_returns(const QVector<Point>& curve) {
    QVector<double> out;
    if (curve.size() < 2)
        return out;
    out.reserve(curve.size() - 1);
    for (int i = 1; i < curve.size(); ++i) {
        const double prev = curve[i - 1].v;
        if (prev > 0 && std::isfinite(prev) && std::isfinite(curve[i].v))
            out.append(curve[i].v / prev - 1.0);
    }
    return out;
}

QVector<double> underwater_pct(const QVector<Point>& curve) {
    QVector<double> out;
    out.reserve(curve.size());
    double peak = -std::numeric_limits<double>::infinity();
    for (const auto& p : curve) {
        if (std::isfinite(p.v))
            peak = std::max(peak, p.v);
        const double dd = (peak > 0 && std::isfinite(p.v)) ? (p.v - peak) / peak * 100.0 : 0.0;
        out.append(std::min(0.0, dd));
    }
    return out;
}

Drawdown drawdown(const QVector<Point>& curve) {
    Drawdown d;
    if (curve.isEmpty())
        return d;
    int peak_idx = 0;
    double peak = curve[0].v;
    int run_start = -1; // index of the peak a current under-water stretch started from
    for (int i = 0; i < curve.size(); ++i) {
        const double v = curve[i].v;
        if (!std::isfinite(v))
            continue;
        if (v >= peak) {
            if (run_start >= 0) {
                const int len = i - run_start;
                if (len > d.longest_points) {
                    d.longest_points = len;
                    d.longest_start_ms = curve[run_start].t_ms;
                    d.longest_end_ms = curve[i].t_ms;
                    d.longest_days = (curve[i].t_ms - curve[run_start].t_ms) / kPerfMsPerDay;
                }
                if (d.peak == run_start && d.recovery < 0 && d.trough >= 0)
                    d.recovery = i;
                run_start = -1;
            }
            peak = v;
            peak_idx = i;
            continue;
        }
        if (run_start < 0)
            run_start = peak_idx;
        // The currency fall is tracked on its own: for P&L curves that start at 0
        // (live deployments) a percentage of the running peak is meaningless.
        const double abs_dd = peak - v;
        d.max_abs = std::max(d.max_abs, abs_dd);
        const double pct = peak > 0 ? abs_dd / peak * 100.0 : 0.0;
        if (pct > d.max_pct || (d.trough < 0 && abs_dd > 0)) {
            d.max_pct = pct;
            d.peak = peak_idx;
            d.trough = i;
            d.recovery = -1;
        }
    }
    if (run_start >= 0) {
        const int last = curve.size() - 1;
        const int len = last - run_start;
        if (len > d.longest_points) {
            d.longest_points = len;
            d.longest_start_ms = curve[run_start].t_ms;
            d.longest_end_ms = curve[last].t_ms;
            d.longest_days = (curve[last].t_ms - curve[run_start].t_ms) / kPerfMsPerDay;
        }
    }
    return d;
}

Ratios ratios(const QVector<double>& returns, double periods_per_year, double risk_free_annual_pct) {
    Ratios r;
    r.n = returns.size();
    if (r.n < 2)
        return r;
    if (!(periods_per_year > 0))
        periods_per_year = 252.0;
    const double rf_annual = std::isfinite(risk_free_annual_pct) ? risk_free_annual_pct / 100.0 : 0.0;
    const double rf_p = std::pow(1.0 + rf_annual, 1.0 / periods_per_year) - 1.0;

    double sum = 0;
    for (double x : returns)
        sum += x;
    r.mean = sum / r.n;
    double ss = 0, down = 0;
    for (double x : returns) {
        ss += (x - r.mean) * (x - r.mean);
        const double ex = x - rf_p;
        if (ex < 0)
            down += ex * ex;
    }
    r.stdev = std::sqrt(ss / (r.n - 1));
    const double dd = std::sqrt(down / r.n);
    const double ann = std::sqrt(periods_per_year);
    r.ann_vol_pct = r.stdev * ann * 100.0;
    const double excess = r.mean - rf_p;
    if (r.stdev > 1e-15)
        r.sharpe = excess / r.stdev * ann;
    if (dd > 1e-15)
        r.sortino = excess / dd * ann;
    return r;
}

RatioBasis ratio_basis(const QVector<Point>& curve, bool intraday, double periods_per_year,
                       double trading_days_per_year, int min_days) {
    RatioBasis b;
    if (intraday) {
        const QVector<Point> days = daily_close(curve);
        if (days.size() >= std::max(2, min_days)) {
            b.returns = period_returns(days);
            b.periods_per_year = trading_days_per_year > 0 ? trading_days_per_year : 252.0;
            b.daily = true;
            return b;
        }
    }
    b.returns = period_returns(curve);
    b.periods_per_year = periods_per_year > 0 ? periods_per_year : estimate_periods_per_year(curve);
    return b;
}

double years_between(qint64 t0_ms, qint64 t1_ms) {
    return (t1_ms - t0_ms) / kPerfMsPerDay / kPerfDaysPerYear;
}

double cagr_pct(double start_value, double end_value, qint64 t0_ms, qint64 t1_ms) {
    const double years = years_between(t0_ms, t1_ms);
    if (!(start_value > 0) || !(years > 0) || !std::isfinite(end_value))
        return std::numeric_limits<double>::quiet_NaN();
    if (end_value <= 0)
        return -100.0;
    return (std::pow(end_value / start_value, 1.0 / years) - 1.0) * 100.0;
}

double estimate_periods_per_year(const QVector<Point>& curve) {
    if (curve.size() < 2)
        return 0.0;
    const double years = years_between(curve.first().t_ms, curve.last().t_ms);
    if (!(years > 0))
        return 0.0;
    return (curve.size() - 1) / years;
}

QVector<Point> daily_close(const QVector<Point>& curve) {
    QVector<Point> out;
    int last_key = 0;
    for (const auto& p : curve) {
        const int k = p.day > 0 ? p.day : day_key(point_date(p));
        if (!out.isEmpty() && k == last_key)
            out.last() = p;
        else
            out.append(p);
        out.last().day = k;
        last_key = k;
    }
    return out;
}

QVector<QPair<int, double>> monthly_returns(const QVector<Point>& curve, double base_value) {
    QVector<QPair<int, double>> out;
    if (curve.isEmpty())
        return out;
    double prev_end = base_value > 0 ? base_value : curve.first().v;
    int cur_month = 0;
    double month_end = prev_end;
    auto flush = [&]() {
        if (cur_month == 0)
            return;
        const double ret = prev_end > 0 ? (month_end / prev_end - 1.0) * 100.0 : 0.0;
        out.append({cur_month, perf_round(ret, 4)});
        prev_end = month_end;
    };
    for (const auto& p : curve) {
        const QDate d = point_date(p);
        const int m = d.year() * 100 + d.month();
        if (m != cur_month) {
            flush();
            cur_month = m;
        }
        if (std::isfinite(p.v))
            month_end = p.v;
    }
    flush();
    return out;
}

TradeStats trade_stats(const QVector<ClosedTrade>& trades) {
    TradeStats s;
    s.trades = trades.size();
    if (s.trades == 0)
        return s;
    double sum_ret = 0, sum_bars = 0, sum_mae = 0, sum_mfe = 0;
    int run_w = 0, run_l = 0;
    for (const auto& t : trades) {
        s.net_pnl += t.net_pnl;
        s.total_fees += t.fees;
        sum_ret += t.ret_pct;
        sum_bars += t.bars_held;
        sum_mae += t.mae_pct;
        sum_mfe += t.mfe_pct;
        if (t.net_pnl > 0) {
            ++s.wins;
            s.gross_profit += t.net_pnl;
            s.largest_win = std::max(s.largest_win, t.net_pnl);
            ++run_w;
            run_l = 0;
        } else if (t.net_pnl < 0) {
            ++s.losses;
            s.gross_loss += -t.net_pnl;
            s.largest_loss = std::max(s.largest_loss, -t.net_pnl);
            ++run_l;
            run_w = 0;
        } else {
            run_w = run_l = 0;
        }
        s.max_consecutive_wins = std::max(s.max_consecutive_wins, run_w);
        s.max_consecutive_losses = std::max(s.max_consecutive_losses, run_l);
    }
    s.win_rate_pct = 100.0 * s.wins / s.trades;
    s.avg_win = s.wins > 0 ? s.gross_profit / s.wins : 0.0;
    s.avg_loss = s.losses > 0 ? s.gross_loss / s.losses : 0.0;
    s.payoff_ratio = s.avg_loss > 0 ? s.avg_win / s.avg_loss : 0.0;
    if (s.gross_loss > 0)
        s.profit_factor = s.gross_profit / s.gross_loss;
    else
        s.profit_factor_infinite = s.gross_profit > 0;
    s.expectancy = s.net_pnl / s.trades;
    s.expectancy_pct = sum_ret / s.trades;
    s.avg_bars_held = sum_bars / s.trades;
    s.avg_mae_pct = sum_mae / s.trades;
    s.avg_mfe_pct = sum_mfe / s.trades;
    return s;
}

QJsonObject trade_stats_json(const TradeStats& s) {
    QJsonObject o;
    o["total_trades"] = s.trades;
    o["winning_trades"] = s.wins;
    o["losing_trades"] = s.losses;
    o["win_rate"] = perf_round(s.win_rate_pct, 2);
    o["gross_profit"] = perf_round(s.gross_profit, 2);
    o["gross_loss"] = perf_round(s.gross_loss, 2);
    o["net_trade_pnl"] = perf_round(s.net_pnl, 2);
    o["avg_win"] = perf_round(s.avg_win, 2);
    o["avg_loss"] = perf_round(s.avg_loss, 2);
    o["largest_win"] = perf_round(s.largest_win, 2);
    o["largest_loss"] = perf_round(s.largest_loss, 2);
    o["payoff_ratio"] = perf_round(s.payoff_ratio, 3);
    o["profit_factor"] = s.profit_factor_infinite ? QJsonValue(QJsonValue::Null) : QJsonValue(perf_round(s.profit_factor, 3));
    o["profit_factor_infinite"] = s.profit_factor_infinite;
    o["expectancy"] = perf_round(s.expectancy, 2);
    o["expectancy_pct"] = perf_round(s.expectancy_pct, 3);
    o["avg_pnl"] = perf_round(s.expectancy, 2);
    o["avg_bars_held"] = perf_round(s.avg_bars_held, 2);
    o["avg_mae_pct"] = perf_round(s.avg_mae_pct, 3);
    o["avg_mfe_pct"] = perf_round(s.avg_mfe_pct, 3);
    o["max_consecutive_wins"] = s.max_consecutive_wins;
    o["max_consecutive_losses"] = s.max_consecutive_losses;
    return o;
}

} // namespace fincept::algo::perf
