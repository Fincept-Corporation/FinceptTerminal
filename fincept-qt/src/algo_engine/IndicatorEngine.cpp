// src/algo_engine/IndicatorEngine.cpp
#include "algo_engine/IndicatorEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace fincept::algo {

// ── Helpers ─────────────────────────────────────────────────────────────────

void IndicatorEngine::extract_arrays(const QVector<OhlcvCandle>& candles,
                                     QVector<double>& open, QVector<double>& high,
                                     QVector<double>& low, QVector<double>& close,
                                     QVector<double>& volume) {
    const int n = candles.size();
    open.resize(n);
    high.resize(n);
    low.resize(n);
    close.resize(n);
    volume.resize(n);
    for (int i = 0; i < n; ++i) {
        open[i] = candles[i].open;
        high[i] = candles[i].high;
        low[i] = candles[i].low;
        close[i] = candles[i].close;
        volume[i] = candles[i].volume;
    }
}

QVector<double> IndicatorEngine::sma_series(const QVector<double>& src, int period) {
    const int n = src.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (n < period) return out;
    double sum = 0;
    for (int i = 0; i < period; ++i)
        sum += src[i];
    out[period - 1] = sum / period;
    for (int i = period; i < n; ++i) {
        sum += src[i] - src[i - period];
        out[i] = sum / period;
    }
    return out;
}

QVector<double> IndicatorEngine::ema_series(const QVector<double>& src, int period) {
    const int n = src.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (n < period) return out;
    double sum = 0;
    for (int i = 0; i < period; ++i)
        sum += src[i];
    out[period - 1] = sum / period;
    double mult = 2.0 / (period + 1);
    for (int i = period; i < n; ++i)
        out[i] = (src[i] - out[i - 1]) * mult + out[i - 1];
    return out;
}

QVector<double> IndicatorEngine::true_range_series(const QVector<double>& high,
                                                    const QVector<double>& low,
                                                    const QVector<double>& close) {
    const int n = high.size();
    QVector<double> tr(n, 0);
    if (n > 0) tr[0] = high[0] - low[0];
    for (int i = 1; i < n; ++i) {
        double hl = high[i] - low[i];
        double hc = std::abs(high[i] - close[i - 1]);
        double lc = std::abs(low[i] - close[i - 1]);
        tr[i] = std::max({hl, hc, lc});
    }
    return tr;
}

static IndicatorResult make_result(double curr, double prev) {
    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("value")] = curr;
    r.previous[QStringLiteral("value")] = prev;
    return r;
}

static IndicatorResult make_error(const QString& msg) {
    IndicatorResult r;
    r.error = msg;
    return r;
}

