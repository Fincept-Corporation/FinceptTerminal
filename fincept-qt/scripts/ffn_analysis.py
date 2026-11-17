"""
FFN (Financial Functions) Analysis — deep analytics per symbol plus allocation
alternatives (ERC, inverse-vol, equal, current) for the CURRENT book.

Input (stdin JSON):
    {"symbols": ["AAPL","MSFT"], "weights": {"AAPL": 0.6, "MSFT": 0.4},
     "risk_free_rate": 0.042,          # annual fraction; absent = 0 (meta.risk_free_known = false)
     "period": "1y",                   # 6mo|1y|2y|5y|10y|max
     "base_currency": "USD", "currencies": {"VOD.L": "GBp"},   # prices converted into the base
     "benchmark": "SPY",               # its trading days are the calendar of the book-level figures
     "periods_per_year": 252}          # else measured from that calendar

Definitions (shared with portfolio_metrics.py): volatility = stdev of daily returns ×
√(observations per year of THAT series' own calendar); Sharpe = (mean − rf/period) /
stdev × √ppy (arithmetic, not CAGR-based); annualised return = calendar CAGR over
365.25-day years, only for spans of at least ~11.5 months (CAGR_MIN_SPAN_DAYS = 350, so the
default "1y" window is annualised; shorter spans give null plus a *_note saying why);
avg_monthly_return = mean of
the compounded calendar-month returns. Each symbol is measured on its own trading
days — a holiday in one market no longer deletes that day for every other symbol.
Values that cannot be computed are null, never a fake 0.
"""
import json
import math
import os
import sys

import numpy as np

try:
    import fincept_json
except ImportError:
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

ALLOWED_PERIODS = {"6mo", "1y", "2y", "5y", "10y", "max"}
LIMIT = 252  # points per chart series
RISK_FREE_RATE = 0.0


def _fin(x):
    try:
        v = float(x)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def max_streak(series):
    max_s, cur = 0, 0
    for v in series:
        if v:
            cur += 1
            max_s = max(max_s, cur)
        else:
            cur = 0
    return max_s


# A "1y" download spans 360-364 calendar days from first to last trading day (less after a long
# holiday), so ">= 365" never annualised the default window. ~11.5 months still annualises with an
# exponent of at most 365.25/350 = 1.04; anything shorter is withheld (GIPS: never annualise < 1 year).
CAGR_MIN_SPAN_DAYS = 350


def _span_days(index):
    return (index[-1] - index[0]).days if len(index) > 1 else 0


def _cagr(level_or_cum, index):
    """Calendar CAGR of a cumulative growth factor over its index; None under CAGR_MIN_SPAN_DAYS."""
    span = _span_days(index)
    if span < CAGR_MIN_SPAN_DAYS:
        return None
    g = float(level_or_cum)
    return g ** (365.25 / span) - 1.0 if g > 0 else -1.0


def _cagr_note(index):
    """Why the CAGR is null, or None when it was computed."""
    span = _span_days(index)
    if span >= CAGR_MIN_SPAN_DAYS:
        return None
    return (f"not annualised: {span} days of history is under {CAGR_MIN_SPAN_DAYS} "
            "(~11.5 months); see total_return")


def _monthly_mean(returns):
    m = (1.0 + returns).groupby([returns.index.year, returns.index.month]).prod() - 1.0
    return float(m.mean()) if len(m) else None


def _dict_series(s):
    return {idx.strftime("%Y-%m-%d"): float(v) for idx, v in s.items() if v is not None and math.isfinite(float(v))}


def portfolio_stats(rets_df, weights_arr, ppy):
    """Book stats for weights on daily returns (union calendar, closed market = 0)."""
    import portfolio_metrics as pm

    w = np.array(weights_arr, dtype=float)
    w = w / w.sum()
    port = (rets_df.fillna(0.0) * w).sum(axis=1)
    port = port[rets_df.notna().any(axis=1)]
    cum = (1 + port).cumprod()
    total_ret = float(cum.iloc[-1] - 1)
    vol, sharpe, sortino = pm.sharpe_sortino(port, RISK_FREE_RATE, ppy)
    max_dd = float((cum / cum.cummax() - 1.0).min())
    return {
        "total_return": total_ret,
        "cagr": _cagr(cum.iloc[-1], port.index),
        "cagr_note": _cagr_note(port.index),
        "volatility": vol,
        "sharpe": sharpe,
        "sortino": sortino,
        "max_drawdown": max_dd,
    }


