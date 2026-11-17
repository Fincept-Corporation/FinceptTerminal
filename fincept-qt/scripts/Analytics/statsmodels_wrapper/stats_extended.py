"""Thin wrappers over statsmodels.stats (and a few scipy tests), returning JSON-safe dicts.

Every public function's return value goes through _json_safe (see the bottom of the module):
numpy values become plain Python, NaN/inf become None.
"""
import functools
import inspect
import math
import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.stats import api as stats_api
from statsmodels.stats import proportion, multitest, multicomp, diagnostic, anova, power, weightstats


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


def _call_result_object(func, *args, **kwargs):
    """Call a statsmodels test asking for its result object where this version offers one.

    statsmodels 0.15 warns that these tests stop returning plain tuples in 0.16; their result
    objects are NamedTuples, so the positional reads below work with either return type.
    """
    try:
        if 'result_object' in inspect.signature(func).parameters:
            kwargs['result_object'] = True
    except (TypeError, ValueError):
        pass
    return func(*args, **kwargs)


def _resid(results_or_resid) -> np.ndarray:
    """Residuals from a fitted results object, or the array itself."""
    if hasattr(results_or_resid, 'resid'):
        return np.asarray(results_or_resid.resid)
    return np.asarray(results_or_resid, dtype=float)


def _null_ratio_kwarg(func, ratio_null: float) -> Dict[str, float]:
    """The null rate ratio is `value` in statsmodels >= 0.14 and `ratio_null` before."""
    params = inspect.signature(func).parameters
    return {'value': ratio_null} if 'value' in params else {'ratio_null': ratio_null}


def _equivalence_result(f_stat, df, n_groups: int, nobs_total, equiv_margin: float, alpha: float) -> Dict[str, Any]:
    """Anova equivalence test (Cohen's f-squared margin) at the requested alpha.

    statsmodels' equivalence_oneway / equivalence_scale_oneway fix alpha at 0.05 for crit_f,
    crit_es and reject (the p-value does not depend on alpha); recompute those at `alpha`.
    """
    res = stats_api.equivalence_oneway_generic(
        f_stat, n_groups, np.asarray(nobs_total, dtype=float), equiv_margin, df, alpha=alpha)
    return {
        'statistic': res.statistic,
        'pvalue': res.pvalue,
        'effectsize': res.effectsize,
        'type_effectsize': res.type_effectsize,
        'crit_f': res.crit_f,
        'crit_es': res.crit_es,
        'reject': res.reject,
        'power_zero': res.power_zero,
        'df': res.df,
        'alpha': alpha
    }


def anova_oneway(
    *args,
    use_var: str = 'equal'
) -> Dict[str, Any]:
    """One-way ANOVA over the groups in args. use_var: 'equal' (classic F), 'unequal' (Welch) or 'bf'."""
    result = stats_api.anova_oneway([np.asarray(a, dtype=float) for a in args], use_var=use_var)

    return {
        'statistic': float(result.statistic),
        'pvalue': float(result.pvalue)
    }

