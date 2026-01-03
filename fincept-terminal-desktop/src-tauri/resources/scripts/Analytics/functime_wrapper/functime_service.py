"""
Functime Service - Python backend for functime analytics via PyO3
=================================================================

Provides JSON-RPC interface for Rust/Tauri to call functime forecasting functions.

Note: This service provides local-only forecasting when functime cloud is not configured.
Uses sklearn-based implementations as fallback for cloud API.
"""

import json
import sys
import os
from typing import Dict, List, Any, Optional
from datetime import datetime, timedelta

# Add this script's directory to Python path for local imports
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

import numpy as np

try:
    import polars as pl
    POLARS_AVAILABLE = True
except ImportError:
    POLARS_AVAILABLE = False

# Try sklearn for local fallback
try:
    from sklearn.linear_model import LinearRegression, Lasso, Ridge, ElasticNet, LassoCV, RidgeCV, ElasticNetCV
    from sklearn.neighbors import KNeighborsRegressor
    from sklearn.model_selection import GridSearchCV, TimeSeriesSplit
    SKLEARN_AVAILABLE = True
except ImportError:
    SKLEARN_AVAILABLE = False

try:
    from lightgbm import LGBMRegressor
    LIGHTGBM_AVAILABLE = True
except ImportError:
    LIGHTGBM_AVAILABLE = False

# Check if functime cloud is available and configured
FUNCTIME_CLOUD_AVAILABLE = False
try:
    import functime
    # Check if credentials exist
    import toml
    config_path = os.path.expanduser("~/.functime.toml")
    if os.path.exists(config_path):
        with open(config_path, 'r') as f:
            config = toml.load(f)
            if config.get('token_id') and config.get('token_secret'):
                FUNCTIME_CLOUD_AVAILABLE = True
except:
    pass

# Use local sklearn implementation as default
FUNCTIME_AVAILABLE = SKLEARN_AVAILABLE and POLARS_AVAILABLE

# Import local modules after path fix
if FUNCTIME_AVAILABLE:
    try:
        from preprocessing import (
            apply_boxcox, scale_data, difference_data,
            create_lags, create_rolling_features, impute_missing
        )
        from cross_validation import (
            split_train_test, create_expanding_window_splits,
            create_sliding_window_splits
        )
        from metrics import (
            calculate_mae, calculate_rmse, calculate_smape,
            calculate_mape, calculate_mse
        )
        from feature_extraction import (
            create_calendar_effects, create_future_calendar_effects
        )
        from offsets import frequency_to_seasonal_period
    except ImportError as e:
        print(f"Warning: Could not import local modules: {e}", file=sys.stderr)

# Import advanced modules
try:
    from advanced_models import (
        naive_forecast, seasonal_naive_forecast, drift_forecast,
        ses_forecast, holt_forecast, theta_forecast,
        croston_forecast, xgboost_forecast, catboost_forecast
    )
    ADVANCED_MODELS_AVAILABLE = True
except ImportError as e:
    ADVANCED_MODELS_AVAILABLE = False
    print(f"Warning: Could not import advanced_models: {e}", file=sys.stderr)

try:
    from ensemble import (
        ensemble_mean, ensemble_median, ensemble_trimmed_mean,
        ensemble_weighted, ensemble_stacking, auto_ensemble
    )
    ENSEMBLE_AVAILABLE = True
except ImportError as e:
    ENSEMBLE_AVAILABLE = False
    print(f"Warning: Could not import ensemble: {e}", file=sys.stderr)

try:
    from anomaly_detection import (
        detect_zscore, detect_iqr, detect_grubbs,
        detect_isolation_forest, detect_lof,
        detect_residual_anomalies, detect_change_points
    )
    ANOMALY_AVAILABLE = True
except ImportError as e:
    ANOMALY_AVAILABLE = False
    print(f"Warning: Could not import anomaly_detection: {e}", file=sys.stderr)

try:
    from seasonality import (
        detect_seasonal_period, decompose_stl, decompose_classical,
        seasonally_adjust, calculate_seasonality_metrics
    )
    SEASONALITY_AVAILABLE = True
except ImportError as e:
    SEASONALITY_AVAILABLE = False
    print(f"Warning: Could not import seasonality: {e}", file=sys.stderr)

try:
    from confidence_intervals import (
        bootstrap_prediction_intervals, residual_prediction_intervals,
        quantile_prediction_intervals, conformal_prediction_intervals,
        monte_carlo_intervals
    )
    CONFIDENCE_AVAILABLE = True
except ImportError as e:
    CONFIDENCE_AVAILABLE = False
    print(f"Warning: Could not import confidence_intervals: {e}", file=sys.stderr)

try:
    from feature_importance import (
        calculate_permutation_importance, calculate_model_importance,
        calculate_shap_importance, analyze_lag_importance, sensitivity_analysis
    )
    FEATURE_IMPORTANCE_AVAILABLE = True
except ImportError as e:
    FEATURE_IMPORTANCE_AVAILABLE = False
    print(f"Warning: Could not import feature_importance: {e}", file=sys.stderr)

try:
    from advanced_cv import (
        blocked_time_series_cv, purged_kfold_cv, combinatorial_purged_cv,
        walk_forward_validation, nested_cv, get_cv_splits
    )
    ADVANCED_CV_AVAILABLE = True
except ImportError as e:
    ADVANCED_CV_AVAILABLE = False
    print(f"Warning: Could not import advanced_cv: {e}", file=sys.stderr)

try:
    from backtesting import (
        walk_forward_backtest, compare_models_backtest,
        rolling_origin_evaluation, performance_attribution, backtest_summary
    )
    BACKTESTING_AVAILABLE = True
except ImportError as e:
    BACKTESTING_AVAILABLE = False
    print(f"Warning: Could not import backtesting: {e}", file=sys.stderr)


