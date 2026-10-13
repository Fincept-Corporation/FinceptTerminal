"""
QuantStats Analysis — quantitative statistics for a portfolio's CURRENT weights
applied to trailing daily price history (a back-test of today's mix).

Input (stdin JSON):
    {"symbols": ["AAPL","MSFT"], "weights": [0.5, 0.5],
     "risk_free_rate": 0.042,          # optional annual fraction, default 0.04
     "benchmark": "SPY"}               # optional; same stats computed for it
Output (stdout JSON): {performance, risk, ratios, distribution, benchmark?, meta}
All returns/ratios are FRACTIONS (the C++ view multiplies by 100). Non-finite
values are emitted as null, never as fake zeros.
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

DEFAULT_RF = 0.04


def _close_frame(data, symbols):
    import pandas as pd

    if data is None or data.empty:
        return None
    close = data["Close"]
    if not isinstance(close, pd.DataFrame):
        close = pd.DataFrame({symbols[0]: close})
    return close


def _periods_per_year(index):
    """Observations per year implied by the data (≈252 equities, ≈365 crypto)."""
    if len(index) < 2:
        return 252.0
    days = (index[-1] - index[0]).days
    if days <= 0:
        return 252.0
    return float(min(366.0, max(50.0, (len(index) - 1) / (days / 365.25))))


def series_stats(r, rf_annual):
    """Statistics for one daily simple-return series (pandas Series)."""
    r = r.dropna()
    n = len(r)
    if n < 2:
        return None
    ppy = _periods_per_year(r.index)
    rf_p = (1.0 + rf_annual) ** (1.0 / ppy) - 1.0
    cumulative = (1 + r).cumprod()
    total_return = float(cumulative.iloc[-1] - 1.0)
    years = max((r.index[-1] - r.index[0]).days / 365.25, 1e-9)
    ann_return = float((1.0 + total_return) ** (1.0 / years) - 1.0) if total_return > -1 else -1.0
    sd = float(r.std(ddof=1))
    ann_vol = sd * math.sqrt(ppy)
    excess_mean = float(r.mean()) - rf_p
    # Standard (arithmetic) Sharpe: mean excess return / stdev, annualised.
    sharpe = excess_mean / sd * math.sqrt(ppy) if sd > 0 else None
    # Sortino: downside deviation vs the risk-free target over ALL periods.
    # (The old code took the stdev of the negative days only, which is a
    # different and smaller number, inflating the ratio.)
    shortfall = np.minimum(r.values - rf_p, 0.0)
    dd = float(np.sqrt(np.mean(shortfall ** 2)))
    sortino = excess_mean / dd * math.sqrt(ppy) if dd > 0 else None
    downside_dev = dd * math.sqrt(ppy)

    peak = cumulative.cummax()
    drawdown = cumulative / peak - 1.0
    max_dd = float(drawdown.min())
    calmar = ann_return / abs(max_dd) if max_dd < 0 else None

    q05 = float(np.percentile(r.values, 5))
    tail = r[r <= q05]
    wins = int((r > 0).sum())
    losses = int((r < 0).sum())
    gains = float(r[r > 0].sum())
    loss_sum = float(-r[r < 0].sum())
    return {
        "performance": {
            "total_return": total_return,
            "annualized_return": ann_return,
            "trading_days": n,
            "best_day": float(r.max()),
            "worst_day": float(r.min()),
            "avg_daily_return": float(r.mean()),
        },
        "risk": {
            "annualized_volatility": ann_vol,
            "max_drawdown": max_dd,
            "var_95_daily": q05,  # 5th percentile daily return (negative = loss)
            "cvar_95_daily": float(tail.mean()) if len(tail) else q05,
            "downside_deviation": downside_dev,
        },
        "ratios": {
            "sharpe_ratio": sharpe,
            "sortino_ratio": sortino,
            "calmar_ratio": calmar,
            "profit_factor": gains / loss_sum if loss_sum > 0 else None,
        },
        "distribution": {
            "skewness": float(r.skew()),
            "kurtosis": float(r.kurtosis()),  # EXCESS kurtosis (normal = 0)
            "win_rate": wins / (wins + losses) if (wins + losses) else None,
            "win_days": wins,
            "loss_days": losses,
            "avg_win": float(r[r > 0].mean()) if wins else None,
            "avg_loss": float(r[r < 0].mean()) if losses else None,
        },
        "meta": {"periods_per_year": ppy, "risk_free_rate": rf_annual,
                 "start": r.index[0].strftime("%Y-%m-%d"), "end": r.index[-1].strftime("%Y-%m-%d")},
    }


def compute_stats(symbols, weights, rf_annual, benchmark, period="1y"):
    import yfinance as yf

    tickers = list(dict.fromkeys(symbols + ([benchmark] if benchmark else [])))
    data = yf.download(tickers, period=period, interval="1d", progress=False, auto_adjust=True)
    close = _close_frame(data, tickers)
    if close is None:
        return {"error": "Could not fetch price data"}

    # A symbol yfinance could not price is an all-NaN column; keep only symbols with data.
    close = close.dropna(axis=1, how="all")
    held = [s for s in symbols if s in close.columns]
    if not held:
        return {"error": "No price data for any symbol"}

    # Each line's returns on its own calendar; a closed market contributes 0.
    rets = close[held].pct_change(fill_method=None)
    rets = rets.iloc[1:]
    # Map weights by NAME (yfinance sorts columns alphabetically); re-normalise
    # over the symbols that actually have data.
    wmap = {s: float(wt) for s, wt in zip(symbols, weights)}
    w = np.array([wmap.get(s, 0.0) for s in held], dtype=float)
    w = np.ones(len(held)) / len(held) if w.sum() <= 0 else w / w.sum()
    port = (rets.fillna(0.0) * w).sum(axis=1)
    port = port[rets.notna().any(axis=1)]
    if len(port) < 2:
        return {"error": "Insufficient price history"}

    out = series_stats(port, rf_annual)
    if out is None:
        return {"error": "Insufficient price history"}
    out["meta"]["symbols_used"] = held
    out["meta"]["symbols_missing"] = [s for s in symbols if s not in held]
    if benchmark and benchmark in close.columns:
        b = close[benchmark].dropna().pct_change(fill_method=None)
        bs = series_stats(b, rf_annual)
        if bs is not None:
            bs["symbol"] = benchmark
            out["benchmark"] = bs
    return out


def main():
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
    weights = params.get("weights", [])
    if not symbols:
        fincept_json.emit({"error": "No symbols provided"})
        return
    if not weights or len(weights) != len(symbols):
        weights = [1.0 / len(symbols)] * len(symbols)
    try:
        rf = float(params.get("risk_free_rate", DEFAULT_RF))
        if not math.isfinite(rf):
            rf = DEFAULT_RF
    except (TypeError, ValueError):
        rf = DEFAULT_RF
    benchmark = str(params.get("benchmark") or "").strip().upper()

    try:
        result = compute_stats(symbols, weights, rf, benchmark)
    except Exception as exc:  # yfinance / pandas failures must come back as JSON
        result = {"error": str(exc)}
    fincept_json.emit(result)


if __name__ == "__main__":
    main()
