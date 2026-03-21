import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import pmdarima as pm
from pmdarima import utils

def calculate_acf(
    y: Union[List, np.ndarray, pd.Series],
    nlags: int = 40,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Calculate autocorrelation function"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    acf_vals, conf_int = pm.acf(y, nlags=nlags, alpha=alpha)

    return {
        'acf': acf_vals.tolist() if hasattr(acf_vals, 'tolist') else list(acf_vals),
        'conf_int': conf_int.tolist() if hasattr(conf_int, 'tolist') else [[float(x) for x in row] for row in conf_int]
    }

def calculate_pacf(
    y: Union[List, np.ndarray, pd.Series],
    nlags: int = 40,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Calculate partial autocorrelation function"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    pacf_vals, conf_int = pm.pacf(y, nlags=nlags, alpha=alpha)

    return {
        'pacf': pacf_vals.tolist() if hasattr(pacf_vals, 'tolist') else list(pacf_vals),
        'conf_int': conf_int.tolist() if hasattr(conf_int, 'tolist') else [[float(x) for x in row] for row in conf_int]
    }

def decompose_timeseries(
    y: Union[List, np.ndarray, pd.Series],
    type: str = 'additive',
    m: int = 1
) -> Dict[str, Any]:
    """Decompose time series into trend, seasonal, and residual"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    decomposition = pm.decompose(y, type_=type, m=m)

    return {
        'trend': decomposition.trend.tolist() if hasattr(decomposition.trend, 'tolist') else list(decomposition.trend),
        'seasonal': decomposition.seasonal.tolist() if hasattr(decomposition.seasonal, 'tolist') else list(decomposition.seasonal),
        'random': decomposition.random.tolist() if hasattr(decomposition.random, 'tolist') else list(decomposition.random)
    }

def difference_series(
    y: Union[List, np.ndarray, pd.Series],
    lag: int = 1,
    differences: int = 1
) -> Dict[str, Any]:
    """Difference a time series"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    y_diff = utils.diff(y, lag=lag, differences=differences)

    return {
        'differenced': y_diff.tolist() if hasattr(y_diff, 'tolist') else list(y_diff)
    }

def inverse_difference(
    y_diff: Union[List, np.ndarray, pd.Series],
    y_original: Union[List, np.ndarray, pd.Series],
    lag: int = 1,
    differences: int = 1
) -> Dict[str, Any]:
    """Inverse difference operation"""
    y_diff = pd.Series(y_diff) if not isinstance(y_diff, pd.Series) else y_diff
    y_original = pd.Series(y_original) if not isinstance(y_original, pd.Series) else y_original

    y_inv = utils.diff_inv(y_diff, lag=lag, differences=differences, xi=y_original[:lag*differences])

    return {
        'original': y_inv.tolist() if hasattr(y_inv, 'tolist') else list(y_inv)
    }

def smape_metric(
    y_true: Union[List, np.ndarray, pd.Series],
    y_pred: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Calculate Symmetric Mean Absolute Percentage Error"""
    y_true = np.array(y_true) if not isinstance(y_true, np.ndarray) else y_true
    y_pred = np.array(y_pred) if not isinstance(y_pred, np.ndarray) else y_pred

    from pmdarima import metrics
    smape = metrics.smape(y_true, y_pred)

    return {
        'smape': float(smape)
    }

def check_endogenous(
    y: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Validate endogenous variable"""
    from pmdarima import metrics

    y_checked = metrics.check_endog(y)

    return {
        'is_valid': True,
        'dtype': str(y_checked.dtype),
        'shape': y_checked.shape
    }

def create_c_array(
    *args
) -> Dict[str, Any]:
    """Create concatenated array (R-style c() function)"""
    result = pm.c(*args)

    return {
        'array': result.tolist() if hasattr(result, 'tolist') else list(result),
        'length': len(result)
    }

def main():
    print("Testing pmdarima utils wrapper")

    np.random.seed(42)
    y = np.cumsum(np.random.randn(100)) + 50

    acf_result = calculate_acf(y, nlags=20)
    print("ACF values count: {}".format(len(acf_result['acf'])))

    pacf_result = calculate_pacf(y, nlags=20)
    print("PACF values count: {}".format(len(pacf_result['pacf'])))

    decomp_result = decompose_timeseries(y, type='additive', m=12)
    print("Decomposition trend count: {}".format(len([x for x in decomp_result['trend'] if x is not None and not np.isnan(x)])))

    diff_result = difference_series(y, lag=1, differences=1)
    print("Differenced series count: {}".format(len(diff_result['differenced'])))

    inv_result = inverse_difference(diff_result['differenced'], y, lag=1, differences=1)
    print("Inverse differenced count: {}".format(len(inv_result['original'])))

    y_true = np.array([10, 20, 30, 40])
    y_pred = np.array([11, 19, 32, 38])
    smape_result = smape_metric(y_true, y_pred)
    print("SMAPE: {:.4f}".format(smape_result['smape']))

    check_result = check_endogenous(y)
    print("Endogenous check: {}".format(check_result['is_valid']))

    c_result = create_c_array(1, 2, 3, 4, 5)
    print("C array length: {}".format(c_result['length']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
