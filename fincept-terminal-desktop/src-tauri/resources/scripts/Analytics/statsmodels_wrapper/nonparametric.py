import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.nonparametric import kde, smoothers_lowess
from statsmodels.nonparametric.kernel_regression import KernelReg
from statsmodels.nonparametric.kernel_density import KDEMultivariate

def kernel_density_estimation(
    data: Union[List, np.ndarray, pd.Series],
    kernel: str = 'gau',
    bw: str = 'normal_reference'
) -> Dict[str, Any]:
    """Univariate Kernel Density Estimation"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    kde_model = kde.KDEUnivariate(data)
    kde_model.fit(kernel=kernel, bw=bw)

    return {
        'support': kde_model.support.tolist() if hasattr(kde_model.support, 'tolist') else list(kde_model.support),
        'density': kde_model.density.tolist() if hasattr(kde_model.density, 'tolist') else list(kde_model.density),
        'cdf': kde_model.cdf.tolist() if hasattr(kde_model.cdf, 'tolist') else list(kde_model.cdf),
        'bw': float(kde_model.bw)
    }

def kernel_regression(
    endog: Union[List, np.ndarray, pd.Series],
    exog: Union[List, np.ndarray, pd.DataFrame],
    var_type: str = 'c',
    reg_type: str = 'll',
    bw: str = 'cv_ls'
) -> Dict[str, Any]:
    """Kernel Regression"""
    endog = np.array(endog) if not isinstance(endog, (np.ndarray, pd.Series)) else endog
    exog = np.array(exog) if not isinstance(exog, (np.ndarray, pd.DataFrame)) else exog

    if exog.ndim == 1:
        exog = exog.reshape(-1, 1)

    kr = KernelReg(endog, exog, var_type=var_type, reg_type=reg_type, bw=bw)

    fitted, mfx = kr.fit()

    return {
        'fitted': fitted.tolist() if hasattr(fitted, 'tolist') else list(fitted),
        'marginal_effects': mfx.tolist() if hasattr(mfx, 'tolist') else [[float(x) for x in row] for row in mfx],
        'bw': kr.bw.tolist() if hasattr(kr.bw, 'tolist') else list(kr.bw)
    }

def lowess_smoothing(
    endog: Union[List, np.ndarray, pd.Series],
    exog: Union[List, np.ndarray, pd.Series],
    frac: float = 0.667,
    it: int = 3
) -> Dict[str, Any]:
    """LOWESS (Locally Weighted Scatterplot Smoothing)"""
    endog = np.array(endog) if not isinstance(endog, (np.ndarray, pd.Series)) else endog
    exog = np.array(exog) if not isinstance(exog, (np.ndarray, pd.Series)) else exog

    result = smoothers_lowess.lowess(endog, exog, frac=frac, it=it)

    return {
        'x': result[:, 0].tolist() if result.shape[1] > 0 else [],
        'y_smoothed': result[:, 1].tolist() if result.shape[1] > 1 else []
    }

def multivariate_kde(
    data: Union[np.ndarray, pd.DataFrame],
    var_type: str = 'c',
    bw: str = 'normal_reference'
) -> Dict[str, Any]:
    """Multivariate Kernel Density Estimation"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.DataFrame)) else data

    if data.ndim == 1:
        data = data.reshape(-1, 1)

    var_type_str = var_type * data.shape[1] if len(var_type) == 1 else var_type

    kde_model = KDEMultivariate(data, var_type=var_type_str, bw=bw)

    return {
        'bw': kde_model.bw.tolist() if hasattr(kde_model.bw, 'tolist') else list(kde_model.bw),
        'data_predict': kde_model.pdf().tolist() if hasattr(kde_model.pdf(), 'tolist') else list(kde_model.pdf())
    }

def main():
    print("Testing statsmodels nonparametric wrapper")

    np.random.seed(42)
    n = 200

    data = np.random.normal(0, 1, n)
    kde_result = kernel_density_estimation(data)
    print("KDE support length: {}".format(len(kde_result['support'])))
    print("KDE bandwidth: {:.4f}".format(kde_result['bw']))

    x = np.linspace(0, 10, n)
    y = np.sin(x) + np.random.randn(n) * 0.2
    kr_result = kernel_regression(y, x, var_type='c', reg_type='ll', bw='cv_ls')
    print("Kernel regression fitted length: {}".format(len(kr_result['fitted'])))

    lowess_result = lowess_smoothing(y, x, frac=0.3)
    print("LOWESS smoothed length: {}".format(len(lowess_result['y_smoothed'])))

    mv_data = np.random.randn(n, 2)
    mvkde_result = multivariate_kde(mv_data, var_type='cc')
    print("Multivariate KDE bandwidth: {}".format(len(mvkde_result['bw'])))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
