import inspect
import math
import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.tsa import stattools, seasonal


def _json_safe(obj):
    """Plain-Python copy of obj that json.dumps accepts: numpy/pandas values converted, NaN/inf -> None."""
    if isinstance(obj, dict):
        return {str(k): _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, pd.DataFrame):
        return [_json_safe(row) for row in obj.values.tolist()]
    if isinstance(obj, (pd.Series, pd.Index)):
        return [_json_safe(v) for v in obj.tolist()]
    if isinstance(obj, np.ndarray):
        return _json_safe(obj.tolist())
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    if isinstance(obj, (bool, np.bool_)):
        return bool(obj)
    if isinstance(obj, (int, np.integer)):
        return int(obj)
    if isinstance(obj, (float, np.floating)):
        return float(obj) if math.isfinite(obj) else None
    return obj


def _call_stattools(func, *args, **kwargs):
    """Call a stattools function asking for its result object when this statsmodels version has one.

    statsmodels 0.15 warns that adfuller/kpss/acf/pacf/ccf stop returning plain tuples in 0.16;
    callers below read the fields by name and fall back to the legacy tuple layout.
    """
    try:
        if 'result_object' in inspect.signature(func).parameters:
            kwargs['result_object'] = True
    except (TypeError, ValueError):
        pass
    return func(*args, **kwargs)


def _values_and_confint(result, name, alpha):
    """(values, confint) from an acf/pacf/ccf result object or its legacy return value."""
    if hasattr(result, name):
        return getattr(result, name), getattr(result, 'confint', None)
    if alpha:
        return result[0], result[1]
    return result, None