// ── Dispatch ────────────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute(const QString& name,
                                         const QVector<OhlcvCandle>& candles,
                                         const QJsonObject& params,
                                         const QString& /*field*/) {
    if (candles.size() < 2)
        return make_error(QStringLiteral("Insufficient data"));

    // Stock attributes
    if (name == "CLOSE" || name == "OPEN" || name == "HIGH" ||
        name == "LOW" || name == "VOLUME" || name == "VWAP")
        return compute_stock_attr(candles, name);

    QVector<double> open, high, low, close, vol;
    extract_arrays(candles, open, high, low, close, vol);

    int period = params.value("period").toInt(14);

    // Moving averages
    if (name == "SMA")  return compute_sma(close, period);
    if (name == "EMA")  return compute_ema(close, period);
    if (name == "WMA")  return compute_wma(close, period);
    if (name == "DEMA") return compute_dema(close, period);
    if (name == "TEMA") return compute_tema(close, period);

    // Momentum
    if (name == "RSI") return compute_rsi(close, period);
    if (name == "MACD") {
        int fast = params.value("fast").toInt(12);
        int slow = params.value("slow").toInt(26);
        int sig = params.value("signal").toInt(9);
        return compute_macd(close, fast, slow, sig);
    }
    if (name == "STOCHASTIC") {
        int k = params.value("k_period").toInt(14);
        int d = params.value("d_period").toInt(3);
        return compute_stochastic(high, low, close, k, d);
    }
    if (name == "CCI")       return compute_cci(high, low, close, period);
    if (name == "WILLIAMS_R") return compute_williams_r(high, low, close, period);
    if (name == "MFI")       return compute_mfi(high, low, close, vol, period);
    if (name == "ROC")       return compute_roc(close, period);

    // Trend
    if (name == "ADX") return compute_adx(high, low, close, period);
    if (name == "SUPERTREND") {
        double mult = params.value("multiplier").toDouble(3.0);
        return compute_supertrend(high, low, close, period, mult);
    }
    if (name == "AROON") return compute_aroon(high, low, period);
    if (name == "ICHIMOKU") {
        int tenkan = params.value("tenkan").toInt(9);
        int kijun = params.value("kijun").toInt(26);
        int senkou = params.value("senkou").toInt(52);
        return compute_ichimoku(high, low, tenkan, kijun, senkou);
    }

    // Volatility
    if (name == "ATR") return compute_atr(high, low, close, period);
    if (name == "BOLLINGER") {
        double sd = params.value("std_dev").toDouble(2.0);
        return compute_bollinger(close, period, sd);
    }
    if (name == "KELTNER") {
        double mult = params.value("multiplier").toDouble(1.5);
        return compute_keltner(high, low, close, period, mult);
    }
    if (name == "DONCHIAN") return compute_donchian(high, low, period);

    // Volume
    if (name == "OBV") return compute_obv(close, vol);
    if (name == "CMF") return compute_cmf(high, low, close, vol, period);
    if (name == "VOL_WIN_CHG") {
        int window = params.value("window").toInt(10);
        return compute_vol_win_chg(vol, window);
    }

    return make_error(QStringLiteral("Unknown indicator: ") + name);
}

// ── Stock attributes ────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute_stock_attr(const QVector<OhlcvCandle>& candles,
                                                     const QString& attr) {
    const auto& curr = candles.last();
    const auto& prev = candles[candles.size() - 2];
    IndicatorResult r;
    r.valid = true;

    double cv = 0, pv = 0;
    if (attr == "CLOSE")  { cv = curr.close;  pv = prev.close; }
    else if (attr == "OPEN")   { cv = curr.open;   pv = prev.open; }
    else if (attr == "HIGH")   { cv = curr.high;   pv = prev.high; }
    else if (attr == "LOW")    { cv = curr.low;    pv = prev.low; }
    else if (attr == "VOLUME") { cv = curr.volume; pv = prev.volume; }
    else if (attr == "VWAP") {
        // Cumulative VWAP over the provided window: Σ(typical·vol)/Σ(vol).
        // (Per-bar (H+L+C)/3 was just typical price, not a VWAP.)
        const int n = candles.size();
        double num = 0.0, den = 0.0;
        for (int i = 0; i < n; ++i) {
            if (i == n - 1) // snapshot the previous-bar VWAP before adding the last bar
                pv = den > 0.0 ? num / den : prev.close;
            const auto& c = candles[i];
            const double tp = (c.high + c.low + c.close) / 3.0;
            const double v = c.volume > 0.0 ? c.volume : 0.0;
            num += tp * v;
            den += v;
        }
        cv = den > 0.0 ? num / den : curr.close;
    }
    r.current[QStringLiteral("value")] = cv;
    r.previous[QStringLiteral("value")] = pv;
    return r;
}

// ── Moving Averages ─────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute_sma(const QVector<double>& src, int period) {
    auto s = sma_series(src, period);
    int n = s.size();
    if (n < 2 || std::isnan(s[n - 1]))
        return make_error(QStringLiteral("Insufficient data for SMA"));
    double prev = (n >= 2 && !std::isnan(s[n - 2])) ? s[n - 2] : s[n - 1];
    return make_result(s[n - 1], prev);
}

IndicatorResult IndicatorEngine::compute_ema(const QVector<double>& src, int period) {
    auto s = ema_series(src, period);
    int n = s.size();
    if (n < 2 || std::isnan(s[n - 1]))
        return make_error(QStringLiteral("Insufficient data for EMA"));
    double prev = (n >= 2 && !std::isnan(s[n - 2])) ? s[n - 2] : s[n - 1];
    return make_result(s[n - 1], prev);
}

