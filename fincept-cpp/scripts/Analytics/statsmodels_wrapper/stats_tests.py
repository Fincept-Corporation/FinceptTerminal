import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.stats import api as stats_api
from statsmodels.stats import diagnostic, stattools, weightstats, proportion, multitest, anova

def ttest_ind(
    x1: Union[List, np.ndarray],
    x2: Union[List, np.ndarray],
    alternative: str = 'two-sided',
    usevar: str = 'pooled'
) -> Dict[str, Any]:
    """Independent samples t-test"""
    x1 = np.array(x1) if not isinstance(x1, np.ndarray) else x1
    x2 = np.array(x2) if not isinstance(x2, np.ndarray) else x2

    result = stats_api.ttest_ind(x1, x2, alternative=alternative, usevar=usevar)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1]),
        'df': float(result[2])
    }

def ttest_1samp(
    data: Union[List, np.ndarray],
    popmean: float = 0,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """One sample t-test"""
    data = np.array(data) if not isinstance(data, np.ndarray) else data

    d = weightstats.DescrStatsW(data)
    result = d.ttest_mean(popmean, alternative=alternative)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1]),
        'df': float(result[2])
    }

def ztest(
    x1: Union[List, np.ndarray],
    x2: Optional[Union[List, np.ndarray]] = None,
    value: float = 0,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Z-test for mean"""
    x1 = np.array(x1) if not isinstance(x1, np.ndarray) else x1
    x2 = np.array(x2) if x2 is not None and not isinstance(x2, np.ndarray) else x2

    result = stats_api.ztest(x1, x2, value=value, alternative=alternative)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def anova_lm(
    *models
) -> Dict[str, Any]:
    """ANOVA for linear models"""
    result = anova.anova_lm(*models)

    return {
        'df_resid': result['df_resid'].tolist() if hasattr(result['df_resid'], 'tolist') else list(result['df_resid']),
        'ssr': result['ssr'].tolist() if hasattr(result['ssr'], 'tolist') else list(result['ssr']),
        'df_diff': result['df_diff'].tolist() if hasattr(result['df_diff'], 'tolist') else list(result['df_diff']),
        'ss_diff': result['ss_diff'].tolist() if hasattr(result['ss_diff'], 'tolist') else list(result['ss_diff']),
        'F': result['F'].tolist() if hasattr(result['F'], 'tolist') else list(result['F']),
        'Pr(>F)': result['Pr(>F)'].tolist() if hasattr(result['Pr(>F)'], 'tolist') else list(result['Pr(>F)'])
    }

def proportions_ztest(
    count: Union[int, List[int], np.ndarray],
    nobs: Union[int, List[int], np.ndarray],
    value: Optional[float] = None,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Test for proportions"""
    result = proportion.proportions_ztest(count, nobs, value=value, alternative=alternative)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def proportion_confint(
    count: int,
    nobs: int,
    alpha: float = 0.05,
    method: str = 'normal'
) -> Dict[str, Any]:
    """Confidence interval for proportion"""
    result = proportion.proportion_confint(count, nobs, alpha=alpha, method=method)

    return {
        'lower': float(result[0]),
        'upper': float(result[1])
    }

def chisquare_test(
    f_obs: Union[List, np.ndarray],
    f_exp: Optional[Union[List, np.ndarray]] = None,
    ddof: int = 0
) -> Dict[str, Any]:
    """Chi-square goodness of fit test"""
    from scipy.stats import chisquare

    f_obs = np.array(f_obs) if not isinstance(f_obs, np.ndarray) else f_obs
    f_exp = np.array(f_exp) if f_exp is not None and not isinstance(f_exp, np.ndarray) else f_exp

    result = chisquare(f_obs, f_exp=f_exp, ddof=ddof)

    return {
        'statistic': float(result.statistic),
        'pvalue': float(result.pvalue)
    }

def jarque_bera_test(
    data: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Jarque-Bera test for normality"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.jarque_bera(data)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1]),
        'skewness': float(result[2]),
        'kurtosis': float(result[3])
    }

def omnibus_normtest(
    data: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Omnibus test for normality"""
    data = np.array(data) if not isinstance(data, (np.ndarray, pd.Series)) else data

    result = stattools.omni_normtest(data)

    return {
        'statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def durbin_watson_test(
    resids: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Durbin-Watson test for autocorrelation"""
    resids = np.array(resids) if not isinstance(resids, (np.ndarray, pd.Series)) else resids

    result = stattools.durbin_watson(resids)

    return {
        'statistic': float(result)
    }

def het_breuschpagan(
    resid: Union[List, np.ndarray, pd.Series],
    exog: Union[np.ndarray, pd.DataFrame]
) -> Dict[str, Any]:
    """Breusch-Pagan test for heteroskedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid
    exog = np.array(exog) if not isinstance(exog, (np.ndarray, pd.DataFrame)) else exog

    result = diagnostic.het_breuschpagan(resid, exog)

    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def het_white(
    resid: Union[List, np.ndarray, pd.Series],
    exog: Union[np.ndarray, pd.DataFrame]
) -> Dict[str, Any]:
    """White test for heteroskedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid
    exog = np.array(exog) if not isinstance(exog, (np.ndarray, pd.DataFrame)) else exog

    result = diagnostic.het_white(resid, exog)

    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def het_arch(
    resid: Union[List, np.ndarray, pd.Series],
    nlags: int = 1
) -> Dict[str, Any]:
    """ARCH test for heteroskedasticity"""
    resid = np.array(resid) if not isinstance(resid, (np.ndarray, pd.Series)) else resid

    result = diagnostic.het_arch(resid, nlags=nlags)

    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def acorr_breusch_godfrey(
    results,
    nlags: int = 1
) -> Dict[str, Any]:
    """Breusch-Godfrey test for serial correlation"""
    result = diagnostic.acorr_breusch_godfrey(results, nlags=nlags)

    return {
        'lm_statistic': float(result[0]),
        'lm_pvalue': float(result[1]),
        'f_statistic': float(result[2]),
        'f_pvalue': float(result[3])
    }

def linear_reset(
    results,
    power: int = 3
) -> Dict[str, Any]:
    """Ramsey RESET test"""
    result = diagnostic.linear_reset(results, power=power)

    return {
        'f_statistic': float(result[0]),
        'f_pvalue': float(result[1])
    }

def linear_harvey_collier(
    results
) -> Dict[str, Any]:
    """Harvey-Collier test for linearity"""
    result = diagnostic.linear_harvey_collier(results)

    return {
        't_statistic': float(result[0]),
        'pvalue': float(result[1])
    }

def multipletests_correction(
    pvalues: Union[List, np.ndarray],
    alpha: float = 0.05,
    method: str = 'fdr_bh'
) -> Dict[str, Any]:
    """Multiple testing correction"""
    pvalues = np.array(pvalues) if not isinstance(pvalues, np.ndarray) else pvalues

    result = multitest.multipletests(pvalues, alpha=alpha, method=method)

    return {
        'reject': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'pvals_corrected': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1]),
        'alphacSidak': float(result[2]),
        'alphacBonf': float(result[3])
    }

