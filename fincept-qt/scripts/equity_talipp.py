"""
equity_talipp.py — TALIpp technical indicator computation for Equity Research tab

Usage:
    python equity_talipp.py <indicator_name> <ohlcv_json> [params_json]

Arguments:
    indicator_name  One of: SMA, EMA, WMA, DEMA, TEMA, TRIMA, KAMA, T3,
                            RSI, MACD, STOCH, STOCHRSI, CCI, ROC, MOM, WILLR, MFI,
                            ATR, BBANDS, NATR, STDDEV, OBV, VWAP, AD,
                            AROON, ADX, DX, SAR, BOP, TRIX, TSI
    ohlcv_json      JSON array of OHLCV objects with keys:
                    timestamp, open, high, low, close, volume
    params_json     Optional JSON object of parameters, e.g.:
                    {"period": 14} or {"fast": 12, "slow": 26, "signal": 9}

Output (stdout):
    JSON object: {"indicator": str, "values": [float], "timestamps": [int], "success": bool}
"""

import sys
import json
import math


def safe_float(v):
    """Convert value to float, return None if NaN/Inf."""
    try:
        f = float(v)
        return None if (math.isnan(f) or math.isinf(f)) else f
    except (TypeError, ValueError):
        return None


def sma(closes, period):
    """Simple Moving Average."""
    result = []
    for i in range(len(closes)):
        if i < period - 1:
            result.append(None)
        else:
            window = closes[i - period + 1:i + 1]
            result.append(sum(window) / period)
    return result


def ema(closes, period, smoothing=2.0):
    """Exponential Moving Average."""
    k = smoothing / (period + 1)
    result = []
    prev = None
    for i, c in enumerate(closes):
        if prev is None:
            if i < period - 1:
                result.append(None)
                continue
            # Seed with SMA of first `period` values
            prev = sum(closes[:period]) / period
            result.append(prev)
        else:
            prev = c * k + prev * (1 - k)
            result.append(prev)
    return result


def wma(closes, period):
    """Weighted Moving Average."""
    result = []
    denom = period * (period + 1) / 2
    for i in range(len(closes)):
        if i < period - 1:
            result.append(None)
        else:
            w = sum((j + 1) * closes[i - period + 1 + j] for j in range(period))
            result.append(w / denom)
    return result


def dema(closes, period):
    e1 = ema(closes, period)
    e1_clean = [v if v is not None else 0.0 for v in e1]
    e2 = ema(e1_clean, period)
    return [
        (2 * v1 - v2) if (v1 is not None and v2 is not None) else None
        for v1, v2 in zip(e1, e2)
    ]


def tema(closes, period):
    e1 = ema(closes, period)
    e1_c = [v if v is not None else 0.0 for v in e1]
    e2 = ema(e1_c, period)
    e2_c = [v if v is not None else 0.0 for v in e2]
    e3 = ema(e2_c, period)
    return [
        (3 * v1 - 3 * v2 + v3) if (v1 is not None and v2 is not None and v3 is not None) else None
        for v1, v2, v3 in zip(e1, e2, e3)
    ]


def trima(closes, period):
    """Triangular Moving Average (SMA of SMA)."""
    s1 = sma(closes, period)
    s1_c = [v if v is not None else 0.0 for v in s1]
    return sma(s1_c, period)


def kama(closes, period=10, fast=2, slow=30):
    """Kaufman's Adaptive Moving Average."""
    fast_k = 2.0 / (fast + 1)
    slow_k = 2.0 / (slow + 1)
    result = [None] * len(closes)
    if len(closes) <= period:
        return result
    result[period - 1] = closes[period - 1]
    for i in range(period, len(closes)):
        direction = abs(closes[i] - closes[i - period])
        volatility = sum(abs(closes[j] - closes[j - 1]) for j in range(i - period + 1, i + 1))
        er = direction / volatility if volatility != 0 else 0
        sc = (er * (fast_k - slow_k) + slow_k) ** 2
        prev = result[i - 1]
        result[i] = prev + sc * (closes[i] - prev)
    return result


