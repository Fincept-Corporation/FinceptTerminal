"""
Fortitudo Service (risk-aware portfolio optimization)
======================================================
Self-contained portfolio risk + optimization service for the Fincept Terminal
"Fortitudo" sub-tab. Same response contract as gs_quant_service /
functime_service / statsmodels_service.

Backed by the in-tree fortitudo_tech_wrapper modules (functions.py,
optimization.py) — those wrap the fortitudo.tech library when present and
fall back to numpy implementations otherwise.

CLI: python fortitudo_service.py <operation> '<json params>'   (prints one JSON object)
Operations: check_status, portfolio_metrics, covariance_matrix, mean_variance_optimize,
            mean_cvar_optimize, efficient_frontier, exp_decay_probabilities

Response contract (all ops):
    success: bool
    operation: str
    data: dict   (when success)
    error: str   (when failure)
    error_kind: str | None    (validation | runtime | unknown_op)
    warning: str              (optional; shown by the panel as a banner)
Tracebacks of uncaught exceptions go to stderr. Values that cannot be computed are
null, never 0.0. Units: inputs are daily returns; `alpha` is the tail probability
(0.05 = 95 %); risk_free_rate / target_return are per-period (daily) decimals;
sharpe_ratio fields are annualised (x sqrt(252)), *_daily fields are per period;
var / cvar are the tail RETURN (negative = loss), see VAR_CONVENTION.
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

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(_script_dir)))
    import fincept_json

PERIODS_PER_YEAR = 252   # inputs are daily returns (yfinance daily closes or the caller's daily series)
MIN_OBSERVATIONS = 30    # below this, means / covariances / tail quantiles are noise

# Wrapper modules (optimization.py + functions.py).
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
        calculate_portfolio_var,
        calculate_portfolio_cvar,
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
    try:
        from importlib.metadata import version as _pkg_version
        NATIVE_VERSION = _pkg_version("fortitudo.tech")
    except Exception:
        NATIVE_VERSION = getattr(ft, "__version__", None)
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


def _safe_float(v, default=None):
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
            import fincept_yahoo as fy  # shared Yahoo session + on-disk bar cache
        except ImportError:
            raise ValidationError("yfinance not installed; supply `returns` instead of `tickers`")
        tk_list = [t.strip().upper() for t in str(tickers).split(",") if t.strip()]
        if not tk_list:
            raise ValidationError("`tickers` is empty")
        if len(tk_list) < 2:
            raise ValidationError(
                "Need at least 2 tickers for portfolio analysis; got 1")
        raw = fy.download(tk_list, period=period, auto_adjust=True)
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
        return _require_history(rets), list(rets.columns)

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
        return _require_history(df), list(df.columns)

    if isinstance(returns, list):
        # list of dicts
        df = pd.DataFrame(returns)
        if df.shape[1] < 2:
            raise ValidationError(
                f"Need at least 2 assets for portfolio analysis; got {df.shape[1]}")
        return _require_history(df), list(df.columns)

    raise ValidationError(f"Unsupported `returns` format: {type(returns).__name__}")


def _require_history(df):
    """Numeric, complete rows only, and enough of them to estimate anything."""
    df = df.apply(pd.to_numeric, errors="coerce").dropna(how="any")
    if len(df) < MIN_OBSERVATIONS:
        raise ValidationError(
            f"need at least {MIN_OBSERVATIONS} complete return observations per asset, got {len(df)}")
    return df


VAR_CONVENTION = ("var / cvar are the portfolio return at the alpha tail (negative = loss), "
                  "per period of the input data; var_pct / cvar_pct are the same in percent")


def _parse_alpha(data):
    """`alpha` is the TAIL probability (0.05 = 95 % VaR / CVaR)."""
    alpha = _safe_float(data.get("alpha", 0.05))
    if alpha is None or not (0.0 < alpha < 0.5):
        raise ValidationError(f"alpha (tail probability) must be in (0, 0.5), got {data.get('alpha')}")
    return alpha


def _annualize_return(r):
    """Daily arithmetic mean -> compounded annual return."""
    if r is None or not math.isfinite(r) or r <= -1.0:
        return None
    return (1.0 + r) ** PERIODS_PER_YEAR - 1.0


def _annualize_sharpe(s):
    if s is None or not math.isfinite(s):
        return None
    return s * math.sqrt(PERIODS_PER_YEAR)


def _weights_vector(result_weights, assets):
    """Optimizer weights (dict by asset name or list) -> array in `assets` order."""
    if isinstance(result_weights, dict):
        return np.asarray([float(result_weights.get(a, 0.0)) for a in assets], dtype=float)
    return np.asarray(result_weights, dtype=float)


def _portfolio_figures(w, rets_df, alpha, rf):
    """Every per-portfolio number the optimiser tabs show, computed from the weights."""
    R = rets_df.values.astype(float)
    mean = R.mean(axis=0)
    cov = np.atleast_2d(np.cov(R, rowvar=False))
    er = float(w @ mean)
    variance = float(w @ cov @ w)
    vol = math.sqrt(max(variance, 0.0))
    sharpe_daily = (er - rf) / vol if vol > 0 else None
    var_loss = calculate_portfolio_var(w, rets_df, None, alpha)
    cvar_loss = calculate_portfolio_cvar(w, rets_df, None, alpha)
    return {
        "expected_return": er,
        "annualized_return": _annualize_return(er),
        "volatility": vol,
        "annualized_volatility": vol * math.sqrt(PERIODS_PER_YEAR),
        "variance": variance,
        "sharpe_ratio": _annualize_sharpe(sharpe_daily),
        "sharpe_ratio_daily": sharpe_daily,
        "var": -float(var_loss),
        "cvar": -float(cvar_loss),
        "alpha": alpha,
        "var_convention": VAR_CONVENTION,
    }


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

    alpha = _parse_alpha(data)
    rf = _safe_float(data.get("risk_free_rate", 0.0), 0.0)  # per period (daily), like the optimisers

    metrics = calculate_all_metrics(weights, rets_df, None, alpha)

    expected_return = float(metrics["expected_return"])
    volatility = float(metrics["volatility"])
    # calculate_all_metrics returns VaR / CVaR as positive losses; the wire format is
    # the return at the tail (negative = loss), which the panel colours red below -2 %.
    var = -float(metrics["var"])
    cvar = -float(metrics["cvar"])
    sharpe_daily = (expected_return - rf) / volatility if volatility > 0 else None

    weight_rows = [
        {"asset": str(assets[i]), "weight": _safe_float(weights[i]),
         "weight_pct": _safe_float(weights[i] * 100.0)}
        for i in range(n_assets)
    ]

    return {
        "n_assets": n_assets,
        "n_scenarios": int(len(rets_df)),
        "alpha": alpha,
        "confidence_level": 1.0 - alpha,
        "weights": weight_rows,
        "expected_return": expected_return,
        "volatility": volatility,
        "risk_free_rate": rf,
        "sharpe_ratio": _annualize_sharpe(sharpe_daily),
        "sharpe_ratio_daily": sharpe_daily,
        "annualized_return": _annualize_return(expected_return),
        "annualized_volatility": volatility * math.sqrt(PERIODS_PER_YEAR),
        "var": var,
        "cvar": cvar,
        "var_pct": var * 100.0,
        "cvar_pct": cvar * 100.0,
        "var_convention": VAR_CONVENTION,
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


def _opt_result_to_payload(result, assets, rets_df, alpha, rf):
    """Normalize an optimizer result into the display schema.

    Every figure is recomputed from the optimal weights and the return history, so
    an optimiser that does not produce a field (variance for min-CVaR, VaR for
    mean-variance) still gets the real number instead of a 0.0 placeholder.
    """
    if not isinstance(result, dict) or not result.get("success", True):
        raise ValidationError((result or {}).get("error") or "optimizer returned a non-success payload")
    w = _weights_vector(result.get("weights", {}), assets)
    if len(w) != len(assets):
        raise ValidationError("optimizer returned weights for a different number of assets")
    weight_rows = [
        {"asset": str(assets[i]), "weight": float(w[i]), "weight_pct": float(w[i]) * 100.0}
        for i in range(len(assets))
    ]
    out = {"weights": weight_rows}
    out.update(_portfolio_figures(w, rets_df, alpha, rf))
    out.update({
        "risk_free_rate": rf,
        "n_assets": len(weight_rows),
        "max_weight": float(w.max()) if len(w) else None,
        "min_weight": float(w.min()) if len(w) else None,
        "concentration_hhi": float((w ** 2).sum()) if len(w) else None,
    })
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
      risk_free_rate    : float (PER PERIOD, i.e. daily decimal; default 0.0)
      target_return     : float (per period; the portfolio gets return >= target)
      alpha             : float (tail probability for the reported VaR / CVaR, default 0.05)
    Sharpe ratios in the output are annualised (x sqrt(252)); *_daily keeps the raw one.
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    objective = str(data.get("objective", "min_variance")).lower()
    long_only = bool(data.get("long_only", True))
    max_weight = _safe_float(data.get("max_weight"))
    min_weight = _safe_float(data.get("min_weight"))
    rf = _safe_float(data.get("risk_free_rate", 0.0), 0.0)
    alpha = _parse_alpha(data)

    optimizer = MeanVarianceOptimizer(rets_df, risk_free_rate=rf)

    if objective == "min_variance":
        raw = optimizer.minimum_variance(long_only=long_only,
                                         max_weight=max_weight, min_weight=min_weight)
    elif objective == "max_sharpe":
        raw = optimizer.max_sharpe(long_only=long_only, max_weight=max_weight,
                                   min_weight=min_weight)
    elif objective == "target_return":
        target = _safe_float(data.get("target_return"))
        if target is None:
            raise ValidationError("target_return is required when objective='target_return'")
        raw = optimizer.target_return(target=target, long_only=long_only,
                                      max_weight=max_weight, min_weight=min_weight)
    else:
        raise ValidationError(
            f"unknown objective {objective!r}; expected: min_variance, max_sharpe, target_return")

    payload = _opt_result_to_payload(raw, assets, rets_df, alpha, rf)
    payload["objective"] = objective
    payload["long_only"] = long_only
    if objective == "target_return":
        payload["target_return"] = target
        if not raw.get("target_binding", True):
            payload["_warning"] = ("The target return is below the minimum-variance portfolio's return; "
                                   "the minimum-variance portfolio is shown (lower targets are dominated).")
    return payload


def op_mean_cvar_optimize(data):
    """
    Mean-CVaR portfolio optimization.
    Inputs:
      returns | tickers : returns matrix
      objective         : 'min_cvar' | 'target_return'  (default 'min_cvar')
      target_return     : float (required if objective='target_return')
      alpha             : float (CVaR tail probability, default 0.05)
      long_only         : bool
      max_weight        : float | null
      min_weight        : float | null
      risk_free_rate    : float (per period, only used for the reported Sharpe; default 0)
    Solved exactly as the Rockafellar-Uryasev linear programme.
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    objective = str(data.get("objective", "min_cvar")).lower()
    alpha = _parse_alpha(data)
    long_only = bool(data.get("long_only", True))
    max_weight = _safe_float(data.get("max_weight"))
    min_weight = _safe_float(data.get("min_weight"))
    rf = _safe_float(data.get("risk_free_rate", 0.0), 0.0)

    optimizer = MeanCVaROptimizer(rets_df, alpha=alpha)

    if objective == "min_cvar":
        raw = optimizer.minimum_cvar(long_only=long_only,
                                     max_weight=max_weight, min_weight=min_weight)
    elif objective == "target_return":
        target = _safe_float(data.get("target_return"))
        if target is None:
            raise ValidationError("target_return is required when objective='target_return'")
        raw = optimizer.target_return(target=target, long_only=long_only,
                                      max_weight=max_weight, min_weight=min_weight)
    else:
        raise ValidationError(
            f"unknown objective {objective!r}; expected: min_cvar, target_return")

    payload = _opt_result_to_payload(raw, assets, rets_df, alpha, rf)
    payload["objective"] = objective
    payload["long_only"] = long_only
    if objective == "target_return":
        payload["target_return"] = target
    return payload


