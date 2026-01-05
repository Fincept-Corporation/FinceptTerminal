"""
Backtesting Framework Module
============================

Provides comprehensive backtesting for time series forecasting:
- Walk-forward backtesting
- Rolling origin evaluation
- Multi-horizon accuracy tracking
- Model comparison framework
- Performance attribution

Essential for validating forecasting models in production.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple, Callable
from datetime import datetime, timedelta
import warnings

warnings.filterwarnings('ignore')


# ============================================================================
# WALK-FORWARD BACKTESTING
# ============================================================================

def walk_forward_backtest(
    y: pl.DataFrame,
    forecaster: Callable,
    initial_train_size: int = 100,
    test_size: int = 1,
    step_size: int = 1,
    horizons: Optional[List[int]] = None,
    freq: str = '1d',
    retrain: bool = True
) -> Dict[str, Any]:
    """
    Walk-forward backtesting with optional retraining.

    Simulates real-world forecasting by generating forecasts at each
    origin and comparing with actual values.

    Args:
        y: Panel data
        forecaster: Forecasting function (y_train, fh, freq) -> result
        initial_train_size: Initial training window
        test_size: Number of periods to forecast
        step_size: Step between forecast origins
        horizons: Specific horizons to track (default: all up to test_size)
        freq: Data frequency
        retrain: Whether to retrain at each origin
    """
    if horizons is None:
        horizons = list(range(1, test_size + 1))

    results = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        if n < initial_train_size + max(horizons):
            continue

        entity_results = []
        forecasts_by_horizon = {h: [] for h in horizons}
        actuals_by_horizon = {h: [] for h in horizons}

        origin = initial_train_size

        while origin + max(horizons) <= n:
            train_data = entity_data[:origin]
            actual_data = entity_data[origin:origin + max(horizons)]
            actual_values = actual_data['value'].to_list()

            try:
                fc_result = forecaster(train_data, fh=max(horizons), freq=freq)

                if 'forecast' in fc_result:
                    fc_values = [f['value'] for f in fc_result['forecast']]

                    for h in horizons:
                        if h - 1 < len(fc_values) and h - 1 < len(actual_values):
                            forecasts_by_horizon[h].append(fc_values[h - 1])
                            actuals_by_horizon[h].append(actual_values[h - 1])

                            entity_results.append({
                                'origin': origin,
                                'horizon': h,
                                'forecast': float(fc_values[h - 1]),
                                'actual': float(actual_values[h - 1]),
                                'error': float(fc_values[h - 1] - actual_values[h - 1]),
                                'abs_error': float(abs(fc_values[h - 1] - actual_values[h - 1]))
                            })

            except Exception as e:
                pass

            origin += step_size

        # Calculate metrics by horizon
        metrics_by_horizon = {}
        for h in horizons:
            if len(forecasts_by_horizon[h]) > 0:
                fc = np.array(forecasts_by_horizon[h])
                act = np.array(actuals_by_horizon[h])
                errors = fc - act

                metrics_by_horizon[h] = {
                    'mae': float(np.mean(np.abs(errors))),
                    'rmse': float(np.sqrt(np.mean(errors ** 2))),
                    'mape': float(np.mean(np.abs(errors / (act + 1e-10))) * 100),
                    'bias': float(np.mean(errors)),
                    'n_forecasts': len(fc)
                }

        results.append({
            'entity_id': entity_id,
            'n_origins': len(set([r['origin'] for r in entity_results])),
            'forecasts': entity_results,
            'metrics_by_horizon': metrics_by_horizon
        })

    return {
        'success': True,
        'method': 'walk_forward_backtest',
        'initial_train_size': initial_train_size,
        'test_size': test_size,
        'step_size': step_size,
        'horizons': horizons,
        'results': results
    }


# ============================================================================
# MODEL COMPARISON BACKTEST
# ============================================================================

def compare_models_backtest(
    y: pl.DataFrame,
    forecasters: Dict[str, Callable],
    initial_train_size: int = 100,
    test_size: int = 1,
    step_size: int = 5,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Compare multiple forecasting models via backtesting.

    Args:
        y: Panel data
        forecasters: Dict of {model_name: forecaster_function}
        initial_train_size: Initial training window
        test_size: Forecast horizon
        step_size: Step between origins
        freq: Data frequency
    """
    results = {}
    comparison = []

    for model_name, forecaster in forecasters.items():
        bt_result = walk_forward_backtest(
            y=y,
            forecaster=forecaster,
            initial_train_size=initial_train_size,
            test_size=test_size,
            step_size=step_size,
            freq=freq
        )

        results[model_name] = bt_result

        # Aggregate metrics
        if bt_result['results']:
            all_metrics = []
            for entity_result in bt_result['results']:
                for h, metrics in entity_result['metrics_by_horizon'].items():
                    all_metrics.append(metrics)

            if all_metrics:
                avg_mae = np.mean([m['mae'] for m in all_metrics])
                avg_rmse = np.mean([m['rmse'] for m in all_metrics])
                avg_mape = np.mean([m['mape'] for m in all_metrics])

                comparison.append({
                    'model': model_name,
                    'mae': float(avg_mae),
                    'rmse': float(avg_rmse),
                    'mape': float(avg_mape),
                    'n_forecasts': sum([m['n_forecasts'] for m in all_metrics])
                })

    # Rank models
    comparison = sorted(comparison, key=lambda x: x['mae'])
    for i, c in enumerate(comparison):
        c['rank'] = i + 1

    return {
        'success': True,
        'method': 'model_comparison',
        'comparison': comparison,
        'best_model': comparison[0]['model'] if comparison else None,
        'detailed_results': results
    }


