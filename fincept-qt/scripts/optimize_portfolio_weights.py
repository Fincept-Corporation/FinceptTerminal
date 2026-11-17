"""
optimize_portfolio_weights.py — Portfolio optimization for Fincept Terminal.

Input (stdin JSON):
  {
    "symbols":        ["AAPL", "MSFT", "GOOGL"],
    "weights":        [0.4, 0.35, 0.25],       # current weights (fractions)
    "method":         "max_sharpe",             # see STRATEGIES below
    "returns_method": "mean_historical",        # mean_historical | ema | capm | james_stein
    "risk_model":     "sample_covariance",      # sample_covariance | ledoit_wolf | semicovariance | exponential
    "risk_free_rate": 0.04,                     # annual, fraction; optional, default 0.04
    "period": "1y",                             # history window: 6mo|1y|2y|3y|5y|10y
    "base_currency": "USD",                     # prices converted into it (FX risk included)
    "currencies": {"VOD.L": "GBp"},             # quoted currency per symbol (else from the suffix)
    "benchmark": "SPY",                         # its trading days are the observation calendar
    "periods_per_year": 252,                    # else measured from that calendar
    "max_weight": 0.25, "min_weight": 0.0,      # optional per-position bounds (fractions)
    "sectors": {"AAPL": "Technology"},          # optional, with
    "sector_caps": {"Technology": 0.40}         #   optional maximum weight per sector
  }
  Every solver run reports "converged" / "solver_message"; a strategy whose solver did
  not converge is flagged in "warnings" (and its weights are the best point found).
  Frontier points whose solve failed are dropped ("frontier_failed" counts them).
  Values that cannot be computed are null (never 0).

Output (stdout JSON):
  {
    "weights":                {"AAPL": 0.45, ...},   # optimal weights
    "expected_annual_return": 0.142,
    "annual_volatility":      0.178,
    "sharpe_ratio":           1.25,
    "strategy":               "max_sharpe",
    "frontier": [                                    # 40 points on efficient frontier
      {"volatility": 0.10, "return": 0.06, "sharpe": 0.20},
      ...
    ],
    "comparison": {                                  # all-methods comparison
      "max_sharpe":      {"weights": {...}, "return": ..., "volatility": ..., "sharpe": ...},
      "min_volatility":  {...},
      "risk_parity":     {...},
      "hrp":             {...},
      "equal_weight":    {...}
    },
    "current":   {"return": ..., "volatility": ..., "sharpe": ...},   # today's weights, same model
    "assets":    {"AAPL": {"return": ..., "volatility": ...}, ...},  # each holding on its own
    "implied_returns":   {...},   # Black-Litterman equilibrium prior (current weights)
    "posterior_returns": {...},   # prior blended with the historical-return views
    "returns_model": "...", "risk_model": "...", "periods_per_year": 252.0
  }

STRATEGIES (method field):
  max_sharpe        — Maximise Sharpe ratio (default)
  min_volatility    — Minimise portfolio standard deviation
  risk_parity       — Equal risk contribution per asset
  max_return        — Maximise expected return (fully invests in best asset)
  equal_weight      — 1/N allocation (no optimisation)
  hrp               — Hierarchical Risk Parity (correlation clustering + recursive bisection)
  target_return     — Efficient portfolio at the requested target return (default 10%)
  black_litterman   — Max Sharpe on BL posterior returns (current-weights prior + historical views)

Annualisation uses the observations per year implied by the data (~252 for one
equity market, ~365 for crypto) instead of a fixed 252.
"""
import math
import sys
import json
import numpy as np


# ── Helpers ───────────────────────────────────────────────────────────────────

def convert_numpy(obj):
    if isinstance(obj, dict):
        return {k: convert_numpy(v) for k, v in obj.items()}
    elif isinstance(obj, (list, tuple)):
        return [convert_numpy(v) for v in obj]
    elif isinstance(obj, np.integer):
        return int(obj)
    elif isinstance(obj, np.floating):
        v = float(obj)
        return None if (v != v or v == float("inf") or v == float("-inf")) else v
    elif isinstance(obj, np.ndarray):
        return [convert_numpy(x) for x in obj]
    elif isinstance(obj, float):
        return None if (obj != obj or obj == float("inf") or obj == float("-inf")) else obj
    return obj