def parse_panel_data_json(data_json: str) -> "pl.DataFrame":
    """Parse JSON data into Polars DataFrame for functime panel data format"""
    data = json.loads(data_json)

    if isinstance(data, dict):
        # Check if it's multi-entity format {entity: {date: value}}
        first_value = next(iter(data.values()), None)
        if isinstance(first_value, dict):
            # Multi-entity: {entity: {date: value}}
            rows = []
            for entity_id, time_values in data.items():
                for time_str, value in time_values.items():
                    rows.append({
                        'entity_id': entity_id,
                        'time': time_str,
                        'value': float(value)
                    })
            df = pl.DataFrame(rows)
            df = df.with_columns([
                pl.col('time').str.to_datetime()
            ])
        else:
            # Single entity: {date: value} - wrap as entity 'default'
            rows = []
            for time_str, value in data.items():
                rows.append({
                    'entity_id': 'default',
                    'time': time_str,
                    'value': float(value)
                })
            df = pl.DataFrame(rows)
            df = df.with_columns([
                pl.col('time').str.to_datetime()
            ])
    elif isinstance(data, list):
        # Array format: [{entity_id, time, value}, ...]
        df = pl.DataFrame(data)
        if 'time' in df.columns and df['time'].dtype == pl.Utf8:
            df = df.with_columns([
                pl.col('time').str.to_datetime()
            ])
        if 'entity_id' not in df.columns:
            df = df.with_columns([
                pl.lit('default').alias('entity_id')
            ])
    else:
        raise ValueError(f"Unsupported data format: {type(data)}")

    # Sort by entity_id and time
    df = df.sort(['entity_id', 'time'])
    return df


def serialize_to_json(obj: Any) -> str:
    """Convert object to JSON string with proper type handling"""
    from datetime import date

    def convert(item):
        if item is None:
            return None
        if isinstance(item, datetime):
            return item.isoformat()
        if isinstance(item, date):
            return item.isoformat()
        if isinstance(item, (np.integer, np.floating)):
            return float(item)
        if isinstance(item, np.ndarray):
            return [convert(v) for v in item.tolist()]
        if isinstance(item, np.bool_):
            return bool(item)
        if POLARS_AVAILABLE and isinstance(item, pl.DataFrame):
            return [convert(d) for d in item.to_dicts()]
        if POLARS_AVAILABLE and isinstance(item, pl.Series):
            return [convert(v) for v in item.to_list()]
        if isinstance(item, dict):
            return {str(k): convert(v) for k, v in item.items()}
        if isinstance(item, (list, tuple)):
            return [convert(v) for v in item]
        if isinstance(item, float) and np.isnan(item):
            return None
        return item

    return json.dumps(convert(obj))


