// src/services/equity/TechnicalIndicators.cpp
//
// Port of the `ta` 0.11.0 indicators the Technicals frame uses, on top of exact re-implementations
// of the pandas 2.3 / numpy kernels they ran on. Two conventions run through the whole file:
//
//  * Every expression keeps ta's operand order (`(100 * (c - lo)) / (hi - lo)`, not a rearranged
//    equivalent) so each value rounds exactly as it did in Python.
//  * A product that feeds an addition is stored in its own variable first. Within one expression
//    Clang may contract a*b + c into a fused multiply-add (it does on arm64), which rounds once
//    instead of twice; separate statements keep the two roundings pandas performed.
#include "services/equity/TechnicalIndicators.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::equity::technicals {

namespace {

using TiVec = QVector<double>;

constexpr double kTiNaN = std::numeric_limits<double>::quiet_NaN();

// ── numpy / pandas kernels ──────────────────────────────────────────────────────────────────────

// numpy's pairwise summation (the inner loop of np.add.reduce on contiguous float64): eight
// accumulators up to 128 elements, recursive halving above. Every ndarray / Series sum and mean
// in the ta code went through it.
double ti_pairwise_sum(const double* a, qsizetype n) {
    if (n < 8) {
        double res = 0.0;
        for (qsizetype i = 0; i < n; ++i)
            res += a[i];
        return res;
    }
    if (n <= 128) {
        double r[8];
        for (int k = 0; k < 8; ++k)
            r[k] = a[k];
        qsizetype i = 8;
        for (; i < n - (n % 8); i += 8) {
            for (int k = 0; k < 8; ++k)
                r[k] += a[i + k];
        }
        double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; ++i)
            res += a[i];
        return res;
    }
    qsizetype n2 = n / 2;
    n2 -= n2 % 8;
    return ti_pairwise_sum(a, n2) + ti_pairwise_sum(a + n2, n - n2);
}

// np.sum / ndarray.sum(): the reduction starts from the additive identity 0.0.
double ti_np_sum(const double* a, qsizetype n) {
    return 0.0 + ti_pairwise_sum(a, n);
}

// Series.mean() (pandas nanmean): NaN cells count as 0 in the sum and are left out of the count.
double ti_series_mean(const double* a, qsizetype n) {
    TiVec filled(n);
    qsizetype count = 0;
    for (qsizetype i = 0; i < n; ++i) {
        if (std::isnan(a[i])) {
            filled[i] = 0.0;
        } else {
            filled[i] = a[i];
            ++count;
        }
    }
    if (count == 0)
        return kTiNaN;
    return ti_np_sum(filled.constData(), n) / static_cast<double>(count);
}

// pandas hands every rolling / EWM kernel its input with ±inf turned into NaN
// (BaseWindow._prep_values), so an infinite value is a missing one inside a window.
TiVec ti_window_input(const TiVec& v) {
    if (std::none_of(v.cbegin(), v.cend(), [](double x) { return std::isinf(x); }))
        return v;
    TiVec out = v;
    for (double& x : out) {
        if (std::isinf(x))
            x = kTiNaN;
    }
    return out;
}

// rolling(window) bounds (pandas FixedWindowIndexer, center=False): [max(0, i + 1 - w), i + 1).
qsizetype ti_window_start(qsizetype i, qsizetype w) {
    return std::max<qsizetype>(0, i + 1 - w);
}

// rolling(w, min_periods).sum() — pandas roll_sum: Kahan running sum with separate add / remove
// compensations, and the exact `value * nobs` when every value in the window is the same.
TiVec ti_roll_sum(const TiVec& input, qsizetype w, qint64 minp) {
    const TiVec v = ti_window_input(input);
    const qsizetype n = v.size();
    TiVec out(n, kTiNaN);
    double sum = 0.0;
    double comp_add = 0.0;
    double comp_rem = 0.0;
    double prev = 0.0;
    qint64 nobs = 0;
    qint64 same = 0;
    auto add = [&](double val) {
        if (val == val) {
            ++nobs;
            const double y = val - comp_add;
            const double t = sum + y;
            comp_add = t - sum - y;
            sum = t;
            same = (val == prev) ? same + 1 : 1;
            prev = val;
        }
    };
    auto remove = [&](double val) {
        if (val == val) {
            --nobs;
            const double y = -val - comp_rem;
            const double t = sum + y;
            comp_rem = t - sum - y;
            sum = t;
        }
    };
    for (qsizetype i = 0; i < n; ++i) {
        const qsizetype s = ti_window_start(i, w);
        if (i == 0 || s >= i) {
            prev = v[s];
            same = 0;
            sum = comp_add = comp_rem = 0.0;
            nobs = 0;
            for (qsizetype j = s; j <= i; ++j)
                add(v[j]);
        } else {
            for (qsizetype j = ti_window_start(i - 1, w); j < s; ++j)
                remove(v[j]);
            add(v[i]);
        }
        if (nobs == 0 && minp == 0)
            out[i] = 0.0;
        else if (nobs >= minp)
            out[i] = same >= nobs ? prev * static_cast<double>(nobs) : sum;
    }
    return out;
}

// rolling(w, min_periods).mean() — pandas roll_mean: roll_sum's Kahan sums, the exact value when
// the window is constant, and a sign clamp when every value is non-negative / non-positive.
TiVec ti_roll_mean(const TiVec& input, qsizetype w, qint64 minp) {
    const TiVec v = ti_window_input(input);
    const qsizetype n = v.size();
    TiVec out(n, kTiNaN);
    double sum = 0.0;
    double comp_add = 0.0;
    double comp_rem = 0.0;
    double prev = 0.0;
    qint64 nobs = 0;
    qint64 neg = 0;
    qint64 same = 0;
    auto add = [&](double val) {
        if (val == val) {
            ++nobs;
            const double y = val - comp_add;
            const double t = sum + y;
            comp_add = t - sum - y;
            sum = t;
            if (std::signbit(val))
                ++neg;
            same = (val == prev) ? same + 1 : 1;
            prev = val;
        }
    };
    auto remove = [&](double val) {
        if (val == val) {
            --nobs;
            const double y = -val - comp_rem;
            const double t = sum + y;
            comp_rem = t - sum - y;
            sum = t;
            if (std::signbit(val))
                --neg;
        }
    };
    for (qsizetype i = 0; i < n; ++i) {
        const qsizetype s = ti_window_start(i, w);
        if (i == 0 || s >= i) {
            sum = comp_add = comp_rem = 0.0;
            nobs = neg = 0;
            prev = v[s];
            same = 0;
            for (qsizetype j = s; j <= i; ++j)
                add(v[j]);
        } else {
            for (qsizetype j = ti_window_start(i - 1, w); j < s; ++j)
                remove(v[j]);
            add(v[i]);
        }
        if (nobs >= minp && nobs > 0) {
            double r = sum / static_cast<double>(nobs);
            if (same >= nobs)
                r = prev;
            else if (neg == 0 && r < 0)
                r = 0.0;
            else if (neg == nobs && r > 0)
                r = 0.0;
            out[i] = r;
        }
    }
    return out;
}

// rolling(w, min_periods).std(ddof) — pandas roll_var (Welford with Kahan-compensated mean, 0 for
// a constant window) followed by zsqrt (a negative variance becomes 0).
TiVec ti_roll_std(const TiVec& input, qsizetype w, qint64 minp_in, int ddof) {
    const TiVec v = ti_window_input(input);
    const qsizetype n = v.size();
    const qint64 minp = std::max<qint64>(minp_in, 1);
    TiVec out(n, kTiNaN);
    double mean = 0.0;
    double ssqdm = 0.0;
    double nobs = 0.0;
    double comp_add = 0.0;
    double comp_rem = 0.0;
    double prev = 0.0;
    qint64 same = 0;
    auto add = [&](double val) {
        if (val != val)
            return;
        nobs += 1.0;
        same = (val == prev) ? same + 1 : 1;
        prev = val;
        const double prev_mean = mean - comp_add;
        const double y = val - comp_add;
        const double t = y - mean;
        comp_add = t + mean - y;
        const double delta = t;
        if (nobs != 0.0)
            mean = mean + delta / nobs;
        else
            mean = 0.0;
        const double prod = (val - prev_mean) * (val - mean);
        ssqdm = ssqdm + prod;
    };
    auto remove = [&](double val) {
        if (val == val) {
            nobs -= 1.0;
            if (nobs != 0.0) {
                const double prev_mean = mean - comp_rem;
                const double y = val - comp_rem;
                const double t = y - mean;
                comp_rem = t + mean - y;
                const double delta = t;
                mean = mean - delta / nobs;
                const double prod = (val - prev_mean) * (val - mean);
                ssqdm = ssqdm - prod;
            } else {
                mean = 0.0;
                ssqdm = 0.0;
            }
        }
    };
    for (qsizetype i = 0; i < n; ++i) {
        const qsizetype s = ti_window_start(i, w);
        if (i == 0 || s >= i) {
            prev = v[s];
            same = 0;
            mean = ssqdm = nobs = comp_add = comp_rem = 0.0;
            for (qsizetype j = s; j <= i; ++j)
                add(v[j]);
        } else {
            for (qsizetype j = ti_window_start(i - 1, w); j < s; ++j)
                remove(v[j]);
            add(v[i]);
        }
        if (nobs >= static_cast<double>(minp) && nobs > static_cast<double>(ddof)) {
            double var = 0.0;
            if (!(nobs == 1.0 || static_cast<double>(same) >= nobs))
                var = ssqdm / (nobs - static_cast<double>(ddof));
            out[i] = var < 0 ? 0.0 : std::sqrt(var);
        }
    }
    return out;
}