def compute_ffn(symbols, weights, period, base, currencies, bench, ppy_in):
    import pandas as pd
    import portfolio_metrics as pm

    mk = pm.load_market(symbols, currencies, base, bench or pm.GLOBAL_BENCHMARK, period)
    closes = mk["closes_base"]
    result = {}
    for sym in symbols:
        prices = closes.get(sym)
        if prices is None or len(prices) < 2:
            continue
        returns = prices.pct_change(fill_method=None).dropna()
        own_ppy = pm.calendar_periods_per_year(prices.index)
        total_ret = float(prices.iloc[-1] / prices.iloc[0] - 1)
        vol, sharpe, sortino = pm.sharpe_sortino(returns, RISK_FREE_RATE, own_ppy)
        dd = prices / prices.cummax() - 1.0
        result[sym] = {
            "total_return": total_ret,
            "annualized_return": _cagr(prices.iloc[-1] / prices.iloc[0], prices.index),
            "annualized_return_note": _cagr_note(prices.index),
            "annualized_volatility": vol,
            "max_drawdown": float(dd.min()),
            "sharpe_ratio": sharpe,
            "sortino_ratio": sortino,
            "current_price": float(prices.iloc[-1]),
            "start_price": float(prices.iloc[0]),
            "best_day": float(returns.max()),
            "worst_day": float(returns.min()),
            "avg_daily_return": float(returns.mean()),
            "positive_days": int((returns > 0).sum()),
            "negative_days": int((returns < 0).sum()),
            "max_win_streak": max_streak((returns > 0).astype(int).tolist()),
            "max_loss_streak": max_streak((returns < 0).astype(int).tolist()),
            "avg_monthly_return": _monthly_mean(returns),
            "skewness": _fin(returns.skew()),
            "kurtosis": _fin(returns.kurtosis()),
            "periods_per_year": own_ppy,
            "start": prices.index[0].strftime("%Y-%m-%d"),
        }

    valid_syms = [s for s in symbols if s in result]
    if not valid_syms:
        return {"error": "No price history for any holding (" + ", ".join(symbols) + ")"}

    # Rebased and drawdown series: each symbol on ITS OWN dates.
    result["rebased"] = {s: _dict_series((closes[s] / closes[s].iloc[0] * 100.0).tail(LIMIT)) for s in valid_syms}
    result["drawdown_series"] = {s: _dict_series((closes[s] / closes[s].cummax() - 1.0).tail(LIMIT))
                                 for s in valid_syms}

    # Rolling correlations: each pair on the dates both traded.
    rolling_out = {}
    if len(valid_syms) >= 2:
        WINDOW = 60
        for i in range(len(valid_syms)):
            for j in range(i + 1, len(valid_syms)):
                s1, s2 = valid_syms[i], valid_syms[j]
                pair = pd.concat([closes[s1].pct_change(fill_method=None).rename("a"),
                                  closes[s2].pct_change(fill_method=None).rename("b")], axis=1).dropna()
                if len(pair) <= WINDOW:
                    continue
                rc = pair["a"].rolling(WINDOW).corr(pair["b"]).dropna().tail(LIMIT)
                rolling_out[f"{s1}_{s2}"] = _dict_series(rc)
    result["rolling_corr"] = rolling_out

    # Allocation alternatives: from the date every holding has prices, union calendar
    # (a market holiday is a zero-return day for that line, not a deleted day).
    opt_out = {}
    if len(valid_syms) >= 2:
        start = max(closes[s].index[1] for s in valid_syms if len(closes[s]) > 1)
        ret_df = pd.DataFrame({s: closes[s].pct_change(fill_method=None) for s in valid_syms}).sort_index()
        ret_df = ret_df[ret_df.index >= start]
        bench_level = mk["bench_level"]
        ppy = ppy_in or (pm.calendar_periods_per_year(bench_level.index) if bench_level is not None
                         else pm.calendar_periods_per_year(ret_df.index))
        n = len(valid_syms)
        if len(ret_df) >= 30:
            filled = ret_df.fillna(0.0)
            equal_w = {s: round(1.0 / n, 6) for s in valid_syms}
            cur = np.array([weights.get(s, 0.0) for s in valid_syms], dtype=float)
            current_w = {s: round(float(cur[i] / cur.sum()), 6) for i, s in enumerate(valid_syms)} \
                if cur.sum() > 0 else dict(equal_w)
            notes = []
            erc_w = None
            try:
                import ffn
                erc_arr = ffn.calc_erc_weights(filled, covar_method="ledoit-wolf", risk_parity_method="ccd",
                                               maximum_iterations=100, tolerance=1e-8)
                erc_vals = np.asarray(getattr(erc_arr, "values", erc_arr), dtype=float)
                erc_w = {s: round(float(erc_vals[i]), 6) for i, s in enumerate(valid_syms)}
            except Exception as e:
                notes.append(f"ERC weights unavailable: {type(e).__name__}")
            vols = filled.std()
            inv_vol_w = None
            if (vols > 0).all():
                iv = (1.0 / vols) / (1.0 / vols).sum()
                inv_vol_w = {s: round(float(iv[s]), 6) for s in valid_syms}
            else:
                notes.append("inverse-vol weights unavailable: a holding has zero volatility")

            stats = {}
            for name, wd in [("erc", erc_w), ("inv_vol", inv_vol_w), ("equal", equal_w), ("current", current_w)]:
                if wd is None:
                    continue
                try:
                    stats[name] = portfolio_stats(ret_df, [wd.get(s, 0.0) for s in valid_syms], ppy)
                except Exception as e:
                    print(f"[ffn_analysis] stats for {name} failed: {e}", file=sys.stderr)
            opt_out = {"erc": erc_w, "inv_vol": inv_vol_w, "equal": equal_w, "current": current_w, "stats": stats,
                       "start": ret_df.index[0].strftime("%Y-%m-%d"), "periods_per_year": ppy, "notes": notes}
    result["optimization"] = opt_out
    result["meta"] = {"base_currency": base, "period": period, "risk_free_rate": RISK_FREE_RATE,
                      "cagr_min_span_days": CAGR_MIN_SPAN_DAYS,
                      "warnings": mk["warnings"], "symbols_missing": [s for s in symbols if s not in result]}
    return result


