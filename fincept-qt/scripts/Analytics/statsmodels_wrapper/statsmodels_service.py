"""
Statsmodels Service (econometrics backend)
===========================================
Self-contained econometric service for the Fincept Terminal "Statsmodels"
sub-tab. Same response contract as gs_quant_service / functime_service.

CLI: python statsmodels_service.py <operation> '<json params>'   (prints one JSON object)
Operations: check_status, ols, arima, stationarity_tests, acf_pacf, granger_causality, descriptive,
            cointegration, rolling_ols, regime_switching, pca, decompose

Response contract (all ops):
    success: bool
    operation: str
    data: dict   (when success)
    error: str   (when failure)
    error_kind: str | None    (validation | runtime | unknown_op)
    warning: str              (optional; the panel shows it as a banner)
Tracebacks of uncaught exceptions go to stderr. Values that cannot be computed
are null, never 0.0.
"""

import sys
import os
import io
import json
import math
import inspect
import contextlib
import traceback
import warnings
from typing import Dict, List, Any

import numpy as np
import pandas as pd

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json

# Fail loudly only on truly missing essentials.
try:
    import statsmodels.api as sm
    from statsmodels.tsa.arima.model import ARIMA
    from statsmodels.tsa.stattools import adfuller, kpss, acf, pacf, grangercausalitytests, coint
    from statsmodels.stats.stattools import durbin_watson, jarque_bera as jb_test
    from statsmodels.stats.diagnostic import het_breuschpagan
    STATSMODELS_OK = True
except ImportError:
    STATSMODELS_OK = False

try:
    from scipy import stats as scstats
    SCIPY_OK = True
except ImportError:
    SCIPY_OK = False

# Suppress statsmodels' chatty warnings — they're for advanced users, not the UI.
warnings.filterwarnings("ignore", category=UserWarning)
warnings.filterwarnings("ignore", category=FutureWarning)


# ============================================================================
# INPUT COERCION + VALIDATION
# ============================================================================

class ValidationError(ValueError):
    """Raised when an op's input doesn't pass coercion/length checks."""
    pass


def _coerce_floats(value, field_name):
    """Accept list[number], ndarray, or CSV/whitespace string. Returns list[float]."""
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


def _require_finite(values, field_name):
    arr = np.asarray(values, dtype=float)
    if not np.all(np.isfinite(arr)):
        raise ValidationError(f"{field_name} contains NaN or infinite values")
    return arr


def _require_not_constant(arr, field_name):
    arr = np.asarray(arr, dtype=float)
    if arr.size == 0 or float(np.ptp(arr)) == 0.0:
        raise ValidationError(f"{field_name} is constant; the statistic is undefined for a constant series")


def _safe_float(v, default=None):
    """float, or `default` (None -> JSON null) for NaN / inf / non-numeric."""
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


def _require_statsmodels():
    if not STATSMODELS_OK:
        raise ValidationError("statsmodels is not installed in the venv")


# ---- version-tolerant test wrappers ----------------------------------------
# statsmodels 0.15 warns that adfuller / kpss will return result objects instead
# of tuples in 0.16; the object form exists from 0.15 on (result_object=True).
# These helpers read the fields by name there and fall back to the tuple on 0.14.

def _adf(x, **kw):
    """-> (statistic, pvalue, lags, nobs, critical_values dict)"""
    try:
        r = adfuller(x, result_object=True, **kw)
        return r.statistic, r.pvalue, r.lags, r.nobs, dict(r.critical_values)
    except TypeError as e:
        if "result_object" not in str(e):
            raise
    r = adfuller(x, **kw)
    return r[0], r[1], r[2], r[3], dict(r[4])


def _kpss(x, **kw):
    """-> (statistic, pvalue, lags, critical_values dict)"""
    try:
        r = kpss(x, result_object=True, **kw)
        return r.statistic, r.pvalue, r.lags, dict(r.critical_values)
    except TypeError as e:
        if "result_object" not in str(e):
            raise
    r = kpss(x, **kw)
    return r[0], r[1], r[2], dict(r[3])


def _crit(d, key):
    return _safe_float(d.get(key)) if isinstance(d, dict) else None


def _with_dates(data, n):
    """Optional `dates` list (same length as the series) -> list[str] or None."""
    dates = data.get("dates")
    if not dates:
        return None
    if not isinstance(dates, (list, tuple)) or len(dates) != n:
        raise ValidationError(f"dates must be a list with one entry per observation ({n})")
    return [str(d)[:10] for d in dates]


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_check_status(_data):
    return {
        "statsmodels": STATSMODELS_OK,
        "scipy": SCIPY_OK,
        "backend": "statsmodels + scipy",
        "ops_available": list(OPERATIONS.keys()),
    }