// rolling(w, min_periods).max() / .min() — pandas skips NaN, needs `minp` observations, and on a
// tie keeps the newest bar (its deque pops equal values), which only matters for ±0.
TiVec ti_roll_extreme(const TiVec& input, qsizetype w, qint64 minp, bool is_max) {
    const TiVec v = ti_window_input(input);
    const qsizetype n = v.size();
    TiVec out(n, kTiNaN);
    for (qsizetype i = 0; i < n; ++i) {
        qint64 nobs = 0;
        double best = kTiNaN;
        for (qsizetype j = ti_window_start(i, w); j <= i; ++j) {
            const double x = v[j];
            if (x != x)
                continue;
            if (nobs == 0 || (is_max ? x >= best : x <= best))
                best = x;
            ++nobs;
        }
        if (nobs > 0 && nobs >= minp)
            out[i] = best;
    }
    return out;
}

TiVec ti_roll_max(const TiVec& v, qsizetype w, qint64 minp) {
    return ti_roll_extreme(v, w, minp, true);
}

TiVec ti_roll_min(const TiVec& v, qsizetype w, qint64 minp) {
    return ti_roll_extreme(v, w, minp, false);
}

// rolling(w).apply(fn, raw=True) calls fn only where the window holds at least `minp` finite
// values; with the default min_periods (= w) that is a full window with no NaN / ±inf.
bool ti_apply_window_ok(const TiVec& v, qsizetype i, qsizetype w) {
    if (i + 1 < w)
        return false;
    for (qsizetype j = i + 1 - w; j <= i; ++j) {
        if (!std::isfinite(v[j]))
            return false;
    }
    return true;
}

// ewm(com, min_periods, adjust=False, ignore_na=False).mean() — the pandas `ewm` kernel. A NaN
// keeps decaying the old weight, and an unchanged value is not re-averaged (constant series stay
// exact).
TiVec ti_ewm_mean(const TiVec& input, double com, int min_periods) {
    const TiVec v = ti_window_input(input);
    const qsizetype n = v.size();
    TiVec out(n, kTiNaN);
    if (n == 0)
        return out;
    const qint64 minp = std::max(min_periods, 1);
    const double alpha = 1.0 / (1.0 + com);
    const double old_wt_factor = 1.0 - alpha;
    const double new_wt = alpha;
    double weighted = v[0];
    qint64 nobs = (weighted == weighted) ? 1 : 0;
    out[0] = nobs >= minp ? weighted : kTiNaN;
    double old_wt = 1.0;
    for (qsizetype i = 1; i < n; ++i) {
        const double cur = v[i];
        const bool is_observation = cur == cur;
        nobs += is_observation ? 1 : 0;
        if (weighted == weighted) {
            old_wt *= old_wt_factor;
            if (is_observation) {
                if (weighted != cur) {
                    const double old_part = old_wt * weighted;
                    const double new_part = new_wt * cur;
                    weighted = old_part + new_part;
                    weighted /= (old_wt + new_wt);
                }
                old_wt = 1.0;
            }
        } else if (is_observation) {
            weighted = cur;
        }
        out[i] = nobs >= minp ? weighted : kTiNaN;
    }
    return out;
}

// ta's _ema(): ewm(span=window, min_periods=window, adjust=False). pandas turns the span into a
// centre of mass (span - 1) / 2.
TiVec ti_ema(const TiVec& v, int window) {
    return ti_ewm_mean(v, (static_cast<double>(window) - 1.0) / 2.0, window);
}

// ewm(alpha=1/window, ...): pandas converts alpha to a centre of mass (1 - alpha) / alpha, and
// the kernel turns it back with 1 / (1 + com) — the round trip is kept for bit-exact weights.
TiVec ti_wilder_ewm(const TiVec& v, int window) {
    const double alpha = 1.0 / static_cast<double>(window);
    return ti_ewm_mean(v, (1.0 - alpha) / alpha, window);
}

// ── Series helpers ──────────────────────────────────────────────────────────────────────────────

TiVec ti_shift(const TiVec& v, qsizetype k, double fill = kTiNaN) {
    const qsizetype n = v.size();
    TiVec out(n, fill);
    for (qsizetype i = k; i < n; ++i)
        out[i] = v[i - k];
    return out;
}

// np.roll(v, k): out[i] = v[(i - k) mod n] — ta's KAMA wraps the newest closes round to the front.
TiVec ti_np_roll(const TiVec& v, qsizetype k) {
    const qsizetype n = v.size();
    TiVec out(n, kTiNaN);
    if (n == 0)
        return out;
    const qsizetype shift = k % n;
    for (qsizetype i = 0; i < n; ++i)
        out[i] = v[(i - shift + n) % n];
    return out;
}

// Series.cumsum() on float64: NaN cells stay NaN and are skipped by the running total.
TiVec ti_cumsum(const TiVec& v) {
    const qsizetype n = v.size();
    TiVec out(n, kTiNaN);
    double acc = 0.0;
    for (qsizetype i = 0; i < n; ++i) {
        const bool missing = std::isnan(v[i]);
        const double x = missing ? 0.0 : v[i];
        acc = (i == 0) ? x : acc + x;
        if (!missing)
            out[i] = acc;
    }
    return out;
}

// Series.pct_change() in pandas 2.x: forward-fill first (the default fill_method='pad'), then
// data / data.shift(1) - 1.
TiVec ti_pct_change(const TiVec& v) {
    const qsizetype n = v.size();
    TiVec data(n);
    double last = kTiNaN;
    for (qsizetype i = 0; i < n; ++i) {
        if (!std::isnan(v[i]))
            last = v[i];
        data[i] = std::isnan(v[i]) ? last : v[i];
    }
    TiVec out(n, kTiNaN);
    for (qsizetype i = 1; i < n; ++i)
        out[i] = data[i] / data[i - 1] - 1.0;
    return out;
}

// ta's _true_range(): the row max (NaN skipped) of high - low, |high - prev_close| and
// |low - prev_close|; NaN only when all three are NaN.
TiVec ti_true_range(const TiVec& high, const TiVec& low, const TiVec& prev_close) {
    const qsizetype n = high.size();
    TiVec out(n, kTiNaN);
    for (qsizetype i = 0; i < n; ++i) {
        const double parts[3] = {high[i] - low[i], std::fabs(high[i] - prev_close[i]),
                                 std::fabs(low[i] - prev_close[i])};
        bool have = false;
        double best = kTiNaN;
        for (double p : parts) {
            if (std::isnan(p))
                continue;
            if (!have || p > best)
                best = p;
            have = true;
        }
        out[i] = best;
    }
    return out;
}

// ((h + l) + c) / 3.0 — ta's typical price.
TiVec ti_typical_price(const TiVec& h, const TiVec& l, const TiVec& c) {
    const qsizetype n = c.size();
    TiVec out(n);
    for (qsizetype i = 0; i < n; ++i)
        out[i] = (h[i] + l[i] + c[i]) / 3.0;
    return out;
}

// ta's RSIIndicator: up / down moves (the first move, and any move touching a NaN, count as 0),
// Wilder-smoothed; 100 whenever the average loss is exactly 0.
TiVec ti_rsi(const TiVec& c, int window) {
    const qsizetype n = c.size();
    TiVec up(n);
    TiVec down(n);
    for (qsizetype i = 0; i < n; ++i) {
        const double diff = i > 0 ? c[i] - c[i - 1] : kTiNaN;
        up[i] = diff > 0 ? diff : 0.0;
        down[i] = -(diff < 0 ? diff : 0.0);
    }
    const TiVec ema_up = ti_wilder_ewm(up, window);
    const TiVec ema_down = ti_wilder_ewm(down, window);
    TiVec out(n);
    for (qsizetype i = 0; i < n; ++i) {
        const double rs = ema_up[i] / ema_down[i];
        out[i] = ema_down[i] == 0 ? 100.0 : 100.0 - (100.0 / (1.0 + rs));
    }
    return out;
}

