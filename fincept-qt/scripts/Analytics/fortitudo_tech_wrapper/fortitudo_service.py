"""
Fortitudo Service (risk-aware portfolio optimization)
======================================================
Self-contained portfolio risk + optimization service for the Fincept Terminal
"Fortitudo" sub-tab. Same response contract as gs_quant_service /
functime_service / statsmodels_service.

Backed by the in-tree fortitudo_tech_wrapper modules (functions.py,
optimization.py) — those wrap the fortitudo.tech library when present and
fall back to numpy implementations otherwise. The legacy entry point is
preserved as fortitudo_service_legacy.py.

Response contract (all ops):
    success: bool
    operation: str
    data: dict   (when success)
    error: str   (when failure)
    error_kind: str | None    (validation | runtime | unknown_op)
    traceback: str | None     (only on uncaught exceptions)
"""

import sys
import os
import json
import math
import traceback
from typing import Dict, List, Any, Optional

import numpy as np
import pandas as pd

# Add this module's directory to path so we can import sibling files.
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

# Wrapper modules (optimization.py + functions.py + option_pricing.py).
# These exist in this directory; they shadow / wrap fortitudo.tech.
try:
    from optimization import MeanVarianceOptimizer, MeanCVaROptimizer
    OPTIMIZATION_OK = True
except ImportError as e:
    OPTIMIZATION_OK = False
    _opt_err = str(e)

try:
    from functions import (
        calculate_all_metrics,
        calculate_covariance_matrix,
        calculate_correlation_matrix,
        calculate_simulation_moments,
        calculate_exp_decay_probabilities,
    )
    FUNCTIONS_OK = True
except ImportError as e:
    FUNCTIONS_OK = False
    _fn_err = str(e)

# Optional: detect native fortitudo.tech for status reporting only.
try:
    import fortitudo.tech as ft  # noqa: F401
    NATIVE_OK = True
    NATIVE_VERSION = getattr(ft, "__version__", "unknown")
except ImportError:
    NATIVE_OK = False
    NATIVE_VERSION = None


# ============================================================================
# INPUT COERCION + VALIDATION
# ============================================================================

class ValidationError(ValueError):
    pass


def _coerce_floats(value, field_name):
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        out = []
        for i, v in enumerate(value):
            try:
                out.append(float(v))
            except (TypeError, ValueError):
                raise ValidationError(f"{field_name}[{i}] is not numeric: {v!r}")
        return out
    if isinstance(value, np.ndarray):
        return [float(v) for v in value.tolist()]
    if isinstance(value, str):
        s = value.strip()
        if not s:
            return []
        for sep in [",", ";", "\t", "\n"]:
            s = s.replace(sep, " ")
        parts = [p for p in s.split(" ") if p.strip()]
        out = []
        for i, p in enumerate(parts):
            try:
                out.append(float(p))
            except ValueError:
                raise ValidationError(f"{field_name}[{i}] is not numeric: {p!r}")
        return out
    try:
        return [float(value)]
    except (TypeError, ValueError):
        raise ValidationError(f"{field_name} is not numeric: {value!r}")


def _safe_float(v, default=0.0):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return default
    if math.isnan(f) or math.isinf(f):
        return default
    return f


def _safe_int(v, default=0):
    try:
        f = float(v)
        if math.isnan(f) or math.isinf(f):
            return default
        return int(f)
    except (TypeError, ValueError):
        return default


