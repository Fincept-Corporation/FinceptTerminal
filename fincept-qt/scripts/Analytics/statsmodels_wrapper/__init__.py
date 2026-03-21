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

from .regression import (
    fit_ols,
    fit_wls,
    fit_gls,
    fit_glsar,
    fit_recursive_ls,
    predict_ols,
    regression_diagnostics,
    fit_rolling_ols,
    fit_rolling_wls
)

from .timeseries_models import (
    fit_arima,
    forecast_arima,
    fit_sarimax,
    fit_varmax,
    fit_autoreg,
    fit_ardl,
    fit_exponential_smoothing,
    stl_decompose,
    fit_dynamic_factor,
    fit_unobserved_components,
    fit_markov_regression,
    fit_vecm,
    fit_holt,
    fit_simple_exp_smoothing,
    fit_markov_autoregression,
    stl_forecast
)

from .timeseries_funcs import (
    calculate_acf,
    calculate_pacf,
    calculate_ccf,
    adf_test,
    kpss_test,
    coint_test,
    bds_test,
    q_stat,
    acorr_ljungbox,
    seasonal_decompose_additive,
    arma_order_select,
    detrend_linear,
    add_trend_to_data
)

from .glm import (
    fit_glm,
    fit_logit,
    predict_logit,
    fit_probit,
    fit_poisson,
    fit_negative_binomial,
    fit_mnlogit,
    fit_quantreg,
    fit_gee,
    fit_zero_inflated_poisson,
    fit_conditional_logit,
    fit_conditional_poisson,
    fit_generalized_poisson,
    fit_zero_inflated_generalized_poisson
)

# Import stats_extended module (100+ additional statistical functions)
from . import stats_extended

from .stats_tests import (
    ttest_ind,
    ttest_1samp,
    ztest,
    anova_lm,
    proportions_ztest,
    proportion_confint,
    chisquare_test,
    jarque_bera_test,
    omnibus_normtest,
    durbin_watson_test,
    het_breuschpagan,
    het_white,
    het_arch,
    acorr_breusch_godfrey,
    linear_reset,
    linear_harvey_collier,
    multipletests_correction,
    fdrcorrection_twostage,
    compare_means,
    descr_stats
)

from .power_analysis import (
    ttest_power,
    ftest_power,
    proportion_power,
    calculate_effect_size_cohens_d,
    calculate_effect_size_proportion,
    sample_size_proportion_confint,
    chisquare_power,
    anova_power
)

from .multivariate import (
    perform_pca,
    perform_factor_analysis,
    perform_manova,
    canonical_correlation
)

from .robust import (
    fit_rlm,
    mad,
    huber_scale,
    huber_location_scale
)

from .survival import (
    fit_cox_ph,
    survdiff,
    kaplan_meier
)

from .nonparametric import (
    kernel_density_estimation,
    kernel_regression,
    lowess_smoothing,
    multivariate_kde
)

from .gam import (
    fit_glm_gam,
    fit_gam_formula
)

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
