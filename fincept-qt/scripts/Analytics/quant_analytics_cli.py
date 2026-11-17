"""
Quant Analytics CLI - Unified Command Line Interface
Provides a unified entry point for quantitative analytics in Fincept Terminal.

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
import re
import numpy as np
import pandas as pd
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, date

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(script_dir))
    import fincept_json

# A ticker: letters/digits plus . - ^ = (AAPL, BRK.B, 0700.HK, ^GSPC, ES=F, EURUSD=X, BTC-USD).
_TICKER_RE = re.compile(r'^[A-Za-z0-9.\-^=]+$')
# Holt-Winters season length when the caller gives none: one trading week for daily bars,
# one year for weekly and monthly bars.
_DEFAULT_SEASONAL_PERIODS = {'1d': 5, '1wk': 52, '1mo': 12}


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
        elif isinstance(obj, (datetime, date)):
            return obj.isoformat()
        elif obj is None or isinstance(obj, (str, int)):
            return obj
        elif pd.isna(obj):
            return None
        return obj

    def _looks_like_ticker(self, s: str) -> bool:
        s = s.strip()
        if not s or ',' in s or s.startswith('[') or s.startswith('{'):
            return False
        try:
            float(s)
            return False  # a lone number is data, not a symbol
        except ValueError:
            pass
        # e.g. AAPL, BRK.B, TCS.NS, ^GSPC, 0700.HK, ES=F, EURUSD=X, BTC-USD
        return bool(_TICKER_RE.match(s)) and any(c.isalpha() for c in s)

    def _fetch_ticker_history(self, ticker: str, period: str = '1y',
                              interval: str = '1d', column: str = 'Close') -> np.ndarray:
        try:
            import fincept_yahoo as fy  # the shared Yahoo layer: bar cache, limiter, back-off
            fy.yf()
        except ImportError as exc:
            raise ImportError(f"yfinance is required to fetch ticker '{ticker}': {exc}")

        import contextlib
        with contextlib.redirect_stdout(sys.stderr):  # keep any library chatter out of the JSON stdout
            # One long download per symbol shared by every script; later calls slice it.
            df = fy.download(ticker, period=period, interval=interval, auto_adjust=False)
        if df is None or df.empty:
            raise ValueError(f"No data returned for ticker '{ticker}' (period={period})")

        # yfinance returns a MultiIndex when multiple tickers / multi-level cols
        if isinstance(df.columns, pd.MultiIndex):
            # pick the requested column at level 0
            try:
                series = df[column].iloc[:, 0]
            except (KeyError, IndexError):
                series = df.iloc[:, 0]
        else:
            series = df[column] if column in df.columns else df.iloc[:, 0]

        values = pd.Series(series).dropna().to_numpy(dtype=float)
        if values.size == 0:
            raise ValueError(f"Ticker '{ticker}' produced an empty series after cleaning")
        return values

    def _parse_data(self, data: Union[str, List, Dict],
                    period: str = '1y', interval: str = '1d',
                    column: str = 'Close') -> np.ndarray:
        if isinstance(data, str):
            stripped = data.strip()
            # 1. JSON list / array
            if stripped.startswith('[') or stripped.startswith('{'):
                parsed = json.loads(stripped)
                if isinstance(parsed, list):
                    return np.array(parsed, dtype=float)
                if isinstance(parsed, dict):
                    return pd.DataFrame(parsed).values
            # 2. Ticker symbol → fetch close prices
            if self._looks_like_ticker(stripped):
                return self._fetch_ticker_history(stripped, period=period,
                                                  interval=interval, column=column)
            # 3. Numeric values separated by commas (or whitespace / semicolons)
            tokens = [t for t in re.split(r'[,;\s]+', stripped) if t]
            if not tokens:
                raise ValueError("Empty data input")
            values = []
            for t in tokens:
                try:
                    values.append(float(t))
                except ValueError:
                    raise ValueError(f"Could not read '{t}' as a number. Enter one ticker symbol "
                                     f"(e.g. AAPL, 0700.HK, ES=F) or comma-separated numbers") from None
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

    def _data_kwargs(self, params: Dict[str, Any]) -> Dict[str, str]:
        return {
            'period': params.get('period', '1y'),
            'interval': params.get('interval', '1d'),
            'column': params.get('column', 'Close'),
        }

    def trend_analysis(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []), **self._data_kwargs(params))
        trend_type = params.get('trend_type', 'linear')

        result = self.analyzer.analyze_trend(data, trend_type=trend_type)
        return {'success': True, 'analysis_type': 'trend_analysis', 'result': self._convert_numpy_types(result)}

    def stationarity_test(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []), **self._data_kwargs(params))
        test_type = params.get('test_type', 'adf')

        result = self.analyzer.test_stationarity(data, test_type=test_type)
        return {'success': True, 'analysis_type': 'stationarity_test', 'result': self._convert_numpy_types(result)}

    def arima_model(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        data = self._parse_data(params.get('data', []), **self._data_kwargs(params))

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

        data = self._parse_data(params.get('data', []), **self._data_kwargs(params))
        horizon = params.get('horizon', 5)
        method = params.get('method', 'simple_exponential')
        train_size = params.get('train_size', 0.8)

        options = {}
        if params.get('alpha') is not None:
            options['alpha'] = params['alpha']
        if params.get('order') is not None:
            options['order'] = params['order']
        if params.get('seasonal') is not None:
            options['seasonal'] = params['seasonal']
        seasonal_periods = params.get('seasonal_periods')
        if seasonal_periods is None:
            seasonal_periods = _DEFAULT_SEASONAL_PERIODS.get(str(params.get('interval', '1d')), 5)
        options['seasonal_periods'] = seasonal_periods

        result = self.analyzer.generate_forecasts(data, horizon=horizon, method=method, train_size=train_size,
                                                  options=options)
        return {'success': True, 'analysis_type': 'forecasting', 'result': self._convert_numpy_types(result)}

    @staticmethod
    def _n_lags(params: Dict[str, Any]) -> int:
        try:
            n_lags = int(params.get('n_lags') or 5)
        except (TypeError, ValueError):
            raise ValueError(f"n_lags must be an integer, got {params.get('n_lags')!r}") from None
        return n_lags if n_lags >= 1 else 5

    def _create_lag_features(self, data: np.ndarray, n_lags: int = 5) -> tuple:
        """Regression features: X = the n_lags previous values, y = the next value."""
        if len(data) < n_lags + 2:
            raise ValueError(f"Need at least {n_lags + 2} data points for lag features, got {len(data)}")

        X = []
        y = []
        for i in range(n_lags, len(data)):
            X.append(data[i-n_lags:i])
            y.append(data[i])
        return np.array(X), np.array(y)

    def _create_direction_features(self, data: np.ndarray, n_lags: int = 5) -> tuple:
        """Classification features for a series.

        y = 1 when the next value is higher than the current one, else 0 (flat or down).
        X = the n_lags previous one-step changes: simple returns for a strictly positive series
        (prices), plain differences otherwise. Only changes up to the current value are used.
        """
        data = np.asarray(data, dtype=float)
        if len(data) < n_lags + 12:
            raise ValueError(f"Need at least {n_lags + 12} data points for {n_lags} lagged changes, got {len(data)}")
        if np.all(data > 0):
            changes = data[1:] / data[:-1] - 1.0
            feature_kind = 'lagged simple returns'
        else:
            changes = np.diff(data)
            feature_kind = 'lagged differences'
        X = np.array([changes[i - n_lags:i] for i in range(n_lags, len(changes))])
        y = (changes[n_lags:] > 0).astype(int)
        info = {
            'features': f'{n_lags} {feature_kind}',
            'target': '1 if the next value is higher than the current one, else 0',
        }
        return X, y, info

    def _ml_inputs(self, params: Dict[str, Any], problem_type: Optional[str]) -> tuple:
        """(X, y, feature_info) from explicit X/y, or built from the `data` series."""
        X = params.get('X')
        y = params.get('y')
        if X is not None and y is not None:
            return np.array(X, dtype=float), np.array(y, dtype=float), None
        data = params.get('data')
        if data is None:
            raise ValueError('Either provide X/y arrays or a data time series')
        data_arr = self._parse_data(data, **self._data_kwargs(params))
        n_lags = self._n_lags(params)
        if problem_type == 'classification':
            return self._create_direction_features(data_arr, n_lags=n_lags)
        X, y = self._create_lag_features(data_arr, n_lags=n_lags)
        info = {'features': f'{n_lags} lagged values', 'target': 'the next value'}
        return X, y, info

    def supervised_learning(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        problem_type = params.get('problem_type', 'regression')
        algorithms = params.get('algorithms') or ['ridge', 'random_forest']
        if isinstance(algorithms, str):
            algorithms = [a.strip() for a in algorithms.split(',') if a.strip()]

        try:
            X, y, feature_info = self._ml_inputs(params, problem_type)
        except ValueError as e:
            return {'success': False, 'error': str(e)}

        result = self.analyzer.supervised_learning_analysis(X, y, problem_type=problem_type, algorithms=algorithms)
        result = self._convert_numpy_types(result)
        if feature_info:
            result.setdefault('metadata', {}).update(feature_info)

        payload = {'success': True, 'analysis_type': 'supervised_learning', 'result': result}
        value = result.get('value') or {}
        failed = {k: v.get('error') for k, v in value.items() if isinstance(v, dict) and 'error' in v}
        if value and len(failed) == len(value):
            payload['success'] = False
            payload['error'] = 'Every algorithm failed: ' + '; '.join(f'{k}: {e}' for k, e in failed.items())
        return payload

    def unsupervised_learning(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        X = params.get('X')

        if X is None:
            data = params.get('data')
            if data is not None:
                data_arr = self._parse_data(data, **self._data_kwargs(params))
                try:
                    X, _ = self._create_lag_features(data_arr, n_lags=self._n_lags(params))
                except ValueError as e:
                    return {'success': False, 'error': str(e)}
            else:
                return {'success': False, 'error': 'Either provide X array or a data time series'}
        else:
            X = np.array(X, dtype=float)

        methods = params.get('methods') or ['pca', 'kmeans']
        if isinstance(methods, str):
            methods = [m.strip() for m in methods.split(',') if m.strip()]

        result = self.analyzer.unsupervised_learning_analysis(X, methods=methods)
        return {'success': True, 'analysis_type': 'unsupervised_learning', 'result': self._convert_numpy_types(result)}

    def model_evaluation(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if self.analyzer is None:
            return {'success': False, 'error': f'Module not loaded: {getattr(self, "import_error", "unknown")}'}

        # None = decide from y (regression unless y is integer-valued with <= 5 classes)
        problem_type = params.get('problem_type')
        try:
            X, y, feature_info = self._ml_inputs(params, problem_type)
        except ValueError as e:
            return {'success': False, 'error': str(e)}

        model_type = params.get('model_type', 'random_forest')
        cv_folds = params.get('cv_folds', 5)

        result = self.analyzer.evaluate_model_performance(X, y, model_type=model_type, cv_folds=cv_folds,
                                                          problem_type=problem_type)
        result = self._convert_numpy_types(result)
        if feature_info:
            result.setdefault('metadata', {}).update(feature_info)
        return {'success': True, 'analysis_type': 'model_evaluation', 'result': result}

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

        population = self._parse_data(params.get('population', []), **self._data_kwargs(params))
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

        data = self._parse_data(params.get('data', []), **self._data_kwargs(params))
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

        raw = params.get('data', [])
        if isinstance(raw, str):
            try:
                arr = self._parse_data(raw, **self._data_kwargs(params))
                data = pd.Series(arr)
            except Exception as e:
                return {'success': False, 'error': f'Could not parse data: {e}'}
        elif isinstance(raw, list):
            data = pd.Series(raw)
        elif isinstance(raw, dict):
            data = pd.DataFrame(raw)
        else:
            data = pd.Series(raw)

        data_type = params.get('data_type', 'general')
        data_name = params.get('data_name', 'data')

        cleaned_data, report = self.validator.validate_financial_data(data, data_type=data_type, data_name=data_name)

        return {
            'success': True,
            'analysis_type': 'validate_data',
            'result': self._convert_numpy_types(report.to_dict()),
            'data_cleaned': isinstance(cleaned_data, pd.DataFrame) or isinstance(cleaned_data, pd.Series)
        }


def _emit(result: Dict[str, Any]) -> None:
    """The one stdout payload: NaN/Infinity become null (a bare NaN makes the host drop it all)."""
    print(fincept_json.dumps(result))


def main():
    if len(sys.argv) < 2:
        _emit({'success': False, 'error': 'No command provided',
               'usage': 'python quant_analytics_cli.py <command> [params_json]'})
        return

    command = sys.argv[1].lower()
    params = {}

    if len(sys.argv) > 2:
        try:
            params = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            _emit({'success': False, 'error': f'Invalid JSON parameters: {e}'})
            return
        if not isinstance(params, dict):
            _emit({'success': False, 'error': 'Parameters must be a JSON object'})
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
        _emit({
            'success': False,
            'error': f'Unknown command: {command}',
            'available_commands': list(command_map.keys())
        })
        return

    try:
        result = command_map[command]()
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)  # diagnostics go to stderr, never into the payload
        result = {
            'success': False,
            'error': str(e) or type(e).__name__,
            'command': command
        }
    _emit(result)


if __name__ == '__main__':
    main()