IndicatorResult IndicatorEngine::compute_wma(const QVector<double>& src, int period) {
    int n = src.size();
    if (n < period)
        return make_error(QStringLiteral("Insufficient data for WMA"));
    double denom = period * (period + 1) / 2.0;
    auto calc = [&](int end) {
        double sum = 0;
        for (int i = 0; i < period; ++i)
            sum += src[end - period + 1 + i] * (i + 1);
        return sum / denom;
    };
    double curr = calc(n - 1);
    double prev = (n >= period + 1) ? calc(n - 2) : curr;
    return make_result(curr, prev);
}

IndicatorResult IndicatorEngine::compute_dema(const QVector<double>& src, int period) {
    auto e1 = ema_series(src, period);
    auto e2 = ema_series(e1, period);
    int n = e2.size();
    if (n < 1 || std::isnan(e2[n - 1]))
        return make_error(QStringLiteral("Insufficient data for DEMA"));
    double curr = 2.0 * e1[n - 1] - e2[n - 1];
    double prev = curr;
    if (n >= 2 && !std::isnan(e2[n - 2]))
        prev = 2.0 * e1[n - 2] - e2[n - 2];
    return make_result(curr, prev);
}

IndicatorResult IndicatorEngine::compute_tema(const QVector<double>& src, int period) {
    auto e1 = ema_series(src, period);
    auto e2 = ema_series(e1, period);
    auto e3 = ema_series(e2, period);
    int n = e3.size();
    if (n < 1 || std::isnan(e3[n - 1]))
        return make_error(QStringLiteral("Insufficient data for TEMA"));
    double curr = 3.0 * e1[n - 1] - 3.0 * e2[n - 1] + e3[n - 1];
    double prev = curr;
    if (n >= 2 && !std::isnan(e3[n - 2]))
        prev = 3.0 * e1[n - 2] - 3.0 * e2[n - 2] + e3[n - 2];
    return make_result(curr, prev);
}

// ── Momentum ────────────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute_rsi(const QVector<double>& close, int period) {
    int n = close.size();
    if (n < period + 1)
        return make_error(QStringLiteral("Insufficient data for RSI"));

    QVector<double> gains(n, 0), losses(n, 0);
    for (int i = 1; i < n; ++i) {
        double diff = close[i] - close[i - 1];
        if (diff > 0) gains[i] = diff;
        else losses[i] = -diff;
    }

    double avg_gain = 0, avg_loss = 0;
    for (int i = 1; i <= period; ++i) {
        avg_gain += gains[i];
        avg_loss += losses[i];
    }
    avg_gain /= period;
    avg_loss /= period;

    QVector<double> rsi_vals(n, std::numeric_limits<double>::quiet_NaN());
    auto calc_rsi = [](double ag, double al) -> double {
        if (al < 1e-10) return 100.0;
        return 100.0 - 100.0 / (1.0 + ag / al);
    };
    rsi_vals[period] = calc_rsi(avg_gain, avg_loss);

    for (int i = period + 1; i < n; ++i) {
        avg_gain = (avg_gain * (period - 1) + gains[i]) / period;
        avg_loss = (avg_loss * (period - 1) + losses[i]) / period;
        rsi_vals[i] = calc_rsi(avg_gain, avg_loss);
    }

    double prev = (n >= 2 && !std::isnan(rsi_vals[n - 2])) ? rsi_vals[n - 2] : rsi_vals[n - 1];
    return make_result(rsi_vals[n - 1], prev);
}