def op_ols(data):
    """
    OLS regression with full diagnostic suite.
    Inputs:
      y           : list[float]    (dependent, required, >= 10)
      x           : list[float] | list[list[float]]   (regressors; if 1-D treated as a single column)
      add_constant: bool           (default True)
    """
    _require_statsmodels()
    y_vals = _coerce_floats(data.get("y"), "y")
    _require_min_length(y_vals, 10, "y")
    add_const = bool(data.get("add_constant", True))

    raw_x = data.get("x")
    if raw_x is None:
        raise ValidationError("x is required (regressor matrix or single column)")

    # Normalize x to a 2-D list
    if isinstance(raw_x, list) and raw_x and isinstance(raw_x[0], (list, tuple)):
        # 2-D: list of rows
        x_rows = [_coerce_floats(row, f"x[{i}]") for i, row in enumerate(raw_x)]
        if len(x_rows) != len(y_vals):
            raise ValidationError(
                f"x has {len(x_rows)} rows but y has {len(y_vals)} observations")
        n_features = len(x_rows[0]) if x_rows else 0
        for i, r in enumerate(x_rows):
            if len(r) != n_features:
                raise ValidationError(f"x[{i}] has {len(r)} columns, expected {n_features}")
        X = np.asarray(x_rows, dtype=float)
    else:
        # 1-D: single regressor column
        x_vals = _coerce_floats(raw_x, "x")
        if len(x_vals) != len(y_vals):
            raise ValidationError(
                f"x ({len(x_vals)}) and y ({len(y_vals)}) must have the same length")
        X = np.asarray(x_vals, dtype=float).reshape(-1, 1)
        n_features = 1

    y = _require_finite(y_vals, "y")
    if not np.all(np.isfinite(X)):
        raise ValidationError("x contains NaN or infinite values")
    if add_const:
        constant_cols = [i + 1 for i in range(X.shape[1]) if float(np.ptp(X[:, i])) == 0.0]
        if constant_cols:
            names = ", ".join(f"x{i}" for i in constant_cols)
            raise ValidationError(
                f"regressor {names} is constant, so it is collinear with the intercept; "
                f"remove it or set add_constant to false")
        # has_constant="add": never let statsmodels silently skip the intercept.
        X_mat = sm.add_constant(X, has_constant="add")
    else:
        X_mat = X
    model = sm.OLS(y, X_mat).fit()

    coef_names = []
    if add_const:
        coef_names.append("const")
    coef_names.extend([f"x{i + 1}" for i in range(n_features)])

    ci = np.asarray(model.conf_int())
    coefficients = []
    for i, name in enumerate(coef_names):
        p_i = _safe_float(model.pvalues[i])
        coefficients.append({
            "name": name,
            "estimate": _safe_float(model.params[i]),
            "std_error": _safe_float(model.bse[i]),
            "t_stat": _safe_float(model.tvalues[i]),
            "p_value": p_i,
            "ci_lower": _safe_float(ci[i][0]),
            "ci_upper": _safe_float(ci[i][1]),
            "significant_5pct": bool(p_i < 0.05) if p_i is not None else False,
        })

    # Durbin-Watson + Breusch-Pagan
    dw = _safe_float(durbin_watson(model.resid))
    try:
        bp_lm, bp_lm_p, _, _ = het_breuschpagan(model.resid, X_mat)
    except Exception:
        bp_lm, bp_lm_p = float("nan"), float("nan")

    return {
        "n_observations": int(model.nobs),
        "n_features": n_features,
        "has_constant": add_const,
        "coefficients": coefficients,
        "r_squared": _safe_float(model.rsquared),
        "adj_r_squared": _safe_float(model.rsquared_adj),
        "f_statistic": _safe_float(model.fvalue),
        "f_p_value": _safe_float(model.f_pvalue),
        "aic": _safe_float(model.aic),
        "bic": _safe_float(model.bic),
        "log_likelihood": _safe_float(model.llf),
        "residual_std_error": _safe_float(np.sqrt(model.mse_resid)),
        "df_residual": _safe_int(model.df_resid),
        "df_model": _safe_int(model.df_model),
        "durbin_watson": dw,
        "dw_interpretation": (
            None if dw is None
            else "no autocorrelation" if 1.5 <= dw <= 2.5
            else "positive autocorrelation" if dw < 1.5
            else "negative autocorrelation"),
        "breusch_pagan_lm": _safe_float(bp_lm),
        "breusch_pagan_p": _safe_float(bp_lm_p),
        "homoscedastic_5pct": bool(bp_lm_p > 0.05) if not math.isnan(bp_lm_p) else None,
    }


