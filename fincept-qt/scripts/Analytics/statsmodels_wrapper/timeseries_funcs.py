import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.tsa import stattools, seasonal

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

    result = stattools.acf(data, nlags=nlags, alpha=alpha, fft=fft)

    if alpha:
        acf_vals, confint = result
        return {
            'acf': acf_vals.tolist() if hasattr(acf_vals, 'tolist') else list(acf_vals),
            'confint': confint.tolist() if hasattr(confint, 'tolist') else [[float(x) for x in row] for row in confint]
        }
    else:
        return {
            'acf': result.tolist() if hasattr(result, 'tolist') else list(result)
        }

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

    result = stattools.pacf(data, nlags=nlags, method=method, alpha=alpha)

    if alpha:
        pacf_vals, confint = result
        return {
            'pacf': pacf_vals.tolist() if hasattr(pacf_vals, 'tolist') else list(pacf_vals),
            'confint': confint.tolist() if hasattr(confint, 'tolist') else [[float(x) for x in row] for row in confint]
        }
    else:
        return {
            'pacf': result.tolist() if hasattr(result, 'tolist') else list(result)
        }

def calculate_ccf(
    x: Union[List, np.ndarray, pd.Series],
    y: Union[List, np.ndarray, pd.Series],
    nlags: Optional[int] = None
) -> Dict[str, Any]:
    """Calculate cross-correlation function"""
    x = np.array(x) if not isinstance(x, (np.ndarray, pd.Series)) else x
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y

    result = stattools.ccf(x, y, nlags=nlags)

    return {
        'ccf': result.tolist() if hasattr(result, 'tolist') else list(result)
    }

def adf_test(
    data: Union[List, np.ndarray, pd.Series],
    maxlag: Optional[int] = None,
    regression: str = 'c',
    autolag: Optional[str] = 'AIC'
) -> Dict[str, Any]:
    """Augmented Dickey-Fuller test for unit root"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.adfuller(data, maxlag=maxlag, regression=regression, autolag=autolag)

    return {
        'adf_statistic': float(result[0]),
        'pvalue': float(result[1]),
        'usedlag': int(result[2]),
        'nobs': int(result[3]),
        'critical_values': {k: float(v) for k, v in result[4].items()},
        'icbest': float(result[5]) if len(result) > 5 else None
    }

def kpss_test(
    data: Union[List, np.ndarray, pd.Series],
    regression: str = 'c',
    nlags: Optional[int] = None
) -> Dict[str, Any]:
    """KPSS test for stationarity"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.kpss(data, regression=regression, nlags=nlags)

    return {
        'kpss_statistic': float(result[0]),
        'pvalue': float(result[1]),
        'lags': int(result[2]),
        'critical_values': {k: float(v) for k, v in result[3].items()}
    }

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

    return {
        'coint_t': float(result[0]),
        'pvalue': float(result[1]),
        'critical_values': result[2].tolist() if hasattr(result[2], 'tolist') else list(result[2])
    }

def bds_test(
    data: Union[List, np.ndarray, pd.Series],
    max_dim: int = 2,
    epsilon: Optional[float] = None,
    distance: float = 1.5
) -> Dict[str, Any]:
    """BDS test for independence"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.bds(data, max_dim=max_dim, epsilon=epsilon, distance=distance)

    return {
        'statistic': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'pvalue': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1])
    }

def q_stat(
    data: Union[List, np.ndarray, pd.Series],
    nobs: Optional[int] = None
) -> Dict[str, Any]:
    """Ljung-Box Q statistic"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    if nobs is None:
        nobs = len(data)

    result = stattools.q_stat(stattools.acf(data, nlags=40)[1:], nobs=nobs)

    return {
        'q_stat': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'pvalue': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1])
    }

def acorr_ljungbox(
    data: Union[List, np.ndarray, pd.Series],
    lags: Optional[Union[int, List[int]]] = None,
    return_df: bool = False
) -> Dict[str, Any]:
    """Ljung-Box test for autocorrelation"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = sm.stats.acorr_ljungbox(data, lags=lags, return_df=return_df)

    if return_df:
        return {
            'lb_stat': result['lb_stat'].tolist() if hasattr(result['lb_stat'], 'tolist') else list(result['lb_stat']),
            'lb_pvalue': result['lb_pvalue'].tolist() if hasattr(result['lb_pvalue'], 'tolist') else list(result['lb_pvalue'])
        }
    else:
        return {
            'lb_stat': result.iloc[:, 0].tolist() if hasattr(result.iloc[:, 0], 'tolist') else list(result.iloc[:, 0]),
            'lb_pvalue': result.iloc[:, 1].tolist() if hasattr(result.iloc[:, 1], 'tolist') else list(result.iloc[:, 1])
        }

def seasonal_decompose_additive(
    data: Union[List, np.ndarray, pd.Series],
    period: int = 12,
    model: str = 'additive'
) -> Dict[str, Any]:
    """Seasonal decomposition"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    result = seasonal.seasonal_decompose(data, model=model, period=period)

    return {
        'trend': result.trend.dropna().tolist() if hasattr(result.trend, 'tolist') else list(result.trend.dropna()),
        'seasonal': result.seasonal.tolist() if hasattr(result.seasonal, 'tolist') else list(result.seasonal),
        'resid': result.resid.dropna().tolist() if hasattr(result.resid, 'tolist') else list(result.resid.dropna())
    }

def arma_order_select(
    data: Union[List, np.ndarray, pd.Series],
    max_ar: int = 4,
    max_ma: int = 2,
    ic: str = 'aic'
) -> Dict[str, Any]:
    """Select ARMA order"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.arma_order_select_ic(data, max_ar=max_ar, max_ma=max_ma, ic=ic)

    return {
        'aic_min_order': list(result.aic_min_order),
        'bic_min_order': list(result.bic_min_order),
        'aic': {str(k): float(v) for k, v in result.aic.items()},
        'bic': {str(k): float(v) for k, v in result.bic.items()}
    }

def detrend_linear(
    data: Union[List, np.ndarray, pd.Series],
    order: int = 1
) -> Dict[str, Any]:
    """Remove linear trend"""
    from statsmodels.tsa.tsatools import detrend

    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = detrend(data, order=order)

    return {
        'detrended': result.tolist() if hasattr(result, 'tolist') else list(result)
    }

def add_trend_to_data(
    data: Union[List, np.ndarray, pd.Series],
    trend: str = 'c',
    prepend: bool = False
) -> Dict[str, Any]:
    """Add trend to data"""
    from statsmodels.tsa.tsatools import add_trend

    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = add_trend(data, trend=trend, prepend=prepend)

    return {
        'data_with_trend': result.tolist() if hasattr(result, 'tolist') else [[float(x) for x in row] for row in result]
    }

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