def t3(closes, period=5, vfactor=0.7):
    """Triple Exponential Moving Average (T3) by Tillson."""
    c1 = -(vfactor ** 3)
    c2 = 3 * vfactor ** 2 + 3 * vfactor ** 3
    c3 = -6 * vfactor ** 2 - 3 * vfactor - 3 * vfactor ** 3
    c4 = 1 + 3 * vfactor + vfactor ** 3 + 3 * vfactor ** 2
    e1 = ema(closes, period)
    e1_c = [v or 0.0 for v in e1]
    e2 = ema(e1_c, period)
    e2_c = [v or 0.0 for v in e2]
    e3 = ema(e2_c, period)
    e3_c = [v or 0.0 for v in e3]
    e4 = ema(e3_c, period)
    e4_c = [v or 0.0 for v in e4]
    e5 = ema(e4_c, period)
    e5_c = [v or 0.0 for v in e5]
    e6 = ema(e5_c, period)
    return [
        (c1 * v6 + c2 * v5 + c3 * v4 + c4 * v3)
        if all(v is not None for v in [v3, v4, v5, v6]) else None
        for v3, v4, v5, v6 in zip(e3, e4, e5, e6)
    ]


def rsi(closes, period=14):
    result = [None] * len(closes)
    if len(closes) < period + 1:
        return result
    gains = [max(closes[i] - closes[i - 1], 0) for i in range(1, len(closes))]
    losses = [max(closes[i - 1] - closes[i], 0) for i in range(1, len(closes))]
    avg_gain = sum(gains[:period]) / period
    avg_loss = sum(losses[:period]) / period
    result[period] = 100 - (100 / (1 + avg_gain / avg_loss)) if avg_loss != 0 else 100
    for i in range(period + 1, len(closes)):
        avg_gain = (avg_gain * (period - 1) + gains[i - 1]) / period
        avg_loss = (avg_loss * (period - 1) + losses[i - 1]) / period
        rs = avg_gain / avg_loss if avg_loss != 0 else 0
        result[i] = 100 - (100 / (1 + rs))
    return result


def macd_calc(closes, fast=12, slow=26, signal=9):
    e_fast = ema(closes, fast)
    e_slow = ema(closes, slow)
    macd_line = [
        (f - s) if (f is not None and s is not None) else None
        for f, s in zip(e_fast, e_slow)
    ]
    macd_clean = [v or 0.0 for v in macd_line]
    sig_line = ema(macd_clean, signal)
    hist = [
        (m - s) if (m is not None and s is not None) else None
        for m, s in zip(macd_line, sig_line)
    ]
    return macd_line, sig_line, hist


def stoch(highs, lows, closes, k_period=14, d_period=3):
    k_vals = []
    for i in range(len(closes)):
        if i < k_period - 1:
            k_vals.append(None)
        else:
            window_h = highs[i - k_period + 1:i + 1]
            window_l = lows[i - k_period + 1:i + 1]
            highest = max(window_h)
            lowest  = min(window_l)
            denom   = highest - lowest
            k_vals.append((closes[i] - lowest) / denom * 100 if denom != 0 else 50)
    k_clean = [v or 50.0 for v in k_vals]
    d_vals = sma(k_clean, d_period)
    return k_vals, d_vals


def cci_calc(highs, lows, closes, period=20):
    result = []
    for i in range(len(closes)):
        if i < period - 1:
            result.append(None)
        else:
            tp = [(highs[j] + lows[j] + closes[j]) / 3 for j in range(i - period + 1, i + 1)]
            mean_tp = sum(tp) / period
            mean_dev = sum(abs(x - mean_tp) for x in tp) / period
            result.append((tp[-1] - mean_tp) / (0.015 * mean_dev) if mean_dev != 0 else 0)
    return result


def roc(closes, period=10):
    result = [None] * period
    for i in range(period, len(closes)):
        prev = closes[i - period]
        result.append((closes[i] - prev) / prev * 100 if prev != 0 else 0)
    return result


def mom(closes, period=14):
    result = [None] * period
    for i in range(period, len(closes)):
        result.append(closes[i] - closes[i - period])
    return result


def willr(highs, lows, closes, period=14):
    result = []
    for i in range(len(closes)):
        if i < period - 1:
            result.append(None)
        else:
            hh = max(highs[i - period + 1:i + 1])
            ll = min(lows[i - period + 1:i + 1])
            denom = hh - ll
            result.append((hh - closes[i]) / denom * -100 if denom != 0 else -50)
    return result