ALLOWED_PERIODS = {"6mo", "1y", "2y", "3y", "5y", "10y"}


def fetch_returns(symbols, period="1y", base="USD", currencies=None, benchmark=""):
    """(daily base-currency returns DataFrame, periods_per_year, warnings) or (None, _, warnings).

    Returns start where every symbol has prices; afterwards a market holiday is a
    zero-return day for that line (union calendar) instead of deleting the day for
    every symbol, and nothing is back-filled before a listing."""
    import pandas as pd
    import portfolio_metrics as pm

    yf_period = "5y" if period == "3y" else period
    mk = pm.load_market(symbols, currencies or {}, base, benchmark or pm.GLOBAL_BENCHMARK, yf_period)
    closes = mk["closes_base"]
    warnings = list(mk["warnings"])
    available = [s for s in symbols if s in closes and len(closes[s]) > 2]
    if not available:
        return None, 252.0, warnings
    dropped = [s for s in symbols if s not in available]
    if dropped:
        warnings.append("no price history (left out): " + ", ".join(dropped))
    rets = pd.DataFrame({s: closes[s].pct_change(fill_method=None) for s in available}).sort_index()
    start = max(closes[s].index[1] for s in available)
    if period == "3y":
        start = max(start, rets.index[-1] - pd.DateOffset(years=3))
    rets = rets[rets.index >= start]
    rets = rets[rets.notna().any(axis=1)].fillna(0.0)
    bl = mk["bench_level"]
    ppy = pm.calendar_periods_per_year(bl.index) if bl is not None else pm.calendar_periods_per_year(rets.index)
    return rets, ppy, warnings


# ── Core optimiser ────────────────────────────────────────────────────────────

EWM_SPAN = 180          # observations — recent-weighted mean / covariance
BL_TAU = 0.05           # Black-Litterman prior uncertainty scale

RETURNS_LABELS = {
    "mean_historical": "Mean historical",
    "ema": "Exponentially weighted (span %d)" % EWM_SPAN,
    "capm": "CAPM (equal-weight market proxy)",
    "james_stein": "James-Stein shrinkage",
}
RISK_LABELS = {
    "sample_covariance": "Sample covariance",
    "ledoit_wolf": "Ledoit-Wolf shrinkage",
    "semicovariance": "Downside semicovariance",
    "exponential": "Exponentially weighted covariance (span %d)" % EWM_SPAN,
}


def periods_per_year(index):
    """Observations per year implied by the data (~252 for one equity market, ~365 crypto)."""
    if len(index) < 2:
        return 252.0
    days = (index[-1] - index[0]).days
    if days <= 0:
        return 252.0
    return float(min(366.0, max(50.0, (len(index) - 1) / (days / 365.25))))


def expected_returns(returns_df, method, ppy, rf):
    """Annualised expected returns by the chosen model (arithmetic, fractions)."""
    if method == "ema":
        return returns_df.ewm(span=EWM_SPAN).mean().iloc[-1].values * ppy
    if method == "capm":
        market = returns_df.mean(axis=1)
        var_m = float(market.var(ddof=1))
        mkt_ret = float(market.mean()) * ppy
        if var_m <= 0:
            return returns_df.mean().values * ppy
        betas = np.array([float(returns_df[c].cov(market)) / var_m for c in returns_df.columns])
        return rf + betas * (mkt_ret - rf)
    if method == "james_stein":
        mu = returns_df.mean().values
        n, t = len(mu), len(returns_df)
        grand = float(mu.mean())
        spread = float(np.sum((mu - grand) ** 2))
        if n > 3 and spread > 0 and t > 0:
            s2 = float(returns_df.var(ddof=1).mean())   # average per-asset daily variance
            shrink = min(1.0, max(0.0, (n - 3) * s2 / (t * spread)))
        else:
            shrink = 0.0
        return (grand + (1.0 - shrink) * (mu - grand)) * ppy
    return returns_df.mean().values * ppy           # mean_historical


