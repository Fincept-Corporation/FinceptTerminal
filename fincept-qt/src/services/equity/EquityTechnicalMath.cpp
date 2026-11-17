// src/services/equity/EquityTechnicalMath.cpp
#include "services/equity/EquityTechnicalMath.h"

#include <QDate>
#include <QDateTime>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <map>

namespace fincept::services::equity::tmath {

namespace tmath_detail {

bool fin(double v) {
    return std::isfinite(v);
}

QVector<double> nan_vec(int n) {
    return QVector<double>(n, kNaN);
}

/// Highest high / lowest low over the `n` bars ending at i (NaN when incomplete).
double window_max(const QVector<double>& v, int i, int n) {
    if (i + 1 < n)
        return kNaN;
    double m = -std::numeric_limits<double>::infinity();
    for (int j = i - n + 1; j <= i; ++j) {
        if (!fin(v[j]))
            return kNaN;
        m = std::max(m, v[j]);
    }
    return m;
}
double window_min(const QVector<double>& v, int i, int n) {
    if (i + 1 < n)
        return kNaN;
    double m = std::numeric_limits<double>::infinity();
    for (int j = i - n + 1; j <= i; ++j) {
        if (!fin(v[j]))
            return kNaN;
        m = std::min(m, v[j]);
    }
    return m;
}

/// Wilder smoothing (RMA) of `v`, seeded with the simple mean of the first n finite values.
QVector<double> rma(const QVector<double>& v, int n) {
    const int len = static_cast<int>(v.size());
    QVector<double> out = nan_vec(len);
    double acc = 0.0;
    int count = 0;
    double prev = kNaN;
    for (int i = 0; i < len; ++i) {
        if (!fin(v[i])) {
            if (fin(prev))
                out[i] = prev;
            continue;
        }
        if (!fin(prev)) {
            acc += v[i];
            if (++count == n) {
                prev = acc / n;
                out[i] = prev;
            }
            continue;
        }
        prev = (prev * (n - 1) + v[i]) / n;
        out[i] = prev;
    }
    return out;
}

double last_finite(const QVector<double>& v, int back = 0) {
    int seen = 0;
    for (int i = static_cast<int>(v.size()) - 1; i >= 0; --i) {
        if (fin(v[i])) {
            if (seen == back)
                return v[i];
            ++seen;
        }
    }
    return kNaN;
}

void add_vote(RatingGroup& g, const QString& name, const QString& key, double value, int vote) {
    if (!fin(value))
        return; // not enough history: no vote, not a neutral one
    RatingVote rv;
    rv.name = name;
    rv.key = key;
    rv.value = value;
    rv.signal = vote > 0 ? TechSignal::Buy : vote < 0 ? TechSignal::Sell : TechSignal::Neutral;
    if (vote > 0)
        ++g.buy;
    else if (vote < 0)
        ++g.sell;
    else
        ++g.neutral;
    g.votes.append(rv);
}

void finish_group(RatingGroup& g) {
    const int n = g.buy + g.sell + g.neutral;
    g.score = n > 0 ? static_cast<double>(g.buy - g.sell) / n : kNaN;
    g.signal = signal_for_score(g.score);
}

} // namespace tmath_detail

using namespace tmath_detail;

// ── Bars ─────────────────────────────────────────────────────────────────────

Bars bars_from_candles(const QVector<Candle>& candles) {
    std::map<qint64, const Candle*> by_time;
    for (const auto& c : candles) {
        if (!fin(c.close) || c.close <= 0.0 || c.timestamp <= 0)
            continue;
        by_time[c.timestamp] = &c; // a later duplicate wins
    }
    Bars b;
    const int n = static_cast<int>(by_time.size());
    b.time.reserve(n);
    b.open.reserve(n);
    b.high.reserve(n);
    b.low.reserve(n);
    b.close.reserve(n);
    b.volume.reserve(n);
    for (const auto& kv : by_time) {
        const Candle& c = *kv.second;
        b.time.append(kv.first);
        b.close.append(c.close);
        b.open.append(fin(c.open) && c.open > 0 ? c.open : c.close);
        b.high.append(fin(c.high) && c.high > 0 ? std::max(c.high, c.close) : c.close);
        b.low.append(fin(c.low) && c.low > 0 ? std::min(c.low, c.close) : c.close);
        b.volume.append(static_cast<double>(c.volume));
    }
    return b;
}

qint64 daily_utc_midnight(qint64 epoch_secs) {
    // Exchange-local midnights lie between UTC−10 and UTC+13 of the trading date's UTC
    // midnight; +13 h lands every one of them inside the right UTC day.
    constexpr qint64 kDay = 86400;
    const qint64 shifted = epoch_secs + 13 * 3600;
    return (shifted >= 0 ? shifted / kDay : (shifted - kDay + 1) / kDay) * kDay;
}

QVector<Candle> normalize_daily(QVector<Candle> candles) {
    for (auto& c : candles)
        c.timestamp = daily_utc_midnight(c.timestamp);
    std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) { return a.timestamp < b.timestamp; });
    // A late intraday print of today can duplicate the last daily bar: keep the latest.
    QVector<Candle> out;
    out.reserve(candles.size());
    for (const auto& c : candles) {
        if (!out.isEmpty() && out.last().timestamp == c.timestamp)
            out.last() = c;
        else
            out.append(c);
    }
    return out;
}