def binom_test(
    count: int,
    nobs: int,
    prop: float = 0.5,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Binomial test"""
    result = stats_api.binom_test(count, nobs, prop=prop, alternative=alternative)

    return {
        'pvalue': float(result)
    }

def binom_tost(
    count: int,
    nobs: int,
    low: float,
    upp: float
) -> Dict[str, Any]:
    """TOST for binomial proportions"""
    result = stats_api.binom_tost(count, nobs, low, upp)

    return {
        'pvalue': float(result[0])
    }

def cochrans_q(
    data: Union[np.ndarray, pd.DataFrame],
    return_object: bool = False
) -> Dict[str, Any]:
    """Cochran's Q test. return_object is accepted for backward compatibility; the output is the same."""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.DataFrame)) else data

    # The result object (.statistic/.pvalue) is statsmodels' default return; passing return_object
    # is deprecated in 0.15 and the keyword goes away in 0.16.
    result = stats_api.cochrans_q(data)

    return {
        'statistic': float(result.statistic),
        'pvalue': float(result.pvalue)
    }

def cohens_kappa(
    table: Union[np.ndarray, pd.DataFrame],
    weights: Optional[Union[str, np.ndarray]] = None
) -> Dict[str, Any]:
    """Cohen's kappa for a square rater-agreement contingency table.

    weights: None (simple kappa), a weighting kind ('linear', 'quadratic', 'ca', 'fc',
    'toeplitz') or category scores. var is the variance of kappa; the test is against kappa = 0.
    """
    table = np.array(table) if not isinstance(table, (np.ndarray, pd.DataFrame)) else table

    if isinstance(weights, str):  # statsmodels takes the weighting kind as `wt`, scores as `weights`
        result = stats_api.cohens_kappa(table, wt=weights)
    else:
        result = stats_api.cohens_kappa(table, weights=weights)  # KappaResults: a dict

    return {
        'kappa': result['kappa'],
        'var': result['var_kappa'],
        'std_kappa': result['std_kappa'],
        'kappa_low': result['kappa_low'],
        'kappa_upp': result['kappa_upp'],
        'kappa_max': result['kappa_max'],
        'z_value': result['z_value'],
        'pvalue_one_sided': result['pvalue_one_sided'],
        'pvalue_two_sided': result['pvalue_two_sided']
    }

def confint_poisson(
    count: int,
    exposure: float = 1.0,
    alpha: float = 0.05,
    method: str = 'score'
) -> Dict[str, Any]:
    """Confidence interval for Poisson parameter"""
    result = stats_api.confint_poisson(count, exposure=exposure, alpha=alpha, method=method)

    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def confint_proportions_2indep(
    count1: int,
    nobs1: int,
    count2: int,
    nobs2: int,
    method: str = 'wald',
    compare: str = 'diff',
    alpha: float = 0.05
) -> Dict[str, Any]:
    """CI for difference of two independent proportions"""
    result = stats_api.confint_proportions_2indep(count1, nobs1, count2, nobs2, method=method, compare=compare, alpha=alpha)

    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def effectsize_2proportions(
    count1: int,
    nobs1: int,
    count2: int,
    nobs2: int
) -> Dict[str, Any]:
    """Effect size (difference in proportions) with its variance and standard deviation"""
    result = stats_api.effectsize_2proportions(count1, nobs1, count2, nobs2)

    if isinstance(result, tuple):  # (effect size, variance of the effect size)
        var_diff = float(result[1]) if len(result) > 1 else None
        return {
            'effect_size': float(result[0]),
            'var_diff': var_diff,
            'std_diff': math.sqrt(var_diff) if var_diff is not None and var_diff >= 0 else None
        }
    else:
        return {
            'effect_size': float(result)
        }

def gof_chisquare_discrete(
    counts: Union[List, np.ndarray],
    probs: Union[List, np.ndarray],
    ddof: int = 0
) -> Dict[str, Any]:
    """Pearson chi-square goodness-of-fit test of observed counts against category probabilities.

    statsmodels' gof_chisquare_discrete tests random draws from a scipy distribution, not
    counts; this uses statsmodels.stats.gof.chisquare with expected = probs * sum(counts).
    df = number of categories - 1 - ddof.
    """
    from statsmodels.stats import gof

    counts = np.asarray(counts, dtype=float)
    probs = np.asarray(probs, dtype=float)
    if counts.shape != probs.shape:
        raise ValueError(f"counts and probs need the same length, got {counts.shape} and {probs.shape}")
    expected = probs / probs.sum() * counts.sum()

    statistic, pvalue = gof.chisquare(counts, f_exp=expected, ddof=ddof)

    return {
        'statistic': float(statistic),
        'pvalue': float(pvalue),
        'df': int(len(counts) - 1 - ddof)
    }

def normal_ad(
    data: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Anderson-Darling test for normality"""
    data = np.array(data) if not isinstance(data, np.ndarray) else data

    result = stats_api.normal_ad(data)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def acorr_lm(
    results,
    nlags: int = 1,
    store: bool = False
) -> Dict[str, Any]:
    """Lagrange Multiplier test for autocorrelation of a series (or of a fitted model's residuals).

    results: a fitted results object (its resid is tested) or the series itself.
    store is accepted for backward compatibility; intermediate results are not returned.
    """
    result = _call_result_object(diagnostic.acorr_lm, _resid(results), nlags=nlags)

    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def anova_generic(
    means: Union[List, np.ndarray],
    vars: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    use_var: str = 'equal'
) -> Dict[str, Any]:
    """Generic ANOVA from summary statistics"""
    means = np.array(means) if not isinstance(means, np.ndarray) else means
    vars = np.array(vars) if not isinstance(vars, np.ndarray) else vars
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs

    result = stats_api.anova_generic(means, vars, nobs, use_var=use_var)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def binom_test_reject_interval(
    value: float,
    nobs: int,
    alpha: float = 0.05,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Rejection interval for binomial test"""
    result = stats_api.binom_test_reject_interval(value, nobs, alpha=alpha, alternative=alternative)

    return {
        'low': int(result[0]),
        'upp': int(result[1])
    }

def binom_tost_reject_interval(
    low: float,
    upp: float,
    nobs: int,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Rejection interval for TOST binomial"""
    result = stats_api.binom_tost_reject_interval(low, upp, nobs, alpha=alpha)

    return {
        'xlim': (int(result[0]), int(result[1]))
    }

def breaks_cusumolsresid(
    results,
    ddof: Optional[int] = None
) -> Dict[str, Any]:
    """CUSUM test for parameter stability, based on OLS residuals.

    results: a fitted OLS results object or its residuals. ddof is the number of estimated
    parameters (degrees-of-freedom correction of the error variance); default: the number of
    parameters of `results`, or 0 when residuals are passed.
    critical_values maps significance level to the critical value of the statistic.
    """
    if ddof is None:
        ddof = len(np.atleast_1d(results.params)) if hasattr(results, 'params') else 0

    statistic, pvalue, crit = diagnostic.breaks_cusumolsresid(_resid(results), ddof=ddof)

    return {
        'statistic': float(statistic),
        'pvalue': float(pvalue),
        'critical_values': {f'{level}%': value for level, value in crit}
    }

def breaks_hansen(
    results
) -> Dict[str, Any]:
    """Hansen's (1992) test for parameter instability of an OLS model.

    statsmodels returns no p-value, only a short table of 5% critical values indexed by the
    number of regressors. critical_value_5pct is the entry for this model's regressor count
    (null when the table has none) and reject_5pct compares the statistic with it.
    """
    statistic, crit = diagnostic.breaks_hansen(results)

    k_vars = int(np.asarray(results.model.exog).shape[1])
    table = {int(row['nobs']): float(row['crit']) for row in crit}
    crit_value = table.get(k_vars)

    return {
        'statistic': float(statistic),
        'k_vars': k_vars,
        'critical_value_5pct': crit_value,
        'reject_5pct': bool(statistic > crit_value) if crit_value is not None else None,
        'critical_values_5pct': [{'k_vars': k, 'crit': v} for k, v in table.items()]
    }

def chisquare_effectsize(
    probs0: Union[List, np.ndarray],
    probs1: Union[List, np.ndarray],
    correction: Optional[float] = None,
    cohen: bool = True,
    axis: int = 0
) -> Dict[str, Any]:
    """Effect size for chi-square test"""
    probs0 = np.array(probs0) if not isinstance(probs0, np.ndarray) else probs0
    probs1 = np.array(probs1) if not isinstance(probs1, np.ndarray) else probs1

    result = stats_api.chisquare_effectsize(probs0, probs1, correction=correction, cohen=cohen, axis=axis)

    return {
        'effect_size': float(result)
    }

def combine_effects(
    effect: Union[List, np.ndarray],
    variance: Union[List, np.ndarray],
    method_re: str = 'iterated',
    row_names: Optional[List[str]] = None
) -> Dict[str, Any]:
    """Meta-analysis: combine per-study effect sizes (with their variances).

    Returns the fixed-effect and random-effects combined estimates (eff, sd_eff, ci_low,
    ci_upp), the per-study rows with their fixed/random weights, and heterogeneity statistics
    (Q test, tau2, I2).
    """
    effect = np.array(effect) if not isinstance(effect, np.ndarray) else effect
    variance = np.array(variance) if not isinstance(variance, np.ndarray) else variance

    result = stats_api.combine_effects(effect, variance, method_re=method_re, row_names=row_names)

    frame = result.summary_frame()
    cols = ['eff', 'sd_eff', 'ci_low', 'ci_upp']
    homogeneity = result.test_homogeneity()

    return {
        'fixed_effect': {c: frame.loc['fixed effect', c] for c in cols},
        'random_effect': {c: frame.loc['random effect', c] for c in cols},
        'studies': [
            {'name': str(name), **{c: frame.loc[name, c] for c in cols + ['w_fe', 'w_re']}}
            for name in frame.index[:result.k]
        ],
        'q': result.q,
        'df': result.df,
        'pvalue_homogeneity': homogeneity.pvalue,
        'tau2': result.tau2,
        'i2': result.i2,
        'method_re': method_re
    }

def compare_cox(
    results1,
    results2
) -> Dict[str, Any]:
    """Cox test for comparing non-nested models (raises ValueError for nested models)"""
    result = _call_result_object(stats_api.compare_cox, results1, results2)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def compare_encompassing(
    results1,
    results2
) -> Dict[str, Any]:
    """Davidson-MacKinnon encompassing test for non-nested models (raises ValueError for nested models).

    'x': F test of the null that model 1 is equivalent to the model that encompasses both;
    'z': the same for model 2.
    """
    result = stats_api.compare_encompassing(results1, results2)  # DataFrame, rows x and z

    return {
        row: {
            'statistic': float(result.loc[row, 'stat']),
            'pvalue': float(result.loc[row, 'pvalue']),
            'df_num': int(result.loc[row, 'df_num']),
            'df_denom': int(result.loc[row, 'df_denom'])
        }
        for row in result.index
    }

def compare_j(
    results1,
    results2
) -> Dict[str, Any]:
    """J test for comparing non-nested models (raises ValueError for nested models)"""
    result = _call_result_object(stats_api.compare_j, results1, results2)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def confint_effectsize_oneway(
    means: Union[List, np.ndarray],
    vars: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    alpha: float = 0.05
) -> Dict[str, Any]:
    """CI for the one-way ANOVA effect size, from group means, variances and sizes.

    The classic (equal-variance) F statistic is computed from the summary statistics and
    inverted through the noncentral F distribution. lower/upper bound Cohen's f-squared;
    the same interval is also given for eta-squared, omega-squared, Cohen's f and the
    noncentrality parameter.
    """
    means = np.asarray(means, dtype=float)
    vars = np.asarray(vars, dtype=float)
    nobs = np.asarray(nobs, dtype=float)

    anova_res = stats_api.anova_generic(means, vars, nobs, use_var='equal')
    ci = stats_api.confint_effectsize_oneway(anova_res.statistic, anova_res.df, alpha=alpha,
                                             nobs=anova_res.nobs_total)

    return {
        'lower': ci.f2[0],
        'upper': ci.f2[1],
        'f_stat': anova_res.statistic,
        'df': anova_res.df,
        'ci_f2': ci.f2,
        'ci_eta2': ci.eta2,
        'ci_omega2': ci.ci_omega2,
        'ci_f': ci.ci_f,
        'ci_nc': ci.ci_nc,
        'alpha': alpha
    }

def confint_mvmean(
    data: Union[np.ndarray, pd.DataFrame],
    alpha: float = 0.05,
    cov: Optional[np.ndarray] = None,
    simult: bool = False
) -> Dict[str, Any]:
    """CI for the mean vector of multivariate data (rows = observations).

    cov: optional known covariance matrix of the variables; the sample covariance is used
    when omitted. simult=True gives simultaneous (Hotelling T2) intervals.
    """
    data = np.asarray(data, dtype=float)

    if cov is None:
        lower, upper, mean = stats_api.confint_mvmean(data, alpha=alpha, simult=simult)
    else:
        from statsmodels.stats.multivariate import confint_mvmean_fromstats
        k_vars = data.shape[1]
        lower, upper, mean = confint_mvmean_fromstats(
            data.mean(0), np.asarray(cov, dtype=float), data.shape[0],
            lin_transf=np.eye(k_vars), alpha=alpha, simult=simult)

    return {
        'lower': lower,
        'upper': upper,
        'mean': mean
    }

def confint_poisson_2indep(
    count1: int,
    exposure1: float,
    count2: int,
    exposure2: float,
    method: str = 'score',
    compare: str = 'ratio',
    alpha: float = 0.05
) -> Dict[str, Any]:
    """CI for ratio of two Poisson rates"""
    result = stats_api.confint_poisson_2indep(count1, exposure1, count2, exposure2,
                                               method=method, compare=compare, alpha=alpha)

    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def confint_quantile_poisson(
    count: int,
    alpha: float = 0.05,
    exposure: float = 1.0,
    method: str = 'exact',
    prob: float = 0.5,
    exposure_new: float = 1.0,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """CI for the prob-quantile of a Poisson variable whose rate is estimated from count / exposure.

    exposure_new is the exposure of the predicted observation. method is a confint_poisson
    method ('exact' means 'exact-c'). A one-sided interval has a null bound (unbounded).
    """
    if method == 'exact':
        method = 'exact-c'
    lower, upper = stats_api.confint_quantile_poisson(
        count, exposure, prob, exposure_new=exposure_new, method=method, alpha=alpha,
        alternative=alternative)

    return {
        'lower': float(lower),
        'upper': float(upper)
    }

def convert_effectsize_fsqu(
    f2: Optional[float],
    eta2: Optional[float] = None
) -> Dict[str, Any]:
    """Convert between Cohen's f-squared and eta-squared (pass f2=None to convert from eta2)"""
    result = stats_api.convert_effectsize_fsqu(f2=f2, eta2=eta2)

    return {
        'f2': float(result.f2),
        'eta_squared': float(result.eta2)
    }

def corr_clipped(
    corr: Union[np.ndarray, pd.DataFrame],
    threshold: float = 1e-15
) -> Dict[str, Any]:
    """Clip correlation matrix"""
    corr = np.array(corr) if not isinstance(corr, (np.ndarray, pd.DataFrame)) else corr

    result = stats_api.corr_clipped(corr, threshold=threshold)

    return {
        'corr_clipped': np.asarray(result)
    }

def corr_nearest(
    corr: Union[np.ndarray, pd.DataFrame],
    threshold: float = 1e-15,
    n_fact: int = 100
) -> Dict[str, Any]:
    """Find nearest correlation matrix"""
    corr = np.array(corr) if not isinstance(corr, (np.ndarray, pd.DataFrame)) else corr

    result = stats_api.corr_nearest(corr, threshold=threshold, n_fact=n_fact)

    return {
        'corr_nearest': np.asarray(result)
    }

# Batch 3: Additional stats functions (25 more)

def cov_nearest_factor_homog(
    cov: Union[np.ndarray, pd.DataFrame],
    rank: int
) -> Dict[str, Any]:
    """Nearest covariance matrix of the form diag(d) + F F' with homogeneous d and rank(F) = rank.

    factor is F (p x rank) and diag the diagonal part.
    """
    cov = np.array(cov) if not isinstance(cov, (np.ndarray, pd.DataFrame)) else cov
    result = stats_api.cov_nearest_factor_homog(cov, rank)  # FactoredPSDMatrix
    return {
        'cov_nearest': result.to_matrix(),
        'factor': result.root,
        'diag': result.diag
    }

def _panel_groupidx(groupidx, nobs: int) -> List[Tuple[int, int]]:
    """(start, end) pairs (end exclusive) from pairs or from one group label per observation."""
    idx = np.asarray(groupidx)
    if idx.ndim == 2 and idx.shape[1] == 2:
        return [(int(a), int(b)) for a, b in idx]
    if idx.ndim != 1 or len(idx) != nobs:
        raise ValueError(f"groupidx must hold one group label per observation ({nobs}) "
                         f"or (start, end) index pairs")
    starts = (np.flatnonzero(idx[1:] != idx[:-1]) + 1).tolist()
    bounds = [0] + starts + [nobs]
    pairs = list(zip(bounds[:-1], bounds[1:]))
    if len({idx[a] for a, _ in pairs}) != len(pairs):
        raise ValueError("cov_nw_panel needs the observations of each group to be contiguous "
                         "(sort by group, then by time)")
    return pairs

def cov_nw_panel(
    results,
    nlags: Optional[int] = None,
    groupidx: Optional[Union[List, np.ndarray]] = None
) -> Dict[str, Any]:
    """Newey-West (HAC) covariance of the parameters for panel data.

    nlags: number of lags (required, int >= 0).
    groupidx (required): a group label per observation, with each group's observations
    contiguous and in time order, or a list of (start, end) index pairs, end exclusive.
    """
    if nlags is None:
        raise ValueError("cov_nw_panel needs nlags (int >= 0)")
    if groupidx is None:
        raise ValueError("cov_nw_panel needs groupidx: a group label per observation "
                         "(contiguous groups) or a list of (start, end) index pairs")
    pairs = _panel_groupidx(groupidx, int(results.nobs))
    result = stats_api.cov_nw_panel(results, int(nlags), pairs)
    return {
        'cov': np.asarray(result)
    }

def cov_white_simple(
    results
) -> Dict[str, Any]:
    """White heteroscedasticity robust covariance"""
    result = stats_api.cov_white_simple(results)
    return {
        'cov': np.asarray(result)
    }

def durbin_watson(
    resids: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Durbin-Watson statistic"""
    resids = np.array(resids) if not isinstance(resids, (np.ndarray, pd.Series)) else resids
    result = stats_api.durbin_watson(resids)
    return {
        'statistic': float(result)
    }

def effectsize_oneway(
    means: Union[List, np.ndarray],
    vars: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    use_var: str = 'equal'
) -> Dict[str, Any]:
    """Effect size for one-way ANOVA"""
    means = np.array(means) if not isinstance(means, np.ndarray) else means
    vars = np.array(vars) if not isinstance(vars, np.ndarray) else vars
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs
    result = stats_api.effectsize_oneway(means, vars, nobs, use_var=use_var)
    return {
        'effect_size': float(result)
    }

def effectsize_smd(
    mean1: float,
    sd1: float,
    nobs1: int,
    mean2: float,
    sd2: float,
    nobs2: int
) -> Dict[str, Any]:
    """Standardized mean difference"""
    result = stats_api.effectsize_smd(mean1, sd1, nobs1, mean2, sd2, nobs2)
    return {
        'effect_size': float(result[0]) if isinstance(result, tuple) else float(result)
    }

def equivalence_oneway(
    data: List[Union[List, np.ndarray]],
    equiv_margin: float,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Anova equivalence test: are the group means equivalent within equiv_margin (Cohen's f-squared)?

    Welch anova (unequal variances), as statsmodels' default. reject = equivalence shown at alpha.
    """
    data_arrays = [np.asarray(d, dtype=float) for d in data]
    res = stats_api.equivalence_oneway(data_arrays, equiv_margin)
    return _equivalence_result(res.f_stat, res.df, len(data_arrays),
                               sum(len(d) for d in data_arrays), equiv_margin, alpha)

def equivalence_oneway_generic(
    means: Union[List, np.ndarray],
    vars: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    equiv_margin: float,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Anova equivalence test from group means, variances and sizes (same test as equivalence_oneway)"""
    means = np.asarray(means, dtype=float)
    vars = np.asarray(vars, dtype=float)
    nobs = np.asarray(nobs, dtype=float)
    anova_res = stats_api.anova_generic(means, vars, nobs, use_var='unequal', welch_correction=True)
    return _equivalence_result(anova_res.statistic, anova_res.df, anova_res.n_groups,
                               anova_res.nobs_total, equiv_margin, alpha)

def equivalence_scale_oneway(
    data: List[Union[List, np.ndarray]],
    equiv_margin: float,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Equivalence test for the scale (dispersion) of several groups (Brown-Forsythe anova on |x - median|)"""
    data_arrays = [np.asarray(d, dtype=float) for d in data]
    res = stats_api.equivalence_scale_oneway(data_arrays, equiv_margin)
    return _equivalence_result(res.f_stat, res.df, len(data_arrays),
                               sum(len(d) for d in data_arrays), equiv_margin, alpha)

def etest_poisson_2indep(
    count1: int,
    exposure1: float,
    count2: int,
    exposure2: float,
    ratio_null: float = 1.0,
    method: str = 'score',
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """E-test comparing two Poisson rates (null: rate1 / rate2 = ratio_null)"""
    result = stats_api.etest_poisson_2indep(
        count1, exposure1, count2, exposure2, method=method, alternative=alternative,
        **_null_ratio_kwarg(stats_api.etest_poisson_2indep, ratio_null))
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def f2_to_wellek(
    f2: float,
    k_groups: int,
    n_total: Optional[int] = None
) -> Dict[str, Any]:
    """Convert Cohen's f-squared to Wellek's epsilon (n_total is not used by the conversion)"""
    result = stats_api.f2_to_wellek(f2, k_groups)
    return {
        'epsilon': float(result)
    }

def fdrcorrection(
    pvals: Union[List, np.ndarray],
    alpha: float = 0.05,
    method: str = 'indep'
) -> Dict[str, Any]:
    """Benjamini-Hochberg FDR correction"""
    pvals = np.array(pvals) if not isinstance(pvals, np.ndarray) else pvals
    rejected, pvals_corrected = stats_api.fdrcorrection(pvals, alpha=alpha, method=method)
    return {
        'rejected': rejected,
        'pvals_corrected': pvals_corrected
    }

def fdrcorrection_twostage(
    pvals: Union[List, np.ndarray],
    alpha: float = 0.05,
    method: str = 'bky'
) -> Dict[str, Any]:
    """Two-stage FDR correction. m0 is the estimated number of true nulls."""
    pvals = np.array(pvals) if not isinstance(pvals, np.ndarray) else pvals
    result = stats_api.fdrcorrection_twostage(pvals, alpha=alpha, method=method)
    return {
        'rejected': result[0],
        'pvals_corrected': result[1],
        'm0': result[2] if len(result) > 2 else None,
        'alpha_stages': result[3] if len(result) > 3 else None
    }

def fleiss_kappa(
    table: Union[np.ndarray, pd.DataFrame],
    method: str = 'fleiss'
) -> Dict[str, Any]:
    """Fleiss' kappa for inter-rater agreement"""
    table = np.array(table) if not isinstance(table, (np.ndarray, pd.DataFrame)) else table
    result = stats_api.fleiss_kappa(table, method=method)
    return {
        'kappa': float(result)
    }

def fstat_to_wellek(
    fstat: float,
    df_num: int,
    df_denom: int
) -> Dict[str, Any]:
    """Wellek's effect size from a one-way anova F statistic with df (df_num, df_denom).

    k = df_num + 1 groups and N = df_denom + k observations, so nobs_mean = N / k.
    """
    n_groups = df_num + 1
    nobs_mean = (df_denom + n_groups) / n_groups
    eps2 = float(stats_api.fstat_to_wellek(fstat, n_groups, nobs_mean))
    return {
        'epsilon': math.sqrt(eps2) if eps2 >= 0 else None,
        'epsilon_squared': eps2
    }

def het_arch(
    resid: Union[List, np.ndarray, pd.Series],
    nlags: int = 1
) -> Dict[str, Any]:
    """ARCH test for heteroscedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid
    result = _call_result_object(stats_api.het_arch, resid, nlags=nlags)
    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def het_breuschpagan(
    resid: Union[List, np.ndarray, pd.Series],
    exog: Union[np.ndarray, pd.DataFrame]
) -> Dict[str, Any]:
    """Breusch-Pagan test for heteroscedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid
    exog = np.array(exog) if not isinstance(exog, (np.ndarray, pd.DataFrame)) else exog
    result = stats_api.het_breuschpagan(resid, exog)
    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def het_goldfeldquandt(
    y: Union[List, np.ndarray, pd.Series],
    X: Union[np.ndarray, pd.DataFrame],
    split: Optional[float] = None,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Goldfeld-Quandt test for heteroscedasticity"""
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    X = np.array(X) if not isinstance(X, (np.ndarray, pd.DataFrame)) else X
    result = _call_result_object(stats_api.het_goldfeldquandt, y, X, split=split, alternative=alternative)
    return {
        'f_statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def het_white(
    resid: Union[List, np.ndarray, pd.Series],
    exog: Union[np.ndarray, pd.DataFrame]
) -> Dict[str, Any]:
    """White test for heteroscedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid
    exog = np.array(exog) if not isinstance(exog, (np.ndarray, pd.DataFrame)) else exog
    result = stats_api.het_white(resid, exog)
    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def lilliefors(
    x: Union[List, np.ndarray, pd.Series],
    dist: str = 'norm',
    pvalmethod: str = 'approx'
) -> Dict[str, Any]:
    """Lilliefors test for normality"""
    x = np.array(x) if not isinstance(x, (np.ndarray, pd.Series)) else x
    result = stats_api.lilliefors(x, dist=dist, pvalmethod=pvalmethod)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

# Batch 4: Additional stats functions (25 more)

def linear_harvey_collier(
    results
) -> Dict[str, Any]:
    """Harvey-Collier test for linearity"""
    result = stats_api.linear_harvey_collier(results)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def linear_lm(
    results,
    func: Optional[Any] = None
) -> Dict[str, Any]:
    """Lagrange multiplier test for linearity of a fitted OLS model.

    Adds func(exog) (default: squares of the regressors) to the auxiliary regression of the
    residuals; f_statistic/f_pvalue are the F form of the same test.
    """
    lm, lm_pvalue, ftest = stats_api.linear_lm(_resid(results), results.model.exog, func=func)
    return {
        'lm_statistic': float(lm),
        'lm_pvalue': float(lm_pvalue),
        'f_statistic': float(np.squeeze(ftest.fvalue)),
        'f_pvalue': float(ftest.pvalue)
    }

def linear_rainbow(
    results,
    frac: float = 0.5
) -> Dict[str, Any]:
    """Rainbow test for linearity"""
    result = stats_api.linear_rainbow(results, frac=frac)
    return {
        'f_statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def linear_reset(
    results,
    power: int = 2
) -> Dict[str, Any]:
    """Ramsey RESET test (F form): adds fitted-value powers 2..power to the regression"""
    # linear_reset returns a ContrastResults object, not a tuple
    result = stats_api.linear_reset(results, power=power, use_f=True)
    return {
        'f_statistic': float(np.squeeze(result.fvalue)),
        'pvalue': float(result.pvalue),
        'df_num': float(result.df_num),
        'df_denom': float(result.df_denom)
    }

def local_fdr(
    zscores: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Local false discovery rate"""
    zscores = np.array(zscores) if not isinstance(zscores, np.ndarray) else zscores
    result = stats_api.local_fdr(zscores)
    return {
        'fdr': np.asarray(result)
    }

def mcnemar(
    table: Union[np.ndarray, pd.DataFrame],
    exact: bool = True,
    correction: bool = True
) -> Dict[str, Any]:
    """McNemar test for paired nominal data"""
    table = np.array(table) if not isinstance(table, (np.ndarray, pd.DataFrame)) else table
    result = stats_api.mcnemar(table, exact=exact, correction=correction)
    return {
        'statistic': float(result.statistic),
        'pvalue': float(result.pvalue)
    }

def multinomial_proportions_confint(
    counts: Union[List, np.ndarray],
    alpha: float = 0.05,
    method: str = 'goodman'
) -> Dict[str, Any]:
    """Simultaneous CIs for multinomial proportions: lower[i], upper[i] bound category i"""
    counts = np.array(counts) if not isinstance(counts, np.ndarray) else counts
    result = np.asarray(stats_api.multinomial_proportions_confint(counts, alpha=alpha, method=method))
    # result has one row (lower, upper) per category
    return {
        'lower': result[:, 0],
        'upper': result[:, 1]
    }

def multipletests(
    pvals: Union[List, np.ndarray],
    alpha: float = 0.05,
    method: str = 'hs',
    is_sorted: bool = False,
    returnsorted: bool = False
) -> Dict[str, Any]:
    """Multiple testing p-value correction"""
    pvals = np.array(pvals) if not isinstance(pvals, np.ndarray) else pvals
    result = stats_api.multipletests(pvals, alpha=alpha, method=method,
                                      is_sorted=is_sorted, returnsorted=returnsorted)
    return {
        'reject': result[0],
        'pvals_corrected': result[1],
        'alphacSidak': float(result[2]),
        'alphacBonf': float(result[3])
    }

def nonequivalence_poisson_2indep(
    count1: int,
    exposure1: float,
    count2: int,
    exposure2: float,
    low: float,
    upp: float,
    method: str = 'score',
    compare: str = 'ratio'
) -> Dict[str, Any]:
    """Non-equivalence test for two Poisson rates"""
    result = stats_api.nonequivalence_poisson_2indep(count1, exposure1, count2, exposure2,
                                                      low=low, upp=upp, method=method, compare=compare)
    return {
        'statistic': float(result[0]),
        # statsmodels reports 2 * min(one-sided p-values) without capping it at 1
        'pvalue': min(1.0, float(result[1]))
    }

def omni_normtest(
    resids: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Omnibus test for normality"""
    resids = np.array(resids) if not isinstance(resids, (np.ndarray, pd.Series)) else resids
    result = stats_api.omni_normtest(resids)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def power_binom_tost(
    low: float,
    upp: float,
    p0: float,
    nobs: int,
    alpha: float = 0.05,
    method: str = 'fast'
) -> Dict[str, Any]:
    """Power of the binomial TOST equivalence test when the true proportion is p0.

    method is accepted for backward compatibility and ignored (statsmodels has one method).
    """
    result = stats_api.power_binom_tost(low, upp, nobs, p_alt=p0, alpha=alpha)
    return {
        'power': float(result)
    }

def power_equivalence_poisson_2indep(
    rate1: float,
    rate2: float,
    nobs1: int,
    nobs2: int,
    alpha: float = 0.05,
    low: Optional[float] = None,
    upp: Optional[float] = None
) -> Dict[str, Any]:
    """Power of the equivalence test of two Poisson rates (rate ratio margins low, upp; both required).

    The normal approximation can go below zero when the test has no power; that is reported as 0.
    """
    if low is None or upp is None:
        raise ValueError("power_equivalence_poisson_2indep needs the equivalence margins low and upp "
                         "(limits for rate1 / rate2)")
    result = stats_api.power_equivalence_poisson_2indep(
        rate1, rate2, nobs1, low, upp, nobs_ratio=nobs2 / nobs1, alpha=alpha)
    return {
        'power': max(0.0, float(result))
    }

def power_poisson_ratio_2indep(
    rate1: float,
    rate2: float,
    nobs1: int,
    nobs2: int,
    alpha: float = 0.05,
    value: float = 1.0,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Power of the test of rate1 / rate2 = value for two Poisson samples"""
    result = stats_api.power_poisson_ratio_2indep(
        rate1, rate2, nobs1, nobs_ratio=nobs2 / nobs1, value=value, alpha=alpha,
        alternative=alternative)
    return {
        'power': float(getattr(result, 'power', result))
    }

def power_proportions_2indep(
    diff: float,
    prop2: float,
    nobs1: int,
    nobs2: Optional[int] = None,
    alpha: float = 0.05,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Power of the test of two independent proportions (prop1 = prop2 + diff); nobs2 defaults to nobs1"""
    ratio = 1 if nobs2 is None else nobs2 / nobs1
    result = stats_api.power_proportions_2indep(diff, prop2, nobs1, ratio=ratio,
                                                alpha=alpha, alternative=alternative)
    return {
        'power': float(getattr(result, 'power', result))
    }

def power_ztost_prop(
    low: float,
    upp: float,
    prop: float,
    nobs: int,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Power of the z-TOST equivalence test for one proportion when the true proportion is prop"""
    result = stats_api.power_ztost_prop(low, upp, nobs, prop, alpha=alpha)
    return {
        'power': float(result[0])
    }

def powerdiscrepancy(
    observed: Union[List, np.ndarray],
    expected: Union[List, np.ndarray],
    alpha: float = 0.05,
    ddof: int = 0,
    lambd: Union[float, str] = 0.0
) -> Dict[str, Any]:
    """Cressie-Read power-divergence goodness-of-fit test of observed counts against expected.

    expected may be counts or proportions (rescaled to the observed total). lambd selects the
    statistic: 0 / 'loglikeratio' (G-test, default), 1 / 'pearson', -0.5 / 'freeman_tukey',
    2/3 / 'cressie_read'. reject compares the p-value with alpha.
    """
    observed = np.asarray(observed, dtype=float)
    expected = np.asarray(expected, dtype=float)
    statistic, pvalue = stats_api.powerdiscrepancy(observed, expected, lambd=lambd, ddof=ddof)
    statistic, pvalue = float(np.squeeze(statistic)), float(np.squeeze(pvalue))
    return {
        'statistic': statistic,
        'pvalue': pvalue,
        'reject': pvalue < alpha
    }

def proportion_confint(
    count: int,
    nobs: int,
    alpha: float = 0.05,
    method: str = 'normal'
) -> Dict[str, Any]:
    """CI for a binomial proportion"""
    result = stats_api.proportion_confint(count, nobs, alpha=alpha, method=method)
    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def proportion_effectsize(
    prop1: float,
    prop2: float,
    method: str = 'normal'
) -> Dict[str, Any]:
    """Effect size for difference in proportions"""
    result = stats_api.proportion_effectsize(prop1, prop2, method=method)
    return {
        'effect_size': float(result)
    }

# Batch 5: Remaining stats functions (final batch)

def proportions_chisquare(
    count: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    value: Optional[float] = None
) -> Dict[str, Any]:
    """Chi-square test that k proportions are equal (or, with value, all equal to value)"""
    count = np.array(count) if not isinstance(count, np.ndarray) else count
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs
    statistic, pvalue, _tables = stats_api.proportions_chisquare(count, nobs, value=value)
    k = int(np.atleast_1d(count).shape[0])
    return {
        'statistic': float(statistic),
        'pvalue': float(pvalue),
        'df': k - 1 if value is None else k
    }

def proportions_ztest(
    count: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    value: Optional[float] = None,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Z-test for proportions"""
    count = np.array(count) if not isinstance(count, np.ndarray) else count
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs
    result = stats_api.proportions_ztest(count, nobs, value=value, alternative=alternative)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def proportions_ztost(
    count: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    low: float,
    upp: float
) -> Dict[str, Any]:
    """TOST for proportions"""
    count = np.array(count) if not isinstance(count, np.ndarray) else count
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs
    result = stats_api.proportions_ztost(count, nobs, low, upp)
    return {
        'pvalue': float(result[0])
    }

def recursive_olsresiduals(
    results
) -> Dict[str, Any]:
    """Recursive residuals of an OLS model with the CUSUM and CUSUM-of-squares statistics.

    resid / resid_standardized have one entry per observation (null for the first k - 1,
    k = number of regressors); params are the recursive parameter estimates per observation.
    cusum and its 95% band (cusum_lower, cusum_upper) follow statsmodels; cusum_squares is the
    cumulative share of squared standardized recursive residuals from observation k on (as in
    RecursiveLS.cusum_squares).
    """
    (rresid, rparams, _rypred, rresid_standardized, _rresid_scaled,
     rcusum, rcusumci) = stats_api.recursive_olsresiduals(results)
    k = int(np.asarray(results.model.exog).shape[1])
    w = np.asarray(rresid_standardized)[k:]
    w = w[np.isfinite(w)]
    cusum_squares = np.cumsum(w ** 2) / np.sum(w ** 2) if w.size and np.sum(w ** 2) > 0 else np.array([])
    return {
        'resid': rresid,
        'resid_standardized': rresid_standardized,
        'params': rparams,
        'cusum': rcusum,
        'cusum_lower': np.asarray(rcusumci)[0],
        'cusum_upper': np.asarray(rcusumci)[1],
        'cusum_squares': cusum_squares
    }

def runstest_1samp(
    x: Union[List, np.ndarray, pd.Series],
    cutoff: str = 'mean',
    correction: bool = True
) -> Dict[str, Any]:
    """Runs test for randomness (1 sample)"""
    x = np.array(x) if not isinstance(x, (np.ndarray, pd.Series)) else x
    result = stats_api.runstest_1samp(x, cutoff=cutoff, correction=correction)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def runstest_2samp(
    x: Union[List, np.ndarray, pd.Series],
    y: Union[List, np.ndarray, pd.Series],
    correction: bool = True
) -> Dict[str, Any]:
    """Runs test for randomness (2 samples)"""
    x = np.array(x) if not isinstance(x, (np.ndarray, pd.Series)) else x
    y = np.array(y) if not isinstance(y, (np.ndarray, pd.Series)) else y
    result = stats_api.runstest_2samp(x, y, correction=correction)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def samplesize_confint_proportion(
    proportion: float,
    half_length: float,
    alpha: float = 0.05,
    method: str = 'normal'
) -> Dict[str, Any]:
    """Sample size for a proportion CI of the given half-length (rounded up)"""
    result = stats_api.samplesize_confint_proportion(proportion, half_length, alpha=alpha, method=method)
    return {
        'sample_size': int(math.ceil(float(result)))
    }

def test_poisson(
    count: int,
    nobs: int,
    value: float,
    method: str = 'wald',
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Test for Poisson rate"""
    result = stats_api.test_poisson(count, nobs, value, method=method, alternative=alternative)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def test_poisson_2indep(
    count1: int,
    exposure1: float,
    count2: int,
    exposure2: float,
    ratio_null: float = 1.0,
    method: str = 'wald',
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Test for two independent Poisson rates (null: rate1 / rate2 = ratio_null)"""
    result = stats_api.test_poisson_2indep(
        count1, exposure1, count2, exposure2, method=method, alternative=alternative,
        **_null_ratio_kwarg(stats_api.test_poisson_2indep, ratio_null))
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def test_proportions_2indep(
    count1: int,
    nobs1: int,
    count2: int,
    nobs2: int,
    value: Optional[float] = None,
    method: str = 'wald',
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Test for two independent proportions"""
    result = stats_api.test_proportions_2indep(count1, nobs1, count2, nobs2,
                                                value=value, method=method, alternative=alternative)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def tost_poisson_2indep(
    count1: int,
    exposure1: float,
    count2: int,
    exposure2: float,
    low: float,
    upp: float,
    method: str = 'score'
) -> Dict[str, Any]:
    """TOST equivalence test for two Poisson rates (rate ratio within [low, upp]).

    statistic1 / pvalue1: one-sided test against low; statistic2 / pvalue2: against upp;
    pvalue is the larger of the two.
    """
    result = stats_api.tost_poisson_2indep(count1, exposure1, count2, exposure2,
                                           low=low, upp=upp, method=method)
    return {
        'pvalue': float(result.pvalue),
        'statistic1': float(result.results_larger.statistic),
        'statistic2': float(result.results_smaller.statistic),
        'pvalue1': float(result.results_larger.pvalue),
        'pvalue2': float(result.results_smaller.pvalue)
    }

def ttest_ind(
    x1: Union[List, np.ndarray, pd.Series],
    x2: Union[List, np.ndarray, pd.Series],
    alternative: str = 'two-sided',
    usevar: str = 'pooled',
    value: float = 0
) -> Dict[str, Any]:
    """Independent t-test"""
    x1 = np.array(x1) if not isinstance(x1, (np.ndarray, pd.Series)) else x1
    x2 = np.array(x2) if not isinstance(x2, (np.ndarray, pd.Series)) else x2
    result = stats_api.ttest_ind(x1, x2, alternative=alternative, usevar=usevar, value=value)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1]),
        'df': float(result[2])
    }

def ttost_ind(
    x1: Union[List, np.ndarray, pd.Series],
    x2: Union[List, np.ndarray, pd.Series],
    low: float,
    upp: float,
    usevar: str = 'pooled'
) -> Dict[str, Any]:
    """TOST for independent samples"""
    x1 = np.array(x1) if not isinstance(x1, (np.ndarray, pd.Series)) else x1
    x2 = np.array(x2) if not isinstance(x2, (np.ndarray, pd.Series)) else x2
    result = stats_api.ttost_ind(x1, x2, low, upp, usevar=usevar)
    return {
        'pvalue': float(result[0])
    }

def ttost_paired(
    x1: Union[List, np.ndarray, pd.Series],
    x2: Union[List, np.ndarray, pd.Series],
    low: float,
    upp: float
) -> Dict[str, Any]:
    """TOST for paired samples"""
    x1 = np.array(x1) if not isinstance(x1, (np.ndarray, pd.Series)) else x1
    x2 = np.array(x2) if not isinstance(x2, (np.ndarray, pd.Series)) else x2
    result = stats_api.ttost_paired(x1, x2, low, upp)
    return {
        'pvalue': float(result[0])
    }

def wellek_to_f2(
    epsilon: float,
    k_groups: int,
    n_total: Optional[int] = None
) -> Dict[str, Any]:
    """Convert Wellek's epsilon to Cohen's f-squared (n_total is not used by the conversion)"""
    result = stats_api.wellek_to_f2(epsilon, k_groups)
    return {
        'f_squared': float(result)
    }

def zconfint(
    sample_mean: Union[float, List, np.ndarray],
    std_error: Optional[float],
    alpha: float = 0.05,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Normal (z) confidence interval for a mean.

    Either sample_mean and std_error (the standard error of the mean), or a data array as
    sample_mean with std_error=None. A one-sided interval ('larger' / 'smaller') has a null bound.
    """
    from scipy import stats as sp_stats

    if np.ndim(sample_mean) > 0:
        if std_error is not None:
            raise ValueError("zconfint: pass either data, or sample_mean with std_error")
        lower, upper = stats_api.zconfint(np.asarray(sample_mean, dtype=float), alpha=alpha,
                                          alternative=alternative)
        return {'lower': float(lower), 'upper': float(upper)}
    if std_error is None:
        raise ValueError("zconfint needs std_error with a scalar sample_mean")

    mean, se = float(sample_mean), float(std_error)
    if alternative in ('two-sided', '2-sided', '2s'):
        z = sp_stats.norm.ppf(1 - alpha / 2.0)
        lower, upper = mean - z * se, mean + z * se
    elif alternative in ('larger', 'l'):
        lower, upper = mean - sp_stats.norm.ppf(1 - alpha) * se, math.inf
    elif alternative in ('smaller', 's'):
        lower, upper = -math.inf, mean + sp_stats.norm.ppf(1 - alpha) * se
    else:
        raise ValueError(f"alternative must be 'two-sided', 'larger' or 'smaller', got {alternative!r}")
    return {
        'lower': lower,
        'upper': upper
    }

def ztest(
    x1: Union[List, np.ndarray, pd.Series],
    x2: Optional[Union[List, np.ndarray, pd.Series]] = None,
    value: float = 0,
    alternative: str = 'two-sided',
    ddof: int = 1
) -> Dict[str, Any]:
    """Z-test for one or two samples"""
    x1 = np.array(x1) if not isinstance(x1, (np.ndarray, pd.Series)) else x1
    if x2 is not None:
        x2 = np.array(x2) if not isinstance(x2, (np.ndarray, pd.Series)) else x2
    result = stats_api.ztest(x1, x2=x2, value=value, alternative=alternative, ddof=ddof)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def ztost(
    x1: Union[List, np.ndarray, pd.Series],
    low: float,
    upp: float,
    x2: Optional[Union[List, np.ndarray, pd.Series]] = None,
    ddof: int = 1
) -> Dict[str, Any]:
    """Z-test TOST"""
    x1 = np.array(x1) if not isinstance(x1, (np.ndarray, pd.Series)) else x1
    if x2 is not None:
        x2 = np.array(x2) if not isinstance(x2, (np.ndarray, pd.Series)) else x2
    result = stats_api.ztost(x1, low, upp, x2=x2, ddof=ddof)
    return {
        'pvalue': float(result[0])
    }


def _json_safe_returns(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        return _json_safe(func(*args, **kwargs))
    return wrapper


# Make every public wrapper above return JSON-safe data (NaN/inf -> None, numpy -> Python).
for _name, _func in list(globals().items()):
    if (inspect.isfunction(_func) and _func.__module__ == __name__
            and not _name.startswith('_') and _name != 'main'):
        globals()[_name] = _json_safe_returns(_func)
del _name, _func


def main():
    print("Testing statsmodels stats_extended wrapper")

    np.random.seed(42)

    data1 = np.random.randn(50) + 0.5
    data2 = np.random.randn(50)
    data3 = np.random.randn(50) - 0.5

    anova_result = anova_oneway(data1, data2, data3)
    print("ANOVA p-value: {:.4f}".format(anova_result['pvalue']))

    binom_result = binom_test(60, 100, prop=0.5)
    print("Binomial test p-value: {:.4f}".format(binom_result['pvalue']))

    ci_poisson = confint_poisson(20, exposure=1.0)
    print("Poisson CI: [{:.4f}, {:.4f}]".format(ci_poisson['lower'], ci_poisson['upper']))

    es_result = effectsize_2proportions(60, 100, 50, 100)
    print("Effect size: {:.4f}".format(es_result['effect_size']))

    ad_result = normal_ad(data1)
    print("Anderson-Darling p-value: {:.4f}".format(ad_result['pvalue']))

    reject_int = binom_test_reject_interval(0.5, 100, alpha=0.05)
    print("Binomial reject interval: [{}, {}]".format(reject_int['low'], reject_int['upp']))

    chi_es = chisquare_effectsize([0.25, 0.25, 0.25, 0.25], [0.3, 0.3, 0.2, 0.2])
    print("Chi-square effect size: {:.4f}".format(chi_es['effect_size']))

    # Batch 3 tests
    dw_result = durbin_watson(data1)
    print("Durbin-Watson statistic: {:.4f}".format(dw_result['statistic']))

    fdr_result = fdrcorrection([0.01, 0.04, 0.1, 0.3, 0.5])
    print("FDR rejected count: {}".format(sum(fdr_result['rejected'])))

    lillie_result = lilliefors(data1)
    print("Lilliefors p-value: {:.4f}".format(lillie_result['pvalue']))

    # Batch 4 tests
    prop_ci = proportion_confint(60, 100, alpha=0.05)
    print("Proportion CI: [{:.4f}, {:.4f}]".format(prop_ci['lower'], prop_ci['upper']))

    multi_result = multipletests([0.01, 0.04, 0.1, 0.3, 0.5], method='bonferroni')
    print("Multipletests rejected count: {}".format(sum(multi_result['reject'])))

    omni_result = omni_normtest(data1)
    print("Omnibus normtest p-value: {:.4f}".format(omni_result['pvalue']))

    # Batch 5 tests
    ttest_result = ttest_ind(data1, data2)
    print("T-test p-value: {:.4f}".format(ttest_result['pvalue']))

    ztest_result = ztest(data1, value=0.5)
    print("Z-test p-value: {:.4f}".format(ztest_result['pvalue']))

    runs_result = runstest_1samp(data1)
    print("Runs test p-value: {:.4f}".format(runs_result['pvalue']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
