"""
optimize_portfolio_weights.py — Portfolio optimization for Fincept Terminal.

Input (stdin JSON):
  {
    "symbols":        ["AAPL", "MSFT", "GOOGL"],
    "weights":        [0.4, 0.35, 0.25],       # current weights (fractions)
    "method":         "max_sharpe",             # see STRATEGIES below
    "returns_method": "mean_historical",        # mean_historical | ema | capm | james_stein
    "risk_model":     "sample_covariance",      # sample_covariance | ledoit_wolf | semicovariance | exponential
    "risk_free_rate": 0.04                      # annual, fraction; optional, default 0.04
  }

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
        return 0.0 if (v != v or v == float("inf") or v == float("-inf")) else v
    elif isinstance(obj, np.ndarray):
        return [convert_numpy(x) for x in obj]
    elif isinstance(obj, float):
        return 0.0 if (obj != obj or obj == float("inf") or obj == float("-inf")) else obj
    return obj


def fetch_price_data(symbols, period="1y"):
    import yfinance as yf
    import pandas as pd

    data = yf.download(symbols, period=period, interval="1d",
                       progress=False, auto_adjust=True)
    if data is None or data.empty:
        return None

    if isinstance(data.columns, pd.MultiIndex):
        level0 = data.columns.get_level_values(0)
        if "Close" in level0:
            close = data["Close"]
        elif "Adj Close" in level0:
            close = data["Adj Close"]
        else:
            close = data.iloc[:, :len(symbols)]
    else:
        close = data[["Close"]] if "Close" in data.columns else data

    if not isinstance(close, pd.DataFrame):
        close = pd.DataFrame(close)

    if len(symbols) == 1 and list(close.columns) != symbols:
        close.columns = [symbols[0]]

    # Keep only requested symbols
    available = [s for s in symbols if s in close.columns]
    if not available:
        return None
    close = close[available].dropna(how="all")
    # A ticker yfinance could not price (delisted, mutual-fund code, bad suffix) comes
    # back as an all-NaN column. Left in, the main() pct_change().dropna() discards
    # EVERY row and the whole run fails with "Insufficient price history" because of
    # one holding. Drop such columns; main() derives `available` from what is left
    # and reports it in the "symbols" output key.
    close = close.dropna(axis=1, how="all")
    if close.shape[1] == 0:
        return None
    return close


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


def build_params(returns_df, returns_method="mean_historical", risk_model="sample_covariance", rf=0.04):
    """Return (mean_returns_annual, cov_annual, n, periods_per_year) from a returns DataFrame."""
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

    bounds      = tuple((0.0, 1.0) for _ in range(n))
    constraints = [{"type": "eq", "fun": lambda w: np.sum(w) - 1.0}]
    w0          = np.ones(n) / n

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
            return _build_result(w, mean_ret, cov, strategy, RF)

        elif strategy == "hrp":
            # Real HRP (clusters by correlation, then allocates by recursive bisection).
            # It used to be plain inverse-variance weighting under the HRP label.
            return _build_result(hrp_weights(cov), mean_ret, cov, strategy, RF)

        elif strategy == "inv_var":
            diag = np.diag(cov)
            diag = np.where(diag > 1e-12, diag, 1e-12)
            inv_var = 1.0 / diag
            w = inv_var / inv_var.sum()
            return _build_result(w, mean_ret, cov, strategy, RF)

        elif strategy in ("target_return",):
            tgt = extra.get("target_return", 0.10)
            cons = constraints + [{"type": "eq", "fun": lambda w: port_ret(w) - tgt}]
            res = minimize(port_vol, w0, method="SLSQP",
                           bounds=bounds, constraints=cons,
                           options={"ftol": 1e-9, "maxiter": 1000})

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
            return _build_result(res.x, mu_bl, cov, strategy, RF)

        else:
            # Fallback: equal weight
            w = np.ones(n) / n
            return _build_result(w, mean_ret, cov, strategy, RF)

        if not getattr(res, "success", False):
            # Still use best-found weights
            pass
        return _build_result(res.x, mean_ret, cov, strategy, RF)

    except Exception as exc:
        return {"error": str(exc)}


def _build_result(weights, mean_ret, cov, strategy, rf=0.04):
    RF   = rf
    w    = np.clip(weights, 0.0, 1.0)
    s    = w.sum()
    if s > 1e-10:
        w = w / s
    r    = float(np.dot(w, mean_ret))
    v    = float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0)))
    sh   = float((r - RF) / v) if v > 1e-10 else 0.0
    return {"weights": w, "return": r, "volatility": v, "sharpe": sh, "strategy": strategy}


# ── Efficient frontier ────────────────────────────────────────────────────────

def build_frontier(mean_ret, cov, n, num_points=40, rf=0.04):
    """Return list of {volatility, return, sharpe} frontier points."""
    from scipy.optimize import minimize
    RF = rf

    bounds      = tuple((0.0, 1.0) for _ in range(n))
    w0          = np.ones(n) / n

    # Find min-vol and max-return to span the frontier
    min_vol_res = minimize(
        lambda w: float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0))),
        w0, method="SLSQP",
        bounds=bounds,
        constraints=[{"type": "eq", "fun": lambda w: np.sum(w) - 1.0}],
        options={"ftol": 1e-9, "maxiter": 1000},
    )
    min_ret = float(np.dot(min_vol_res.x, mean_ret))
    max_ret = float(np.max(mean_ret))          # upper bound: all-in best asset

    targets = np.linspace(min_ret, max_ret * 0.95, num_points)
    points  = []

    for tgt in targets:
        try:
            cons = [
                {"type": "eq", "fun": lambda w: np.sum(w) - 1.0},
                {"type": "eq", "fun": lambda w, t=tgt: float(np.dot(w, mean_ret)) - t},
            ]
            res = minimize(
                lambda w: float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0))),
                w0, method="SLSQP",
                bounds=bounds, constraints=cons,
                options={"ftol": 1e-9, "maxiter": 500},
            )
            if res.success or res.fun < 1.0:
                vol = float(res.fun)
                ret = float(np.dot(res.x, mean_ret))
                sh  = float((ret - RF) / vol) if vol > 1e-10 else 0.0
                points.append({"volatility": vol, "return": ret, "sharpe": sh})
        except Exception:
            pass

    return points


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

    symbols = params.get("symbols", [])
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
    }
    raw_method = params.get("method", "max_sharpe").lower().replace("-", "_")
    strategy   = method_map.get(raw_method, raw_method)

    # Annual risk-free rate (fraction) for every Sharpe ratio and the max-Sharpe
    # objective. It was hard-coded to 0.04, so a caller's rate (the workflow
    # optimisation node sends one) was silently ignored.
    try:
        rf = float(params.get("risk_free_rate", 0.04))
    except (TypeError, ValueError):
        rf = float("nan")
    if not math.isfinite(rf) or not -0.5 < rf < 1.0:
        print(json.dumps({"error": "risk_free_rate must be an annual fraction between -0.5 and 1 (e.g. 0.04)"}))
        return

    close = fetch_price_data(symbols)
    if close is None:
        print(json.dumps({"error": "Could not fetch price data"}))
        return

    available = [s for s in symbols if s in close.columns]
    if not available:
        print(json.dumps({"error": "No price data for any symbol"}))
        return

    returns_df = close[available].pct_change().dropna()
    if len(returns_df) < 20:
        print(json.dumps({"error": "Insufficient price history (need ≥ 20 trading days)"}))
        return

    # The UI offers these choices; they used to be accepted and ignored.
    def _key(v, default):
        return str(v or default).strip().lower().replace("-", "_").replace(" ", "_")
    returns_method = _key(params.get("returns_method"), "mean_historical")
    if returns_method not in RETURNS_LABELS:
        returns_method = "mean_historical"
    risk_model = _key(params.get("risk_model"), "sample_covariance")
    if risk_model not in RISK_LABELS:
        risk_model = "sample_covariance"

    mean_ret, cov, n, ppy = build_params(returns_df, returns_method, risk_model, rf)

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

    # ── Primary optimisation ──────────────────────────────────────────────────
    primary = run_strategy(strategy, mean_ret, cov, n, extra=dict(params, bl_returns=bl_posterior), rf=rf)
    if "error" in primary:
        print(json.dumps({"error": primary["error"]}))
        return

    weights_arr  = primary["weights"]
    weights_dict = {available[i]: float(weights_arr[i]) for i in range(n)}

    # ── Efficient frontier ────────────────────────────────────────────────────
    frontier = []
    try:
        frontier = build_frontier(mean_ret, cov, n, rf=rf)
    except Exception:
        pass

    # ── Multi-strategy comparison ─────────────────────────────────────────────
    comparison_strategies = [
        "max_sharpe", "min_volatility", "risk_parity", "hrp", "equal_weight"
    ]
    comparison = {}
    for cs in comparison_strategies:
        try:
            r = run_strategy(cs, mean_ret, cov, n, rf=rf)
            if "error" not in r:
                w_arr = r["weights"]
                comparison[cs] = {
                    "weights":    {available[i]: float(w_arr[i]) for i in range(n)},
                    "return":     r["return"],
                    "volatility": r["volatility"],
                    "sharpe":     r["sharpe"],
                }
        except Exception:
            pass

    # ── Risk decomposition for the primary weights ────────────────────────────
    # Marginal risk contribution MRC = (Σ·w)/σ_p ; risk contribution RC = w·MRC.
    # Σ RC = σ_p, so RC%/asset = RC/σ_p. Reported as % of total portfolio risk.
    risk_contributions = {}
    marginal_risk = {}
    asset_volatility = {}
    try:
        w_p = np.asarray(weights_arr, dtype=float)
        sigma_p = float(np.sqrt(max(np.dot(w_p, np.dot(cov, w_p)), 0.0)))
        cov_w = np.dot(cov, w_p)
        mrc = cov_w / sigma_p if sigma_p > 1e-12 else np.zeros(n)
        rc = w_p * mrc
        diag = np.sqrt(np.clip(np.diag(cov), 0.0, None))
        for i in range(n):
            sym = available[i]
            asset_volatility[sym] = float(diag[i])               # annualised vol
            marginal_risk[sym] = float(mrc[i])
            risk_contributions[sym] = float(rc[i] / sigma_p * 100.0) if sigma_p > 1e-12 else 0.0
    except Exception:
        pass

    # ── Black-Litterman: equilibrium prior and posterior ──────────────────────
    # pi = delta·Sigma·w_current — the returns at which today's mix would be optimal;
    # the posterior blends it with the historical-return views (see
    # black_litterman_returns). The B-L method optimises on the posterior.
    implied_returns = {available[i]: float(bl_pi[i]) for i in range(n)}
    posterior_returns = {available[i]: float(bl_posterior[i]) for i in range(n)}

    # Each holding on its own, and today's mix, on the same model — for the frontier chart.
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
        "risk_free_rate":         rf,
        "strategy":               strategy,
        "symbols":                available,
        "frontier":               frontier,
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
    }
    if current is not None:
        output["current"] = current

    print(json.dumps(convert_numpy(output)))


if __name__ == "__main__":
    main()
