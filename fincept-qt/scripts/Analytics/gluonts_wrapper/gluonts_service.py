"""
GluonTS Service (probabilistic forecasting backend)
====================================================
Self-contained probabilistic-forecasting + distributional-modeling service for
the Fincept Terminal "GluonTS" sub-tab.

Notes:
- The legacy implementation (preserved as gluonts_service_legacy.py) imports
  the heavy `gluonts` package, which depends on torch/mxnet and is not
  installed in this venv. This rewrite uses scipy + sklearn + numpy only,
  giving the same conceptual surface (probabilistic point + interval
  forecasts, distribution fits, evaluation metrics, baseline predictors)
  without the dependency.

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
from typing import Dict, List, Any

import numpy as np

try:
    from scipy import stats as scstats
    SCIPY_OK = True
except ImportError:
    SCIPY_OK = False

try:
    from sklearn.linear_model import LinearRegression
    SKLEARN_OK = True
except ImportError:
    SKLEARN_OK = False


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


def _require_min_length(values, n, field_name):
    if len(values) < n:
        raise ValidationError(
            f"{field_name} needs at least {n} values, got {len(values)}")


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


# ============================================================================
# SHARED FORECASTING UTILS
# ============================================================================

def _bootstrap_paths(values, horizon, n_paths, lags, rng):
    """
    Generate n_paths bootstrapped forecast trajectories using residuals from a
    simple AR(lags) regression. Returns (paths[n_paths, horizon], in_sample_resid).
    """
    if not SKLEARN_OK:
        raise ValidationError("sklearn not installed — bootstrap forecasting unavailable")
    n = len(values)
    arr = np.asarray(values, dtype=float)
    if n <= lags + 5:
        raise ValidationError(f"need at least lags+6={lags + 6} obs, got {n}")
    X, y = [], []
    for i in range(lags, n):
        X.append(arr[i - lags:i])
        y.append(arr[i])
    X = np.asarray(X, dtype=float)
    y = np.asarray(y, dtype=float)
    base = LinearRegression().fit(X, y)
    resid = y - base.predict(X)

    paths = np.zeros((n_paths, horizon), dtype=float)
    for b in range(n_paths):
        last = list(arr[-lags:])
        shocks = rng.choice(resid, size=horizon, replace=True)
        for h in range(horizon):
            x_pred = np.asarray([last[-lags:]], dtype=float)
            yhat = float(base.predict(x_pred)[0]) + shocks[h]
            paths[b, h] = yhat
            last.append(yhat)
    return paths, resid


def _seasonal_naive_array(values, horizon, season_length):
    """Repeat the last `season_length` observations forward."""
    arr = np.asarray(values, dtype=float)
    n = len(arr)
    season = max(1, min(season_length, n))
    out = np.zeros(horizon, dtype=float)
    for h in range(horizon):
        out[h] = float(arr[n - season + (h % season)])
    return out


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_check_status(_data):
    return {
        "scipy": SCIPY_OK,
        "sklearn": SKLEARN_OK,
        "gluonts_native": False,
        "backend": "numpy" + (" + scipy" if SCIPY_OK else "") + (" + sklearn" if SKLEARN_OK else ""),
        "ops_available": list(OPERATIONS.keys()),
        "note": "Self-contained probabilistic-forecasting backend. Heavy gluonts package not required.",
    }


def op_probabilistic_forecast(data):
    """
    Bootstrap-residual probabilistic forecast with multi-quantile bands (10/25/50/75/90).

    Inputs:
      values    : list[float]  (required, >= 30)
      horizon   : int          (default 14)
      lags      : int          (AR window, default 7)
      n_paths   : int          (bootstrap paths, default 500)
      seed      : int          (RNG seed, default 42)
    """
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    horizon_raw = _safe_int(data.get("horizon", 14))
    if horizon_raw < 1:
        raise ValidationError(f"horizon must be >= 1, got {horizon_raw}")
    horizon = min(horizon_raw, 365)
    lags = max(1, min(_safe_int(data.get("lags", 7)), len(values) // 2))
    n_paths = max(100, min(_safe_int(data.get("n_paths", 500)), 5000))
    seed = _safe_int(data.get("seed", 42))

    rng = np.random.default_rng(seed)
    paths, resid = _bootstrap_paths(values, horizon, n_paths, lags, rng)

    qs = [10, 25, 50, 75, 90]
    quantile_arr = np.percentile(paths, qs, axis=0)  # shape (5, horizon)
    point = quantile_arr[2]  # median = 50th percentile
    mean_path = paths.mean(axis=0)

    # Build per-step forecast rows
    forecast = []
    for h in range(horizon):
        forecast.append({
            "step": h + 1,
            "p10": _safe_float(quantile_arr[0, h]),
            "p25": _safe_float(quantile_arr[1, h]),
            "p50": _safe_float(quantile_arr[2, h]),
            "p75": _safe_float(quantile_arr[3, h]),
            "p90": _safe_float(quantile_arr[4, h]),
            "mean": _safe_float(mean_path[h]),
            "width_50": _safe_float(quantile_arr[3, h] - quantile_arr[1, h]),
            "width_80": _safe_float(quantile_arr[4, h] - quantile_arr[0, h]),
        })

    last_actual = float(values[-1])
    horizon_drift = float(point[-1] - last_actual)

    return {
        "n_observations": len(values),
        "horizon": horizon,
        "lags": lags,
        "n_paths": n_paths,
        "residual_std": _safe_float(float(resid.std())),
        "last_actual": _safe_float(last_actual),
        "first_forecast_p50": _safe_float(point[0]),
        "last_forecast_p50": _safe_float(point[-1]),
        "horizon_drift": _safe_float(horizon_drift),
        "mean_p80_width": _safe_float(float((quantile_arr[4] - quantile_arr[0]).mean())),
        "first_p80_width": _safe_float(quantile_arr[4, 0] - quantile_arr[0, 0]),
        "last_p80_width": _safe_float(quantile_arr[4, -1] - quantile_arr[0, -1]),
        "forecast": forecast,
    }


def op_quantile_forecast(data):
    """
    Forecast with a user-chosen quantile list.

    Inputs:
      values    : list[float]   (required, >= 30)
      quantiles : list[float]   (in (0, 1), default [0.05, 0.5, 0.95])
      horizon   : int           (default 14)
      lags      : int           (default 7)
      n_paths   : int           (default 500)
      seed      : int           (default 42)
    """
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    raw_qs = data.get("quantiles")
    if raw_qs is None:
        qs = [0.05, 0.5, 0.95]
    else:
        qs = _coerce_floats(raw_qs, "quantiles")
        if not qs:
            raise ValidationError("quantiles list is empty — provide at least one value in (0, 1)")
    for q in qs:
        if not (0.0 < q < 1.0):
            raise ValidationError(f"quantile {q} must be in (0, 1)")
    qs = sorted(qs)
    horizon_raw = _safe_int(data.get("horizon", 14))
    if horizon_raw < 1:
        raise ValidationError(f"horizon must be >= 1, got {horizon_raw}")
    horizon = min(horizon_raw, 365)
    lags = max(1, min(_safe_int(data.get("lags", 7)), len(values) // 2))
    n_paths = max(100, min(_safe_int(data.get("n_paths", 500)), 5000))
    seed = _safe_int(data.get("seed", 42))

    rng = np.random.default_rng(seed)
    paths, resid = _bootstrap_paths(values, horizon, n_paths, lags, rng)

    pcts = [q * 100.0 for q in qs]
    quantile_arr = np.percentile(paths, pcts, axis=0)  # (len(qs), horizon)

    quantile_rows = []
    for h in range(horizon):
        row = {"step": h + 1}
        for i, q in enumerate(qs):
            row[f"q{int(round(q * 100))}"] = _safe_float(quantile_arr[i, h])
        quantile_rows.append(row)

    # Per-quantile summary across horizon
    quantile_summary = []
    for i, q in enumerate(qs):
        col = quantile_arr[i]
        quantile_summary.append({
            "quantile": _safe_float(q),
            "label": f"q{int(round(q * 100))}",
            "mean": _safe_float(float(col.mean())),
            "min": _safe_float(float(col.min())),
            "max": _safe_float(float(col.max())),
            "first": _safe_float(float(col[0])),
            "last": _safe_float(float(col[-1])),
        })

    return {
        "n_observations": len(values),
        "horizon": horizon,
        "lags": lags,
        "n_paths": n_paths,
        "n_quantiles": len(qs),
        "quantiles": qs,
        "residual_std": _safe_float(float(resid.std())),
        "last_actual": _safe_float(float(values[-1])),
        "forecast": quantile_rows,
        "quantile_summary": quantile_summary,
    }


def op_distribution_fit(data):
    """
    Fit candidate distributions (normal, student-t, lognormal, skewnormal) to a
    series. Return AIC/BIC for model selection.

    Inputs:
      values : list[float]    (required, >= 30)
    """
    if not SCIPY_OK:
        raise ValidationError("scipy not installed — cannot fit distributions")
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    arr = np.asarray(values, dtype=float)
    n = len(arr)

    candidates = []
    # Normal
    candidates.append(("normal", scstats.norm, ()))
    # Student-t — needs positive df, fitted automatically
    candidates.append(("student_t", scstats.t, ()))
    # Lognormal — only if all values positive
    if (arr > 0).all():
        candidates.append(("lognormal", scstats.lognorm, ()))
    # Skewnormal — handles asymmetric tails
    candidates.append(("skewnormal", scstats.skewnorm, ()))

    fits = []
    for name, dist, _ in candidates:
        try:
            params = dist.fit(arr)
            log_lik = float(np.sum(dist.logpdf(arr, *params)))
            k = len(params)
            aic = 2 * k - 2 * log_lik
            bic = k * math.log(n) - 2 * log_lik
            # KS test against fitted distribution
            ks_stat, ks_p = scstats.kstest(arr, dist.cdf, args=params)
        except Exception as e:
            params = ()
            log_lik = float("nan")
            aic = float("inf")
            bic = float("inf")
            ks_stat = float("nan")
            ks_p = 0.0

        fits.append({
            "distribution": name,
            "n_params": len(params),
            "params": [_safe_float(p) for p in params],
            "log_likelihood": _safe_float(log_lik),
            "aic": _safe_float(aic, default=float("inf")) if not math.isinf(aic) else None,
            "bic": _safe_float(bic, default=float("inf")) if not math.isinf(bic) else None,
            "ks_statistic": _safe_float(ks_stat),
            "ks_p_value": _safe_float(ks_p),
            "cannot_reject_at_5pct": bool(ks_p > 0.05),
        })

    # Pick best by AIC (lowest)
    valid = [f for f in fits if f["aic"] is not None and not math.isinf(float(f["aic"]))]
    best_by_aic = min(valid, key=lambda f: f["aic"])["distribution"] if valid else None
    best_by_bic = min(valid, key=lambda f: f["bic"])["distribution"] if valid else None

    # Also compute basic stats so the user can sanity-check the fits
    return {
        "n_observations": n,
        "data_mean": _safe_float(float(arr.mean())),
        "data_std": _safe_float(float(arr.std(ddof=1))),
        "data_skew": _safe_float(float(scstats.skew(arr))),
        "data_kurtosis": _safe_float(float(scstats.kurtosis(arr))),
        "data_min": _safe_float(float(arr.min())),
        "data_max": _safe_float(float(arr.max())),
        "best_by_aic": best_by_aic,
        "best_by_bic": best_by_bic,
        "fits": fits,
    }


def op_evaluate_forecast(data):
    """
    Forecast accuracy + probabilistic-coverage evaluation.

    Inputs:
      actuals      : list[float]              (required, >= 5)
      point        : list[float]              (point forecast, same length)
      lower        : list[float]              (optional, lower band)
      upper        : list[float]              (optional, upper band)
      training     : list[float]              (optional historical series for MASE)
      season       : int                      (default 1, used for MASE denominator)
      coverage_target_pct : float             (default 80, only used for label)
    """
    actuals = _coerce_floats(data.get("actuals"), "actuals")
    point = _coerce_floats(data.get("point"), "point")
    _require_min_length(actuals, 5, "actuals")
    if len(actuals) != len(point):
        raise ValidationError(
            f"actuals ({len(actuals)}) and point ({len(point)}) must have the same length")

    a = np.asarray(actuals, dtype=float)
    p = np.asarray(point, dtype=float)
    err = a - p
    mae = float(np.mean(np.abs(err)))
    mse = float(np.mean(err ** 2))
    rmse = float(np.sqrt(mse))
    safe_a = np.where(np.abs(a) < 1e-12, np.nan, a)
    mape = float(np.nanmean(np.abs(err / safe_a)) * 100.0)
    denom = (np.abs(a) + np.abs(p))
    safe_denom = np.where(denom < 1e-12, np.nan, denom)
    smape = float(np.nanmean(2.0 * np.abs(err) / safe_denom) * 100.0)
    bias = float(err.mean())

    # MASE — naive seasonal benchmark from training history if provided
    mase = None
    training_raw = data.get("training")
    if training_raw is not None:
        training = _coerce_floats(training_raw, "training")
        if len(training) > 1:
            season = max(1, _safe_int(data.get("season", 1)))
            train_arr = np.asarray(training, dtype=float)
            if len(train_arr) > season:
                naive_err = np.abs(train_arr[season:] - train_arr[:-season])
                naive_scale = float(naive_err.mean())
                if naive_scale > 0:
                    mase = float(mae / naive_scale)

    # Coverage + interval metrics
    has_intervals = data.get("lower") is not None and data.get("upper") is not None
    coverage_pct = None
    avg_width = None
    interval_score = None
    if has_intervals:
        lower = _coerce_floats(data.get("lower"), "lower")
        upper = _coerce_floats(data.get("upper"), "upper")
        if len(lower) != len(actuals) or len(upper) != len(actuals):
            raise ValidationError("lower/upper bands must match actuals length")
        l = np.asarray(lower, dtype=float)
        u = np.asarray(upper, dtype=float)
        within = (a >= l) & (a <= u)
        coverage_pct = float(within.mean() * 100.0)
        avg_width = float((u - l).mean())
        # Winkler/interval score with target alpha = 1 - coverage_target/100
        coverage_target = _safe_float(data.get("coverage_target_pct", 80.0))
        alpha = max(1e-9, 1.0 - coverage_target / 100.0)
        below = a < l
        above = a > u
        score = (u - l) + (2.0 / alpha) * (l - a) * below + (2.0 / alpha) * (a - u) * above
        interval_score = float(score.mean())

    # Direction accuracy on first differences
    if len(a) > 1:
        actual_dir = np.sign(np.diff(a))
        pred_dir = np.sign(np.diff(p))
        direction_acc = float(np.mean(actual_dir == pred_dir) * 100.0)
    else:
        direction_acc = 0.0

    return {
        "n_observations": len(actuals),
        "mae": _safe_float(mae),
        "mse": _safe_float(mse),
        "rmse": _safe_float(rmse),
        "mape_pct": _safe_float(mape),
        "smape_pct": _safe_float(smape),
        "mase": _safe_float(mase) if mase is not None else None,
        "bias": _safe_float(bias),
        "direction_accuracy_pct": _safe_float(direction_acc),
        "has_intervals": has_intervals,
        "coverage_pct": _safe_float(coverage_pct) if coverage_pct is not None else None,
        "coverage_target_pct": _safe_float(data.get("coverage_target_pct", 80.0)),
        "avg_interval_width": _safe_float(avg_width) if avg_width is not None else None,
        "interval_score": _safe_float(interval_score) if interval_score is not None else None,
    }


def op_seasonal_naive(data):
    """
    Seasonal naive baseline: repeat the last `season_length` observations forward.

    Inputs:
      values        : list[float]   (required)
      horizon       : int           (default 14)
      season_length : int           (default 1 = pure naive)
    """
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 1, "values")
    horizon = max(1, min(_safe_int(data.get("horizon", 14)), 365))
    season = max(1, _safe_int(data.get("season_length", 1)))

    arr = np.asarray(values, dtype=float)
    if season > len(arr):
        raise ValidationError(
            f"season_length ({season}) exceeds series length ({len(arr)})")
    fc = _seasonal_naive_array(values, horizon, season)

    forecast = [{"step": i + 1, "value": _safe_float(v)} for i, v in enumerate(fc.tolist())]
    return {
        "n_observations": len(arr),
        "horizon": horizon,
        "season_length": season,
        "method": "pure naive (repeat last)" if season == 1 else f"seasonal naive (s={season})",
        "last_actual": _safe_float(float(arr[-1])),
        "first_forecast": _safe_float(float(fc[0])),
        "last_forecast": _safe_float(float(fc[-1])),
        "forecast_min": _safe_float(float(fc.min())),
        "forecast_max": _safe_float(float(fc.max())),
        "forecast_mean": _safe_float(float(fc.mean())),
        "forecast": forecast,
    }


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "check_status": op_check_status,
    "probabilistic_forecast": op_probabilistic_forecast,
    "quantile_forecast": op_quantile_forecast,
    "distribution_fit": op_distribution_fit,
    "evaluate_forecast": op_evaluate_forecast,
    "seasonal_naive": op_seasonal_naive,
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
            "error": "Usage: gluonts_service.py <operation> [json_data]",
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