def ledoit_wolf_cov(returns_df):
    """Ledoit-Wolf (2004) shrinkage of the sample covariance toward a scaled identity."""
    x = returns_df.values - returns_df.values.mean(axis=0)
    t, n = x.shape
    s = x.T @ x / t
    mu = float(np.trace(s)) / n
    f = mu * np.eye(n)
    d2 = float(np.sum((s - f) ** 2)) / n
    row_sq = np.sum(x ** 2, axis=1)                 # |x_t|^2
    b2_bar = (float(np.sum(row_sq ** 2)) - t * float(np.sum(s ** 2))) / (n * t * t)
    b2 = min(max(b2_bar, 0.0), d2)
    shrink = b2 / d2 if d2 > 0 else 0.0
    return shrink * f + (1.0 - shrink) * s


def risk_model_cov(returns_df, method, rf_daily):
    """Daily covariance by the chosen model (annualised by the caller)."""
    if method == "ledoit_wolf":
        return ledoit_wolf_cov(returns_df)
    if method == "semicovariance":
        downside = np.minimum(returns_df.values - rf_daily, 0.0)
        return downside.T @ downside / len(returns_df)
    if method == "exponential":
        n = returns_df.shape[1]
        ew = returns_df.ewm(span=EWM_SPAN).cov()
        return ew.iloc[-n:].values
    return returns_df.cov().values                  # sample_covariance


def build_params(returns_df, returns_method="mean_historical", risk_model="sample_covariance", rf=0.04, ppy=None):
    """Return (mean_returns_annual, cov_annual, n, periods_per_year) from a returns DataFrame."""
    if not ppy:
        ppy = periods_per_year(returns_df.index)
    rf_daily = (1.0 + rf) ** (1.0 / ppy) - 1.0
    mean_ret = expected_returns(returns_df, returns_method, ppy, rf)
    cov = risk_model_cov(returns_df, risk_model, rf_daily) * ppy
    cov = (cov + cov.T) / 2.0                       # symmetric for the solver
    return mean_ret, cov, len(mean_ret), ppy


def hrp_weights(cov):
    """Hierarchical Risk Parity (Lopez de Prado): single-linkage clustering on the
    correlation distance, quasi-diagonal ordering, recursive bisection with
    inverse-variance cluster weights."""
    n = cov.shape[0]
    if n == 1:
        return np.ones(1)
    from scipy.cluster.hierarchy import leaves_list, linkage
    from scipy.spatial.distance import squareform

    sd = np.sqrt(np.clip(np.diag(cov), 1e-18, None))
    corr = np.clip(cov / np.outer(sd, sd), -1.0, 1.0)
    dist = np.sqrt(np.clip((1.0 - corr) / 2.0, 0.0, 1.0))
    np.fill_diagonal(dist, 0.0)
    order = list(leaves_list(linkage(squareform(dist, checks=False), method="single")))

    def cluster_var(idx):
        sub = cov[np.ix_(idx, idx)]
        ivp = 1.0 / np.clip(np.diag(sub), 1e-18, None)
        ivp /= ivp.sum()
        return float(ivp @ sub @ ivp)

    w = np.ones(n)
    clusters = [order]
    while clusters:
        nxt = []
        for c in clusters:
            if len(c) <= 1:
                continue
            half = len(c) // 2
            left, right = c[:half], c[half:]
            v_l, v_r = cluster_var(left), cluster_var(right)
            alpha = 1.0 - v_l / (v_l + v_r) if (v_l + v_r) > 0 else 0.5
            w[left] *= alpha
            w[right] *= 1.0 - alpha
            nxt += [left, right]
        clusters = nxt
    return w / w.sum()


def black_litterman_returns(mean_ret, cov, w_mkt, rf, delta=None):
    """Posterior expected returns. Prior: market-implied equilibrium pi = delta*Sigma*w_mkt
    (w_mkt = the caller's current weights). Views: the chosen historical-return estimate,
    one absolute view per asset with He-Litterman confidence Omega = diag(tau*Sigma).
    Returns (pi, posterior, delta)."""
    n = len(mean_ret)
    if delta is None:
        # Market-implied risk aversion of the prior portfolio, kept in a sane band.
        port_ret = float(w_mkt @ mean_ret)
        port_var = float(w_mkt @ cov @ w_mkt)
        delta = (port_ret - rf) / port_var if port_var > 0 else 2.5
        delta = float(min(5.0, max(1.0, delta)))
    pi = delta * cov @ w_mkt
    tau_sigma = BL_TAU * cov
    omega = np.diag(np.clip(np.diag(tau_sigma), 1e-12, None))
    p = np.eye(n)
    try:
        inv_ts = np.linalg.inv(tau_sigma)
        inv_om = np.linalg.inv(omega)
        a = inv_ts + p.T @ inv_om @ p
        b = inv_ts @ pi + p.T @ inv_om @ mean_ret
        posterior = np.linalg.solve(a, b)
    except np.linalg.LinAlgError:
        posterior = (pi + mean_ret) / 2.0
    return pi, posterior, delta