def mfi(highs, lows, closes, volumes, period=14):
    typical = [(h + l + c) / 3 for h, l, c in zip(highs, lows, closes)]
    result = [None] * period
    for i in range(period, len(closes)):
        pos_flow = sum(
            typical[j] * volumes[j]
            for j in range(i - period + 1, i + 1)
            if typical[j] >= typical[j - 1]
        )
        neg_flow = sum(
            typical[j] * volumes[j]
            for j in range(i - period + 1, i + 1)
            if typical[j] < typical[j - 1]
        )
        mfr = pos_flow / neg_flow if neg_flow != 0 else 0
        result.append(100 - (100 / (1 + mfr)))
    return result


def atr(highs, lows, closes, period=14):
    tr = [highs[0] - lows[0]]
    for i in range(1, len(closes)):
        tr.append(max(highs[i] - lows[i],
                      abs(highs[i] - closes[i - 1]),
                      abs(lows[i]  - closes[i - 1])))
    result = [None] * (period - 1)
    atr_val = sum(tr[:period]) / period
    result.append(atr_val)
    for i in range(period, len(tr)):
        atr_val = (atr_val * (period - 1) + tr[i]) / period
        result.append(atr_val)
    return result


def bbands(closes, period=20, stddev=2.0):
    mid = sma(closes, period)
    upper, lower = [], []
    for i in range(len(closes)):
        if mid[i] is None:
            upper.append(None)
            lower.append(None)
        else:
            window = closes[i - period + 1:i + 1]
            mean   = mid[i]
            sd     = (sum((x - mean) ** 2 for x in window) / period) ** 0.5
            upper.append(mean + stddev * sd)
            lower.append(mean - stddev * sd)
    return upper, mid, lower


def natr(highs, lows, closes, period=14):
    atr_vals = atr(highs, lows, closes, period)
    return [
        (a / c * 100) if (a is not None and c and c != 0) else None
        for a, c in zip(atr_vals, closes)
    ]


def stddev(closes, period=20):
    result = []
    for i in range(len(closes)):
        if i < period - 1:
            result.append(None)
        else:
            window = closes[i - period + 1:i + 1]
            mean   = sum(window) / period
            result.append((sum((x - mean) ** 2 for x in window) / period) ** 0.5)
    return result


def obv(closes, volumes):
    result = [volumes[0]]
    for i in range(1, len(closes)):
        if closes[i] > closes[i - 1]:
            result.append(result[-1] + volumes[i])
        elif closes[i] < closes[i - 1]:
            result.append(result[-1] - volumes[i])
        else:
            result.append(result[-1])
    return result


def vwap(highs, lows, closes, volumes):
    """Daily VWAP (cumulative, resets would need session info — use cumulative here)."""
    typ = [(h + l + c) / 3 for h, l, c in zip(highs, lows, closes)]
    cum_tp_vol = 0.0
    cum_vol    = 0.0
    result     = []
    for tp, v in zip(typ, volumes):
        cum_tp_vol += tp * v
        cum_vol    += v
        result.append(cum_tp_vol / cum_vol if cum_vol != 0 else tp)
    return result


def ad(highs, lows, closes, volumes):
    """Accumulation/Distribution Line."""
    result = []
    cumulative = 0.0
    for h, l, c, v in zip(highs, lows, closes, volumes):
        denom = h - l
        clv   = ((c - l) - (h - c)) / denom if denom != 0 else 0
        cumulative += clv * v
        result.append(cumulative)
    return result


def aroon(highs, lows, period=14):
    up, down = [], []
    for i in range(len(highs)):
        if i < period:
            up.append(None)
            down.append(None)
        else:
            window_h = highs[i - period:i + 1]
            window_l = lows[i - period:i + 1]
            bars_since_high = period - window_h.index(max(window_h))
            bars_since_low  = period - window_l.index(min(window_l))
            up.append((period - bars_since_high) / period * 100)
            down.append((period - bars_since_low)  / period * 100)
    return up, down


