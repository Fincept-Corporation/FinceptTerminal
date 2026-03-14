import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.stats import api as stats_api
from statsmodels.stats import proportion, multitest, multicomp, diagnostic, anova, power, weightstats

def anova_oneway(
    *args,
    use_var: str = 'equal'
) -> Dict[str, Any]:
    """One-way ANOVA"""
    from scipy import stats as sp_stats

    f_stat, pval = sp_stats.f_oneway(*args)

    return {
        'statistic': float(f_stat),
        'pvalue': float(pval)
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
    """Cochran's Q test"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.DataFrame)) else data

    result = stats_api.cochrans_q(data, return_object=return_object)

    if return_object:
        return {
            'statistic': float(result.statistic),
            'pvalue': float(result.pvalue)
        }
    else:
        return {
            'statistic': float(result[0]),
            'pvalue': float(result[1])
        }

def cohens_kappa(
    table: Union[np.ndarray, pd.DataFrame],
    weights: Optional[Union[str, np.ndarray]] = None
) -> Dict[str, Any]:
    """Cohen's kappa coefficient"""
    table = np.array(table) if not isinstance(table, (np.ndarray, pd.DataFrame)) else table

    result = stats_api.cohens_kappa(table, weights=weights)

    return {
        'kappa': float(result[0]),
        'var': float(result[1]) if len(result) > 1 else None
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
    """Effect size for two proportions"""
    result = stats_api.effectsize_2proportions(count1, nobs1, count2, nobs2)

    if isinstance(result, tuple):
        return {
            'effect_size': float(result[0]),
            'std_diff': float(result[1]) if len(result) > 1 else None
        }
    else:
        return {
            'effect_size': float(result)
        }

def fleiss_kappa(
    table: Union[np.ndarray, pd.DataFrame],
    method: str = 'fleiss'
) -> Dict[str, Any]:
    """Fleiss' kappa for multiple raters"""
    table = np.array(table) if not isinstance(table, (np.ndarray, pd.DataFrame)) else table

    result = stats_api.fleiss_kappa(table, method=method)

    return {
        'kappa': float(result)
    }

def gof_chisquare_discrete(
    counts: Union[List, np.ndarray],
    probs: Union[List, np.ndarray],
    ddof: int = 0
) -> Dict[str, Any]:
    """Chi-square goodness of fit test for discrete distribution"""
    counts = np.array(counts) if not isinstance(counts, np.ndarray) else counts
    probs = np.array(probs) if not isinstance(probs, np.ndarray) else probs

    result = stats_api.gof_chisquare_discrete(counts, probs, ddof=ddof)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def lilliefors(
    data: Union[List, np.ndarray],
    dist: str = 'norm'
) -> Dict[str, Any]:
    """Lilliefors test for normality"""
    data = np.array(data) if not isinstance(data, np.ndarray) else data

    result = stats_api.lilliefors(data, dist=dist)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def linear_harvey_collier(
    results
) -> Dict[str, Any]:
    """Harvey-Collier test for linearity"""
    result = diagnostic.linear_harvey_collier(results)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def linear_rainbow(
    results
) -> Dict[str, Any]:
    """Rainbow test for linearity"""
    result = diagnostic.linear_rainbow(results)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def linear_reset(
    results,
    power: int = 3
) -> Dict[str, Any]:
    """Ramsey's RESET test"""
    result = diagnostic.linear_reset(results, power=power)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def mcnemar(
    table: Union[np.ndarray, pd.DataFrame],
    exact: bool = True,
    correction: bool = True
) -> Dict[str, Any]:
    """McNemar test"""
    table = np.array(table) if not isinstance(table, (np.ndarray, pd.DataFrame)) else table

    result = stats_api.mcnemar(table, exact=exact, correction=correction)

    return {
        'statistic': float(result.statistic),
        'pvalue': float(result.pvalue)
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

def proportions_chisquare(
    count: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    value: Optional[Union[List, np.ndarray]] = None
) -> Dict[str, Any]:
    """Chi-square test for proportions"""
    count = np.array(count) if not isinstance(count, np.ndarray) else count
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs

    result = stats_api.proportions_chisquare(count, nobs, value=value)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1]),
        'df': int(result[2])
    }