// ta's AverageTrueRange. The first window - 1 rows are a placeholder 0; ta raises (and produced
// no output at all) for a series shorter than the window — all NaN here.
TiVec ti_atr(const TiVec& h, const TiVec& l, const TiVec& c, int window) {
    const qsizetype n = c.size();
    if (window < 1 || n < window)
        return TiVec(n, kTiNaN);
    const TiVec tr = ti_true_range(h, l, ti_shift(c, 1));
    TiVec atr(n, 0.0);
    atr[window - 1] = ti_series_mean(tr.constData(), window);
    for (qsizetype i = window; i < n; ++i) {
        const double carried = atr[i - 1] * static_cast<double>(window - 1);
        atr[i] = (carried + tr[i]) / static_cast<double>(window);
    }
    return atr;
}

struct TiAdx {
    TiVec adx;
    TiVec pos;
    TiVec neg;
};

// Sum of the first `count` non-NaN values (ta: series.dropna().iloc[0:count].sum()).
double ti_first_valid_sum(const TiVec& v, qsizetype count) {
    TiVec picked;
    picked.reserve(count);
    for (qsizetype i = 0; i < v.size() && picked.size() < count; ++i) {
        if (!std::isnan(v[i]))
            picked.append(v[i]);
    }
    return ti_np_sum(picked.constData(), picked.size());
}

// ta's ADXIndicator, structure and quirks included: Wilder sums over max(high, prev close) -
// min(low, prev close) and the directional moves, ADX = 0 for the first 2·window - 1 rows and
// ±DI = 0 up to row `window`. ta raises for fewer than 2·window bars (no output) — all NaN here.
TiAdx ti_adx(const TiVec& h, const TiVec& l, const TiVec& c, int window) {
    const qsizetype n = c.size();
    const qsizetype w = window;
    TiAdx r{TiVec(n, kTiNaN), TiVec(n, kTiNaN), TiVec(n, kTiNaN)};
    if (w < 1 || n < 2 * w)
        return r;
    const double wd = static_cast<double>(w);

    TiVec ddm(n);
    TiVec pos(n);
    TiVec neg(n);
    for (qsizetype i = 0; i < n; ++i) {
        const double cs = i > 0 ? c[i - 1] : kTiNaN;
        // np.amax / np.amin: NaN-propagating.
        const double pdm = (std::isnan(h[i]) || std::isnan(cs)) ? kTiNaN : (h[i] >= cs ? h[i] : cs);
        const double pdn = (std::isnan(l[i]) || std::isnan(cs)) ? kTiNaN : (l[i] <= cs ? l[i] : cs);
        ddm[i] = pdm - pdn;
        const double diff_up = i > 0 ? h[i] - h[i - 1] : kTiNaN;
        const double diff_down = i > 0 ? l[i - 1] - l[i] : kTiNaN;
        const double up_flag = (diff_up > diff_down && diff_up > 0) ? 1.0 : 0.0;
        const double down_flag = (diff_down > diff_up && diff_down > 0) ? 1.0 : 0.0;
        pos[i] = std::fabs(up_flag * diff_up);
        neg[i] = std::fabs(down_flag * diff_down);
    }

    const qsizetype len = n - (w - 1);
    TiVec trs(len, 0.0);
    TiVec dip(len, 0.0);
    TiVec din(len, 0.0);
    trs[0] = ti_first_valid_sum(ddm, w);
    dip[0] = ti_first_valid_sum(pos, w);
    din[0] = ti_first_valid_sum(neg, w);
    for (qsizetype i = 1; i < len - 1; ++i) {
        trs[i] = trs[i - 1] - (trs[i - 1] / wd) + ddm[w + i];
        dip[i] = dip[i - 1] - (dip[i - 1] / wd) + pos[w + i];
        din[i] = din[i - 1] - (din[i - 1] / wd) + neg[w + i];
    }

    TiVec dx(len, 0.0);
    for (qsizetype i = 0; i < len; ++i) {
        const double plus = trs[i] != 0 ? 100.0 * (dip[i] / trs[i]) : 0.0;
        const double minus = trs[i] != 0 ? 100.0 * (din[i] / trs[i]) : 0.0;
        dx[i] = (plus + minus != 0) ? 100.0 * std::fabs((plus - minus) / (plus + minus)) : 0.0;
    }
    TiVec adx_tail(len, 0.0);
    adx_tail[w] = ti_np_sum(dx.constData(), w) / wd;
    for (qsizetype i = w + 1; i < len; ++i) {
        const double carried = adx_tail[i - 1] * static_cast<double>(w - 1);
        adx_tail[i] = (carried + dx[i - 1]) / wd;
    }
    for (qsizetype j = 0; j < n; ++j)
        r.adx[j] = j < w - 1 ? 0.0 : adx_tail[j - (w - 1)];

    r.pos.fill(0.0);
    r.neg.fill(0.0);
    for (qsizetype i = 1; i < len - 1; ++i) {
        r.pos[i + w] = trs[i] != 0 ? 100.0 * (dip[i] / trs[i]) : 0.0;
        r.neg[i + w] = trs[i] != 0 ? 100.0 * (din[i] / trs[i]) : 0.0;
    }
    return r;
}

struct TiBands {
    TiVec mavg;
    TiVec hband;
    TiVec lband;
    TiVec pband;
    TiVec wband;
    TiVec hband_indicator;
    TiVec lband_indicator;
};

// ta's BollingerBands: rolling mean ± window_dev population standard deviations (ddof = 0).
TiBands ti_bollinger(const TiVec& c, int window, int window_dev) {
    const qsizetype n = c.size();
    TiBands b;
    b.mavg = ti_roll_mean(c, window, window);
    const TiVec mstd = ti_roll_std(c, window, window, 0);
    b.hband = TiVec(n);
    b.lband = TiVec(n);
    b.pband = TiVec(n);
    b.wband = TiVec(n);
    b.hband_indicator = TiVec(n);
    b.lband_indicator = TiVec(n);
    const double dev = static_cast<double>(window_dev);
    for (qsizetype i = 0; i < n; ++i) {
        const double spread = dev * mstd[i];
        b.hband[i] = b.mavg[i] + spread;
        b.lband[i] = b.mavg[i] - spread;
        b.wband[i] = ((b.hband[i] - b.lband[i]) / b.mavg[i]) * 100.0;
        const double width = b.hband[i] != b.lband[i] ? b.hband[i] - b.lband[i] : kTiNaN;
        b.pband[i] = (c[i] - b.lband[i]) / width;
        b.hband_indicator[i] = c[i] > b.hband[i] ? 1.0 : 0.0;
        b.lband_indicator[i] = c[i] < b.lband[i] ? 1.0 : 0.0;
    }
    return b;
}

struct TiStoch {
    TiVec k;
    TiVec d;
};

// ta's StochasticOscillator: %K over `window` bars, %D its `smooth_window`-bar mean.
TiStoch ti_stochastic(const TiVec& h, const TiVec& l, const TiVec& c, int window, int smooth_window) {
    const qsizetype n = c.size();
    const TiVec smin = ti_roll_min(l, window, window);
    const TiVec smax = ti_roll_max(h, window, window);
    TiStoch s;
    s.k = TiVec(n);
    for (qsizetype i = 0; i < n; ++i)
        s.k[i] = (100.0 * (c[i] - smin[i])) / (smax[i] - smin[i]);
    s.d = ti_roll_mean(s.k, smooth_window, smooth_window);
    return s;
}

// ta's CCIIndicator: (tp - SMA(tp)) / (constant · mean absolute deviation of tp).
TiVec ti_cci(const TiVec& h, const TiVec& l, const TiVec& c, int window, double constant) {
    const qsizetype n = c.size();
    const TiVec tp = ti_typical_price(h, l, c);
    const TiVec tp_mean = ti_roll_mean(tp, window, window);
    TiVec out(n, kTiNaN);
    TiVec dev(window);
    for (qsizetype i = 0; i < n; ++i) {
        if (!ti_apply_window_ok(tp, i, window))
            continue;
        const double* x = tp.constData() + (i + 1 - window);
        const double m = ti_np_sum(x, window) / static_cast<double>(window);
        for (int k = 0; k < window; ++k)
            dev[k] = std::fabs(x[k] - m);
        const double mad = ti_np_sum(dev.constData(), window) / static_cast<double>(window);
        out[i] = (tp[i] - tp_mean[i]) / (constant * mad);
    }
    return out;
}

// ta's WilliamsRIndicator over `lbp` bars.
TiVec ti_williams_r(const TiVec& h, const TiVec& l, const TiVec& c, int lbp) {
    const qsizetype n = c.size();
    const TiVec hh = ti_roll_max(h, lbp, lbp);
    const TiVec ll = ti_roll_min(l, lbp, lbp);
    TiVec out(n);
    for (qsizetype i = 0; i < n; ++i)
        out[i] = (-100.0 * (hh[i] - c[i])) / (hh[i] - ll[i]);
    return out;
}

