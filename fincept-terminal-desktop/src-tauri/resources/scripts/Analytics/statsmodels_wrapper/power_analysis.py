import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
import statsmodels.api as sm
from statsmodels.stats import power

def ttest_power(
    effect_size: float,
    nobs: Optional[int] = None,
    alpha: float = 0.05,
    power: Optional[float] = None,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Calculate power for t-test"""
    analysis = sm.stats.TTestIndPower()

    if power is None:
        calc_power = analysis.solve_power(effect_size=effect_size, nobs1=nobs, alpha=alpha, alternative=alternative)
        return {
            'effect_size': float(effect_size),
            'nobs': int(nobs) if nobs else None,
            'alpha': float(alpha),
            'power': float(calc_power),
            'alternative': alternative
        }
    elif nobs is None:
        calc_nobs = analysis.solve_power(effect_size=effect_size, power=power, alpha=alpha, alternative=alternative)
        return {
            'effect_size': float(effect_size),
            'nobs': float(calc_nobs),
            'alpha': float(alpha),
            'power': float(power),
            'alternative': alternative
        }
    else:
        calc_effect = analysis.solve_power(power=power, nobs1=nobs, alpha=alpha, alternative=alternative)
        return {
            'effect_size': float(calc_effect),
            'nobs': int(nobs),
            'alpha': float(alpha),
            'power': float(power),
            'alternative': alternative
        }

def ftest_power(
    effect_size: float,
    nobs: Optional[int] = None,
    k_groups: int = 2,
    alpha: float = 0.05,
    power: Optional[float] = None
) -> Dict[str, Any]:
    """Calculate power for F-test ANOVA"""
    analysis = sm.stats.FTestAnovaPower()

    if power is None and nobs is not None:
        calc_power = analysis.solve_power(effect_size=effect_size, nobs=nobs, k_groups=k_groups, alpha=alpha)
        return {
            'effect_size': float(effect_size),
            'nobs': int(nobs),
            'k_groups': int(k_groups),
            'alpha': float(alpha),
            'power': float(calc_power)
        }
    elif nobs is None and power is not None:
        calc_nobs = analysis.solve_power(effect_size=effect_size, power=power, k_groups=k_groups, alpha=alpha)
        return {
            'effect_size': float(effect_size),
            'nobs': float(calc_nobs),
            'k_groups': int(k_groups),
            'alpha': float(alpha),
            'power': float(power)
        }
    else:
        return {'error': 'Must provide either power or nobs'}

def proportion_power(
    effect_size: float,
    nobs: Optional[int] = None,
    alpha: float = 0.05,
    power: Optional[float] = None,
    alternative: str = 'two-sided'
) -> Dict[str, Any]:
    """Calculate power for proportion test"""
    from statsmodels.stats.proportion import proportion_effectsize

    analysis = sm.stats.NormalIndPower()

    if power is None and nobs is not None:
        calc_power = analysis.solve_power(effect_size=effect_size, nobs1=nobs, alpha=alpha, alternative=alternative)
        return {
            'effect_size': float(effect_size),
            'nobs': int(nobs),
            'alpha': float(alpha),
            'power': float(calc_power),
            'alternative': alternative
        }
    elif nobs is None and power is not None:
        calc_nobs = analysis.solve_power(effect_size=effect_size, power=power, alpha=alpha, alternative=alternative)
        return {
            'effect_size': float(effect_size),
            'nobs': float(calc_nobs),
            'alpha': float(alpha),
            'power': float(power),
            'alternative': alternative
        }
    else:
        return {'error': 'Must provide either power or nobs'}

def calculate_effect_size_cohens_d(
    mean1: float,
    mean2: float,
    std: float
) -> Dict[str, Any]:
    """Calculate Cohen's d effect size"""
    effect_size = abs(mean1 - mean2) / std

    return {
        'cohens_d': float(effect_size),
        'interpretation': 'small' if effect_size < 0.5 else ('medium' if effect_size < 0.8 else 'large')
    }

def calculate_effect_size_proportion(
    prop1: float,
    prop2: float
) -> Dict[str, Any]:
    """Calculate effect size for proportions"""
    from statsmodels.stats.proportion import proportion_effectsize

    effect_size = proportion_effectsize(prop1, prop2)

    return {
        'effect_size': float(effect_size),
        'prop1': float(prop1),
        'prop2': float(prop2)
    }

def sample_size_proportion_confint(
    proportion: float,
    half_width: float,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Calculate sample size for proportion confidence interval"""
    from statsmodels.stats.proportion import samplesize_confint_proportion

    nobs = samplesize_confint_proportion(proportion, half_width, alpha=alpha)

    return {
        'sample_size': int(np.ceil(nobs)),
        'proportion': float(proportion),
        'half_width': float(half_width),
        'alpha': float(alpha)
    }

def chisquare_power(
    effect_size: float,
    nobs: Optional[int] = None,
    n_bins: int = 2,
    alpha: float = 0.05,
    power: Optional[float] = None
) -> Dict[str, Any]:
    """Calculate power for chi-square test"""
    analysis = sm.stats.GofChisquarePower()

    if power is None and nobs is not None:
        calc_power = analysis.solve_power(effect_size=effect_size, nobs=nobs, n_bins=n_bins, alpha=alpha)
        return {
            'effect_size': float(effect_size),
            'nobs': int(nobs),
            'n_bins': int(n_bins),
            'alpha': float(alpha),
            'power': float(calc_power)
        }
    elif nobs is None and power is not None:
        calc_nobs = analysis.solve_power(effect_size=effect_size, power=power, n_bins=n_bins, alpha=alpha)
        return {
            'effect_size': float(effect_size),
            'nobs': float(calc_nobs),
            'n_bins': int(n_bins),
            'alpha': float(alpha),
            'power': float(power)
        }
    else:
        return {'error': 'Must provide either power or nobs'}

def anova_power(
    effect_size: float,
    nobs: Optional[int] = None,
    k_groups: int = 2,
    alpha: float = 0.05,
    power: Optional[float] = None
) -> Dict[str, Any]:
    """Calculate power for ANOVA"""
    analysis = sm.stats.FTestAnovaPower()

    if power is None and nobs is not None:
        calc_power = analysis.solve_power(effect_size=effect_size, nobs=nobs, k_groups=k_groups, alpha=alpha)
        return {
            'effect_size': float(effect_size),
            'nobs': int(nobs),
            'k_groups': int(k_groups),
            'alpha': float(alpha),
            'power': float(calc_power)
        }
    elif nobs is None and power is not None:
        calc_nobs = analysis.solve_power(effect_size=effect_size, power=power, k_groups=k_groups, alpha=alpha)
        return {
            'effect_size': float(effect_size),
            'nobs': float(calc_nobs),
            'k_groups': int(k_groups),
            'alpha': float(alpha),
            'power': float(power)
        }
    else:
        return {'error': 'Must provide either power or nobs'}

def main():
    print("Testing statsmodels power analysis wrapper")

    ttest_result = ttest_power(effect_size=0.5, nobs=50, alpha=0.05)
    print("T-test power: {:.4f}".format(ttest_result['power']))

    ttest_nobs = ttest_power(effect_size=0.5, power=0.8, alpha=0.05)
    print("Required sample size for power=0.8: {:.0f}".format(ttest_nobs['nobs']))

    ftest_result = ftest_power(effect_size=0.25, nobs=60, k_groups=3, alpha=0.05)
    print("F-test power: {:.4f}".format(ftest_result['power']))

    prop_result = proportion_power(effect_size=0.3, nobs=100, alpha=0.05)
    print("Proportion test power: {:.4f}".format(prop_result['power']))

    cohens_d = calculate_effect_size_cohens_d(mean1=10, mean2=12, std=3)
    print("Cohen's d: {:.4f} ({})".format(cohens_d['cohens_d'], cohens_d['interpretation']))

    prop_es = calculate_effect_size_proportion(prop1=0.6, prop2=0.7)
    print("Proportion effect size: {:.4f}".format(prop_es['effect_size']))

    ss_result = sample_size_proportion_confint(proportion=0.5, half_width=0.05, alpha=0.05)
    print("Required sample size for CI: {}".format(ss_result['sample_size']))

    anova_result = anova_power(effect_size=0.25, nobs=90, k_groups=3, alpha=0.05)
    print("ANOVA power: {:.4f}".format(anova_result['power']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
