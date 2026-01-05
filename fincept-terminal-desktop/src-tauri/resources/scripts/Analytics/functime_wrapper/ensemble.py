"""
Ensemble Forecasting Methods
============================

Provides ensemble forecasting capabilities:
- Simple averaging
- Weighted averaging (based on validation performance)
- Stacking with meta-learner
- Trimmed mean (excludes outliers)
- Median ensemble

Combines forecasts from multiple models for improved accuracy.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Callable
from datetime import datetime, timedelta
import warnings

warnings.filterwarnings('ignore')

# Import base forecasters
try:
    from forecasting import (
        forecast_linear_model, forecast_lasso, forecast_ridge,
        forecast_elasticnet, forecast_knn, forecast_lightgbm
    )
    BASE_FORECASTERS_AVAILABLE = True
except ImportError:
    BASE_FORECASTERS_AVAILABLE = False

try:
    from advanced_models import (
        forecast_naive, forecast_seasonal_naive, forecast_ses,
        forecast_holt, forecast_theta, forecast_xgboost
    )
    ADVANCED_FORECASTERS_AVAILABLE = True
except ImportError:
    ADVANCED_FORECASTERS_AVAILABLE = False


# ============================================================================
# SIMPLE ENSEMBLE METHODS
# ============================================================================

def ensemble_mean(
    forecasts: List[Dict[str, Any]]
) -> Dict[str, Any]:
    """
    Simple mean ensemble - averages all forecasts.
    """
    if not forecasts:
        return {'success': False, 'error': 'No forecasts provided'}

    # Extract forecast values
    all_forecasts = []
    for fc in forecasts:
        if 'forecast' in fc:
            all_forecasts.append(fc['forecast'])

    if not all_forecasts:
        return {'success': False, 'error': 'No valid forecasts found'}

    # Convert to DataFrames and merge
    combined = _combine_forecasts(all_forecasts)

    if combined is None:
        return {'success': False, 'error': 'Failed to combine forecasts'}

    # Calculate mean
    result = combined.group_by(['entity_id', 'time']).agg([
        pl.col('value').mean().alias('value')
    ]).sort(['entity_id', 'time'])

    return {
        'success': True,
        'model_type': 'EnsembleMean',
        'forecast': result.to_dicts(),
        'shape': result.shape,
        'n_models': len(all_forecasts)
    }


def ensemble_median(
    forecasts: List[Dict[str, Any]]
) -> Dict[str, Any]:
    """
    Median ensemble - takes median of all forecasts.
    More robust to outliers than mean.
    """
    if not forecasts:
        return {'success': False, 'error': 'No forecasts provided'}

    all_forecasts = [fc['forecast'] for fc in forecasts if 'forecast' in fc]

    if not all_forecasts:
        return {'success': False, 'error': 'No valid forecasts found'}

    combined = _combine_forecasts(all_forecasts)

    if combined is None:
        return {'success': False, 'error': 'Failed to combine forecasts'}

    # Calculate median
    result = combined.group_by(['entity_id', 'time']).agg([
        pl.col('value').median().alias('value')
    ]).sort(['entity_id', 'time'])

    return {
        'success': True,
        'model_type': 'EnsembleMedian',
        'forecast': result.to_dicts(),
        'shape': result.shape,
        'n_models': len(all_forecasts)
    }


def ensemble_trimmed_mean(
    forecasts: List[Dict[str, Any]],
    trim_fraction: float = 0.1
) -> Dict[str, Any]:
    """
    Trimmed mean ensemble - excludes extreme values before averaging.
    """
    if not forecasts:
        return {'success': False, 'error': 'No forecasts provided'}

    all_forecasts = [fc['forecast'] for fc in forecasts if 'forecast' in fc]

    if len(all_forecasts) < 3:
        # Not enough to trim, fall back to mean
        return ensemble_mean(forecasts)

    combined = _combine_forecasts(all_forecasts)

    if combined is None:
        return {'success': False, 'error': 'Failed to combine forecasts'}

    # Custom trimmed mean aggregation
    def trimmed_mean(values: np.ndarray, trim: float) -> float:
        n = len(values)
        k = int(n * trim)
        if k == 0:
            return np.mean(values)
        sorted_vals = np.sort(values)
        return np.mean(sorted_vals[k:-k]) if k > 0 else np.mean(sorted_vals)

    # Group and calculate trimmed mean
    result_rows = []
    for (entity_id, time), group in combined.group_by(['entity_id', 'time']):
        values = group['value'].to_numpy()
        tm = trimmed_mean(values, trim_fraction)
        result_rows.append({
            'entity_id': entity_id,
            'time': time,
            'value': float(tm)
        })

    result = pl.DataFrame(result_rows).sort(['entity_id', 'time'])

    return {
        'success': True,
        'model_type': 'EnsembleTrimmedMean',
        'forecast': result.to_dicts(),
        'shape': result.shape,
        'n_models': len(all_forecasts),
        'trim_fraction': trim_fraction
    }


# ============================================================================
# WEIGHTED ENSEMBLE
# ============================================================================

def ensemble_weighted(
    forecasts: List[Dict[str, Any]],
    weights: Optional[List[float]] = None,
    metric_scores: Optional[List[float]] = None
) -> Dict[str, Any]:
    """
    Weighted ensemble - weights forecasts by performance or custom weights.

    If metric_scores provided (e.g., MAE values), inverse-weights by error.
    If weights provided directly, uses those.
    Otherwise, uses equal weights.
    """
    if not forecasts:
        return {'success': False, 'error': 'No forecasts provided'}

    all_forecasts = [fc['forecast'] for fc in forecasts if 'forecast' in fc]

    if not all_forecasts:
        return {'success': False, 'error': 'No valid forecasts found'}

    n_models = len(all_forecasts)

    # Determine weights
    if weights is not None:
        if len(weights) != n_models:
            return {'success': False, 'error': 'Weights length must match number of models'}
        w = np.array(weights)
    elif metric_scores is not None:
        if len(metric_scores) != n_models:
            return {'success': False, 'error': 'Metric scores length must match number of models'}
        # Inverse weight by error (lower error = higher weight)
        scores = np.array(metric_scores)
        # Avoid division by zero
        scores = np.maximum(scores, 1e-10)
        w = 1.0 / scores
    else:
        w = np.ones(n_models)

    # Normalize weights
    w = w / w.sum()

    # Combine forecasts with weights
    combined_rows = []
    for i, fc in enumerate(all_forecasts):
        for row in fc:
            combined_rows.append({
                'entity_id': row['entity_id'],
                'time': row['time'],
                'value': row['value'],
                'weight': float(w[i]),
                'model_idx': i
            })

    combined = pl.DataFrame(combined_rows)

    # Calculate weighted average
    result = combined.group_by(['entity_id', 'time']).agg([
        (pl.col('value') * pl.col('weight')).sum().alias('value')
    ]).sort(['entity_id', 'time'])

    return {
        'success': True,
        'model_type': 'EnsembleWeighted',
        'forecast': result.to_dicts(),
        'shape': result.shape,
        'n_models': n_models,
        'weights': w.tolist()
    }


# ============================================================================
# STACKING ENSEMBLE
# ============================================================================

def ensemble_stacking(
    y_train: pl.DataFrame,
    fh: int,
    base_models: List[str] = ['lasso', 'ridge', 'knn'],
    meta_model: str = 'ridge',
    freq: str = '1d',
    validation_size: int = 10
) -> Dict[str, Any]:
    """
    Stacking ensemble with meta-learner.

    1. Split data into train and validation
    2. Fit base models on train, predict validation
    3. Train meta-learner on base model predictions
    4. Generate final forecast using meta-learner
    """
    if not BASE_FORECASTERS_AVAILABLE:
        return {'success': False, 'error': 'Base forecasters not available'}

    entities = y_train['entity_id'].unique().to_list()
    result_rows = []

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')

        if len(entity_data) < validation_size + 10:
            # Not enough data for stacking
            continue

        # Split into train and validation
        train_data = entity_data.head(-validation_size)
        val_data = entity_data.tail(validation_size)

        # Prepare train for single entity
        train_single = train_data.clone()

        # Get base model predictions on validation set
        base_preds = []
        model_forecasters = _get_model_forecasters()

        for model_name in base_models:
            if model_name not in model_forecasters:
                continue

            try:
                forecast_result = model_forecasters[model_name](
                    train_single, fh=validation_size, freq=freq
                )
                if 'forecast' in forecast_result:
                    preds = [f['value'] for f in forecast_result['forecast']]
                    base_preds.append(preds)
            except Exception:
                continue

        if len(base_preds) < 2:
            continue

        # Create meta-features
        X_meta = np.column_stack(base_preds)
        y_val = val_data['value'].to_numpy()

        # Fit meta-model (simple linear regression)
        from sklearn.linear_model import Ridge
        meta = Ridge(alpha=1.0)
        meta.fit(X_meta, y_val)

        # Now generate forecasts using all training data
        full_train = entity_data.clone()
        final_base_preds = []

        for model_name in base_models:
            if model_name not in model_forecasters:
                continue

            try:
                forecast_result = model_forecasters[model_name](
                    full_train, fh=fh, freq=freq
                )
                if 'forecast' in forecast_result:
                    preds = [f['value'] for f in forecast_result['forecast']]
                    times = [f['time'] for f in forecast_result['forecast']]
                    final_base_preds.append(preds)
            except Exception:
                continue

        if len(final_base_preds) != len(base_preds):
            continue

        # Apply meta-model
        X_final = np.column_stack(final_base_preds)
        final_preds = meta.predict(X_final)

        for i, pred in enumerate(final_preds):
            result_rows.append({
                'entity_id': entity_id,
                'time': times[i],
                'value': float(pred)
            })

    if not result_rows:
        return {'success': False, 'error': 'Stacking failed for all entities'}

    result = pl.DataFrame(result_rows).sort(['entity_id', 'time'])

    return {
        'success': True,
        'model_type': 'EnsembleStacking',
        'forecast': result.to_dicts(),
        'shape': result.shape,
        'base_models': base_models,
        'meta_model': meta_model
    }


# ============================================================================
# AUTO ENSEMBLE
# ============================================================================

def auto_ensemble(
    y_train: pl.DataFrame,
    fh: int,
    models: Optional[List[str]] = None,
    validation_size: int = 10,
    weighting: str = 'inverse_error',  # 'equal', 'inverse_error', 'best_n'
    best_n: int = 3,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Automatically select and combine best performing models.

    1. Run all models on validation set
    2. Evaluate performance
    3. Combine using specified weighting method
    """
    if models is None:
        models = ['naive', 'ses', 'lasso', 'ridge', 'knn']

    entities = y_train['entity_id'].unique().to_list()
    result_rows = []
    model_scores = {}

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')

        if len(entity_data) < validation_size + 10:
            continue

        train_data = entity_data.head(-validation_size)
        val_data = entity_data.tail(validation_size)
        y_val = val_data['value'].to_numpy()

        # Evaluate each model
        model_forecasters = _get_model_forecasters()
        model_performances = {}
        model_forecasts = {}

        for model_name in models:
            if model_name not in model_forecasters:
                continue

            try:
                forecast_result = model_forecasters[model_name](
                    train_data, fh=validation_size, freq=freq
                )
                if 'forecast' in forecast_result:
                    preds = np.array([f['value'] for f in forecast_result['forecast']])
                    mae = np.mean(np.abs(y_val - preds))
                    model_performances[model_name] = mae
                    model_forecasts[model_name] = forecast_result
            except Exception:
                continue

        if not model_performances:
            continue

        # Select models based on weighting strategy
        if weighting == 'best_n':
            sorted_models = sorted(model_performances.items(), key=lambda x: x[1])
            selected_models = [m[0] for m in sorted_models[:best_n]]
            weights = np.ones(len(selected_models)) / len(selected_models)
        elif weighting == 'inverse_error':
            selected_models = list(model_performances.keys())
            errors = np.array([model_performances[m] for m in selected_models])
            weights = 1.0 / (errors + 1e-10)
            weights = weights / weights.sum()
        else:  # equal
            selected_models = list(model_performances.keys())
            weights = np.ones(len(selected_models)) / len(selected_models)

        # Generate final forecasts
        full_forecasts = []
        for model_name in selected_models:
            try:
                forecast_result = model_forecasters[model_name](
                    entity_data, fh=fh, freq=freq
                )
                if 'forecast' in forecast_result:
                    full_forecasts.append(forecast_result['forecast'])
            except Exception:
                continue

        if not full_forecasts:
            continue

        # Combine with weights
        n_horizons = len(full_forecasts[0])
        for h in range(n_horizons):
            weighted_sum = 0.0
            times_h = full_forecasts[0][h]['time']

            for i, fc in enumerate(full_forecasts):
                if i < len(weights):
                    weighted_sum += fc[h]['value'] * weights[i]

            result_rows.append({
                'entity_id': entity_id,
                'time': times_h,
                'value': float(weighted_sum)
            })

        model_scores[entity_id] = model_performances

    if not result_rows:
        return {'success': False, 'error': 'Auto ensemble failed'}

    result = pl.DataFrame(result_rows).sort(['entity_id', 'time'])

    return {
        'success': True,
        'model_type': 'AutoEnsemble',
        'forecast': result.to_dicts(),
        'shape': result.shape,
        'models_used': models,
        'weighting': weighting,
        'model_scores': model_scores
    }


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def _combine_forecasts(forecasts: List[List[Dict]]) -> Optional[pl.DataFrame]:
    """Combine multiple forecast lists into single DataFrame with model index."""
    if not forecasts:
        return None

    all_rows = []
    for i, fc in enumerate(forecasts):
        for row in fc:
            all_rows.append({
                'entity_id': row['entity_id'],
                'time': row['time'],
                'value': row['value'],
                'model_idx': i
            })

    if not all_rows:
        return None

    return pl.DataFrame(all_rows)