IndicatorResult IndicatorEngine::compute_macd(const QVector<double>& close,
                                               int fast, int slow, int signal_period) {
    auto ema_fast = ema_series(close, fast);
    auto ema_slow = ema_series(close, slow);
    int n = close.size();

    QVector<double> macd_line(n, std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < n; ++i) {
        if (!std::isnan(ema_fast[i]) && !std::isnan(ema_slow[i]))
            macd_line[i] = ema_fast[i] - ema_slow[i];
    }

    auto signal_line = ema_series(macd_line, signal_period);

    if (std::isnan(macd_line[n - 1]) || std::isnan(signal_line[n - 1]))
        return make_error(QStringLiteral("Insufficient data for MACD"));

    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("line")] = macd_line[n - 1];
    r.current[QStringLiteral("signal_line")] = signal_line[n - 1];
    r.current[QStringLiteral("histogram")] = macd_line[n - 1] - signal_line[n - 1];
    if (n >= 2 && !std::isnan(macd_line[n - 2]) && !std::isnan(signal_line[n - 2])) {
        r.previous[QStringLiteral("line")] = macd_line[n - 2];
        r.previous[QStringLiteral("signal_line")] = signal_line[n - 2];
        r.previous[QStringLiteral("histogram")] = macd_line[n - 2] - signal_line[n - 2];
    }
    return r;
}

IndicatorResult IndicatorEngine::compute_stochastic(const QVector<double>& high,
                                                     const QVector<double>& low,
                                                     const QVector<double>& close,
                                                     int k_period, int d_period) {
    int n = close.size();
    if (n < k_period)
        return make_error(QStringLiteral("Insufficient data for Stochastic"));

    QVector<double> k_vals(n, std::numeric_limits<double>::quiet_NaN());
    for (int i = k_period - 1; i < n; ++i) {
        double hh = high[i], ll = low[i];
        for (int j = 1; j < k_period; ++j) {
            hh = std::max(hh, high[i - j]);
            ll = std::min(ll, low[i - j]);
        }
        double range = hh - ll;
        k_vals[i] = range > 1e-10 ? (close[i] - ll) / range * 100.0 : 50.0;
    }

    auto d_vals = sma_series(k_vals, d_period);

    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("k")] = k_vals[n - 1];
    r.current[QStringLiteral("d")] = std::isnan(d_vals[n - 1]) ? k_vals[n - 1] : d_vals[n - 1];
    if (n >= 2 && !std::isnan(k_vals[n - 2])) {
        r.previous[QStringLiteral("k")] = k_vals[n - 2];
        r.previous[QStringLiteral("d")] = std::isnan(d_vals[n - 2]) ? k_vals[n - 2] : d_vals[n - 2];
    }
    return r;
}

IndicatorResult IndicatorEngine::compute_cci(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close, int period) {
    int n = close.size();
    if (n < period)
        return make_error(QStringLiteral("Insufficient data for CCI"));

    QVector<double> tp(n);
    for (int i = 0; i < n; ++i)
        tp[i] = (high[i] + low[i] + close[i]) / 3.0;

    auto sma = sma_series(tp, period);

    auto calc_cci = [&](int idx) -> double {
        if (std::isnan(sma[idx])) return 0;
        double mean_dev = 0;
        for (int j = 0; j < period; ++j)
            mean_dev += std::abs(tp[idx - j] - sma[idx]);
        mean_dev /= period;
        return mean_dev > 1e-10 ? (tp[idx] - sma[idx]) / (0.015 * mean_dev) : 0;
    };

    double curr = calc_cci(n - 1);
    double prev = (n >= period + 1) ? calc_cci(n - 2) : curr;
    return make_result(curr, prev);
}

IndicatorResult IndicatorEngine::compute_williams_r(const QVector<double>& high,
                                                     const QVector<double>& low,
                                                     const QVector<double>& close, int period) {
    int n = close.size();
    if (n < period)
        return make_error(QStringLiteral("Insufficient data for Williams %R"));

    auto calc = [&](int idx) -> double {
        double hh = high[idx], ll = low[idx];
        for (int j = 1; j < period; ++j) {
            hh = std::max(hh, high[idx - j]);
            ll = std::min(ll, low[idx - j]);
        }
        double range = hh - ll;
        return range > 1e-10 ? (hh - close[idx]) / range * -100.0 : -50.0;
    };

    double curr = calc(n - 1);
    double prev = (n >= period + 1) ? calc(n - 2) : curr;
    return make_result(curr, prev);
}

