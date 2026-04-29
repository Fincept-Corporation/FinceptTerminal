"""
Statsmodels Wrapper for Fincept Terminal

Comprehensive wrapper for statsmodels library covering:
- Regression models (OLS, WLS, GLS, GLSAR, RecursiveLS)
- Time series models (ARIMA, SARIMAX, VARMAX, AutoReg, etc.)
- Time series functions (ACF, PACF, ADF, KPSS, cointegration tests)
- GLM models (Logit, Probit, Poisson, NegativeBinomial, QuantReg, GEE)
- Statistical tests (t-tests, ANOVA, proportions, diagnostics)
- Power analysis (t-test, F-test, proportions, ANOVA)
- Multivariate analysis (PCA, Factor Analysis, MANOVA, Canonical Correlation)
- Robust regression (RLM, MAD, Huber scale)
- Survival analysis (Cox PH, Kaplan-Meier, Log-rank test)
- Nonparametric methods (KDE, Kernel Regression, LOWESS)
- GAM (Generalized Additive Models)
"""


# Import stats_extended module (100+ additional statistical functions)
from . import stats_extended


__all__ = [
    # Regression
    'fit_ols', 'fit_wls', 'fit_gls', 'fit_glsar', 'fit_recursive_ls', 'predict_ols',
    'regression_diagnostics', 'fit_rolling_ols', 'fit_rolling_wls',
    # Time series models
    'fit_arima', 'forecast_arima', 'fit_sarimax', 'fit_varmax', 'fit_autoreg', 'fit_ardl',
    'fit_exponential_smoothing', 'stl_decompose', 'fit_dynamic_factor', 'fit_unobserved_components',
    'fit_markov_regression', 'fit_vecm', 'fit_holt', 'fit_simple_exp_smoothing',
    'fit_markov_autoregression', 'stl_forecast',
    # Time series functions
    'calculate_acf', 'calculate_pacf', 'calculate_ccf', 'adf_test', 'kpss_test', 'coint_test',
    'bds_test', 'q_stat', 'acorr_ljungbox', 'seasonal_decompose_additive', 'arma_order_select',
    'detrend_linear', 'add_trend_to_data',
    # GLM models
    'fit_glm', 'fit_logit', 'predict_logit', 'fit_probit', 'fit_poisson', 'fit_negative_binomial',
    'fit_mnlogit', 'fit_quantreg', 'fit_gee', 'fit_zero_inflated_poisson',
    'fit_conditional_logit', 'fit_conditional_poisson',
    'fit_generalized_poisson', 'fit_zero_inflated_generalized_poisson',
    # Statistical tests
    'ttest_ind', 'ttest_1samp', 'ztest', 'anova_lm', 'proportions_ztest', 'proportion_confint',
    'chisquare_test', 'jarque_bera_test', 'omnibus_normtest', 'durbin_watson_test',
    'het_breuschpagan', 'het_white', 'het_arch', 'acorr_breusch_godfrey', 'linear_reset',
    'linear_harvey_collier', 'multipletests_correction', 'fdrcorrection_twostage', 'compare_means',
    'descr_stats',
    # Power analysis
    'ttest_power', 'ftest_power', 'proportion_power', 'calculate_effect_size_cohens_d',
    'calculate_effect_size_proportion', 'sample_size_proportion_confint', 'chisquare_power',
    'anova_power',
    # Multivariate
    'perform_pca', 'perform_factor_analysis', 'perform_manova', 'canonical_correlation',
    # Robust
    'fit_rlm', 'mad', 'huber_scale', 'huber_location_scale',
    # Survival
    'fit_cox_ph', 'survdiff', 'kaplan_meier',
    # Nonparametric
    'kernel_density_estimation', 'kernel_regression', 'lowess_smoothing', 'multivariate_kde',
    # GAM
    'fit_glm_gam', 'fit_gam_formula',
    # Extended stats module (100+ functions - access via stats_extended.function_name)
    'stats_extended'
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "fit_ols": ("regression", "fit_ols"),
    "fit_wls": ("regression", "fit_wls"),
    "fit_gls": ("regression", "fit_gls"),
    "fit_glsar": ("regression", "fit_glsar"),
    "fit_recursive_ls": ("regression", "fit_recursive_ls"),
    "predict_ols": ("regression", "predict_ols"),
    "regression_diagnostics": ("regression", "regression_diagnostics"),
    "fit_rolling_ols": ("regression", "fit_rolling_ols"),
    "fit_rolling_wls": ("regression", "fit_rolling_wls"),
    "fit_arima": ("timeseries_models", "fit_arima"),
    "forecast_arima": ("timeseries_models", "forecast_arima"),
    "fit_sarimax": ("timeseries_models", "fit_sarimax"),
    "fit_varmax": ("timeseries_models", "fit_varmax"),
    "fit_autoreg": ("timeseries_models", "fit_autoreg"),
    "fit_ardl": ("timeseries_models", "fit_ardl"),
    "fit_exponential_smoothing": ("timeseries_models", "fit_exponential_smoothing"),
    "stl_decompose": ("timeseries_models", "stl_decompose"),
    "fit_dynamic_factor": ("timeseries_models", "fit_dynamic_factor"),
    "fit_unobserved_components": ("timeseries_models", "fit_unobserved_components"),
    "fit_markov_regression": ("timeseries_models", "fit_markov_regression"),
    "fit_vecm": ("timeseries_models", "fit_vecm"),
    "fit_holt": ("timeseries_models", "fit_holt"),
    "fit_simple_exp_smoothing": ("timeseries_models", "fit_simple_exp_smoothing"),
    "fit_markov_autoregression": ("timeseries_models", "fit_markov_autoregression"),
    "stl_forecast": ("timeseries_models", "stl_forecast"),
    "calculate_acf": ("timeseries_funcs", "calculate_acf"),
    "calculate_pacf": ("timeseries_funcs", "calculate_pacf"),
    "calculate_ccf": ("timeseries_funcs", "calculate_ccf"),
    "adf_test": ("timeseries_funcs", "adf_test"),
    "kpss_test": ("timeseries_funcs", "kpss_test"),
    "coint_test": ("timeseries_funcs", "coint_test"),
    "bds_test": ("timeseries_funcs", "bds_test"),
    "q_stat": ("timeseries_funcs", "q_stat"),
    "acorr_ljungbox": ("timeseries_funcs", "acorr_ljungbox"),
    "seasonal_decompose_additive": ("timeseries_funcs", "seasonal_decompose_additive"),
    "arma_order_select": ("timeseries_funcs", "arma_order_select"),
    "detrend_linear": ("timeseries_funcs", "detrend_linear"),
    "add_trend_to_data": ("timeseries_funcs", "add_trend_to_data"),
    "fit_glm": ("glm", "fit_glm"),
    "fit_logit": ("glm", "fit_logit"),
    "predict_logit": ("glm", "predict_logit"),
    "fit_probit": ("glm", "fit_probit"),
    "fit_poisson": ("glm", "fit_poisson"),
    "fit_negative_binomial": ("glm", "fit_negative_binomial"),
    "fit_mnlogit": ("glm", "fit_mnlogit"),
    "fit_quantreg": ("glm", "fit_quantreg"),
    "fit_gee": ("glm", "fit_gee"),
    "fit_zero_inflated_poisson": ("glm", "fit_zero_inflated_poisson"),
    "fit_conditional_logit": ("glm", "fit_conditional_logit"),
    "fit_conditional_poisson": ("glm", "fit_conditional_poisson"),
    "fit_generalized_poisson": ("glm", "fit_generalized_poisson"),
    "fit_zero_inflated_generalized_poisson": ("glm", "fit_zero_inflated_generalized_poisson"),
    "ttest_ind": ("stats_tests", "ttest_ind"),
    "ttest_1samp": ("stats_tests", "ttest_1samp"),
    "ztest": ("stats_tests", "ztest"),
    "anova_lm": ("stats_tests", "anova_lm"),
    "proportions_ztest": ("stats_tests", "proportions_ztest"),
    "proportion_confint": ("stats_tests", "proportion_confint"),
    "chisquare_test": ("stats_tests", "chisquare_test"),
    "jarque_bera_test": ("stats_tests", "jarque_bera_test"),
    "omnibus_normtest": ("stats_tests", "omnibus_normtest"),
    "durbin_watson_test": ("stats_tests", "durbin_watson_test"),
    "het_breuschpagan": ("stats_tests", "het_breuschpagan"),
    "het_white": ("stats_tests", "het_white"),
    "het_arch": ("stats_tests", "het_arch"),
    "acorr_breusch_godfrey": ("stats_tests", "acorr_breusch_godfrey"),
    "linear_reset": ("stats_tests", "linear_reset"),
    "linear_harvey_collier": ("stats_tests", "linear_harvey_collier"),
    "multipletests_correction": ("stats_tests", "multipletests_correction"),
    "fdrcorrection_twostage": ("stats_tests", "fdrcorrection_twostage"),
    "compare_means": ("stats_tests", "compare_means"),
    "descr_stats": ("stats_tests", "descr_stats"),
    "ttest_power": ("power_analysis", "ttest_power"),
    "ftest_power": ("power_analysis", "ftest_power"),
    "proportion_power": ("power_analysis", "proportion_power"),
    "calculate_effect_size_cohens_d": ("power_analysis", "calculate_effect_size_cohens_d"),
    "calculate_effect_size_proportion": ("power_analysis", "calculate_effect_size_proportion"),
    "sample_size_proportion_confint": ("power_analysis", "sample_size_proportion_confint"),
    "chisquare_power": ("power_analysis", "chisquare_power"),
    "anova_power": ("power_analysis", "anova_power"),
    "perform_pca": ("multivariate", "perform_pca"),
    "perform_factor_analysis": ("multivariate", "perform_factor_analysis"),
    "perform_manova": ("multivariate", "perform_manova"),
    "canonical_correlation": ("multivariate", "canonical_correlation"),
    "fit_rlm": ("robust", "fit_rlm"),
    "mad": ("robust", "mad"),
    "huber_scale": ("robust", "huber_scale"),
    "huber_location_scale": ("robust", "huber_location_scale"),
    "fit_cox_ph": ("survival", "fit_cox_ph"),
    "survdiff": ("survival", "survdiff"),
    "kaplan_meier": ("survival", "kaplan_meier"),
    "kernel_density_estimation": ("nonparametric", "kernel_density_estimation"),
    "kernel_regression": ("nonparametric", "kernel_regression"),
    "lowess_smoothing": ("nonparametric", "lowess_smoothing"),
    "multivariate_kde": ("nonparametric", "multivariate_kde"),
    "fit_glm_gam": ("gam", "fit_glm_gam"),
    "fit_gam_formula": ("gam", "fit_gam_formula"),
}


def __getattr__(name: str):  # PEP 562
    target = _LAZY_ATTRS.get(name)
    if target is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    submodule, original_name = target
    import importlib
    mod = importlib.import_module(f".{submodule}", __name__)
    value = getattr(mod, original_name)
    globals()[name] = value  # cache for subsequent access
    return value


def __dir__() -> list[str]:
    return sorted(set(globals()) | set(_LAZY_ATTRS))