Bars resample(const Bars& d, Resample to) {
    if (to == Resample::Daily)
        return d;
    Bars out;
    int cur_key = -1;
    for (int i = 0; i < d.size(); ++i) {
        const QDate day = QDateTime::fromSecsSinceEpoch(d.time[i], QTimeZone::UTC).date();
        int year = 0;
        const int week = day.weekNumber(&year);
        const int key = to == Resample::Weekly ? year * 100 + week : day.year() * 100 + day.month();
        if (key != cur_key) {
            cur_key = key;
            out.time.append(d.time[i]);
            out.open.append(d.open[i]);
            out.high.append(d.high[i]);
            out.low.append(d.low[i]);
            out.close.append(d.close[i]);
            out.volume.append(d.volume[i]);
            continue;
        }
        const int j = out.size() - 1;
        out.high[j] = std::max(out.high[j], d.high[i]);
        out.low[j] = std::min(out.low[j], d.low[i]);
        out.close[j] = d.close[i];
        out.volume[j] += d.volume[i];
    }
    return out;
}

// ── Moving averages ──────────────────────────────────────────────────────────

QVector<double> sma(const QVector<double>& v, int n) {
    const int len = static_cast<int>(v.size());
    QVector<double> out = nan_vec(len);
    if (n <= 0)
        return out;
    double acc = 0.0;
    int valid = 0;
    for (int i = 0; i < len; ++i) {
        if (fin(v[i])) {
            acc += v[i];
            ++valid;
        }
        if (i >= n && fin(v[i - n])) {
            acc -= v[i - n];
            --valid;
        }
        if (i + 1 >= n && valid == n)
            out[i] = acc / n;
    }
    return out;
}

QVector<double> ema(const QVector<double>& v, int n) {
    const int len = static_cast<int>(v.size());
    QVector<double> out = nan_vec(len);
    if (n <= 0)
        return out;
    const double k = 2.0 / (n + 1.0);
    double prev = kNaN;
    double acc = 0.0;
    int count = 0;
    for (int i = 0; i < len; ++i) {
        if (!fin(v[i])) {
            out[i] = prev;
            continue;
        }
        if (!fin(prev)) {
            acc += v[i];
            if (++count == n) {
                prev = acc / n; // SMA seed
                out[i] = prev;
            }
            continue;
        }
        prev = v[i] * k + prev * (1.0 - k);
        out[i] = prev;
    }
    return out;
}

QVector<double> wma(const QVector<double>& v, int n) {
    const int len = static_cast<int>(v.size());
    QVector<double> out = nan_vec(len);
    if (n <= 0)
        return out;
    const double denom = n * (n + 1) / 2.0;
    for (int i = n - 1; i < len; ++i) {
        double acc = 0.0;
        bool ok = true;
        for (int j = 0; j < n; ++j) {
            const double x = v[i - n + 1 + j];
            if (!fin(x)) {
                ok = false;
                break;
            }
            acc += x * (j + 1);
        }
        if (ok)
            out[i] = acc / denom;
    }
    return out;
}