def runstest_1samp(
    data: Union[List, np.ndarray],
    cutoff: str = 'mean',
    correction: bool = True
) -> Dict[str, Any]:
    """Wald-Wolfowitz runs test"""
    data = np.array(data) if not isinstance(data, np.ndarray) else data

    result = stats_api.runstest_1samp(data, cutoff=cutoff, correction=correction)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def acorr_lm(
    results,
    nlags: int = 1,
    store: bool = False
) -> Dict[str, Any]:
    """Lagrange Multiplier test for autocorrelation"""
    result = stats_api.acorr_lm(results, nlags=nlags, store=store)

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
    ddof: int = 0
) -> Dict[str, Any]:
    """CUSUM test for parameter stability"""
    result = diagnostic.breaks_cusumolsresid(results, ddof=ddof)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def breaks_hansen(
    results
) -> Dict[str, Any]:
    """Hansen's test for parameter instability"""
    result = diagnostic.breaks_hansen(results)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
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
    """Combine effect sizes using meta-analysis"""
    effect = np.array(effect) if not isinstance(effect, np.ndarray) else effect
    variance = np.array(variance) if not isinstance(variance, np.ndarray) else variance

    result = stats_api.combine_effects(effect, variance, method_re=method_re, row_names=row_names)

    return {
        'eff': float(result.eff),
        'var_eff': float(result.var_eff),
        'sd_eff': float(result.sd_eff),
        'ci_eff': (float(result.ci_eff[0]), float(result.ci_eff[1])),
        'pvalue': float(result.pvalue)
    }

def compare_cox(
    results1,
    results2
) -> Dict[str, Any]:
    """Cox test for comparing non-nested models"""
    result = stats_api.compare_cox(results1, results2)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def compare_encompassing(
    results1,
    results2
) -> Dict[str, Any]:
    """Encompassing test"""
    result = stats_api.compare_encompassing(results1, results2)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def compare_j(
    results1,
    results2
) -> Dict[str, Any]:
    """J test for comparing non-nested models"""
    result = stats_api.compare_j(results1, results2)

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
    """CI for effect size in one-way ANOVA"""
    means = np.array(means) if not isinstance(means, np.ndarray) else means
    vars = np.array(vars) if not isinstance(vars, np.ndarray) else vars
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs

    result = stats_api.confint_effectsize_oneway(means, vars, nobs, alpha=alpha)

    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def confint_mvmean(
    data: Union[np.ndarray, pd.DataFrame],
    alpha: float = 0.05,
    cov: Optional[np.ndarray] = None
) -> Dict[str, Any]:
    """CI for multivariate mean"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.DataFrame)) else data

    result = stats_api.confint_mvmean(data, alpha=alpha, cov=cov)

    return {
        'lower': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'upper': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1])
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
    method: str = 'exact'
) -> Dict[str, Any]:
    """CI for Poisson quantile"""
    result = stats_api.confint_quantile_poisson(count, alpha=alpha, exposure=exposure, method=method)

    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def convert_effectsize_fsqu(
    f2: float
) -> Dict[str, Any]:
    """Convert f-squared to other effect sizes"""
    result = stats_api.convert_effectsize_fsqu(f2)

    return {
        'eta_squared': float(result)
    }

def corr_clipped(
    corr: Union[np.ndarray, pd.DataFrame],
    threshold: float = 1e-15
) -> Dict[str, Any]:
    """Clip correlation matrix"""
    corr = np.array(corr) if not isinstance(corr, (np.ndarray, pd.DataFrame)) else corr

    result = stats_api.corr_clipped(corr, threshold=threshold)

    return {
        'corr_clipped': result.tolist() if hasattr(result, 'tolist') else [[float(x) for x in row] for row in result]
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
        'corr_nearest': result.tolist() if hasattr(result, 'tolist') else [[float(x) for x in row] for row in result]
    }

# Batch 3: Additional stats functions (25 more)

def cov_nearest_factor_homog(
    cov: Union[np.ndarray, pd.DataFrame],
    rank: int
) -> Dict[str, Any]:
    """Nearest covariance with factor structure"""
    cov = np.array(cov) if not isinstance(cov, (np.ndarray, pd.DataFrame)) else cov
    result = stats_api.cov_nearest_factor_homog(cov, rank)
    return {
        'cov_nearest': result.tolist() if hasattr(result, 'tolist') else [[float(x) for x in row] for row in result]
    }

def cov_nw_panel(
    results,
    nlags: Optional[int] = None,
    groupidx: Optional[Union[List, np.ndarray]] = None
) -> Dict[str, Any]:
    """Newey-West panel covariance"""
    result = stats_api.cov_nw_panel(results, nlags=nlags, groupidx=groupidx)
    return {
        'cov': result.tolist() if hasattr(result, 'tolist') else [[float(x) for x in row] for row in result]
    }

def cov_white_simple(
    results
) -> Dict[str, Any]:
    """White heteroscedasticity robust covariance"""
    result = stats_api.cov_white_simple(results)
    return {
        'cov': result.tolist() if hasattr(result, 'tolist') else [[float(x) for x in row] for row in result]
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
    """Equivalence test for one-way ANOVA"""
    data_arrays = [np.array(d) if not isinstance(d, np.ndarray) else d for d in data]
    result = stats_api.equivalence_oneway(*data_arrays, equiv_margin=equiv_margin, alpha=alpha)
    return {
        'pvalue': float(result[0]),
        'statistic': float(result[1])
    }

def equivalence_oneway_generic(
    means: Union[List, np.ndarray],
    vars: Union[List, np.ndarray],
    nobs: Union[List, np.ndarray],
    equiv_margin: float,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Generic equivalence test from summary stats"""
    means = np.array(means) if not isinstance(means, np.ndarray) else means
    vars = np.array(vars) if not isinstance(vars, np.ndarray) else vars
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs
    result = stats_api.equivalence_oneway_generic(means, vars, nobs, equiv_margin, alpha=alpha)
    return {
        'pvalue': float(result[0]),
        'statistic': float(result[1])
    }

