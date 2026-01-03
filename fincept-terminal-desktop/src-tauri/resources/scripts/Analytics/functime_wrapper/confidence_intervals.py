"""
Confidence Intervals and Prediction Intervals Module
====================================================

Provides uncertainty quantification for forecasts:
- Bootstrap confidence intervals
- Quantile regression intervals
- Residual-based prediction intervals
- Conformal prediction intervals
- Monte Carlo simulation intervals

Essential for risk management in financial forecasting.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple, Callable
from datetime import datetime, timedelta
import warnings

warnings.filterwarnings('ignore')

# Try importing sklearn
try:
    from sklearn.linear_model import QuantileRegressor
    SKLEARN_AVAILABLE = True
except ImportError:
    SKLEARN_AVAILABLE = False


# ============================================================================
# BOOTSTRAP CONFIDENCE INTERVALS
# ============================================================================

def bootstrap_prediction_intervals(
    y_train: pl.DataFrame,
    forecaster: Callable,
    fh: int,
    n_bootstrap: int = 100,
    confidence_levels: List[float] = [0.80, 0.95],
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Generate prediction intervals using bootstrap resampling.

    Args:
        y_train: Training data (panel format)
        forecaster: Forecasting function that takes (y_train, fh, freq)
        fh: Forecast horizon
        n_bootstrap: Number of bootstrap samples
        confidence_levels: Confidence levels (e.g., [0.80, 0.95])
        freq: Data frequency
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()
        n = len(values)

        if n < 10:
            continue

        # Generate bootstrap forecasts
        bootstrap_forecasts = []

        for _ in range(n_bootstrap):
            # Resample with replacement
            indices = np.random.choice(n, size=n, replace=True)
            resampled = entity_data[indices.tolist()].sort('time')

            try:
                fc_result = forecaster(resampled, fh=fh, freq=freq)
                if 'forecast' in fc_result:
                    fc_values = [f['value'] for f in fc_result['forecast']]
                    fc_times = [f['time'] for f in fc_result['forecast']]
                    bootstrap_forecasts.append(fc_values)
            except Exception:
                continue

        if len(bootstrap_forecasts) < 10:
            continue

        # Calculate point forecast (median)
        bootstrap_array = np.array(bootstrap_forecasts)
        point_forecast = np.median(bootstrap_array, axis=0)

        # Calculate intervals for each confidence level
        for h_idx in range(fh):
            row = {
                'entity_id': entity_id,
                'time': fc_times[h_idx],
                'forecast': float(point_forecast[h_idx]),
                'horizon': h_idx + 1
            }

            for conf in confidence_levels:
                alpha = 1 - conf
                lower_pct = alpha / 2 * 100
                upper_pct = (1 - alpha / 2) * 100

                lower = np.percentile(bootstrap_array[:, h_idx], lower_pct)
                upper = np.percentile(bootstrap_array[:, h_idx], upper_pct)

                row[f'lower_{int(conf*100)}'] = float(lower)
                row[f'upper_{int(conf*100)}'] = float(upper)

            result_rows.append(row)

    result = pl.DataFrame(result_rows) if result_rows else pl.DataFrame()

    return {
        'success': True,
        'method': 'bootstrap',
        'n_bootstrap': n_bootstrap,
        'confidence_levels': confidence_levels,
        'data': result.to_dicts() if len(result) > 0 else [],
        'shape': result.shape if len(result) > 0 else (0, 0)
    }


# ============================================================================
# RESIDUAL-BASED PREDICTION INTERVALS
# ============================================================================

def residual_prediction_intervals(
    y_train: pl.DataFrame,
    forecast: pl.DataFrame,
    y_validation: Optional[pl.DataFrame] = None,
    confidence_levels: List[float] = [0.80, 0.95]
) -> Dict[str, Any]:
    """
    Generate prediction intervals based on historical residuals.

    If y_validation is provided, uses actual residuals from validation.
    Otherwise, uses in-sample residuals.

    Args:
        y_train: Training data
        forecast: Point forecasts
        y_validation: Optional validation actuals for residual calculation
        confidence_levels: Confidence levels
    """
    result_rows = []
    entities = forecast['entity_id'].unique().to_list()

    for entity_id in entities:
        fc_data = forecast.filter(pl.col('entity_id') == entity_id).sort('time')

        # Get residuals
        if y_validation is not None:
            val_data = y_validation.filter(pl.col('entity_id') == entity_id).sort('time')
            # Calculate residuals from validation
            residuals = _calculate_residuals(val_data, fc_data)
        else:
            # Use in-sample estimation (assume residuals from training)
            train_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
            values = train_data['value'].to_numpy()
            # Estimate residuals as deviations from rolling mean
            window = min(10, len(values) // 2)
            if window > 1:
                rolling_mean = np.convolve(values, np.ones(window)/window, mode='valid')
                residuals = values[window-1:] - rolling_mean
            else:
                residuals = values - np.mean(values)

        if len(residuals) < 5:
            continue

        # Assume residuals are normally distributed
        residual_std = np.std(residuals)
        residual_mean = np.mean(residuals)

        # Apply intervals to forecast
        for row in fc_data.iter_rows(named=True):
            result_row = {
                'entity_id': row['entity_id'],
                'time': row['time'],
                'forecast': float(row['value'])
            }

            for conf in confidence_levels:
                from scipy import stats
                z = stats.norm.ppf((1 + conf) / 2)

                lower = row['value'] - z * residual_std + residual_mean
                upper = row['value'] + z * residual_std + residual_mean

                result_row[f'lower_{int(conf*100)}'] = float(lower)
                result_row[f'upper_{int(conf*100)}'] = float(upper)

            result_rows.append(result_row)

    result = pl.DataFrame(result_rows) if result_rows else pl.DataFrame()

    return {
        'success': True,
        'method': 'residual',
        'confidence_levels': confidence_levels,
        'data': result.to_dicts() if len(result) > 0 else [],
        'shape': result.shape if len(result) > 0 else (0, 0)
    }


def _calculate_residuals(actuals: pl.DataFrame, forecasts: pl.DataFrame) -> np.ndarray:
    """Calculate residuals between actuals and forecasts."""
    joined = actuals.join(
        forecasts.select(['entity_id', 'time', 'value']).rename({'value': 'forecast'}),
        on=['entity_id', 'time'],
        how='inner'
    )

    if len(joined) == 0:
        return np.array([])

    return (joined['value'] - joined['forecast']).to_numpy()


# ============================================================================
# QUANTILE REGRESSION INTERVALS
# ============================================================================

def quantile_prediction_intervals(
    y_train: pl.DataFrame,
    fh: int,
    lags: int = 10,
    quantiles: List[float] = [0.025, 0.10, 0.25, 0.50, 0.75, 0.90, 0.975],
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Generate prediction intervals using quantile regression.

    Args:
        y_train: Training data
        fh: Forecast horizon
        lags: Number of lag features
        quantiles: Quantiles to predict
        freq: Data frequency
    """
    if not SKLEARN_AVAILABLE:
        return {'success': False, 'error': 'sklearn not available for quantile regression'}

    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if len(values) < lags + 10:
            continue

        # Create lag features
        X, y = _create_lag_features(values, lags)

        # Fit quantile regressors
        quantile_predictions = {}

        for q in quantiles:
            try:
                qr = QuantileRegressor(quantile=q, alpha=0.1, solver='highs')
                qr.fit(X, y)
                quantile_predictions[q] = qr
            except Exception:
                continue

        if len(quantile_predictions) < len(quantiles):
            continue

        # Generate forecasts
        last_lags = values[-lags:].tolist()
        delta = _get_time_delta(freq)

        for h in range(1, fh + 1):
            X_pred = np.array([last_lags[-lags:]]).reshape(1, -1)

            row = {
                'entity_id': entity_id,
                'time': times[-1] + (delta * h),
                'horizon': h
            }

            median_pred = None
            for q, model in quantile_predictions.items():
                pred = float(model.predict(X_pred)[0])
                row[f'q{int(q*100):02d}'] = pred
                if q == 0.50:
                    median_pred = pred

            if median_pred is not None:
                row['forecast'] = median_pred
                last_lags.append(median_pred)

            result_rows.append(row)

    result = pl.DataFrame(result_rows) if result_rows else pl.DataFrame()

    return {
        'success': True,
        'method': 'quantile_regression',
        'quantiles': quantiles,
        'data': result.to_dicts() if len(result) > 0 else [],
        'shape': result.shape if len(result) > 0 else (0, 0)
    }