IndicatorResult IndicatorEngine::compute_mfi(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close,
                                              const QVector<double>& volume, int period) {
    int n = close.size();
    if (n < period + 1)
        return make_error(QStringLiteral("Insufficient data for MFI"));

    QVector<double> tp(n);
    for (int i = 0; i < n; ++i)
        tp[i] = (high[i] + low[i] + close[i]) / 3.0;

    auto calc = [&](int end) -> double {
        double pos = 0, neg = 0;
        for (int i = end - period + 1; i <= end; ++i) {
            double mf = tp[i] * volume[i];
            if (tp[i] > tp[i - 1]) pos += mf;
            else if (tp[i] < tp[i - 1]) neg += mf;
        }
        return neg > 1e-10 ? 100.0 - 100.0 / (1.0 + pos / neg) : 100.0;
    };

    double curr = calc(n - 1);
    double prev = (n >= period + 2) ? calc(n - 2) : curr;
    return make_result(curr, prev);
}

IndicatorResult IndicatorEngine::compute_roc(const QVector<double>& close, int period) {
    int n = close.size();
    if (n <= period)
        return make_error(QStringLiteral("Insufficient data for ROC"));
    double curr = (close[n - 1] - close[n - 1 - period]) / close[n - 1 - period] * 100.0;
    double prev = curr;
    if (n > period + 1)
        prev = (close[n - 2] - close[n - 2 - period]) / close[n - 2 - period] * 100.0;
    return make_result(curr, prev);
}

// ── Trend ───────────────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute_adx(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close, int period) {
    int n = close.size();
    if (n < period * 2 + 1)
        return make_error(QStringLiteral("Insufficient data for ADX"));

    auto tr = true_range_series(high, low, close);
    QVector<double> plus_dm(n, 0), minus_dm(n, 0);
    for (int i = 1; i < n; ++i) {
        double up = high[i] - high[i - 1];
        double down = low[i - 1] - low[i];
        if (up > down && up > 0) plus_dm[i] = up;
        if (down > up && down > 0) minus_dm[i] = down;
    }

    // Wilder's smoothing
    double atr_smooth = 0, plus_smooth = 0, minus_smooth = 0;
    for (int i = 1; i <= period; ++i) {
        atr_smooth += tr[i];
        plus_smooth += plus_dm[i];
        minus_smooth += minus_dm[i];
    }

    QVector<double> dx_vals;
    auto calc_dx = [&]() -> double {
        double pdi = atr_smooth > 1e-10 ? plus_smooth / atr_smooth * 100.0 : 0;
        double mdi = atr_smooth > 1e-10 ? minus_smooth / atr_smooth * 100.0 : 0;
        double sum = pdi + mdi;
        return sum > 1e-10 ? std::abs(pdi - mdi) / sum * 100.0 : 0;
    };

    dx_vals.append(calc_dx());
    for (int i = period + 1; i < n; ++i) {
        atr_smooth = atr_smooth - atr_smooth / period + tr[i];
        plus_smooth = plus_smooth - plus_smooth / period + plus_dm[i];
        minus_smooth = minus_smooth - minus_smooth / period + minus_dm[i];
        dx_vals.append(calc_dx());
    }

    if (dx_vals.size() < period)
        return make_error(QStringLiteral("Insufficient data for ADX"));

    double adx = 0;
    for (int i = 0; i < period; ++i)
        adx += dx_vals[i];
    adx /= period;

    for (int i = period; i < dx_vals.size(); ++i)
        adx = (adx * (period - 1) + dx_vals[i]) / period;

    double prev_adx = adx;
    if (dx_vals.size() > period) {
        double adx2 = 0;
        for (int i = 0; i < period; ++i)
            adx2 += dx_vals[i];
        adx2 /= period;
        for (int i = period; i < dx_vals.size() - 1; ++i)
            adx2 = (adx2 * (period - 1) + dx_vals[i]) / period;
        prev_adx = adx2;
    }

    IndicatorResult r;
    r.valid = true;
    double pdi_curr = atr_smooth > 1e-10 ? plus_smooth / atr_smooth * 100.0 : 0;
    double mdi_curr = atr_smooth > 1e-10 ? minus_smooth / atr_smooth * 100.0 : 0;
    r.current[QStringLiteral("value")] = adx;
    r.current[QStringLiteral("plus_di")] = pdi_curr;
    r.current[QStringLiteral("minus_di")] = mdi_curr;
    r.previous[QStringLiteral("value")] = prev_adx;
    r.previous[QStringLiteral("plus_di")] = pdi_curr;
    r.previous[QStringLiteral("minus_di")] = mdi_curr;
    return r;
}

