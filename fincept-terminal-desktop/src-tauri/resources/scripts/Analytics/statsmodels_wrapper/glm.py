import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.genmod import families

def fit_glm(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    family: str = 'gaussian',
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Generalized Linear Model"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    family_map = {
        'gaussian': families.Gaussian(),
        'binomial': families.Binomial(),
        'poisson': families.Poisson(),
        'gamma': families.Gamma(),
        'inverse_gaussian': families.InverseGaussian(),
        'negative_binomial': families.NegativeBinomial(),
        'tweedie': families.Tweedie()
    }

    fam = family_map.get(family, families.Gaussian())
    model = sm.GLM(y, X, family=fam)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'deviance': float(results.deviance),
        'pearson_chi2': float(results.pearson_chi2),
        'null_deviance': float(results.null_deviance)
    }

def fit_logit(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Logistic Regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.Logit(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'llf': float(results.llf),
        'pseudo_rsquared': float(results.prsquared)
    }

def predict_logit(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    X_new: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Logit and predict probabilities"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    X_new = np.array(X_new) if not isinstance(X_new, (np.ndarray, pd.DataFrame)) else X_new

    if add_constant:
        X = sm.add_constant(X)
        X_new = sm.add_constant(X_new)

    model = sm.Logit(y, X)
    results = model.fit(disp=0)
    predictions = results.predict(X_new)

    return {
        'probabilities': predictions.tolist() if hasattr(predictions, 'tolist') else list(predictions),
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params)
    }

def fit_probit(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Probit Regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.Probit(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'llf': float(results.llf),
        'pseudo_rsquared': float(results.prsquared)
    }

def fit_poisson(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Poisson Regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.Poisson(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'llf': float(results.llf)
    }

def fit_negative_binomial(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Negative Binomial Regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.NegativeBinomial(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'llf': float(results.llf),
        'alpha': float(results.params['alpha']) if 'alpha' in results.params else None
    }

def fit_mnlogit(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Multinomial Logit"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.MNLogit(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.values.tolist() if hasattr(results.params, 'values') else [[float(x) for x in row] for row in results.params],
        'aic': float(results.aic),
        'bic': float(results.bic),
        'llf': float(results.llf)
    }

def fit_quantreg(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    quantile: float = 0.5,
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Quantile Regression"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.QuantReg(y, X)
    results = model.fit(q=quantile)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'quantile': float(quantile)
    }

def fit_gee(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    groups: Union[List, np.ndarray, pd.Series],
    family: str = 'gaussian',
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Generalized Estimating Equations"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    groups = np.array(groups) if not isinstance(groups, (np.ndarray, pd.Series)) else groups

    if add_constant:
        X = sm.add_constant(X)

    family_map = {
        'gaussian': families.Gaussian(),
        'binomial': families.Binomial(),
        'poisson': families.Poisson()
    }

    fam = family_map.get(family, families.Gaussian())
    model = sm.GEE(y, X, groups=groups, family=fam)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues)
    }

def fit_zero_inflated_poisson(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[List, np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Zero-Inflated Poisson"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.ZeroInflatedPoisson(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'tvalues': results.tvalues.tolist() if hasattr(results.tvalues, 'tolist') else list(results.tvalues),
        'pvalues': results.pvalues.tolist() if hasattr(results.pvalues, 'tolist') else list(results.pvalues),
        'aic': float(results.aic),
        'bic': float(results.bic)
    }

def fit_conditional_logit(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[np.ndarray, pd.DataFrame],
    groups: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Fit Conditional Logit model"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    groups = np.array(groups) if not isinstance(groups, (np.ndarray, pd.Series)) else groups

    model = sm.ConditionalLogit(y, X, groups=groups)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'llf': float(results.llf)
    }

def fit_conditional_poisson(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[np.ndarray, pd.DataFrame],
    groups: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Fit Conditional Poisson model"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    groups = np.array(groups) if not isinstance(groups, (np.ndarray, pd.Series)) else groups

    model = sm.ConditionalPoisson(y, X, groups=groups)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'llf': float(results.llf)
    }

def fit_generalized_poisson(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Generalized Poisson model"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.GeneralizedPoisson(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'bse': results.bse.tolist() if hasattr(results.bse, 'tolist') else list(results.bse),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'llf': float(results.llf)
    }

def fit_zero_inflated_generalized_poisson(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[np.ndarray, pd.DataFrame],
    add_constant: bool = True
) -> Dict[str, Any]:
    """Fit Zero-Inflated Generalized Poisson"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X

    if add_constant:
        X = sm.add_constant(X)

    model = sm.ZeroInflatedGeneralizedPoisson(y, X)
    results = model.fit(disp=0)

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic)
    }

def main():
    print("Testing statsmodels GLM wrapper")

    np.random.seed(42)
    n = 200

    X = np.random.randn(n, 2)

    y_continuous = 2 + 3 * X[:, 0] - 1.5 * X[:, 1] + np.random.randn(n) * 0.5
    glm_result = fit_glm(y_continuous, X, family='gaussian')
    print("GLM (Gaussian) AIC: {:.4f}".format(glm_result['aic']))

    prob = 1 / (1 + np.exp(-(1 + 2 * X[:, 0] - X[:, 1])))
    y_binary = (np.random.rand(n) < prob).astype(int)
    logit_result = fit_logit(y_binary, X)
    print("Logit AIC: {:.4f}".format(logit_result['aic']))

    probit_result = fit_probit(y_binary, X)
    print("Probit AIC: {:.4f}".format(probit_result['aic']))

    lambda_val = np.exp(1 + 0.5 * X[:, 0] - 0.3 * X[:, 1])
    y_count = np.random.poisson(lambda_val)
    poisson_result = fit_poisson(y_count, X)
    print("Poisson AIC: {:.4f}".format(poisson_result['aic']))

    nb_result = fit_negative_binomial(y_count, X)
    print("Negative Binomial AIC: {:.4f}".format(nb_result['aic']))

    quantreg_result = fit_quantreg(y_continuous, X, quantile=0.5)
    print("Quantile Reg (median) params: {}".format(len(quantreg_result['params'])))

    y_multinomial = np.random.choice([0, 1, 2], size=n, p=[0.3, 0.5, 0.2])
    mnlogit_result = fit_mnlogit(y_multinomial, X)
    print("Multinomial Logit AIC: {:.4f}".format(mnlogit_result['aic']))

    X_new = np.random.randn(10, 2)
    pred_result = predict_logit(y_binary, X, X_new)
    print("Logit predictions length: {}".format(len(pred_result['probabilities'])))

    groups = np.repeat(np.arange(50), 4)
    clogit_result = fit_conditional_logit(y_binary, X, groups)
    print("Conditional Logit llf: {:.4f}".format(clogit_result['llf']))

    genpoisson_result = fit_generalized_poisson(y_count, X)
    print("Generalized Poisson AIC: {:.4f}".format(genpoisson_result['aic']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