def main():
    global RISK_FREE_RATE
    stdin_data = sys.stdin.read()
    if not stdin_data.strip():
        fincept_json.emit({"error": "No input data"})
        return
    try:
        params = json.loads(stdin_data)
    except Exception as exc:
        fincept_json.emit({"error": f"JSON parse error: {exc}"})
        return
    symbols = [str(s).strip().upper() for s in params.get("symbols", []) if str(s).strip()]
    if not symbols:
        fincept_json.emit({"error": "No symbols provided"})
        return
    raw_weights = params.get("weights", {}) or {}
    weights = {}
    for s in symbols:
        v = _fin(raw_weights.get(s))
        weights[s] = v if v is not None and v > 0 else 0.0
    if sum(weights.values()) <= 0:
        weights = {s: 1.0 / len(symbols) for s in symbols}
    rf = _fin(params.get("risk_free_rate"))
    rf_known = rf is not None
    RISK_FREE_RATE = rf if rf is not None else 0.0
    period = str(params.get("period") or "1y").strip().lower()
    if period not in ALLOWED_PERIODS:
        period = "1y"
    base = str(params.get("base_currency") or "USD").strip().upper() or "USD"
    currencies = {str(k).strip().upper(): str(v) for k, v in (params.get("currencies") or {}).items()}
    try:
        result = compute_ffn(symbols, weights, period, base, currencies, str(params.get("benchmark") or "").strip(),
                             _fin(params.get("periods_per_year")))
    except SystemExit:
        raise
    except Exception as exc:  # yfinance / pandas failures must come back as JSON, not a traceback
        result = {"error": str(exc)}
    if "meta" in result:
        result["meta"]["risk_free_known"] = rf_known
    fincept_json.emit(result)


if __name__ == "__main__":
    main()
