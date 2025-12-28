import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.multivariate import pca, factor, manova, cancorr

def perform_pca(
    data: Union[np.ndarray, pd.DataFrame],
    ncomp: Optional[int] = None,
    standardize: bool = True
) -> Dict[str, Any]:
    """Principal Component Analysis"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    pca_model = pca.PCA(data, ncomp=ncomp, standardize=standardize)

    return {
        'eigenvalues': pca_model.eigenvals.tolist() if hasattr(pca_model.eigenvals, 'tolist') else list(pca_model.eigenvals),
        'eigenvectors': pca_model.eigenvecs.values.tolist() if hasattr(pca_model.eigenvecs, 'values') else [[float(x) for x in row] for row in pca_model.eigenvecs],
        'loadings': pca_model.loadings.values.tolist() if hasattr(pca_model.loadings, 'values') else [[float(x) for x in row] for row in pca_model.loadings],
        'scores': pca_model.scores.values.tolist() if hasattr(pca_model.scores, 'values') else [[float(x) for x in row] for row in pca_model.scores],
        'rsquare': pca_model.rsquare.tolist() if hasattr(pca_model.rsquare, 'tolist') else list(pca_model.rsquare)
    }

def perform_factor_analysis(
    data: Union[np.ndarray, pd.DataFrame],
    n_factor: int = 1,
    method: str = 'pa'
) -> Dict[str, Any]:
    """Factor Analysis"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    fa = factor.Factor(data, n_factor=n_factor, method=method)
    result = fa.fit()

    return {
        'loadings': result.loadings.values.tolist() if hasattr(result.loadings, 'values') else [[float(x) for x in row] for row in result.loadings],
        'uniqueness': result.uniqueness.tolist() if hasattr(result.uniqueness, 'tolist') else list(result.uniqueness),
        'eigenvalues': result.eigenvals.tolist() if hasattr(result.eigenvals, 'tolist') else list(result.eigenvals)
    }

def perform_manova(
    data: pd.DataFrame,
    endog_vars: List[str],
    exog_vars: List[str]
) -> Dict[str, Any]:
    """Multivariate Analysis of Variance"""
    from statsmodels.formula.api import ols

    formula = ' + '.join(endog_vars) + ' ~ ' + ' + '.join(exog_vars)

    manova_model = manova.MANOVA.from_formula(formula, data=data)
    result = manova_model.mv_test()

    return {
        'summary': str(result)
    }

def canonical_correlation(
    x: Union[np.ndarray, pd.DataFrame],
    y: Union[np.ndarray, pd.DataFrame]
) -> Dict[str, Any]:
    """Canonical Correlation Analysis"""
    x = pd.DataFrame(x) if not isinstance(x, pd.DataFrame) else x
    y = pd.DataFrame(y) if not isinstance(y, pd.DataFrame) else y

    cc = cancorr.CanCorr(x, y)

    return {
        'cancorr': cc.cancorr.tolist() if hasattr(cc.cancorr, 'tolist') else list(cc.cancorr),
        'x_cancoef': cc.x_cancoef.values.tolist() if hasattr(cc.x_cancoef, 'values') else [[float(v) for v in row] for row in cc.x_cancoef],
        'y_cancoef': cc.y_cancoef.values.tolist() if hasattr(cc.y_cancoef, 'values') else [[float(v) for v in row] for row in cc.y_cancoef]
    }

def main():
    print("Testing statsmodels multivariate wrapper")

    np.random.seed(42)
    n = 100

    X = np.random.randn(n, 5)
    pca_result = perform_pca(X, ncomp=3)
    print("PCA eigenvalues: {}".format(len(pca_result['eigenvalues'])))
    print("PCA loadings shape: {}x{}".format(len(pca_result['loadings']), len(pca_result['loadings'][0])))

    fa_result = perform_factor_analysis(X, n_factor=2)
    print("Factor loadings shape: {}x{}".format(len(fa_result['loadings']), len(fa_result['loadings'][0])))

    X1 = np.random.randn(n, 2)
    X2 = np.random.randn(n, 2)
    cc_result = canonical_correlation(X1, X2)
    print("Canonical correlations: {}".format(len(cc_result['cancorr'])))

    data = pd.DataFrame({
        'y1': np.random.randn(n),
        'y2': np.random.randn(n),
        'x1': np.random.choice(['A', 'B'], n),
        'x2': np.random.randn(n)
    })
    manova_result = perform_manova(data, ['y1', 'y2'], ['x1', 'x2'])
    print("MANOVA completed")

    print("Test: PASSED")

if __name__ == "__main__":
    main()