def adx_calc(highs, lows, closes, period=14):
    tr_vals = [max(highs[0] - lows[0], 0)]
    plus_dm = [0.0]
    minus_dm = [0.0]
    for i in range(1, len(closes)):
        tr_vals.append(max(highs[i] - lows[i],
                           abs(highs[i] - closes[i - 1]),
                           abs(lows[i]  - closes[i - 1])))
        pdm = highs[i] - highs[i - 1]
        ndm = lows[i - 1] - lows[i]
        plus_dm.append(max(pdm, 0) if pdm > ndm else 0)
        minus_dm.append(max(ndm, 0) if ndm > pdm else 0)

    def smooth(vals, p):
        result = [sum(vals[:p])]
        for v in vals[p:]:
            result.append(result[-1] - result[-1] / p + v)
        return [None] * (p - 1) + result

    str_  = smooth(tr_vals, period)
    spdm  = smooth(plus_dm, period)
    sndm  = smooth(minus_dm, period)
    adx_vals = []
    for s, p, n in zip(str_, spdm, sndm):
        if s is None or s == 0:
            adx_vals.append(None)
        else:
            adx_vals.append(abs(p - n) / (p + n) * 100 if (p + n) != 0 else 0)
    return smooth([v or 0.0 for v in adx_vals], period)


def sar(highs, lows, acceleration=0.02, maximum=0.2):
    result = [None] * len(highs)
    if len(highs) < 2:
        return result
    is_bull = highs[1] > highs[0]
    af  = acceleration
    ep  = highs[0] if is_bull else lows[0]
    sar_val = lows[0] if is_bull else highs[0]
    result[0] = sar_val
    for i in range(1, len(highs)):
        sar_val = sar_val + af * (ep - sar_val)
        if is_bull:
            sar_val = min(sar_val, lows[i - 1], lows[max(i - 2, 0)])
            if lows[i] < sar_val:
                is_bull = False
                sar_val = ep
                ep = lows[i]
                af = acceleration
            else:
                if highs[i] > ep:
                    ep = highs[i]
                    af = min(af + acceleration, maximum)
        else:
            sar_val = max(sar_val, highs[i - 1], highs[max(i - 2, 0)])
            if highs[i] > sar_val:
                is_bull = True
                sar_val = ep
                ep = highs[i]
                af = acceleration
            else:
                if lows[i] < ep:
                    ep = lows[i]
                    af = min(af + acceleration, maximum)
        result[i] = sar_val
    return result


def bop_calc(opens, highs, lows, closes):
    return [
        (c - o) / (h - l) if (h - l) != 0 else 0
        for o, h, l, c in zip(opens, highs, lows, closes)
    ]


def trix(closes, period=15):
    e1 = ema(closes, period)
    e1_c = [v or 0.0 for v in e1]
    e2 = ema(e1_c, period)
    e2_c = [v or 0.0 for v in e2]
    e3 = ema(e2_c, period)
    result = [None]
    for i in range(1, len(e3)):
        if e3[i] is not None and e3[i - 1] is not None and e3[i - 1] != 0:
            result.append((e3[i] - e3[i - 1]) / e3[i - 1] * 100)
        else:
            result.append(None)
    return result


def tsi(closes, long_period=25, short_period=13):
    mom_1 = [None] + [closes[i] - closes[i - 1] for i in range(1, len(closes))]
    abs_mom = [None if v is None else abs(v) for v in mom_1]
    mom_c   = [v or 0.0 for v in mom_1]
    abs_c   = [v or 0.0 for v in abs_mom]
    e1  = ema(mom_c, long_period)
    ea1 = ema(abs_c, long_period)
    e1_c  = [v or 0.0 for v in e1]
    ea1_c = [v or 0.0 for v in ea1]
    e2  = ema(e1_c,  short_period)
    ea2 = ema(ea1_c, short_period)
    return [
        (v / a * 100) if (a is not None and a != 0 and v is not None) else None
        for v, a in zip(e2, ea2)
    ]


