import math
import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.regression.rolling import RollingOLS, RollingWLS


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


def fit_ols(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Ordinary Least Squares regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.OLS(y, X)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'bse': results.bse,
        'tvalues': results.tvalues,
        'pvalues': results.pvalues,
        'rsquared': float(results.rsquared),
        'rsquared_adj': float(results.rsquared_adj),
        'fvalue': float(results.fvalue),
        'f_pvalue': float(results.f_pvalue),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'nobs': int(results.nobs)
    })

def fit_wls(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    weights: Union[List, np.ndarray],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Weighted Least Squares regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    weights = np.array(weights) if not isinstance(weights, np.ndarray) else weights

    if add_constant:
        X = sm.add_constant(X)

    model = sm.WLS(y, X, weights=weights)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'bse': results.bse,
        'tvalues': results.tvalues,
        'pvalues': results.pvalues,
        'rsquared': float(results.rsquared),
        'rsquared_adj': float(results.rsquared_adj),
        'aic': float(results.aic),
        'bic': float(results.bic)
    })

def fit_gls(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    sigma: Optional[np.ndarray] = None,
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Generalized Least Squares regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.GLS(y, X, sigma=sigma)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'bse': results.bse,
        'tvalues': results.tvalues,
        'pvalues': results.pvalues,
        'aic': float(results.aic),
        'bic': float(results.bic)
    })

def fit_glsar(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    rho: int = 1,
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit GLS with AR(rho) errors (iterative). 'rho' in the result is the estimated AR coefficients."""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.GLSAR(y, X, rho=rho)
    results = model.iterative_fit()

    return _json_safe({
        'params': results.params,
        'bse': results.bse,
        'tvalues': results.tvalues,
        'pvalues': results.pvalues,
        # the AR coefficients live on the model; RegressionResults has no rho
        'rho': np.atleast_1d(model.rho)
    })

def fit_recursive_ls(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Recursive Least Squares"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.RecursiveLS(y, X)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'cusum': results.cusum,
        'cusum_squares': results.cusum_squares,
        'aic': float(results.aic),
        'bic': float(results.bic)
    })

def predict_ols(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    X_new: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit OLS and predict on new data"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    X_new = np.array(X_new) if not isinstance(X_new, (np.ndarray, pd.DataFrame)) else X_new

    if add_constant:
        X = sm.add_constant(X)
        X_new = sm.add_constant(X_new)

    model = sm.OLS(y, X)
    results = model.fit()
    predictions = results.predict(X_new)

    return _json_safe({
        'predictions': predictions,
        'params': results.params
    })

def regression_diagnostics(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Get comprehensive regression diagnostics"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.OLS(y, X)
    results = model.fit()

    from statsmodels.stats.stattools import jarque_bera

    jb_stat, jb_pval, _, _ = jarque_bera(results.resid)
    influence = results.get_influence()

    return _json_safe({
        'residuals': results.resid,
        'fitted_values': results.fittedvalues,
        'leverage': influence.hat_matrix_diag,
        'cooks_d': influence.cooks_distance[0],
        'condition_number': float(results.condition_number),
        'jarque_bera': {
            'statistic': float(jb_stat),
            'pvalue': float(jb_pval)
        },
        'durbin_watson': float(sm.stats.stattools.durbin_watson(results.resid))
    })

def fit_rolling_ols(
    y: Union[pd.Series, pd.DataFrame],
    X: Union[pd.Series, pd.DataFrame],
    window: int = 60,
    min_nobs: Optional[int] = None
) -> Dict[str, Any]:
    """Fit Rolling OLS regression.

    One row per observation; rows before the first full window (warm-up) are null.
    """
    y = pd.Series(y) if not isinstance(y, pd.Series) else y
    X = pd.DataFrame(X) if not isinstance(X, pd.DataFrame) else X

    model = RollingOLS(y, X, window=window, min_nobs=min_nobs)
    results = model.fit()

    return _json_safe({
        'params': np.asarray(results.params),
        'rsquared': np.asarray(results.rsquared)
    })

def fit_rolling_wls(
    y: Union[pd.Series, pd.DataFrame],
    X: Union[pd.Series, pd.DataFrame],
    window: int = 60,
    weights: Optional[pd.Series] = None,
    min_nobs: Optional[int] = None
) -> Dict[str, Any]:
    """Fit Rolling WLS regression.

    One row per observation; rows before the first full window (warm-up) are null.
    """
    y = pd.Series(y) if not isinstance(y, pd.Series) else y
    X = pd.DataFrame(X) if not isinstance(X, pd.DataFrame) else X

    model = RollingWLS(y, X, window=window, weights=weights, min_nobs=min_nobs)
    results = model.fit()

    return _json_safe({
        'params': np.asarray(results.params),
        'rsquared': np.asarray(results.rsquared)
    })

def main():
    print("Testing statsmodels regression wrapper")

    np.random.seed(42)
    n = 100
    X = np.random.randn(n, 2)
    y = 2 + 3 * X[:, 0] - 1.5 * X[:, 1] + np.random.randn(n) * 0.5

    result = fit_ols(y, X)
    print("OLS R-squared: {:.4f}".format(result['rsquared']))
    print("Params: {}".format(result['params']))

    weights = np.random.uniform(0.5, 1.5, n)
    result_wls = fit_wls(y, X, weights)
    print("WLS R-squared: {:.4f}".format(result_wls['rsquared']))

    X_new = np.random.randn(10, 2)
    pred_result = predict_ols(y, X, X_new)
    print("Predictions shape: {}".format(len(pred_result['predictions'])))

    diag = regression_diagnostics(y, X)
    print("Durbin-Watson: {:.4f}".format(diag['durbin_watson']))

    y_ts = pd.Series(y)
    X_ts = pd.DataFrame(X)
    rolling_result = fit_rolling_ols(y_ts, X_ts, window=50)
    print("Rolling OLS params shape: {}x{}".format(len(rolling_result['params']), len(rolling_result['params'][0])))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