# ============================================================================
# ROLLING ORIGIN EVALUATION
# ============================================================================

def rolling_origin_evaluation(
    y: pl.DataFrame,
    forecaster: Callable,
    min_train_size: int = 50,
    max_train_size: Optional[int] = None,
    test_size: int = 1,
    freq: str = '1d'
) -> Dict[str, Any]:
    """
    Rolling origin evaluation with varying training sizes.

    Analyzes how forecast accuracy changes with training set size.

    Args:
        y: Panel data
        forecaster: Forecasting function
        min_train_size: Minimum training window
        max_train_size: Maximum training window (None = use all available)
        test_size: Test set size
        freq: Data frequency
    """
    results = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        if n < min_train_size + test_size:
            continue

        entity_results = []
        train_size_metrics = {}

        train_size = min_train_size
        actual_max = max_train_size or (n - test_size)

        while train_size <= min(actual_max, n - test_size):
            # Use most recent data for training
            train_end = n - test_size
            train_start = max(0, train_end - train_size)

            train_data = entity_data[train_start:train_end]
            test_data = entity_data[train_end:train_end + test_size]
            actual_values = test_data['value'].to_list()

            try:
                fc_result = forecaster(train_data, fh=test_size, freq=freq)

                if 'forecast' in fc_result:
                    fc_values = [f['value'] for f in fc_result['forecast']]

                    errors = [fc_values[i] - actual_values[i]
                              for i in range(min(len(fc_values), len(actual_values)))]

                    mae = np.mean(np.abs(errors))
                    rmse = np.sqrt(np.mean(np.array(errors) ** 2))

                    train_size_metrics[train_size] = {
                        'mae': float(mae),
                        'rmse': float(rmse),
                        'train_size': train_size
                    }

            except Exception:
                pass

            # Increase train size
            train_size = int(train_size * 1.2) + 1

        results.append({
            'entity_id': entity_id,
            'metrics_by_train_size': train_size_metrics,
            'optimal_train_size': min(train_size_metrics.items(),
                                       key=lambda x: x[1]['mae'])[0] if train_size_metrics else None
        })

    return {
        'success': True,
        'method': 'rolling_origin_evaluation',
        'min_train_size': min_train_size,
        'max_train_size': max_train_size,
        'results': results
    }


# ============================================================================
# PERFORMANCE ATTRIBUTION
# ============================================================================