QVector<double> hma(const QVector<double>& v, int n) {
    const QVector<double> half = wma(v, std::max(1, n / 2));
    const QVector<double> full = wma(v, n);
    QVector<double> diff(v.size(), kNaN);
    for (int i = 0; i < v.size(); ++i)
        if (fin(half[i]) && fin(full[i]))
            diff[i] = 2.0 * half[i] - full[i];
    return wma(diff, std::max(1, static_cast<int>(std::lround(std::sqrt(static_cast<double>(n))))));
}

QVector<double> vwma(const QVector<double>& close, const QVector<double>& volume, int n) {
    const int len = static_cast<int>(close.size());
    QVector<double> out = nan_vec(len);
    for (int i = n - 1; i < len; ++i) {
        double pv = 0.0, vv = 0.0;
        bool ok = true;
        for (int j = i - n + 1; j <= i; ++j) {
            if (!fin(close[j]) || !fin(volume[j])) {
                ok = false;
                break;
            }
            pv += close[j] * volume[j];
            vv += volume[j];
        }
        if (ok && vv > 0.0)
            out[i] = pv / vv;
    }
    return out;
}

// ── Oscillators ──────────────────────────────────────────────────────────────

QVector<double> rsi(const QVector<double>& close, int n) {
    const int len = static_cast<int>(close.size());
    QVector<double> gain = nan_vec(len), loss = nan_vec(len);
    for (int i = 1; i < len; ++i) {
        if (!fin(close[i]) || !fin(close[i - 1]))
            continue;
        const double d = close[i] - close[i - 1];
        gain[i] = d > 0 ? d : 0.0;
        loss[i] = d < 0 ? -d : 0.0;
    }
    const QVector<double> ag = rma(gain, n), al = rma(loss, n);
    QVector<double> out = nan_vec(len);
    for (int i = 0; i < len; ++i) {
        if (!fin(ag[i]) || !fin(al[i]))
            continue;
        out[i] = al[i] == 0.0 ? (ag[i] == 0.0 ? 50.0 : 100.0) : 100.0 - 100.0 / (1.0 + ag[i] / al[i]);
    }
    return out;
}

Macd macd(const QVector<double>& close, int fast, int slow, int signal) {
    Macd m;
    const QVector<double> f = ema(close, fast), s = ema(close, slow);
    m.line = nan_vec(static_cast<int>(close.size()));
    for (int i = 0; i < close.size(); ++i)
        if (fin(f[i]) && fin(s[i]))
            m.line[i] = f[i] - s[i];
    m.signal = ema(m.line, signal);
    m.hist = nan_vec(static_cast<int>(close.size()));
    for (int i = 0; i < close.size(); ++i)
        if (fin(m.line[i]) && fin(m.signal[i]))
            m.hist[i] = m.line[i] - m.signal[i];
    return m;
}

Bands bollinger(const QVector<double>& close, int n, double k) {
    Bands b;
    const int len = static_cast<int>(close.size());
    b.mid = sma(close, n);
    b.upper = nan_vec(len);
    b.lower = nan_vec(len);
    for (int i = n - 1; i < len; ++i) {
        if (!fin(b.mid[i]))
            continue;
        double ss = 0.0;
        for (int j = i - n + 1; j <= i; ++j)
            ss += (close[j] - b.mid[i]) * (close[j] - b.mid[i]);
        const double sd = std::sqrt(ss / n);
        b.upper[i] = b.mid[i] + k * sd;
        b.lower[i] = b.mid[i] - k * sd;
    }
    return b;
}

Stoch stochastic(const Bars& b, int n, int smooth_k, int smooth_d) {
    const int len = b.size();
    QVector<double> raw = nan_vec(len);
    for (int i = 0; i < len; ++i) {
        const double hh = window_max(b.high, i, n), ll = window_min(b.low, i, n);
        if (fin(hh) && fin(ll) && hh > ll)
            raw[i] = 100.0 * (b.close[i] - ll) / (hh - ll);
        else if (fin(hh) && fin(ll))
            raw[i] = 50.0;
    }
    Stoch s;
    s.k = sma(raw, smooth_k);
    s.d = sma(s.k, smooth_d);
    return s;
}