def op_arima(data):
    """
    Fit an ARIMA(p,d,q) model and produce out-of-sample forecasts.
    Inputs:
      values  : list[float]  (required, >= 30)
      p, d, q : int          (default 1, 1, 1)
      horizon : int          (default 14)
    In-sample diagnostics (R^2, residual mean / std, Ljung-Box) skip the first d
    residuals: with differencing they are the raw level of the series (the state
    space filter has no history to predict from), not one-step forecast errors.
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    p = max(0, _safe_int(data.get("p", 1)))
    d = max(0, _safe_int(data.get("d", 1)))
    q = max(0, _safe_int(data.get("q", 1)))
    horizon = max(1, min(_safe_int(data.get("horizon", 14)), 365))

    arr = _require_finite(values, "values")
    _require_not_constant(arr, "values")
    if len(arr) - d < max(20, p + q + 10):
        raise ValidationError(
            f"ARIMA({p},{d},{q}) needs more data: only {len(arr) - d} observations remain after differencing")
    try:
        model = ARIMA(arr, order=(p, d, q)).fit()
    except Exception as e:
        raise ValidationError(f"ARIMA({p},{d},{q}) fit failed: {e}")

    resid_all = np.asarray(model.resid, dtype=float)
    resid = resid_all[d:]                       # drop the d start-up residuals (raw levels)
    target = arr[d:]
    ss_res = float(np.sum(resid ** 2))
    ss_tot = float(np.sum((target - target.mean()) ** 2))
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else None

    # Forecast with prediction intervals
    fc_res = model.get_forecast(steps=horizon)
    point = np.asarray(fc_res.predicted_mean, dtype=float)
    ci = np.asarray(fc_res.conf_int(alpha=0.05), dtype=float)
    forecast = []
    for i in range(horizon):
        forecast.append({
            "step": i + 1,
            "point": _safe_float(point[i]),
            "lower_95": _safe_float(ci[i][0]),
            "upper_95": _safe_float(ci[i][1]),
        })

    # Residual diagnostics (Ljung-Box at lag 10; at least 20 residuals are guaranteed above)
    lb_lag = 10
    try:
        from statsmodels.stats.diagnostic import acorr_ljungbox
        lb = acorr_ljungbox(resid, lags=[lb_lag], return_df=True)
        lb_stat = _safe_float(lb["lb_stat"].iloc[0])
        lb_p = _safe_float(lb["lb_pvalue"].iloc[0])
    except Exception:
        lb_stat, lb_p = None, None

    return {
        "n_observations": int(len(arr)),
        "order": [p, d, q],
        "horizon": horizon,
        "aic": _safe_float(model.aic),
        "bic": _safe_float(model.bic),
        "hqic": _safe_float(model.hqic),
        "log_likelihood": _safe_float(model.llf),
        "in_sample_r2": _safe_float(r2),
        "residual_mean": _safe_float(float(resid.mean())),
        "residual_std": _safe_float(float(resid.std())),
        "n_residuals_used": int(len(resid)),
        "ljung_box_lag": lb_lag,
        "ljung_box_lag10_stat": lb_stat,
        "ljung_box_lag10_p": lb_p,
        "ljung_box_residuals_white_noise": (bool(lb_p > 0.05) if lb_p is not None else None),
        "params": [
            {"name": str(name), "estimate": _safe_float(val)}
            for name, val in zip(model.param_names, model.params)
        ],
        "last_actual": _safe_float(arr[-1]),
        "first_forecast": _safe_float(point[0]) if len(point) else None,
        "last_forecast": _safe_float(point[-1]) if len(point) else None,
        "forecast": forecast,
    }


def op_stationarity_tests(data):
    """
    ADF + KPSS at a single differencing order, with full critical-value tables.
    Inputs:
      values         : list[float]   (required, >= 30)
      d              : int           (differencing order, default 0)
      adf_regression : 'c' | 'ct' | 'ctt' | 'n'   (default 'c')
      kpss_regression: 'c' | 'ct'    (default 'c')
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    d = max(0, min(3, _safe_int(data.get("d", 0))))
    adf_reg = str(data.get("adf_regression", "c"))
    kpss_reg = str(data.get("kpss_regression", "c"))
    if adf_reg not in ("c", "ct", "ctt", "n"):
        raise ValidationError(f"adf_regression must be c, ct, ctt or n, got {adf_reg!r}")
    if kpss_reg not in ("c", "ct"):
        raise ValidationError(f"kpss_regression must be c or ct, got {kpss_reg!r}")

    s = pd.Series(_require_finite(values, "values"), dtype=float)
    for _ in range(d):
        s = s.diff().dropna()
    if len(s) < 10:
        raise ValidationError(
            f"after differencing d={d}, only {len(s)} observations remain")
    _require_not_constant(s.to_numpy(), "the (differenced) series")

    try:
        adf_stat, adf_p, adf_lag, adf_nobs, adf_crit = _adf(s, autolag="AIC", regression=adf_reg)
    except Exception as e:
        raise ValidationError(f"ADF test failed: {e}")
    try:
        with warnings.catch_warnings():
            # p-value outside the KPSS table range is clipped to the table end; that is fine here.
            warnings.simplefilter("ignore")
            kpss_stat, kpss_p, kpss_lag, kpss_crit = _kpss(s, regression=kpss_reg, nlags="auto")
    except Exception as e:
        raise ValidationError(f"KPSS test failed: {e}")
    adf_stationary = bool(adf_p < 0.05)
    kpss_stationary = bool(kpss_p >= 0.05)

    verdict = "STATIONARY" if (adf_stationary and kpss_stationary) else "NON-STATIONARY"
    if adf_stationary and not kpss_stationary:
        verdict = "DIFFERENCE-STATIONARY (trend present)"
    elif kpss_stationary and not adf_stationary:
        verdict = "TREND-STATIONARY (no unit root rejected)"

    return {
        "n_observations": int(len(s)),
        "differencing_order": d,
        "verdict": verdict,
        "adf": {
            "statistic": _safe_float(adf_stat),
            "p_value": _safe_float(adf_p),
            "lags_used": _safe_int(adf_lag),
            "n_obs_used": _safe_int(adf_nobs),
            "critical_1pct": _crit(adf_crit, "1%"),
            "critical_5pct": _crit(adf_crit, "5%"),
            "critical_10pct": _crit(adf_crit, "10%"),
            "stationary_5pct": adf_stationary,
            "regression": adf_reg,
        },
        "kpss": {
            "statistic": _safe_float(kpss_stat),
            "p_value": _safe_float(kpss_p),
            "lags_used": _safe_int(kpss_lag),
            "critical_1pct": _crit(kpss_crit, "1%"),
            "critical_5pct": _crit(kpss_crit, "5%"),
            "critical_10pct": _crit(kpss_crit, "10%"),
            "stationary_5pct": kpss_stationary,
            "regression": kpss_reg,
        },
    }