IndicatorResult IndicatorEngine::compute_supertrend(const QVector<double>& high,
                                                     const QVector<double>& low,
                                                     const QVector<double>& close,
                                                     int period, double multiplier) {
    int n = close.size();
    auto atr_s = true_range_series(high, low, close);
    auto atr_ema = ema_series(atr_s, period);

    if (n < period + 1 || std::isnan(atr_ema[n - 1]))
        return make_error(QStringLiteral("Insufficient data for SuperTrend"));

    QVector<double> upper(n), lower(n), st(n);
    QVector<int> dir(n, 1);

    int start = period - 1;
    for (int i = start; i < n; ++i) {
        if (std::isnan(atr_ema[i])) continue;
        double hl2 = (high[i] + low[i]) / 2.0;
        upper[i] = hl2 + multiplier * atr_ema[i];
        lower[i] = hl2 - multiplier * atr_ema[i];
    }

    for (int i = start + 1; i < n; ++i) {
        if (close[i - 1] > upper[i - 1]) lower[i] = std::max(lower[i], lower[i - 1]);
        if (close[i - 1] < lower[i - 1]) upper[i] = std::min(upper[i], upper[i - 1]);

        if (close[i] > upper[i])      dir[i] = 1;
        else if (close[i] < lower[i]) dir[i] = -1;
        else                           dir[i] = dir[i - 1];

        st[i] = dir[i] == 1 ? lower[i] : upper[i];
    }

    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("value")] = st[n - 1];
    r.current[QStringLiteral("direction")] = dir[n - 1];
    if (n >= 2) {
        r.previous[QStringLiteral("value")] = st[n - 2];
        r.previous[QStringLiteral("direction")] = dir[n - 2];
    }
    return r;
}

IndicatorResult IndicatorEngine::compute_aroon(const QVector<double>& high,
                                                const QVector<double>& low, int period) {
    int n = high.size();
    if (n < period + 1)
        return make_error(QStringLiteral("Insufficient data for Aroon"));

    auto calc = [&](int end) -> std::pair<double, double> {
        int hi_idx = 0, lo_idx = 0;
        double hh = high[end], ll = low[end];
        for (int j = 1; j <= period; ++j) {
            if (high[end - j] > hh) { hh = high[end - j]; hi_idx = j; }
            if (low[end - j] < ll) { ll = low[end - j]; lo_idx = j; }
        }
        double up = (static_cast<double>(period - hi_idx) / period) * 100.0;
        double down = (static_cast<double>(period - lo_idx) / period) * 100.0;
        return {up, down};
    };

    auto [up_c, dn_c] = calc(n - 1);
    auto [up_p, dn_p] = (n >= period + 2) ? calc(n - 2) : std::pair{up_c, dn_c};

    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("up")] = up_c;
    r.current[QStringLiteral("down")] = dn_c;
    r.previous[QStringLiteral("up")] = up_p;
    r.previous[QStringLiteral("down")] = dn_p;
    return r;
}

IndicatorResult IndicatorEngine::compute_ichimoku(const QVector<double>& high,
                                                   const QVector<double>& low,
                                                   int tenkan, int kijun, int senkou) {
    int n = high.size();
    if (n < senkou)
        return make_error(QStringLiteral("Insufficient data for Ichimoku"));

    auto midpoint = [&](int end, int period) -> double {
        double hh = high[end], ll = low[end];
        for (int j = 1; j < period; ++j) {
            hh = std::max(hh, high[end - j]);
            ll = std::min(ll, low[end - j]);
        }
        return (hh + ll) / 2.0;
    };

    int i = n - 1;
    double ts = midpoint(i, tenkan);
    double ks = midpoint(i, kijun);
    double sa = (ts + ks) / 2.0;
    double sb = midpoint(i, senkou);

    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("tenkan_sen")] = ts;
    r.current[QStringLiteral("kijun_sen")] = ks;
    r.current[QStringLiteral("senkou_a")] = sa;
    r.current[QStringLiteral("senkou_b")] = sb;

    if (n > senkou) {
        int p = n - 2;
        r.previous[QStringLiteral("tenkan_sen")] = midpoint(p, tenkan);
        r.previous[QStringLiteral("kijun_sen")] = midpoint(p, kijun);
        double pts = midpoint(p, tenkan), pks = midpoint(p, kijun);
        r.previous[QStringLiteral("senkou_a")] = (pts + pks) / 2.0;
        r.previous[QStringLiteral("senkou_b")] = midpoint(p, senkou);
    }
    return r;
}