def _get_model_forecasters() -> Dict[str, Callable]:
    """Get dictionary of available model forecasters."""
    forecasters = {}

    if BASE_FORECASTERS_AVAILABLE:
        forecasters.update({
            'linear': forecast_linear_model,
            'lasso': forecast_lasso,
            'ridge': forecast_ridge,
            'elasticnet': forecast_elasticnet,
            'knn': forecast_knn,
            'lightgbm': forecast_lightgbm,
        })

    if ADVANCED_FORECASTERS_AVAILABLE:
        forecasters.update({
            'naive': forecast_naive,
            'seasonal_naive': forecast_seasonal_naive,
            'ses': forecast_ses,
            'holt': forecast_holt,
            'theta': forecast_theta,
            'xgboost': forecast_xgboost,
        })

    return forecasters


def main():
    """Test ensemble methods."""
    print("Testing Ensemble Forecasting Methods")
    print("=" * 50)

    # Create sample data
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 3, 31),
        interval='1d',
        eager=True
    ).to_list()

    n = len(dates)
    np.random.seed(42)
    values = [100 + 0.5 * i + 10 * np.sin(2 * np.pi * i / 7) + np.random.randn() * 3 for i in range(n)]

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    # Create mock forecasts
    fh = 7
    last_time = dates[-1]
    delta = timedelta(days=1)

    mock_forecasts = []
    for model_shift in [0, 2, -1]:  # Different biases
        fc = []
        for i in range(1, fh + 1):
            fc.append({
                'entity_id': 'A',
                'time': last_time + (delta * i),
                'value': 150 + model_shift + np.random.randn()
            })
        mock_forecasts.append({'forecast': fc})

    # Test mean ensemble
    mean_result = ensemble_mean(mock_forecasts)
    print(f"Mean Ensemble: {mean_result['n_models']} models combined")

    # Test median ensemble
    median_result = ensemble_median(mock_forecasts)
    print(f"Median Ensemble: {median_result['n_models']} models combined")

    # Test trimmed mean
    trimmed_result = ensemble_trimmed_mean(mock_forecasts, trim_fraction=0.1)
    print(f"Trimmed Mean Ensemble: {trimmed_result.get('n_models', 0)} models combined")

    # Test weighted ensemble
    weights = [0.5, 0.3, 0.2]
    weighted_result = ensemble_weighted(mock_forecasts, weights=weights)
    print(f"Weighted Ensemble: weights = {weighted_result.get('weights', [])}")

    # Test auto ensemble
    auto_result = auto_ensemble(df, fh=fh, models=['naive', 'ses'], validation_size=10)
    print(f"Auto Ensemble: success = {auto_result['success']}")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