def calculate_acf(
    data: Union[List, np.ndarray, pd.Series],
    nlags: Optional[int] = None,
    alpha: Optional[float] = None,
    fft: bool = False
) -> Dict[str, Any]:
    """Calculate autocorrelation function"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    if nlags is None:
        nlags = min(len(data) // 2, 40)

    result = _call_stattools(stattools.acf, data, nlags=nlags, alpha=alpha, fft=fft)
    acf_vals, confint = _values_and_confint(result, 'acf', alpha)

    out = {'acf': acf_vals}
    if alpha:
        out['confint'] = confint
    return _json_safe(out)

def calculate_pacf(
    data: Union[List, np.ndarray, pd.Series],
    nlags: Optional[int] = None,
    method: str = 'ywm',
    alpha: Optional[float] = None
) -> Dict[str, Any]:
    """Calculate partial autocorrelation function"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    if nlags is None:
        nlags = min(len(data) // 2, 40)

    result = _call_stattools(stattools.pacf, data, nlags=nlags, method=method, alpha=alpha)
    pacf_vals, confint = _values_and_confint(result, 'pacf', alpha)

    out = {'pacf': pacf_vals}
    if alpha:
        out['confint'] = confint
    return _json_safe(out)

def calculate_ccf(
    x: Union[List, np.ndarray, pd.Series],
    y: Union[List, np.ndarray, pd.Series],
    nlags: Optional[int] = None
) -> Dict[str, Any]:
    """Calculate cross-correlation function"""
    x = np.array(x) if not isinstance(x, (np.ndarray, pd.Series)) else x
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y

    result = _call_stattools(stattools.ccf, x, y, nlags=nlags)
    ccf_vals, _ = _values_and_confint(result, 'ccf', None)

    return _json_safe({'ccf': ccf_vals})

def adf_test(
    data: Union[List, np.ndarray, pd.Series],
    maxlag: Optional[int] = None,
    regression: str = 'c',
    autolag: Optional[str] = 'AIC'
) -> Dict[str, Any]:
    """Augmented Dickey-Fuller test for unit root"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = _call_stattools(stattools.adfuller, data, maxlag=maxlag, regression=regression, autolag=autolag)

    if hasattr(result, 'statistic'):  # ADFullerResult (statsmodels >= 0.15 with result_object)
        stat, pvalue, usedlag, nobs = result.statistic, result.pvalue, result.lags, result.nobs
        crit, icbest = result.critical_values, result.icbest
    else:  # legacy tuple: (adf, pvalue, usedlag, nobs, critvalues[, icbest])
        stat, pvalue, usedlag, nobs, crit = result[:5]
        icbest = result[5] if len(result) > 5 else None

    return _json_safe({
        'adf_statistic': float(stat),
        'pvalue': float(pvalue),
        'usedlag': int(usedlag),
        'nobs': int(nobs),
        'critical_values': {k: float(v) for k, v in crit.items()},
        'icbest': float(icbest) if icbest is not None else None
    })

def kpss_test(
    data: Union[List, np.ndarray, pd.Series],
    regression: str = 'c',
    nlags: Optional[Union[int, str]] = 'auto'
) -> Dict[str, Any]:
    """KPSS test for stationarity. nlags: int, 'auto' (default) or 'legacy'; None means 'auto'."""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data
    if nlags is None:
        nlags = 'auto'  # statsmodels >= 0.15 rejects None

    result = _call_stattools(stattools.kpss, data, regression=regression, nlags=nlags)

    if hasattr(result, 'statistic'):  # KPSSResult
        stat, pvalue, lags, crit = result.statistic, result.pvalue, result.lags, result.critical_values
    else:  # legacy tuple: (kpss_stat, p_value, lags, crit)
        stat, pvalue, lags, crit = result[:4]

    return _json_safe({
        'kpss_statistic': float(stat),
        'pvalue': float(pvalue),
        'lags': int(lags),
        'critical_values': {k: float(v) for k, v in crit.items()}
    })

def coint_test(
    y0: Union[List, np.ndarray, pd.Series],
    y1: Union[List, np.ndarray, pd.Series],
    trend: str = 'c',
    maxlag: Optional[int] = None,
    autolag: Optional[str] = 'aic'
) -> Dict[str, Any]:
    """Engle-Granger cointegration test"""
    y0 = np.array(y0) if not isinstance(y0, (np.ndarray, pd.Series)) else y0
    y1 = np.array(y1) if not isinstance(y1, (np.ndarray, pd.Series)) else y1

    result = stattools.coint(y0, y1, trend=trend, maxlag=maxlag, autolag=autolag)

    return _json_safe({
        'coint_t': float(result[0]),
        'pvalue': float(result[1]),
        'critical_values': result[2]
    })

def bds_test(
    data: Union[List, np.ndarray, pd.Series],
    max_dim: int = 2,
    epsilon: Optional[float] = None,
    distance: float = 1.5
) -> Dict[str, Any]:
    """BDS test for independence"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.bds(data, max_dim=max_dim, epsilon=epsilon, distance=distance)

    return _json_safe({
        'statistic': np.asarray(result[0]),
        'pvalue': np.asarray(result[1])
    })

def q_stat(
    data: Union[List, np.ndarray, pd.Series],
    nobs: Optional[int] = None
) -> Dict[str, Any]:
    """Ljung-Box Q statistic for lags 1..40 (fewer for short series)"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    if nobs is None:
        nobs = len(data)

    nlags = max(1, min(40, len(data) - 1))
    acf_vals, _ = _values_and_confint(_call_stattools(stattools.acf, data, nlags=nlags), 'acf', None)
    result = stattools.q_stat(np.asarray(acf_vals)[1:], nobs=nobs)

    return _json_safe({
        'q_stat': result[0],
        'pvalue': result[1]
    })

def acorr_ljungbox(
    data: Union[List, np.ndarray, pd.Series],
    lags: Optional[Union[int, List[int]]] = None,
    return_df: bool = False
) -> Dict[str, Any]:
    """Ljung-Box test for autocorrelation"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = sm.stats.acorr_ljungbox(data, lags=lags, return_df=return_df)

    if isinstance(result, pd.DataFrame):
        return _json_safe({'lb_stat': result['lb_stat'], 'lb_pvalue': result['lb_pvalue']})
    return _json_safe({'lb_stat': result[0], 'lb_pvalue': result[1]})

def seasonal_decompose_additive(
    data: Union[List, np.ndarray, pd.Series],
    period: int = 12,
    model: str = 'additive'
) -> Dict[str, Any]:
    """Seasonal decomposition.

    trend, seasonal and resid are aligned with the input: one entry per observation. The
    moving-average trend (and so resid) is undefined for period // 2 observations at each end;
    those entries are null.
    """
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    result = seasonal.seasonal_decompose(data, model=model, period=period)

    return _json_safe({
        'trend': result.trend,
        'seasonal': result.seasonal,
        'resid': result.resid
    })

def arma_order_select(
    data: Union[List, np.ndarray, pd.Series],
    max_ar: int = 4,
    max_ma: int = 2,
    ic: Union[str, List[str]] = 'aic'
) -> Dict[str, Any]:
    """Select ARMA order.

    Always evaluates AIC and BIC (plus any other criterion named in ic, e.g. 'hqic').
    The per-criterion tables are matrices: row p = AR order 0..max_ar, column q = MA order
    0..max_ma; null where the fit failed.
    """
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    criteria = ['aic', 'bic']
    for name in ([ic] if isinstance(ic, str) else list(ic or [])):
        if name not in criteria:
            criteria.append(name)

    result = stattools.arma_order_select_ic(data, max_ar=max_ar, max_ma=max_ma, ic=criteria)

    out = {}
    for name in criteria:
        out[f'{name}_min_order'] = [int(v) for v in result[f'{name}_min_order']]
    for name in criteria:
        out[name] = result[name].values
    return _json_safe(out)

def detrend_linear(
    data: Union[List, np.ndarray, pd.Series],
    order: int = 1
) -> Dict[str, Any]:
    """Remove linear trend"""
    from statsmodels.tsa.tsatools import detrend

    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = detrend(data, order=order)

    return _json_safe({'detrended': np.asarray(result)})

def add_trend_to_data(
    data: Union[List, np.ndarray, pd.Series],
    trend: str = 'c',
    prepend: bool = False
) -> Dict[str, Any]:
    """Add trend to data"""
    from statsmodels.tsa.tsatools import add_trend

    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = add_trend(data, trend=trend, prepend=prepend)

    return _json_safe({'data_with_trend': np.asarray(result)})

def main():
    print("Testing statsmodels timeseries functions wrapper")

    np.random.seed(42)
    n = 200

    ts_data = np.cumsum(np.random.randn(n))

    acf_result = calculate_acf(ts_data, nlags=20)
    print("ACF length: {}".format(len(acf_result['acf'])))

    pacf_result = calculate_pacf(ts_data, nlags=20)
    print("PACF length: {}".format(len(pacf_result['pacf'])))

    adf_result = adf_test(ts_data)
    print("ADF p-value: {:.4f}".format(adf_result['pvalue']))

    kpss_result = kpss_test(ts_data)
    print("KPSS p-value: {:.4f}".format(kpss_result['pvalue']))

    seasonal_data = np.sin(np.arange(n) * 2 * np.pi / 12) + np.random.randn(n) * 0.1 + np.arange(n) * 0.01
    decomp_result = seasonal_decompose_additive(seasonal_data, period=12)
    print("Decomposition trend length: {}".format(len(decomp_result['trend'])))

    ljungbox_result = acorr_ljungbox(ts_data, lags=10)
    print("Ljung-Box stats length: {}".format(len(ljungbox_result['lb_stat'])))

    ts_data2 = ts_data + np.random.randn(n) * 0.5
    ccf_result = calculate_ccf(ts_data, ts_data2, nlags=20)
    print("CCF length: {}".format(len(ccf_result['ccf'])))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