def _normalize_returns_input(data):
    """
    Resolve `returns` into a pd.DataFrame (n_scenarios x n_assets).

    Accepted forms:
      1. dict {asset: [r1, r2, ...]}            (preferred wire format from C++)
      2. dict {asset: {date: r}}                (legacy)
      3. list of dicts [{asset: r, asset2: r}]  (rare)
      4. tickers: "AAPL,MSFT,GOOG" (in `tickers` field, fetched via yfinance)

    Also accepts a `tickers` field that triggers yfinance fetch.
    Returns (DataFrame, asset_names).
    """
    tickers = data.get("tickers")
    period = str(data.get("period", "1y"))
    returns = data.get("returns")

    if tickers and not returns:
        try:
            import yfinance as yf
        except ImportError:
            raise ValidationError("yfinance not installed; supply `returns` instead of `tickers`")
        tk_list = [t.strip().upper() for t in str(tickers).split(",") if t.strip()]
        if not tk_list:
            raise ValidationError("`tickers` is empty")
        if len(tk_list) < 2:
            raise ValidationError(
                "Need at least 2 tickers for portfolio analysis; got 1")
        raw = yf.download(tk_list, period=period, auto_adjust=True, progress=False)
        if raw.empty:
            raise ValidationError(f"yfinance returned no data for {tk_list} (period={period})")
        if isinstance(raw.columns, pd.MultiIndex):
            close = raw["Close"] if "Close" in raw.columns.get_level_values(0) else raw
        else:
            close = raw["Close"] if "Close" in raw.columns else raw
        if isinstance(close, pd.Series):
            close = close.to_frame(name=tk_list[0])
        rets = close.pct_change(fill_method=None).dropna(how="any")
        if rets.empty:
            raise ValidationError(f"No usable price history for {tk_list}")
        # Reorder columns to match requested ticker order if shape matches
        if rets.shape[1] == len(tk_list) and set(rets.columns) >= set(tk_list):
            rets = rets[tk_list]
        return rets, list(rets.columns)

    if returns is None:
        raise ValidationError("`returns` (or `tickers`) is required")

    if isinstance(returns, dict):
        # Sniff first value
        first_val = next(iter(returns.values()), None)
        if isinstance(first_val, dict):
            # {asset: {date: r}}
            df = pd.DataFrame(returns)
            try:
                df.index = pd.to_datetime(df.index)
            except Exception:
                pass
        else:
            # {asset: [r1, r2, ...]}
            cleaned = {asset: _coerce_floats(vals, f"returns[{asset}]")
                       for asset, vals in returns.items()}
            lengths = [len(v) for v in cleaned.values()]
            if not lengths:
                raise ValidationError("`returns` is empty")
            if len(set(lengths)) > 1:
                raise ValidationError(
                    f"all return columns must have the same length, got {lengths}")
            df = pd.DataFrame(cleaned)
        if df.shape[1] < 2:
            raise ValidationError(
                f"Need at least 2 assets for portfolio analysis; got {df.shape[1]}")
        return df, list(df.columns)

    if isinstance(returns, list):
        # list of dicts
        df = pd.DataFrame(returns)
        if df.shape[1] < 2:
            raise ValidationError(
                f"Need at least 2 assets for portfolio analysis; got {df.shape[1]}")
        return df, list(df.columns)

    raise ValidationError(f"Unsupported `returns` format: {type(returns).__name__}")


def _require_modules():
    missing = []
    if not OPTIMIZATION_OK:
        missing.append(f"optimization ({_opt_err})")
    if not FUNCTIONS_OK:
        missing.append(f"functions ({_fn_err})")
    if missing:
        raise ValidationError("required wrapper modules failed to import: " + "; ".join(missing))


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_check_status(_data):
    return {
        "wrappers_optimization": OPTIMIZATION_OK,
        "wrappers_functions": FUNCTIONS_OK,
        "native_fortitudo_tech": NATIVE_OK,
        "native_version": NATIVE_VERSION,
        "mode": "native" if NATIVE_OK else "wrapper-fallback",
        "ops_available": list(OPERATIONS.keys()),
    }


