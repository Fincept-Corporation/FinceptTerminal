"""
Functime Service (pandas/sklearn/statsmodels backend)
======================================================
Self-contained time-series analytics service for the Fincept Terminal
"Functime" sub-tab. Does not require the polars-based functime library
(the original wrapper is preserved as functime_service_polars_legacy.py).

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
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional

import numpy as np
import pandas as pd

# Fail loudly only on truly missing essentials.
try:
    from sklearn.linear_model import LinearRegression, Lasso, Ridge, ElasticNet
    from sklearn.neighbors import KNeighborsRegressor
    from sklearn.ensemble import IsolationForest
    SKLEARN_OK = True
except ImportError:
    SKLEARN_OK = False

try:
    from statsmodels.tsa.seasonal import STL
    from statsmodels.tsa.holtwinters import ExponentialSmoothing
    from statsmodels.tsa.arima.model import ARIMA
    from statsmodels.tsa.stattools import adfuller, kpss, acf
    STATSMODELS_OK = True
except ImportError:
    STATSMODELS_OK = False

try:
    from scipy import stats as scstats
    SCIPY_OK = True
except ImportError:
    SCIPY_OK = False


# ============================================================================
# INPUT COERCION + VALIDATION
# ============================================================================

class ValidationError(ValueError):
    """Raised when an op's input doesn't pass coercion/length checks."""
    pass


def _coerce_floats(value, field_name):
    """
    Accept list[number], tuple[number], np.ndarray, or a CSV / whitespace-separated string.
    Returns list[float]. Empty / None -> [].
    """
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


def _series_from_list(values, dates=None, name="series"):
    """Build a pd.Series of floats with a date index (default daily)."""
    if dates:
        idx = pd.to_datetime(dates)
    else:
        idx = pd.date_range("2020-01-01", periods=len(values), freq="B")
    return pd.Series(np.asarray(values, dtype=float), index=idx, name=name)


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


def _sample_curve(series, max_points=250):
    """Return [{date, value}] sampled to ~max_points entries, always including the last."""
    out = []
    if series is None or len(series) == 0:
        return out
    step = max(1, len(series) // max_points)
    for i, (d, v) in enumerate(series.items()):
        if i % step == 0:
            out.append({"date": str(d)[:10], "value": _safe_float(v)})
    last_date = str(series.index[-1])[:10]
    if not out or out[-1]["date"] != last_date:
        out.append({"date": last_date, "value": _safe_float(series.iloc[-1])})
    return out


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_check_status(_data):
    """Report library availability — no inputs."""
    return {
        "sklearn": SKLEARN_OK,
        "statsmodels": STATSMODELS_OK,
        "scipy": SCIPY_OK,
        "backend": "pandas + sklearn + statsmodels",
        "ops_available": list(OPERATIONS.keys()),
    }


def op_forecast(data):
    """
    Multi-model forecasting.
    Inputs:
      values      : list[float] | CSV string  (required, >= 30 values)
      model       : 'linear' | 'ridge' | 'lasso' | 'elasticnet' | 'knn' |
                    'holt_winters' | 'arima' | 'naive' | 'drift'  (default: 'linear')
      horizon     : int       (default: 14)
      lags        : int       (default: 7)
      alpha       : float     (regularization for ridge/lasso/elasticnet, default 1.0)
      l1_ratio    : float     (elasticnet only, default 0.5)
      n_neighbors : int       (knn only, default 5)
      season      : int       (holt_winters/arima seasonal period, default 0=disabled)
    """
    if not SKLEARN_OK:
        raise ValidationError("sklearn is not installed in the venv — cannot run forecast")

    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")

    model_type = str(data.get("model", "linear")).lower()
    horizon = max(1, _safe_int(data.get("horizon", 14)))
    lags = max(1, _safe_int(data.get("lags", 7)))
    alpha = _safe_float(data.get("alpha", 1.0))
    l1_ratio = _safe_float(data.get("l1_ratio", 0.5))
    n_neighbors = max(1, _safe_int(data.get("n_neighbors", 5)))
    season = max(0, _safe_int(data.get("season", 0)))

    series = _series_from_list(values, data.get("dates"))
    arr = series.to_numpy()
    n = len(arr)

    # Compute in-sample fit and out-of-sample forecast
    in_sample = np.full(n, np.nan)
    forecast: List[float] = []

    def _ml_forecast(model):
        # Build lag matrix
        if n <= lags + 1:
            raise ValidationError(f"need at least lags+2={lags + 2} observations, got {n}")
        X, y = [], []
        for i in range(lags, n):
            X.append(arr[i - lags:i])
            y.append(arr[i])
        X = np.asarray(X, dtype=float)
        y = np.asarray(y, dtype=float)
        model.fit(X, y)
        # In-sample fitted (fit ignores first `lags` points)
        in_sample[lags:] = model.predict(X)
        # Recursive out-of-sample
        last = list(arr[-lags:])
        out = []
        for _ in range(horizon):
            x_pred = np.asarray([last[-lags:]], dtype=float)
            yhat = float(model.predict(x_pred)[0])
            out.append(yhat)
            last.append(yhat)
        return out

    if model_type == "linear":
        forecast = _ml_forecast(LinearRegression())
    elif model_type == "ridge":
        forecast = _ml_forecast(Ridge(alpha=alpha))
    elif model_type == "lasso":
        forecast = _ml_forecast(Lasso(alpha=alpha, max_iter=10000))
    elif model_type == "elasticnet":
        forecast = _ml_forecast(ElasticNet(alpha=alpha, l1_ratio=l1_ratio, max_iter=10000))
    elif model_type == "knn":
        forecast = _ml_forecast(KNeighborsRegressor(n_neighbors=min(n_neighbors, max(1, n - lags - 1))))
    elif model_type == "naive":
        # Repeat the last value
        in_sample[1:] = arr[:-1]
        forecast = [float(arr[-1])] * horizon
    elif model_type == "drift":
        # Linear interpolation from first to last point
        slope = (arr[-1] - arr[0]) / (n - 1) if n > 1 else 0.0
        in_sample[1:] = arr[:-1] + slope
        forecast = [float(arr[-1] + slope * (i + 1)) for i in range(horizon)]
    elif model_type == "holt_winters":
        if not STATSMODELS_OK:
            raise ValidationError("statsmodels not installed — cannot run holt_winters")
        kwargs = {"trend": "add"}
        if season >= 2:
            kwargs.update({"seasonal": "add", "seasonal_periods": season})
        try:
            mdl = ExponentialSmoothing(arr, **kwargs).fit(optimized=True)
        except Exception as e:
            raise ValidationError(f"holt_winters fit failed: {e}")
        in_sample[:] = mdl.fittedvalues
        forecast = [float(v) for v in mdl.forecast(horizon)]
    elif model_type == "arima":
        if not STATSMODELS_OK:
            raise ValidationError("statsmodels not installed — cannot run arima")
        order = (1, 1, 1)
        seasonal_order = (0, 0, 0, 0)
        if season >= 2:
            seasonal_order = (1, 0, 1, season)
        try:
            mdl = ARIMA(arr, order=order, seasonal_order=seasonal_order).fit()
        except Exception as e:
            raise ValidationError(f"arima fit failed: {e}")
        in_sample[:] = mdl.fittedvalues
        forecast = [float(v) for v in mdl.forecast(horizon)]
    else:
        raise ValidationError(
            f"unknown model {model_type!r}; expected one of: "
            "linear, ridge, lasso, elasticnet, knn, naive, drift, holt_winters, arima")

    # Build forecast dates (extend the last frequency)
    if isinstance(series.index, pd.DatetimeIndex) and len(series.index) >= 2:
        delta = series.index[-1] - series.index[-2]
    else:
        delta = pd.Timedelta(days=1)
    forecast_dates = [series.index[-1] + delta * (i + 1) for i in range(horizon)]

    # In-sample residuals + summary
    fitted = pd.Series(in_sample, index=series.index)
    resid = (series - fitted).dropna()
    resid_std = float(resid.std()) if len(resid) > 1 else 0.0
    if len(resid) > 0:
        rss = float((resid ** 2).sum())
        tss = float(((series.loc[resid.index] - series.loc[resid.index].mean()) ** 2).sum())
        r2 = 1.0 - rss / tss if tss > 0 else 0.0
        mae = float(resid.abs().mean())
        rmse = float(np.sqrt((resid ** 2).mean()))
    else:
        r2, mae, rmse = 0.0, 0.0, 0.0

    history = _sample_curve(series, 250)
    in_sample_curve = []
    for d, v in fitted.items():
        if not math.isnan(v):
            in_sample_curve.append({"date": str(d)[:10], "value": _safe_float(v)})
    # Subsample fitted to ~250 too
    if len(in_sample_curve) > 250:
        step = max(1, len(in_sample_curve) // 250)
        in_sample_curve = [r for i, r in enumerate(in_sample_curve) if i % step == 0]

    return {
        "model": model_type,
        "n_observations": n,
        "horizon": horizon,
        "lags": lags,
        "season": season,
        "in_sample_r2": _safe_float(r2),
        "in_sample_mae": _safe_float(mae),
        "in_sample_rmse": _safe_float(rmse),
        "residual_std": _safe_float(resid_std),
        "history": history,
        "fitted": in_sample_curve,
        "forecast": [
            {"date": str(d)[:10], "value": _safe_float(v)}
            for d, v in zip(forecast_dates, forecast)
        ],
        "forecast_min": _safe_float(min(forecast) if forecast else 0.0),
        "forecast_max": _safe_float(max(forecast) if forecast else 0.0),
        "forecast_mean": _safe_float(float(np.mean(forecast)) if forecast else 0.0),
        "last_actual": _safe_float(arr[-1]),
        "first_forecast": _safe_float(forecast[0] if forecast else 0.0),
        "last_forecast": _safe_float(forecast[-1] if forecast else 0.0),
    }


def op_anomaly_detection(data):
    """
    Detect anomalies in a time series.
    Inputs:
      values       : list[float] | CSV string  (required, >= 20)
      method       : 'zscore' | 'iqr' | 'isolation_forest' | 'residual'  (default 'zscore')
      threshold    : float — z-score / residual cutoff in std units (default 3.0)
      iqr_k        : float — IQR multiplier (default 1.5)
      contamination: float — IsolationForest expected anomaly fraction (default 0.05)
    """
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 20, "values")
    method = str(data.get("method", "zscore")).lower()

    series = _series_from_list(values, data.get("dates"))
    arr = series.to_numpy()
    n = len(arr)
    flags = np.zeros(n, dtype=bool)
    scores = np.zeros(n, dtype=float)

    if method == "zscore":
        thr = _safe_float(data.get("threshold", 3.0))
        mu = float(arr.mean())
        sd = float(arr.std()) or 1.0
        scores = (arr - mu) / sd
        flags = np.abs(scores) > thr
    elif method == "iqr":
        k = _safe_float(data.get("iqr_k", 1.5))
        q1, q3 = np.percentile(arr, [25, 75])
        iqr = q3 - q1
        lo, hi = q1 - k * iqr, q3 + k * iqr
        flags = (arr < lo) | (arr > hi)
        scores = np.where(arr < lo, (lo - arr) / (iqr or 1.0),
                          np.where(arr > hi, (arr - hi) / (iqr or 1.0), 0.0))
    elif method == "isolation_forest":
        if not SKLEARN_OK:
            raise ValidationError("sklearn not installed — cannot run isolation_forest")
        cont = max(0.001, min(0.5, _safe_float(data.get("contamination", 0.05))))
        clf = IsolationForest(contamination=cont, random_state=42)
        clf.fit(arr.reshape(-1, 1))
        preds = clf.predict(arr.reshape(-1, 1))  # -1 anomaly, 1 normal
        flags = preds == -1
        # Anomaly score: lower = more anomalous; flip sign so higher = more anomalous
        scores = -clf.score_samples(arr.reshape(-1, 1))
    elif method == "residual":
        # Fit a simple linear trend, flag residuals > threshold * std
        thr = _safe_float(data.get("threshold", 3.0))
        x = np.arange(n, dtype=float).reshape(-1, 1)
        if SKLEARN_OK:
            mdl = LinearRegression().fit(x, arr)
            trend = mdl.predict(x)
        else:
            slope, intercept = np.polyfit(np.arange(n), arr, 1)
            trend = slope * np.arange(n) + intercept
        resid = arr - trend
        sd = float(resid.std()) or 1.0
        scores = resid / sd
        flags = np.abs(scores) > thr
    else:
        raise ValidationError(
            f"unknown method {method!r}; expected one of: zscore, iqr, isolation_forest, residual")

    anomalies = []
    for i in range(n):
        if flags[i]:
            anomalies.append({
                "date": str(series.index[i])[:10],
                "value": _safe_float(arr[i]),
                "score": _safe_float(scores[i]),
                "index": int(i),
            })
    # Sort by absolute score, biggest first
    anomalies.sort(key=lambda a: abs(a["score"]), reverse=True)

    return {
        "method": method,
        "n_observations": n,
        "n_anomalies": int(flags.sum()),
        "anomaly_rate_pct": _safe_float(float(flags.mean() * 100)),
        "anomalies": anomalies[:200],  # Cap at 200 for the wire
        "history": _sample_curve(series, 300),
        "score_min": _safe_float(float(scores.min())),
        "score_max": _safe_float(float(scores.max())),
        "score_mean": _safe_float(float(scores.mean())),
        "score_std": _safe_float(float(scores.std())),
    }


def op_seasonality(data):
    """
    Decompose a series into trend + seasonal + residual via STL, and detect
    the dominant period via FFT/autocorrelation when none is provided.
    Inputs:
      values  : list[float]    (required, >= 24)
      period  : int            (optional; auto-detected when omitted/0)
      robust  : bool           (default True)
    """
    if not STATSMODELS_OK:
        raise ValidationError("statsmodels not installed — cannot run seasonality")
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 24, "values")

    series = _series_from_list(values, data.get("dates"))
    arr = series.to_numpy()
    n = len(arr)

    period = _safe_int(data.get("period", 0))
    detected = False
    if period < 2:
        # Auto-detect via autocorrelation peak (skip lag 0)
        max_lag = min(n // 2, 366)
        ac = acf(arr, nlags=max_lag, fft=True)
        # Find first significant peak after lag 1
        if len(ac) > 4:
            peaks = []
            for i in range(2, len(ac) - 1):
                if ac[i] > ac[i - 1] and ac[i] > ac[i + 1] and ac[i] > 0.2:
                    peaks.append((i, float(ac[i])))
            if peaks:
                peaks.sort(key=lambda p: -p[1])
                period = peaks[0][0]
                detected = True
        if period < 2:
            period = max(2, n // 4)
            detected = True

    if period * 2 >= n:
        period = max(2, n // 3)

    robust = bool(data.get("robust", True))
    try:
        stl = STL(arr, period=period, robust=robust).fit()
    except Exception as e:
        raise ValidationError(f"STL decomposition failed: {e}")

    trend = pd.Series(stl.trend, index=series.index)
    seasonal = pd.Series(stl.seasonal, index=series.index)
    resid = pd.Series(stl.resid, index=series.index)

    # Strength of trend / seasonality (Hyndman 2018 formulation)
    var_resid = float(resid.var())
    var_detrended = float((seasonal + resid).var()) or 1.0
    var_deseasonalized = float((trend + resid).var()) or 1.0
    trend_strength = max(0.0, 1.0 - var_resid / var_deseasonalized)
    seasonal_strength = max(0.0, 1.0 - var_resid / var_detrended)

    return {
        "n_observations": n,
        "period": int(period),
        "period_auto_detected": detected,
        "trend_strength": _safe_float(trend_strength),
        "seasonal_strength": _safe_float(seasonal_strength),
        "residual_std": _safe_float(float(resid.std())),
        "history": _sample_curve(series, 300),
        "trend": _sample_curve(trend, 300),
        "seasonal": _sample_curve(seasonal, 300),
        "residual": _sample_curve(resid, 300),
    }


def op_metrics(data):
    """
    Forecast accuracy metrics between actual and predicted series (must align).
    Inputs:
      actual    : list[float] (required)
      predicted : list[float] (required, same length as actual)
    """
    actual = _coerce_floats(data.get("actual"), "actual")
    predicted = _coerce_floats(data.get("predicted"), "predicted")
    _require_min_length(actual, 2, "actual")
    if len(actual) != len(predicted):
        raise ValidationError(
            f"actual ({len(actual)}) and predicted ({len(predicted)}) must have the same length")

    a = np.asarray(actual, dtype=float)
    p = np.asarray(predicted, dtype=float)
    err = a - p

    mae = float(np.mean(np.abs(err)))
    mse = float(np.mean(err ** 2))
    rmse = float(np.sqrt(mse))
    # MAPE / sMAPE — guard against zero
    safe_a = np.where(np.abs(a) < 1e-12, np.nan, a)
    mape = float(np.nanmean(np.abs(err / safe_a)) * 100.0)
    denom = (np.abs(a) + np.abs(p))
    safe_denom = np.where(denom < 1e-12, np.nan, denom)
    smape = float(np.nanmean(2.0 * np.abs(err) / safe_denom) * 100.0)
    # R²
    ss_res = float(np.sum(err ** 2))
    ss_tot = float(np.sum((a - a.mean()) ** 2)) or 1.0
    r2 = 1.0 - ss_res / ss_tot
    # Bias / direction accuracy
    bias = float(np.mean(err))
    if len(a) > 1:
        actual_dir = np.sign(np.diff(a))
        pred_dir = np.sign(np.diff(p))
        direction_acc = float(np.mean(actual_dir == pred_dir) * 100.0)
    else:
        direction_acc = 0.0

    return {
        "n_observations": len(a),
        "mae": _safe_float(mae),
        "mse": _safe_float(mse),
        "rmse": _safe_float(rmse),
        "mape_pct": _safe_float(mape),
        "smape_pct": _safe_float(smape),
        "r_squared": _safe_float(r2),
        "bias": _safe_float(bias),
        "direction_accuracy_pct": _safe_float(direction_acc),
    }


def op_confidence_intervals(data):
    """
    Build prediction intervals around a point forecast.
    Inputs:
      values     : list[float]  (training history, >= 30)
      horizon    : int          (default 14)
      lags       : int          (default 7)
      n_boot     : int          (bootstrap iterations, default 200)
      confidence : float        (e.g. 0.95, default 0.95)
      method     : 'bootstrap' | 'residual'  (default bootstrap)
    """
    if not SKLEARN_OK:
        raise ValidationError("sklearn not installed — cannot build intervals")
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    horizon = max(1, _safe_int(data.get("horizon", 14)))
    lags = max(1, _safe_int(data.get("lags", 7)))
    n_boot = max(50, min(1000, _safe_int(data.get("n_boot", 200))))
    confidence = _safe_float(data.get("confidence", 0.95))
    if not (0.5 < confidence < 1.0):
        raise ValidationError(f"confidence must be in (0.5, 1.0), got {confidence}")
    method = str(data.get("method", "bootstrap")).lower()

    series = _series_from_list(values, data.get("dates"))
    arr = series.to_numpy()
    n = len(arr)
    if n <= lags + 5:
        raise ValidationError(f"need at least lags+6={lags + 6} observations, got {n}")

    # Fit base linear regression on lag features
    X, y = [], []
    for i in range(lags, n):
        X.append(arr[i - lags:i])
        y.append(arr[i])
    X = np.asarray(X, dtype=float)
    y = np.asarray(y, dtype=float)
    base = LinearRegression().fit(X, y)
    point_resid = y - base.predict(X)
    resid_std = float(point_resid.std()) or 1.0

    # Recursive point forecast
    last = list(arr[-lags:])
    point = []
    for _ in range(horizon):
        x_pred = np.asarray([last[-lags:]], dtype=float)
        yhat = float(base.predict(x_pred)[0])
        point.append(yhat)
        last.append(yhat)

    alpha = 1.0 - confidence
    lower_q = alpha / 2.0 * 100.0
    upper_q = (1.0 - alpha / 2.0) * 100.0

    if method == "residual":
        # Parametric: assume Gaussian residuals; widen with sqrt(h) for accumulation
        if not SCIPY_OK:
            z = 1.96  # default 95% z-score if scipy missing
        else:
            z = float(scstats.norm.ppf(1.0 - alpha / 2.0))
        lower = [point[h] - z * resid_std * math.sqrt(h + 1) for h in range(horizon)]
        upper = [point[h] + z * resid_std * math.sqrt(h + 1) for h in range(horizon)]
    elif method == "bootstrap":
        rng = np.random.default_rng(42)
        boot_paths = np.zeros((n_boot, horizon), dtype=float)
        for b in range(n_boot):
            last_b = list(arr[-lags:])
            shocks = rng.choice(point_resid, size=horizon, replace=True)
            for h in range(horizon):
                x_pred = np.asarray([last_b[-lags:]], dtype=float)
                yhat = float(base.predict(x_pred)[0]) + shocks[h]
                boot_paths[b, h] = yhat
                last_b.append(yhat)
        lower = list(np.percentile(boot_paths, lower_q, axis=0))
        upper = list(np.percentile(boot_paths, upper_q, axis=0))
    else:
        raise ValidationError(f"unknown method {method!r}; expected: bootstrap, residual")

    if isinstance(series.index, pd.DatetimeIndex) and len(series.index) >= 2:
        delta = series.index[-1] - series.index[-2]
    else:
        delta = pd.Timedelta(days=1)
    forecast_dates = [series.index[-1] + delta * (i + 1) for i in range(horizon)]

    intervals = [
        {
            "date": str(d)[:10],
            "point": _safe_float(p),
            "lower": _safe_float(l),
            "upper": _safe_float(u),
            "width": _safe_float(u - l),
        }
        for d, p, l, u in zip(forecast_dates, point, lower, upper)
    ]

    return {
        "method": method,
        "n_observations": n,
        "horizon": horizon,
        "confidence": _safe_float(confidence),
        "lags": lags,
        "n_boot": n_boot if method == "bootstrap" else None,
        "residual_std": _safe_float(resid_std),
        "history": _sample_curve(series, 250),
        "intervals": intervals,
        "mean_width": _safe_float(float(np.mean([i["width"] for i in intervals]))),
        "first_lower": _safe_float(intervals[0]["lower"]),
        "first_upper": _safe_float(intervals[0]["upper"]),
        "last_lower": _safe_float(intervals[-1]["lower"]),
        "last_upper": _safe_float(intervals[-1]["upper"]),
    }


def op_stationarity(data):
    """
    ADF + KPSS stationarity tests with a recommended differencing order.
    Inputs:
      values : list[float] (required, >= 30)
      max_d  : int         (max differencing order to test, default 2)
    """
    if not STATSMODELS_OK:
        raise ValidationError("statsmodels not installed — cannot run stationarity tests")
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    max_d = max(0, min(3, _safe_int(data.get("max_d", 2))))

    series = pd.Series(values, dtype=float)
    results = []
    recommended_d = None
    for d in range(max_d + 1):
        s = series.copy()
        for _ in range(d):
            s = s.diff().dropna()
        if len(s) < 10:
            continue
        # ADF: H0 = unit root (non-stationary). Reject (p < .05) -> stationary.
        try:
            adf_stat, adf_p, _, _, adf_crit, _ = adfuller(s, autolag="AIC")
            adf_stationary = adf_p < 0.05
        except Exception as e:
            adf_stat, adf_p, adf_crit, adf_stationary = float("nan"), 1.0, {}, False
        # KPSS: H0 = stationary. Reject -> non-stationary.
        try:
            kpss_stat, kpss_p, _, kpss_crit = kpss(s, regression="c", nlags="auto")
            kpss_stationary = kpss_p >= 0.05
        except Exception:
            kpss_stat, kpss_p, kpss_crit, kpss_stationary = float("nan"), 0.0, {}, False
        verdict_both = adf_stationary and kpss_stationary
        results.append({
            "differencing_order": d,
            "n_observations": int(len(s)),
            "adf_statistic": _safe_float(adf_stat),
            "adf_p_value": _safe_float(adf_p),
            "adf_critical_5pct": _safe_float(adf_crit.get("5%")) if isinstance(adf_crit, dict) else 0.0,
            "adf_stationary": bool(adf_stationary),
            "kpss_statistic": _safe_float(kpss_stat),
            "kpss_p_value": _safe_float(kpss_p),
            "kpss_critical_5pct": _safe_float(kpss_crit.get("5%")) if isinstance(kpss_crit, dict) else 0.0,
            "kpss_stationary": bool(kpss_stationary),
            "both_stationary": bool(verdict_both),
        })
        if recommended_d is None and verdict_both:
            recommended_d = d

    if recommended_d is None:
        recommended_d = max_d

    return {
        "n_observations": len(series),
        "max_d_tested": max_d,
        "recommended_d": int(recommended_d),
        "tests": results,
    }


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "check_status": op_check_status,
    "forecast": op_forecast,
    "anomaly_detection": op_anomaly_detection,
    "seasonality": op_seasonality,
    "metrics": op_metrics,
    "confidence_intervals": op_confidence_intervals,
    "stationarity": op_stationarity,
}


def dispatch(operation, data):
    """Dispatch to operation handler. Always returns a dict with a `success` key."""
    handler = OPERATIONS.get(operation)
    if handler is None:
        return {
            "success": False,
            "operation": operation,
            "error": f"Unknown operation: {operation}",
            "error_kind": "unknown_op",
            "available": list(OPERATIONS.keys()),
        }
    try:
        result = handler(data or {})
        return {
            "success": True,
            "operation": operation,
            "data": result,
        }
    except ValidationError as e:
        return {
            "success": False,
            "operation": operation,
            "error": str(e),
            "error_kind": "validation",
        }
    except Exception as e:
        return {
            "success": False,
            "operation": operation,
            "error": str(e),
            "error_kind": "runtime",
            "traceback": traceback.format_exc(),
        }


def main(args):
    """Entry point: args = [operation, json_data]"""
    if len(args) < 1:
        print(json.dumps({
            "success": False,
            "error": "Usage: functime_service.py <operation> [json_data]",
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
                "success": False,
                "operation": operation,
                "error": f"Invalid JSON input: {e}",
                "error_kind": "validation",
            }))
            return

    result = dispatch(operation, data)
    print(json.dumps(result, default=str))


if __name__ == "__main__":
    main(sys.argv[1:])
