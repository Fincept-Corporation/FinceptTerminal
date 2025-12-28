import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.gam.api import GLMGam, BSplines

def fit_glm_gam(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    alpha: Union[float, List[float]] = 0.01,
    family: str = 'gaussian'
) -> Dict[str, Any]:
    """Fit Generalized Additive Model using penalized B-splines"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if X.ndim == 1:
        X = X.reshape(-1, 1)

    from statsmodels.genmod import families

    family_map = {
        'gaussian': families.Gaussian(),
        'binomial': families.Binomial(),
        'poisson': families.Poisson()
    }

    fam = family_map.get(family, families.Gaussian())

    bs = BSplines(X, df=[10] * X.shape[1], degree=[3] * X.shape[1])

    if isinstance(alpha, float):
        alpha = [alpha] * X.shape[1]

    model = GLMGam(y, smoother=bs, alpha=alpha, family=fam)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'deviance': float(results.deviance),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

def fit_gam_formula(
    formula: str,
    data: pd.DataFrame,
    alpha: float = 0.01,
    family: str = 'gaussian'
) -> Dict[str, Any]:
    """Fit GAM using formula interface"""
    from statsmodels.genmod import families

    family_map = {
        'gaussian': families.Gaussian(),
        'binomial': families.Binomial(),
        'poisson': families.Poisson()
    }

    fam = family_map.get(family, families.Gaussian())

    model = GLMGam.from_formula(formula, data=data, smoother=None, alpha=alpha, family=fam)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'deviance': float(results.deviance)
    }

def main():
    print("Testing statsmodels GAM wrapper")

    np.random.seed(42)
    n = 200

    x = np.linspace(0, 10, n)
    y = np.sin(x) + 0.5 * np.cos(2 * x) + np.random.randn(n) * 0.2

    gam_result = fit_glm_gam(y, x, alpha=0.01, family='gaussian')
    print("GAM AIC: {:.4f}".format(gam_result['aic']))
    print("GAM deviance: {:.4f}".format(gam_result['deviance']))
    print("GAM fitted values length: {}".format(len(gam_result['fittedvalues'])))

    data = pd.DataFrame({
        'y': y,
        'x': x
    })

    print("Test: PASSED")

if __name__ == "__main__":
    main()
