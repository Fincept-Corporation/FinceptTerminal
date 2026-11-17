"""
QuantStats Monte Carlo Simulation — forward paths of the CURRENT book.

Input (stdin JSON):
    {"symbols": ["AAPL","VOD.L"], "weights": [0.6, 0.4],
     "num_simulations": 5000,      # paths, 100 .. 100000
     "horizon_days": 252,          # trading days simulated (default: one year of the calendar)
     "seed": null,                 # integer for a reproducible run; null/absent = fresh randomness
     "method": "bootstrap",        # "bootstrap" (default: whole historical days, keeps fat tails and
                                   #  co-movement) | "gbm" (multivariate normal on log returns)
     "period": "2y",               # history the model is fitted on: 1y|2y|3y|5y|10y
     "base_currency": "USD", "currencies": {"VOD.L": "GBp"}, "benchmark": "SPY",
     "periods_per_year": 252}      # else measured from the benchmark's calendar
Each holding is simulated on its own (jointly), the book is rebalanced to its
weights daily. History starts where every holding of >= 1 % weight has prices;
nothing is forward/back-filled. Prices are in the base currency (FX risk included).

Output (percent values): median_return, percentile_5, percentile_95, prob_loss,
expected_max_dd, cvar_5, annual_drift, annual_volatility, horizon_days,
num_simulations, method, seed (null when random), observations, history_start,
history_end, periods_per_year, paths (first 50, cumulative %), percentile_paths
{p5,p25,p50,p75,p95}, terminal_returns (a sample of <= 2000 paths' final cumulative %),
num_paths_shown, warnings. A value that cannot be computed is
null, never 0.
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

ALLOWED_PERIODS = {"1y", "2y", "3y", "5y", "10y"}
MIN_OBS = 60


def _fin(x):
    try:
        v = float(x)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def compute_max_drawdown_paths(paths):
    """Max drawdown per path (array [num_sims, steps] of levels starting at 1.0)."""
    peak = np.maximum.accumulate(paths, axis=1)
    return ((paths - peak) / peak).min(axis=1)


def run_simulation(symbols, weights, num_simulations, horizon, seed, method, period, base, currencies, bench, ppy):
    import pandas as pd
    import portfolio_metrics as pm

    yf_period = "5y" if period == "3y" else period
    mk = pm.load_market(symbols, currencies, base, bench or pm.GLOBAL_BENCHMARK, yf_period)
    closes = mk["closes_base"]
    warnings = list(mk["warnings"])
    held = [s for s in symbols if s in closes]
    if not held:
        return {"error": "No price history for any holding"}
    wmap = {s: float(w) for s, w in zip(symbols, weights)}
    w = pd.Series({s: max(0.0, wmap.get(s, 0.0)) for s in held}, dtype=float)
    dropped = [s for s in symbols if s not in held]
    if dropped:
        warnings.append("no price history (left out, weights renormalised): " + ", ".join(dropped))
    w = pd.Series(1.0 / len(held), index=held) if w.sum() <= 0 else w / w.sum()

    rets = pd.DataFrame({s: closes[s].pct_change(fill_method=None) for s in held}).sort_index().iloc[1:]
    material = [s for s in held if w[s] >= 0.01]
    start = max(closes[s].index[1] for s in material) if material else rets.index[0]
    if period == "3y":
        start = max(start, rets.index[-1] - pd.DateOffset(years=3))
    rets = rets[rets.index >= start]
    bench_level = mk["bench_level"]
    if bench_level is not None and len(rets):
        cal = bench_level.index[bench_level.index >= rets.index[0]]
        rets = pd.DataFrame({s: pm.compound_onto(rets[s].fillna(0.0), cal) for s in held}).reindex(cal).fillna(0.0)
        if not ppy:
            ppy = pm.calendar_periods_per_year(bench_level.index)
    else:
        rets = rets.fillna(0.0)  # a closed market is a zero-return day for that line
        if not ppy:
            ppy = pm.calendar_periods_per_year(rets.index)
    if len(rets) < MIN_OBS:
        return {"error": f"Insufficient shared history: {len(rets)} days since the youngest material listing "
                         f"(need {MIN_OBS})"}
    if not horizon:
        horizon = int(round(ppy))

    R = rets[held].values.astype(float)        # (T, n)
    wv = w[held].values.astype(float)
    rng = np.random.default_rng(seed)
    port_hist = R @ wv
    mu_d = float(np.mean(port_hist))
    sd_d = float(np.std(port_hist, ddof=1))

    # Paths in chunks to bound memory: (chunk, horizon) portfolio daily returns.
    levels = np.empty((num_simulations, horizon), dtype=float)
    done = 0
    if method == "gbm":
        logr = np.log1p(R)
        mu = logr.mean(axis=0)
        cov = np.cov(logr, rowvar=False).reshape(len(held), len(held))
    while done < num_simulations:
        k = min(5000, num_simulations - done)
        if method == "gbm":
            draws = rng.multivariate_normal(mu, cov, size=(k, horizon))   # log returns per asset
            daily = np.expm1(draws) @ wv
        else:
            idx = rng.integers(0, len(R), size=(k, horizon))
            daily = R[idx] @ wv
        levels[done:done + k] = np.cumprod(1.0 + daily, axis=1)
        done += k

    terminal = levels[:, -1] - 1.0
    full = np.hstack([np.ones((num_simulations, 1)), levels])
    max_dd = compute_max_drawdown_paths(full)
    tail = np.sort(terminal)[: max(1, int(0.05 * num_simulations))]
    n_shown = min(50, num_simulations)
    percentile_paths = {"p%d" % p: [float(v) for v in (np.percentile(levels, p, axis=0) - 1.0) * 100.0]
                        for p in (5, 25, 50, 75, 95)}
    return {
        "median_return": float(np.median(terminal)) * 100.0,
        "percentile_5": float(np.percentile(terminal, 5)) * 100.0,
        "percentile_95": float(np.percentile(terminal, 95)) * 100.0,
        "prob_loss": float(np.mean(terminal < 0)) * 100.0,
        "expected_max_dd": float(np.mean(max_dd)) * 100.0,
        "cvar_5": float(np.mean(tail)) * 100.0,
        "annual_drift": mu_d * ppy * 100.0,
        "annual_volatility": sd_d * math.sqrt(ppy) * 100.0,
        "horizon_days": horizon,
        "num_simulations": num_simulations,
        "method": method,
        "seed": seed,
        "observations": int(len(R)),
        "history_start": rets.index[0].strftime("%Y-%m-%d"),
        "history_end": rets.index[-1].strftime("%Y-%m-%d"),
        "periods_per_year": ppy,
        "base_currency": base,
        "paths": [[(float(levels[i, t]) - 1.0) * 100.0 for t in range(horizon)] for i in range(n_shown)],
        "percentile_paths": percentile_paths,
        # A sample (without replacement) of every path's terminal cumulative return, percent: the
        # outcome histogram. Drawn after the paths, so a seeded run's paths are unchanged.
        "terminal_returns": [round(float(v) * 100.0, 3)
                             for v in rng.choice(terminal, size=min(2000, num_simulations), replace=False)],
        "num_paths_shown": n_shown,
        "warnings": warnings,
    }


def main():
    try:
        stdin_data = sys.stdin.read()
        if not stdin_data.strip():
            fincept_json.emit({"error": "No input data"})
            return
        params = json.loads(stdin_data)
        symbols = [str(s).strip().upper() for s in params.get("symbols", []) if str(s).strip()]
        weights = params.get("weights", [])
        if not symbols:
            fincept_json.emit({"error": "No symbols provided"})
            return
        if not weights or len(weights) != len(symbols):
            weights = [1.0 / len(symbols)] * len(symbols)
        try:
            num_simulations = int(params.get("num_simulations", 5000))
        except (TypeError, ValueError):
            num_simulations = 5000
        num_simulations = max(100, min(num_simulations, 100000))
        try:
            horizon = int(params["horizon_days"]) if params.get("horizon_days") is not None else 0
        except (TypeError, ValueError):
            horizon = 0
        horizon = max(0, min(horizon, 2520))
        seed = params.get("seed")
        try:
            seed = int(seed) if seed is not None else None
        except (TypeError, ValueError):
            seed = None
        method = str(params.get("method") or "bootstrap").strip().lower()
        if method not in ("bootstrap", "gbm"):
            method = "bootstrap"
        period = str(params.get("period") or "2y").strip().lower()
        if period not in ALLOWED_PERIODS:
            period = "2y"
        base = str(params.get("base_currency") or "USD").strip().upper() or "USD"
        currencies = {str(k).strip().upper(): str(v) for k, v in (params.get("currencies") or {}).items()}
        result = run_simulation(symbols, weights, num_simulations, horizon, seed, method, period, base, currencies,
                                str(params.get("benchmark") or "").strip(), _fin(params.get("periods_per_year")))
        fincept_json.emit(result)
    except SystemExit:
        raise
    except Exception as exc:
        fincept_json.emit({"error": str(exc)})


if __name__ == "__main__":
    main()
