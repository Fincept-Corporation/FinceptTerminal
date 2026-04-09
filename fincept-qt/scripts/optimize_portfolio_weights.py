"""
optimize_portfolio_weights.py — Portfolio optimization for Fincept Terminal.

Input (stdin JSON):
  {
    "symbols":        ["AAPL", "MSFT", "GOOGL"],
    "weights":        [0.4, 0.35, 0.25],       # current weights (fractions)
    "method":         "max_sharpe",             # see STRATEGIES below
    "returns_method": "mean_historical",        # ignored (scipy only for now)
    "risk_model":     "sample_covariance"       # ignored (scipy only for now)
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
    }
  }

STRATEGIES (method field):
  max_sharpe        — Maximise Sharpe ratio (default)
  min_volatility    — Minimise portfolio standard deviation
  risk_parity       — Equal risk contribution per asset
  max_return        — Maximise expected return (fully invests in best asset)
  equal_weight      — 1/N allocation (no optimisation)
  hrp               — Hierarchical Risk Parity (inverse-variance proxy)
  target_return     — Efficient portfolio at 10% target return
  black_litterman   — BL with equal-weight market prior
"""
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
    return close


# ── Core optimiser ────────────────────────────────────────────────────────────

def build_params(returns_df):
    """Return (mean_returns_annual, cov_annual, n) from a returns DataFrame."""
    mean_ret = returns_df.mean().values * 252
    cov      = returns_df.cov().values * 252
    return mean_ret, cov, len(mean_ret)


def run_strategy(strategy, mean_ret, cov, n, extra=None):
    """
    Returns dict: {weights, return, volatility, sharpe} or {error}.
    weights is a numpy array of length n.
    """
    if extra is None:
        extra = {}
    RF = 0.04

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
            return _build_result(w, mean_ret, cov, strategy)

        elif strategy in ("hrp", "inv_var"):
            diag = np.diag(cov)
            diag = np.where(diag > 1e-12, diag, 1e-12)
            inv_var = 1.0 / diag
            w = inv_var / inv_var.sum()
            return _build_result(w, mean_ret, cov, strategy)

        elif strategy in ("target_return",):
            tgt = extra.get("target_return", 0.10)
            cons = constraints + [{"type": "eq", "fun": lambda w: port_ret(w) - tgt}]
            res = minimize(port_vol, w0, method="SLSQP",
                           bounds=bounds, constraints=cons,
                           options={"ftol": 1e-9, "maxiter": 1000})

        elif strategy == "black_litterman":
            delta = 2.5
            pi    = delta * np.dot(cov, w0)
            def neg_bl_sharpe(w):
                r = float(np.dot(w, pi))
                v = port_vol(w)
                return -(r - RF) / v if v > 1e-10 else 0.0
            res = minimize(neg_bl_sharpe, w0, method="SLSQP",
                           bounds=bounds, constraints=constraints,
                           options={"ftol": 1e-9, "maxiter": 1000})

        else:
            # Fallback: equal weight
            w = np.ones(n) / n
            return _build_result(w, mean_ret, cov, strategy)

        if not getattr(res, "success", False):
            # Still use best-found weights
            pass
        return _build_result(res.x, mean_ret, cov, strategy)

    except Exception as exc:
        return {"error": str(exc)}


def _build_result(weights, mean_ret, cov, strategy):
    RF   = 0.04
    w    = np.clip(weights, 0.0, 1.0)
    s    = w.sum()
    if s > 1e-10:
        w = w / s
    r    = float(np.dot(w, mean_ret))
    v    = float(np.sqrt(max(np.dot(w, np.dot(cov, w)), 0.0)))
    sh   = float((r - RF) / v) if v > 1e-10 else 0.0
    return {"weights": w, "return": r, "volatility": v, "sharpe": sh, "strategy": strategy}


# ── Efficient frontier ────────────────────────────────────────────────────────

def build_frontier(mean_ret, cov, n, num_points=40):
    """Return list of {volatility, return, sharpe} frontier points."""
    from scipy.optimize import minimize
    RF = 0.04

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
    }
    raw_method = params.get("method", "max_sharpe").lower().replace("-", "_")
    strategy   = method_map.get(raw_method, raw_method)

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

    mean_ret, cov, n = build_params(returns_df)

    # ── Primary optimisation ──────────────────────────────────────────────────
    primary = run_strategy(strategy, mean_ret, cov, n, extra=params)
    if "error" in primary:
        print(json.dumps({"error": primary["error"]}))
        return

    weights_arr  = primary["weights"]
    weights_dict = {available[i]: float(weights_arr[i]) for i in range(n)}

    # ── Efficient frontier ────────────────────────────────────────────────────
    frontier = []
    try:
        frontier = build_frontier(mean_ret, cov, n)
    except Exception:
        pass

    # ── Multi-strategy comparison ─────────────────────────────────────────────
    comparison_strategies = [
        "max_sharpe", "min_volatility", "risk_parity", "hrp", "equal_weight"
    ]
    comparison = {}
    for cs in comparison_strategies:
        try:
            r = run_strategy(cs, mean_ret, cov, n)
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

    output = {
        "weights":                weights_dict,
        "expected_annual_return": primary["return"],
        "annual_volatility":      primary["volatility"],
        "sharpe_ratio":           primary["sharpe"],
        "strategy":               strategy,
        "symbols":                available,
        "frontier":               frontier,
        "comparison":             comparison,
    }

    print(json.dumps(convert_numpy(output)))


if __name__ == "__main__":
    main()