def op_efficient_frontier(data):
    """
    Mean-Variance efficient frontier with N points.
    Inputs:
      returns | tickers : returns matrix
      n_points          : int (default 20, clamped to [5, 50])
      long_only         : bool (default True)
      max_weight        : float | null
      min_weight        : float | null
      risk_free_rate    : float (per period / daily decimal, default 0.0)
    Point 0 is the minimum-variance portfolio; the last point is the highest return
    attainable under the weight limits (found by LP, not "100 % in the best asset").
    """
    _require_modules()
    rets_df, assets = _normalize_returns_input(data)
    n_points = max(5, min(50, _safe_int(data.get("n_points", 20))))
    long_only = bool(data.get("long_only", True))
    max_weight = _safe_float(data.get("max_weight"))
    min_weight = _safe_float(data.get("min_weight"))
    rf = _safe_float(data.get("risk_free_rate", 0.0), 0.0)

    optimizer = MeanVarianceOptimizer(rets_df, risk_free_rate=rf)
    raw = optimizer.efficient_frontier(n_points=n_points, long_only=long_only,
                                       max_weight=max_weight, min_weight=min_weight)

    if not isinstance(raw, dict) or not raw.get("success", True):
        raise ValidationError((raw or {}).get("error") or "efficient_frontier returned non-success")

    frontier = raw.get("frontier", []) or []
    points = []
    for pt in frontier:
        w = _weights_vector(pt.get("weights", {}), assets)
        er = float(pt["expected_return"])
        vol = float(pt["volatility"])
        sharpe_daily = (er - rf) / vol if vol > 0 else None
        points.append({
            "expected_return": er,
            "annualized_return": _annualize_return(er),
            "volatility": vol,
            "annualized_volatility": vol * math.sqrt(PERIODS_PER_YEAR),
            "sharpe_ratio": _annualize_sharpe(sharpe_daily),
            "sharpe_ratio_daily": sharpe_daily,
            "weights": [{"asset": str(assets[i]), "weight": float(w[i])} for i in range(len(assets))],
        })
    if not points:
        raise ValidationError("no efficient-frontier point could be solved")

    min_var_index = min(range(len(points)), key=lambda i: points[i]["volatility"])
    with_sharpe = [i for i in range(len(points)) if points[i]["sharpe_ratio"] is not None]
    max_sharpe_index = max(with_sharpe, key=lambda i: points[i]["sharpe_ratio"]) if with_sharpe else None

    out = {
        "n_points": len(points),
        "requested_points": int(raw.get("requested_points", n_points)),
        "failed_points": int(raw.get("failed_points", 0)),
        "n_assets": len(assets),
        "assets": assets,
        "long_only": long_only,
        "risk_free_rate": rf,
        "frontier": points,
        "min_volatility": min(p["volatility"] for p in points),
        "max_volatility": max(p["volatility"] for p in points),
        "min_return": min(p["expected_return"] for p in points),
        "max_return": max(p["expected_return"] for p in points),
        "best_sharpe": points[max_sharpe_index]["sharpe_ratio"] if max_sharpe_index is not None else None,
        "min_var_index": min_var_index,
        "max_sharpe_index": max_sharpe_index,
    }
    if raw.get("note"):
        out["note"] = raw["note"]
    if out["failed_points"]:
        out["_warning"] = (f"{out['failed_points']} of {out['requested_points']} frontier points "
                           f"could not be solved and are not shown.")
    return out


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
            float(probs_arr[-1] / probs_arr[0]) if probs_arr[0] > 0 else None),
        "effective_sample_size": _safe_float(ess),
        "ess_pct_of_n": _safe_float((ess / n) * 100.0) if n > 0 else None,
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
        envelope = {"success": True, "operation": operation, "data": result}
        # The panel shows a top-level "warning" as a banner above the results.
        warning = result.pop("_warning", None) if isinstance(result, dict) else None
        if warning:
            envelope["warning"] = warning
        return envelope
    except ValidationError as e:
        return {"success": False, "operation": operation, "error": str(e),
                "error_kind": "validation"}
    except Exception as e:
        traceback.print_exc(file=sys.stderr)  # diagnostics to stderr only
        return {"success": False, "operation": operation,
                "error": f"{type(e).__name__}: {e}" if str(e) else type(e).__name__,
                "error_kind": "runtime"}


def main(args):
    if len(args) < 1:
        fincept_json.emit({
            "success": False,
            "error": "Usage: fortitudo_service.py <operation> [json_data]",
            "error_kind": "usage",
        })
        return
    operation = args[0]
    data = {}
    if len(args) > 1 and args[1].strip():
        try:
            data = json.loads(args[1])
        except json.JSONDecodeError as e:
            fincept_json.emit({
                "success": False, "operation": operation,
                "error": f"Invalid JSON: {e}", "error_kind": "validation",
            })
            return
    if not isinstance(data, dict):
        fincept_json.emit({"success": False, "operation": operation,
                           "error": "parameters must be a JSON object", "error_kind": "validation"})
        return
    fincept_json.emit(dispatch(operation, data))


if __name__ == "__main__":
    main(sys.argv[1:])
