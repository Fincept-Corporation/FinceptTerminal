"""
Advanced Forecasting Models for Functime Wrapper
=================================================

Provides additional forecasting models beyond the base functime models:
- Naive/Seasonal Naive (baseline models)
- XGBoost forecasting
- Theta Method
- Croston's method for intermittent demand
- Simple Exponential Smoothing

Uses sklearn/numpy implementations for compatibility.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
from datetime import datetime, timedelta
import warnings

warnings.filterwarnings('ignore')

# Try importing XGBoost
try:
    import xgboost as xgb
    XGBOOST_AVAILABLE = True
except ImportError:
    XGBOOST_AVAILABLE = False

# Try importing CatBoost
try:
    import catboost as cb
    CATBOOST_AVAILABLE = True
except ImportError:
    CATBOOST_AVAILABLE = False


# ============================================================================
# NAIVE MODELS
# ============================================================================

def forecast_naive(
    y_train: pl.DataFrame,
    fh: int,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Naive forecast - uses last value as forecast for all horizons.
    Good baseline model for comparison.
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        last_value = entity_data['value'].to_list()[-1]
        last_time = entity_data['time'].to_list()[-1]

        # Generate future times
        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = last_time + (delta * i)
            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(last_value)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'Naive',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh
    }


def forecast_seasonal_naive(
    y_train: pl.DataFrame,
    fh: int,
    sp: int = 7,  # Seasonal period (7 for weekly, 12 for monthly, etc.)
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Seasonal Naive forecast - uses value from same season in previous period.
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_list()
        times = entity_data['time'].to_list()
        last_time = times[-1]

        # Generate future times
        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = last_time + (delta * i)
            # Get value from sp periods ago (with wraparound)
            seasonal_idx = len(values) - sp + ((i - 1) % sp)
            if seasonal_idx < 0:
                seasonal_idx = len(values) - 1  # Fallback to last value
            forecast_value = values[seasonal_idx]

            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(forecast_value)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'SeasonalNaive',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'seasonal_period': sp
    }


def forecast_drift(
    y_train: pl.DataFrame,
    fh: int,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Drift method - linear extrapolation from first to last value.
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_list()
        times = entity_data['time'].to_list()

        first_value = values[0]
        last_value = values[-1]
        n = len(values)

        # Calculate drift
        drift = (last_value - first_value) / (n - 1) if n > 1 else 0

        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = times[-1] + (delta * i)
            forecast_value = last_value + (drift * i)

            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(forecast_value)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'Drift',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh
    }


# ============================================================================
# EXPONENTIAL SMOOTHING
# ============================================================================

def forecast_ses(
    y_train: pl.DataFrame,
    fh: int,
    alpha: float = 0.3,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Simple Exponential Smoothing (SES).
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = np.array(entity_data['value'].to_list())
        times = entity_data['time'].to_list()

        # Fit SES
        level = values[0]
        for v in values[1:]:
            level = alpha * v + (1 - alpha) * level

        # Forecast is constant (the final level)
        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = times[-1] + (delta * i)
            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(level)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'SES',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'alpha': alpha
    }


def forecast_holt(
    y_train: pl.DataFrame,
    fh: int,
    alpha: float = 0.3,
    beta: float = 0.1,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Holt's Linear Trend Method (Double Exponential Smoothing).
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = np.array(entity_data['value'].to_list())
        times = entity_data['time'].to_list()

        # Initialize
        level = values[0]
        trend = values[1] - values[0] if len(values) > 1 else 0

        # Fit
        for v in values[1:]:
            prev_level = level
            level = alpha * v + (1 - alpha) * (level + trend)
            trend = beta * (level - prev_level) + (1 - beta) * trend

        # Forecast
        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = times[-1] + (delta * i)
            forecast_value = level + (trend * i)
            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(forecast_value)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'Holt',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'alpha': alpha,
        'beta': beta
    }


# ============================================================================
# THETA METHOD
# ============================================================================

def forecast_theta(
    y_train: pl.DataFrame,
    fh: int,
    theta: float = 2.0,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Theta Method - decomposes series and applies SES to theta-line.
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = np.array(entity_data['value'].to_list())
        times = entity_data['time'].to_list()
        n = len(values)

        # Decompose into trend and residual
        # Theta line: z_t = theta * y_t - (theta - 1) * trend_t
        x = np.arange(n)
        slope, intercept = np.polyfit(x, values, 1)
        trend = intercept + slope * x

        # Apply theta transformation
        theta_line = theta * values - (theta - 1) * trend

        # Apply SES to theta line
        alpha = 0.3
        level = theta_line[0]
        for v in theta_line[1:]:
            level = alpha * v + (1 - alpha) * level

        # Forecast
        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = times[-1] + (delta * i)
            future_x = n + i - 1
            future_trend = intercept + slope * future_x
            # Invert theta transformation
            forecast_value = (level + (theta - 1) * future_trend) / theta
            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(forecast_value)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'Theta',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'theta': theta
    }


# ============================================================================
# CROSTON'S METHOD (Intermittent Demand)
# ============================================================================

def forecast_croston(
    y_train: pl.DataFrame,
    fh: int,
    alpha: float = 0.3,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Croston's method for intermittent demand forecasting.
    Separates demand size from demand occurrence.
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = np.array(entity_data['value'].to_list())
        times = entity_data['time'].to_list()

        # Separate demand and intervals
        demand_times = []
        demand_sizes = []
        last_demand_idx = -1

        for i, v in enumerate(values):
            if v > 0:
                demand_sizes.append(v)
                if last_demand_idx >= 0:
                    demand_times.append(i - last_demand_idx)
                else:
                    demand_times.append(1)
                last_demand_idx = i

        if len(demand_sizes) == 0:
            # No demand - forecast zero
            forecast_value = 0.0
        elif len(demand_sizes) == 1:
            # Single demand - use that value
            forecast_value = demand_sizes[0]
        else:
            # Smooth demand sizes
            z = demand_sizes[0]
            for d in demand_sizes[1:]:
                z = alpha * d + (1 - alpha) * z

            # Smooth intervals
            p = demand_times[0]
            for t in demand_times[1:]:
                p = alpha * t + (1 - alpha) * p

            # Forecast = demand / interval
            forecast_value = z / p if p > 0 else z

        # Generate forecasts
        delta = _get_time_delta(freq)
        for i in range(1, fh + 1):
            future_time = times[-1] + (delta * i)
            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': float(forecast_value)
            })

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'Croston',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'alpha': alpha
    }


# ============================================================================
# XGBOOST FORECASTING
# ============================================================================

def forecast_xgboost(
    y_train: pl.DataFrame,
    fh: int,
    lags: int = 10,
    freq: str = '1d',
    **xgb_params
) -> Dict[str, Any]:
    """
    XGBoost forecaster with lag features.
    """
    if not XGBOOST_AVAILABLE:
        return {
            'success': False,
            'error': 'XGBoost not installed. Run: pip install xgboost'
        }

    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    default_params = {
        'n_estimators': 100,
        'max_depth': 5,
        'learning_rate': 0.1,
        'objective': 'reg:squarederror'
    }
    default_params.update(xgb_params)

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = np.array(entity_data['value'].to_list())
        times = entity_data['time'].to_list()

        # Create lag features
        X, y = _create_lag_features(values, lags)

        if len(X) < 10:
            # Not enough data, fall back to naive
            forecast_value = values[-1]
            delta = _get_time_delta(freq)
            for i in range(1, fh + 1):
                future_time = times[-1] + (delta * i)
                result_rows.append({
                    'entity_id': entity_id,
                    'time': future_time,
                    'value': float(forecast_value)
                })
            continue

        # Fit XGBoost
        model = xgb.XGBRegressor(**default_params)
        model.fit(X, y)

        # Recursive forecasting
        last_lags = values[-lags:].tolist()
        delta = _get_time_delta(freq)

        for i in range(1, fh + 1):
            X_pred = np.array([last_lags[-lags:]]).reshape(1, -1)
            pred = float(model.predict(X_pred)[0])
            future_time = times[-1] + (delta * i)

            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': pred
            })

            # Update lags for next prediction
            last_lags.append(pred)

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'XGBoost',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'lags': lags,
        'params': default_params
    }


def forecast_catboost(
    y_train: pl.DataFrame,
    fh: int,
    lags: int = 10,
    freq: str = '1d',
    **cb_params
) -> Dict[str, Any]:
    """
    CatBoost forecaster with lag features.
    """
    if not CATBOOST_AVAILABLE:
        return {
            'success': False,
            'error': 'CatBoost not installed. Run: pip install catboost'
        }

    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    default_params = {
        'iterations': 100,
        'depth': 5,
        'learning_rate': 0.1,
        'loss_function': 'RMSE',
        'verbose': False
    }
    default_params.update(cb_params)

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = np.array(entity_data['value'].to_list())
        times = entity_data['time'].to_list()

        # Create lag features
        X, y = _create_lag_features(values, lags)

        if len(X) < 10:
            forecast_value = values[-1]
            delta = _get_time_delta(freq)
            for i in range(1, fh + 1):
                future_time = times[-1] + (delta * i)
                result_rows.append({
                    'entity_id': entity_id,
                    'time': future_time,
                    'value': float(forecast_value)
                })
            continue

        # Fit CatBoost
        model = cb.CatBoostRegressor(**default_params)
        model.fit(X, y)

        # Recursive forecasting
        last_lags = values[-lags:].tolist()
        delta = _get_time_delta(freq)

        for i in range(1, fh + 1):
            X_pred = np.array([last_lags[-lags:]]).reshape(1, -1)
            pred = float(model.predict(X_pred)[0])
            future_time = times[-1] + (delta * i)

            result_rows.append({
                'entity_id': entity_id,
                'time': future_time,
                'value': pred
            })

            last_lags.append(pred)

    forecast = pl.DataFrame(result_rows)

    return {
        'model_type': 'CatBoost',
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'lags': lags
    }


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def _get_time_delta(freq: str) -> timedelta:
    """Convert frequency string to timedelta."""
    freq_map = {
        '1d': timedelta(days=1),
        '1w': timedelta(weeks=1),
        '1h': timedelta(hours=1),
        '30m': timedelta(minutes=30),
        '15m': timedelta(minutes=15),
        '1m': timedelta(minutes=1),
        '1mo': timedelta(days=30),
        '1q': timedelta(days=91),
        '1y': timedelta(days=365),
    }
    return freq_map.get(freq, timedelta(days=1))


def _create_lag_features(values: np.ndarray, lags: int) -> Tuple[np.ndarray, np.ndarray]:
    """Create lag features for supervised learning."""
    n = len(values)
    X = []
    y = []

    for i in range(lags, n):
        X.append(values[i-lags:i])
        y.append(values[i])

    return np.array(X), np.array(y)


def get_available_models() -> List[Dict[str, str]]:
    """Return list of available advanced models."""
    models = [
        {'id': 'naive', 'name': 'Naive', 'description': 'Last value forecast (baseline)'},
        {'id': 'seasonal_naive', 'name': 'Seasonal Naive', 'description': 'Seasonal last value'},
        {'id': 'drift', 'name': 'Drift', 'description': 'Linear extrapolation'},
        {'id': 'ses', 'name': 'SES', 'description': 'Simple Exponential Smoothing'},
        {'id': 'holt', 'name': 'Holt', 'description': 'Double Exponential Smoothing'},
        {'id': 'theta', 'name': 'Theta', 'description': 'Theta method'},
        {'id': 'croston', 'name': 'Croston', 'description': 'Intermittent demand'},
    ]

    if XGBOOST_AVAILABLE:
        models.append({'id': 'xgboost', 'name': 'XGBoost', 'description': 'Gradient boosting'})

    if CATBOOST_AVAILABLE:
        models.append({'id': 'catboost', 'name': 'CatBoost', 'description': 'Categorical boosting'})

    return models


# ============================================================================
# ALIASES FOR COMPATIBILITY
# ============================================================================

# These aliases match the import names expected by functime_service.py
naive_forecast = forecast_naive
seasonal_naive_forecast = forecast_seasonal_naive
drift_forecast = forecast_drift
ses_forecast = forecast_ses
holt_forecast = forecast_holt
theta_forecast = forecast_theta
croston_forecast = forecast_croston
xgboost_forecast = forecast_xgboost
catboost_forecast = forecast_catboost


def main():
    """Test advanced models."""
    print("Testing Advanced Forecasting Models")
    print("=" * 50)

    # Create sample data
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 3, 31),
        interval='1d',
        eager=True
    ).to_list()

    # Generate synthetic data with trend and seasonality
    n = len(dates)
    np.random.seed(42)
    values = [100 + 0.5 * i + 10 * np.sin(2 * np.pi * i / 7) + np.random.randn() * 3 for i in range(n)]

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    fh = 7

    # Test Naive
    naive_result = forecast_naive(df, fh=fh)
    print(f"Naive: {len(naive_result['forecast'])} predictions")

    # Test Seasonal Naive
    snaive_result = forecast_seasonal_naive(df, fh=fh, sp=7)
    print(f"Seasonal Naive: {len(snaive_result['forecast'])} predictions")

    # Test SES
    ses_result = forecast_ses(df, fh=fh, alpha=0.3)
    print(f"SES: {len(ses_result['forecast'])} predictions")

    # Test Holt
    holt_result = forecast_holt(df, fh=fh, alpha=0.3, beta=0.1)
    print(f"Holt: {len(holt_result['forecast'])} predictions")

    # Test Theta
    theta_result = forecast_theta(df, fh=fh, theta=2.0)
    print(f"Theta: {len(theta_result['forecast'])} predictions")

    # Test Croston (with some zeros)
    df_intermittent = df.with_columns([
        pl.when(pl.col('value') < 105).then(0).otherwise(pl.col('value')).alias('value')
    ])
    croston_result = forecast_croston(df_intermittent, fh=fh)
    print(f"Croston: {len(croston_result['forecast'])} predictions")

    # Test XGBoost if available
    if XGBOOST_AVAILABLE:
        xgb_result = forecast_xgboost(df, fh=fh, lags=7)
        print(f"XGBoost: {len(xgb_result['forecast'])} predictions")

    print("\nAvailable models:", [m['id'] for m in get_available_models()])
    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