Stoch stoch_rsi(const QVector<double>& close, int rsi_n, int stoch_n, int smooth_k, int smooth_d) {
    const QVector<double> r = rsi(close, rsi_n);
    const int len = static_cast<int>(close.size());
    QVector<double> raw = nan_vec(len);
    for (int i = 0; i < len; ++i) {
        const double hh = window_max(r, i, stoch_n), ll = window_min(r, i, stoch_n);
        if (fin(hh) && fin(ll))
            raw[i] = hh > ll ? 100.0 * (r[i] - ll) / (hh - ll) : 50.0;
    }
    Stoch s;
    s.k = sma(raw, smooth_k);
    s.d = sma(s.k, smooth_d);
    return s;
}

QVector<double> cci(const Bars& b, int n) {
    const int len = b.size();
    QVector<double> tp(len);
    for (int i = 0; i < len; ++i)
        tp[i] = (b.high[i] + b.low[i] + b.close[i]) / 3.0;
    const QVector<double> ma = sma(tp, n);
    QVector<double> out = nan_vec(len);
    for (int i = n - 1; i < len; ++i) {
        if (!fin(ma[i]))
            continue;
        double md = 0.0;
        for (int j = i - n + 1; j <= i; ++j)
            md += std::fabs(tp[j] - ma[i]);
        md /= n;
        out[i] = md > 0.0 ? (tp[i] - ma[i]) / (0.015 * md) : 0.0;
    }
    return out;
}

QVector<double> atr(const Bars& b, int n) {
    const int len = b.size();
    QVector<double> tr = nan_vec(len);
    for (int i = 0; i < len; ++i) {
        const double hl = b.high[i] - b.low[i];
        if (i == 0) {
            tr[i] = hl;
            continue;
        }
        tr[i] = std::max({hl, std::fabs(b.high[i] - b.close[i - 1]), std::fabs(b.low[i] - b.close[i - 1])});
    }
    return rma(tr, n);
}

Adx adx(const Bars& b, int n) {
    const int len = b.size();
    QVector<double> pdm = nan_vec(len), mdm = nan_vec(len), tr = nan_vec(len);
    for (int i = 1; i < len; ++i) {
        const double up = b.high[i] - b.high[i - 1];
        const double dn = b.low[i - 1] - b.low[i];
        pdm[i] = (up > dn && up > 0) ? up : 0.0;
        mdm[i] = (dn > up && dn > 0) ? dn : 0.0;
        tr[i] = std::max({b.high[i] - b.low[i], std::fabs(b.high[i] - b.close[i - 1]),
                          std::fabs(b.low[i] - b.close[i - 1])});
    }
    const QVector<double> str = rma(tr, n), spdm = rma(pdm, n), smdm = rma(mdm, n);
    Adx a;
    a.plus_di = nan_vec(len);
    a.minus_di = nan_vec(len);
    QVector<double> dx = nan_vec(len);
    for (int i = 0; i < len; ++i) {
        if (!fin(str[i]) || str[i] <= 0.0)
            continue;
        a.plus_di[i] = 100.0 * spdm[i] / str[i];
        a.minus_di[i] = 100.0 * smdm[i] / str[i];
        const double s = a.plus_di[i] + a.minus_di[i];
        dx[i] = s > 0.0 ? 100.0 * std::fabs(a.plus_di[i] - a.minus_di[i]) / s : 0.0;
    }
    a.adx = rma(dx, n);
    return a;
}

QVector<double> awesome_oscillator(const Bars& b) {
    const int len = b.size();
    QVector<double> mid(len);
    for (int i = 0; i < len; ++i)
        mid[i] = (b.high[i] + b.low[i]) / 2.0;
    const QVector<double> f = sma(mid, 5), s = sma(mid, 34);
    QVector<double> out = nan_vec(len);
    for (int i = 0; i < len; ++i)
        if (fin(f[i]) && fin(s[i]))
            out[i] = f[i] - s[i];
    return out;
}

QVector<double> momentum(const QVector<double>& close, int n) {
    QVector<double> out = nan_vec(static_cast<int>(close.size()));
    for (int i = n; i < close.size(); ++i)
        if (fin(close[i]) && fin(close[i - n]))
            out[i] = close[i] - close[i - n];
    return out;
}

