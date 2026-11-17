
"""Quantitative Quant Modules 3042 Module
=================================

Quantitative analysis modules and functions

===== DATA SOURCES REQUIRED =====
INPUT:
  - High-frequency market data and price series
  - Order book data and market microstructure
  - Alternative data sources and sentiment indicators
  - Economic data and market fundamentals
  - Historical factor returns and premiums

OUTPUT:
  - Quantitative trading signals and strategies
  - Factor model implementations and analysis
  - Risk models and portfolio construction methods
  - Backtest results and performance attribution
  - Alpha generation and research insights

PARAMETERS:
  - factor_model: Factor model type (default: 'fama_french_5')
  - lookback_period: Historical lookback window (default: 252 days)
  - rebalance_frequency: Strategy rebalancing frequency (default: 'monthly')
  - universe_size: Investment universe size (default: 1000)
  - risk_model: Risk model for portfolio construction (default: 'barra')
"""



import numpy as np
import pandas as pd
from typing import Union, Dict, List, Any, Optional, Tuple, Callable
from scipy import stats
import warnings

# scikit-learn is imported inside the machine-learning methods only: the CLI builds this
# analyzer for every command, and the sampling / time-series commands never need it.

from .base_calculator import BaseCalculator, validate_inputs, timing_decorator
from .exceptions import handle_calculation_error, DataValidationError, CalculationError

# Forecasting methods offered by generate_forecasts (the CFA Quant panel offers the first six).
FORECAST_METHODS = ('simple_exponential', 'holt', 'holt_winters', 'arima', 'naive', 'drift',
                    'linear_trend', 'moving_average')

REGRESSION_MODELS = ('ridge', 'lasso', 'elastic_net', 'svr', 'knn', 'decision_tree', 'random_forest')
CLASSIFICATION_MODELS = ('logistic', 'ridge', 'svc', 'knn', 'decision_tree', 'random_forest')