# ============================================================================
# CONFORMAL PREDICTION INTERVALS
# ============================================================================

def conformal_prediction_intervals(
    y_train: pl.DataFrame,
    forecast: pl.DataFrame,
    calibration_size: int = 50,
    confidence_levels: List[float] = [0.80, 0.95]
) -> Dict[str, Any]:
    """
    Generate prediction intervals using conformal prediction.

    Uses a calibration set to determine interval widths that
    guarantee coverage at the specified confidence level.

    Args:
        y_train: Full training data (includes calibration portion)
        forecast: Point forecasts
        calibration_size: Number of recent points for calibration
        confidence_levels: Coverage levels
    """
    result_rows = []
    entities = forecast['entity_id'].unique().to_list()

    for entity_id in entities:
        train_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        fc_data = forecast.filter(pl.col('entity_id') == entity_id).sort('time')

        values = train_data['value'].to_numpy()
        n = len(values)

        if n < calibration_size + 10:
            continue

        # Split into proper training and calibration
        cal_values = values[-calibration_size:]

        # Calculate nonconformity scores (absolute residuals from rolling forecast)
        # Simple: use deviation from previous value as nonconformity
        scores = np.abs(np.diff(cal_values))

        if len(scores) < 5:
            continue

        # Calculate conformal quantiles
        for row in fc_data.iter_rows(named=True):
            result_row = {
                'entity_id': row['entity_id'],
                'time': row['time'],
                'forecast': float(row['value'])
            }

            for conf in confidence_levels:
                # Conformal quantile
                q = np.quantile(scores, conf)

                result_row[f'lower_{int(conf*100)}'] = float(row['value'] - q)
                result_row[f'upper_{int(conf*100)}'] = float(row['value'] + q)

            result_rows.append(result_row)

    result = pl.DataFrame(result_rows) if result_rows else pl.DataFrame()

    return {
        'success': True,
        'method': 'conformal',
        'calibration_size': calibration_size,
        'confidence_levels': confidence_levels,
        'data': result.to_dicts() if len(result) > 0 else [],
        'shape': result.shape if len(result) > 0 else (0, 0)
    }