// ta's OnBalanceVolumeIndicator: ± volume by close direction (the first bar counts +), summed.
TiVec ti_obv(const TiVec& c, const TiVec& v) {
    const qsizetype n = c.size();
    TiVec signed_volume(n);
    for (qsizetype i = 0; i < n; ++i)
        signed_volume[i] = (i > 0 && c[i] < c[i - 1]) ? -v[i] : v[i];
    return ti_cumsum(signed_volume);
}

// ta's VolumeWeightedAveragePrice: a rolling `window`-bar VWAP of the typical price.
TiVec ti_vwap(const TiVec& h, const TiVec& l, const TiVec& c, const TiVec& v, int window) {
    const qsizetype n = c.size();
    const TiVec tp = ti_typical_price(h, l, c);
    TiVec tpv(n);
    for (qsizetype i = 0; i < n; ++i)
        tpv[i] = tp[i] * v[i];
    const TiVec total_pv = ti_roll_sum(tpv, window, window);
    const TiVec total_volume = ti_roll_sum(v, window, window);
    TiVec out(n);
    for (qsizetype i = 0; i < n; ++i)
        out[i] = total_pv[i] / total_volume[i];
    return out;
}

// ((c - l) - (h - c)) / (h - l) with 0/0 → 0 (ta fills NaN only; x/0 stays ±inf).
TiVec ti_close_location(const TiVec& h, const TiVec& l, const TiVec& c) {
    const qsizetype n = c.size();
    TiVec out(n);
    for (qsizetype i = 0; i < n; ++i) {
        const double clv = ((c[i] - l[i]) - (h[i] - c[i])) / (h[i] - l[i]);
        out[i] = std::isnan(clv) ? 0.0 : clv;
    }
    return out;
}

struct TiMacd {
    TiVec line;
    TiVec signal;
    TiVec diff;
};

TiMacd ti_macd(const TiVec& c, int fast, int slow, int sign) {
    const qsizetype n = c.size();
    const TiVec ema_fast = ti_ema(c, fast);
    const TiVec ema_slow = ti_ema(c, slow);
    TiMacd m;
    m.line = TiVec(n);
    for (qsizetype i = 0; i < n; ++i)
        m.line[i] = ema_fast[i] - ema_slow[i];
    m.signal = ti_ema(m.line, sign);
    m.diff = TiVec(n);
    for (qsizetype i = 0; i < n; ++i)
        m.diff[i] = m.line[i] - m.signal[i];
    return m;
}

// ── The frame ───────────────────────────────────────────────────────────────────────────────────

struct TiFrameBuilder {
    QVector<IndicatorColumn> cols;
    void add(const char* name, TiVec values) {
        cols.append(IndicatorColumn{QString::fromLatin1(name), std::move(values)});
    }
};

void ti_add_trend(TiFrameBuilder& f, const PriceSeries& s) {
    const TiVec& h = s.high;
    const TiVec& l = s.low;
    const TiVec& c = s.close;
    const qsizetype n = c.size();

    f.add("sma_20", ti_roll_mean(c, 20, 20));
    f.add("ema_12", ti_ema(c, 12));

    // WMA 9: weights i·2 / (9·10) for i = 1..9, oldest bar lightest; pandas Series.sum() of the
    // weighted window.
    {
        constexpr int kW = 9;
        double weight[kW];
        for (int i = 1; i <= kW; ++i)
            weight[i - 1] = static_cast<double>(i * 2) / static_cast<double>(kW * (kW + 1));
        TiVec wma(n, kTiNaN);
        double prod[kW];
        for (qsizetype i = 0; i < n; ++i) {
            if (!ti_apply_window_ok(c, i, kW))
                continue;
            for (int k = 0; k < kW; ++k)
                prod[k] = weight[k] * c[i + 1 - kW + k];
            wma[i] = ti_np_sum(prod, kW);
        }
        f.add("wma_9", wma);
    }

    TiMacd macd = ti_macd(c, 12, 26, 9);
    f.add("macd", macd.line);
    f.add("macd_signal", macd.signal);
    f.add("macd_diff", macd.diff);

    // TRIX 15: triple EMA, % change against the previous bar (the first bar's "previous" is the
    // mean of the triple EMA; it lands on a NaN row either way).
    {
        const TiVec ema3 = ti_ema(ti_ema(ti_ema(c, 15), 15), 15);
        const TiVec prev = ti_shift(ema3, 1, ti_series_mean(ema3.constData(), n));
        TiVec trix(n);
        for (qsizetype i = 0; i < n; ++i)
            trix[i] = ((ema3[i] - prev[i]) / prev[i]) * 100.0;
        f.add("trix", trix);
    }

    // Mass index (9, 25).
    {
        TiVec amplitude(n);
        for (qsizetype i = 0; i < n; ++i)
            amplitude[i] = h[i] - l[i];
        const TiVec ema1 = ti_ema(amplitude, 9);
        const TiVec ema2 = ti_ema(ema1, 9);
        TiVec mass(n);
        for (qsizetype i = 0; i < n; ++i)
            mass[i] = ema1[i] / ema2[i];
        f.add("mass_index", ti_roll_sum(mass, 25, 25));
    }

    // Ichimoku (9, 26, 52), visual=False: no forward shift; span B accepts partial windows.
    {
        const TiVec hi9 = ti_roll_max(h, 9, 9);
        const TiVec lo9 = ti_roll_min(l, 9, 9);
        const TiVec hi26 = ti_roll_max(h, 26, 26);
        const TiVec lo26 = ti_roll_min(l, 26, 26);
        const TiVec hi52 = ti_roll_max(h, 52, 0);
        const TiVec lo52 = ti_roll_min(l, 52, 0);
        TiVec conv(n);
        TiVec base(n);
        TiVec span_a(n);
        TiVec span_b(n);
        for (qsizetype i = 0; i < n; ++i) {
            conv[i] = 0.5 * (hi9[i] + lo9[i]);
            base[i] = 0.5 * (hi26[i] + lo26[i]);
            span_a[i] = 0.5 * (conv[i] + base[i]);
            span_b[i] = 0.5 * (hi52[i] + lo52[i]);
        }
        f.add("ichimoku_conversion", conv);
        f.add("ichimoku_base", base);
        f.add("ichimoku_a", span_a);
        f.add("ichimoku_b", span_b);
    }

    // KST: ROC 10/15/20/30 smoothed 10/10/10/15, signal = 9-bar mean (partial windows allowed).
    // ta fills the first `roc` bars' previous close with the mean close of the whole series.
    {
        const double mean_close = ti_series_mean(c.constData(), n);
        const int rocs[4] = {10, 15, 20, 30};
        const int windows[4] = {10, 10, 10, 15};
        TiVec rocma[4];
        for (int k = 0; k < 4; ++k) {
            const TiVec prev = ti_shift(c, rocs[k], mean_close);
            TiVec roc(n);
            for (qsizetype i = 0; i < n; ++i)
                roc[i] = (c[i] - prev[i]) / prev[i];
            rocma[k] = ti_roll_mean(roc, windows[k], windows[k]);
        }
        TiVec kst(n);
        for (qsizetype i = 0; i < n; ++i) {
            const double t2 = 2.0 * rocma[1][i];
            const double t3 = 3.0 * rocma[2][i];
            const double t4 = 4.0 * rocma[3][i];
            double acc = rocma[0][i] + t2;
            acc = acc + t3;
            acc = acc + t4;
            kst[i] = 100.0 * acc;
        }
        f.add("kst", kst);
        f.add("kst_signal", ti_roll_mean(kst, 9, 0));
    }

    // DPO 20: close shifted int(0.5·20 + 1) = 11 bars minus the 20-bar SMA.
    {
        const TiVec shifted = ti_shift(c, 11, ti_series_mean(c.constData(), n));
        const TiVec sma = ti_roll_mean(c, 20, 20);
        TiVec dpo(n);
        for (qsizetype i = 0; i < n; ++i)
            dpo[i] = shifted[i] - sma[i];
        f.add("dpo", dpo);
    }

    f.add("cci", ti_cci(h, l, c, 20, 0.015));

    TiAdx adx = ti_adx(h, l, c, 14);
    f.add("adx", adx.adx);
    f.add("adx_pos", adx.pos);
    f.add("adx_neg", adx.neg);

    // Vortex 14 (the first bar's previous close is the mean close, as in ta).
    {
        const TiVec prev_close = ti_shift(c, 1, ti_series_mean(c.constData(), n));
        const TiVec trn = ti_roll_sum(ti_true_range(h, l, prev_close), 14, 14);
        TiVec vmp(n);
        TiVec vmm(n);
        for (qsizetype i = 0; i < n; ++i) {
            vmp[i] = i > 0 ? std::fabs(h[i] - l[i - 1]) : kTiNaN;
            vmm[i] = i > 0 ? std::fabs(l[i] - h[i - 1]) : kTiNaN;
        }
        const TiVec vmp_sum = ti_roll_sum(vmp, 14, 14);
        const TiVec vmm_sum = ti_roll_sum(vmm, 14, 14);
        TiVec vip(n);
        TiVec vin(n);
        for (qsizetype i = 0; i < n; ++i) {
            vip[i] = vmp_sum[i] / trn[i];
            vin[i] = vmm_sum[i] / trn[i];
        }
        f.add("vortex_pos", vip);
        f.add("vortex_neg", vin);
    }

    // Parabolic SAR (step 0.02, max 0.2): ta's state machine, starting in an up-trend at bar 2.
    {
        constexpr double kStep = 0.02;
        constexpr double kMaxStep = 0.20;
        TiVec psar = c;
        TiVec psar_up(n, kTiNaN);
        TiVec psar_down(n, kTiNaN);
        if (n > 0) {
            bool up_trend = true;
            double af = kStep;
            double up_trend_high = h[0];
            double down_trend_low = l[0];
            for (qsizetype i = 2; i < n; ++i) {
                bool reversal = false;
                const double max_high = h[i];
                const double min_low = l[i];
                if (up_trend) {
                    const double move = af * (up_trend_high - psar[i - 1]);
                    psar[i] = psar[i - 1] + move;
                    if (min_low < psar[i]) {
                        reversal = true;
                        psar[i] = up_trend_high;
                        down_trend_low = min_low;
                        af = kStep;
                    } else {
                        if (max_high > up_trend_high) {
                            up_trend_high = max_high;
                            af = std::min(af + kStep, kMaxStep);
                        }
                        const double low1 = l[i - 1];
                        const double low2 = l[i - 2];
                        if (low2 < psar[i])
                            psar[i] = low2;
                        else if (low1 < psar[i])
                            psar[i] = low1;
                    }
                } else {
                    const double move = af * (psar[i - 1] - down_trend_low);
                    psar[i] = psar[i - 1] - move;
                    if (max_high > psar[i]) {
                        reversal = true;
                        psar[i] = down_trend_low;
                        up_trend_high = max_high;
                        af = kStep;
                    } else {
                        if (min_low < down_trend_low) {
                            down_trend_low = min_low;
                            af = std::min(af + kStep, kMaxStep);
                        }
                        const double high1 = h[i - 1];
                        const double high2 = h[i - 2];
                        if (high2 > psar[i])
                            psar[i] = high2;
                        else if (high1 > psar[i])
                            psar[i] = high1;
                    }
                }
                up_trend = up_trend != reversal;
                if (up_trend)
                    psar_up[i] = psar[i];
                else
                    psar_down[i] = psar[i];
            }
        }
        // Flags: 1 on the first bar of a new up / down run. ta reads psar_up's VALUE for both
        // flags (a ta bug); psar_up is NaN on every down bar, so the down flag still comes out 1.
        TiVec up_flag(n);
        TiVec down_flag(n);
        for (qsizetype i = 0; i < n; ++i) {
            const bool prev_up_missing = i == 0 || std::isnan(psar_up[i - 1]);
            const bool prev_down_missing = i == 0 || std::isnan(psar_down[i - 1]);
            const double up_pick = (!std::isnan(psar_up[i]) && prev_up_missing) ? psar_up[i] : 0.0;
            const double down_pick = (!std::isnan(psar_down[i]) && prev_down_missing) ? psar_up[i] : 0.0;
            up_flag[i] = up_pick == 0 ? up_pick : 1.0;
            down_flag[i] = down_pick == 0 ? down_pick : 1.0;
        }
        f.add("psar", psar);
        f.add("psar_up", psar_up);
        f.add("psar_down", psar_down);
        f.add("psar_up_indicator", up_flag);
        f.add("psar_down_indicator", down_flag);
    }

    // Schaff Trend Cycle (slow 50, fast 23, cycle 10, smoothing 3 / 3).
    {
        const TiVec ema_fast = ti_ema(c, 23);
        const TiVec ema_slow = ti_ema(c, 50);
        TiVec macd_line(n);
        for (qsizetype i = 0; i < n; ++i)
            macd_line[i] = ema_fast[i] - ema_slow[i];
        const TiVec macd_min = ti_roll_min(macd_line, 10, 10);
        const TiVec macd_max = ti_roll_max(macd_line, 10, 10);
        TiVec stoch_k(n);
        for (qsizetype i = 0; i < n; ++i)
            stoch_k[i] = (100.0 * (macd_line[i] - macd_min[i])) / (macd_max[i] - macd_min[i]);
        const TiVec stoch_d = ti_ema(stoch_k, 3);
        const TiVec d_min = ti_roll_min(stoch_d, 10, 10);
        const TiVec d_max = ti_roll_max(stoch_d, 10, 10);
        TiVec stoch_kd(n);
        for (qsizetype i = 0; i < n; ++i)
            stoch_kd[i] = (100.0 * (stoch_d[i] - d_min[i])) / (d_max[i] - d_min[i]);
        f.add("stc", ti_ema(stoch_kd, 3));
    }

    // Aroon 25 over a 26-bar window: bars since the window's first highest high / lowest low.
    {
        constexpr int kW = 25;
        TiVec up(n, kTiNaN);
        TiVec down(n, kTiNaN);
        for (qsizetype i = 0; i < n; ++i) {
            if (ti_apply_window_ok(h, i, kW + 1)) {
                const double* x = h.constData() + (i - kW);
                int arg = 0;
                for (int k = 1; k <= kW; ++k) {
                    if (x[k] > x[arg])
                        arg = k;
                }
                up[i] = (static_cast<double>(arg) / kW) * 100.0;
            }
            if (ti_apply_window_ok(l, i, kW + 1)) {
                const double* x = l.constData() + (i - kW);
                int arg = 0;
                for (int k = 1; k <= kW; ++k) {
                    if (x[k] < x[arg])
                        arg = k;
                }
                down[i] = (static_cast<double>(arg) / kW) * 100.0;
            }
        }
        TiVec indicator(n);
        for (qsizetype i = 0; i < n; ++i)
            indicator[i] = up[i] - down[i];
        f.add("aroon_up", up);
        f.add("aroon_down", down);
        f.add("aroon_indicator", indicator);
    }
}