def run_strategy(strategy, mean_ret, cov, n, extra=None, rf=0.04):
    """
    Returns dict: {weights, return, volatility, sharpe} or {error}.
    weights is a numpy array of length n.
    """
    if extra is None:
        extra = {}
    RF = rf

    from scipy.optimize import minimize

    bounds      = extra.get("bounds") or tuple((0.0, 1.0) for _ in range(n))
    constraints = [{"type": "eq", "fun": lambda w: np.sum(w) - 1.0}] + list(extra.get("ineq") or [])
    w0          = feasible_start(bounds, n)

    def port_ret(w):  return float(np.dot(w, mean_ret))
    def port_vol(w):  return float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0)))
    def neg_sharpe(w):
        r, v = port_ret(w), port_vol(w)
        return -(r - RF) / v if v > 1e-10 else 0.0

    try:
        if strategy in ("max_sharpe", "max_return_adj"):
            res = minimize(neg_sharpe, w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-9, "maxiter": 1000})

        elif strategy == "min_volatility":
            res = minimize(port_vol, w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-9, "maxiter": 1000})

        elif strategy == "max_return":
            res = minimize(lambda w: -port_ret(w), w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-9, "maxiter": 1000})

        elif strategy == "risk_parity":
            def rp_obj(w):
                v = port_vol(w)
                if v < 1e-10:
                    return 0.0
                mrc = np.dot(cov, w) / v
                rc  = w * mrc
                target = v / n
                return float(np.sum((rc - target) ** 2))
            res = minimize(rp_obj, w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-12, "maxiter": 2000})

        elif strategy in ("equal_weight", "equal"):
            w = np.ones(n) / n
            return _with_caps_note(_build_result(w, mean_ret, cov, strategy, RF), w, extra)

        elif strategy == "hrp":
            # Real HRP (clusters by correlation, then allocates by recursive bisection).
            # It used to be plain inverse-variance weighting under the HRP label.
            w = hrp_weights(cov)
            return _with_caps_note(_build_result(w, mean_ret, cov, strategy, RF), w, extra)

        elif strategy == "inv_var":
            diag = np.diag(cov)
            diag = np.where(diag > 1e-12, diag, 1e-12)
            inv_var = 1.0 / diag
            w = inv_var / inv_var.sum()
            return _with_caps_note(_build_result(w, mean_ret, cov, strategy, RF), w, extra)

        elif strategy in ("target_return",):
            # Efficient portfolio for the target: minimum volatility subject to return >= target.
            # The old EQUALITY constraint forced a dominated portfolio (more risk, less return than
            # the minimum-volatility mix) whenever the target sat below that mix's return.
            try:
                tgt = float(extra.get("target_return", 0.10))
            except (TypeError, ValueError):
                tgt = float("nan")
            if not math.isfinite(tgt):
                return {"error": "target_return must be an annual fraction (e.g. 0.10)"}
            top = minimize(lambda w: -port_ret(w), w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-12, "maxiter": 1000})
            max_ret = port_ret(top.x) if top.success else None
            if max_ret is not None and tgt > max_ret + 1e-6:
                return {"error": f"target return {tgt:.2%} is above the highest expected return these holdings "
                                 f"can reach under the weight limits ({max_ret:.2%})"}
            cons = constraints + [{"type": "ineq", "fun": lambda w: port_ret(w) - tgt}]
            res = minimize(port_vol, w0, method="SLSQP",
                           bounds=bounds, constraints=cons,
                           options={"ftol": 1e-9, "maxiter": 1000})
            got = port_ret(res.x)
            if got < tgt - 1e-6:
                return {"error": f"target return {tgt:.2%} could not be reached (best found {got:.2%})"}
            out = _build_result(res.x, mean_ret, cov, strategy, RF)
            out["converged"] = bool(getattr(res, "success", False))
            out["solver_message"] = str(getattr(res, "message", ""))
            out["target_return"] = tgt
            if got > tgt + 5e-4:
                out["target_note"] = (f"target {tgt:.2%} is below the minimum-volatility portfolio's expected return "
                                      f"({got:.2%}): that portfolio meets the target with the least risk, so it is "
                                      "the answer (a mix earning exactly the target would carry more risk)")
            return out

        elif strategy == "black_litterman":
            # Posterior returns from main() (equilibrium prior at the CURRENT weights,
            # blended with the historical-return views). Without them fall back to the
            # equal-weight equilibrium, which alone just reproduces the prior.
            mu_bl = extra.get("bl_returns")
            if mu_bl is None:
                mu_bl = 2.5 * np.dot(cov, w0)
            mu_bl = np.asarray(mu_bl, dtype=float)

            def neg_bl_sharpe(w):
                r = float(np.dot(w, mu_bl))
                v = port_vol(w)
                return -(r - RF) / v if v > 1e-10 else 0.0
            res = minimize(neg_bl_sharpe, w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-9, "maxiter": 1000})
            out = _build_result(res.x, mu_bl, cov, strategy, RF)
            out["converged"] = bool(getattr(res, "success", False))
            out["solver_message"] = str(getattr(res, "message", ""))
            return out

        else:
            return {"error": f"unknown method '{strategy}'"}

        out = _build_result(res.x, mean_ret, cov, strategy, RF)
        # Reported, not hidden: a non-converged solve returns its best point flagged.
        out["converged"] = bool(getattr(res, "success", False))
        out["solver_message"] = str(getattr(res, "message", ""))
        return out

    except Exception as exc:
        return {"error": str(exc)}