def equivalence_scale_oneway(
    data: List[Union[List, np.ndarray]],
    equiv_margin: float,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Equivalence test for scale in one-way"""
    data_arrays = [np.array(d) if not isinstance(d, np.ndarray) else d for d in data]
    result = stats_api.equivalence_scale_oneway(*data_arrays, equiv_margin=equiv_margin, alpha=alpha)
    return {
        'pvalue': float(result[0]),
        'statistic': float(result[1])
    }

def etest_poisson_2indep(
    count1: int,
    exposure1: float,
    count2: int,
    exposure2: float,
    ratio_null: float = 1.0,
    method: str = 'score',
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Equivalence test for two Poisson rates"""
    result = stats_api.etest_poisson_2indep(count1, exposure1, count2, exposure2,
                                             ratio_null=ratio_null, method=method,
                                             alternative=alternative)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def f2_to_wellek(
    f2: float,
    k_groups: int,
    n_total: int
) -> Dict[str, Any]:
    """Convert f-squared to Wellek's epsilon"""
    result = stats_api.f2_to_wellek(f2, k_groups, n_total)
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
        'rejected': rejected.tolist() if hasattr(rejected, 'tolist') else list(rejected),
        'pvals_corrected': pvals_corrected.tolist() if hasattr(pvals_corrected, 'tolist') else list(pvals_corrected)
    }