void ti_add_momentum(TiFrameBuilder& f, const PriceSeries& s) {
    const TiVec& h = s.high;
    const TiVec& l = s.low;
    const TiVec& c = s.close;
    const qsizetype n = c.size();

    const TiVec rsi = ti_rsi(c, 14);
    f.add("rsi", rsi);

    TiStoch stoch = ti_stochastic(h, l, c, 14, 3);
    f.add("stoch_k", stoch.k);
    f.add("stoch_d", stoch.d);

    // Stochastic RSI (14, 3, 3) — 0..1 scale.
    {
        const TiVec lowest = ti_roll_min(rsi, 14, 14);
        const TiVec highest = ti_roll_max(rsi, 14, 14);
        TiVec srsi(n);
        for (qsizetype i = 0; i < n; ++i)
            srsi[i] = (rsi[i] - lowest[i]) / (highest[i] - lowest[i]);
        const TiVec k = ti_roll_mean(srsi, 3, 3);
        f.add("stoch_rsi", srsi);
        f.add("stoch_rsi_k", k);
        f.add("stoch_rsi_d", ti_roll_mean(k, 3, 3));
    }

    f.add("williams_r", ti_williams_r(h, l, c, 14));

    // Awesome oscillator (5, 34) of the median price.
    {
        TiVec median(n);
        for (qsizetype i = 0; i < n; ++i)
            median[i] = 0.5 * (h[i] + l[i]);
        const TiVec fast = ti_roll_mean(median, 5, 5);
        const TiVec slow = ti_roll_mean(median, 34, 34);
        TiVec ao(n);
        for (qsizetype i = 0; i < n; ++i)
            ao[i] = fast[i] - slow[i];
        f.add("ao", ao);
    }

    // KAMA (10, 2, 30). ta builds the volatility and the 10-bar change with np.roll, so the first
    // bars wrap round to the newest closes — those rows fall inside the warm-up.
    {
        constexpr int kW = 10;
        const TiVec rolled1 = ti_np_roll(c, 1);
        const TiVec rolled_w = ti_np_roll(c, kW);
        TiVec volatility(n);
        TiVec change(n);
        for (qsizetype i = 0; i < n; ++i) {
            volatility[i] = std::fabs(c[i] - rolled1[i]);
            change[i] = std::fabs(c[i] - rolled_w[i]);
        }
        const TiVec er_den = ti_roll_sum(volatility, kW, kW);
        constexpr double kFastMinusSlow = 2.0 / (2 + 1) - 2.0 / (30 + 1.0);
        constexpr double kSlowSc = 2 / (30 + 1.0);
        TiVec sc(n);
        for (qsizetype i = 0; i < n; ++i) {
            const double er = er_den[i] != 0 ? change[i] / er_den[i] : 0.0;
            const double scaled = er * kFastMinusSlow;
            const double base = scaled + kSlowSc;
            sc[i] = base * base;
        }
        TiVec kama(n, 0.0);
        bool first_value = true;
        for (qsizetype i = 0; i < n; ++i) {
            if (std::isnan(sc[i])) {
                kama[i] = kTiNaN;
            } else if (first_value) {
                kama[i] = c[i];
                first_value = false;
            } else {
                const double step = sc[i] * (c[i] - kama[i - 1]);
                kama[i] = kama[i - 1] + step;
            }
        }
        f.add("kama", kama);
    }

    // ROC 12 (percent).
    {
        const TiVec prev = ti_shift(c, 12);
        TiVec roc(n);
        for (qsizetype i = 0; i < n; ++i)
            roc[i] = ((c[i] - prev[i]) / prev[i]) * 100.0;
        f.add("roc", roc);
    }

    // TSI (25, 13): double-smoothed momentum over double-smoothed absolute momentum.
    {
        TiVec diff(n);
        TiVec abs_diff(n);
        for (qsizetype i = 0; i < n; ++i) {
            diff[i] = i > 0 ? c[i] - c[i - 1] : kTiNaN;
            abs_diff[i] = std::fabs(diff[i]);
        }
        const TiVec smoothed = ti_ema(ti_ema(diff, 25), 13);
        const TiVec smoothed_abs = ti_ema(ti_ema(abs_diff, 25), 13);
        TiVec tsi(n);
        for (qsizetype i = 0; i < n; ++i)
            tsi[i] = (smoothed[i] / smoothed_abs[i]) * 100.0;
        f.add("tsi", tsi);
    }

    // Ultimate oscillator (7, 14, 28; weights 4, 2, 1).
    {
        const TiVec prev_close = ti_shift(c, 1);
        const TiVec tr = ti_true_range(h, l, prev_close);
        TiVec bp(n);
        for (qsizetype i = 0; i < n; ++i) {
            // min(low, prev close) with NaN propagation (skipna=False).
            const double floor_px = (std::isnan(l[i]) || std::isnan(prev_close[i]))
                                        ? kTiNaN
                                        : (l[i] <= prev_close[i] ? l[i] : prev_close[i]);
            bp[i] = c[i] - floor_px;
        }
        const int windows[3] = {7, 14, 28};
        TiVec avg[3];
        for (int k = 0; k < 3; ++k) {
            const TiVec bp_sum = ti_roll_sum(bp, windows[k], windows[k]);
            const TiVec tr_sum = ti_roll_sum(tr, windows[k], windows[k]);
            avg[k] = TiVec(n);
            for (qsizetype i = 0; i < n; ++i)
                avg[k][i] = bp_sum[i] / tr_sum[i];
        }
        TiVec uo(n);
        for (qsizetype i = 0; i < n; ++i) {
            const double t1 = 4.0 * avg[0][i];
            const double t2 = 2.0 * avg[1][i];
            const double t3 = 1.0 * avg[2][i];
            double acc = t1 + t2;
            acc = acc + t3;
            const double scaled = 100.0 * acc;
            uo[i] = scaled / (4.0 + 2.0 + 1.0);
        }
        f.add("uo", uo);
    }

    // PPO / PVO (26, 12, 9): MACD as a percentage of the slow EMA.
    auto add_percentage_oscillator = [&](const TiVec& src, const char* main, const char* signal, const char* hist) {
        const TiVec fast = ti_ema(src, 12);
        const TiVec slow = ti_ema(src, 26);
        TiVec po(n);
        for (qsizetype i = 0; i < n; ++i)
            po[i] = ((fast[i] - slow[i]) / slow[i]) * 100.0;
        const TiVec sig = ti_ema(po, 9);
        TiVec histogram(n);
        for (qsizetype i = 0; i < n; ++i)
            histogram[i] = po[i] - sig[i];
        f.add(main, po);
        f.add(signal, sig);
        f.add(hist, histogram);
    };
    add_percentage_oscillator(c, "ppo", "ppo_signal", "ppo_hist");
    if (s.has_volume)
        add_percentage_oscillator(s.volume, "pvo", "pvo_signal", "pvo_hist");
}

