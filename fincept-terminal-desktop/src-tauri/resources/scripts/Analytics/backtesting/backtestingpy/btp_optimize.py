"""
Backtesting.py Optimization Module

Extended optimization: grid search, heatmap data, compute_stats, walk-forward.
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional
from dataclasses import asdict


def _get_backtesting():
    from backtesting import Backtest, Strategy
    from backtesting.lib import compute_stats, plot_heatmaps
    return Backtest, Strategy, compute_stats, plot_heatmaps


# ============================================================================
# Extended Optimization
# ============================================================================

def optimize_strategy(bt_instance, params: Dict[str, Any],
                      maximize: str = 'Sharpe Ratio',
                      max_tries: int = 500,
                      method: str = 'grid',
                      constraint=None) -> Dict[str, Any]:
    """
    Run optimization on a Backtest instance.

    Args:
        bt_instance: backtesting.Backtest instance
        params: Dict of param_name -> {min, max, step}
        maximize: Metric to maximize
        max_tries: Max parameter combinations
        method: 'grid' or 'skopt'
        constraint: Optional constraint function

    Returns:
        Dict with optimal_params, stats, heatmap_data
    """
    # Build parameter ranges
    opt_kwargs = {}
    for name, cfg in params.items():
        min_v = cfg.get('min', 1)
        max_v = cfg.get('max', 100)
        step = cfg.get('step', 1)
        if isinstance(min_v, float) or isinstance(max_v, float) or isinstance(step, float):
            # Float range - use list
            values = []
            v = min_v
            while v <= max_v:
                values.append(round(v, 6))
                v += step
            opt_kwargs[name] = values
        else:
            opt_kwargs[name] = range(int(min_v), int(max_v) + 1, int(step))

    # Metric mapping
    metric_map = {
        'sharpe': 'Sharpe Ratio',
        'Sharpe Ratio': 'Sharpe Ratio',
        'return': 'Return [%]',
        'Return': 'Return [%]',
        'sortino': 'Sortino Ratio',
        'calmar': 'Calmar Ratio',
        'profit_factor': 'Profit Factor',
        'win_rate': 'Win Rate [%]',
        'sqn': 'SQN',
    }
    maximize_metric = metric_map.get(maximize, maximize)

    # Run optimization
    kwargs = {
        'maximize': maximize_metric,
        'max_tries': max_tries,
    }
    if constraint:
        kwargs['constraint'] = constraint

    # Use skopt if requested and available
    if method == 'skopt':
        try:
            kwargs['method'] = 'skopt'
        except Exception:
            pass  # Fall back to grid

    stats = bt_instance.optimize(**opt_kwargs, **kwargs)

    # Extract optimal parameters
    optimal_params = {}
    for name in params:
        if hasattr(stats._strategy, name):
            optimal_params[name] = getattr(stats._strategy, name)

    # Build heatmap data if 2 params
    heatmap_data = None
    if len(params) == 2:
        heatmap_data = _extract_heatmap_data(bt_instance, opt_kwargs, maximize_metric, max_tries)

    return {
        'optimal_parameters': optimal_params,
        'metric_name': maximize,
        'metric_value': _safe_float(stats.get(maximize_metric, 0)),
        'stats': _stats_to_dict(stats),
        'heatmap_data': heatmap_data,
    }


def _extract_heatmap_data(bt_instance, opt_kwargs, maximize_metric,
                          max_tries) -> Optional[List[Dict[str, Any]]]:
    """Extract heatmap data from optimization results."""
    try:
        _, _, compute_stats, _ = _get_backtesting()
        param_names = list(opt_kwargs.keys())
        if len(param_names) != 2:
            return None

        # Re-run optimize to get all results
        results = []
        p1_name, p2_name = param_names
        for v1 in list(opt_kwargs[p1_name])[:50]:  # Limit for performance
            for v2 in list(opt_kwargs[p2_name])[:50]:
                try:
                    stats = bt_instance.optimize(
                        **{p1_name: [v1], p2_name: [v2]},
                        maximize=maximize_metric,
                        max_tries=1
                    )
                    results.append({
                        p1_name: v1,
                        p2_name: v2,
                        'value': _safe_float(stats.get(maximize_metric, 0))
                    })
                except Exception:
                    continue

        return results if results else None
    except Exception:
        return None


def compute_extended_stats(trades_series, **kwargs) -> Dict[str, Any]:
    """
    Compute extended statistics using backtesting.lib.compute_stats.

    Args:
        trades_series: Series of trade returns or equity curve
    """
    try:
        _, _, compute_stats, _ = _get_backtesting()
        stats = compute_stats(trades_series, **kwargs)
        return _stats_to_dict(stats)
    except ImportError:
        return {'error': 'backtesting.lib.compute_stats not available'}
    except Exception as e:
        return {'error': str(e)}


# ============================================================================
# Walk-Forward Optimization
# ============================================================================

def walk_forward_optimize(data: pd.DataFrame, strategy_class,
                          params: Dict[str, Any],
                          n_splits: int = 5,
                          train_ratio: float = 0.7,
                          initial_capital: float = 10000,
                          commission: float = 0.0,
                          maximize: str = 'Sharpe Ratio') -> Dict[str, Any]:
    """
    Walk-forward optimization: optimize on training window, test on out-of-sample.

    Args:
        data: OHLCV DataFrame
        strategy_class: Strategy class with tunable parameters
        params: Parameter ranges
        n_splits: Number of walk-forward splits
        train_ratio: Fraction of each window used for training
        initial_capital: Starting capital
        commission: Commission rate
        maximize: Optimization metric

    Returns:
        Dict with per-split results and aggregate metrics
    """
    from backtesting import Backtest

    total_bars = len(data)
    window_size = total_bars // n_splits
    results = []

    for i in range(n_splits):
        start = i * window_size
        end = min(start + window_size, total_bars)
        split_point = start + int((end - start) * train_ratio)

        train_data = data.iloc[start:split_point]
        test_data = data.iloc[split_point:end]

        if len(train_data) < 20 or len(test_data) < 5:
            continue

        # Optimize on training data
        try:
            bt_train = Backtest(train_data, strategy_class, cash=initial_capital, commission=commission)
            opt_kwargs = {}
            for name, cfg in params.items():
                min_v = cfg.get('min', 1)
                max_v = cfg.get('max', 100)
                step = cfg.get('step', 1)
                opt_kwargs[name] = range(int(min_v), int(max_v) + 1, int(step))

            train_stats = bt_train.optimize(**opt_kwargs, maximize=maximize, max_tries=200)

            # Extract optimal params
            opt_params = {}
            for name in params:
                if hasattr(train_stats._strategy, name):
                    opt_params[name] = getattr(train_stats._strategy, name)

            # Test on out-of-sample data with optimal params
            # Create new strategy class with fixed params
            bt_test = Backtest(test_data, strategy_class, cash=initial_capital, commission=commission)
            test_stats = bt_test.run(**opt_params) if opt_params else bt_test.run()

            results.append({
                'split': i + 1,
                'train_start': str(train_data.index[0]),
                'train_end': str(train_data.index[-1]),
                'test_start': str(test_data.index[0]),
                'test_end': str(test_data.index[-1]),
                'optimal_params': opt_params,
                'train_return': _safe_float(train_stats.get('Return [%]', 0)),
                'test_return': _safe_float(test_stats.get('Return [%]', 0)),
                'train_sharpe': _safe_float(train_stats.get('Sharpe Ratio', 0)),
                'test_sharpe': _safe_float(test_stats.get('Sharpe Ratio', 0)),
                'train_trades': int(train_stats.get('# Trades', 0)),
                'test_trades': int(test_stats.get('# Trades', 0)),
            })
        except Exception as e:
            results.append({
                'split': i + 1,
                'error': str(e),
            })

    # Aggregate
    test_returns = [r['test_return'] for r in results if 'test_return' in r]
    test_sharpes = [r['test_sharpe'] for r in results if 'test_sharpe' in r]

    return {
        'splits': results,
        'n_splits': len(results),
        'avg_test_return': float(np.mean(test_returns)) if test_returns else 0,
        'avg_test_sharpe': float(np.mean(test_sharpes)) if test_sharpes else 0,
        'total_test_return': float(np.sum(test_returns)) if test_returns else 0,
    }


# ============================================================================
# Helpers
# ============================================================================

def _safe_float(val, default=0.0) -> float:
    try:
        v = float(val)
        if np.isnan(v) or np.isinf(v):
            return float(default)
        return v
    except (TypeError, ValueError):
        return float(default)


def _stats_to_dict(stats) -> Dict[str, Any]:
    """Convert backtesting.py stats object to serializable dict."""
    result = {}
    for key in stats.keys():
        val = stats[key]
        if isinstance(val, (int, float, np.integer, np.floating)):
            result[key] = _safe_float(val)
        elif isinstance(val, str):
            result[key] = val
        elif isinstance(val, pd.DataFrame):
            continue  # Skip DataFrames (_equity_curve, _trades)
        elif val is None:
            result[key] = None
        else:
            try:
                result[key] = str(val)
            except Exception:
                pass
    return result