QVector<double> williams_r(const Bars& b, int n) {
    QVector<double> out = nan_vec(b.size());
    for (int i = 0; i < b.size(); ++i) {
        const double hh = window_max(b.high, i, n), ll = window_min(b.low, i, n);
        if (fin(hh) && fin(ll))
            out[i] = hh > ll ? -100.0 * (hh - b.close[i]) / (hh - ll) : -50.0;
    }
    return out;
}

QVector<double> ultimate_oscillator(const Bars& b, int s, int m, int l) {
    const int len = b.size();
    QVector<double> bp = nan_vec(len), tr = nan_vec(len);
    for (int i = 1; i < len; ++i) {
        const double lo = std::min(b.low[i], b.close[i - 1]);
        const double hi = std::max(b.high[i], b.close[i - 1]);
        bp[i] = b.close[i] - lo;
        tr[i] = hi - lo;
    }
    auto avg = [&](int i, int n) {
        if (i < n)
            return kNaN;
        double a = 0.0, t = 0.0;
        for (int j = i - n + 1; j <= i; ++j) {
            if (!fin(bp[j]) || !fin(tr[j]))
                return kNaN;
            a += bp[j];
            t += tr[j];
        }
        return t > 0.0 ? a / t : kNaN;
    };
    QVector<double> out = nan_vec(len);
    for (int i = 0; i < len; ++i) {
        const double a1 = avg(i, s), a2 = avg(i, m), a3 = avg(i, l);
        if (fin(a1) && fin(a2) && fin(a3))
            out[i] = 100.0 * (4.0 * a1 + 2.0 * a2 + a3) / 7.0;
    }
    return out;
}

QVector<double> bull_bear_power(const Bars& b, int n) {
    const QVector<double> e = ema(b.close, n);
    QVector<double> out = nan_vec(b.size());
    for (int i = 0; i < b.size(); ++i)
        if (fin(e[i]))
            out[i] = (b.high[i] - e[i]) + (b.low[i] - e[i]);
    return out;
}

QVector<double> ichimoku_base(const Bars& b, int n) {
    QVector<double> out = nan_vec(b.size());
    for (int i = 0; i < b.size(); ++i) {
        const double hh = window_max(b.high, i, n), ll = window_min(b.low, i, n);
        if (fin(hh) && fin(ll))
            out[i] = (hh + ll) / 2.0;
    }
    return out;
}

QVector<double> obv(const Bars& b) {
    QVector<double> out = nan_vec(b.size());
    double acc = 0.0;
    for (int i = 0; i < b.size(); ++i) {
        if (i > 0) {
            if (b.close[i] > b.close[i - 1])
                acc += b.volume[i];
            else if (b.close[i] < b.close[i - 1])
                acc -= b.volume[i];
        }
        out[i] = acc;
    }
    return out;
}

// ── Rating ───────────────────────────────────────────────────────────────────

TechSignal signal_for_score(double s) {
    if (!fin(s))
        return TechSignal::Neutral;
    if (s > 0.5)
        return TechSignal::StrongBuy;
    if (s > 0.1)
        return TechSignal::Buy;
    if (s >= -0.1)
        return TechSignal::Neutral;
    if (s >= -0.5)
        return TechSignal::Sell;
    return TechSignal::StrongSell;
}