def fdrcorrection_twostage(
    pvalues: Union[List, np.ndarray],
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Two-stage FDR correction"""
    pvalues = np.array(pvalues) if not isinstance(pvalues, np.ndarray) else pvalues

    result = multitest.fdrcorrection_twostage(pvalues, alpha=alpha)

    return {
        'reject': result[0].tolist() if hasattr(result[0], 'tolist') else list(result[0]),
        'pvals_corrected': result[1].tolist() if hasattr(result[1], 'tolist') else list(result[1])
    }

def compare_means(
    data1: Union[List, np.ndarray],
    data2: Union[List, np.ndarray],
    usevar: str = 'pooled'
) -> Dict[str, Any]:
    """Compare means of two samples"""
    data1 = np.array(data1) if not isinstance(data1, np.ndarray) else data1
    data2 = np.array(data2) if not isinstance(data2, np.ndarray) else data2

    cm = weightstats.CompareMeans(weightstats.DescrStatsW(data1), weightstats.DescrStatsW(data2))
    tstat, pval, df = cm.ttest_ind(usevar=usevar)

    return {
        'statistic': float(tstat),
        'pvalue': float(pval),
        'df': float(df)
    }

def descr_stats(
    data: Union[List, np.ndarray]
) -> Dict[str, Any]:
    """Descriptive statistics"""
    data = np.array(data) if not isinstance(data, np.ndarray) else data

    d = weightstats.DescrStatsW(data)

    return {
        'mean': float(d.mean),
        'std': float(d.std),
        'std_mean': float(d.std_mean),
        'var': float(d.var),
        'nobs': float(d.nobs)
    }

def main():
    print("Testing statsmodels stats tests wrapper")

    np.random.seed(42)
    n = 100

    x1 = np.random.randn(n) + 0.5
    x2 = np.random.randn(n)

    ttest_result = ttest_ind(x1, x2)
    print("T-test p-value: {:.4f}".format(ttest_result['pvalue']))

    ztest_result = ztest(x1, x2)
    print("Z-test p-value: {:.4f}".format(ztest_result['pvalue']))

    count = 60
    nobs = 100
    prop_result = proportions_ztest(count, nobs, value=0.5)
    print("Proportion test p-value: {:.4f}".format(prop_result['pvalue']))

    prop_ci = proportion_confint(count, nobs, alpha=0.05)
    print("Proportion CI: [{:.4f}, {:.4f}]".format(prop_ci['lower'], prop_ci['upper']))

    jb_result = jarque_bera_test(x1)
    print("Jarque-Bera p-value: {:.4f}".format(jb_result['pvalue']))

    omnibus_result = omnibus_normtest(x1)
    print("Omnibus test p-value: {:.4f}".format(omnibus_result['pvalue']))

    X = np.random.randn(n, 2)
    X = sm.add_constant(X)
    y = 2 + 3 * X[:, 1] - 1.5 * X[:, 2] + np.random.randn(n) * 0.5
    model = sm.OLS(y, X).fit()

    dw_result = durbin_watson_test(model.resid)
    print("Durbin-Watson: {:.4f}".format(dw_result['statistic']))

    bp_result = het_breuschpagan(model.resid, X)
    print("Breusch-Pagan p-value: {:.4f}".format(bp_result['lm_pvalue']))

    pvalues = np.array([0.001, 0.05, 0.1, 0.3, 0.5])
    mt_result = multipletests_correction(pvalues, method='fdr_bh')
    print("Multiple tests rejected: {}".format(sum(mt_result['reject'])))

    descr_result = descr_stats(x1)
    print("Mean: {:.4f}, Std: {:.4f}".format(descr_result['mean'], descr_result['std']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