// ── Volatility ──────────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute_atr(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close, int period) {
    auto tr = true_range_series(high, low, close);
    int n = tr.size();
    if (n < period + 1)
        return make_error(QStringLiteral("Insufficient data for ATR"));

    double atr = 0;
    for (int i = 0; i < period; ++i)
        atr += tr[i + 1];
    atr /= period;

    for (int i = period + 1; i < n; ++i)
        atr = (atr * (period - 1) + tr[i]) / period;

    double prev_atr = atr;
    if (n > period + 1) {
        double a2 = 0;
        for (int i = 0; i < period; ++i)
            a2 += tr[i + 1];
        a2 /= period;
        for (int i = period + 1; i < n - 1; ++i)
            a2 = (a2 * (period - 1) + tr[i]) / period;
        prev_atr = a2;
    }
    return make_result(atr, prev_atr);
}

IndicatorResult IndicatorEngine::compute_bollinger(const QVector<double>& close,
                                                    int period, double std_dev_mult) {
    int n = close.size();
    if (n < period)
        return make_error(QStringLiteral("Insufficient data for Bollinger"));

    auto sma = sma_series(close, period);

    auto calc = [&](int idx) -> std::tuple<double, double, double, double, double> {
        double mid = sma[idx];
        double sum_sq = 0;
        for (int j = 0; j < period; ++j) {
            double diff = close[idx - j] - mid;
            sum_sq += diff * diff;
        }
        double sd = std::sqrt(sum_sq / period);
        double upper = mid + std_dev_mult * sd;
        double lower = mid - std_dev_mult * sd;
        double width = mid > 1e-10 ? (upper - lower) / mid : 0;
        double pct_b = (upper - lower) > 1e-10 ? (close[idx] - lower) / (upper - lower) : 0.5;
        return {upper, mid, lower, width, pct_b};
    };

    auto [u, m, l, w, pb] = calc(n - 1);
    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("upper")] = u;
    r.current[QStringLiteral("middle")] = m;
    r.current[QStringLiteral("lower")] = l;
    r.current[QStringLiteral("width")] = w;
    r.current[QStringLiteral("pct_b")] = pb;

    if (n >= period + 1) {
        auto [u2, m2, l2, w2, pb2] = calc(n - 2);
        r.previous[QStringLiteral("upper")] = u2;
        r.previous[QStringLiteral("middle")] = m2;
        r.previous[QStringLiteral("lower")] = l2;
        r.previous[QStringLiteral("width")] = w2;
        r.previous[QStringLiteral("pct_b")] = pb2;
    }
    return r;
}

IndicatorResult IndicatorEngine::compute_keltner(const QVector<double>& high,
                                                  const QVector<double>& low,
                                                  const QVector<double>& close,
                                                  int period, double multiplier) {
    auto ema = ema_series(close, period);
    auto tr = true_range_series(high, low, close);
    auto atr = ema_series(tr, period);
    int n = close.size();

    if (n < 1 || std::isnan(ema[n - 1]) || std::isnan(atr[n - 1]))
        return make_error(QStringLiteral("Insufficient data for Keltner"));

    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("upper")] = ema[n - 1] + multiplier * atr[n - 1];
    r.current[QStringLiteral("middle")] = ema[n - 1];
    r.current[QStringLiteral("lower")] = ema[n - 1] - multiplier * atr[n - 1];
    if (n >= 2 && !std::isnan(ema[n - 2]) && !std::isnan(atr[n - 2])) {
        r.previous[QStringLiteral("upper")] = ema[n - 2] + multiplier * atr[n - 2];
        r.previous[QStringLiteral("middle")] = ema[n - 2];
        r.previous[QStringLiteral("lower")] = ema[n - 2] - multiplier * atr[n - 2];
    }
    return r;
}