def feasible_start(bounds, n):
    """Equal weights clipped into the bounds and renormalised (SLSQP needs a sane start)."""
    lo = np.array([b[0] for b in bounds], dtype=float)
    hi = np.array([b[1] for b in bounds], dtype=float)
    w = np.clip(np.ones(n) / n, lo, hi)
    for _ in range(50):
        s = w.sum()
        if abs(s - 1.0) < 1e-12:
            break
        free = (w < hi - 1e-12) if s < 1.0 else (w > lo + 1e-12)
        if not free.any():
            break
        w[free] += (1.0 - s) / free.sum()
        w = np.clip(w, lo, hi)
    return w


def _with_caps_note(result, w, extra):
    """Rule-based weights (equal / HRP / inverse variance) ignore the caps: say so."""
    bounds = extra.get("bounds")
    if bounds and any(w[i] > bounds[i][1] + 1e-9 or w[i] < bounds[i][0] - 1e-9 for i in range(len(w))):
        result["caps_violated"] = True
    result["converged"] = True
    return result


def _build_result(weights, mean_ret, cov, strategy, rf=0.04):
    RF   = rf
    w    = np.clip(weights, 0.0, 1.0)
    s    = w.sum()
    if s > 1e-10:
        w = w / s
    r    = float(np.dot(w, mean_ret))
    v    = float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0)))
    sh   = float((r - RF) / v) if v > 1e-10 else None
    return {"weights": w, "return": r, "volatility": v, "sharpe": sh, "strategy": strategy}


# ── Efficient frontier ────────────────────────────────────────────────────────