def fdrcorrection_twostage(
    pvals: Union[List, np.ndarray],
    alpha: float = 0.05,
    method: str = 'bky'
) -> Dict[str, Any]:
    """Two-stage FDR correction"""
    pvals = np.array(pvals) if not isinstance(pvals, np.ndarray) else pvals
    rejected, pvals_corrected = stats_api.fdrcorrection_twostage(pvals, alpha=alpha, method=method)
    return {
        'rejected': rejected.tolist() if hasattr(rejected, 'tolist') else list(rejected),
        'pvals_corrected': pvals_corrected.tolist() if hasattr(pvals_corrected, 'tolist') else list(pvals_corrected)
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
    """Convert F-statistic to Wellek's epsilon"""
    result = stats_api.fstat_to_wellek(fstat, df_num, df_denom)
    return {
        'epsilon': float(result)
    }

def het_arch(
    resid: Union[List, np.ndarray, pd.Series],
    nlags: int = 1
) -> Dict[str, Any]:
    """ARCH test for heteroscedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid
    result = stats_api.het_arch(resid, nlags=nlags)
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
    result = stats_api.het_goldfeldquandt(y, X, split=split, alternative=alternative)
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
    """Lagrange multiplier test for linearity"""
    result = stats_api.linear_lm(results, func=func)
    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
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
    """RESET test for linearity"""
    result = stats_api.linear_reset(results, power=power)
    return {
        'f_statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def local_fdr(
    zscores: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Local false discovery rate"""
    zscores = np.array(zscores) if not isinstance(zscores, np.ndarray) else zscores
    result = stats_api.local_fdr(zscores)
    return {
        'fdr': result.tolist() if hasattr(result, 'tolist') else list(result)
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
    """CI for multinomial proportions"""
    counts = np.array(counts) if not isinstance(counts, np.ndarray) else counts
    result = stats_api.multinomial_proportions_confint(counts, alpha=alpha, method=method)
    return {
        'lower': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'upper': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1])
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
        'reject': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'pvals_corrected': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1]),
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
        'pvalue': float(result[1])
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
    """Power for TOST binomial test"""
    result = stats_api.power_binom_tost(low, upp, p0, nobs, alpha=alpha, method=method)
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
    """Power for equivalence test of two Poisson rates"""
    result = stats_api.power_equivalence_poisson_2indep(rate1, rate2, nobs1, nobs2,
                                                         alpha=alpha, low=low, upp=upp)
    return {
        'power': float(result)
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
    """Power for ratio test of two Poisson rates"""
    result = stats_api.power_poisson_ratio_2indep(rate1, rate2, nobs1, nobs2,
                                                   alpha=alpha, value=value, alternative=alternative)
    return {
        'power': float(result)
    }

def power_proportions_2indep(
    diff: float,
    prop2: float,
    nobs1: int,
    nobs2: Optional[int] = None,
    alpha: float = 0.05,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Power for two independent proportions test"""
    result = stats_api.power_proportions_2indep(diff, prop2, nobs1, nobs2=nobs2,
                                                 alpha=alpha, alternative=alternative)
    return {
        'power': float(result)
    }

def power_ztost_prop(
    low: float,
    upp: float,
    prop: float,
    nobs: int,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Power for TOST proportion test"""
    result = stats_api.power_ztost_prop(low, upp, prop, nobs, alpha=alpha)
    return {
        'power': float(result)
    }

def powerdiscrepancy(
    observed: Union[List, np.ndarray],
    expected: Union[List, np.ndarray],
    alpha: float = 0.05,
    ddof: int = 0
) -> Dict[str, Any]:
    """Power discrepancy for chi-square test"""
    observed = np.array(observed) if not isinstance(observed, np.ndarray) else observed
    expected = np.array(expected) if not isinstance(expected, np.ndarray) else expected
    result = stats_api.powerdiscrepancy(observed, expected, alpha=alpha, ddof=ddof)
    return {
        'power': float(result)
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
    nobs: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Chi-square test for proportions"""
    count = np.array(count) if not isinstance(count, np.ndarray) else count
    nobs = np.array(nobs) if not isinstance(nobs, np.ndarray) else nobs
    result = stats_api.proportions_chisquare(count, nobs)
    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1]),
        'df': int(result[2])
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
    """Recursive OLS residuals"""
    result = stats_api.recursive_olsresiduals(results)
    return {
        'resid': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'cusum': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1]),
        'cusum_squares': result[2].tolist() if hasattr(result[2], 'tolist') else list(result[2])
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
    """Sample size for proportion CI"""
    result = stats_api.samplesize_confint_proportion(proportion, half_length, alpha=alpha, method=method)
    return {
        'sample_size': int(result)
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
    """Test for two independent Poisson rates"""
    result = stats_api.test_poisson_2indep(count1, exposure1, count2, exposure2,
                                           ratio_null=ratio_null, method=method, alternative=alternative)
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
    """TOST for two Poisson rates"""
    result = stats_api.tost_poisson_2indep(count1, exposure1, count2, exposure2,
                                           low=low, upp=upp, method=method)
    return {
        'pvalue': float(result[0]),
        'statistic1': float(result[1][0]),
        'statistic2': float(result[1][1])
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
    n_total: int
) -> Dict[str, Any]:
    """Convert Wellek's epsilon to f-squared"""
    result = stats_api.wellek_to_f2(epsilon, k_groups, n_total)
    return {
        'f_squared': float(result)
    }

def zconfint(
    sample_mean: float,
    std_error: float,
    alpha: float = 0.05,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Z-test confidence interval"""
    result = stats_api.zconfint(sample_mean, std_error, alpha=alpha, alternative=alternative)
    return {
        'lower': float(result[0]),
        'upper': float(result[1])
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
