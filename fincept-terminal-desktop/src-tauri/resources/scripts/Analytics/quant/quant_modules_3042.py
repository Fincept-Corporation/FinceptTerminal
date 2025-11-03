
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
from sklearn.model_selection import train_test_split, cross_val_score, GridSearchCV
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.linear_model import Ridge, Lasso, ElasticNet
from sklearn.svm import SVC, SVR
from sklearn.neighbors import KNeighborsClassifier, KNeighborsRegressor
from sklearn.tree import DecisionTreeClassifier, DecisionTreeRegressor
from sklearn.ensemble import RandomForestClassifier, RandomForestRegressor
from sklearn.cluster import KMeans, AgglomerativeClustering
from sklearn.decomposition import PCA
from sklearn.metrics import classification_report, confusion_matrix, mean_squared_error, r2_score
import warnings

from .base_calculator import BaseCalculator, validate_inputs, timing_decorator
from .exceptions import handle_calculation_error, DataValidationError


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

        if test_type not in ['adf', 'kpss', 'pp']:
            raise DataValidationError(f"Invalid test_type: {test_type}", "test_type")

        results = {}

        if test_type == 'adf':
            # Augmented Dickey-Fuller test
            from statsmodels.tsa.stattools import adfuller
            adf_result = adfuller(data, autolag='AIC')

            results = {
                'test_statistic': adf_result[0],
                'p_value': adf_result[1],
                'critical_values': adf_result[4],
                'is_stationary': adf_result[1] < 0.05,
                'null_hypothesis': 'Unit root (non-stationary)'
            }

        elif test_type == 'kpss':
            # KPSS test
            from statsmodels.tsa.stattools import kpss
            kpss_result = kpss(data, regression='c')

            results = {
                'test_statistic': kpss_result[0],
                'p_value': kpss_result[1],
                'critical_values': kpss_result[3],
                'is_stationary': kpss_result[1] > 0.05,
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
        self._check_data_length(data, min_length=max(20, sum(order) * 3))

        try:
            from statsmodels.tsa.arima.model import ARIMA

            # Fit ARIMA model
            model = ARIMA(data, order=order, seasonal_order=seasonal_order)
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
                    'parameters': fitted_model.params.to_dict(),
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

    @validate_inputs
    @timing_decorator
    @handle_calculation_error
    def generate_forecasts(self,
                           data: Union[List, np.ndarray, pd.Series],
                           horizon: int = 5,
                           method: str = 'simple_exponential',
                           train_size: float = 0.8) -> Dict[str, Any]:
        """Generate forecasts using various methods."""
        data = self._validate_returns(data, "data")
        self._check_data_length(data, min_length=20)

        horizon = int(self._validate_positive(horizon, "horizon"))
        train_size = self._validate_probability(train_size, "train_size")

        # Split data
        split_point = int(len(data) * train_size)
        train_data = data[:split_point]
        test_data = data[split_point:]

        forecasts = {}

        if method == 'simple_exponential':
            # Simple exponential smoothing
            alpha = 0.3  # Smoothing parameter
            smoothed = [train_data[0]]

            for i in range(1, len(train_data)):
                smoothed.append(alpha * train_data[i] + (1 - alpha) * smoothed[-1])

            # Forecast
            forecast_values = [smoothed[-1]] * horizon

        elif method == 'linear_trend':
            # Linear trend forecasting
            time_index = np.arange(len(train_data))
            X = np.column_stack([np.ones(len(time_index)), time_index])
            coefficients = np.linalg.lstsq(X, train_data, rcond=None)[0]

            intercept, slope = coefficients
            future_time = np.arange(len(train_data), len(train_data) + horizon)
            forecast_values = intercept + slope * future_time

        elif method == 'moving_average':
            # Moving average
            window = min(5, len(train_data) // 4)
            last_values = train_data[-window:]
            forecast_values = [np.mean(last_values)] * horizon

        else:
            raise DataValidationError(f"Unknown forecasting method: {method}", "method")

        # Calculate accuracy metrics if test data available
        if len(test_data) > 0:
            n_compare = min(len(test_data), horizon)
            mae = np.mean(np.abs(test_data[:n_compare] - forecast_values[:n_compare]))
            rmse = np.sqrt(np.mean((test_data[:n_compare] - forecast_values[:n_compare]) ** 2))
            mape = np.mean(np.abs((test_data[:n_compare] - forecast_values[:n_compare]) / test_data[:n_compare])) * 100
        else:
            mae = rmse = mape = None

        return self._create_result_dict(
            value=forecast_values,
            method=f'forecast_{method}',
            parameters={
                'data': data.tolist() if hasattr(data, 'tolist') else list(data),
                'horizon': horizon,
                'method': method,
                'train_size': train_size
            },
            metadata={
                'train_data_size': len(train_data),
                'test_data_size': len(test_data),
                'mae': mae,
                'rmse': rmse,
                'mape': mape,
                'forecast_horizon': horizon
            }
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

        # Split data
        X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=self.random_seed)

        # Scale features
        scaler = StandardScaler()
        X_train_scaled = scaler.fit_transform(X_train)
        X_test_scaled = scaler.transform(X_test)

        results = {}

        for algorithm in algorithms:
            try:
                if problem_type == 'regression':
                    model = self._get_regression_model(algorithm)
                else:
                    model = self._get_classification_model(algorithm)

                # Train model
                if algorithm in ['svm', 'knn']:
                    model.fit(X_train_scaled, y_train)
                    y_pred = model.predict(X_test_scaled)
                else:
                    model.fit(X_train, y_train)
                    y_pred = model.predict(X_test)

                # Calculate metrics
                if problem_type == 'regression':
                    mse = mean_squared_error(y_test, y_pred)
                    r2 = r2_score(y_test, y_pred)
                    results[algorithm] = {
                        'mse': mse,
                        'rmse': np.sqrt(mse),
                        'r2_score': r2,
                        'predictions': y_pred.tolist()
                    }
                else:
                    from sklearn.metrics import accuracy_score, precision_score, recall_score
                    accuracy = accuracy_score(y_test, y_pred)
                    precision = precision_score(y_test, y_pred, average='weighted')
                    recall = recall_score(y_test, y_pred, average='weighted')

                    results[algorithm] = {
                        'accuracy': accuracy,
                        'precision': precision,
                        'recall': recall,
                        'predictions': y_pred.tolist()
                    }

            except Exception as e:
                results[algorithm] = {'error': str(e)}

        return self._create_result_dict(
            value=results,
            method=f'supervised_learning_{problem_type}',
            parameters={
                'X_shape': X.shape,
                'y_shape': y.shape,
                'problem_type': problem_type,
                'algorithms': algorithms
            },
            metadata={
                'train_size': len(X_train),
                'test_size': len(X_test),
                'n_features': X.shape[1],
                'best_algorithm': max(results.keys(), key=lambda k: results[k].get(
                    'r2_score' if problem_type == 'regression' else 'accuracy', -np.inf)) if results else None
            }
        )

    def _get_regression_model(self, algorithm: str):
        """Get regression model by algorithm name."""
        models = {
            'ridge': Ridge(alpha=1.0, random_state=self.random_seed),
            'lasso': Lasso(alpha=1.0, random_state=self.random_seed),
            'elastic_net': ElasticNet(alpha=1.0, random_state=self.random_seed),
            'svm': SVR(kernel='rbf'),
            'knn': KNeighborsRegressor(n_neighbors=5),
            'decision_tree': DecisionTreeRegressor(random_state=self.random_seed),
            'random_forest': RandomForestRegressor(n_estimators=100, random_state=self.random_seed)
        }
        return models.get(algorithm, Ridge(random_state=self.random_seed))

    def _get_classification_model(self, algorithm: str):
        """Get classification model by algorithm name."""
        models = {
            'svm': SVC(kernel='rbf', random_state=self.random_seed),
            'knn': KNeighborsClassifier(n_neighbors=5),
            'decision_tree': DecisionTreeClassifier(random_state=self.random_seed),
            'random_forest': RandomForestClassifier(n_estimators=100, random_state=self.random_seed)
        }
        return models.get(algorithm, RandomForestClassifier(random_state=self.random_seed))

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

        # Scale data
        scaler = StandardScaler()
        X_scaled = scaler.fit_transform(X)

        results = {}

        for method in methods:
            try:
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

                    for k in k_range:
                        kmeans = KMeans(n_clusters=k, random_state=self.random_seed)
                        cluster_labels = kmeans.fit_predict(X_scaled)
                        inertias.append(kmeans.inertia_)

                        # Silhouette score
                        from sklearn.metrics import silhouette_score
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
                                   cv_folds: int = 5) -> Dict[str, Any]:
        """Comprehensive model evaluation with cross-validation."""
        if isinstance(X, pd.DataFrame):
            X = X.values
        if isinstance(y, pd.Series):
            y = y.values

        # Determine problem type
        problem_type = 'classification' if len(np.unique(y)) < 10 else 'regression'

        # Get model
        if problem_type == 'regression':
            model = self._get_regression_model(model_type)
            scoring = 'r2'
        else:
            model = self._get_classification_model(model_type)
            scoring = 'accuracy'

        # Cross-validation
        cv_scores = cross_val_score(model, X, y, cv=cv_folds, scoring=scoring)

        # Learning curves (simplified)
        train_sizes = np.linspace(0.1, 1.0, 5)
        train_scores = []
        val_scores = []

        for train_size in train_sizes:
            n_samples = int(len(X) * train_size)
            X_subset = X[:n_samples]
            y_subset = y[:n_samples]

            # Train model
            model.fit(X_subset, y_subset)

            # Training score
            train_pred = model.predict(X_subset)
            if problem_type == 'regression':
                train_score = r2_score(y_subset, train_pred)
            else:
                from sklearn.metrics import accuracy_score
                train_score = accuracy_score(y_subset, train_pred)

            train_scores.append(train_score)

            # Validation score (using remaining data)
            if n_samples < len(X):
                X_val = X[n_samples:]
                y_val = y[n_samples:]
                val_pred = model.predict(X_val)

                if problem_type == 'regression':
                    val_score = r2_score(y_val, val_pred)
                else:
                    val_score = accuracy_score(y_val, val_pred)
            else:
                val_score = train_score

            val_scores.append(val_score)

        # Overfitting detection
        final_train_score = train_scores[-1]
        final_val_score = val_scores[-1]
        overfitting_score = final_train_score - final_val_score
        is_overfitting = overfitting_score > 0.1

        return self._create_result_dict(
            value={
                'cv_scores': cv_scores.tolist(),
                'train_scores': train_scores,
                'val_scores': val_scores
            },
            method='model_evaluation',
            parameters={
                'model_type': model_type,
                'cv_folds': cv_folds,
                'problem_type': problem_type
            },
            metadata={
                'mean_cv_score': np.mean(cv_scores),
                'std_cv_score': np.std(cv_scores),
                'overfitting_score': overfitting_score,
                'is_overfitting': is_overfitting,
                'final_train_score': final_train_score,
                'final_val_score': final_val_score
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
                    continue

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
        else:
            # Default to normal
            population = np.random.normal(loc=population_params.get('loc', 0),
                                          scale=population_params.get('scale', 1), size=10000)

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
                        'significant': p_value < 0.05
                    }

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
                'relative_error': standard_error / population_mean if population_mean != 0 else np.inf
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