DISPATCH = {
    "SMA":      lambda o, h, l, c, v, p: sma(c, int(p.get("period", 20))),
    "EMA":      lambda o, h, l, c, v, p: ema(c, int(p.get("period", 20))),
    "WMA":      lambda o, h, l, c, v, p: wma(c, int(p.get("period", 20))),
    "DEMA":     lambda o, h, l, c, v, p: dema(c, int(p.get("period", 20))),
    "TEMA":     lambda o, h, l, c, v, p: tema(c, int(p.get("period", 20))),
    "TRIMA":    lambda o, h, l, c, v, p: trima(c, int(p.get("period", 20))),
    "KAMA":     lambda o, h, l, c, v, p: kama(c, int(p.get("period", 10))),
    "T3":       lambda o, h, l, c, v, p: t3(c, int(p.get("period", 5)), float(p.get("vfactor", 0.7))),
    "RSI":      lambda o, h, l, c, v, p: rsi(c, int(p.get("period", 14))),
    "MACD":     lambda o, h, l, c, v, p: macd_calc(c, int(p.get("fast", 12)), int(p.get("slow", 26)), int(p.get("signal", 9)))[0],
    "STOCH":    lambda o, h, l, c, v, p: stoch(h, l, c, int(p.get("k_period", 14)), int(p.get("d_period", 3)))[0],
    "STOCHRSI": lambda o, h, l, c, v, p: rsi(rsi(c, int(p.get("period", 14))), int(p.get("period", 14))),
    "CCI":      lambda o, h, l, c, v, p: cci_calc(h, l, c, int(p.get("period", 20))),
    "ROC":      lambda o, h, l, c, v, p: roc(c, int(p.get("period", 10))),
    "MOM":      lambda o, h, l, c, v, p: mom(c, int(p.get("period", 14))),
    "WILLR":    lambda o, h, l, c, v, p: willr(h, l, c, int(p.get("period", 14))),
    "MFI":      lambda o, h, l, c, v, p: mfi(h, l, c, v, int(p.get("period", 14))),
    "ATR":      lambda o, h, l, c, v, p: atr(h, l, c, int(p.get("period", 14))),
    "BBANDS":   lambda o, h, l, c, v, p: bbands(c, int(p.get("period", 20)), float(p.get("stddev", 2.0)))[0],
    "NATR":     lambda o, h, l, c, v, p: natr(h, l, c, int(p.get("period", 14))),
    "STDDEV":   lambda o, h, l, c, v, p: stddev(c, int(p.get("period", 20))),
    "OBV":      lambda o, h, l, c, v, p: obv(c, v),
    "VWAP":     lambda o, h, l, c, v, p: vwap(h, l, c, v),
    "AD":       lambda o, h, l, c, v, p: ad(h, l, c, v),
    "AROON":    lambda o, h, l, c, v, p: aroon(h, l, int(p.get("period", 14)))[0],
    "ADX":      lambda o, h, l, c, v, p: adx_calc(h, l, c, int(p.get("period", 14))),
    "DX":       lambda o, h, l, c, v, p: adx_calc(h, l, c, int(p.get("period", 14))),
    "SAR":      lambda o, h, l, c, v, p: sar(h, l, float(p.get("acceleration", 0.02)), float(p.get("maximum", 0.2))),
    "BOP":      lambda o, h, l, c, v, p: bop_calc(o, h, l, c),
    "TRIX":     lambda o, h, l, c, v, p: trix(c, int(p.get("period", 15))),
    "TSI":      lambda o, h, l, c, v, p: tsi(c, int(p.get("long_period", 25)), int(p.get("short_period", 13))),
}


def main():
    if len(sys.argv) < 3:
        print(json.dumps({
            "success": False,
            "error": "Usage: equity_talipp.py <indicator> <ohlcv_json> [params_json]"
        }))
        sys.exit(1)

    indicator = sys.argv[1].upper()
    ohlcv_raw = sys.argv[2]
    params    = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}

    try:
        data = json.loads(ohlcv_raw)
        if not isinstance(data, list) or len(data) == 0:
            raise ValueError("OHLCV data must be a non-empty JSON array")

        opens  = [float(r.get("open",  0)) for r in data]
        highs  = [float(r.get("high",  0)) for r in data]
        lows   = [float(r.get("low",   0)) for r in data]
        closes = [float(r.get("close", 0)) for r in data]
        vols   = [float(r.get("volume", 0)) for r in data]
        ts     = [int(r.get("timestamp", 0)) for r in data]

        if indicator not in DISPATCH:
            raise ValueError(f"Unknown indicator: {indicator}. "
                             f"Supported: {', '.join(sorted(DISPATCH.keys()))}")

        raw_values = DISPATCH[indicator](opens, highs, lows, closes, vols, params)

        # Pair with timestamps and filter None/NaN
        values_out = []
        timestamps_out = []
        for i, v in enumerate(raw_values):
            fv = safe_float(v)
            if fv is not None and i < len(ts):
                values_out.append(fv)
                timestamps_out.append(ts[i])

        print(json.dumps({
            "success": True,
            "indicator": indicator,
            "values": values_out,
            "timestamps": timestamps_out,
            "count": len(values_out),
        }))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "indicator": indicator}))
        sys.exit(1)


if __name__ == "__main__":
    main()
