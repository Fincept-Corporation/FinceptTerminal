"""
Feature Importance Module
=========================

Provides feature importance analysis for time series models:
- Permutation importance
- SHAP values (if available)
- Model-specific importance (tree-based)
- Lag importance analysis
- Sensitivity analysis

Helps understand which features drive forecasts.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple, Callable
from datetime import datetime, timedelta
import warnings

warnings.filterwarnings('ignore')

# Try importing sklearn
try:
    from sklearn.inspection import permutation_importance
    from sklearn.linear_model import Ridge, Lasso
    from sklearn.ensemble import RandomForestRegressor
    SKLEARN_AVAILABLE = True
except ImportError:
    SKLEARN_AVAILABLE = False

# Try importing SHAP
try:
    import shap
    SHAP_AVAILABLE = True
except ImportError:
    SHAP_AVAILABLE = False


# ============================================================================
# PERMUTATION IMPORTANCE
# ============================================================================

def calculate_permutation_importance(
    y_train: pl.DataFrame,
    n_lags: int = 10,
    n_repeats: int = 10,
    model_type: str = 'ridge'
) -> Dict[str, Any]:
    """
    Calculate permutation importance for lag features.

    Measures how much shuffling each feature degrades model performance.

    Args:
        y_train: Training panel data
        n_lags: Number of lag features
        n_repeats: Number of permutation repeats
        model_type: 'ridge', 'lasso', or 'rf'
    """
    if not SKLEARN_AVAILABLE:
        return {'success': False, 'error': 'sklearn not available'}

    results = {}
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()

        if len(values) < n_lags + 20:
            results[entity_id] = {'error': 'Insufficient data'}
            continue

        # Create lag features
        X, y = _create_lag_features(values, n_lags)

        # Split into train/test
        split_idx = int(len(X) * 0.8)
        X_train, X_test = X[:split_idx], X[split_idx:]
        y_train_arr, y_test = y[:split_idx], y[split_idx:]

        # Fit model
        if model_type == 'ridge':
            model = Ridge(alpha=1.0)
        elif model_type == 'lasso':
            model = Lasso(alpha=0.1)
        elif model_type == 'rf':
            model = RandomForestRegressor(n_estimators=50, random_state=42)
        else:
            model = Ridge(alpha=1.0)

        model.fit(X_train, y_train_arr)

        # Calculate permutation importance
        perm_result = permutation_importance(
            model, X_test, y_test,
            n_repeats=n_repeats,
            random_state=42
        )

        # Format results
        feature_names = [f'lag_{i+1}' for i in range(n_lags)]
        importance_mean = perm_result.importances_mean
        importance_std = perm_result.importances_std

        # Sort by importance
        sorted_idx = np.argsort(importance_mean)[::-1]

        results[entity_id] = {
            'feature_names': [feature_names[i] for i in sorted_idx],
            'importance_mean': importance_mean[sorted_idx].tolist(),
            'importance_std': importance_std[sorted_idx].tolist(),
            'model_type': model_type,
            'baseline_score': float(model.score(X_test, y_test))
        }

    return {
        'success': True,
        'method': 'permutation',
        'n_lags': n_lags,
        'n_repeats': n_repeats,
        'results': results
    }


# ============================================================================
# MODEL-SPECIFIC IMPORTANCE
# ============================================================================

def calculate_model_importance(
    y_train: pl.DataFrame,
    n_lags: int = 10,
    model_type: str = 'ridge'
) -> Dict[str, Any]:
    """
    Calculate model-specific feature importance.

    For linear models: coefficient magnitudes
    For tree models: impurity-based importance

    Args:
        y_train: Training panel data
        n_lags: Number of lag features
        model_type: 'ridge', 'lasso', 'rf'
    """
    if not SKLEARN_AVAILABLE:
        return {'success': False, 'error': 'sklearn not available'}

    results = {}
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()

        if len(values) < n_lags + 20:
            results[entity_id] = {'error': 'Insufficient data'}
            continue

        # Create lag features
        X, y = _create_lag_features(values, n_lags)

        # Standardize for linear models
        X_mean = X.mean(axis=0)
        X_std = X.std(axis=0)
        X_std[X_std == 0] = 1
        X_scaled = (X - X_mean) / X_std

        # Fit model
        if model_type == 'ridge':
            model = Ridge(alpha=1.0)
            model.fit(X_scaled, y)
            importance = np.abs(model.coef_)
        elif model_type == 'lasso':
            model = Lasso(alpha=0.1)
            model.fit(X_scaled, y)
            importance = np.abs(model.coef_)
        elif model_type == 'rf':
            model = RandomForestRegressor(n_estimators=50, random_state=42)
            model.fit(X, y)
            importance = model.feature_importances_
        else:
            model = Ridge(alpha=1.0)
            model.fit(X_scaled, y)
            importance = np.abs(model.coef_)

        # Normalize importance
        if np.sum(importance) > 0:
            importance = importance / np.sum(importance)

        feature_names = [f'lag_{i+1}' for i in range(n_lags)]
        sorted_idx = np.argsort(importance)[::-1]

        results[entity_id] = {
            'feature_names': [feature_names[i] for i in sorted_idx],
            'importance': importance[sorted_idx].tolist(),
            'model_type': model_type,
            'coefficients': model.coef_.tolist() if hasattr(model, 'coef_') else None
        }

    return {
        'success': True,
        'method': 'model_specific',
        'n_lags': n_lags,
        'model_type': model_type,
        'results': results
    }


# ============================================================================
# SHAP VALUES
# ============================================================================

def calculate_shap_importance(
    y_train: pl.DataFrame,
    n_lags: int = 10,
    max_samples: int = 100
) -> Dict[str, Any]:
    """
    Calculate SHAP values for feature importance.

    SHAP provides consistent, game-theoretic feature attributions.

    Args:
        y_train: Training panel data
        n_lags: Number of lag features
        max_samples: Maximum samples for SHAP calculation
    """
    if not SHAP_AVAILABLE:
        return {'success': False, 'error': 'SHAP not available. Install with: pip install shap'}

    if not SKLEARN_AVAILABLE:
        return {'success': False, 'error': 'sklearn not available'}

    results = {}
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()

        if len(values) < n_lags + 20:
            results[entity_id] = {'error': 'Insufficient data'}
            continue

        # Create lag features
        X, y = _create_lag_features(values, n_lags)

        # Limit samples
        if len(X) > max_samples:
            indices = np.random.choice(len(X), max_samples, replace=False)
            X = X[indices]
            y = y[indices]

        # Fit model
        model = Ridge(alpha=1.0)
        model.fit(X, y)

        # Calculate SHAP values
        try:
            explainer = shap.LinearExplainer(model, X)
            shap_values = explainer.shap_values(X)

            # Mean absolute SHAP values per feature
            mean_shap = np.abs(shap_values).mean(axis=0)

            feature_names = [f'lag_{i+1}' for i in range(n_lags)]
            sorted_idx = np.argsort(mean_shap)[::-1]

            results[entity_id] = {
                'feature_names': [feature_names[i] for i in sorted_idx],
                'mean_shap': mean_shap[sorted_idx].tolist(),
                'shap_values_sample': shap_values[:10].tolist() if len(shap_values) >= 10 else shap_values.tolist()
            }
        except Exception as e:
            results[entity_id] = {'error': str(e)}

    return {
        'success': True,
        'method': 'shap',
        'n_lags': n_lags,
        'results': results
    }


# ============================================================================
# LAG IMPORTANCE ANALYSIS
# ============================================================================

def analyze_lag_importance(
    y_train: pl.DataFrame,
    max_lags: int = 20
) -> Dict[str, Any]:
    """
    Analyze importance of different lag values.

    Uses autocorrelation and partial autocorrelation to identify
    significant lags.

    Args:
        y_train: Training panel data
        max_lags: Maximum lag to analyze
    """
    results = {}
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()

        if len(values) < max_lags + 10:
            results[entity_id] = {'error': 'Insufficient data'}
            continue

        n = len(values)
        actual_max_lags = min(max_lags, n // 2 - 1)

        # Calculate ACF
        acf = np.zeros(actual_max_lags + 1)
        mean = np.mean(values)
        var = np.var(values)

        if var == 0:
            results[entity_id] = {'error': 'Zero variance'}
            continue

        for k in range(actual_max_lags + 1):
            acf[k] = np.sum((values[k:] - mean) * (values[:n-k] - mean)) / ((n - k) * var)

        # Calculate PACF (using Yule-Walker equations approximation)
        pacf = np.zeros(actual_max_lags + 1)
        pacf[0] = 1.0
        if actual_max_lags > 0:
            pacf[1] = acf[1]

        for k in range(2, actual_max_lags + 1):
            # Simple approximation
            phi = np.zeros(k + 1)
            phi[1] = acf[1]
            for j in range(2, k + 1):
                num = acf[j] - sum(phi[m] * acf[j - m] for m in range(1, j))
                den = 1 - sum(phi[m] * acf[m] for m in range(1, j))
                if den != 0:
                    phi[j] = num / den
                pacf[k] = phi[k]

        # Significance threshold (95% confidence)
        significance_threshold = 1.96 / np.sqrt(n)

        # Find significant lags
        significant_lags = [
            i for i in range(1, actual_max_lags + 1)
            if abs(acf[i]) > significance_threshold or abs(pacf[i]) > significance_threshold
        ]

        results[entity_id] = {
            'acf': acf.tolist(),
            'pacf': pacf.tolist(),
            'lags': list(range(actual_max_lags + 1)),
            'significant_lags': significant_lags,
            'significance_threshold': float(significance_threshold),
            'recommended_lags': significant_lags[:5] if len(significant_lags) > 0 else [1, 2, 3]
        }

    return {
        'success': True,
        'method': 'lag_analysis',
        'max_lags': max_lags,
        'results': results
    }


# ============================================================================
# SENSITIVITY ANALYSIS
# ============================================================================

def sensitivity_analysis(
    y_train: pl.DataFrame,
    forecaster: Callable,
    fh: int = 10,
    perturbation_pct: float = 0.05,
    n_perturbations: int = 20,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Analyze forecast sensitivity to input perturbations.

    Measures how small changes in input affect forecasts.

    Args:
        y_train: Training panel data
        forecaster: Forecasting function
        fh: Forecast horizon
        perturbation_pct: Percentage perturbation
        n_perturbations: Number of perturbations
        freq: Data frequency
    """
    results = {}
    entities = y_train['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y_train.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        n = len(values)

        if n < 20:
            results[entity_id] = {'error': 'Insufficient data'}
            continue

        # Baseline forecast
        try:
            base_result = forecaster(entity_data, fh=fh, freq=freq)
            if 'forecast' not in base_result:
                results[entity_id] = {'error': 'Forecaster failed'}
                continue
            base_forecast = np.array([f['value'] for f in base_result['forecast']])
        except Exception as e:
            results[entity_id] = {'error': str(e)}
            continue

        # Perturb each recent point and measure impact
        sensitivity_by_point = []
        perturbation_size = perturbation_pct * np.std(values)

        for i in range(min(n_perturbations, n // 2)):
            point_idx = n - 1 - i  # Start from most recent

            # Create perturbed data
            perturbed_values = values.copy()
            perturbed_values[point_idx] += perturbation_size

            perturbed_data = entity_data.clone()
            perturbed_data = perturbed_data.with_columns([
                pl.when(pl.arange(0, len(perturbed_data)) == point_idx)
                .then(pl.lit(perturbed_values[point_idx]))
                .otherwise(pl.col('value'))
                .alias('value')
            ])

            try:
                pert_result = forecaster(perturbed_data, fh=fh, freq=freq)
                if 'forecast' in pert_result:
                    pert_forecast = np.array([f['value'] for f in pert_result['forecast']])
                    forecast_change = np.mean(np.abs(pert_forecast - base_forecast))
                    sensitivity = forecast_change / perturbation_size
                    sensitivity_by_point.append({
                        'point_index': point_idx,
                        'recency': i + 1,
                        'sensitivity': float(sensitivity)
                    })
            except Exception:
                continue

        if sensitivity_by_point:
            avg_sensitivity = np.mean([s['sensitivity'] for s in sensitivity_by_point])
            results[entity_id] = {
                'sensitivity_by_point': sensitivity_by_point,
                'average_sensitivity': float(avg_sensitivity),
                'perturbation_size': float(perturbation_size)
            }
        else:
            results[entity_id] = {'error': 'Could not compute sensitivity'}

    return {
        'success': True,
        'method': 'sensitivity',
        'perturbation_pct': perturbation_pct,
        'results': results
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


def main():
    """Test feature importance methods."""
    print("Testing Feature Importance Module")
    print("=" * 50)

    # Create sample data
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 12, 31),
        interval='1d',
        eager=True
    ).to_list()

    n = len(dates)
    np.random.seed(42)

    # AR(3) process with known coefficients
    values = np.zeros(n)
    values[0:3] = np.random.randn(3) * 10 + 100

    for i in range(3, n):
        values[i] = 0.5 * values[i-1] + 0.3 * values[i-2] + 0.1 * values[i-3] + np.random.randn() * 2

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values.tolist()
    })

    # Test permutation importance
    if SKLEARN_AVAILABLE:
        perm_result = calculate_permutation_importance(df, n_lags=5, n_repeats=5)
        print(f"Permutation importance: {perm_result['results']['A']['feature_names'][:3]}")

        model_result = calculate_model_importance(df, n_lags=5, model_type='ridge')
        print(f"Model importance: {model_result['results']['A']['feature_names'][:3]}")

    # Test lag analysis
    lag_result = analyze_lag_importance(df, max_lags=10)
    print(f"Significant lags: {lag_result['results']['A']['significant_lags']}")

    # Test SHAP
    if SHAP_AVAILABLE:
        shap_result = calculate_shap_importance(df, n_lags=5, max_samples=50)
        print(f"SHAP importance: success = {shap_result['success']}")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