def op_portfolio_metrics(data):
    """
    Compute risk metrics for a weighted portfolio.
    Inputs:
      returns | tickers : see _normalize_returns_input
      weights           : list[float] (must sum ~ 1.0; renormalized if not)
      alpha             : float       (CVaR confidence tail, default 0.05)
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    n_assets = len(assets)

    weights_raw = data.get("weights")
    if weights_raw is None:
        weights = np.full(n_assets, 1.0 / n_assets)
    else:
        weights = np.asarray(_coerce_floats(weights_raw, "weights"), dtype=float)
        if len(weights) != n_assets:
            raise ValidationError(
                f"weights ({len(weights)}) must match number of assets ({n_assets})")
        s = float(weights.sum())
        if s <= 0:
            raise ValidationError("weights sum must be > 0")
        weights = weights / s  # renormalize to 1.0

    alpha = _safe_float(data.get("alpha", 0.05))
    if not (0.0 < alpha < 0.5):
        raise ValidationError(f"alpha must be in (0, 0.5), got {alpha}")

    metrics = calculate_all_metrics(weights, rets_df, None, alpha)

    # Project metrics into a flat, predictable shape
    expected_return = _safe_float(metrics.get("expected_return"))
    volatility = _safe_float(metrics.get("volatility"))
    sharpe = _safe_float(metrics.get("sharpe_ratio"))
    var = _safe_float(metrics.get("var"))
    cvar = _safe_float(metrics.get("cvar"))

    # Annualized projections (assume daily returns -> sqrt(252) for vol)
    ann_return = (1.0 + expected_return) ** 252 - 1.0
    ann_vol = volatility * math.sqrt(252)

    # Per-asset weight rows
    weight_rows = [
        {"asset": str(assets[i]), "weight": _safe_float(weights[i]),
         "weight_pct": _safe_float(weights[i] * 100.0)}
        for i in range(n_assets)
    ]

    return {
        "n_assets": n_assets,
        "n_scenarios": int(len(rets_df)),
        "alpha": alpha,
        "weights": weight_rows,
        "expected_return": expected_return,
        "volatility": volatility,
        "sharpe_ratio": sharpe,
        "annualized_return": _safe_float(ann_return),
        "annualized_volatility": _safe_float(ann_vol),
        "var": var,
        "cvar": cvar,
        "var_pct": _safe_float(var * 100.0),
        "cvar_pct": _safe_float(cvar * 100.0),
        "max_weight": _safe_float(float(weights.max())),
        "min_weight": _safe_float(float(weights.min())),
        "concentration_hhi": _safe_float(float((weights ** 2).sum())),
    }


def op_covariance_matrix(data):
    """
    Compute covariance + correlation matrix and per-asset moments.
    Inputs:
      returns | tickers : see _normalize_returns_input
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    n = len(assets)

    cov = calculate_covariance_matrix(rets_df, None)
    corr = calculate_correlation_matrix(rets_df, None)
    moments = calculate_simulation_moments(rets_df, None)

    def _to_grid(df):
        # Return a list-of-rows dict with clean integer cells for the wire
        if not isinstance(df, pd.DataFrame):
            return []
        rows = []
        n_rows = df.shape[0]
        n_cols = df.shape[1]
        for r in range(n_rows):
            row = {"asset": str(assets[r]) if r < n else f"x{r}"}
            for c in range(n_cols):
                col_label = str(assets[c]) if c < n else f"x{c}"
                row[col_label] = _safe_float(float(df.iloc[r, c]))
            rows.append(row)
        return rows

    moments_rows = []
    if isinstance(moments, pd.DataFrame):
        for r in range(moments.shape[0]):
            asset = str(moments.index[r])
            row = {"asset": asset}
            for c in range(moments.shape[1]):
                col = str(moments.columns[c]).lower()
                row[col] = _safe_float(float(moments.iloc[r, c]))
            moments_rows.append(row)

    # Average off-diagonal correlation as a single-number summary
    if isinstance(corr, pd.DataFrame) and corr.shape[0] >= 2:
        cv = corr.values
        mask = ~np.eye(cv.shape[0], dtype=bool)
        avg_off_diag = float(cv[mask].mean())
        max_off_diag = float(cv[mask].max())
        min_off_diag = float(cv[mask].min())
    else:
        avg_off_diag = max_off_diag = min_off_diag = 0.0

    return {
        "n_assets": n,
        "n_observations": int(len(rets_df)),
        "assets": assets,
        "covariance_matrix": _to_grid(cov),
        "correlation_matrix": _to_grid(corr),
        "moments": moments_rows,
        "avg_off_diag_correlation": _safe_float(avg_off_diag),
        "max_off_diag_correlation": _safe_float(max_off_diag),
        "min_off_diag_correlation": _safe_float(min_off_diag),
    }