def op_acf_pacf(data):
    """
    ACF and PACF together with significance bands — used for ARIMA(p,q) order selection.
    Inputs:
      values : list[float]  (required, >= 20)
      nlags  : int          (default 30)
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 20, "values")
    nlags = max(2, min(_safe_int(data.get("nlags", 30)), len(values) // 2))

    arr = _require_finite(values, "values")
    _require_not_constant(arr, "values")
    try:
        acf_vals, acf_confint = acf(arr, nlags=nlags, alpha=0.05, fft=True)
    except Exception as e:
        raise ValidationError(f"ACF failed: {e}")
    try:
        pacf_vals, pacf_confint = pacf(arr, nlags=nlags, alpha=0.05)
    except Exception as e:
        raise ValidationError(f"PACF failed: {e}")

    # 95% significance band ≈ ±1.96 / sqrt(n)
    sig_band = 1.96 / math.sqrt(len(arr))

    lags = []
    significant_acf_lags = []
    significant_pacf_lags = []
    for k in range(len(acf_vals)):
        a_v = float(acf_vals[k])
        p_v = float(pacf_vals[k]) if k < len(pacf_vals) else float("nan")
        a_sig = abs(a_v) > sig_band
        p_sig = (not math.isnan(p_v)) and abs(p_v) > sig_band
        if k > 0:
            if a_sig:
                significant_acf_lags.append(k)
            if p_sig:
                significant_pacf_lags.append(k)
        lags.append({
            "lag": k,
            "acf": _safe_float(a_v),
            "pacf": _safe_float(p_v),
            "acf_significant": a_sig,
            "pacf_significant": p_sig,
        })

    # Crude ARIMA suggestion: count significant lags before first cutoff
    suggested_p = next((k - 1 for k in range(1, len(lags))
                        if not lags[k]["pacf_significant"]), 0)
    suggested_q = next((k - 1 for k in range(1, len(lags))
                        if not lags[k]["acf_significant"]), 0)
    suggested_p = max(0, min(suggested_p, 5))
    suggested_q = max(0, min(suggested_q, 5))

    return {
        "n_observations": len(arr),
        "nlags": nlags,
        "significance_band_95pct": _safe_float(sig_band),
        "lags": lags,
        "n_significant_acf_lags": len(significant_acf_lags),
        "n_significant_pacf_lags": len(significant_pacf_lags),
        "first_significant_acf_lag": significant_acf_lags[0] if significant_acf_lags else None,
        "first_significant_pacf_lag": significant_pacf_lags[0] if significant_pacf_lags else None,
        "suggested_arima_p": int(suggested_p),
        "suggested_arima_q": int(suggested_q),
    }


def op_granger_causality(data):
    """
    Test whether one series Granger-causes another.
    Inputs:
      y       : list[float]   (effect series, required, >= 30)
      x       : list[float]   (potential cause, required, same length)
      max_lag : int           (default 4)
    """
    _require_statsmodels()
    y_vals = _coerce_floats(data.get("y"), "y")
    x_vals = _coerce_floats(data.get("x"), "x")
    _require_min_length(y_vals, 30, "y")
    if len(y_vals) != len(x_vals):
        raise ValidationError(
            f"y ({len(y_vals)}) and x ({len(x_vals)}) must have the same length")
    max_lag = max(1, min(_safe_int(data.get("max_lag", 4)), 20))
    y_arr = _require_finite(y_vals, "y")
    x_arr = _require_finite(x_vals, "x")
    _require_not_constant(y_arr, "y")
    _require_not_constant(x_arr, "x")
    if len(y_arr) < 3 * max_lag + 10:
        raise ValidationError(
            f"max_lag {max_lag} needs at least {3 * max_lag + 10} observations, got {len(y_arr)}")

    arr = np.column_stack([y_arr, x_arr])
    # statsmodels 0.15 removed the `verbose` argument (passing it raises TypeError);
    # 0.14 still has it and prints the test tables to stdout by default. Pass it only
    # where it exists and keep any printing off our stdout, which must hold one JSON payload.
    kw = {"verbose": False} if "verbose" in inspect.signature(grangercausalitytests).parameters else {}
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            results = grangercausalitytests(arr, maxlag=max_lag, **kw)
    except Exception as e:
        raise ValidationError(f"Granger test failed: {e}")

    rows = []
    causal_lags = []
    for lag in range(1, max_lag + 1):
        if lag not in results:
            continue
        f_test = results[lag][0]["ssr_ftest"]
        chi2_test = results[lag][0]["ssr_chi2test"]
        f_stat, f_p = float(f_test[0]), float(f_test[1])
        chi2_stat, chi2_p = float(chi2_test[0]), float(chi2_test[1])
        causal = f_p < 0.05
        if causal:
            causal_lags.append(lag)
        rows.append({
            "lag": lag,
            "f_statistic": _safe_float(f_stat),
            "f_p_value": _safe_float(f_p),
            "chi2_statistic": _safe_float(chi2_stat),
            "chi2_p_value": _safe_float(chi2_p),
            "x_causes_y_5pct": bool(causal),
        })

    return {
        "n_observations": len(y_vals),
        "max_lag": max_lag,
        "causal_lags": causal_lags,
        "x_causes_y_at_any_lag": len(causal_lags) > 0,
        "tests": rows,
        "interpretation": (
            f"x Granger-causes y at lag(s) {causal_lags}" if causal_lags
            else "no Granger-causality from x to y at any tested lag"),
    }


def op_descriptive(data):
    """
    Descriptive statistics + normality tests (Jarque-Bera, Shapiro-Wilk).
    Inputs:
      values : list[float]  (required, >= 8)
    Shape statistics and normality tests are null for a constant series.
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 8, "values")

    arr = _require_finite(values, "values")
    n = len(arr)
    constant = float(np.ptp(arr)) == 0.0

    mean = float(arr.mean())
    median = float(np.median(arr))
    std = float(arr.std(ddof=1))
    var = float(arr.var(ddof=1))
    skew = kurt = None
    if SCIPY_OK and not constant:
        skew = _safe_float(scstats.skew(arr))
        kurt = _safe_float(scstats.kurtosis(arr))
    p25, p50, p75 = np.percentile(arr, [25, 50, 75])
    iqr = float(p75 - p25)

    # Jarque-Bera
    jb_stat, jb_p = float("nan"), float("nan")
    if not constant:
        try:
            jb_stat, jb_p, _, _ = jb_test(arr)
        except Exception:
            pass

    # Shapiro-Wilk (scipy)
    sw_stat, sw_p = float("nan"), float("nan")
    if SCIPY_OK and n <= 5000 and not constant:
        try:
            sw_stat, sw_p = scstats.shapiro(arr)
        except Exception:
            pass

    return {
        "n_observations": n,
        "mean": _safe_float(mean),
        "median": _safe_float(median),
        "std": _safe_float(std),
        "variance": _safe_float(var),
        "min": _safe_float(float(arr.min())),
        "max": _safe_float(float(arr.max())),
        "range": _safe_float(float(arr.max() - arr.min())),
        "p25": _safe_float(p25),
        "p50": _safe_float(p50),
        "p75": _safe_float(p75),
        "iqr": _safe_float(iqr),
        "skewness": skew,
        "kurtosis_excess": kurt,
        "sum": _safe_float(float(arr.sum())),
        "coefficient_of_variation": _safe_float(std / mean) if abs(mean) > 1e-12 else None,
        "jarque_bera_stat": _safe_float(jb_stat),
        "jarque_bera_p": _safe_float(jb_p),
        "jarque_bera_normal_5pct": bool(jb_p > 0.05) if not math.isnan(jb_p) else None,
        "shapiro_wilk_stat": _safe_float(sw_stat),
        "shapiro_wilk_p": _safe_float(sw_p),
        "shapiro_wilk_normal_5pct": (
            bool(sw_p > 0.05) if not math.isnan(sw_p) else None),
    }