def build_frontier(mean_ret, cov, n, num_points=40, rf=0.04, bounds=None, ineq=None):
    """(points, failed): {volatility, return, sharpe} for every target whose solve
    CONVERGED (a failed solve is not a frontier point), and how many failed."""
    from scipy.optimize import minimize
    RF = rf

    bounds = bounds or tuple((0.0, 1.0) for _ in range(n))
    base_cons = [{"type": "eq", "fun": lambda w: np.sum(w) - 1.0}] + list(ineq or [])
    w0 = feasible_start(bounds, n)

    def vol(w):
        return float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0)))

    min_vol_res = minimize(vol, w0, method="SLSQP", bounds=bounds, constraints=base_cons,
                           options={"ftol": 1e-9, "maxiter": 1000})
    if not min_vol_res.success:
        return [], num_points
    max_ret_res = minimize(lambda w: -float(np.dot(w, mean_ret)), w0, method="SLSQP", bounds=bounds,
                           constraints=base_cons, options={"ftol": 1e-9, "maxiter": 1000})
    min_ret = float(np.dot(min_vol_res.x, mean_ret))
    max_ret = float(np.dot(max_ret_res.x, mean_ret)) if max_ret_res.success else float(np.max(mean_ret))
    if max_ret <= min_ret:
        return [{"volatility": vol(min_vol_res.x), "return": min_ret,
                 "sharpe": (min_ret - RF) / vol(min_vol_res.x) if vol(min_vol_res.x) > 1e-10 else None}], 0

    points, failed = [], 0
    for tgt in np.linspace(min_ret, max_ret, num_points):
        try:
            cons = base_cons + [{"type": "eq", "fun": lambda w, t=tgt: float(np.dot(w, mean_ret)) - t}]
            res = minimize(vol, w0, method="SLSQP", bounds=bounds, constraints=cons,
                           options={"ftol": 1e-9, "maxiter": 500})
            if not res.success:
                failed += 1
                continue
            v = float(res.fun)
            r = float(np.dot(res.x, mean_ret))
            points.append({"volatility": v, "return": r, "sharpe": float((r - RF) / v) if v > 1e-10 else None})
        except Exception:
            failed += 1
    return points, failed