void ti_add_volatility(TiFrameBuilder& f, const PriceSeries& s) {
    const TiVec& h = s.high;
    const TiVec& l = s.low;
    const TiVec& c = s.close;
    const qsizetype n = c.size();

    f.add("atr", ti_atr(h, l, c, 14));

    TiBands bb = ti_bollinger(c, 20, 2);
    f.add("bb_mavg", bb.mavg);
    f.add("bb_hband", bb.hband);
    f.add("bb_lband", bb.lband);
    f.add("bb_pband", bb.pband);
    f.add("bb_wband", bb.wband);
    f.add("bb_hband_indicator", bb.hband_indicator);
    f.add("bb_lband_indicator", bb.lband_indicator);

    // Keltner channel, original version (20): SMA of the typical price; the bands are SMAs of
    // (4h - 2l + c) / 3 and (-2h + 4l + c) / 3 that accept partial windows.
    {
        TiVec high_src(n);
        TiVec low_src(n);
        for (qsizetype i = 0; i < n; ++i) {
            high_src[i] = ((4.0 * h[i]) - (2.0 * l[i]) + c[i]) / 3.0;
            low_src[i] = ((-2.0 * h[i]) + (4.0 * l[i]) + c[i]) / 3.0;
        }
        const TiVec mid = ti_roll_mean(ti_typical_price(h, l, c), 20, 20);
        const TiVec upper = ti_roll_mean(high_src, 20, 0);
        const TiVec lower = ti_roll_mean(low_src, 20, 0);
        TiVec pband(n);
        TiVec wband(n);
        TiVec hind(n);
        TiVec lind(n);
        for (qsizetype i = 0; i < n; ++i) {
            wband[i] = ((upper[i] - lower[i]) / mid[i]) * 100.0;
            pband[i] = (c[i] - lower[i]) / (upper[i] - lower[i]);
            hind[i] = c[i] > upper[i] ? 1.0 : 0.0;
            lind[i] = c[i] < lower[i] ? 1.0 : 0.0;
        }
        f.add("kc_mavg", mid);
        f.add("kc_hband", upper);
        f.add("kc_lband", lower);
        f.add("kc_pband", pband);
        f.add("kc_wband", wband);
        f.add("kc_hband_indicator", hind);
        f.add("kc_lband_indicator", lind);
    }

    // Donchian channel (20).
    {
        const TiVec upper = ti_roll_max(h, 20, 20);
        const TiVec lower = ti_roll_min(l, 20, 20);
        const TiVec mavg = ti_roll_mean(c, 20, 20);
        TiVec mband(n);
        TiVec pband(n);
        TiVec wband(n);
        for (qsizetype i = 0; i < n; ++i) {
            mband[i] = ((upper[i] - lower[i]) / 2.0) + lower[i];
            pband[i] = (c[i] - lower[i]) / (upper[i] - lower[i]);
            wband[i] = ((upper[i] - lower[i]) / mavg[i]) * 100.0;
        }
        f.add("dc_hband", upper);
        f.add("dc_lband", lower);
        f.add("dc_mband", mband);
        f.add("dc_pband", pband);
        f.add("dc_wband", wband);
    }

    // Ulcer index (14): RMS of the % drawdown from the rolling 14-bar high.
    {
        constexpr int kW = 14;
        const TiVec peak = ti_roll_max(c, kW, 1);
        TiVec drawdown(n);
        for (qsizetype i = 0; i < n; ++i)
            drawdown[i] = (100.0 * (c[i] - peak[i])) / peak[i];
        TiVec ui(n, kTiNaN);
        double sq[kW];
        for (qsizetype i = 0; i < n; ++i) {
            if (!ti_apply_window_ok(drawdown, i, kW))
                continue;
            const double* x = drawdown.constData() + (i + 1 - kW);
            for (int k = 0; k < kW; ++k)
                sq[k] = (x[k] * x[k]) / static_cast<double>(kW);
            ui[i] = std::sqrt(ti_np_sum(sq, kW));
        }
        f.add("ui", ui);
    }
}