# ----------------------------------------------------------------------------
# Pair / regime / factor / decomposition ops
# ----------------------------------------------------------------------------

def _paired_series(data, min_n, y_key="y", x_key="x"):
    y_vals = _coerce_floats(data.get(y_key), y_key)
    x_vals = _coerce_floats(data.get(x_key), x_key)
    _require_min_length(y_vals, min_n, y_key)
    if len(y_vals) != len(x_vals):
        raise ValidationError(
            f"{y_key} ({len(y_vals)}) and {x_key} ({len(x_vals)}) must have the same length")
    y = _require_finite(y_vals, y_key)
    x = _require_finite(x_vals, x_key)
    _require_not_constant(y, y_key)
    _require_not_constant(x, x_key)
    return y, x


def _deterministic_terms(n, trend):
    """Columns for the cointegrating regression's deterministic part."""
    t = np.arange(1, n + 1, dtype=float)
    cols = []
    if trend in ("c", "ct", "ctt"):
        cols.append(np.ones(n))
    if trend in ("ct", "ctt"):
        cols.append(t)
    if trend == "ctt":
        cols.append(t ** 2)
    return np.column_stack(cols) if cols else np.empty((n, 0))


def op_cointegration(data):
    """
    Engle-Granger cointegration test of two price (level) series, plus the hedge ratio
    and spread statistics a pairs trader needs.
    Inputs:
      y      : list[float]  (dependent leg, levels, >= 30)
      x      : list[float]  (independent leg, same length)
      trend  : 'c' | 'ct' | 'ctt' | 'n'   (deterministic terms, default 'c')
    The decision uses the Engle-Granger (MacKinnon) p-value `p_value`. `spread_adf_p` is a
    plain ADF p-value on the fitted spread, shown for reference only: it ignores that the
    hedge ratio was estimated and is therefore too optimistic.
    """
    _require_statsmodels()
    y, x = _paired_series(data, 30)
    n = len(y)
    trend = str(data.get("trend", "c")).lower()
    if trend not in ("c", "ct", "ctt", "n"):
        raise ValidationError(f"trend must be c, ct, ctt or n, got {trend!r}")

    try:
        res = coint(y, x, trend=trend, autolag="aic")
    except Exception as e:
        raise ValidationError(f"cointegration test failed: {e}")
    t_stat, p_value, crit = float(res[0]), float(res[1]), np.asarray(res[2], dtype=float).ravel()

    # Cointegrating regression y = deterministic + beta * x (same terms as the test)
    D = _deterministic_terms(n, trend)
    X = np.column_stack([D, x]) if D.shape[1] else x.reshape(-1, 1)
    ols = sm.OLS(y, X).fit()
    hedge_ratio = float(ols.params[-1])
    intercept = float(ols.params[0]) if trend != "n" else 0.0
    spread = np.asarray(ols.resid, dtype=float)  # y - deterministic - beta*x

    try:
        sp_stat, sp_p, _, _, _ = _adf(spread, autolag="AIC", regression="n")
    except Exception:
        sp_stat, sp_p = float("nan"), float("nan")

    # Mean-reversion half-life of the spread: d(s_t) = a + b s_{t-1}; HL = -ln2 / ln(1 + b)
    ds = np.diff(spread)
    hl_fit = sm.OLS(ds, sm.add_constant(spread[:-1], has_constant="add")).fit()
    b = float(hl_fit.params[1])
    half_life = (-math.log(2.0) / math.log(1.0 + b)) if -1.0 < b < 0.0 else None

    sp_mean = float(spread.mean())
    sp_std = float(spread.std(ddof=1))
    tail = spread[-1000:]
    return {
        "n_observations": n,
        "trend": trend,
        "method": "Engle-Granger two-step (statsmodels coint, MacKinnon p-values)",
        "t_statistic": _safe_float(t_stat),
        "p_value": _safe_float(p_value),
        "critical_1pct": _safe_float(crit[0]) if crit.size > 0 else None,
        "critical_5pct": _safe_float(crit[1]) if crit.size > 1 else None,
        "critical_10pct": _safe_float(crit[2]) if crit.size > 2 else None,
        "cointegrated_5pct": bool(p_value < 0.05),
        "hedge_ratio": _safe_float(hedge_ratio),
        "intercept": _safe_float(intercept),
        "regression_r_squared": _safe_float(ols.rsquared),
        "spread_definition": "y - intercept [- trend terms] - hedge_ratio * x",
        "spread_adf_statistic": _safe_float(sp_stat),
        "spread_adf_p": _safe_float(sp_p),
        "spread_mean": _safe_float(sp_mean),
        "spread_std": _safe_float(sp_std),
        "spread_last": _safe_float(spread[-1]),
        "spread_zscore_last": _safe_float((spread[-1] - sp_mean) / sp_std) if sp_std > 0 else None,
        "half_life_periods": _safe_float(half_life),
        "spread_start_index": int(n - len(tail)),
        "spread": [_safe_float(v) for v in tail],
    }