def build_constraints(params, available):
    """(bounds, inequality constraints, warnings) from max_weight / min_weight / sector caps."""
    n = len(available)
    warnings = []

    def frac(v, default):
        try:
            f = float(v)
        except (TypeError, ValueError):
            return default
        return f if math.isfinite(f) else default

    max_w = frac(params.get("max_weight"), 1.0)
    min_w = frac(params.get("min_weight"), 0.0)
    max_w = min(1.0, max(0.0, max_w))
    min_w = min(max_w, max(0.0, min_w))
    if max_w * n < 1.0 - 1e-9:
        warnings.append(f"max_weight {max_w:.2%} × {n} holdings cannot reach 100% — cap raised to {1.0 / n:.2%}")
        max_w = 1.0 / n
    if min_w * n > 1.0 + 1e-9:
        warnings.append(f"min_weight {min_w:.2%} × {n} holdings exceeds 100% — floor lowered to {1.0 / n:.2%}")
        min_w = 1.0 / n
    bounds = tuple((min_w, max_w) for _ in range(n))

    ineq = []
    sectors = params.get("sectors") if isinstance(params.get("sectors"), dict) else {}
    caps = params.get("sector_caps") if isinstance(params.get("sector_caps"), dict) else {}
    sector_of = {str(k).strip().upper(): str(v) for k, v in sectors.items()}
    for sector, cap in caps.items():
        c = frac(cap, None)
        if c is None or c <= 0:
            continue
        idx = [i for i, s in enumerate(available) if sector_of.get(s) == str(sector)]
        if not idx:
            continue
        if c < min_w * len(idx) - 1e-9:
            warnings.append(f"sector cap {sector} {c:.2%} is below the minimum weights of its holdings — ignored")
            continue
        ineq.append({"type": "ineq", "fun": lambda w, ii=tuple(idx), cc=c: cc - float(np.sum(w[list(ii)]))})
    return bounds, ineq, warnings, max_w, min_w


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    # Support both command-line args (--args <json>) and stdin
    stdin_data = ""
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--args" and i + 1 < len(args):
            stdin_data = args[i + 1]
            i += 2
        elif not args[i].startswith("--") and not stdin_data:
            stdin_data = args[i]
            i += 1
        else:
            i += 1

    if not stdin_data.strip():
        # Fall back to stdin for backwards compatibility
        stdin_data = sys.stdin.read()

    if not stdin_data.strip():
        print(json.dumps({"error": "No input data"}))
        return

    try:
        params = json.loads(stdin_data)
    except Exception as exc:
        print(json.dumps({"error": f"JSON parse error: {exc}"}))
        return

    symbols = [str(s).strip().upper() for s in params.get("symbols", []) if str(s).strip()]
    if not symbols:
        print(json.dumps({"error": "No symbols provided"}))
        return

    # Map C++ method name to internal strategy key
    method_map = {
        "max_sharpe":        "max_sharpe",
        "max sharpe":        "max_sharpe",
        "min_volatility":    "min_volatility",
        "min volatility":    "min_volatility",
        "risk_parity":       "risk_parity",
        "risk parity":       "risk_parity",
        "max_return":        "max_return",
        "max return":        "max_return",
        "equal_weight":      "equal_weight",
        "equal weight":      "equal_weight",
        "hrp":               "hrp",
        "target_return":     "target_return",
        "target return":     "target_return",
        "black_litterman":   "black_litterman",
        "b-l model":         "black_litterman",
        "b_l_model":         "black_litterman",  # "B-L Model" → lower → replace(-,_)
        "b-l_model":         "black_litterman",
        "inv_var":           "inv_var",
    }
    raw_method = str(params.get("method", "max_sharpe")).lower().replace("-", "_")
    strategy   = method_map.get(raw_method, raw_method)

    try:
        rf = float(params.get("risk_free_rate", 0.04))
    except (TypeError, ValueError):
        rf = float("nan")
    if not math.isfinite(rf) or not -0.5 < rf < 1.0:
        print(json.dumps({"error": "risk_free_rate must be an annual fraction between -0.5 and 1 (e.g. 0.04)"}))
        return

    period = str(params.get("period") or "1y").strip().lower()
    if period not in ALLOWED_PERIODS:
        period = "1y"
    base = str(params.get("base_currency") or "USD").strip().upper() or "USD"
    currencies = {str(k).strip().upper(): str(v) for k, v in (params.get("currencies") or {}).items()}
    try:
        returns_df, ppy_cal, warnings = fetch_returns(symbols, period, base, currencies,
                                                      str(params.get("benchmark") or "").strip())
    except SystemExit:
        raise
    except Exception as exc:
        print(json.dumps({"error": f"Could not fetch price data: {exc}"}))
        return
    if returns_df is None:
        print(json.dumps({"error": "No price data for any symbol"}))
        return
    available = list(returns_df.columns)
    if len(returns_df) < 20:
        print(json.dumps({"error": f"Insufficient shared price history ({len(returns_df)} days since the youngest "
                                   "listing; need ≥ 20)"}))
        return
    try:
        ppy_in = float(params.get("periods_per_year"))
        ppy_in = ppy_in if math.isfinite(ppy_in) and ppy_in > 0 else None
    except (TypeError, ValueError):
        ppy_in = None

    # The UI offers these choices; they used to be accepted and ignored.
    def _key(v, default):
        return str(v or default).strip().lower().replace("-", "_").replace(" ", "_")
    returns_method = _key(params.get("returns_method"), "mean_historical")
    if returns_method not in RETURNS_LABELS:
        returns_method = "mean_historical"
    risk_model = _key(params.get("risk_model"), "sample_covariance")
    if risk_model not in RISK_LABELS:
        risk_model = "sample_covariance"

    mean_ret, cov, n, ppy = build_params(returns_df, returns_method, risk_model, rf, ppy_in or ppy_cal)
    bounds, ineq, cwarn, max_w, min_w = build_constraints(params, available)
    warnings += cwarn

    # Current weights over the assets that have data (the BL prior and the "current"
    # point on the frontier).
    in_weights = params.get("weights", [])
    w_cur = np.zeros(n)
    have_cur = isinstance(in_weights, list) and len(in_weights) == len(symbols)
    if have_cur:
        wmap = {symbols[i]: float(in_weights[i]) for i in range(len(symbols))}
        for i in range(n):
            w_cur[i] = max(0.0, wmap.get(available[i], 0.0))
    if w_cur.sum() <= 1e-9:
        have_cur = False
        w_cur = np.ones(n) / n
    else:
        w_cur = w_cur / w_cur.sum()

    bl_pi, bl_posterior, bl_delta = black_litterman_returns(mean_ret, cov, w_cur, rf)
    extra = dict(params, bl_returns=bl_posterior, bounds=bounds, ineq=ineq)

    # ── Primary optimisation ──────────────────────────────────────────────────
    primary = run_strategy(strategy, mean_ret, cov, n, extra=extra, rf=rf)
    if "error" in primary:
        print(json.dumps({"error": primary["error"]}))
        return
    if not primary.get("converged", True):
        warnings.append(f"{strategy}: the optimiser did not converge ({primary.get('solver_message')}) — "
                        "the weights are its best point, not an optimum")
    if primary.get("caps_violated"):
        warnings.append(f"{strategy} is rule-based: its weights do not honour the position / sector caps")
    if primary.get("target_note"):
        warnings.append(primary["target_note"])

    weights_arr  = primary["weights"]
    weights_dict = {available[i]: float(weights_arr[i]) for i in range(n)}

    # ── Efficient frontier ────────────────────────────────────────────────────
    frontier, frontier_failed = [], 0
    try:
        frontier, frontier_failed = build_frontier(mean_ret, cov, n, rf=rf, bounds=bounds, ineq=ineq)
    except Exception as exc:
        warnings.append(f"efficient frontier failed: {exc}")

    # ── Multi-strategy comparison ─────────────────────────────────────────────
    comparison = {}
    for cs in ["max_sharpe", "min_volatility", "risk_parity", "hrp", "equal_weight"]:
        try:
            r = run_strategy(cs, mean_ret, cov, n, extra=extra, rf=rf)
            if "error" not in r:
                w_arr = r["weights"]
                comparison[cs] = {
                    "weights":    {available[i]: float(w_arr[i]) for i in range(n)},
                    "return":     r["return"],
                    "volatility": r["volatility"],
                    "sharpe":     r["sharpe"],
                    "converged":  r.get("converged", True),
                }
        except Exception:
            pass

    # ── Risk decomposition for the primary weights ────────────────────────────
    # Marginal risk contribution MRC = (Σ·w)/σ_p ; risk contribution RC = w·MRC.
    # Σ RC = σ_p, so RC%/asset = RC/σ_p. Reported as % of total portfolio risk.
    risk_contributions, marginal_risk, asset_volatility = {}, {}, {}
    w_p = np.asarray(weights_arr, dtype=float)
    sigma_p = float(np.sqrt(max(np.dot(w_p, np.dot(cov, w_p)), 0.0)))
    cov_w = np.dot(cov, w_p)
    diag = np.sqrt(np.clip(np.diag(cov), 0.0, None))
    for i in range(n):
        sym = available[i]
        asset_volatility[sym] = float(diag[i])
        if sigma_p > 1e-12:
            marginal_risk[sym] = float(cov_w[i] / sigma_p)
            risk_contributions[sym] = float(w_p[i] * cov_w[i] / sigma_p / sigma_p * 100.0)
        else:
            marginal_risk[sym] = None
            risk_contributions[sym] = None

    implied_returns = {available[i]: float(bl_pi[i]) for i in range(n)}
    posterior_returns = {available[i]: float(bl_posterior[i]) for i in range(n)}
    assets = {available[i]: {"return": float(mean_ret[i]), "volatility": float(np.sqrt(max(cov[i, i], 0.0)))}
              for i in range(n)}
    current = None
    if have_cur:
        cur = _build_result(w_cur, mean_ret, cov, "current", rf)
        current = {"return": cur["return"], "volatility": cur["volatility"], "sharpe": cur["sharpe"]}

    output = {
        "weights":                weights_dict,
        "expected_annual_return": primary["return"],
        "annual_volatility":      primary["volatility"],
        "sharpe_ratio":           primary["sharpe"],
        "converged":              primary.get("converged", True),
        "solver_message":         primary.get("solver_message", ""),
        "risk_free_rate":         rf,
        "strategy":               strategy,
        "symbols":                available,
        "symbols_missing":        [s for s in symbols if s not in available],
        "frontier":               frontier,
        "frontier_failed":        frontier_failed,
        "comparison":             comparison,
        "risk_contributions":     risk_contributions,
        "marginal_risk":          marginal_risk,
        "asset_volatility":       asset_volatility,
        "implied_returns":        implied_returns,
        "posterior_returns":      posterior_returns,
        "bl_delta":               bl_delta,
        "assets":                 assets,
        "returns_model":          RETURNS_LABELS[returns_method],
        "risk_model":             RISK_LABELS[risk_model],
        "periods_per_year":       ppy,
        "period":                 period,
        "base_currency":          base,
        "history_start":          returns_df.index[0].strftime("%Y-%m-%d"),
        "observations":           int(len(returns_df)),
        "max_weight":             max_w,
        "min_weight":             min_w,
        "warnings":               warnings,
    }
    if current is not None:
        output["current"] = current

    print(json.dumps(convert_numpy(output)))


if __name__ == "__main__":
    main()