void ti_add_volume(TiFrameBuilder& f, const PriceSeries& s) {
    const TiVec& h = s.high;
    const TiVec& l = s.low;
    const TiVec& c = s.close;
    const TiVec& v = s.volume;
    const qsizetype n = c.size();

    const TiVec clv = ti_close_location(h, l, c);
    TiVec money_flow_volume(n);
    for (qsizetype i = 0; i < n; ++i)
        money_flow_volume[i] = clv[i] * v[i];

    f.add("adi", ti_cumsum(money_flow_volume));
    f.add("obv", ti_obv(c, v));

    {
        const TiVec mfv_sum = ti_roll_sum(money_flow_volume, 20, 20);
        const TiVec vol_sum = ti_roll_sum(v, 20, 20);
        TiVec cmf(n);
        for (qsizetype i = 0; i < n; ++i)
            cmf[i] = mfv_sum[i] / vol_sum[i];
        f.add("cmf", cmf);
    }

    {
        TiVec force(n);
        for (qsizetype i = 0; i < n; ++i)
            force[i] = i > 0 ? (c[i] - c[i - 1]) * v[i] : kTiNaN;
        f.add("fi", ti_ema(force, 13));
    }

    // Ease of movement (14), scaled by 1e8.
    {
        TiVec emv(n);
        for (qsizetype i = 0; i < n; ++i) {
            const double moved = i > 0 ? (h[i] - h[i - 1]) + (l[i] - l[i - 1]) : kTiNaN;
            emv[i] = ((moved * (h[i] - l[i])) / (2.0 * v[i])) * 100000000.0;
        }
        f.add("eom", emv);
        f.add("eom_signal", ti_roll_mean(emv, 14, 14));
    }

    const TiVec pct = ti_pct_change(c);
    {
        TiVec flow(n);
        for (qsizetype i = 0; i < n; ++i)
            flow[i] = pct[i] * v[i];
        f.add("vpt", ti_cumsum(flow));
    }

    // Negative volume index: compounds the % change only on bars whose volume fell.
    {
        TiVec nvi(n, kTiNaN);
        if (n > 0)
            nvi[0] = 1000.0;
        for (qsizetype i = 1; i < n; ++i)
            nvi[i] = v[i - 1] > v[i] ? nvi[i - 1] * (1.0 + pct[i]) : nvi[i - 1];
        f.add("nvi", nvi);
    }

    f.add("vwap", ti_vwap(h, l, c, v, 14));

    // Money flow index (14).
    {
        constexpr int kW = 14;
        const TiVec tp = ti_typical_price(h, l, c);
        TiVec flow(n);
        for (qsizetype i = 0; i < n; ++i) {
            const double prev = i > 0 ? tp[i - 1] : kTiNaN;
            const double direction = tp[i] > prev ? 1.0 : (tp[i] < prev ? -1.0 : 0.0);
            flow[i] = (tp[i] * v[i]) * direction;
        }
        TiVec mfi(n, kTiNaN);
        double positive[kW];
        double negative[kW];
        for (qsizetype i = 0; i < n; ++i) {
            if (!ti_apply_window_ok(flow, i, kW))
                continue;
            const double* x = flow.constData() + (i + 1 - kW);
            for (int k = 0; k < kW; ++k) {
                positive[k] = x[k] >= 0.0 ? x[k] : 0.0;
                negative[k] = x[k] < 0.0 ? x[k] : 0.0;
            }
            const double ratio = ti_np_sum(positive, kW) / std::fabs(ti_np_sum(negative, kW));
            mfi[i] = 100.0 - (100.0 / (1.0 + ratio));
        }
        f.add("mfi", mfi);
    }
}

void ti_add_others(TiFrameBuilder& f, const PriceSeries& s) {
    const TiVec& c = s.close;
    const qsizetype n = c.size();
    TiVec daily(n, kTiNaN);
    TiVec daily_log(n, kTiNaN);
    TiVec cumulative(n);
    for (qsizetype i = 0; i < n; ++i) {
        if (i > 0) {
            daily[i] = ((c[i] / c[i - 1]) - 1.0) * 100.0;
            daily_log[i] = (std::log(c[i]) - std::log(c[i - 1])) * 100.0;
        }
        cumulative[i] = ((c[i] / c[0]) - 1.0) * 100.0;
    }
    f.add("daily_return", daily);
    f.add("daily_log_return", daily_log);
    f.add("cumulative_return", cumulative);
}

QJsonValue ti_json_number(double v) {
    return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}

// Numeric cell of a bar: the lowercase key, else its capitalised form; NaN when absent / not a
// number. `present` reports whether either key exists at all.
double ti_bar_value(const QJsonObject& bar, const QString& key, const QString& capitalised, bool* present) {
    auto it = bar.constFind(key);
    if (it == bar.constEnd())
        it = bar.constFind(capitalised);
    if (it == bar.constEnd())
        return kTiNaN;
    *present = true;
    return it->isDouble() ? it->toDouble() : kTiNaN;
}

} // namespace

bool read_price_series(const QJsonArray& bars, const QStringList& required, PriceSeries* out, QString* error) {
    const qsizetype n = bars.size();
    PriceSeries s;
    s.open = TiVec(n, kTiNaN);
    s.high = TiVec(n, kTiNaN);
    s.low = TiVec(n, kTiNaN);
    s.close = TiVec(n, kTiNaN);
    s.volume = TiVec(n, kTiNaN);
    static const QString kKeys[5] = {QStringLiteral("open"), QStringLiteral("high"), QStringLiteral("low"),
                                     QStringLiteral("close"), QStringLiteral("volume")};
    static const QString kCaps[5] = {QStringLiteral("Open"), QStringLiteral("High"), QStringLiteral("Low"),
                                     QStringLiteral("Close"), QStringLiteral("Volume")};
    TiVec* targets[5] = {&s.open, &s.high, &s.low, &s.close, &s.volume};
    bool present[5] = {false, false, false, false, false};
    for (qsizetype i = 0; i < n; ++i) {
        const QJsonValue bar = bars.at(i);
        if (!bar.isObject()) {
            if (error)
                *error = QStringLiteral("Price row %1 is not an object").arg(i);
            return false;
        }
        const QJsonObject o = bar.toObject();
        for (int k = 0; k < 5; ++k)
            (*targets[k])[i] = ti_bar_value(o, kKeys[k], kCaps[k], &present[k]);
    }
    for (const QString& col : required) {
        for (int k = 0; k < 5; ++k) {
            if (col == kKeys[k] && !present[k]) {
                if (error)
                    *error = QStringLiteral("Missing required column: %1").arg(col);
                return false;
            }
        }
    }
    s.has_volume = present[4];
    *out = std::move(s);
    return true;
}

QVector<IndicatorColumn> compute_all(const PriceSeries& series) {
    TiFrameBuilder f;
    ti_add_trend(f, series);
    ti_add_momentum(f, series);
    ti_add_volatility(f, series);
    if (series.has_volume)
        ti_add_volume(f, series);
    ti_add_others(f, series);
    return f.cols;
}

TechnicalsFrame compute_frame(const QJsonArray& bars, qsizetype keep_last) {
    TechnicalsFrame frame;
    PriceSeries series;
    if (!read_price_series(
            bars, {QStringLiteral("open"), QStringLiteral("high"), QStringLiteral("low"), QStringLiteral("close")},
            &series, &frame.error))
        return frame;
    const QVector<IndicatorColumn> cols = compute_all(series);
    const qsizetype n = bars.size();
    const qsizetype first = keep_last > 0 ? std::max<qsizetype>(0, n - keep_last) : 0;
    for (qsizetype i = first; i < n; ++i) {
        QJsonObject row = bars.at(i).toObject();
        for (const IndicatorColumn& col : cols)
            row.insert(col.name, ti_json_number(col.values[i]));
        frame.rows.append(row);
    }
    frame.ok = true;
    return frame;
}

// ── Single indicator ─────────────────────────────────────────────────────────────────────────────

namespace {

enum class TiSingle { Sma, Ema, Rsi, Macd, Bbands, Atr, Stoch, Adx, Cci, Willr, Obv, Vwap };

struct TiSingleDef {
    const char* name;
    TiSingle id;
    bool uses_period;
    bool needs_high_low;
    bool needs_volume;
};

constexpr TiSingleDef kTiSingleDefs[] = {
    {"SMA", TiSingle::Sma, true, false, false},       {"EMA", TiSingle::Ema, true, false, false},
    {"RSI", TiSingle::Rsi, true, false, false},       {"MACD", TiSingle::Macd, false, false, false},
    {"BBANDS", TiSingle::Bbands, true, false, false}, {"ATR", TiSingle::Atr, true, true, false},
    {"STOCH", TiSingle::Stoch, true, true, false},    {"ADX", TiSingle::Adx, true, true, false},
    {"CCI", TiSingle::Cci, true, true, false},        {"WILLR", TiSingle::Willr, true, true, false},
    {"OBV", TiSingle::Obv, false, false, true},       {"VWAP", TiSingle::Vwap, true, true, true},
};

// Bars needed before the indicator's main column has its first value (clean data).
qint64 ti_single_min_bars(TiSingle id, int period) {
    switch (id) {
        case TiSingle::Macd:
            return 26;
        case TiSingle::Adx:
            return 2 * static_cast<qint64>(period);
        case TiSingle::Obv:
            return 1;
        default:
            return period;
    }
}

// Price rows out of the node's input: an array of bars, an object wrapping one, or column arrays.
bool ti_extract_bars(const QJsonValue& input, QJsonArray* bars, QString* error) {
    if (input.isArray()) {
        *bars = input.toArray();
        return true;
    }
    if (!input.isObject()) {
        *error = QStringLiteral("Technical Indicators needs price data — connect a Historical Data node (an array "
                                "of OHLCV rows)");
        return false;
    }
    const QJsonObject o = input.toObject();
    for (const char* key : {"data", "rows", "candles", "bars", "history", "prices"}) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isArray()) {
            *bars = v.toArray();
            return true;
        }
    }
    // Column arrays: {"close": [...], "high": [...], ...} → one object per index.
    qsizetype len = -1;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (!it->isArray())
            continue;
        len = std::max<qsizetype>(len, it->toArray().size());
    }
    const bool has_close = o.value(QStringLiteral("close")).isArray() || o.value(QStringLiteral("Close")).isArray();
    if (len < 0 || !has_close) {
        *error = QStringLiteral("Technical Indicators could not find price rows in the input (expected an array "
                                "of OHLCV rows, or an object with them under \"data\")");
        return false;
    }
    QJsonArray out;
    for (qsizetype i = 0; i < len; ++i) {
        QJsonObject row;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (it->isArray()) {
                const QJsonArray col = it->toArray();
                row.insert(it.key(), i < col.size() ? col.at(i) : QJsonValue(QJsonValue::Null));
            }
        }
        out.append(row);
    }
    *bars = out;
    return true;
}

} // namespace