def op_rolling_ols(data):
    """
    Rolling OLS of y on x with an intercept (rolling beta / hedge ratio).
    Inputs:
      y      : list[float]  (dependent, e.g. asset returns, >= 20)
      x      : list[float]  (regressor, e.g. benchmark returns, same length)
      window : int          (default 60; 10 <= window <= len(y))
      dates  : list[str]    (optional, one per observation)
    points[i] is the regression over observations i-window+1 .. i; the first window-1
    rows have null alpha / beta / r_squared (warm-up).
    """
    _require_statsmodels()
    y, x = _paired_series(data, 20)
    n = len(y)
    window = _safe_int(data.get("window", 60), 60)
    if window < 10:
        raise ValidationError(f"window must be at least 10, got {window}")
    if window > n:
        raise ValidationError(f"window ({window}) is longer than the series ({n} observations)")
    dates = _with_dates(data, n)

    from statsmodels.regression.rolling import RollingOLS
    X = sm.add_constant(x.reshape(-1, 1), has_constant="add")
    try:
        res = RollingOLS(y, X, window=window).fit()
    except Exception as e:
        raise ValidationError(f"rolling OLS failed: {e}")
    params = np.asarray(res.params, dtype=float)
    r2 = np.asarray(res.rsquared, dtype=float)

    points = []
    for i in range(n):
        row = {
            "index": i,
            "alpha": _safe_float(params[i, 0]),
            "beta": _safe_float(params[i, 1]),
            "r_squared": _safe_float(r2[i]),
        }
        if dates:
            row["date"] = dates[i]
        points.append(row)

    betas = params[:, 1][np.isfinite(params[:, 1])]
    if betas.size == 0:
        raise ValidationError("no window produced an estimate (is x constant inside every window?)")
    last = n - 1
    return {
        "window": window,
        "n_observations": n,
        "n_estimates": int(betas.size),
        "points": points,
        "last_beta": _safe_float(params[last, 1]),
        "last_alpha": _safe_float(params[last, 0]),
        "last_r_squared": _safe_float(r2[last]),
        "beta_min": _safe_float(betas.min()),
        "beta_max": _safe_float(betas.max()),
        "beta_mean": _safe_float(betas.mean()),
        "beta_std": _safe_float(betas.std(ddof=1)) if betas.size > 1 else None,
    }


