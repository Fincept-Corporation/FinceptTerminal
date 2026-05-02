"""
Statsmodels Service (econometrics backend)
===========================================
Self-contained econometric service for the Fincept Terminal "Statsmodels"
sub-tab. Same response contract as gs_quant_service / functime_service.

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
import warnings
from typing import Dict, List, Any

import numpy as np
import pandas as pd

# Fail loudly only on truly missing essentials.
try:
    import statsmodels.api as sm
    from statsmodels.tsa.arima.model import ARIMA
    from statsmodels.tsa.stattools import adfuller, kpss, acf, pacf, grangercausalitytests
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


def _require_statsmodels():
    if not STATSMODELS_OK:
        raise ValidationError("statsmodels is not installed in the venv")


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

    y = np.asarray(y_vals, dtype=float)
    X_mat = sm.add_constant(X) if add_const else X
    model = sm.OLS(y, X_mat).fit()

    coef_names = []
    if add_const:
        coef_names.append("const")
    coef_names.extend([f"x{i + 1}" for i in range(n_features)])

    coefficients = []
    for i, name in enumerate(coef_names):
        coefficients.append({
            "name": name,
            "estimate": _safe_float(model.params[i]),
            "std_error": _safe_float(model.bse[i]),
            "t_stat": _safe_float(model.tvalues[i]),
            "p_value": _safe_float(model.pvalues[i]),
            "ci_lower": _safe_float(model.conf_int()[i][0]),
            "ci_upper": _safe_float(model.conf_int()[i][1]),
            "significant_5pct": bool(model.pvalues[i] < 0.05),
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
            "no autocorrelation" if 1.5 <= dw <= 2.5
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
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 30, "values")
    p = max(0, _safe_int(data.get("p", 1)))
    d = max(0, _safe_int(data.get("d", 1)))
    q = max(0, _safe_int(data.get("q", 1)))
    horizon = max(1, min(_safe_int(data.get("horizon", 14)), 365))

    arr = np.asarray(values, dtype=float)
    try:
        model = ARIMA(arr, order=(p, d, q)).fit()
    except Exception as e:
        raise ValidationError(f"ARIMA({p},{d},{q}) fit failed: {e}")

    fitted = model.fittedvalues
    resid = model.resid
    ss_res = float(np.sum(resid ** 2))
    ss_tot = float(np.sum((arr - arr.mean()) ** 2)) or 1.0
    r2 = 1.0 - ss_res / ss_tot

    # Forecast with prediction intervals
    fc_res = model.get_forecast(steps=horizon)
    point = fc_res.predicted_mean
    ci = fc_res.conf_int(alpha=0.05)
    forecast = []
    for i in range(horizon):
        lo = float(ci[i][0]) if ci.ndim == 2 else float(ci.iloc[i, 0])
        up = float(ci[i][1]) if ci.ndim == 2 else float(ci.iloc[i, 1])
        forecast.append({
            "step": i + 1,
            "point": _safe_float(point[i]),
            "lower_95": _safe_float(lo),
            "upper_95": _safe_float(up),
        })

    # Residual diagnostics (Ljung-Box at lag 10)
    try:
        from statsmodels.stats.diagnostic import acorr_ljungbox
        lb = acorr_ljungbox(resid, lags=[10], return_df=True)
        lb_stat = _safe_float(lb["lb_stat"].iloc[0])
        lb_p = _safe_float(lb["lb_pvalue"].iloc[0])
    except Exception:
        lb_stat, lb_p = 0.0, 0.0

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
        "ljung_box_lag10_stat": lb_stat,
        "ljung_box_lag10_p": lb_p,
        "ljung_box_residuals_white_noise": bool(lb_p > 0.05),
        "params": [
            {"name": str(name), "estimate": _safe_float(val)}
            for name, val in zip(model.param_names, model.params)
        ],
        "last_actual": _safe_float(arr[-1]),
        "first_forecast": _safe_float(point[0] if len(point) else 0.0),
        "last_forecast": _safe_float(point[-1] if len(point) else 0.0),
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

    s = pd.Series(values, dtype=float)
    for _ in range(d):
        s = s.diff().dropna()
    if len(s) < 10:
        raise ValidationError(
            f"after differencing d={d}, only {len(s)} observations remain")

    # ADF
    try:
        adf_stat, adf_p, adf_lag, adf_nobs, adf_crit, adf_icbest = adfuller(
            s, autolag="AIC", regression=adf_reg)
        adf_stationary = adf_p < 0.05
    except Exception as e:
        adf_stat, adf_p, adf_lag, adf_nobs, adf_crit = float("nan"), 1.0, 0, 0, {}
        adf_stationary = False

    # KPSS
    try:
        kpss_stat, kpss_p, kpss_lag, kpss_crit = kpss(
            s, regression=kpss_reg, nlags="auto")
        kpss_stationary = kpss_p >= 0.05
    except Exception:
        kpss_stat, kpss_p, kpss_lag, kpss_crit = float("nan"), 0.0, 0, {}
        kpss_stationary = False

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
            "critical_1pct": _safe_float(adf_crit.get("1%")) if isinstance(adf_crit, dict) else 0.0,
            "critical_5pct": _safe_float(adf_crit.get("5%")) if isinstance(adf_crit, dict) else 0.0,
            "critical_10pct": _safe_float(adf_crit.get("10%")) if isinstance(adf_crit, dict) else 0.0,
            "stationary_5pct": bool(adf_stationary),
            "regression": adf_reg,
        },
        "kpss": {
            "statistic": _safe_float(kpss_stat),
            "p_value": _safe_float(kpss_p),
            "lags_used": _safe_int(kpss_lag),
            "critical_1pct": _safe_float(kpss_crit.get("1%")) if isinstance(kpss_crit, dict) else 0.0,
            "critical_5pct": _safe_float(kpss_crit.get("5%")) if isinstance(kpss_crit, dict) else 0.0,
            "critical_10pct": _safe_float(kpss_crit.get("10%")) if isinstance(kpss_crit, dict) else 0.0,
            "stationary_5pct": bool(kpss_stationary),
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

    arr = np.asarray(values, dtype=float)
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

    arr = np.column_stack([y_vals, x_vals])
    try:
        # statsmodels prints chatty warnings; silence them
        results = grangercausalitytests(arr, maxlag=max_lag, verbose=False)
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
    """
    _require_statsmodels()
    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 8, "values")

    arr = np.asarray(values, dtype=float)
    n = len(arr)

    mean = float(arr.mean())
    median = float(np.median(arr))
    std = float(arr.std(ddof=1))
    var = float(arr.var(ddof=1))
    skew = _safe_float(scstats.skew(arr) if SCIPY_OK else 0.0)
    kurt = _safe_float(scstats.kurtosis(arr) if SCIPY_OK else 0.0)
    p25, p50, p75 = np.percentile(arr, [25, 50, 75])
    iqr = float(p75 - p25)

    # Jarque-Bera
    try:
        jb_stat, jb_p, jb_skew, jb_kurt = jb_test(arr)
    except Exception:
        jb_stat, jb_p, jb_skew, jb_kurt = float("nan"), 1.0, skew, kurt

    # Shapiro-Wilk (scipy)
    sw_stat, sw_p = float("nan"), float("nan")
    if SCIPY_OK and n <= 5000:
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
        "skewness": _safe_float(skew),
        "kurtosis_excess": _safe_float(kurt),
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
            "error": "Usage: statsmodels_service.py <operation> [json_data]",
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