class AdvancedQuantAnalyzer(BaseCalculator):
    """Comprehensive analyzer for advanced quantitative methods."""

    def __init__(self, precision: int = 6, random_seed: Optional[int] = 42):
        super().__init__(precision)
        self.random_seed = random_seed
        if random_seed is not None:
            np.random.seed(random_seed)

    def get_supported_methods(self) -> List[str]:
        return [
            # Time Series
            'trend_analysis', 'stationarity_test', 'arima_model', 'forecasting',
            # Machine Learning
            'supervised_learning', 'unsupervised_learning', 'model_evaluation', 'feature_engineering',
            # Sampling
            'sampling_techniques', 'central_limit_theorem', 'resampling_methods', 'sampling_error_analysis'
        ]

    # ========================================
    # TIME SERIES ANALYSIS METHODS
    # ========================================

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def analyze_trend(self,
                      data: Union[List, np.ndarray, pd.Series],
                      trend_type: str = 'linear',
                      dates: Optional[pd.DatetimeIndex] = None) -> Dict[str, Any]:
        """Analyze linear and log-linear trends in time series data."""
        data = self._validate_returns(data, "data")
        self._check_data_length(data, min_length=10)

        if trend_type not in ['linear', 'log_linear']:
            raise DataValidationError(f"Invalid trend_type: {trend_type}", "trend_type")

        # Create time index
        if dates is not None:
            time_index = np.arange(len(dates))
        else:
            time_index = np.arange(len(data))

        # Prepare data for regression
        if trend_type == 'log_linear':
            if np.any(data <= 0):
                raise DataValidationError("Log-linear trend requires positive data", "data")
            y_data = np.log(data)
        else:
            y_data = data

        # Fit trend using OLS
        X = np.column_stack([np.ones(len(time_index)), time_index])
        coefficients = np.linalg.lstsq(X, y_data, rcond=None)[0]

        intercept, slope = coefficients
        fitted_values = intercept + slope * time_index
        residuals = y_data - fitted_values

        # Transform back if log-linear
        if trend_type == 'log_linear':
            fitted_values_original = np.exp(fitted_values)
            residuals_original = data - fitted_values_original
        else:
            fitted_values_original = fitted_values
            residuals_original = residuals

        # Calculate R-squared
        ss_res = np.sum(residuals ** 2)
        ss_tot = np.sum((y_data - np.mean(y_data)) ** 2)
        r_squared = 1 - (ss_res / ss_tot) if ss_tot != 0 else 0

        # Statistical tests
        n = len(data)
        mse = ss_res / (n - 2)
        var_slope = mse / np.sum((time_index - np.mean(time_index)) ** 2)
        t_stat = slope / np.sqrt(var_slope) if var_slope > 0 else np.inf
        p_value = 2 * (1 - stats.t.cdf(abs(t_stat), n - 2))

        return self._create_result_dict(
            value={'slope': slope, 'intercept': intercept},
            method=f'{trend_type}_trend',
            parameters={
                'data': data.tolist() if hasattr(data, 'tolist') else list(data),
                'trend_type': trend_type
            },
            metadata={
                'fitted_values': fitted_values_original.tolist(),
                'residuals': residuals_original.tolist(),
                'r_squared': r_squared,
                'slope_t_statistic': t_stat,
                'slope_p_value': p_value,
                'trend_significant': p_value < 0.05,
                'trend_direction': 'upward' if slope > 0 else 'downward' if slope < 0 else 'flat'
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def test_stationarity(self,
                          data: Union[List, np.ndarray, pd.Series],
                          test_type: str = 'adf') -> Dict[str, Any]:
        """Test for stationarity using various unit root tests."""
        data = self._validate_returns(data, "data")
        self._check_data_length(data, min_length=20)

        # Phillips-Perron ('pp') used to be accepted here and returned an empty result; statsmodels has
        # no PP test (it lives in the 'arch' package, which is not installed).
        if test_type not in ['adf', 'kpss']:
            raise DataValidationError(f"Invalid test_type: {test_type} (available: adf, kpss)", "test_type")

        results = {}

        if test_type == 'adf':
            # Augmented Dickey-Fuller test
            from statsmodels.tsa.stattools import adfuller
            adf_result = adfuller(data, autolag='AIC')

            results = {
                'test_statistic': float(adf_result[0]),
                'p_value': float(adf_result[1]),
                'critical_values': {k: float(v) for k, v in adf_result[4].items()},
                # A real bool: the panel reads this with QJsonValue::toBool().
                'is_stationary': bool(adf_result[1] < 0.05),
                'lags_used': int(adf_result[2]),
                'null_hypothesis': 'Unit root (non-stationary)'
            }

        elif test_type == 'kpss':
            # KPSS test
            from statsmodels.tsa.stattools import kpss
            with warnings.catch_warnings():
                # statsmodels warns when the statistic is outside its p-value table; the p-value is then
                # the table bound (0.01 or 0.1), which is what we report.
                warnings.simplefilter('ignore')
                kpss_result = kpss(data, regression='c', nlags='auto')

            results = {
                'test_statistic': float(kpss_result[0]),
                'p_value': float(kpss_result[1]),
                'critical_values': {k: float(v) for k, v in kpss_result[3].items()},
                'is_stationary': bool(kpss_result[1] > 0.05),
                'lags_used': int(kpss_result[2]),
                'null_hypothesis': 'Stationary'
            }

        # Additional stationarity indicators
        rolling_mean = pd.Series(data).rolling(window=min(20, len(data) // 4)).mean()
        rolling_std = pd.Series(data).rolling(window=min(20, len(data) // 4)).std()

        return self._create_result_dict(
            value=results,
            method=f'{test_type}_stationarity_test',
            parameters={
                'data': data.tolist() if hasattr(data, 'tolist') else list(data),
                'test_type': test_type
            },
            metadata={
                'rolling_mean_stability': np.std(rolling_mean.dropna()),
                'rolling_std_stability': np.std(rolling_std.dropna()),
                'mean_reversion_strength': abs(np.corrcoef(data[:-1], np.diff(data))[0, 1]) if len(data) > 1 else 0
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def fit_arima_model(self,
                        data: Union[List, np.ndarray, pd.Series],
                        order: Tuple[int, int, int] = (1, 0, 1),
                        seasonal_order: Optional[Tuple[int, int, int, int]] = None) -> Dict[str, Any]:
        """Fit ARIMA model to time series data."""
        data = self._validate_returns(data, "data")

        # Validate order parameter
        if order is None or not isinstance(order, (tuple, list)) or len(order) != 3:
            raise DataValidationError("order must be a tuple/list of 3 integers (p, d, q)", "order")

        order = tuple(order)  # Ensure it's a tuple
        self._check_data_length(data, min_length=max(20, sum(order) * 3))

        try:
            from statsmodels.tsa.arima.model import ARIMA

            # Fit ARIMA model - only pass seasonal_order if it's not None
            if seasonal_order is not None:
                model = ARIMA(data, order=order, seasonal_order=seasonal_order)
            else:
                model = ARIMA(data, order=order)
            fitted_model = model.fit()

            # Model diagnostics
            residuals = fitted_model.resid
            fitted_values = fitted_model.fittedvalues

            # Information criteria
            aic = fitted_model.aic
            bic = fitted_model.bic

            # Ljung-Box test for residual autocorrelation
            from statsmodels.stats.diagnostic import acorr_ljungbox
            ljung_box = acorr_ljungbox(residuals, lags=min(10, len(residuals) // 4), return_df=True)

            return self._create_result_dict(
                value={
                    'parameters': fitted_model.params.tolist() if hasattr(fitted_model.params, 'tolist') else list(fitted_model.params),
                    'fitted_values': fitted_values.tolist(),
                    'residuals': residuals.tolist()
                },
                method='arima_model',
                parameters={
                    'data': data.tolist() if hasattr(data, 'tolist') else list(data),
                    'order': order,
                    'seasonal_order': seasonal_order
                },
                metadata={
                    'aic': aic,
                    'bic': bic,
                    'log_likelihood': fitted_model.llf,
                    'ljung_box_p_value': ljung_box['lb_pvalue'].iloc[-1],
                    'residuals_autocorrelated': ljung_box['lb_pvalue'].iloc[-1] < 0.05,
                    'model_summary': str(fitted_model.summary())
                }
            )

        except ImportError:
            # Fallback: Simple AR(1) model
            if order[0] == 1 and order[1] == 0 and order[2] == 0:
                return self._fit_simple_ar1(data)
            else:
                raise DataValidationError("statsmodels required for ARIMA modeling", "dependencies")

    def _fit_simple_ar1(self, data: np.ndarray) -> Dict[str, Any]:
        """Fallback AR(1) implementation."""
        y = data[1:]
        x = data[:-1]

        # OLS estimation
        X = np.column_stack([np.ones(len(x)), x])
        coefficients = np.linalg.lstsq(X, y, rcond=None)[0]

        intercept, phi = coefficients
        fitted_values = intercept + phi * data[:-1]
        residuals = y - fitted_values

        return self._create_result_dict(
            value={
                'parameters': {'const': intercept, 'ar.L1': phi},
                'fitted_values': np.concatenate([[data[0]], fitted_values]).tolist(),
                'residuals': np.concatenate([[0], residuals]).tolist()
            },
            method='ar1_model',
            parameters={'data': data.tolist()},
            metadata={
                'r_squared': 1 - np.var(residuals) / np.var(y),
                'is_stationary': abs(phi) < 1
            }
        )

    def _forecast_series(self,
                         series: np.ndarray,
                         horizon: int,
                         method: str,
                         options: Dict[str, Any]) -> Tuple[np.ndarray, Dict[str, Any]]:
        """Point forecasts for the `horizon` steps after the last value of `series`.

        Returns (forecast values, fit details). Raises DataValidationError when the method
        cannot be fitted to this series - never a placeholder forecast.
        """
        n = len(series)
        info: Dict[str, Any] = {}

        if method == 'naive':
            values = np.repeat(series[-1], horizon)

        elif method == 'drift':
            # Random walk with drift: last value plus the average historical change per step.
            slope = (series[-1] - series[0]) / (n - 1)
            values = series[-1] + slope * np.arange(1, horizon + 1)
            info['drift_per_step'] = float(slope)

        elif method == 'simple_exponential':
            alpha = float(options.get('alpha', 0.3))
            if not 0.0 < alpha <= 1.0:
                raise DataValidationError(f"alpha must be in (0, 1], got {alpha}", "alpha")
            level = series[0]
            for x in series[1:]:
                level = alpha * x + (1.0 - alpha) * level
            values = np.repeat(level, horizon)
            info['alpha'] = alpha

        elif method == 'linear_trend':
            t = np.arange(n)
            slope, intercept = np.polyfit(t, series, 1)
            values = intercept + slope * np.arange(n, n + horizon)
            info['slope'] = float(slope)
            info['intercept'] = float(intercept)

        elif method == 'moving_average':
            window = max(1, min(5, n // 4))
            values = np.repeat(np.mean(series[-window:]), horizon)
            info['window'] = window

        elif method in ('holt', 'holt_winters'):
            from statsmodels.tsa.holtwinters import ExponentialSmoothing
            if method == 'holt':
                model = ExponentialSmoothing(series, trend='add', initialization_method='estimated')
            else:
                m = int(options.get('seasonal_periods') or 0)
                if m < 2:
                    raise DataValidationError(f"seasonal_periods must be at least 2, got {m}", "seasonal_periods")
                if n < 2 * m:
                    raise DataValidationError(
                        f"holt_winters needs at least two full seasonal cycles ({2 * m} observations at "
                        f"seasonal_periods={m}) to fit, got {n}. Use a longer period or a smaller "
                        f"seasonal_periods", "data")
                seasonal = options.get('seasonal', 'add')
                if seasonal not in ('add', 'mul'):
                    raise DataValidationError(f"seasonal must be 'add' or 'mul', got {seasonal}", "seasonal")
                if seasonal == 'mul' and np.any(series <= 0):
                    raise DataValidationError("multiplicative seasonality needs strictly positive data", "seasonal")
                model = ExponentialSmoothing(series, trend='add', seasonal=seasonal, seasonal_periods=m,
                                             initialization_method='estimated')
                info['seasonal_periods'] = m
                info['seasonal'] = seasonal
            with warnings.catch_warnings():
                warnings.simplefilter('ignore')  # optimiser convergence chatter
                fit = model.fit()
            values = np.asarray(fit.forecast(horizon), dtype=float)
            for key in ('smoothing_level', 'smoothing_trend', 'smoothing_seasonal'):
                val = fit.params.get(key)
                if val is not None and np.isfinite(val):
                    info[key] = float(val)
            info['aic'] = float(fit.aic)

        elif method == 'arima':
            from statsmodels.tsa.arima.model import ARIMA
            order = options.get('order') or (1, 1, 1)
            if not isinstance(order, (list, tuple)) or len(order) != 3:
                raise DataValidationError("order must be a list of 3 integers (p, d, q)", "order")
            order = tuple(int(o) for o in order)
            if n < max(20, sum(order) * 3):
                raise DataValidationError(
                    f"ARIMA{order} needs at least {max(20, sum(order) * 3)} observations to fit, got {n}", "data")
            with warnings.catch_warnings():
                warnings.simplefilter('ignore')
                fit = ARIMA(series, order=order).fit()
            values = np.asarray(fit.forecast(horizon), dtype=float)
            info['order'] = list(order)
            info['aic'] = float(fit.aic)

        else:
            raise DataValidationError(
                f"Unknown forecasting method: {method} (available: {', '.join(FORECAST_METHODS)})", "method")

        values = np.asarray(values, dtype=float)
        if not np.all(np.isfinite(values)):
            raise CalculationError("the model produced non-finite forecasts for this series", method)
        return values, info

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def generate_forecasts(self,
                           data: Union[List, np.ndarray, pd.Series],
                           horizon: int = 5,
                           method: str = 'simple_exponential',
                           train_size: float = 0.8,
                           options: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Fit `method` on the first `train_size` share of `data` and forecast `horizon` steps.

        `value` is the forecast from the end of the training window (so it can be scored
        against the held-out tail: mae / rmse / mape). The same method refitted on the full
        series gives metadata.forecast_out_of_sample - the forecast beyond the last observation.
        options: alpha (simple_exponential), seasonal_periods / seasonal (holt_winters), order (arima).
        """
        options = dict(options or {})
        data = self._validate_returns(data, "data")
        self._check_data_length(data, min_length=20)

        horizon = int(self._validate_positive(horizon, "horizon"))
        train_size = self._validate_probability(train_size, "train_size")
        if method not in FORECAST_METHODS:
            raise DataValidationError(
                f"Unknown forecasting method: {method} (available: {', '.join(FORECAST_METHODS)})", "method")

        # Split data
        split_point = int(len(data) * train_size)
        if split_point < 10:
            raise DataValidationError(
                f"train_size {train_size} leaves {split_point} training observations; at least 10 are needed",
                "train_size")
        train_data = data[:split_point]
        test_data = data[split_point:]

        forecast_values, fit_info = self._forecast_series(train_data, horizon, method, options)

        # Accuracy on the overlap between the forecast and the held-out tail
        n_compare = min(len(test_data), horizon)
        if n_compare > 0:
            actual = test_data[:n_compare]
            err = actual - forecast_values[:n_compare]
            mae = float(np.mean(np.abs(err)))
            rmse = float(np.sqrt(np.mean(err ** 2)))
            nonzero = actual != 0
            mape = float(np.mean(np.abs(err[nonzero] / actual[nonzero])) * 100) if nonzero.any() else None
        else:
            mae = rmse = mape = None

        # Forecast beyond the last observation (same method, fitted on the whole series)
        out_of_sample, out_of_sample_error = None, None
        if len(test_data) > 0:
            try:
                out_of_sample = self._forecast_series(data, horizon, method, options)[0].tolist()
            except Exception as exc:  # reported, not replaced by a made-up number
                out_of_sample_error = str(exc)
        else:
            out_of_sample = forecast_values.tolist()

        metadata = {
            'train_data_size': len(train_data),
            'test_data_size': len(test_data),
            'n_compared': n_compare,
            'mae': mae,
            'rmse': rmse,
            'mape': mape,
            'forecast_horizon': horizon,
            'forecast_origin': 'end_of_training_window',
            'fit': fit_info,
            'forecast_out_of_sample': out_of_sample,
        }
        if out_of_sample_error:
            metadata['forecast_out_of_sample_error'] = out_of_sample_error

        return self._create_result_dict(
            value=forecast_values.tolist(),
            method=f'forecast_{method}',
            parameters={
                'data': data.tolist() if hasattr(data, 'tolist') else list(data),
                'horizon': horizon,
                'method': method,
                'train_size': train_size
            },
            metadata=metadata
        )

    # ========================================
    # MACHINE LEARNING METHODS
    # ========================================

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def supervised_learning_analysis(self,
                                     X: Union[np.ndarray, pd.DataFrame],
                                     y: Union[np.ndarray, pd.Series],
                                     problem_type: str = 'regression',
                                     algorithms: List[str] = ['ridge', 'random_forest']) -> Dict[str, Any]:
        """Comprehensive supervised learning analysis."""
        # Validate inputs
        if isinstance(X, pd.DataFrame):
            X = X.values
        if isinstance(y, pd.Series):
            y = y.values

        X = self._validate_numeric_input(X, "X")
        y = self._validate_numeric_input(y, "y")

        if len(X) != len(y):
            raise DataValidationError(f"Length mismatch: X({len(X)}) vs y({len(y)})", "data")

        if problem_type not in ['regression', 'classification']:
            raise DataValidationError(f"Invalid problem_type: {problem_type}", "problem_type")
        if X.ndim == 1:
            X = X.reshape(-1, 1)
        if len(X) < 10:
            raise DataValidationError(f"Need at least 10 samples, got {len(X)}", "data")

        from sklearn.model_selection import train_test_split
        from sklearn.metrics import (mean_squared_error, r2_score, accuracy_score, precision_score,
                                     recall_score)

        # Split data (chronological: the test set is the most recent 20 %)
        X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, shuffle=False)

        results = {}

        for algorithm in algorithms:
            try:
                if problem_type == 'regression':
                    model = self._get_regression_model(algorithm)
                else:
                    model = self._get_classification_model(algorithm)

                # Scaling (SVR / KNN / logistic ...) is inside the returned pipeline.
                model.fit(X_train, y_train)
                y_pred = model.predict(X_test)

                # Calculate metrics
                if problem_type == 'regression':
                    mse = mean_squared_error(y_test, y_pred)
                    r2 = r2_score(y_test, y_pred)
                    results[algorithm] = {
                        'model': self._estimator_name(model),
                        'mse': mse,
                        'rmse': np.sqrt(mse),
                        'r2_score': r2,
                        'predictions': y_pred.tolist()
                    }
                else:
                    accuracy = accuracy_score(y_test, y_pred)
                    precision = precision_score(y_test, y_pred, average='weighted', zero_division=0)
                    recall = recall_score(y_test, y_pred, average='weighted', zero_division=0)

                    results[algorithm] = {
                        'model': self._estimator_name(model),
                        'accuracy': accuracy,
                        'precision': precision,
                        'recall': recall,
                        'predictions': y_pred.tolist()
                    }

            except Exception as e:
                results[algorithm] = {'error': str(e)}

        # Best algorithm among the ones that actually ran (None when every one failed)
        score_key = 'r2_score' if problem_type == 'regression' else 'accuracy'
        scored = {k: v[score_key] for k, v in results.items()
                  if 'error' not in v and v.get(score_key) is not None and np.isfinite(v[score_key])}
        best_algorithm = max(scored, key=scored.get) if scored else None

        metadata = {
            'train_size': len(X_train),
            'test_size': len(X_test),
            'y_test': y_test.tolist(),
            'n_features': X.shape[1],
            'best_algorithm': best_algorithm,
            'n_failed': sum(1 for v in results.values() if 'error' in v),
        }
        if problem_type == 'classification':
            classes, counts = np.unique(y_test, return_counts=True)
            # Accuracy of always predicting the most common test class - the bar a classifier must beat.
            metadata['majority_class_accuracy'] = float(counts.max() / counts.sum()) if len(counts) else None

        return self._create_result_dict(
            value=results,
            method=f'supervised_learning_{problem_type}',
            parameters={
                'X_shape': X.shape,
                'y_shape': y.shape,
                'problem_type': problem_type,
                'algorithms': algorithms
            },
            metadata=metadata
        )

    @staticmethod
    def _estimator_name(model) -> str:
        """Class name of the fitted estimator (the last step of a pipeline / the wrapped regressor)."""
        est = model
        if hasattr(est, 'regressor'):  # TransformedTargetRegressor
            est = est.regressor
        if hasattr(est, 'steps'):  # Pipeline
            est = est.steps[-1][1]
        return type(est).__name__

    def _get_regression_model(self, algorithm: str):
        """Regression model for `algorithm`; raises DataValidationError for an unknown name."""
        from sklearn.pipeline import make_pipeline
        from sklearn.preprocessing import StandardScaler
        name = str(algorithm).strip().lower()
        if name == 'ridge':
            from sklearn.linear_model import Ridge
            return Ridge(alpha=1.0, random_state=self.random_seed)
        if name == 'lasso':
            from sklearn.linear_model import Lasso
            return Lasso(alpha=1.0, random_state=self.random_seed)
        if name == 'elastic_net':
            from sklearn.linear_model import ElasticNet
            return ElasticNet(alpha=1.0, random_state=self.random_seed)
        if name in ('svr', 'svm'):
            # RBF SVR is scale-sensitive in X *and* y (epsilon / C are in target units), so both are
            # standardised; predictions come back in the original units.
            from sklearn.svm import SVR
            from sklearn.compose import TransformedTargetRegressor
            return TransformedTargetRegressor(regressor=make_pipeline(StandardScaler(), SVR(kernel='rbf')),
                                              transformer=StandardScaler())
        if name == 'knn':
            from sklearn.neighbors import KNeighborsRegressor
            return make_pipeline(StandardScaler(), KNeighborsRegressor(n_neighbors=5))
        if name == 'decision_tree':
            from sklearn.tree import DecisionTreeRegressor
            return DecisionTreeRegressor(random_state=self.random_seed)
        if name == 'random_forest':
            from sklearn.ensemble import RandomForestRegressor
            return RandomForestRegressor(n_estimators=100, random_state=self.random_seed)
        raise DataValidationError(
            f"Unknown regression model '{algorithm}' (available: {', '.join(REGRESSION_MODELS)})", "model_type")

    def _get_classification_model(self, algorithm: str):
        """Classification model for `algorithm`; raises DataValidationError for an unknown name."""
        from sklearn.pipeline import make_pipeline
        from sklearn.preprocessing import StandardScaler
        name = str(algorithm).strip().lower()
        if name in ('logistic', 'logistic_regression'):
            from sklearn.linear_model import LogisticRegression
            return make_pipeline(StandardScaler(), LogisticRegression(max_iter=1000))
        if name == 'ridge':
            from sklearn.linear_model import RidgeClassifier
            return make_pipeline(StandardScaler(), RidgeClassifier(alpha=1.0))
        if name in ('svc', 'svm', 'svr'):
            # 'svr' is accepted because the panel offers one model list for both problem types;
            # for classification it means the support-vector classifier (reported as model: SVC).
            from sklearn.svm import SVC
            return make_pipeline(StandardScaler(), SVC(kernel='rbf', random_state=self.random_seed))
        if name == 'knn':
            from sklearn.neighbors import KNeighborsClassifier
            return make_pipeline(StandardScaler(), KNeighborsClassifier(n_neighbors=5))
        if name == 'decision_tree':
            from sklearn.tree import DecisionTreeClassifier
            return DecisionTreeClassifier(random_state=self.random_seed)
        if name == 'random_forest':
            from sklearn.ensemble import RandomForestClassifier
            return RandomForestClassifier(n_estimators=100, random_state=self.random_seed)
        if name in ('lasso', 'elastic_net'):
            raise DataValidationError(
                f"'{algorithm}' is a regression model; for classification use one of: "
                f"{', '.join(CLASSIFICATION_MODELS)}", "algorithm")
        raise DataValidationError(
            f"Unknown classification model '{algorithm}' (available: {', '.join(CLASSIFICATION_MODELS)})",
            "algorithm")

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def unsupervised_learning_analysis(self,
                                       X: Union[np.ndarray, pd.DataFrame],
                                       methods: List[str] = ['pca', 'kmeans']) -> Dict[str, Any]:
        """Unsupervised learning analysis including PCA and clustering."""
        if isinstance(X, pd.DataFrame):
            X = X.values

        X = self._validate_numeric_input(X, "X")
        if X.ndim == 1:
            X = X.reshape(-1, 1)
        if len(X) < 4:
            raise DataValidationError(f"Need at least 4 samples, got {len(X)}", "X")

        from sklearn.preprocessing import StandardScaler
        from sklearn.cluster import KMeans, AgglomerativeClustering
        from sklearn.decomposition import PCA
        from sklearn.metrics import silhouette_score

        # Scale data
        scaler = StandardScaler()
        X_scaled = scaler.fit_transform(X)

        results = {}

        for requested in methods:
            method = str(requested).strip().lower()
            # The panel offers "agglomerative"; the result key the panel reads is "hierarchical".
            if method == 'agglomerative':
                method = 'hierarchical'
            try:
                if method not in ('pca', 'kmeans', 'hierarchical'):
                    raise DataValidationError(
                        f"Unknown method '{requested}' (available: pca, kmeans, hierarchical/agglomerative)",
                        "methods")
                if method == 'pca':
                    # Principal Component Analysis
                    pca = PCA()
                    X_pca = pca.fit_transform(X_scaled)

                    # Calculate cumulative explained variance
                    cumvar = np.cumsum(pca.explained_variance_ratio_)
                    n_components_95 = np.argmax(cumvar >= 0.95) + 1

                    results['pca'] = {
                        'explained_variance_ratio': pca.explained_variance_ratio_.tolist(),
                        'cumulative_variance': cumvar.tolist(),
                        'components_for_95_variance': n_components_95,
                        'principal_components': X_pca.tolist()
                    }

                elif method == 'kmeans':
                    # K-means clustering
                    inertias = []
                    silhouette_scores = []
                    k_range = range(2, min(11, len(X) // 2))
                    if len(k_range) == 0:
                        raise DataValidationError(f"k-means needs at least 6 samples, got {len(X)}", "X")

                    for k in k_range:
                        kmeans = KMeans(n_clusters=k, random_state=self.random_seed)
                        cluster_labels = kmeans.fit_predict(X_scaled)
                        inertias.append(kmeans.inertia_)

                        # Silhouette score
                        sil_score = silhouette_score(X_scaled, cluster_labels)
                        silhouette_scores.append(sil_score)

                    # Find optimal k
                    optimal_k = k_range[np.argmax(silhouette_scores)]

                    # Final clustering with optimal k
                    final_kmeans = KMeans(n_clusters=optimal_k, random_state=self.random_seed)
                    final_labels = final_kmeans.fit_predict(X_scaled)

                    results['kmeans'] = {
                        'optimal_k': optimal_k,
                        'cluster_labels': final_labels.tolist(),
                        'cluster_centers': final_kmeans.cluster_centers_.tolist(),
                        'inertia_by_k': list(zip(k_range, inertias)),
                        'silhouette_by_k': list(zip(k_range, silhouette_scores))
                    }

                elif method == 'hierarchical':
                    # Hierarchical clustering
                    hierarchical = AgglomerativeClustering(n_clusters=3)
                    cluster_labels = hierarchical.fit_predict(X_scaled)

                    results['hierarchical'] = {
                        'cluster_labels': cluster_labels.tolist(),
                        'n_clusters': 3
                    }

            except Exception as e:
                results[method] = {'error': str(e)}

        return self._create_result_dict(
            value=results,
            method='unsupervised_learning',
            parameters={
                'X_shape': X.shape,
                'methods': methods
            },
            metadata={
                'n_samples': X.shape[0],
                'n_features': X.shape[1],
                'data_scaled': True
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def evaluate_model_performance(self,
                                   X: Union[np.ndarray, pd.DataFrame],
                                   y: Union[np.ndarray, pd.Series],
                                   model_type: str = 'random_forest',
                                   cv_folds: int = 5,
                                   problem_type: Optional[str] = None,
                                   holdout_fraction: float = 0.2,
                                   overfitting_threshold: float = 0.1) -> Dict[str, Any]:
        """Time-ordered model evaluation: walk-forward CV, a learning curve and a hold-out check.

        * cv_scores: TimeSeriesSplit(cv_folds) over the whole sample - every fold is scored on
          observations that come after the ones it was trained on.
        * The last `holdout_fraction` of the sample is a fixed validation set. The learning curve
          trains on growing shares of the earlier part (train_sizes) and scores each fit on its own
          training rows (train_scores) and on the hold-out (val_scores).
        * final_train_score / final_val_score come from the fit on the whole training part;
          is_overfitting = final_train_score - final_val_score > overfitting_threshold.
        problem_type: 'regression' / 'classification'; when None it is classification only if y
        is integer-valued with at most 5 distinct classes.
        """
        from sklearn.model_selection import TimeSeriesSplit, cross_val_score
        from sklearn.metrics import r2_score, accuracy_score

        if isinstance(X, pd.DataFrame):
            X = X.values
        if isinstance(y, pd.Series):
            y = y.values
        X = self._validate_numeric_input(np.asarray(X, dtype=float), "X")
        y = self._validate_numeric_input(np.asarray(y, dtype=float), "y")
        if X.ndim == 1:
            X = X.reshape(-1, 1)
        if len(X) != len(y):
            raise DataValidationError(f"Length mismatch: X({len(X)}) vs y({len(y)})", "data")

        # Determine problem type
        if problem_type is None:
            classes = np.unique(y)
            integer_valued = bool(np.all(np.mod(y, 1) == 0))
            problem_type = 'classification' if integer_valued and len(classes) <= 5 else 'regression'
        elif problem_type not in ('regression', 'classification'):
            raise DataValidationError(f"Invalid problem_type: {problem_type}", "problem_type")

        cv_folds = int(cv_folds)
        if cv_folds < 2:
            raise DataValidationError(f"cv_folds must be at least 2, got {cv_folds}", "cv_folds")

        n = len(X)
        n_holdout = int(round(n * holdout_fraction))
        n_train = n - n_holdout
        min_train = 10
        if n_holdout < 2 or n_train < min_train:
            raise DataValidationError(
                f"Need at least {int(np.ceil((min_train + 2) / (1 - holdout_fraction)))} samples for a "
                f"{holdout_fraction:.0%} hold-out evaluation, got {n}", "data")
        if n // (cv_folds + 1) < 2:
            raise DataValidationError(
                f"{n} samples are too few for {cv_folds} time-series CV folds (each fold needs at least 2 "
                f"test samples); use fewer folds or more data", "cv_folds")

        # Get model
        if problem_type == 'regression':
            model = self._get_regression_model(model_type)
            scoring, scorer = 'r2', r2_score
        else:
            model = self._get_classification_model(model_type)
            scoring, scorer = 'accuracy', accuracy_score

        # Walk-forward cross-validation
        cv_scores = cross_val_score(model, X, y, cv=TimeSeriesSplit(n_splits=cv_folds), scoring=scoring)

        # Learning curve on the training part, validated on the fixed hold-out
        X_fit, y_fit = X[:n_train], y[:n_train]
        X_val, y_val = X[n_train:], y[n_train:]
        train_sizes = sorted({min(n_train, max(min_train, int(round(f * n_train))))
                              for f in (0.2, 0.4, 0.6, 0.8, 1.0)})
        train_scores, val_scores = [], []
        for size in train_sizes:
            try:
                model.fit(X_fit[:size], y_fit[:size])
                train_scores.append(float(scorer(y_fit[:size], model.predict(X_fit[:size]))))
                val_scores.append(float(scorer(y_val, model.predict(X_val))))
            except Exception as exc:  # e.g. a classification subset that holds a single class
                if size == n_train:
                    raise
                self.logger.debug(f"learning-curve point {size} skipped: {exc}")
                train_scores.append(None)
                val_scores.append(None)

        # Overfitting detection: the last learning-curve point is the fit on the whole training part
        final_train_score = train_scores[-1]
        final_val_score = val_scores[-1]
        overfitting_score = final_train_score - final_val_score
        is_overfitting = bool(overfitting_score > overfitting_threshold)

        return self._create_result_dict(
            value={
                'cv_scores': cv_scores.tolist(),
                'train_scores': train_scores,
                'val_scores': val_scores,
                'train_sizes': train_sizes
            },
            method='model_evaluation',
            parameters={
                'model_type': model_type,
                'cv_folds': cv_folds,
                'problem_type': problem_type
            },
            metadata={
                'model': self._estimator_name(model),
                'scoring': scoring,
                'cv_method': 'time_series_split',
                'mean_cv_score': np.mean(cv_scores),
                'std_cv_score': np.std(cv_scores),
                'overfitting_score': overfitting_score,
                'overfitting_threshold': overfitting_threshold,
                'is_overfitting': is_overfitting,
                'final_train_score': final_train_score,
                'final_val_score': final_val_score,
                'train_size': n_train,
                'holdout_size': n_holdout
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def feature_engineering_analysis(self,
                                     data: Union[pd.DataFrame, np.ndarray],
                                     target_column: Optional[str] = None,
                                     text_columns: Optional[List[str]] = None) -> Dict[str, Any]:
        """Feature engineering including selection and text processing."""
        if isinstance(data, np.ndarray):
            data = pd.DataFrame(data)

        results = {}

        # Feature selection (if target provided)
        if target_column and target_column in data.columns:
            from sklearn.feature_selection import SelectKBest, f_regression, mutual_info_regression

            X = data.drop(columns=[target_column]).select_dtypes(include=[np.number])
            y = data[target_column]

            # Select top features
            selector = SelectKBest(score_func=f_regression, k=min(10, X.shape[1]))
            X_selected = selector.fit_transform(X, y)

            selected_features = X.columns[selector.get_support()].tolist()
            feature_scores = selector.scores_.tolist()

            results['feature_selection'] = {
                'selected_features': selected_features,
                'feature_scores': feature_scores,
                'n_features_selected': len(selected_features)
            }

        # Text processing (basic)
        if text_columns:
            text_stats = {}
            for col in text_columns:
                if col in data.columns:
                    text_data = data[col].astype(str)
                    text_stats[col] = {
                        'avg_length': text_data.str.len().mean(),
                        'total_unique': text_data.nunique(),
                        'most_common': text_data.value_counts().head().to_dict()
                    }

            results['text_analysis'] = text_stats

        # Missing value analysis
        missing_stats = {
            'missing_by_column': data.isnull().sum().to_dict(),
            'total_missing': data.isnull().sum().sum(),
            'missing_percentage': (data.isnull().sum() / len(data) * 100).to_dict()
        }

        results['missing_value_analysis'] = missing_stats

        # Correlation analysis for numeric features
        numeric_data = data.select_dtypes(include=[np.number])
        if not numeric_data.empty:
            correlation_matrix = numeric_data.corr()

            # High correlation pairs
            high_corr_pairs = []
            for i in range(len(correlation_matrix.columns)):
                for j in range(i + 1, len(correlation_matrix.columns)):
                    corr_val = correlation_matrix.iloc[i, j]
                    if abs(corr_val) > 0.8:
                        high_corr_pairs.append({
                            'feature1': correlation_matrix.columns[i],
                            'feature2': correlation_matrix.columns[j],
                            'correlation': corr_val
                        })

            results['correlation_analysis'] = {
                'correlation_matrix': correlation_matrix.to_dict(),
                'high_correlation_pairs': high_corr_pairs
            }

        return self._create_result_dict(
            value=results,
            method='feature_engineering',
            parameters={
                'data_shape': data.shape,
                'target_column': target_column,
                'text_columns': text_columns
            },
            metadata={
                'n_numeric_features': len(data.select_dtypes(include=[np.number]).columns),
                'n_categorical_features': len(data.select_dtypes(include=['object']).columns),
                'data_quality_score': max(0, 100 - (missing_stats['total_missing'] / data.size * 100))
            }
        )

    # ========================================
    # SAMPLING METHODS
    # ========================================

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def analyze_sampling_techniques(self,
                                    population: Union[List, np.ndarray, pd.DataFrame],
                                    sample_size: int,
                                    sampling_methods: List[str] = ['simple', 'stratified', 'systematic']) -> Dict[
        str, Any]:
        """Demonstrate various sampling techniques."""
        if isinstance(population, pd.DataFrame):
            population_data = population
        else:
            population = self._validate_numeric_input(population, "population")
            population_data = pd.DataFrame({'value': population})

        sample_size = int(self._validate_positive(sample_size, "sample_size"))

        if sample_size >= len(population_data):
            raise DataValidationError("Sample size must be less than population size", "sample_size")

        results = {}

        for method in sampling_methods:
            try:
                if method == 'simple':
                    # Simple random sampling
                    sample_indices = np.random.choice(len(population_data), size=sample_size, replace=False)
                    sample = population_data.iloc[sample_indices]

                elif method == 'stratified':
                    # Stratified sampling (using first column for stratification)
                    if len(population_data.columns) > 1:
                        strata_col = population_data.columns[1]
                    else:
                        # Create artificial strata based on quantiles
                        strata_col = 'artificial_strata'
                        population_data[strata_col] = pd.qcut(population_data.iloc[:, 0], q=3,
                                                              labels=['low', 'medium', 'high'])

                    sample_list = []
                    strata_counts = population_data[strata_col].value_counts()

                    for stratum in strata_counts.index:
                        stratum_size = len(population_data[population_data[strata_col] == stratum])
                        stratum_sample_size = max(1, int(sample_size * stratum_size / len(population_data)))

                        stratum_data = population_data[population_data[strata_col] == stratum]
                        if len(stratum_data) >= stratum_sample_size:
                            stratum_sample = stratum_data.sample(n=stratum_sample_size, random_state=self.random_seed)
                            sample_list.append(stratum_sample)

                    sample = pd.concat(sample_list) if sample_list else population_data.sample(n=sample_size,
                                                                                               random_state=self.random_seed)

                elif method == 'systematic':
                    # Systematic sampling
                    k = len(population_data) // sample_size
                    start = np.random.randint(0, k)
                    indices = [start + i * k for i in range(sample_size) if start + i * k < len(population_data)]
                    sample = population_data.iloc[indices]

                elif method == 'cluster':
                    # Cluster sampling (simplified)
                    n_clusters = max(2, sample_size // 5)
                    cluster_size = len(population_data) // n_clusters

                    # Create clusters
                    clusters = [population_data.iloc[i:i + cluster_size] for i in
                                range(0, len(population_data), cluster_size)]

                    # Randomly select clusters
                    n_select_clusters = max(1, sample_size // cluster_size)
                    selected_clusters = np.random.choice(len(clusters), size=min(n_select_clusters, len(clusters)),
                                                         replace=False)

                    sample_list = [clusters[i] for i in selected_clusters]
                    sample = pd.concat(sample_list)[:sample_size]

                else:
                    raise DataValidationError(
                        f"Unknown sampling method '{method}' (available: simple, stratified, systematic, cluster)",
                        "sampling_methods")

                # Calculate sample statistics
                if len(sample) > 0:
                    numeric_cols = sample.select_dtypes(include=[np.number]).columns
                    if len(numeric_cols) > 0:
                        sample_mean = sample[numeric_cols].mean().to_dict()
                        sample_std = sample[numeric_cols].std().to_dict()

                        # Compare with population
                        pop_mean = population_data[numeric_cols].mean().to_dict()
                        pop_std = population_data[numeric_cols].std().to_dict()

                        results[method] = {
                            'sample_size': len(sample),
                            'sample_mean': sample_mean,
                            'sample_std': sample_std,
                            'population_mean': pop_mean,
                            'population_std': pop_std,
                            'bias': {col: sample_mean[col] - pop_mean[col] for col in sample_mean.keys()}
                        }
                    else:
                        results[method] = {'sample_size': len(sample), 'note': 'No numeric columns for statistics'}

            except Exception as e:
                results[method] = {'error': str(e)}

        return self._create_result_dict(
            value=results,
            method='sampling_techniques',
            parameters={
                'population_size': len(population_data),
                'sample_size': sample_size,
                'sampling_methods': sampling_methods
            },
            metadata={
                'population_shape': population_data.shape,
                'sampling_fraction': sample_size / len(population_data)
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def demonstrate_central_limit_theorem(self,
                                          population_dist: str = 'exponential',
                                          population_params: Dict[str, float] = None,
                                          sample_sizes: List[int] = [5, 10, 30, 100],
                                          n_samples: int = 1000) -> Dict[str, Any]:
        """Demonstrate Central Limit Theorem with various distributions."""
        if population_params is None:
            population_params = {'scale': 1.0}

        n_samples = int(self._validate_positive(n_samples, "n_samples"))
        sample_sizes = self._validate_sample_sizes(sample_sizes)

        # Generate population
        if population_dist == 'exponential':
            population = np.random.exponential(scale=population_params.get('scale', 1.0), size=10000)
        elif population_dist == 'uniform':
            low = population_params.get('low', 0)
            high = population_params.get('high', 1)
            population = np.random.uniform(low=low, high=high, size=10000)
        elif population_dist == 'binomial':
            n = population_params.get('n', 10)
            p = population_params.get('p', 0.5)
            population = np.random.binomial(n=n, p=p, size=10000)
        elif population_dist == 'poisson':
            # Offered by the panel; it silently ran on a NORMAL population.
            population = np.random.poisson(lam=population_params.get('lam', 3.0), size=10000)
        elif population_dist == 'normal':
            population = np.random.normal(loc=population_params.get('loc', 0),
                                          scale=population_params.get('scale', 1), size=10000)
        else:
            # An unknown name used to fall through to a normal population under the requested label.
            raise DataValidationError(
                f"Unknown population_dist '{population_dist}' (available: exponential, uniform, binomial, "
                f"poisson, normal)", "population_dist")

        population_mean = np.mean(population)
        population_std = np.std(population)

        results = {}

        for sample_size in sample_sizes:
            sample_means = []

            for _ in range(n_samples):
                sample = np.random.choice(population, size=sample_size, replace=True)
                sample_means.append(np.mean(sample))

            sample_means = np.array(sample_means)

            # Test normality of sample means
            _, shapiro_p = stats.shapiro(sample_means) if len(sample_means) <= 5000 else (None, None)

            # Theoretical vs empirical
            theoretical_mean = population_mean
            theoretical_std = population_std / np.sqrt(sample_size)

            empirical_mean = np.mean(sample_means)
            empirical_std = np.std(sample_means)

            results[f'n_{sample_size}'] = {
                'sample_means': sample_means.tolist(),
                'empirical_mean': empirical_mean,
                'empirical_std': empirical_std,
                'theoretical_mean': theoretical_mean,
                'theoretical_std': theoretical_std,
                'mean_bias': empirical_mean - theoretical_mean,
                'std_bias': empirical_std - theoretical_std,
                'shapiro_p_value': shapiro_p,
                'appears_normal': shapiro_p > 0.05 if shapiro_p is not None else None
            }

        return self._create_result_dict(
            value=results,
            method='central_limit_theorem',
            parameters={
                'population_dist': population_dist,
                'population_params': population_params,
                'sample_sizes': sample_sizes,
                'n_samples': n_samples
            },
            metadata={
                'population_mean': population_mean,
                'population_std': population_std,
                'population_size': len(population),
                'clt_convergence': {str(n): results[f'n_{n}']['appears_normal'] for n in sample_sizes if
                                    f'n_{n}' in results}
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def advanced_resampling_analysis(self,
                                     data: Union[List, np.ndarray, pd.Series],
                                     statistic_func: Callable = np.mean,
                                     methods: List[str] = ['bootstrap', 'jackknife', 'permutation'],
                                     n_resamples: int = 1000) -> Dict[str, Any]:
        """Advanced resampling methods for statistical inference."""
        data = self._validate_returns(data, "data")
        self._check_data_length(data, min_length=5)

        n_resamples = int(self._validate_positive(n_resamples, "n_resamples"))

        if not callable(statistic_func):
            raise DataValidationError("statistic_func must be callable", "statistic_func")

        # Original statistic
        original_stat = statistic_func(data)

        results = {}

        for method in methods:
            try:
                if method == 'bootstrap':
                    # Bootstrap resampling
                    bootstrap_stats = []
                    for _ in range(n_resamples):
                        bootstrap_sample = np.random.choice(data, size=len(data), replace=True)
                        bootstrap_stats.append(statistic_func(bootstrap_sample))

                    bootstrap_stats = np.array(bootstrap_stats)

                    # Confidence interval
                    ci_lower = np.percentile(bootstrap_stats, 2.5)
                    ci_upper = np.percentile(bootstrap_stats, 97.5)

                    results['bootstrap'] = {
                        'resampled_statistics': bootstrap_stats.tolist(),
                        'mean': np.mean(bootstrap_stats),
                        'std': np.std(bootstrap_stats),
                        'bias': np.mean(bootstrap_stats) - original_stat,
                        'confidence_interval': [ci_lower, ci_upper]
                    }

                elif method == 'jackknife':
                    # Jackknife resampling
                    n = len(data)
                    jackknife_stats = []

                    for i in range(n):
                        jackknife_sample = np.concatenate([data[:i], data[i + 1:]])
                        jackknife_stats.append(statistic_func(jackknife_sample))

                    jackknife_stats = np.array(jackknife_stats)

                    # Bias correction
                    jackknife_mean = np.mean(jackknife_stats)
                    bias = (n - 1) * (jackknife_mean - original_stat)
                    bias_corrected = original_stat - bias

                    # Variance estimate
                    jackknife_var = (n - 1) / n * np.sum((jackknife_stats - jackknife_mean) ** 2)

                    results['jackknife'] = {
                        'resampled_statistics': jackknife_stats.tolist(),
                        'bias': bias,
                        'bias_corrected_estimate': bias_corrected,
                        'variance_estimate': jackknife_var,
                        'std_error': np.sqrt(jackknife_var)
                    }

                elif method == 'permutation':
                    # Permutation test (for hypothesis testing)
                    # Test if mean differs from zero
                    permutation_stats = []

                    for _ in range(n_resamples):
                        # Random sign flips
                        signs = np.random.choice([-1, 1], size=len(data))
                        permuted_data = data * signs
                        permutation_stats.append(statistic_func(permuted_data))

                    permutation_stats = np.array(permutation_stats)

                    # P-value (two-tailed test)
                    p_value = np.mean(np.abs(permutation_stats) >= np.abs(original_stat))

                    results['permutation'] = {
                        'resampled_statistics': permutation_stats.tolist(),
                        'p_value': p_value,
                        'significant': bool(p_value < 0.05)
                    }

                else:
                    raise DataValidationError(
                        f"Unknown resampling method '{method}' (available: bootstrap, jackknife, permutation)",
                        "methods")

            except Exception as e:
                results[method] = {'error': str(e)}

        return self._create_result_dict(
            value=results,
            method='advanced_resampling',
            parameters={
                'data': data.tolist() if hasattr(data, 'tolist') else list(data),
                'methods': methods,
                'n_resamples': n_resamples
            },
            metadata={
                'original_statistic': original_stat,
                'data_size': len(data),
                'statistic_name': getattr(statistic_func, '__name__', 'custom_function')
            }
        )

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def calculate_sampling_error_analysis(self,
                                          population_mean: float,
                                          population_std: float,
                                          sample_sizes: List[int] = [10, 30, 50, 100, 500],
                                          confidence_level: float = 0.95,
                                          n_simulations: int = 1000) -> Dict[str, Any]:
        """Analyze sampling error and determine optimal sample sizes."""
        population_mean = self._validate_numeric_input(population_mean, "population_mean")
        population_std = self._validate_positive(population_std, "population_std")
        confidence_level = self._validate_probability(confidence_level, "confidence_level")
        n_simulations = int(self._validate_positive(n_simulations, "n_simulations"))
        sample_sizes = self._validate_sample_sizes(sample_sizes)
        if not 0 < confidence_level < 1:
            raise DataValidationError(f"confidence_level must be strictly between 0 and 1, got {confidence_level}",
                                      "confidence_level")

        alpha = 1 - confidence_level
        z_critical = stats.norm.ppf(1 - alpha / 2)

        results = {}

        for sample_size in sample_sizes:
            # Theoretical calculations
            standard_error = population_std / np.sqrt(sample_size)
            margin_of_error = z_critical * standard_error

            # Simulation
            sample_means = []
            for _ in range(n_simulations):
                sample = np.random.normal(population_mean, population_std, size=sample_size)
                sample_means.append(np.mean(sample))

            sample_means = np.array(sample_means)

            # Calculate coverage probability
            ci_lower = sample_means - margin_of_error
            ci_upper = sample_means + margin_of_error
            coverage = np.mean((ci_lower <= population_mean) & (population_mean <= ci_upper))

            # Mean squared error
            mse = np.mean((sample_means - population_mean) ** 2)

            results[f'n_{sample_size}'] = {
                'sample_size': sample_size,
                'theoretical_se': standard_error,
                'empirical_se': np.std(sample_means),
                'theoretical_moe': margin_of_error,
                'empirical_moe': z_critical * np.std(sample_means),
                'coverage_probability': coverage,
                'mean_squared_error': mse,
                # SE relative to the mean is undefined for a zero mean
                'relative_error': standard_error / abs(population_mean) if population_mean != 0 else None
            }

        # Sample size recommendation
        target_moe = 0.1 * population_std  # 10% of population std as target
        recommended_n = int(np.ceil((z_critical * population_std / target_moe) ** 2))

        return self._create_result_dict(
            value=results,
            method='sampling_error_analysis',
            parameters={
                'population_mean': population_mean,
                'population_std': population_std,
                'sample_sizes': sample_sizes,
                'confidence_level': confidence_level,
                'n_simulations': n_simulations
            },
            metadata={
                'recommended_sample_size': recommended_n,
                'target_margin_of_error': target_moe,
                'z_critical': z_critical,
                'coverage_target': confidence_level
            }
        )

    @staticmethod
    def _validate_sample_sizes(sample_sizes) -> List[int]:
        """A non-empty list of positive integer sample sizes."""
        if sample_sizes is None or isinstance(sample_sizes, (str, bytes)) or len(sample_sizes) == 0:
            raise DataValidationError("sample_sizes must be a non-empty list of positive integers", "sample_sizes")
        sizes = []
        for s in sample_sizes:
            try:
                v = int(s)
            except (TypeError, ValueError):
                raise DataValidationError(f"sample size {s!r} is not an integer", "sample_sizes")
            if v < 1 or v != float(s):
                raise DataValidationError(f"sample sizes must be positive integers, got {s!r}", "sample_sizes")
            sizes.append(v)
        return sizes

    # ========================================
    # MAIN CALCULATION DISPATCHER
    # ========================================

    def calculate(self, calculation_type: str, **kwargs) -> Dict[str, Any]:
        """Main calculation dispatcher for all advanced methods."""
        methods = {
            # Time Series
            'trend_analysis': self.analyze_trend,
            'stationarity_test': self.test_stationarity,
            'arima_model': self.fit_arima_model,
            'forecasting': self.generate_forecasts,

            # Machine Learning
            'supervised_learning': self.supervised_learning_analysis,
            'unsupervised_learning': self.unsupervised_learning_analysis,
            'model_evaluation': self.evaluate_model_performance,
            'feature_engineering': self.feature_engineering_analysis,

            # Sampling Methods
            'sampling_techniques': self.analyze_sampling_techniques,
            'central_limit_theorem': self.demonstrate_central_limit_theorem,
            'resampling_methods': self.advanced_resampling_analysis,
            'sampling_error_analysis': self.calculate_sampling_error_analysis
        }

        if calculation_type not in methods:
            raise DataValidationError(f"Unknown calculation type: {calculation_type}", "calculation_type")

        return methods[calculation_type](**kwargs)


# ========================================
# UTILITY FUNCTIONS FOR QUICK ACCESS
# ========================================

# Time Series Utilities
def detect_trend(data: Union[List, np.ndarray], trend_type: str = 'linear') -> Dict[str, float]:
    """Quick trend detection."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.analyze_trend(data, trend_type)
    return result['value']


def test_stationarity_quick(data: Union[List, np.ndarray], test: str = 'adf') -> bool:
    """Quick stationarity test."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.test_stationarity(data, test)
    return result['value']['is_stationary']


def forecast_series(data: Union[List, np.ndarray], horizon: int = 5, method: str = 'simple_exponential') -> List[float]:
    """Quick forecasting."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.generate_forecasts(data, horizon, method)
    return result['value']


# Machine Learning Utilities
def quick_ml_comparison(X: np.ndarray, y: np.ndarray, problem_type: str = 'regression') -> str:
    """Compare ML algorithms and return best performer."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.supervised_learning_analysis(X, y, problem_type)
    return result['metadata']['best_algorithm']


def detect_overfitting(X: np.ndarray, y: np.ndarray, model_type: str = 'random_forest') -> bool:
    """Quick overfitting detection."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.evaluate_model_performance(X, y, model_type)
    return result['metadata']['is_overfitting']


def optimal_clusters(X: np.ndarray) -> int:
    """Find optimal number of clusters."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.unsupervised_learning_analysis(X, ['kmeans'])
    return result['value']['kmeans']['optimal_k']


# Sampling Utilities
def compare_sampling_methods(population: np.ndarray, sample_size: int) -> Dict[str, float]:
    """Compare different sampling methods."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.analyze_sampling_techniques(population, sample_size)
    return {method: data.get('bias', {}).get('value', 0) for method, data in result['value'].items() if
            isinstance(data, dict)}


def required_sample_size(population_std: float, margin_of_error: float, confidence: float = 0.95) -> int:
    """Calculate required sample size."""
    z_critical = stats.norm.ppf(1 - (1 - confidence) / 2)
    return int(np.ceil((z_critical * population_std / margin_of_error) ** 2))


def clt_convergence_check(dist_type: str = 'exponential', sample_sizes: List[int] = [5, 30, 100]) -> Dict[int, bool]:
    """Check CLT convergence for different sample sizes."""
    analyzer = AdvancedQuantAnalyzer()
    result = analyzer.demonstrate_central_limit_theorem(dist_type, sample_sizes=sample_sizes)
    return result['metadata']['clt_convergence']