def op_regime_switching(data):
    """
    Markov-switching mean/variance model (Hamilton) on a return series.
    Inputs:
      values             : list[float]  (returns, >= max(60, 30 * k_regimes))
      k_regimes          : int          (2..4, default 2)
      switching_variance : bool         (default True)
    Regimes are re-labelled in ascending order of variance (regime 0 = calmest).
    transition_matrix[i][j] = P(regime j tomorrow | regime i today) (rows sum to 1).
    smoothed_probabilities[t][i] = P(regime i at t | all data).
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    k = _safe_int(data.get("k_regimes", 2), 2)
    if not (2 <= k <= 4):
        raise ValidationError(f"k_regimes must be between 2 and 4, got {k}")
    min_n = max(60, 30 * k)
    _require_min_length(values, min_n, "values")
    switching_variance = bool(data.get("switching_variance", True))
    arr = _require_finite(values, "values")
    _require_not_constant(arr, "values")
    n = len(arr)

    # Fit on a rescaled series (unit standard deviation): the optimiser is far more
    # reliable on O(1) numbers than on daily returns of ~0.01. Means / variances and the
    # log-likelihood are mapped back to the original scale below.
    scale = float(arr.std(ddof=1))
    z = arr / scale

    from statsmodels.tsa.regime_switching.markov_regression import MarkovRegression
    rng_state = np.random.get_state()
    try:
        np.random.seed(12345)  # search_reps draws random starting values: keep runs reproducible
        model = MarkovRegression(z, k_regimes=k, trend="c", switching_variance=switching_variance)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            res = model.fit(search_reps=20, disp=False)
    except Exception as e:
        raise ValidationError(f"regime-switching fit failed: {e}")
    finally:
        np.random.set_state(rng_state)

    params = pd.Series(np.asarray(res.params, dtype=float), index=list(model.param_names))
    means = np.array([params[f"const[{i}]"] for i in range(k)]) * scale
    if switching_variance:
        variances = np.array([params[f"sigma2[{i}]"] for i in range(k)]) * scale ** 2
    else:
        variances = np.full(k, params["sigma2"] * scale ** 2)

    # statsmodels: regime_transition[i, j] = P(S_t = i | S_{t-1} = j) (columns sum to 1)
    rt = np.asarray(res.regime_transition, dtype=float)[:, :, 0]
    P = rt.T  # P[i, j] = P(next = j | current = i)

    order = np.argsort(variances, kind="stable") if switching_variance else np.argsort(means, kind="stable")
    means, variances = means[order], variances[order]
    P = P[np.ix_(order, order)]
    smoothed = np.asarray(res.smoothed_marginal_probabilities, dtype=float)[:, order]

    regimes = []
    for i in range(k):
        stay = P[i, i]
        regimes.append({
            "regime": i,
            "mean": _safe_float(means[i]),
            "variance": _safe_float(variances[i]),
            "std": _safe_float(math.sqrt(variances[i])) if variances[i] >= 0 else None,
            "expected_duration": _safe_float(1.0 / (1.0 - stay)) if stay < 1.0 else None,
            "share_of_time": _safe_float(float(np.mean(np.argmax(smoothed, axis=1) == i))),
        })

    # Log-likelihood of the original data: llf(z) - n * ln(scale)
    llf = float(res.llf) - n * math.log(scale)
    n_params = len(res.params)
    converged = bool(res.mle_retvals.get("converged", True)) if isinstance(res.mle_retvals, dict) else True
    current = int(np.argmax(smoothed[-1]))
    out = {
        "n_observations": n,
        "k_regimes": k,
        "switching_variance": switching_variance,
        "regime_order": "ascending variance" if switching_variance else "ascending mean",
        "regimes": regimes,
        "transition_matrix": [[_safe_float(P[i, j]) for j in range(k)] for i in range(k)],
        "smoothed_probabilities": [[_safe_float(v) for v in row] for row in smoothed],
        "current_regime": current,
        "current_regime_probability": _safe_float(smoothed[-1, current]),
        "log_likelihood": _safe_float(llf),
        "aic": _safe_float(-2.0 * llf + 2.0 * n_params),
        "bic": _safe_float(-2.0 * llf + n_params * math.log(n)),
        "converged": converged,
    }
    if not converged:
        out["_warning"] = "The regime-switching likelihood optimiser did not report convergence; treat the estimates with caution."
    return out


def op_pca(data):
    """
    Principal component analysis of a panel of series (e.g. asset returns).
    Inputs:
      series       : {name: [values...]} (>= 2 series, same length, >= 20 rows)
      n_components : int (default min(3, n_series))
      standardize  : bool (default True: correlation-matrix PCA; False: covariance)
      input_type   : 'returns' (default) | 'prices' (converted to simple returns first)
    Eigen-decomposition of the correlation (or covariance) matrix. Loadings are unit
    eigenvectors, sign-fixed so each component's loadings sum to a non-negative number.
    """
    raw = data.get("series") or data.get("returns")
    if not isinstance(raw, dict) or len(raw) < 2:
        raise ValidationError("series must be an object with at least 2 named series: {\"AAPL\": [...], ...}")
    cols = {str(k): _coerce_floats(v, f"series[{k}]") for k, v in raw.items()}
    lengths = {len(v) for v in cols.values()}
    if len(lengths) != 1:
        raise ValidationError(f"all series must have the same length, got {sorted(lengths)}")
    df = pd.DataFrame(cols)
    input_type = str(data.get("input_type", "returns")).lower()
    if input_type == "prices":
        if (df <= 0).any().any():
            raise ValidationError("input_type 'prices' needs strictly positive values")
        df = df.pct_change().iloc[1:]
    elif input_type != "returns":
        raise ValidationError(f"input_type must be returns or prices, got {input_type!r}")
    if not np.all(np.isfinite(df.to_numpy(dtype=float))):
        raise ValidationError("series contain NaN or infinite values")
    n_obs, n_assets = df.shape
    if n_obs < 20:
        raise ValidationError(f"PCA needs at least 20 observations per series, got {n_obs}")
    stds = df.std(ddof=1)
    flat = [c for c in df.columns if not stds[c] > 0]
    if flat:
        raise ValidationError(f"series with zero variance: {', '.join(flat)}")
    standardize = bool(data.get("standardize", True))
    n_comp = _safe_int(data.get("n_components", min(3, n_assets)), min(3, n_assets))
    if not (1 <= n_comp <= n_assets):
        raise ValidationError(f"n_components must be between 1 and {n_assets}, got {n_comp}")

    X = df.to_numpy(dtype=float)
    X = X - X.mean(axis=0)
    if standardize:
        X = X / X.std(axis=0, ddof=1)
    C = np.cov(X, rowvar=False)
    eigvals, eigvecs = np.linalg.eigh(C)
    idx = np.argsort(eigvals)[::-1]
    eigvals, eigvecs = eigvals[idx], eigvecs[:, idx]
    eigvals = np.clip(eigvals, 0.0, None)
    for j in range(eigvecs.shape[1]):
        if eigvecs[:, j].sum() < 0:
            eigvecs[:, j] = -eigvecs[:, j]
    total = float(eigvals.sum())
    ratio = eigvals / total if total > 0 else np.full_like(eigvals, np.nan)
    scores_last = X[-1] @ eigvecs[:, :n_comp]

    names = list(df.columns)
    loadings = []
    for i, a in enumerate(names):
        row = {"asset": a}
        for j in range(n_comp):
            row[f"pc{j + 1}"] = _safe_float(eigvecs[i, j])
        loadings.append(row)
    return {
        "n_observations": int(n_obs),
        "n_assets": int(n_assets),
        "n_components": int(n_comp),
        "standardize": standardize,
        "input_type": input_type,
        "matrix": "correlation" if standardize else "covariance",
        "explained_variance_ratio": [_safe_float(v) for v in ratio[:n_comp]],
        "cumulative_explained_variance": [_safe_float(v) for v in np.cumsum(ratio)[:n_comp]],
        "eigenvalues": [_safe_float(v) for v in eigvals[:n_comp]],
        "all_eigenvalues": [_safe_float(v) for v in eigvals],
        "loadings": loadings,
        "last_scores": [_safe_float(v) for v in scores_last],
    }


def _stamp(dates, i, n):
    return dates[i] if dates else f"obs {i}"


def _curve(values, dates, max_points=300):
    """[{date, value}] sampled to ~max_points, always including the last point
    (same shape as functime_service's seasonality curves)."""
    n = len(values)
    if n == 0:
        return []
    step = max(1, n // max_points)
    out = [{"date": _stamp(dates, i, n), "value": _safe_float(values[i])} for i in range(0, n, step)]
    if out[-1]["date"] != _stamp(dates, n - 1, n):
        out.append({"date": _stamp(dates, n - 1, n), "value": _safe_float(values[n - 1])})
    return out


def _detect_period(arr):
    """Dominant seasonal period from the first strong ACF peak, or None."""
    n = len(arr)
    max_lag = min(n // 2, 366)
    if max_lag < 4:
        return None
    ac = acf(arr - np.mean(arr), nlags=max_lag, fft=True)
    peaks = [(i, float(ac[i])) for i in range(2, len(ac) - 1)
             if ac[i] > ac[i - 1] and ac[i] > ac[i + 1] and ac[i] > 0.2]
    if not peaks:
        return None
    peaks.sort(key=lambda p: -p[1])
    return int(peaks[0][0])


def op_decompose(data):
    """
    STL decomposition into trend + seasonal + residual (same output shape as the
    functime "seasonality" op).
    Inputs:
      values : list[float]  (required; at least 2 full periods + 1, and >= 24)
      period : int          (season length; 0 / omitted = detect from the ACF)
      robust : bool         (default True: robust STL, down-weights outliers)
      dates  : list[str]    (optional, one per observation)
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 24, "values")
    arr = _require_finite(values, "values")
    _require_not_constant(arr, "values")
    n = len(arr)
    dates = _with_dates(data, n)

    period = _safe_int(data.get("period", 0), 0)
    detected = False
    if period < 2:
        period = _detect_period(arr)
        if period is None:
            raise ValidationError(
                "no dominant seasonal period found in the autocorrelation; pass `period` explicitly")
        detected = True
    if n < 2 * period + 1:
        raise ValidationError(
            f"a period of {period} needs at least {2 * period + 1} observations (two full seasons), got {n}")

    from statsmodels.tsa.seasonal import STL
    robust = bool(data.get("robust", True))
    try:
        stl = STL(arr, period=period, robust=robust).fit()
    except Exception as e:
        raise ValidationError(f"STL decomposition failed: {e}")
    trend = np.asarray(stl.trend, dtype=float)
    seasonal = np.asarray(stl.seasonal, dtype=float)
    resid = np.asarray(stl.resid, dtype=float)

    # Strength of trend / seasonality (Hyndman & Athanasopoulos, FPP3 sec. 4.3)
    var_resid = float(np.var(resid, ddof=1))
    var_detrended = float(np.var(seasonal + resid, ddof=1))
    var_deseasonalized = float(np.var(trend + resid, ddof=1))
    trend_strength = max(0.0, 1.0 - var_resid / var_deseasonalized) if var_deseasonalized > 0 else None
    seasonal_strength = max(0.0, 1.0 - var_resid / var_detrended) if var_detrended > 0 else None

    return {
        "n_observations": n,
        "period": int(period),
        "period_auto_detected": detected,
        "robust": robust,
        "method": "STL (statsmodels)",
        "trend_strength": _safe_float(trend_strength),
        "seasonal_strength": _safe_float(seasonal_strength),
        "residual_std": _safe_float(float(np.std(resid, ddof=1))),
        "history": _curve(arr, dates),
        "trend": _curve(trend, dates),
        "seasonal": _curve(seasonal, dates),
        "residual": _curve(resid, dates),
    }


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "check_status": op_check_status,
    "ols": op_ols,
    "arima": op_arima,
    "stationarity_tests": op_stationarity_tests,
    "acf_pacf": op_acf_pacf,
    "granger_causality": op_granger_causality,
    "descriptive": op_descriptive,
    "cointegration": op_cointegration,
    "rolling_ols": op_rolling_ols,
    "regime_switching": op_regime_switching,
    "pca": op_pca,
    "decompose": op_decompose,
}


def dispatch(operation, data):
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
            "error": "Usage: statsmodels_service.py <operation> [json_data]",
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
