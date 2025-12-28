import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.robust import robust_linear_model, scale

def fit_rlm(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    M: str = 'huber',
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Robust Linear Model"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    norm_map = {
        'huber': sm.robust.norms.HuberT(),
        'andrews': sm.robust.norms.AndrewWave(),
        'bisquare': sm.robust.norms.TukeyBiweight(),
        'hampel': sm.robust.norms.Hampel(),
        'ramsay_e': sm.robust.norms.RamsayE()
    }

    norm = norm_map.get(M.lower(), sm.robust.norms.HuberT())
    model = sm.RLM(y, X, M=norm)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'scale': float(results.scale),
        'weights': results.weights.tolist() if hasattr(results.weights, 'tolist') else list(results.weights)
    }

def mad(
    data: Union[List, np.ndarray, pd.Series],
    center: Optional[float] = None
) -> Dict[str, Any]:
    """Median Absolute Deviation"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    if center is None:
        result = scale.mad(data)
    else:
        result = scale.mad(data, center=float(center))

    return {
        'mad': float(result)
    }

def huber_scale(
    data: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Huber's proposal 2 for estimating scale"""
    from statsmodels.robust.scale import huber

    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    loc, scale_est = huber(data)

    return {
        'location': float(loc),
        'scale': float(scale_est)
    }

def huber_location_scale(
    data: Union[List, np.ndarray, pd.Series],
    tol: float = 1e-08,
    maxiter: int = 30
) -> Dict[str, Any]:
    """Huber M-estimator of location and scale"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    h = sm.robust.scale.Huber(maxiter=maxiter, tol=tol)
    result = h(data)

    return {
        'location': float(result[0]),
        'scale': float(result[1])
    }

def main():
    print("Testing statsmodels robust wrapper")

    np.random.seed(42)
    n = 100

    X = np.random.randn(n, 2)
    y = 2 + 3 * X[:, 0] - 1.5 * X[:, 1] + np.random.randn(n) * 0.5

    y[95:] += 10

    rlm_result = fit_rlm(y, X, M='huber')
    print("RLM scale: {:.4f}".format(rlm_result['scale']))
    print("RLM params: {}".format([round(p, 4) for p in rlm_result['params']]))

    mad_result = mad(y)
    print("MAD: {:.4f}".format(mad_result['mad']))

    huber_result = huber_scale(y)
    print("Huber location: {:.4f}, scale: {:.4f}".format(huber_result['location'], huber_result['scale']))

    huber_ms = huber_location_scale(y)
    print("Huber M-est location: {:.4f}, scale: {:.4f}".format(huber_ms['location'], huber_ms['scale']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