IndicatorResult IndicatorEngine::compute_donchian(const QVector<double>& high,
                                                   const QVector<double>& low, int period) {
    int n = high.size();
    if (n < period)
        return make_error(QStringLiteral("Insufficient data for Donchian"));

    auto calc = [&](int end) -> std::pair<double, double> {
        double hh = high[end], ll = low[end];
        for (int j = 1; j < period; ++j) {
            hh = std::max(hh, high[end - j]);
            ll = std::min(ll, low[end - j]);
        }
        return {hh, ll};
    };

    auto [uc, lc] = calc(n - 1);
    IndicatorResult r;
    r.valid = true;
    r.current[QStringLiteral("upper")] = uc;
    r.current[QStringLiteral("lower")] = lc;
    if (n >= period + 1) {
        auto [up, lp] = calc(n - 2);
        r.previous[QStringLiteral("upper")] = up;
        r.previous[QStringLiteral("lower")] = lp;
    }
    return r;
}

// ── Volume ──────────────────────────────────────────────────────────────────

IndicatorResult IndicatorEngine::compute_obv(const QVector<double>& close,
                                              const QVector<double>& volume) {
    int n = close.size();
    if (n < 2)
        return make_error(QStringLiteral("Insufficient data for OBV"));

    double obv = 0, prev_obv = 0;
    for (int i = 1; i < n; ++i) {
        if (i == n - 1) prev_obv = obv;
        if (close[i] > close[i - 1]) obv += volume[i];
        else if (close[i] < close[i - 1]) obv -= volume[i];
    }
    return make_result(obv, prev_obv);
}

IndicatorResult IndicatorEngine::compute_vol_win_chg(const QVector<double>& volume, int window) {
    if (window < 1)
        return make_error(QStringLiteral("VOL_WIN_CHG: window must be >= 1"));
    const int n = volume.size();
    if (n < 2 * window)
        return make_error(QStringLiteral("Insufficient data for VOL_WIN_CHG (need %1 bars)")
                              .arg(2 * window));
    // Compute the windowed %-change at the current bar and at the previous bar so
    // crossing/rising operators (which re-read the prior sample) stay consistent.
    auto win_pct = [&](int last_index) -> double {
        double sum_last = 0, sum_prev = 0;
        for (int i = last_index - window + 1; i <= last_index; ++i)
            sum_last += volume[i];
        for (int i = last_index - 2 * window + 1; i <= last_index - window; ++i)
            sum_prev += volume[i];
        if (sum_prev <= 0.0)
            return std::numeric_limits<double>::quiet_NaN();
        return (sum_last / sum_prev - 1.0) * 100.0;
    };
    double curr = win_pct(n - 1);
    double prev = (n >= 2 * window + 1) ? win_pct(n - 2)
                                        : std::numeric_limits<double>::quiet_NaN();
    return make_result(curr, prev);
}

IndicatorResult IndicatorEngine::compute_cmf(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close,
                                              const QVector<double>& volume, int period) {
    int n = close.size();
    if (n < period)
        return make_error(QStringLiteral("Insufficient data for CMF"));

    auto calc = [&](int end) -> double {
        double mfv_sum = 0, vol_sum = 0;
        for (int j = 0; j < period; ++j) {
            int idx = end - j;
            double hl = high[idx] - low[idx];
            double mf_mult = hl > 1e-10 ? ((close[idx] - low[idx]) - (high[idx] - close[idx])) / hl : 0;
            mfv_sum += mf_mult * volume[idx];
            vol_sum += volume[idx];
        }
        return vol_sum > 1e-10 ? mfv_sum / vol_sum : 0;
    };

    double curr = calc(n - 1);
    double prev = (n >= period + 1) ? calc(n - 2) : curr;
    return make_result(curr, prev);
}

} // namespace fincept::algo
