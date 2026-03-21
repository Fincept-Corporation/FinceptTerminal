"""
Statsmodels CLI - Unified Command Line Interface for Statsmodels Analytics
Provides a unified entry point for all statsmodels functionality in Fincept Terminal.

Categories:
- Time Series: ARIMA, SARIMAX, VAR, Exponential Smoothing, STL Decomposition
- Regression: OLS, WLS, GLS, Rolling Regression
- GLM: Logit, Probit, Poisson, Quantile Regression
- Statistical Tests: ADF, KPSS, Normality, Heteroscedasticity
- Power Analysis: Sample size, Effect size calculations
- Multivariate: PCA, Factor Analysis
- Nonparametric: KDE, LOWESS
"""

import sys
import os

# Add the script's directory to Python path to find statsmodels_wrapper
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

import json
import numpy as np
import pandas as pd
from typing import Dict, Any, List, Optional, Union
from datetime import datetime
from decimal import Decimal


class DecimalEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, Decimal):
            return float(obj)
        if isinstance(obj, datetime):
            return obj.isoformat()
        if isinstance(obj, np.ndarray):
            # Convert NaN/Inf to None in arrays
            return [self._sanitize_value(x) for x in obj.tolist()]
        if isinstance(obj, pd.Series):
            return [self._sanitize_value(x) for x in obj.tolist()]
        if isinstance(obj, pd.DataFrame):
            return obj.to_dict('records')
        if isinstance(obj, (np.int64, np.int32)):
            return int(obj)
        if isinstance(obj, (np.float64, np.float32)):
            return self._sanitize_value(float(obj))
        if pd.isna(obj):
            return None
        return super().default(obj)

    def _sanitize_value(self, val):
        """Convert NaN, Inf to None for valid JSON"""
        if isinstance(val, float):
            if np.isnan(val) or np.isinf(val):
                return None
        return val

    def encode(self, obj):
        """Override encode to handle NaN/Inf in nested structures"""
        return super().encode(self._sanitize_obj(obj))

    def _sanitize_obj(self, obj):
        """Recursively sanitize NaN/Inf values in nested structures"""
        if isinstance(obj, dict):
            return {k: self._sanitize_obj(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [self._sanitize_obj(x) for x in obj]
        elif isinstance(obj, float):
            if np.isnan(obj) or np.isinf(obj):
                return None
            return obj
        elif isinstance(obj, (np.float64, np.float32)):
            val = float(obj)
            if np.isnan(val) or np.isinf(val):
                return None
            return val
        return obj


class StatsmodelsEngine:
    def __init__(self):
        self._load_modules()

    def _load_modules(self):
        try:
            from statsmodels_wrapper import (
                fit_arima, forecast_arima, fit_sarimax, fit_varmax,
                fit_autoreg, fit_exponential_smoothing, stl_decompose,
                fit_holt, fit_simple_exp_smoothing, fit_markov_regression
            )
            self.ts_models = {
                'fit_arima': fit_arima,
                'forecast_arima': forecast_arima,
                'fit_sarimax': fit_sarimax,
                'fit_varmax': fit_varmax,
                'fit_autoreg': fit_autoreg,
                'fit_exponential_smoothing': fit_exponential_smoothing,
                'stl_decompose': stl_decompose,
                'fit_holt': fit_holt,
                'fit_simple_exp_smoothing': fit_simple_exp_smoothing,
                'fit_markov_regression': fit_markov_regression
            }
        except ImportError as e:
            self.ts_models = {}

        try:
            from statsmodels_wrapper import (
                calculate_acf, calculate_pacf, adf_test, kpss_test,
                coint_test, acorr_ljungbox, seasonal_decompose_additive,
                arma_order_select
            )
            self.ts_funcs = {
                'acf': calculate_acf,
                'pacf': calculate_pacf,
                'adf_test': adf_test,
                'kpss_test': kpss_test,
                'coint_test': coint_test,
                'ljung_box': acorr_ljungbox,
                'seasonal_decompose': seasonal_decompose_additive,
                'arma_order_select': arma_order_select
            }
        except ImportError:
            self.ts_funcs = {}

        try:
            from statsmodels_wrapper import (
                fit_ols, fit_wls, fit_gls, predict_ols,
                regression_diagnostics, fit_rolling_ols
            )
            self.regression = {
                'ols': fit_ols,
                'wls': fit_wls,
                'gls': fit_gls,
                'predict_ols': predict_ols,
                'diagnostics': regression_diagnostics,
                'rolling_ols': fit_rolling_ols
            }
        except ImportError:
            self.regression = {}

        try:
            from statsmodels_wrapper import (
                fit_glm, fit_logit, predict_logit, fit_probit,
                fit_poisson, fit_quantreg
            )
            self.glm = {
                'glm': fit_glm,
                'logit': fit_logit,
                'predict_logit': predict_logit,
                'probit': fit_probit,
                'poisson': fit_poisson,
                'quantreg': fit_quantreg
            }
        except ImportError:
            self.glm = {}

        try:
            from statsmodels_wrapper import (
                ttest_ind, ttest_1samp, ztest, anova_lm,
                jarque_bera_test, durbin_watson_test,
                het_breuschpagan, het_white, descr_stats
            )
            self.stats_tests = {
                'ttest_ind': ttest_ind,
                'ttest_1samp': ttest_1samp,
                'ztest': ztest,
                'anova': anova_lm,
                'jarque_bera': jarque_bera_test,
                'durbin_watson': durbin_watson_test,
                'breusch_pagan': het_breuschpagan,
                'white_test': het_white,
                'descriptive': descr_stats
            }
        except ImportError:
            self.stats_tests = {}

        try:
            from statsmodels_wrapper import (
                ttest_power, ftest_power, proportion_power,
                calculate_effect_size_cohens_d, anova_power
            )
            self.power = {
                'ttest_power': ttest_power,
                'ftest_power': ftest_power,
                'proportion_power': proportion_power,
                'effect_size': calculate_effect_size_cohens_d,
                'anova_power': anova_power
            }
        except ImportError:
            self.power = {}

        try:
            from statsmodels_wrapper import (
                perform_pca, perform_factor_analysis
            )
            self.multivariate = {
                'pca': perform_pca,
                'factor_analysis': perform_factor_analysis
            }
        except ImportError:
            self.multivariate = {}

        try:
            from statsmodels_wrapper import (
                kernel_density_estimation, lowess_smoothing
            )
            self.nonparametric = {
                'kde': kernel_density_estimation,
                'lowess': lowess_smoothing
            }
        except ImportError:
            self.nonparametric = {}

    def _parse_data(self, params: Dict[str, Any]) -> pd.Series:
        data = params.get('data', [])
        if isinstance(data, str):
            data = [float(x.strip()) for x in data.split(',') if x.strip()]
        if isinstance(data, dict):
            return pd.Series(data)
        return pd.Series(data)

    def _parse_dataframe(self, params: Dict[str, Any], key: str = 'data') -> pd.DataFrame:
        data = params.get(key, {})
        if isinstance(data, str):
            data = json.loads(data)
        return pd.DataFrame(data)

    def fit_arima(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'fit_arima' not in self.ts_models:
            return {"error": "ARIMA module not available"}

        data = self._parse_data(params)
        order = params.get('order', [1, 0, 0])
        if isinstance(order, str):
            order = json.loads(order)
        order = tuple(order)

        result = self.ts_models['fit_arima'](data, order=order)
        return {
            "success": True,
            "analysis_type": "arima",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def forecast_arima(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'forecast_arima' not in self.ts_models:
            return {"error": "ARIMA forecasting module not available"}

        data = self._parse_data(params)
        order = params.get('order', [1, 0, 0])
        if isinstance(order, str):
            order = json.loads(order)
        order = tuple(order)
        steps = int(params.get('steps', 10))

        result = self.ts_models['forecast_arima'](data, order=order, steps=steps)
        return {
            "success": True,
            "analysis_type": "arima_forecast",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def fit_sarimax(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'fit_sarimax' not in self.ts_models:
            return {"error": "SARIMAX module not available"}

        data = self._parse_data(params)
        order = params.get('order', [1, 0, 0])
        if isinstance(order, str):
            order = json.loads(order)
        order = tuple(order)

        seasonal_order = params.get('seasonal_order', [0, 0, 0, 12])
        if isinstance(seasonal_order, str):
            seasonal_order = json.loads(seasonal_order)
        seasonal_order = tuple(seasonal_order)

        result = self.ts_models['fit_sarimax'](data, order=order, seasonal_order=seasonal_order)
        return {
            "success": True,
            "analysis_type": "sarimax",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def fit_exponential_smoothing(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'fit_exponential_smoothing' not in self.ts_models:
            return {"error": "Exponential smoothing module not available"}

        data = self._parse_data(params)
        trend = params.get('trend', None)
        seasonal = params.get('seasonal', None)
        seasonal_periods = params.get('seasonal_periods', None)
        if seasonal_periods:
            seasonal_periods = int(seasonal_periods)

        result = self.ts_models['fit_exponential_smoothing'](
            data, trend=trend, seasonal=seasonal, seasonal_periods=seasonal_periods
        )
        return {
            "success": True,
            "analysis_type": "exponential_smoothing",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def stl_decompose(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'stl_decompose' not in self.ts_models:
            return {"error": "STL decomposition module not available"}

        data = self._parse_data(params)
        period = int(params.get('period', 12))
        seasonal = int(params.get('seasonal', 7))

        result = self.ts_models['stl_decompose'](data, period=period, seasonal=seasonal)
        return {
            "success": True,
            "analysis_type": "stl_decomposition",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def adf_test(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'adf_test' not in self.ts_funcs:
            return {"error": "ADF test module not available"}

        data = self._parse_data(params)
        result = self.ts_funcs['adf_test'](data)
        return {
            "success": True,
            "analysis_type": "adf_test",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def kpss_test(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'kpss_test' not in self.ts_funcs:
            return {"error": "KPSS test module not available"}

        data = self._parse_data(params)
        result = self.ts_funcs['kpss_test'](data)
        return {
            "success": True,
            "analysis_type": "kpss_test",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def calculate_acf(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'acf' not in self.ts_funcs:
            return {"error": "ACF module not available"}

        data = self._parse_data(params)
        nlags = int(params.get('nlags', 40))
        result = self.ts_funcs['acf'](data, nlags=nlags)
        return {
            "success": True,
            "analysis_type": "acf",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def calculate_pacf(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'pacf' not in self.ts_funcs:
            return {"error": "PACF module not available"}

        data = self._parse_data(params)
        nlags = int(params.get('nlags', 40))
        result = self.ts_funcs['pacf'](data, nlags=nlags)
        return {
            "success": True,
            "analysis_type": "pacf",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def ljung_box_test(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'ljung_box' not in self.ts_funcs:
            return {"error": "Ljung-Box test module not available"}

        data = self._parse_data(params)
        lags = int(params.get('lags', 10))
        result = self.ts_funcs['ljung_box'](data, lags=lags)
        return {
            "success": True,
            "analysis_type": "ljung_box",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def fit_ols(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'ols' not in self.regression:
            return {"error": "OLS regression module not available"}

        y = self._parse_data(params)
        X = params.get('X', None)
        if X is None:
            X = np.arange(len(y)).reshape(-1, 1)
        else:
            if isinstance(X, str):
                X = json.loads(X)
            X = np.array(X)

        result = self.regression['ols'](y.values, X)
        return {
            "success": True,
            "analysis_type": "ols_regression",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def regression_diagnostics(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'diagnostics' not in self.regression:
            return {"error": "Regression diagnostics module not available"}

        y = self._parse_data(params)
        X = params.get('X', None)
        if X is None:
            X = np.arange(len(y)).reshape(-1, 1)
        else:
            if isinstance(X, str):
                X = json.loads(X)
            X = np.array(X)

        result = self.regression['diagnostics'](y.values, X)
        return {
            "success": True,
            "analysis_type": "regression_diagnostics",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def fit_logit(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'logit' not in self.glm:
            return {"error": "Logit model module not available"}

        # For logit, we need both y (binary outcome) and X (predictors)
        # If only 'data' is provided, treat it as a dataframe with y and X columns
        # Or if 'y' and 'X' are separate, use those

        X = params.get('X', None)
        y_data = params.get('y', params.get('data', None))

        if y_data is None:
            return {"error": "Data (y) required for logit model"}

        # Parse y
        if isinstance(y_data, str):
            try:
                y_data = json.loads(y_data)
            except json.JSONDecodeError:
                y_data = [float(x.strip()) for x in y_data.split(',') if x.strip()]
        y = np.array(y_data) if isinstance(y_data, list) else np.array(y_data)

        # Logit requires binary outcomes in [0, 1] range
        # Convert non-binary data to binary if needed
        unique_vals = np.unique(y)

        if len(unique_vals) == 2:
            # Already binary-like, convert to 0/1
            if not (np.all(np.isin(unique_vals, [0, 1]))):
                y = (y == unique_vals[1]).astype(float)
        elif len(unique_vals) > 2:
            # Convert continuous data to binary using returns
            # Calculate returns: positive return = 1, negative = 0
            returns = np.diff(y) / y[:-1]
            y_binary = (returns >= 0).astype(float)
            y = y_binary
            # Adjust X to match the new length (one less due to diff)
            # We'll create X after this block, so just note the conversion
            conversion_note = f"Converted {len(unique_vals)} unique values to binary (positive/negative returns). Data length: {len(y)}"
        else:
            return {"error": "Data must have at least 2 values for logit analysis"}

        # Parse X or create default X (must match y length after conversion)
        X = params.get('X', None)
        if X is None:
            # If no X provided, create a simple trend predictor
            X = np.arange(len(y)).reshape(-1, 1)
        else:
            if isinstance(X, str):
                try:
                    X = json.loads(X)
                except json.JSONDecodeError:
                    X = [[float(x.strip())] for x in X.split(',') if x.strip()]
            X = np.array(X)

        result = self.glm['logit'](y, X)
        response = {
            "success": True,
            "analysis_type": "logit",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

        # Add conversion note if data was converted
        if 'conversion_note' in dir() and conversion_note:
            response["note"] = conversion_note
            response["data_info"] = {
                "original_unique_values": int(len(unique_vals)),
                "binary_samples": int(len(y)),
                "positive_returns": int(np.sum(y)),
                "negative_returns": int(len(y) - np.sum(y))
            }

        return response

    def fit_glm(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Fit Generalized Linear Model with automatic family selection"""
        if 'glm' not in self.glm:
            return {"error": "GLM module not available"}

        # Parse y data
        y_data = params.get('y', params.get('data', None))
        if y_data is None:
            return {"error": "Data (y) required for GLM model"}

        if isinstance(y_data, str):
            try:
                y_data = json.loads(y_data)
            except json.JSONDecodeError:
                y_data = [float(x.strip()) for x in y_data.split(',') if x.strip()]
        y = np.array(y_data) if isinstance(y_data, list) else np.array(y_data)

        # Parse X or create default X
        X = params.get('X', None)
        if X is None:
            X = np.arange(len(y)).reshape(-1, 1)
        else:
            if isinstance(X, str):
                try:
                    X = json.loads(X)
                except json.JSONDecodeError:
                    X = [[float(x.strip())] for x in X.split(',') if x.strip()]
            X = np.array(X)

        # Get family type - default to gaussian
        family = params.get('family', 'gaussian').lower()

        # Validate data for binomial family
        if family == 'binomial':
            # Ensure data is in [0, 1] range for binomial
            if np.any(y < 0) or np.any(y > 1):
                # Try to convert to binary (0/1)
                unique_vals = np.unique(y)
                if len(unique_vals) == 2:
                    y = (y == unique_vals[1]).astype(float)
                else:
                    return {"error": "For binomial family, y must be binary (0/1) or proportions in [0, 1]"}

        result = self.glm['glm'](y, X, family=family)
        return {
            "success": True,
            "analysis_type": "glm",
            "family": family,
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def ttest_independent(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'ttest_ind' not in self.stats_tests:
            return {"error": "T-test module not available"}

        sample1 = params.get('sample1', [])
        sample2 = params.get('sample2', [])
        if isinstance(sample1, str):
            sample1 = [float(x.strip()) for x in sample1.split(',')]
        if isinstance(sample2, str):
            sample2 = [float(x.strip()) for x in sample2.split(',')]

        result = self.stats_tests['ttest_ind'](sample1, sample2)
        return {
            "success": True,
            "analysis_type": "ttest_independent",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def ttest_one_sample(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'ttest_1samp' not in self.stats_tests:
            return {"error": "T-test module not available"}

        data = self._parse_data(params)
        popmean = float(params.get('popmean', 0))

        result = self.stats_tests['ttest_1samp'](data.values, popmean)
        return {
            "success": True,
            "analysis_type": "ttest_one_sample",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def jarque_bera(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'jarque_bera' not in self.stats_tests:
            return {"error": "Jarque-Bera test module not available"}

        data = self._parse_data(params)
        result = self.stats_tests['jarque_bera'](data.values)
        return {
            "success": True,
            "analysis_type": "jarque_bera",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def descriptive_stats(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'descriptive' not in self.stats_tests:
            return {"error": "Descriptive statistics module not available"}

        data = self._parse_data(params)
        result = self.stats_tests['descriptive'](data.values)
        return {
            "success": True,
            "analysis_type": "descriptive_statistics",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def ttest_power(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'ttest_power' not in self.power:
            return {"error": "Power analysis module not available"}

        effect_size = float(params.get('effect_size', 0.5))
        alpha = float(params.get('alpha', 0.05))
        alternative = params.get('alternative', 'two-sided')

        # Get power and nobs - exactly one must be None for solve_power to work
        power_val = params.get('power', None)
        nobs_val = params.get('nobs', None)

        # Convert to proper types if provided
        if power_val is not None and power_val != '' and power_val != 'null':
            power_val = float(power_val)
        else:
            power_val = None

        if nobs_val is not None and nobs_val != '' and nobs_val != 'null':
            nobs_val = int(nobs_val)
        else:
            nobs_val = None

        # If both are None, default to calculating power with nobs=50
        if power_val is None and nobs_val is None:
            nobs_val = 50
        # If both are provided, calculate power (ignore provided power)
        elif power_val is not None and nobs_val is not None:
            power_val = None

        result = self.power['ttest_power'](
            effect_size=effect_size, alpha=alpha, power=power_val, nobs=nobs_val,
            alternative=alternative
        )
        return {
            "success": True,
            "analysis_type": "ttest_power",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def perform_pca(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'pca' not in self.multivariate:
            return {"error": "PCA module not available"}

        data = self._parse_dataframe(params)
        # The wrapper function uses 'ncomp' not 'n_components'
        ncomp = params.get('n_components', params.get('ncomp', 2))
        if ncomp is not None:
            ncomp = int(ncomp)
        standardize = params.get('standardize', True)

        result = self.multivariate['pca'](data, ncomp=ncomp, standardize=standardize)
        return {
            "success": True,
            "analysis_type": "pca",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def kernel_density(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'kde' not in self.nonparametric:
            return {"error": "KDE module not available"}

        data = self._parse_data(params)
        result = self.nonparametric['kde'](data.values)
        return {
            "success": True,
            "analysis_type": "kernel_density",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def lowess_smooth(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if 'lowess' not in self.nonparametric:
            return {"error": "LOWESS module not available"}

        y = self._parse_data(params)
        x = params.get('x', None)
        if x is None:
            x = np.arange(len(y))
        else:
            if isinstance(x, str):
                x = [float(v.strip()) for v in x.split(',')]
            x = np.array(x)

        frac = float(params.get('frac', 0.3))
        result = self.nonparametric['lowess'](y.values, x, frac=frac)
        return {
            "success": True,
            "analysis_type": "lowess",
            "result": result,
            "timestamp": datetime.now().isoformat()
        }

    def get_available_analyses(self) -> Dict[str, Any]:
        analyses = {
            "time_series": {
                "arima": "Fit ARIMA model",
                "arima_forecast": "ARIMA forecasting",
                "sarimax": "Seasonal ARIMA with exogenous variables",
                "exponential_smoothing": "Exponential smoothing models",
                "stl_decompose": "STL seasonal decomposition",
                "holt": "Holt exponential smoothing",
                "autoreg": "Autoregressive model"
            },
            "stationarity_tests": {
                "adf_test": "Augmented Dickey-Fuller test",
                "kpss_test": "KPSS stationarity test",
                "acf": "Autocorrelation function",
                "pacf": "Partial autocorrelation function",
                "ljung_box": "Ljung-Box test for autocorrelation"
            },
            "regression": {
                "ols": "Ordinary Least Squares regression",
                "wls": "Weighted Least Squares",
                "gls": "Generalized Least Squares",
                "rolling_ols": "Rolling window OLS",
                "diagnostics": "Regression diagnostics"
            },
            "glm": {
                "logit": "Logistic regression",
                "probit": "Probit regression",
                "poisson": "Poisson regression",
                "quantreg": "Quantile regression"
            },
            "statistical_tests": {
                "ttest_ind": "Independent samples t-test",
                "ttest_1samp": "One-sample t-test",
                "jarque_bera": "Jarque-Bera normality test",
                "descriptive": "Descriptive statistics"
            },
            "power_analysis": {
                "ttest_power": "T-test power analysis",
                "ftest_power": "F-test power analysis",
                "effect_size": "Effect size calculation"
            },
            "multivariate": {
                "pca": "Principal Component Analysis",
                "factor_analysis": "Factor Analysis"
            },
            "nonparametric": {
                "kde": "Kernel Density Estimation",
                "lowess": "LOWESS smoothing"
            }
        }
        return {
            "success": True,
            "available_analyses": analyses,
            "timestamp": datetime.now().isoformat()
        }

    def execute(self, command: str, params: Dict[str, Any]) -> Dict[str, Any]:
        command_map = {
            "list": self.get_available_analyses,
            "arima": self.fit_arima,
            "arima_forecast": self.forecast_arima,
            "sarimax": self.fit_sarimax,
            "exponential_smoothing": self.fit_exponential_smoothing,
            "stl_decompose": self.stl_decompose,
            "adf_test": self.adf_test,
            "kpss_test": self.kpss_test,
            "acf": self.calculate_acf,
            "pacf": self.calculate_pacf,
            "ljung_box": self.ljung_box_test,
            "ols": self.fit_ols,
            "regression_diagnostics": self.regression_diagnostics,
            "logit": self.fit_logit,
            "glm": self.fit_glm,
            "ttest_ind": self.ttest_independent,
            "ttest_1samp": self.ttest_one_sample,
            "jarque_bera": self.jarque_bera,
            "descriptive": self.descriptive_stats,
            "ttest_power": self.ttest_power,
            "pca": self.perform_pca,
            "kde": self.kernel_density,
            "lowess": self.lowess_smooth
        }

        if command == "list":
            return command_map["list"]()

        handler = command_map.get(command)
        if not handler:
            return {
                "error": f"Unknown command: {command}",
                "available_commands": list(command_map.keys())
            }

        try:
            return handler(params)
        except Exception as e:
            import traceback
            return {
                "error": str(e),
                "traceback": traceback.format_exc(),
                "command": command,
                "timestamp": datetime.now().isoformat()
            }


def main(args: List[str] = None) -> str:
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        result = {
            "error": "No command provided",
            "usage": "statsmodels_cli.py <command> [params_json]",
            "available_commands": [
                "list", "arima", "arima_forecast", "sarimax",
                "exponential_smoothing", "stl_decompose", "adf_test",
                "kpss_test", "acf", "pacf", "ljung_box", "ols",
                "regression_diagnostics", "logit", "ttest_ind",
                "ttest_1samp", "jarque_bera", "descriptive",
                "ttest_power", "pca", "kde", "lowess"
            ]
        }
        return json.dumps(result, cls=DecimalEncoder)

    command = args[0]
    params = {}

    if len(args) > 1:
        try:
            params = json.loads(args[1])
        except json.JSONDecodeError:
            params = {"raw_input": args[1]}

    engine = StatsmodelsEngine()
    result = engine.execute(command, params)

    return json.dumps(result, cls=DecimalEncoder)


if __name__ == "__main__":
    print(main())