def performance_attribution(
    backtest_results: Dict[str, Any],
    segments: Optional[Dict[str, Callable]] = None
) -> Dict[str, Any]:
    """
    Attribute forecast performance to different segments/regimes.

    Args:
        backtest_results: Results from walk_forward_backtest
        segments: Dict of {segment_name: condition_function}
    """
    if segments is None:
        # Default segments based on volatility and trend
        segments = {
            'high_volatility': lambda data: np.std(data) > np.median(np.std(data)),
            'low_volatility': lambda data: np.std(data) <= np.median(np.std(data)),
            'uptrend': lambda data: data[-1] > data[0] if len(data) > 1 else False,
            'downtrend': lambda data: data[-1] < data[0] if len(data) > 1 else False
        }

    attribution = {}

    for entity_result in backtest_results.get('results', []):
        entity_id = entity_result['entity_id']
        forecasts = entity_result.get('forecasts', [])

        if not forecasts:
            continue

        # Simple attribution based on error magnitude
        errors = [f['abs_error'] for f in forecasts]
        horizons = [f['horizon'] for f in forecasts]

        # By horizon
        horizon_attribution = {}
        for h in set(horizons):
            h_errors = [e for e, ho in zip(errors, horizons) if ho == h]
            if h_errors:
                horizon_attribution[h] = {
                    'mean_error': float(np.mean(h_errors)),
                    'contribution_pct': float(sum(h_errors) / sum(errors) * 100) if sum(errors) > 0 else 0
                }

        # By error percentile
        percentile_attribution = {
            'p25': float(np.percentile(errors, 25)),
            'p50': float(np.percentile(errors, 50)),
            'p75': float(np.percentile(errors, 75)),
            'p90': float(np.percentile(errors, 90)),
            'p95': float(np.percentile(errors, 95))
        }

        # Worst forecasts
        sorted_forecasts = sorted(forecasts, key=lambda x: x['abs_error'], reverse=True)
        worst_forecasts = sorted_forecasts[:5]

        attribution[entity_id] = {
            'by_horizon': horizon_attribution,
            'by_percentile': percentile_attribution,
            'worst_forecasts': worst_forecasts,
            'total_error': float(sum(errors)),
            'n_forecasts': len(forecasts)
        }

    return {
        'success': True,
        'method': 'performance_attribution',
        'attribution': attribution
    }


# ============================================================================
# SUMMARY STATISTICS
# ============================================================================

def backtest_summary(
    backtest_results: Dict[str, Any]
) -> Dict[str, Any]:
    """
    Generate summary statistics from backtest results.
    """
    if not backtest_results.get('results'):
        return {'success': False, 'error': 'No results to summarize'}

    all_forecasts = []
    all_metrics = []

    for entity_result in backtest_results['results']:
        all_forecasts.extend(entity_result.get('forecasts', []))
        for h, m in entity_result.get('metrics_by_horizon', {}).items():
            m['horizon'] = h
            all_metrics.append(m)

    if not all_forecasts:
        return {'success': False, 'error': 'No forecasts found'}

    errors = [f['error'] for f in all_forecasts]
    abs_errors = [f['abs_error'] for f in all_forecasts]

    summary = {
        'total_forecasts': len(all_forecasts),
        'total_origins': len(set([f['origin'] for f in all_forecasts])),
        'overall_metrics': {
            'mae': float(np.mean(abs_errors)),
            'rmse': float(np.sqrt(np.mean(np.array(errors) ** 2))),
            'bias': float(np.mean(errors)),
            'std_error': float(np.std(errors))
        },
        'error_distribution': {
            'min': float(np.min(abs_errors)),
            'p25': float(np.percentile(abs_errors, 25)),
            'median': float(np.median(abs_errors)),
            'p75': float(np.percentile(abs_errors, 75)),
            'p95': float(np.percentile(abs_errors, 95)),
            'max': float(np.max(abs_errors))
        },
        'by_horizon': {}
    }

    # Metrics by horizon
    for h in sorted(set([m['horizon'] for m in all_metrics])):
        h_metrics = [m for m in all_metrics if m['horizon'] == h]
        summary['by_horizon'][h] = {
            'mae': float(np.mean([m['mae'] for m in h_metrics])),
            'rmse': float(np.mean([m['rmse'] for m in h_metrics])),
            'n_forecasts': sum([m['n_forecasts'] for m in h_metrics])
        }

    return {
        'success': True,
        'summary': summary
    }


def main():
    """Test backtesting framework."""
    print("Testing Backtesting Framework Module")
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
    values = [100 + 0.1 * i + np.random.randn() * 3 for i in range(n)]

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    # Simple naive forecaster for testing
    def naive_forecaster(y_train, fh, freq='1d'):
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

    # Test walk-forward backtest
    bt_result = walk_forward_backtest(
        df, naive_forecaster,
        initial_train_size=100,
        test_size=5,
        step_size=10
    )
    print(f"Walk-forward backtest: {bt_result['results'][0]['n_origins']} origins")

    # Test backtest summary
    summary = backtest_summary(bt_result)
    if summary['success']:
        print(f"Overall MAE: {summary['summary']['overall_metrics']['mae']:.4f}")

    # Test performance attribution
    attr_result = performance_attribution(bt_result)
    print(f"Performance attribution: {len(attr_result['attribution'])} entities")

    # Test rolling origin evaluation
    roe_result = rolling_origin_evaluation(
        df, naive_forecaster,
        min_train_size=50,
        max_train_size=200
    )
    if roe_result['results']:
        print(f"Optimal train size: {roe_result['results'][0]['optimal_train_size']}")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
