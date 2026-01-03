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

        # Simple train/test split by taking last test_size rows per entity
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

        # Run forecast
        forecast_result = run_forecast({
            'data': serialize_to_json(train_data.to_dicts()),
            'model': model_type,
            'horizon': fh,
            'frequency': freq
        })

        # Calculate metrics if we have test data
        metrics = None
        if test_data is not None and len(test_data) > 0 and forecast_result.get('success'):
            try:
                forecast_list = forecast_result.get('forecast', [])
                if len(forecast_list) > 0:
                    # Simple MAE/RMSE calculation
                    y_true_values = test_data['value'].to_numpy()
                    y_pred_values = np.array([f['value'] for f in forecast_list[:len(y_true_values)]])

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
