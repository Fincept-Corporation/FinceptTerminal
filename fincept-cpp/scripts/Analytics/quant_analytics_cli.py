"""
CFA Quant Analytics CLI - Unified Command Line Interface
Provides a unified entry point for CFA-level quantitative analytics in Fincept Terminal.

Categories:
- Time Series: Trend analysis, stationarity tests, ARIMA, forecasting
- Machine Learning: Supervised/unsupervised learning, model evaluation
- Sampling: Bootstrap, jackknife, CLT, sampling error analysis
- Data Validation: Quality checks, outlier detection, missing data analysis

Usage:
    python quant_analytics_cli.py <command> [params_json]

Commands:
    list                    - List all available analyses
    trend_analysis          - Analyze linear/log-linear trends
    stationarity_test       - Test for stationarity (ADF, KPSS)
    arima_model            - Fit ARIMA model
    forecasting            - Generate forecasts
    supervised_learning    - Run supervised ML analysis
    unsupervised_learning  - Run unsupervised ML analysis
    model_evaluation       - Evaluate model performance
    feature_engineering    - Analyze features
    sampling_techniques    - Compare sampling methods
    central_limit_theorem  - Demonstrate CLT
    resampling_methods     - Bootstrap/jackknife analysis
    sampling_error_analysis - Analyze sampling errors
    validate_data          - Validate financial data
"""

import sys
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