def serialize_result(data: Any) -> str:
    """Serialize result to JSON, handling numpy/polars types"""
    from datetime import date

    def convert(obj):
        # Handle None first
        if obj is None:
            return None

        # Handle numpy types
        if isinstance(obj, (np.integer, np.floating)):
            if np.isnan(obj):
                return None
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return [convert(v) for v in obj.tolist()]
        elif isinstance(obj, np.bool_):
            return bool(obj)

        # Handle polars types
        elif POLARS_AVAILABLE and isinstance(obj, pl.DataFrame):
            return [convert(d) for d in obj.to_dicts()]
        elif POLARS_AVAILABLE and isinstance(obj, pl.Series):
            return [convert(v) for v in obj.to_list()]

        # Handle datetime and date
        elif isinstance(obj, datetime):
            return obj.isoformat()
        elif isinstance(obj, date):
            return obj.isoformat()

        # Handle NaN for scalar floats
        elif isinstance(obj, float) and np.isnan(obj):
            return None

        # Handle tuples (convert to list)
        elif isinstance(obj, tuple):
            return [convert(v) for v in obj]

        # Handle dicts and lists
        elif isinstance(obj, dict):
            return {str(k): convert(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [convert(v) for v in obj]

        # Return as-is for other types
        return obj

    return json.dumps(convert(data), indent=2)


# ============================================================================
# COMMAND HANDLERS
# ============================================================================

def check_status() -> Dict[str, Any]:
    """Check functime library availability and version"""
    return {
        "success": True,
        "available": FUNCTIME_AVAILABLE,
        "sklearn_available": SKLEARN_AVAILABLE,
        "polars_available": POLARS_AVAILABLE,
        "lightgbm_available": LIGHTGBM_AVAILABLE,
        "functime_cloud_available": FUNCTIME_CLOUD_AVAILABLE,
        "advanced_models_available": ADVANCED_MODELS_AVAILABLE,
        "ensemble_available": ENSEMBLE_AVAILABLE,
        "anomaly_available": ANOMALY_AVAILABLE,
        "seasonality_available": SEASONALITY_AVAILABLE,
        "confidence_available": CONFIDENCE_AVAILABLE,
        "feature_importance_available": FEATURE_IMPORTANCE_AVAILABLE,
        "advanced_cv_available": ADVANCED_CV_AVAILABLE,
        "backtesting_available": BACKTESTING_AVAILABLE,
        "message": "Time series forecasting ready (sklearn backend)" if FUNCTIME_AVAILABLE else "scikit-learn or polars not installed"
    }


def run_forecast(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run forecasting with specified model using sklearn backend"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Forecasting library not available (sklearn or polars missing)"}

    try:
        data = parse_panel_data_json(params['data'])
        model_type = params.get('model', 'linear')
        fh = params.get('horizon', 7)
        freq = params.get('frequency', '1d')

        # Model parameters
        alpha = params.get('alpha', 1.0)
        l1_ratio = params.get('l1_ratio', 0.5)
        n_neighbors = params.get('n_neighbors', 5)
        lags = params.get('lags', 3)  # Number of lags for autoregression

        # Get unique entities
        entities = data['entity_id'].unique().to_list()
        all_forecasts = []
        final_best_params = None

        for entity_id in entities:
            entity_data = data.filter(pl.col('entity_id') == entity_id).sort('time')
            times = entity_data['time'].to_list()
            values = entity_data['value'].to_numpy()

            if len(values) < lags + 1:
                continue

            # Create lag features
            X, y = [], []
            for i in range(lags, len(values)):
                X.append(values[i-lags:i])
                y.append(values[i])
            X = np.array(X)
            y = np.array(y)

            # Select and fit model
            best_params = None

            if model_type == 'linear':
                model = LinearRegression()
            elif model_type == 'lasso':
                model = Lasso(alpha=alpha)
            elif model_type == 'ridge':
                model = Ridge(alpha=alpha)
            elif model_type == 'elasticnet':
                model = ElasticNet(alpha=alpha, l1_ratio=l1_ratio)
            elif model_type == 'knn':
                model = KNeighborsRegressor(n_neighbors=min(n_neighbors, len(X)))
            elif model_type == 'lightgbm':
                if not LIGHTGBM_AVAILABLE:
                    return {"success": False, "error": "LightGBM not installed. Please install: pip install lightgbm"}
                model = LGBMRegressor(n_estimators=100, verbosity=-1)

            # Auto-tuned models with cross-validation
            elif model_type == 'auto_lasso':
                alphas = np.logspace(-4, 2, 50)
                cv_splits = min(5, len(X) - 1)
                if cv_splits < 2:
                    cv_splits = 2
                model = LassoCV(alphas=alphas, cv=cv_splits, max_iter=10000)
                model.fit(X, y)
                best_params = {'alpha': float(model.alpha_)}

            elif model_type == 'auto_ridge':
                alphas = np.logspace(-4, 4, 50)
                cv_splits = min(5, len(X) - 1)
                if cv_splits < 2:
                    cv_splits = 2
                model = RidgeCV(alphas=alphas, cv=cv_splits)
                model.fit(X, y)
                best_params = {'alpha': float(model.alpha_)}

            elif model_type == 'auto_elasticnet':
                alphas = np.logspace(-4, 2, 20)
                l1_ratios = [0.1, 0.3, 0.5, 0.7, 0.9, 0.95, 0.99]
                cv_splits = min(5, len(X) - 1)
                if cv_splits < 2:
                    cv_splits = 2
                model = ElasticNetCV(alphas=alphas, l1_ratio=l1_ratios, cv=cv_splits, max_iter=10000)
                model.fit(X, y)
                best_params = {'alpha': float(model.alpha_), 'l1_ratio': float(model.l1_ratio_)}

            elif model_type == 'auto_knn':
                # Grid search for best n_neighbors
                max_neighbors = min(20, len(X) - 1)
                if max_neighbors < 1:
                    max_neighbors = 1
                param_grid = {'n_neighbors': list(range(1, max_neighbors + 1, 2))}
                cv_splits = min(3, len(X) - 1)
                if cv_splits < 2:
                    cv_splits = 2
                base_model = KNeighborsRegressor()
                tscv = TimeSeriesSplit(n_splits=cv_splits)
                grid_search = GridSearchCV(base_model, param_grid, cv=tscv, scoring='neg_mean_squared_error')
                grid_search.fit(X, y)
                model = grid_search.best_estimator_
                best_params = grid_search.best_params_

            elif model_type == 'auto_lightgbm':
                if not LIGHTGBM_AVAILABLE:
                    return {"success": False, "error": "LightGBM not installed. Please install: pip install lightgbm"}
                param_grid = {
                    'n_estimators': [50, 100, 200],
                    'learning_rate': [0.01, 0.05, 0.1],
                    'max_depth': [3, 5, 7]
                }
                cv_splits = min(3, len(X) - 1)
                if cv_splits < 2:
                    cv_splits = 2
                base_model = LGBMRegressor(verbosity=-1)
                tscv = TimeSeriesSplit(n_splits=cv_splits)
                grid_search = GridSearchCV(base_model, param_grid, cv=tscv, scoring='neg_mean_squared_error')
                grid_search.fit(X, y)
                model = grid_search.best_estimator_
                best_params = grid_search.best_params_

            else:
                return {"success": False, "error": f"Unknown model type: {model_type}. Supported: linear, lasso, ridge, elasticnet, knn, lightgbm, auto_lasso, auto_ridge, auto_elasticnet, auto_knn, auto_lightgbm"}

            # Fit model if not already fitted by CV
            if model_type not in ['auto_lasso', 'auto_ridge', 'auto_elasticnet', 'auto_knn', 'auto_lightgbm']:
                model.fit(X, y)

            # Store best params from first entity (they're the same for all)
            if best_params and final_best_params is None:
                final_best_params = best_params

            # Generate future forecasts
            last_values = values[-lags:].tolist()
            last_time = times[-1]

            # Parse frequency to timedelta
            freq_map = {
                '1d': timedelta(days=1),
                '1h': timedelta(hours=1),
                '1w': timedelta(weeks=1),
                '1mo': timedelta(days=30),
                '1m': timedelta(minutes=1),
            }
            delta = freq_map.get(freq, timedelta(days=1))

            for h in range(fh):
                X_pred = np.array([last_values[-lags:]])
                y_pred = model.predict(X_pred)[0]
                forecast_time = last_time + delta * (h + 1)

                all_forecasts.append({
                    'entity_id': entity_id,
                    'time': forecast_time.isoformat() if hasattr(forecast_time, 'isoformat') else str(forecast_time),
                    'value': float(y_pred)
                })

                last_values.append(y_pred)

        return {
            "success": True,
            "model": model_type,
            "horizon": fh,
            "frequency": freq,
            "lags": lags,
            "forecast": all_forecasts,
            "shape": [len(all_forecasts), 3],
            "best_params": final_best_params,
            "backend": "sklearn"
        }
    except Exception as e:
        import traceback
        return {"success": False, "error": str(e), "traceback": traceback.format_exc()}


def preprocess_data(params: Dict[str, Any]) -> Dict[str, Any]:
    """Apply preprocessing transformations"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Functime library not available"}

    try:
        data = parse_panel_data_json(params['data'])
        method = params.get('method', 'scale')

        if method == 'boxcox':
            lmbda = params.get('lambda')
            result = apply_boxcox(data, lmbda=lmbda)
        elif method == 'scale':
            scale_method = params.get('scale_method', 'standard')
            result = scale_data(data, method=scale_method)
        elif method == 'difference':
            order = params.get('order', 1)
            result = difference_data(data, order=order)
        elif method == 'lags':
            lags = params.get('lags', [1, 2, 3])
            result = create_lags(data, lags=lags)
        elif method == 'rolling':
            window_sizes = params.get('window_sizes', [3, 7])
            stats = params.get('stats', ['mean'])
            result = create_rolling_features(data, window_sizes=window_sizes, stats=stats)
        elif method == 'impute':
            impute_method = params.get('impute_method', 'forward')
            result = impute_missing(data, method=impute_method)
        else:
            return {"success": False, "error": f"Unknown preprocessing method: {method}"}

        return {
            "success": True,
            "method": method,
            "shape": result.get('shape', []),
            "columns": result.get('columns', []),
            "data": result.get(list(result.keys())[0], [])  # Get first data key
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def cross_validate(params: Dict[str, Any]) -> Dict[str, Any]:
    """Perform cross-validation splits"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Functime library not available"}

    try:
        data = parse_panel_data_json(params['data'])
        method = params.get('method', 'train_test')
        test_size = params.get('test_size', 1)
        n_splits = params.get('n_splits', 3)

        if method == 'train_test':
            result = split_train_test(data, test_size=test_size)
            return {
                "success": True,
                "method": method,
                "train_shape": result['train_shape'],
                "test_shape": result['test_shape']
            }
        elif method == 'expanding':
            step = params.get('step', 1)
            result = create_expanding_window_splits(
                data, test_size=test_size, n_splits=n_splits, step=step
            )
            return {
                "success": True,
                "method": method,
                "n_splits": result['n_splits'],
                "splits_summary": [
                    {"train_shape": s['train_shape'], "test_shape": s['test_shape']}
                    for s in result['splits']
                ]
            }
        elif method == 'sliding':
            train_size = params.get('train_size', 5)
            step = params.get('step', 1)
            result = create_sliding_window_splits(
                data, train_size=train_size, test_size=test_size,
                n_splits=n_splits, step=step
            )
            return {
                "success": True,
                "method": method,
                "n_splits": result['n_splits'],
                "splits_summary": [
                    {"train_shape": s['train_shape'], "test_shape": s['test_shape']}
                    for s in result['splits']
                ]
            }
        else:
            return {"success": False, "error": f"Unknown CV method: {method}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def calculate_metrics(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate forecast accuracy metrics"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Functime library not available"}

    try:
        y_true = parse_panel_data_json(params['y_true'])
        y_pred = parse_panel_data_json(params['y_pred'])
        metrics_list = params.get('metrics', ['mae', 'rmse', 'smape'])

        results = {}

        if 'mae' in metrics_list:
            try:
                mae_result = calculate_mae(y_true, y_pred)
                results['mae'] = mae_result['mean_mae']
            except Exception:
                results['mae'] = None

        if 'rmse' in metrics_list:
            try:
                rmse_result = calculate_rmse(y_true, y_pred)
                results['rmse'] = rmse_result['mean_rmse']
            except Exception:
                results['rmse'] = None

        if 'smape' in metrics_list:
            try:
                smape_result = calculate_smape(y_true, y_pred)
                results['smape'] = smape_result['mean_smape']
            except Exception:
                results['smape'] = None

        if 'mape' in metrics_list:
            try:
                mape_result = calculate_mape(y_true, y_pred)
                results['mape'] = mape_result['mean_mape']
            except Exception:
                results['mape'] = None

        if 'mse' in metrics_list:
            try:
                mse_result = calculate_mse(y_true, y_pred)
                results['mse'] = mse_result['mean_mse']
            except Exception:
                results['mse'] = None

        return {
            "success": True,
            "metrics": results
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def add_features(params: Dict[str, Any]) -> Dict[str, Any]:
    """Add calendar/holiday features"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Functime library not available"}

    try:
        data = parse_panel_data_json(params['data'])
        feature_type = params.get('feature_type', 'calendar')
        freq = params.get('frequency', '1d')

        if feature_type == 'calendar':
            result = create_calendar_effects(data, freq=freq)
        elif feature_type == 'future_calendar':
            fh = params.get('horizon', 7)
            result = create_future_calendar_effects(fh=fh, freq=freq)
        else:
            return {"success": False, "error": f"Unknown feature type: {feature_type}"}

        return {
            "success": True,
            "feature_type": feature_type,
            "shape": result.get('shape', []),
            "columns": result.get('columns', []),
            "data": result.get('data', [])
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_seasonal_period(params: Dict[str, Any]) -> Dict[str, Any]:
    """Get seasonal period for a frequency"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Functime library not available"}

    try:
        freq = params.get('frequency', '1d')
        result = frequency_to_seasonal_period(freq)

        return {
            "success": True,
            "frequency": freq,
            "seasonal_period": result['seasonal_period']
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def full_forecast_pipeline(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run a complete forecasting pipeline with preprocessing and evaluation"""
    if not FUNCTIME_AVAILABLE:
        return {"success": False, "error": "Forecasting library not available (sklearn or polars missing)"}

    try:
        data = parse_panel_data_json(params['data'])
        model_type = params.get('model', 'linear')
        fh = params.get('horizon', 7)
        freq = params.get('frequency', '1d')
        test_size = params.get('test_size', fh)
        preprocess_method = params.get('preprocess', None)

        # Skip preprocessing for now - use data as-is
        processed_data = data
        preprocessing_info = None

        if preprocess_method and preprocess_method != 'none':
            preprocessing_info = {"method": preprocess_method, "note": "Preprocessing not applied - using raw data"}

        # Train/test split: Hold out test_size rows per entity for evaluation
        # We train on earlier data, make predictions for the test period, and compare
        train_data = processed_data
        test_data = None

        try:
            # Group by entity and split
            entities = processed_data['entity_id'].unique().to_list()
            train_rows = []
            test_rows = []

            for entity_id in entities:
                entity_data = processed_data.filter(pl.col('entity_id') == entity_id).sort('time')
                n = len(entity_data)
                if n > test_size:
                    train_rows.extend(entity_data.head(n - test_size).to_dicts())
                    test_rows.extend(entity_data.tail(test_size).to_dicts())
                else:
                    train_rows.extend(entity_data.to_dicts())

            train_data = pl.DataFrame(train_rows) if train_rows else processed_data
            test_data = pl.DataFrame(test_rows) if test_rows else None
        except Exception as e:
            # If split fails, use all data for training
            train_data = processed_data
            print(f"Warning: Could not split data: {e}", file=sys.stderr)

        # Calculate evaluation metrics BEFORE generating the final forecast
        # We need to predict on the test period using the training data
        metrics = None
        if test_data is not None and len(test_data) > 0:
            try:
                # Get actual test values
                y_true_values = test_data['value'].to_numpy()

                # For evaluation, we predict for test_size periods from training data
                eval_forecast_result = run_forecast({
                    'data': serialize_to_json(train_data.to_dicts()),
                    'model': model_type,
                    'horizon': min(test_size, len(y_true_values)),  # Match test size
                    'frequency': freq
                })

                if eval_forecast_result.get('success'):
                    forecast_list = eval_forecast_result.get('forecast', [])
                    if len(forecast_list) > 0:
                        y_pred_values = np.array([f['value'] for f in forecast_list])

                        if len(y_pred_values) > 0 and len(y_true_values) > 0:
                            min_len = min(len(y_true_values), len(y_pred_values))
                            y_true = y_true_values[:min_len]
                            y_pred = y_pred_values[:min_len]

                            mae = float(np.mean(np.abs(y_true - y_pred)))
                            rmse = float(np.sqrt(np.mean((y_true - y_pred) ** 2)))
                            # SMAPE
                            smape = float(np.mean(2 * np.abs(y_true - y_pred) / (np.abs(y_true) + np.abs(y_pred) + 1e-8)) * 100)

                            metrics = {
                                'mae': mae,
                                'rmse': rmse,
                                'smape': smape
                            }
            except Exception as e:
                print(f"Warning: Could not calculate metrics: {e}", file=sys.stderr)

        # Now run forecast on FULL data for actual future predictions
        forecast_result = run_forecast({
            'data': serialize_to_json(processed_data.to_dicts()),
            'model': model_type,
            'horizon': fh,
            'frequency': freq
        })

        return {
            "success": True,
            "model": model_type,
            "horizon": fh,
            "frequency": freq,
            "preprocessing": preprocessing_info,
            "forecast": forecast_result.get('forecast', []),
            "forecast_shape": forecast_result.get('shape'),
            "best_params": forecast_result.get('best_params'),
            "evaluation_metrics": metrics,
            "data_summary": {
                "total_rows": len(data),
                "entities": data['entity_id'].n_unique() if 'entity_id' in data.columns else 1,
                "train_size": len(train_data) if train_data is not None else 0,
                "test_size": len(test_data) if test_data is not None else 0
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# ADVANCED MODELS COMMAND HANDLERS
# ============================================================================

def run_advanced_forecast(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run advanced forecasting models (naive, SES, Holt, Theta, Croston, XGBoost, CatBoost)"""
    if not ADVANCED_MODELS_AVAILABLE:
        return {"success": False, "error": "Advanced models not available"}

    try:
        data = parse_panel_data_json(params['data'])
        model_type = params.get('model', 'naive')
        fh = params.get('horizon', 7)
        freq = params.get('frequency', '1d')

        model_params = params.get('model_params', {})

        if model_type == 'naive':
            result = naive_forecast(data, fh=fh, freq=freq)
        elif model_type == 'seasonal_naive':
            seasonal_period = model_params.get('seasonal_period', 7)
            result = seasonal_naive_forecast(data, fh=fh, seasonal_period=seasonal_period, freq=freq)
        elif model_type == 'drift':
            result = drift_forecast(data, fh=fh, freq=freq)
        elif model_type == 'ses':
            alpha = model_params.get('alpha')
            result = ses_forecast(data, fh=fh, alpha=alpha, freq=freq)
        elif model_type == 'holt':
            alpha = model_params.get('alpha')
            beta = model_params.get('beta')
            result = holt_forecast(data, fh=fh, alpha=alpha, beta=beta, freq=freq)
        elif model_type == 'theta':
            result = theta_forecast(data, fh=fh, freq=freq)
        elif model_type == 'croston':
            alpha = model_params.get('alpha', 0.1)
            result = croston_forecast(data, fh=fh, alpha=alpha, freq=freq)
        elif model_type == 'xgboost':
            n_lags = model_params.get('n_lags', 10)
            xgb_params = model_params.get('xgb_params', {})
            result = xgboost_forecast(data, fh=fh, n_lags=n_lags, xgb_params=xgb_params, freq=freq)
        elif model_type == 'catboost':
            n_lags = model_params.get('n_lags', 10)
            cat_params = model_params.get('cat_params', {})
            result = catboost_forecast(data, fh=fh, n_lags=n_lags, cat_params=cat_params, freq=freq)
        else:
            return {"success": False, "error": f"Unknown advanced model: {model_type}"}

        return result
    except Exception as e:
        import traceback
        return {"success": False, "error": str(e), "traceback": traceback.format_exc()}


# ============================================================================
# ENSEMBLE COMMAND HANDLERS
# ============================================================================

def run_ensemble(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run ensemble forecasting"""
    if not ENSEMBLE_AVAILABLE:
        return {"success": False, "error": "Ensemble methods not available"}

    try:
        forecasts_json = params.get('forecasts', [])
        method = params.get('method', 'mean')

        # Parse forecasts - each should be a list of {entity_id, time, value}
        if isinstance(forecasts_json, str):
            forecasts_json = json.loads(forecasts_json)

        forecasts = [pl.DataFrame(fc) for fc in forecasts_json]

        if method == 'mean':
            result = ensemble_mean(forecasts)
        elif method == 'median':
            result = ensemble_median(forecasts)
        elif method == 'trimmed_mean':
            trim_pct = params.get('trim_pct', 0.1)
            result = ensemble_trimmed_mean(forecasts, trim_pct=trim_pct)
        elif method == 'weighted':
            weights = params.get('weights')
            result = ensemble_weighted(forecasts, weights=weights)
        elif method == 'stacking':
            y_train = parse_panel_data_json(params['y_train'])
            result = ensemble_stacking(forecasts, y_train)
        else:
            return {"success": False, "error": f"Unknown ensemble method: {method}"}

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


def run_auto_ensemble(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run automatic ensemble selection and combination"""
    if not ENSEMBLE_AVAILABLE:
        return {"success": False, "error": "Ensemble methods not available"}

    try:
        data = parse_panel_data_json(params['data'])
        fh = params.get('horizon', 7)
        freq = params.get('frequency', '1d')
        n_best = params.get('n_best', 3)

        result = auto_ensemble(data, fh=fh, freq=freq, n_best=n_best)
        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# ANOMALY DETECTION COMMAND HANDLERS
# ============================================================================

def run_anomaly_detection(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run anomaly detection on time series"""
    if not ANOMALY_AVAILABLE:
        return {"success": False, "error": "Anomaly detection not available"}

    try:
        data = parse_panel_data_json(params['data'])
        method = params.get('method', 'zscore')

        if method == 'zscore':
            threshold = params.get('threshold', 3.0)
            result = detect_zscore(data, threshold=threshold)
        elif method == 'iqr':
            multiplier = params.get('multiplier', 1.5)
            result = detect_iqr(data, k=multiplier)
        elif method == 'grubbs':
            alpha = params.get('alpha', 0.05)
            result = detect_grubbs(data, alpha=alpha)
        elif method == 'isolation_forest':
            contamination = params.get('contamination', 0.05)
            result = detect_isolation_forest(data, contamination=contamination)
        elif method == 'lof':
            n_neighbors = params.get('n_neighbors', 20)
            contamination = params.get('contamination', 0.05)
            result = detect_lof(data, n_neighbors=n_neighbors, contamination=contamination)
        elif method == 'residual':
            threshold = params.get('threshold', 3.0)
            result = detect_residual_anomalies(data, threshold=threshold)
        elif method == 'change_points':
            n_bkps = params.get('n_bkps', 5)
            result = detect_change_points(data, n_bkps=n_bkps)
        else:
            return {"success": False, "error": f"Unknown anomaly method: {method}"}

        # Restructure result to match frontend expectations
        # Frontend expects: { results: { entity_id: { anomalies: [...], n_anomalies, anomaly_rate } } }
        if result.get('success') and 'data' in result:
            # Group anomalies by entity_id
            all_data = result.get('data', [])
            anomalies_list = result.get('anomalies', [])

            # Group by entity_id
            entities_results = {}
            for item in all_data:
                entity_id = item.get('entity_id', 'default')
                if entity_id not in entities_results:
                    entities_results[entity_id] = {
                        'anomalies': [],
                        'n_anomalies': 0,
                        'anomaly_rate': 0.0,
                        'total_points': 0
                    }
                entities_results[entity_id]['total_points'] += 1
                if item.get('is_anomaly'):
                    entities_results[entity_id]['anomalies'].append(item)
                    entities_results[entity_id]['n_anomalies'] += 1

            # Calculate anomaly rate per entity
            for entity_id, entity_data in entities_results.items():
                if entity_data['total_points'] > 0:
                    entity_data['anomaly_rate'] = entity_data['n_anomalies'] / entity_data['total_points']
                del entity_data['total_points']  # Remove temp field

            return {
                'success': True,
                'method': result.get('method', method),
                'results': entities_results
            }

        return result
    except Exception as e:
        import traceback
        return {"success": False, "error": str(e), "traceback": traceback.format_exc()}


# ============================================================================
# SEASONALITY COMMAND HANDLERS
# ============================================================================

def run_seasonality_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run seasonality analysis on time series"""
    if not SEASONALITY_AVAILABLE:
        return {"success": False, "error": "Seasonality analysis not available"}

    try:
        data = parse_panel_data_json(params['data'])
        # Support both 'operation' and 'analysis_type' parameter names
        operation = params.get('operation') or params.get('analysis_type', 'decompose')

        # Map analysis_type values to operation values
        operation_map = {
            'decompose': 'decompose_stl',
            'detect': 'detect_period',
            'strength': 'metrics',
            'adjust': 'adjust',
        }
        operation = operation_map.get(operation, operation)

        period = params.get('period')
        model_type = params.get('model_type', 'additive')

        if operation == 'detect_period':
            method = params.get('method', 'fft')
            max_period = params.get('max_period', 365)
            result = detect_seasonal_period(data, method=method, max_period=max_period)
        elif operation == 'decompose_stl':
            robust = params.get('robust', True)
            result = decompose_stl(data, period=period, robust=robust)
        elif operation == 'decompose_classical':
            result = decompose_classical(data, period=period, model=model_type)
        elif operation == 'adjust':
            method = params.get('method', 'stl')
            result = seasonally_adjust(data, period=period, method=method)
        elif operation == 'metrics':
            result = calculate_seasonality_metrics(data, period=period)
        else:
            return {"success": False, "error": f"Unknown seasonality operation: {operation}"}

        return result
    except Exception as e:
        import traceback
        return {"success": False, "error": str(e), "traceback": traceback.format_exc()}


# ============================================================================
# CONFIDENCE INTERVALS COMMAND HANDLERS
# ============================================================================

def run_confidence_intervals(params: Dict[str, Any]) -> Dict[str, Any]:
    """Generate prediction/confidence intervals"""
    if not CONFIDENCE_AVAILABLE:
        return {"success": False, "error": "Confidence intervals not available"}

    try:
        method = params.get('method', 'monte_carlo')
        confidence_levels = params.get('confidence_levels', [0.80, 0.95])

        if method == 'bootstrap':
            data = parse_panel_data_json(params['data'])
            fh = params.get('horizon', 7)
            freq = params.get('frequency', '1d')
            n_bootstrap = params.get('n_bootstrap', 100)

            # Need a forecaster - create a simple one
            def simple_forecaster(y_train, fh, freq):
                last_value = y_train['value'].to_list()[-1]
                last_time = y_train['time'].to_list()[-1]
                delta = timedelta(days=1)
                forecast = []
                for i in range(1, fh + 1):
                    forecast.append({
                        'entity_id': y_train['entity_id'].to_list()[0],
                        'time': last_time + (delta * i),
                        'value': last_value
                    })
                return {'forecast': forecast}

            result = bootstrap_prediction_intervals(
                data, simple_forecaster, fh=fh,
                n_bootstrap=n_bootstrap, confidence_levels=confidence_levels, freq=freq
            )
        elif method == 'residual':
            data = parse_panel_data_json(params['data'])
            forecast = parse_panel_data_json(params['forecast'])
            y_validation = parse_panel_data_json(params['y_validation']) if 'y_validation' in params else None
            result = residual_prediction_intervals(data, forecast, y_validation, confidence_levels)
        elif method == 'quantile':
            data = parse_panel_data_json(params['data'])
            fh = params.get('horizon', 7)
            lags = params.get('lags', 10)
            quantiles = params.get('quantiles', [0.025, 0.10, 0.50, 0.90, 0.975])
            freq = params.get('frequency', '1d')
            result = quantile_prediction_intervals(data, fh=fh, lags=lags, quantiles=quantiles, freq=freq)
        elif method == 'conformal':
            data = parse_panel_data_json(params['data'])
            forecast = parse_panel_data_json(params['forecast'])
            calibration_size = params.get('calibration_size', 50)
            result = conformal_prediction_intervals(data, forecast, calibration_size, confidence_levels)
        elif method == 'monte_carlo':
            data = parse_panel_data_json(params['data'])
            fh = params.get('horizon', 7)
            n_simulations = params.get('n_simulations', 1000)
            model = params.get('model', 'random_walk')
            freq = params.get('frequency', '1d')
            result = monte_carlo_intervals(data, fh=fh, n_simulations=n_simulations, model=model,
                                          confidence_levels=confidence_levels, freq=freq)
        else:
            return {"success": False, "error": f"Unknown interval method: {method}"}

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# FEATURE IMPORTANCE COMMAND HANDLERS
# ============================================================================

def run_feature_importance(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate feature importance for time series"""
    if not FEATURE_IMPORTANCE_AVAILABLE:
        return {"success": False, "error": "Feature importance not available"}

    try:
        data = parse_panel_data_json(params['data'])
        method = params.get('method', 'permutation')
        n_lags = params.get('n_lags', 10)

        if method == 'permutation':
            n_repeats = params.get('n_repeats', 10)
            model_type = params.get('model_type', 'ridge')
            result = calculate_permutation_importance(data, n_lags=n_lags, n_repeats=n_repeats, model_type=model_type)
        elif method == 'model':
            model_type = params.get('model_type', 'ridge')
            result = calculate_model_importance(data, n_lags=n_lags, model_type=model_type)
        elif method == 'shap':
            max_samples = params.get('max_samples', 100)
            result = calculate_shap_importance(data, n_lags=n_lags, max_samples=max_samples)
        elif method == 'lag_analysis':
            max_lags = params.get('max_lags', 20)
            result = analyze_lag_importance(data, max_lags=max_lags)
        elif method == 'sensitivity':
            fh = params.get('horizon', 10)
            perturbation_pct = params.get('perturbation_pct', 0.05)
            freq = params.get('frequency', '1d')

            # Need a forecaster for sensitivity
            def simple_forecaster(y_train, fh, freq):
                last_value = y_train['value'].to_list()[-1]
                last_time = y_train['time'].to_list()[-1]
                delta = timedelta(days=1)
                forecast = []
                for i in range(1, fh + 1):
                    forecast.append({
                        'entity_id': y_train['entity_id'].to_list()[0],
                        'time': last_time + (delta * i),
                        'value': last_value
                    })
                return {'forecast': forecast}

            result = sensitivity_analysis(data, simple_forecaster, fh=fh,
                                         perturbation_pct=perturbation_pct, freq=freq)
        else:
            return {"success": False, "error": f"Unknown importance method: {method}"}

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# ADVANCED CROSS-VALIDATION COMMAND HANDLERS
# ============================================================================

def run_advanced_cv(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run advanced cross-validation methods"""
    if not ADVANCED_CV_AVAILABLE:
        return {"success": False, "error": "Advanced CV not available"}

    try:
        data = parse_panel_data_json(params['data'])
        method = params.get('method', 'walk_forward')

        if method == 'blocked':
            n_splits = params.get('n_splits', 5)
            test_size = params.get('test_size', 1)
            gap = params.get('gap', 0)
            result = blocked_time_series_cv(data, n_splits=n_splits, test_size=test_size, gap=gap)
        elif method == 'purged':
            n_splits = params.get('n_splits', 5)
            embargo_pct = params.get('embargo_pct', 0.01)
            purge_pct = params.get('purge_pct', 0.01)
            result = purged_kfold_cv(data, n_splits=n_splits, embargo_pct=embargo_pct, purge_pct=purge_pct)
        elif method == 'combinatorial':
            n_test_splits = params.get('n_test_splits', 2)
            n_groups = params.get('n_groups', 6)
            embargo_pct = params.get('embargo_pct', 0.01)
            result = combinatorial_purged_cv(data, n_test_splits=n_test_splits,
                                            n_groups=n_groups, embargo_pct=embargo_pct)
        elif method == 'walk_forward':
            initial_train_size = params.get('initial_train_size', 100)
            test_size = params.get('test_size', 1)
            step_size = params.get('step_size', 1)
            anchored = params.get('anchored', False)
            result = walk_forward_validation(data, initial_train_size=initial_train_size,
                                            test_size=test_size, step_size=step_size, anchored=anchored)
        elif method == 'nested':
            outer_splits = params.get('outer_splits', 5)
            inner_splits = params.get('inner_splits', 3)
            test_size = params.get('test_size', 1)
            result = nested_cv(data, outer_splits=outer_splits, inner_splits=inner_splits, test_size=test_size)
        else:
            return {"success": False, "error": f"Unknown CV method: {method}"}

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# BACKTESTING COMMAND HANDLERS
# ============================================================================

def run_backtest(params: Dict[str, Any]) -> Dict[str, Any]:
    """Run backtesting on time series"""
    if not BACKTESTING_AVAILABLE:
        return {"success": False, "error": "Backtesting not available"}

    try:
        data = parse_panel_data_json(params['data'])
        operation = params.get('operation', 'walk_forward')
        freq = params.get('frequency', '1d')

        # Create a simple forecaster
        def simple_forecaster(y_train, fh, freq):
            last_value = y_train['value'].to_list()[-1]
            last_time = y_train['time'].to_list()[-1]
            delta = timedelta(days=1)
            forecast = []
            for i in range(1, fh + 1):
                forecast.append({
                    'entity_id': y_train['entity_id'].to_list()[0],
                    'time': last_time + (delta * i),
                    'value': last_value
                })
            return {'forecast': forecast}

        if operation == 'walk_forward':
            initial_train_size = params.get('initial_train_size', 100)
            test_size = params.get('test_size', 1)
            step_size = params.get('step_size', 1)
            horizons = params.get('horizons')
            retrain = params.get('retrain', True)
            result = walk_forward_backtest(data, simple_forecaster, initial_train_size=initial_train_size,
                                          test_size=test_size, step_size=step_size,
                                          horizons=horizons, freq=freq, retrain=retrain)
        elif operation == 'compare_models':
            initial_train_size = params.get('initial_train_size', 100)
            test_size = params.get('test_size', 1)
            step_size = params.get('step_size', 5)
            # Use simple forecasters for comparison
            forecasters = {'naive': simple_forecaster}
            result = compare_models_backtest(data, forecasters, initial_train_size=initial_train_size,
                                            test_size=test_size, step_size=step_size, freq=freq)
        elif operation == 'rolling_origin':
            min_train_size = params.get('min_train_size', 50)
            max_train_size = params.get('max_train_size')
            test_size = params.get('test_size', 1)
            result = rolling_origin_evaluation(data, simple_forecaster, min_train_size=min_train_size,
                                              max_train_size=max_train_size, test_size=test_size, freq=freq)
        elif operation == 'attribution':
            backtest_results = params.get('backtest_results', {})
            result = performance_attribution(backtest_results)
        elif operation == 'summary':
            backtest_results = params.get('backtest_results', {})
            result = backtest_summary(backtest_results)
        else:
            return {"success": False, "error": f"Unknown backtest operation: {operation}"}

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

COMMANDS = {
    "check_status": lambda _: check_status(),
    "forecast": run_forecast,
    "preprocess": preprocess_data,
    "cross_validate": cross_validate,
    "metrics": calculate_metrics,
    "add_features": add_features,
    "seasonal_period": get_seasonal_period,
    "full_pipeline": full_forecast_pipeline,
    # Advanced models
    "advanced_forecast": run_advanced_forecast,
    # Ensemble methods
    "ensemble": run_ensemble,
    "auto_ensemble": run_auto_ensemble,
    # Anomaly detection
    "anomaly_detection": run_anomaly_detection,
    # Seasonality analysis
    "seasonality": run_seasonality_analysis,
    # Confidence intervals
    "confidence_intervals": run_confidence_intervals,
    # Feature importance
    "feature_importance": run_feature_importance,
    # Advanced cross-validation
    "advanced_cv": run_advanced_cv,
    # Backtesting
    "backtest": run_backtest,
}


def main(args: list) -> str:
    """Main entry point called by Rust via PyO3"""
    try:
        if len(args) < 1:
            return serialize_result({"success": False, "error": "No command specified"})

        command = args[0]

        if command not in COMMANDS:
            return serialize_result({"success": False, "error": f"Unknown command: {command}"})

        # Parse params if provided
        params = {}
        if len(args) > 1:
            params = json.loads(args[1])

        result = COMMANDS[command](params)
        return serialize_result(result)

    except Exception as e:
        return serialize_result({"success": False, "error": str(e)})


if __name__ == "__main__":
    output = main(sys.argv[1:])
    print(output)