Rating technical_rating(const Bars& b) {
    Rating r;
    if (b.size() < 30)
        return r;
    const double price = b.close.last();

    // Moving averages: price above the average = Buy, below = Sell.
    auto ma_vote = [&](const QString& name, const QString& key, const QVector<double>& series) {
        const double v = series.isEmpty() ? kNaN : series.last();
        add_vote(r.ma, name, key, v, !fin(v) ? 0 : (price > v ? 1 : price < v ? -1 : 0));
    };
    for (int n : {10, 20, 30, 50, 100, 200}) {
        ma_vote(QStringLiteral("SMA %1").arg(n), QStringLiteral("sma_%1").arg(n), sma(b.close, n));
        ma_vote(QStringLiteral("EMA %1").arg(n), QStringLiteral("ema_%1").arg(n), ema(b.close, n));
    }
    ma_vote(QStringLiteral("Ichimoku Base (26)"), QStringLiteral("ichimoku_base"), ichimoku_base(b, 26));
    ma_vote(QStringLiteral("VWMA 20"), QStringLiteral("vwma_20"), vwma(b.close, b.volume, 20));
    ma_vote(QStringLiteral("Hull MA 9"), QStringLiteral("hma_9"), hma(b.close, 9));
    finish_group(r.ma);

    // Oscillators: their own zones plus a direction test (no vote while it points the
    // other way — an oversold reading that is still falling is not yet a buy).
    {
        const QVector<double> x = rsi(b.close, 14);
        const double v = last_finite(x), p = last_finite(x, 1);
        add_vote(r.osc, QStringLiteral("RSI (14)"), QStringLiteral("rsi"), v,
                 (v < 30 && v > p) ? 1 : (v > 70 && v < p) ? -1 : 0);
    }
    {
        const Stoch s = stochastic(b, 14, 3, 3);
        const double k = last_finite(s.k), d = last_finite(s.d);
        add_vote(r.osc, QStringLiteral("Stochastic %K (14, 3, 3)"), QStringLiteral("stoch_k"), k,
                 !fin(d) ? 0 : (k < 20 && k > d) ? 1 : (k > 80 && k < d) ? -1 : 0);
    }
    {
        const QVector<double> x = cci(b, 20);
        const double v = last_finite(x), p = last_finite(x, 1);
        add_vote(r.osc, QStringLiteral("CCI (20)"), QStringLiteral("cci"), v,
                 (v < -100 && v > p) ? 1 : (v > 100 && v < p) ? -1 : 0);
    }
    {
        const Adx a = adx(b, 14);
        const double v = last_finite(a.adx), p = last_finite(a.adx, 1);
        const double pdi = last_finite(a.plus_di), mdi = last_finite(a.minus_di);
        int vote = 0;
        if (fin(v) && v > 20 && v > p)
            vote = pdi > mdi ? 1 : pdi < mdi ? -1 : 0;
        add_vote(r.osc, QStringLiteral("ADX (14)"), QStringLiteral("adx"), v, vote);
    }
    {
        const QVector<double> x = awesome_oscillator(b);
        const double v = last_finite(x), p = last_finite(x, 1);
        add_vote(r.osc, QStringLiteral("Awesome Oscillator"), QStringLiteral("ao"), v,
                 (v > 0 && v > p) ? 1 : (v < 0 && v < p) ? -1 : 0);
    }
    {
        const QVector<double> x = momentum(b.close, 10);
        const double v = last_finite(x), p = last_finite(x, 1);
        add_vote(r.osc, QStringLiteral("Momentum (10)"), QStringLiteral("mom"), v, v > p ? 1 : v < p ? -1 : 0);
    }
    {
        const Macd m = macd(b.close, 12, 26, 9);
        const double l = last_finite(m.line), s = last_finite(m.signal);
        add_vote(r.osc, QStringLiteral("MACD (12, 26, 9)"), QStringLiteral("macd"), l,
                 !fin(s) ? 0 : l > s ? 1 : l < s ? -1 : 0);
    }
    {
        const Stoch s = stoch_rsi(b.close, 14, 14, 3, 3);
        const double k = last_finite(s.k), d = last_finite(s.d);
        add_vote(r.osc, QStringLiteral("Stochastic RSI Fast"), QStringLiteral("stoch_rsi"), k,
                 !fin(d) ? 0 : (k < 20 && k > d) ? 1 : (k > 80 && k < d) ? -1 : 0);
    }
    {
        const QVector<double> x = williams_r(b, 14);
        const double v = last_finite(x), p = last_finite(x, 1);
        add_vote(r.osc, QStringLiteral("Williams %R (14)"), QStringLiteral("williams_r"), v,
                 (v < -80 && v > p) ? 1 : (v > -20 && v < p) ? -1 : 0);
    }
    {
        const QVector<double> x = bull_bear_power(b, 13);
        const double v = last_finite(x), p = last_finite(x, 1);
        add_vote(r.osc, QStringLiteral("Bull Bear Power"), QStringLiteral("bbp"), v,
                 (v > 0 && v > p) ? 1 : (v < 0 && v < p) ? -1 : 0);
    }
    {
        const QVector<double> x = ultimate_oscillator(b, 7, 14, 28);
        const double v = last_finite(x);
        add_vote(r.osc, QStringLiteral("Ultimate Oscillator (7, 14, 28)"), QStringLiteral("uo"), v,
                 v > 70 ? 1 : v < 30 ? -1 : 0);
    }
    finish_group(r.osc);

    if (fin(r.ma.score) && fin(r.osc.score))
        r.summary = (r.ma.score + r.osc.score) / 2.0;
    else if (fin(r.ma.score))
        r.summary = r.ma.score;
    else
        r.summary = r.osc.score;
    r.summary_signal = signal_for_score(r.summary);
    r.ok = fin(r.summary);
    return r;
}