quant_dir = os.path.join(script_dir, 'quant')
if quant_dir not in sys.path:
    sys.path.insert(0, quant_dir)

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
            return [self._sanitize_value(x) for x in obj.tolist()]
        if isinstance(obj, pd.Series):
            return [self._sanitize_value(x) for x in obj.tolist()]
        if isinstance(obj, pd.DataFrame):
            return obj.to_dict('records')
        if isinstance(obj, (np.int64, np.int32)):
            return int(obj)
        if isinstance(obj, (np.float64, np.float32)):
            return self._sanitize_value(float(obj))
        if isinstance(obj, (np.bool_, bool)):
            return bool(obj)
        if pd.isna(obj):
            return None
        return super().default(obj)

    def _sanitize_value(self, val):
        if isinstance(val, float):
            if np.isnan(val) or np.isinf(val):
                return None
        return val

    def encode(self, obj):
        return super().encode(self._sanitize_obj(obj))

    def _sanitize_obj(self, obj):
        if isinstance(obj, dict):
            return {k: self._sanitize_obj(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [self._sanitize_obj(x) for x in obj]
        elif isinstance(obj, tuple):
            return [self._sanitize_obj(x) for x in obj]
        elif isinstance(obj, np.ndarray):
            return [self._sanitize_obj(x) for x in obj.tolist()]
        elif isinstance(obj, pd.Series):
            return [self._sanitize_obj(x) for x in obj.tolist()]
        elif isinstance(obj, pd.DataFrame):
            records = obj.to_dict('records')
            return [self._sanitize_obj(r) for r in records]
        elif isinstance(obj, np.bool_):
            return bool(obj)
        elif isinstance(obj, bool):
            return obj
        elif isinstance(obj, (np.int64, np.int32, np.int16, np.int8)):
            return int(obj)
        elif isinstance(obj, (np.float64, np.float32, np.float16)):
            val = float(obj)
            if np.isnan(val) or np.isinf(val):
                return None
            return val
        elif isinstance(obj, float):
            if np.isnan(obj) or np.isinf(obj):
                return None
            return obj
        elif pd.isna(obj):
            return None
        return obj


class CFAQuantEngine:
    def __init__(self):
        self.analyzer = None
        self.validator = None
        self._load_modules()

    def _load_modules(self):
        try:
            from quant.quant_modules_3042 import AdvancedQuantAnalyzer
            from quant.data_validator import DataValidator
            self.analyzer = AdvancedQuantAnalyzer()
            self.validator = DataValidator()
        except ImportError as e:
            self.import_error = str(e)

    def _convert_numpy_types(self, obj: Any) -> Any:
        """Recursively convert numpy types to Python native types."""
        if isinstance(obj, dict):
            return {k: self._convert_numpy_types(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [self._convert_numpy_types(x) for x in obj]
        elif isinstance(obj, tuple):
            return [self._convert_numpy_types(x) for x in obj]
        elif isinstance(obj, np.ndarray):
            return [self._convert_numpy_types(x) for x in obj.tolist()]
        elif isinstance(obj, np.bool_):
            return bool(obj)
        elif isinstance(obj, bool):
            return obj
        elif isinstance(obj, (np.int64, np.int32, np.int16, np.int8)):
            return int(obj)
        elif isinstance(obj, (np.float64, np.float32, np.float16)):
            val = float(obj)
            if np.isnan(val) or np.isinf(val):
                return None
            return val
        elif isinstance(obj, pd.Series):
            return [self._convert_numpy_types(x) for x in obj.tolist()]
        elif isinstance(obj, pd.DataFrame):
            records = obj.to_dict('records')
            return [self._convert_numpy_types(r) for r in records]
        elif isinstance(obj, float):
            if np.isnan(obj) or np.isinf(obj):
                return None
            return obj
        elif pd.isna(obj):
            return None
        return obj

    def _parse_data(self, data: Union[str, List, Dict]) -> np.ndarray:
        if isinstance(data, str):
            try:
                parsed = json.loads(data)
                if isinstance(parsed, list):
                    return np.array(parsed, dtype=float)
                return np.array(parsed, dtype=float)
            except json.JSONDecodeError:
                values = [float(x.strip()) for x in data.split(',') if x.strip()]
                return np.array(values)
        elif isinstance(data, list):
            return np.array(data, dtype=float)
        elif isinstance(data, dict):
            return pd.DataFrame(data).values
        return np.array(data, dtype=float)

    def list_analyses(self) -> Dict[str, Any]:
        return {
            'success': True,
            'available_analyses': {
                'time_series': {
                    'trend_analysis': 'Analyze linear and log-linear trends',
                    'stationarity_test': 'Test for stationarity (ADF, KPSS)',
                    'arima_model': 'Fit ARIMA model to time series',
                    'forecasting': 'Generate forecasts using various methods'
                },
                'machine_learning': {
                    'supervised_learning': 'Compare supervised ML algorithms',
                    'unsupervised_learning': 'PCA, clustering analysis',
                    'model_evaluation': 'Cross-validation and overfitting detection',
                    'feature_engineering': 'Feature selection and analysis'
                },
                'sampling': {
                    'sampling_techniques': 'Simple, stratified, systematic sampling',
                    'central_limit_theorem': 'CLT demonstration',
                    'resampling_methods': 'Bootstrap, jackknife, permutation',
                    'sampling_error_analysis': 'Sampling error and sample size'
                },
                'data_quality': {
                    'validate_data': 'Validate financial data quality',
                    'validate_returns': 'Validate return series',
                    'validate_prices': 'Validate price data',
                    'validate_rates': 'Validate interest rate data'
                }
            },
            'timestamp': datetime.now().isoformat()
        }

    def trend_analysis(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []))
        trend_type = params.get('trend_type', 'linear')

        result = self.analyzer.analyze_trend(data, trend_type=trend_type)
        return {'success': True, 'analysis_type': 'trend_analysis', 'result': self._convert_numpy_types(result)}

    def stationarity_test(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []))
        test_type = params.get('test_type', 'adf')

        result = self.analyzer.test_stationarity(data, test_type=test_type)
        return {'success': True, 'analysis_type': 'stationarity_test', 'result': self._convert_numpy_types(result)}

    def arima_model(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []))

        # Handle order parameter - ensure it's a tuple
        order_param = params.get('order', [1, 0, 1])
        if order_param is None:
            order = (1, 0, 1)
        elif isinstance(order_param, (list, tuple)):
            order = tuple(order_param)
        else:
            order = (1, 0, 1)

        # Handle seasonal_order parameter
        seasonal_order = params.get('seasonal_order')
        if seasonal_order and isinstance(seasonal_order, (list, tuple)):
            seasonal_order = tuple(seasonal_order)
        else:
            seasonal_order = None

        result = self.analyzer.fit_arima_model(data, order=order, seasonal_order=seasonal_order)
        return {'success': True, 'analysis_type': 'arima_model', 'result': self._convert_numpy_types(result)}

    def forecasting(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []))
        horizon = params.get('horizon', 5)
        method = params.get('method', 'simple_exponential')
        train_size = params.get('train_size', 0.8)

        result = self.analyzer.generate_forecasts(data, horizon=horizon, method=method, train_size=train_size)
        return {'success': True, 'analysis_type': 'forecasting', 'result': self._convert_numpy_types(result)}

    def _create_lag_features(self, data: np.ndarray, n_lags: int = 5) -> tuple:
        """Create lag features from time series data for ML models."""
        if len(data) < n_lags + 2:
            raise ValueError(f"Need at least {n_lags + 2} data points for lag features, got {len(data)}")

        X = []
        y = []
        for i in range(n_lags, len(data)):
            X.append(data[i-n_lags:i])
            y.append(data[i])
        return np.array(X), np.array(y)

    def supervised_learning(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        X = params.get('X')
        y = params.get('y')

        if X is None or y is None:
            data = params.get('data')
            if data is not None:
                data_arr = self._parse_data(data)
                n_lags = params.get('n_lags')
                if n_lags is None or n_lags < 1:
                    n_lags = 5
                try:
                    X, y = self._create_lag_features(data_arr, n_lags=n_lags)
                except ValueError as e:
                    return {'success': False, 'error': str(e)}
            else:
                return {'success': False, 'error': 'Either provide X/y arrays or a data time series'}
        else:
            X = np.array(X)
            y = np.array(y)

        problem_type = params.get('problem_type', 'regression')
        algorithms = params.get('algorithms', ['ridge', 'random_forest'])

        result = self.analyzer.supervised_learning_analysis(X, y, problem_type=problem_type, algorithms=algorithms)
        return {'success': True, 'analysis_type': 'supervised_learning', 'result': self._convert_numpy_types(result)}

    def unsupervised_learning(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        X = params.get('X')

        if X is None:
            data = params.get('data')
            if data is not None:
                data_arr = self._parse_data(data)
                n_lags = params.get('n_lags')
                if n_lags is None or n_lags < 1:
                    n_lags = 5
                try:
                    X, _ = self._create_lag_features(data_arr, n_lags=n_lags)
                except ValueError as e:
                    return {'success': False, 'error': str(e)}
            else:
                return {'success': False, 'error': 'Either provide X array or a data time series'}
        else:
            X = np.array(X)

        methods = params.get('methods', ['pca', 'kmeans'])

        result = self.analyzer.unsupervised_learning_analysis(X, methods=methods)
        return {'success': True, 'analysis_type': 'unsupervised_learning', 'result': self._convert_numpy_types(result)}

    def model_evaluation(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        X = params.get('X')
        y = params.get('y')

        if X is None or y is None:
            data = params.get('data')
            if data is not None:
                data_arr = self._parse_data(data)
                n_lags = params.get('n_lags')
                if n_lags is None or n_lags < 1:
                    n_lags = 5
                try:
                    X, y = self._create_lag_features(data_arr, n_lags=n_lags)
                except ValueError as e:
                    return {'success': False, 'error': str(e)}
            else:
                return {'success': False, 'error': 'Either provide X/y arrays or a data time series'}
        else:
            X = np.array(X)
            y = np.array(y)

        model_type = params.get('model_type', 'random_forest')
        cv_folds = params.get('cv_folds', 5)

        result = self.analyzer.evaluate_model_performance(X, y, model_type=model_type, cv_folds=cv_folds)
        return {'success': True, 'analysis_type': 'model_evaluation', 'result': self._convert_numpy_types(result)}

    def feature_engineering(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = pd.DataFrame(params.get('data', {}))
        target_column = params.get('target_column')
        text_columns = params.get('text_columns')

        result = self.analyzer.feature_engineering_analysis(data, target_column=target_column, text_columns=text_columns)
        return {'success': True, 'analysis_type': 'feature_engineering', 'result': self._convert_numpy_types(result)}

    def sampling_techniques(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        population = self._parse_data(params.get('population', []))
        sample_size = params.get('sample_size', 100)
        sampling_methods = params.get('sampling_methods', ['simple', 'stratified', 'systematic'])

        result = self.analyzer.analyze_sampling_techniques(population, sample_size=sample_size, sampling_methods=sampling_methods)
        return {'success': True, 'analysis_type': 'sampling_techniques', 'result': self._convert_numpy_types(result)}

    def central_limit_theorem(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        population_dist = params.get('population_dist', 'exponential')
        population_params = params.get('population_params', {'scale': 1.0})
        sample_sizes = params.get('sample_sizes', [5, 10, 30, 100])
        n_samples = params.get('n_samples', 1000)

        result = self.analyzer.demonstrate_central_limit_theorem(
            population_dist=population_dist,
            population_params=population_params,
            sample_sizes=sample_sizes,
            n_samples=n_samples
        )
        return {'success': True, 'analysis_type': 'central_limit_theorem', 'result': self._convert_numpy_types(result)}

    def resampling_methods(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []))
        methods = params.get('methods', ['bootstrap', 'jackknife', 'permutation'])
        n_resamples = params.get('n_resamples', 1000)

        result = self.analyzer.advanced_resampling_analysis(data, methods=methods, n_resamples=n_resamples)
        return {'success': True, 'analysis_type': 'resampling_methods', 'result': self._convert_numpy_types(result)}

    def sampling_error_analysis(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        population_mean = params.get('population_mean', 0.0)
        population_std = params.get('population_std', 1.0)
        sample_sizes = params.get('sample_sizes', [10, 30, 50, 100, 500])
        confidence_level = params.get('confidence_level', 0.95)
        n_simulations = params.get('n_simulations', 1000)

        result = self.analyzer.calculate_sampling_error_analysis(
            population_mean=population_mean,
            population_std=population_std,
            sample_sizes=sample_sizes,
            confidence_level=confidence_level,
            n_simulations=n_simulations
        )
        return {'success': True, 'analysis_type': 'sampling_error_analysis', 'result': self._convert_numpy_types(result)}

    def validate_data(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.validator is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = params.get('data', [])
        if isinstance(data, list):
            data = pd.Series(data)
        elif isinstance(data, dict):
            data = pd.DataFrame(data)

        data_type = params.get('data_type', 'general')
        data_name = params.get('data_name', 'data')

        cleaned_data, report = self.validator.validate_financial_data(data, data_type=data_type, data_name=data_name)

        return {
            'success': True,
            'analysis_type': 'validate_data',
            'result': self._convert_numpy_types(report.to_dict()),
            'data_cleaned': isinstance(cleaned_data, pd.DataFrame) or isinstance(cleaned_data, pd.Series)
        }


def main():
    if len(sys.argv) < 2:
        result = {'success': False, 'error': 'No command provided', 'usage': 'python quant_analytics_cli.py <command> [params_json]'}
        print(json.dumps(result, cls=DecimalEncoder))
        return

    command = sys.argv[1].lower()
    params = {}

    if len(sys.argv) > 2:
        try:
            params = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            result = {'success': False, 'error': f'Invalid JSON parameters: {e}'}
            print(json.dumps(result, cls=DecimalEncoder))
            return

    engine = CFAQuantEngine()

    command_map = {
        'list': engine.list_analyses,
        'trend_analysis': lambda: engine.trend_analysis(params),
        'stationarity_test': lambda: engine.stationarity_test(params),
        'arima_model': lambda: engine.arima_model(params),
        'forecasting': lambda: engine.forecasting(params),
        'supervised_learning': lambda: engine.supervised_learning(params),
        'unsupervised_learning': lambda: engine.unsupervised_learning(params),
        'model_evaluation': lambda: engine.model_evaluation(params),
        'feature_engineering': lambda: engine.feature_engineering(params),
        'sampling_techniques': lambda: engine.sampling_techniques(params),
        'central_limit_theorem': lambda: engine.central_limit_theorem(params),
        'resampling_methods': lambda: engine.resampling_methods(params),
        'sampling_error_analysis': lambda: engine.sampling_error_analysis(params),
        'validate_data': lambda: engine.validate_data(params),
    }

    if command not in command_map:
        result = {
            'success': False,
            'error': f'Unknown command: {command}',
            'available_commands': list(command_map.keys())
        }
        print(json.dumps(result, cls=DecimalEncoder))
        return

    try:
        result = command_map[command]()
        print(json.dumps(result, cls=DecimalEncoder))
    except Exception as e:
        import traceback
        result = {
            'success': False,
            'error': str(e),
            'traceback': traceback.format_exc(),
            'command': command
        }
        print(json.dumps(result, cls=DecimalEncoder))


if __name__ == '__main__':
    main()