QStringList single_indicator_names() {
    QStringList names;
    for (const TiSingleDef& d : kTiSingleDefs)
        names.append(QString::fromLatin1(d.name));
    return names;
}

SingleIndicatorResult compute_single(const QJsonValue& price_data, const QString& indicator, int period) {
    SingleIndicatorResult res;
    const QString wanted = indicator.trimmed().toUpper();
    const TiSingleDef* def = nullptr;
    for (const TiSingleDef& d : kTiSingleDefs) {
        if (wanted == QLatin1String(d.name))
            def = &d;
    }
    if (!def) {
        res.error = QStringLiteral("Unknown indicator '%1' (expected one of %2)")
                        .arg(indicator, single_indicator_names().join(QStringLiteral(", ")));
        return res;
    }
    if (def->uses_period && (period < 1 || period > kMaxIndicatorPeriod)) {
        res.error = QStringLiteral("Period must be between 1 and %1 bars").arg(kMaxIndicatorPeriod);
        return res;
    }

    QJsonArray bars;
    if (!ti_extract_bars(price_data, &bars, &res.error))
        return res;
    if (bars.isEmpty()) {
        res.error = QStringLiteral("The price data is empty — there are no bars to compute %1 on")
                        .arg(QLatin1String(def->name));
        return res;
    }
    QStringList required{QStringLiteral("close")};
    if (def->needs_high_low)
        required << QStringLiteral("high") << QStringLiteral("low");
    if (def->needs_volume)
        required << QStringLiteral("volume");
    PriceSeries s;
    if (!read_price_series(bars, required, &s, &res.error)) {
        if (res.error == QLatin1String("Missing required column: volume"))
            res.error = QStringLiteral("%1 needs a volume column in the price data").arg(QLatin1String(def->name));
        return res;
    }
    const qsizetype n = s.close.size();
    const QString label = def->uses_period ? QStringLiteral("%1(%2)").arg(QLatin1String(def->name)).arg(period)
                                           : QString::fromLatin1(def->name);
    const qint64 min_bars = ti_single_min_bars(def->id, period);
    if (n < min_bars) {
        res.error =
            QStringLiteral("%1 needs at least %2 bars of price data; the input has %3").arg(label).arg(min_bars).arg(n);
        return res;
    }

    const TiVec& h = s.high;
    const TiVec& l = s.low;
    const TiVec& c = s.close;
    QVector<IndicatorColumn> cols;
    QJsonObject params;
    auto add = [&cols](const QString& name, TiVec values) { cols.append(IndicatorColumn{name, std::move(values)}); };
    switch (def->id) {
        case TiSingle::Sma:
            add(QStringLiteral("sma_%1").arg(period), ti_roll_mean(c, period, period));
            params[QStringLiteral("window")] = period;
            break;
        case TiSingle::Ema:
            add(QStringLiteral("ema_%1").arg(period), ti_ema(c, period));
            params[QStringLiteral("window")] = period;
            break;
        case TiSingle::Rsi:
            add(QStringLiteral("rsi"), ti_rsi(c, period));
            params[QStringLiteral("window")] = period;
            break;
        case TiSingle::Macd: {
            TiMacd m = ti_macd(c, 12, 26, 9);
            add(QStringLiteral("macd"), m.line);
            add(QStringLiteral("macd_signal"), m.signal);
            add(QStringLiteral("macd_diff"), m.diff);
            params = QJsonObject{{"fast", 12}, {"slow", 26}, {"signal", 9}};
            break;
        }
        case TiSingle::Bbands: {
            TiBands b = ti_bollinger(c, period, 2);
            // ta reports "not above / below the band" (0) before any band exists.
            for (qsizetype i = 0; i < n; ++i) {
                if (std::isnan(b.mavg[i]))
                    b.hband_indicator[i] = b.lband_indicator[i] = kTiNaN;
            }
            add(QStringLiteral("bb_mavg"), b.mavg);
            add(QStringLiteral("bb_hband"), b.hband);
            add(QStringLiteral("bb_lband"), b.lband);
            add(QStringLiteral("bb_pband"), b.pband);
            add(QStringLiteral("bb_wband"), b.wband);
            add(QStringLiteral("bb_hband_indicator"), b.hband_indicator);
            add(QStringLiteral("bb_lband_indicator"), b.lband_indicator);
            params = QJsonObject{{"window", period}, {"std_dev", 2}};
            break;
        }
        case TiSingle::Atr: {
            TiVec atr = ti_atr(h, l, c, period);
            for (qsizetype i = 0; i < std::min<qsizetype>(n, period - 1); ++i)
                atr[i] = kTiNaN; // ta's placeholder zeros
            add(QStringLiteral("atr"), atr);
            params[QStringLiteral("window")] = period;
            break;
        }
        case TiSingle::Stoch: {
            TiStoch st = ti_stochastic(h, l, c, period, 3);
            add(QStringLiteral("stoch_k"), st.k);
            add(QStringLiteral("stoch_d"), st.d);
            params = QJsonObject{{"window", period}, {"smooth_window", 3}};
            break;
        }
        case TiSingle::Adx: {
            TiAdx a = ti_adx(h, l, c, period);
            for (qsizetype i = 0; i < n; ++i) {
                if (i < 2 * static_cast<qsizetype>(period) - 1)
                    a.adx[i] = kTiNaN; // ta's placeholder zeros
                if (i <= period)
                    a.pos[i] = a.neg[i] = kTiNaN;
            }
            add(QStringLiteral("adx"), a.adx);
            add(QStringLiteral("adx_pos"), a.pos);
            add(QStringLiteral("adx_neg"), a.neg);
            params[QStringLiteral("window")] = period;
            break;
        }
        case TiSingle::Cci:
            add(QStringLiteral("cci"), ti_cci(h, l, c, period, 0.015));
            params = QJsonObject{{"window", period}, {"constant", 0.015}};
            break;
        case TiSingle::Willr:
            add(QStringLiteral("williams_r"), ti_williams_r(h, l, c, period));
            params[QStringLiteral("lookback")] = period;
            break;
        case TiSingle::Obv:
            add(QStringLiteral("obv"), ti_obv(c, s.volume));
            break;
        case TiSingle::Vwap:
            add(QStringLiteral("vwap"), ti_vwap(h, l, c, s.volume, period));
            params[QStringLiteral("window")] = period;
            break;
    }

    const IndicatorColumn& main_col = cols.first();
    bool any_value = false;
    for (double v : main_col.values) {
        if (std::isfinite(v)) {
            any_value = true;
            break;
        }
    }
    if (!any_value) {
        res.error = QStringLiteral("%1 has no value for any bar — the input's %2 values are missing or constant")
                        .arg(label, def->needs_high_low ? QStringLiteral("high/low/close") : QStringLiteral("close"));
        return res;
    }

    QJsonArray rows;
    for (qsizetype i = 0; i < n; ++i) {
        QJsonObject row = bars.at(i).toObject();
        for (const IndicatorColumn& col : cols)
            row.insert(col.name, ti_json_number(col.values[i]));
        rows.append(row);
    }
    QJsonArray names;
    for (const IndicatorColumn& col : cols)
        names.append(col.name);

    QJsonObject out;
    out[QStringLiteral("success")] = true;
    out[QStringLiteral("indicator")] = QString::fromLatin1(def->name);
    out[QStringLiteral("period")] = def->uses_period ? QJsonValue(period) : QJsonValue(QJsonValue::Null);
    out[QStringLiteral("params")] = params;
    out[QStringLiteral("columns")] = names;
    out[QStringLiteral("value")] = ti_json_number(main_col.values.last());
    out[QStringLiteral("latest")] = rows.last();
    out[QStringLiteral("data")] = rows;
    res.output = out;
    res.ok = true;
    return res;
}

} // namespace fincept::services::equity::technicals