// ── Performance / risk ───────────────────────────────────────────────────────

double return_since(const Bars& b, qint64 from_secs) {
    if (b.size() < 2 || b.time.first() > from_secs)
        return kNaN;
    const auto it = std::upper_bound(b.time.cbegin(), b.time.cend(), from_secs);
    const int i = static_cast<int>(it - b.time.cbegin()) - 1;
    if (i < 0 || !(b.close[i] > 0.0))
        return kNaN;
    return b.close.last() / b.close[i] - 1.0;
}

double realized_vol(const Bars& b, int window, double periods_per_year) {
    const int n = b.size();
    const int start = window > 0 ? std::max(1, n - window) : 1;
    QVector<double> r;
    for (int i = start; i < n; ++i)
        if (b.close[i] > 0.0 && b.close[i - 1] > 0.0)
            r.append(std::log(b.close[i] / b.close[i - 1]));
    if (r.size() < 5)
        return kNaN;
    double mean = 0.0;
    for (double x : r)
        mean += x;
    mean /= r.size();
    double ss = 0.0;
    for (double x : r)
        ss += (x - mean) * (x - mean);
    return std::sqrt(ss / (r.size() - 1)) * std::sqrt(periods_per_year);
}

QVector<double> drawdown(const QVector<double>& close) {
    QVector<double> out(close.size(), kNaN);
    double peak = kNaN;
    for (int i = 0; i < close.size(); ++i) {
        if (!fin(close[i]))
            continue;
        if (!fin(peak) || close[i] > peak)
            peak = close[i];
        out[i] = peak > 0.0 ? close[i] / peak - 1.0 : kNaN;
    }
    return out;
}

double max_drawdown_since(const Bars& b, qint64 from_secs) {
    double peak = kNaN, mdd = 0.0;
    bool any = false;
    for (int i = 0; i < b.size(); ++i) {
        if (b.time[i] < from_secs)
            continue;
        any = true;
        if (!fin(peak) || b.close[i] > peak)
            peak = b.close[i];
        if (peak > 0.0)
            mdd = std::min(mdd, b.close[i] / peak - 1.0);
    }
    return any ? mdd : kNaN;
}

double beta_vs(const Bars& b, const Bars& bench, int window) {
    // Align by UTC calendar day.
    QHash<qint64, double> bench_close;
    for (int i = 0; i < bench.size(); ++i)
        bench_close.insert(bench.time[i] / 86400, bench.close[i]);
    QVector<double> ra, rb;
    double prev_a = kNaN, prev_b = kNaN;
    for (int i = 0; i < b.size(); ++i) {
        const auto it = bench_close.constFind(b.time[i] / 86400);
        if (it == bench_close.constEnd())
            continue;
        if (fin(prev_a) && fin(prev_b) && prev_a > 0 && prev_b > 0 && b.close[i] > 0 && it.value() > 0) {
            ra.append(std::log(b.close[i] / prev_a));
            rb.append(std::log(it.value() / prev_b));
        }
        prev_a = b.close[i];
        prev_b = it.value();
    }
    const int n = static_cast<int>(ra.size());
    const int s = window > 0 ? std::max(0, n - window) : 0;
    if (n - s < 20)
        return kNaN;
    double ma = 0, mb = 0;
    for (int i = s; i < n; ++i) {
        ma += ra[i];
        mb += rb[i];
    }
    ma /= (n - s);
    mb /= (n - s);
    double cov = 0, var = 0;
    for (int i = s; i < n; ++i) {
        cov += (ra[i] - ma) * (rb[i] - mb);
        var += (rb[i] - mb) * (rb[i] - mb);
    }
    return var > 0.0 ? cov / var : kNaN;
}

} // namespace fincept::services::equity::tmath