def _opt_result_to_payload(result, assets):
    """Normalize an optimizer result dict into our display schema."""
    if not isinstance(result, dict) or not result.get("success", True):
        raise ValidationError(result.get("error", "optimizer returned a non-success payload"))
    weights_raw = result.get("weights", {})
    if isinstance(weights_raw, dict):
        weight_rows = [
            {"asset": str(k), "weight": _safe_float(v),
             "weight_pct": _safe_float(float(v) * 100.0)}
            for k, v in weights_raw.items()
        ]
    elif isinstance(weights_raw, (list, np.ndarray)):
        weight_rows = [
            {"asset": str(assets[i]) if i < len(assets) else f"x{i}",
             "weight": _safe_float(weights_raw[i]),
             "weight_pct": _safe_float(float(weights_raw[i]) * 100.0)}
            for i in range(len(weights_raw))
        ]
    else:
        weight_rows = []
    weights_arr = np.asarray([w["weight"] for w in weight_rows], dtype=float)
    out = {
        "weights": weight_rows,
        "expected_return": _safe_float(result.get("expected_return")),
        "annualized_return": _safe_float((1.0 + _safe_float(result.get("expected_return"))) ** 252 - 1.0),
        "volatility": _safe_float(result.get("volatility")),
        "annualized_volatility": _safe_float(_safe_float(result.get("volatility")) * math.sqrt(252)),
        "sharpe_ratio": _safe_float(result.get("sharpe_ratio")),
        "variance": _safe_float(result.get("variance")),
        "var": _safe_float(result.get("var")),
        "cvar": _safe_float(result.get("cvar")),
        "alpha": _safe_float(result.get("alpha", 0.0)),
        "n_assets": len(weight_rows),
        "max_weight": _safe_float(float(weights_arr.max()) if len(weights_arr) else 0.0),
        "min_weight": _safe_float(float(weights_arr.min()) if len(weights_arr) else 0.0),
        "concentration_hhi": _safe_float(float((weights_arr ** 2).sum()) if len(weights_arr) else 0.0),
    }
    return out


def op_mean_variance_optimize(data):
    """
    Mean-Variance portfolio optimization.
    Inputs:
      returns | tickers : returns matrix
      objective         : 'min_variance' | 'max_sharpe' | 'target_return'  (default 'min_variance')
      target_return     : float (required if objective='target_return')
      long_only         : bool  (default True)
      max_weight        : float | null (default None)
      min_weight        : float | null (default None)
      risk_free_rate    : float (default 0.0)
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    objective = str(data.get("objective", "min_variance")).lower()
    long_only = bool(data.get("long_only", True))
    max_weight = data.get("max_weight")
    min_weight = data.get("min_weight")
    rf = _safe_float(data.get("risk_free_rate", 0.0))

    optimizer = MeanVarianceOptimizer(rets_df, risk_free_rate=rf)

    if objective == "min_variance":
        raw = optimizer.minimum_variance(long_only=long_only,
                                         max_weight=max_weight, min_weight=min_weight)
    elif objective == "max_sharpe":
        raw = optimizer.max_sharpe(long_only=long_only, max_weight=max_weight)
    elif objective == "target_return":
        target = data.get("target_return")
        if target is None:
            raise ValidationError("target_return is required when objective='target_return'")
        raw = optimizer.target_return(target=_safe_float(target),
                                       long_only=long_only, max_weight=max_weight)
    else:
        raise ValidationError(
            f"unknown objective {objective!r}; expected: min_variance, max_sharpe, target_return")

    payload = _opt_result_to_payload(raw, assets)
    payload["objective"] = objective
    payload["long_only"] = long_only
    payload["risk_free_rate"] = rf
    return payload


def op_mean_cvar_optimize(data):
    """
    Mean-CVaR portfolio optimization.
    Inputs:
      returns | tickers : returns matrix
      objective         : 'min_cvar' | 'target_return'  (default 'min_cvar')
      target_return     : float (required if objective='target_return')
      alpha             : float (CVaR tail, default 0.05)
      long_only         : bool
      max_weight        : float | null
      min_weight        : float | null
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    objective = str(data.get("objective", "min_cvar")).lower()
    alpha = _safe_float(data.get("alpha", 0.05))
    long_only = bool(data.get("long_only", True))
    max_weight = data.get("max_weight")
    min_weight = data.get("min_weight")

    if not (0.0 < alpha < 0.5):
        raise ValidationError(f"alpha must be in (0, 0.5), got {alpha}")

    optimizer = MeanCVaROptimizer(rets_df, alpha=alpha)

    if objective == "min_cvar":
        raw = optimizer.minimum_cvar(long_only=long_only,
                                      max_weight=max_weight, min_weight=min_weight)
    elif objective == "target_return":
        target = data.get("target_return")
        if target is None:
            raise ValidationError("target_return is required when objective='target_return'")
        raw = optimizer.target_return(target=_safe_float(target),
                                       long_only=long_only, max_weight=max_weight)
    else:
        raise ValidationError(
            f"unknown objective {objective!r}; expected: min_cvar, target_return")

    payload = _opt_result_to_payload(raw, assets)
    payload["objective"] = objective
    payload["long_only"] = long_only
    payload["alpha"] = alpha
    return payload