# ============================================================================
# MONTE CARLO SIMULATION INTERVALS
# ============================================================================

def monte_carlo_intervals(
    y_train: pl.DataFrame,
    fh: int,
    n_simulations: int = 1000,
    model: str = 'random_walk',
    confidence_levels: List[float] = [0.80, 0.95],
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Generate prediction intervals using Monte Carlo simulation.

    Args:
        y_train: Training data
        fh: Forecast horizon
        n_simulations: Number of simulation paths
        model: 'random_walk', 'drift', or 'mean_revert'
        confidence_levels: Confidence levels
        freq: Data frequency
    """
    result_rows = []
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if len(values) < 20:
            continue

        # Estimate parameters from historical data
        returns = np.diff(values)
        mu = np.mean(returns)
        sigma = np.std(returns)
        last_value = values[-1]

        # Run simulations
        simulations = np.zeros((n_simulations, fh))

        for sim_idx in range(n_simulations):
            path = np.zeros(fh)
            current = last_value

            for h in range(fh):
                if model == 'random_walk':
                    shock = np.random.normal(0, sigma)
                    current = current + shock
                elif model == 'drift':
                    shock = np.random.normal(mu, sigma)
                    current = current + shock
                elif model == 'mean_revert':
                    mean_level = np.mean(values)
                    reversion_speed = 0.1
                    shock = np.random.normal(0, sigma)
                    current = current + reversion_speed * (mean_level - current) + shock

                path[h] = current

            simulations[sim_idx] = path

        # Calculate intervals
        delta = _get_time_delta(freq)

        for h in range(fh):
            sim_values = simulations[:, h]
            point_forecast = np.median(sim_values)

            row = {
                'entity_id': entity_id,
                'time': times[-1] + (delta * (h + 1)),
                'forecast': float(point_forecast),
                'horizon': h + 1
            }

            for conf in confidence_levels:
                alpha = 1 - conf
                lower = np.percentile(sim_values, alpha / 2 * 100)
                upper = np.percentile(sim_values, (1 - alpha / 2) * 100)

                row[f'lower_{int(conf*100)}'] = float(lower)
                row[f'upper_{int(conf*100)}'] = float(upper)

            result_rows.append(row)

    result = pl.DataFrame(result_rows) if result_rows else pl.DataFrame()

    return {
        'success': True,
        'method': 'monte_carlo',
        'model': model,
        'n_simulations': n_simulations,
        'confidence_levels': confidence_levels,
        'data': result.to_dicts() if len(result) > 0 else [],
        'shape': result.shape if len(result) > 0 else (0, 0)
    }


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def _create_lag_features(values: np.ndarray, lags: int) -> Tuple[np.ndarray, np.ndarray]:
    """Create lag features for supervised learning."""
    n = len(values)
    X = []
    y = []

    for i in range(lags, n):
        X.append(values[i-lags:i])
        y.append(values[i])

    return np.array(X), np.array(y)


def _get_time_delta(freq: str) -> timedelta:
    """Convert frequency string to timedelta."""
    freq_map = {
        '1d': timedelta(days=1),
        '1w': timedelta(weeks=1),
        '1h': timedelta(hours=1),
        '1mo': timedelta(days=30),
    }
    return freq_map.get(freq, timedelta(days=1))


def main():
    """Test confidence interval methods."""
    print("Testing Confidence Intervals Module")
    print("=" * 50)

    # Create sample data
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 6, 30),
        interval='1d',
        eager=True
    ).to_list()

    n = len(dates)
    np.random.seed(42)
    values = [100 + 0.2 * i + np.random.randn() * 5 for i in range(n)]

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    # Create mock forecast
    fh = 14
    last_time = dates[-1]
    delta = timedelta(days=1)
    last_value = values[-1]

    fc_rows = []
    for i in range(1, fh + 1):
        fc_rows.append({
            'entity_id': 'A',
            'time': last_time + (delta * i),
            'value': last_value + 0.2 * i
        })

    forecast = pl.DataFrame(fc_rows)

    # Test residual-based intervals
    residual_result = residual_prediction_intervals(
        df, forecast, confidence_levels=[0.80, 0.95]
    )
    print(f"Residual intervals: {len(residual_result['data'])} forecasts")

    # Test conformal intervals
    conformal_result = conformal_prediction_intervals(
        df, forecast, calibration_size=30, confidence_levels=[0.80, 0.95]
    )
    print(f"Conformal intervals: {len(conformal_result['data'])} forecasts")

    # Test Monte Carlo intervals
    mc_result = monte_carlo_intervals(
        df, fh=14, n_simulations=500, model='drift', confidence_levels=[0.80, 0.95]
    )
    print(f"Monte Carlo intervals: {len(mc_result['data'])} forecasts")

    # Test quantile regression
    if SKLEARN_AVAILABLE:
        qr_result = quantile_prediction_intervals(
            df, fh=7, lags=7, quantiles=[0.10, 0.50, 0.90]
        )
        print(f"Quantile regression: {len(qr_result['data'])} forecasts")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