def op_efficient_frontier(data):
    """
    Mean-Variance efficient frontier with N points.
    Inputs:
      returns | tickers : returns matrix
      n_points          : int (default 20, clamped to [5, 50])
      long_only         : bool (default True)
      max_weight        : float | null
      risk_free_rate    : float (default 0.0)
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    n_points = max(5, min(50, _safe_int(data.get("n_points", 20))))
    long_only = bool(data.get("long_only", True))
    max_weight = data.get("max_weight")
    rf = _safe_float(data.get("risk_free_rate", 0.0))

    optimizer = MeanVarianceOptimizer(rets_df, risk_free_rate=rf)
    raw = optimizer.efficient_frontier(n_points=n_points, long_only=long_only,
                                        max_weight=max_weight)

    if not isinstance(raw, dict) or not raw.get("success", True):
        raise ValidationError(raw.get("error", "efficient_frontier returned non-success"))

    frontier = raw.get("frontier", []) or []
    points = []
    for pt in frontier:
        ws = pt.get("weights", {})
        if isinstance(ws, dict):
            weight_rows = [{"asset": str(k), "weight": _safe_float(v)} for k, v in ws.items()]
        else:
            weight_rows = [
                {"asset": str(assets[i]) if i < len(assets) else f"x{i}",
                 "weight": _safe_float(ws[i])}
                for i in range(len(ws))
            ]
        points.append({
            "expected_return": _safe_float(pt.get("expected_return")),
            "annualized_return": _safe_float((1.0 + _safe_float(pt.get("expected_return"))) ** 252 - 1.0),
            "volatility": _safe_float(pt.get("volatility")),
            "annualized_volatility": _safe_float(_safe_float(pt.get("volatility")) * math.sqrt(252)),
            "sharpe_ratio": _safe_float(pt.get("sharpe_ratio")),
            "weights": weight_rows,
        })

    # Identify special points
    min_var_pt = min(points, key=lambda p: p["volatility"]) if points else None
    max_sharpe_pt = max(points, key=lambda p: p["sharpe_ratio"]) if points else None

    return {
        "n_points": len(points),
        "n_assets": len(assets),
        "assets": assets,
        "long_only": long_only,
        "risk_free_rate": rf,
        "frontier": points,
        "min_volatility": _safe_float(min(p["volatility"] for p in points) if points else 0.0),
        "max_volatility": _safe_float(max(p["volatility"] for p in points) if points else 0.0),
        "min_return": _safe_float(min(p["expected_return"] for p in points) if points else 0.0),
        "max_return": _safe_float(max(p["expected_return"] for p in points) if points else 0.0),
        "best_sharpe": _safe_float(max(p["sharpe_ratio"] for p in points) if points else 0.0),
        "min_var_index": points.index(min_var_pt) if min_var_pt else -1,
        "max_sharpe_index": points.index(max_sharpe_pt) if max_sharpe_pt else -1,
    }


def op_exp_decay_probabilities(data):
    """
    Compute exponentially-decayed scenario probabilities for time-weighted analysis.
    Inputs:
      returns | tickers : returns matrix (only the row count matters)
      half_life         : int (in observations, default 60)
    """
    _require_modules()
    rets_df, _assets = _normalize_returns_input(data)
    half_life = max(1, _safe_int(data.get("half_life", 60)))

    probs = calculate_exp_decay_probabilities(rets_df, half_life)
    probs_arr = np.asarray(probs).flatten()

    # Build a sampled (date, weight) curve for the wire — keep it under ~300 rows
    n = len(probs_arr)
    step = max(1, n // 300)
    rows = []
    if isinstance(rets_df.index, pd.DatetimeIndex):
        for i, p in enumerate(probs_arr):
            if i % step == 0:
                rows.append({"date": str(rets_df.index[i])[:10], "weight": _safe_float(float(p))})
    else:
        for i, p in enumerate(probs_arr):
            if i % step == 0:
                rows.append({"index": int(i), "weight": _safe_float(float(p))})

    # Effective sample size (Kish): ESS = (sum p)^2 / sum p^2
    s = float(probs_arr.sum())
    s2 = float((probs_arr ** 2).sum())
    ess = (s * s) / s2 if s2 > 0 else 0.0

    return {
        "n_scenarios": n,
        "half_life": half_life,
        "sum": _safe_float(s),
        "first_weight": _safe_float(float(probs_arr[0])),
        "last_weight": _safe_float(float(probs_arr[-1])),
        "max_weight": _safe_float(float(probs_arr.max())),
        "min_weight": _safe_float(float(probs_arr.min())),
        "weight_ratio_last_to_first": _safe_float(
            float(probs_arr[-1] / probs_arr[0]) if probs_arr[0] > 0 else 0.0),
        "effective_sample_size": _safe_float(ess),
        "ess_pct_of_n": _safe_float((ess / n) * 100.0) if n > 0 else 0.0,
        "weights": rows,
    }


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "check_status": op_check_status,
    "portfolio_metrics": op_portfolio_metrics,
    "covariance_matrix": op_covariance_matrix,
    "mean_variance_optimize": op_mean_variance_optimize,
    "mean_cvar_optimize": op_mean_cvar_optimize,
    "efficient_frontier": op_efficient_frontier,
    "exp_decay_probabilities": op_exp_decay_probabilities,
}


def dispatch(operation, data):
    handler = OPERATIONS.get(operation)
    if handler is None:
        return {
            "success": False, "operation": operation,
            "error": f"Unknown operation: {operation}",
            "error_kind": "unknown_op",
            "available": list(OPERATIONS.keys()),
        }
    try:
        result = handler(data or {})
        return {"success": True, "operation": operation, "data": result}
    except ValidationError as e:
        return {"success": False, "operation": operation, "error": str(e),
                "error_kind": "validation"}
    except Exception as e:
        return {"success": False, "operation": operation, "error": str(e),
                "error_kind": "runtime", "traceback": traceback.format_exc()}


def main(args):
    if len(args) < 1:
        print(json.dumps({
            "success": False,
            "error": "Usage: fortitudo_service.py <operation> [json_data]",
            "error_kind": "usage",
        }))
        return
    operation = args[0]
    data = {}
    if len(args) > 1:
        try:
            data = json.loads(args[1])
        except json.JSONDecodeError as e:
            print(json.dumps({
                "success": False, "operation": operation,
                "error": f"Invalid JSON: {e}", "error_kind": "validation",
            }))
            return
    result = dispatch(operation, data)
    print(json.dumps(result, default=str))


if __name__ == "__main__":
    main(sys.argv[1:])
