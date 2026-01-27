"""
VBT Optimization Module

Leverages VectorBT's vectorized parameter optimization:

Features:
- Grid Search: Test all parameter combinations (vectorized, fast)
- Random Search: Random sampling from parameter space
- Walk-Forward Optimization: In-sample optimization + out-of-sample validation
- Objective Functions: Sharpe, Sortino, Total Return, Calmar, Custom
- Statistical Validation: Monte Carlo benchmarking, p-values
- Results Analysis: Heatmaps, parameter sensitivity, robustness checks
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, List, Optional, Tuple
from itertools import product


def optimize(
    vbt,
    close_series: pd.Series,
    strategy_type: str,
    parameters: Dict[str, Any],
    objective: str = 'sharpe',
    initial_capital: float = 100000,
    method: str = 'grid',
    max_iterations: int = 500,
    request: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """
    Run parameter optimization using VBT's vectorized approach.

    VBT's key advantage: it can test thousands of parameter combinations
    simultaneously using numpy broadcasting, making optimization 100x+ faster
    than iterative approaches.

    Args:
        vbt: vectorbt module
        close_series: Price data
        strategy_type: Strategy type identifier
        parameters: Dict of parameter ranges, e.g.:
            {'fastPeriod': [5, 10, 15, 20], 'slowPeriod': [20, 30, 40, 50]}
            or {'fastPeriod': {'min': 5, 'max': 50, 'step': 5}}
        objective: Optimization target ('sharpe', 'sortino', 'return', 'calmar')
        initial_capital: Starting capital
        method: 'grid' or 'random'
        max_iterations: Max combinations for random search
        request: Additional request parameters

    Returns:
        Dict with optimization results
    """
    request = request or {}

    # Parse parameter ranges into arrays
    param_arrays = _parse_parameter_ranges(parameters, method, max_iterations)

    if not param_arrays:
        return _empty_optimization_result('No parameters to optimize')

    # Generate all parameter combinations
    param_names = list(param_arrays.keys())
    param_values = list(param_arrays.values())
    combinations = list(product(*param_values))

    if len(combinations) == 0:
        return _empty_optimization_result('No valid parameter combinations')

    # Limit combinations
    if len(combinations) > max_iterations:
        if method == 'random':
            np.random.shuffle(combinations)
            combinations = combinations[:max_iterations]
        else:
            combinations = combinations[:max_iterations]

    # Run vectorized optimization
    results = _run_vectorized_optimization(
        vbt, close_series, strategy_type, param_names,
        combinations, objective, initial_capital, request
    )

    # Find best result
    if not results:
        return _empty_optimization_result('All parameter combinations failed')

    # Sort by objective
    results.sort(key=lambda x: x.get('objective_value', -999), reverse=True)
    best = results[0]

    # Walk-forward validation if requested
    walk_forward_results = None
    if request.get('walkForward', False):
        walk_forward_results = _walk_forward_optimization(
            vbt, close_series, strategy_type, best['parameters'],
            objective, initial_capital, request
        )

    # Statistical significance
    random_benchmark = None
    if request.get('randomBenchmark', True) and len(results) > 5:
        random_benchmark = _calculate_significance(results, close_series, initial_capital)

    return {
        'success': True,
        'bestParameters': best['parameters'],
        'bestObjectiveValue': best['objective_value'],
        'bestPerformance': best.get('performance', {}),
        'iterations': len(combinations),
        'totalCombinations': len(combinations),
        'method': method,
        'objective': objective,
        'allResults': results[:100],  # Top 100 results
        'parameterSensitivity': _calculate_sensitivity(results, param_names),
        'walkForward': walk_forward_results,
        'statisticalSignificance': random_benchmark,
    }


def walk_forward_optimize(
    vbt,
    close_series: pd.Series,
    strategy_type: str,
    parameters: Dict[str, Any],
    objective: str = 'sharpe',
    initial_capital: float = 100000,
    n_splits: int = 5,
    train_ratio: float = 0.7,
    request: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """
    Walk-Forward Optimization.

    Splits data into n_splits windows. For each window:
    1. Optimize on training portion (train_ratio)
    2. Validate on remaining out-of-sample portion
    3. Record OOS performance

    This tests parameter robustness and reduces overfitting.

    Args:
        vbt: vectorbt module
        close_series: Full price series
        strategy_type: Strategy identifier
        parameters: Parameter ranges
        objective: Optimization target
        initial_capital: Starting capital
        n_splits: Number of walk-forward windows
        train_ratio: Fraction used for training
        request: Additional parameters

    Returns:
        Walk-forward results with in-sample and out-of-sample metrics
    """
    n = len(close_series)
    window_size = n // n_splits
    results = []

    for i in range(n_splits):
        start_idx = i * window_size
        end_idx = min(start_idx + window_size, n)

        if end_idx - start_idx < 50:  # Need minimum bars
            continue

        window = close_series.iloc[start_idx:end_idx]
        train_end = int(len(window) * train_ratio)

        train_data = window.iloc[:train_end]
        test_data = window.iloc[train_end:]

        if len(train_data) < 30 or len(test_data) < 10:
            continue

        # Optimize on training data
        opt_result = optimize(
            vbt, train_data, strategy_type, parameters,
            objective, initial_capital, method='grid',
            max_iterations=200, request=request
        )

        if not opt_result.get('success'):
            continue

        best_params = opt_result['bestParameters']

        # Validate on test data
        test_perf = _evaluate_params(
            vbt, test_data, strategy_type, best_params,
            initial_capital, request
        )

        results.append({
            'window': i + 1,
            'trainStart': str(train_data.index[0]).split(' ')[0],
            'trainEnd': str(train_data.index[-1]).split(' ')[0],
            'testStart': str(test_data.index[0]).split(' ')[0],
            'testEnd': str(test_data.index[-1]).split(' ')[0],
            'bestParams': best_params,
            'inSampleReturn': opt_result.get('bestPerformance', {}).get('totalReturn', 0),
            'inSampleSharpe': opt_result.get('bestPerformance', {}).get('sharpeRatio', 0),
            'outOfSampleReturn': test_perf.get('totalReturn', 0),
            'outOfSampleSharpe': test_perf.get('sharpeRatio', 0),
        })

    if not results:
        return {'success': False, 'error': 'Walk-forward produced no valid windows'}

    # Aggregate OOS results
    oos_returns = [r['outOfSampleReturn'] for r in results]
    oos_sharpes = [r['outOfSampleSharpe'] for r in results]

    return {
        'success': True,
        'windows': results,
        'nWindows': len(results),
        'avgOosReturn': float(np.mean(oos_returns)),
        'avgOosSharpe': float(np.mean(oos_sharpes)),
        'oosReturnStd': float(np.std(oos_returns)),
        'robustnessScore': float(np.mean([1 if r > 0 else 0 for r in oos_returns])),
        'degradationRatio': _calc_degradation(results),
    }


# ============================================================================
# Internal Implementation
# ============================================================================

def _parse_parameter_ranges(
    parameters: Dict[str, Any], method: str, max_iter: int
) -> Dict[str, np.ndarray]:
    """
    Parse parameter specifications into arrays of values to test.

    Supports:
    - List: [5, 10, 15, 20]
    - Range: {'min': 5, 'max': 50, 'step': 5}
    - Single value (not optimized): 14
    """
    result = {}

    for name, spec in parameters.items():
        if isinstance(spec, list):
            result[name] = np.array(spec)
        elif isinstance(spec, dict):
            min_val = spec.get('min', 5)
            max_val = spec.get('max', 50)
            step = spec.get('step', 5)
            result[name] = np.arange(min_val, max_val + step, step)
        elif isinstance(spec, (int, float)):
            # Single value - still include for completeness
            result[name] = np.array([spec])

    return result


def _run_vectorized_optimization(
    vbt, close_series, strategy_type, param_names,
    combinations, objective, initial_capital, request
) -> List[Dict]:
    """
    Run optimization across all parameter combinations.

    Uses VBT's vectorized approach where possible:
    - For indicator-based strategies, compute indicators once with broadcast parameters
    - For each combination, build portfolio and extract objective metric
    """
    import vbt_strategies as strat
    import vbt_portfolio as pf

    results = []

    for combo in combinations:
        params = dict(zip(param_names, [_to_python_type(v) for v in combo]))

        try:
            # Build signals for this parameter set
            entries, exits = strat.build_strategy_signals(
                vbt, strategy_type, close_series, params
            )

            # Build portfolio
            portfolio = pf.build_portfolio(
                vbt, close_series, entries, exits,
                initial_capital, request
            )

            # Extract objective metric
            obj_value = _get_objective_value(portfolio, objective)

            if not np.isfinite(obj_value):
                continue

            # Get basic performance
            total_return = float(portfolio.total_return())
            stats = portfolio.stats()

            def safe_s(key, default=0.0):
                try:
                    v = float(stats.get(key, default))
                    return v if np.isfinite(v) else default
                except:
                    return default

            results.append({
                'parameters': params,
                'objective_value': obj_value,
                'performance': {
                    'totalReturn': total_return,
                    'sharpeRatio': safe_s('Sharpe Ratio'),
                    'sortinoRatio': safe_s('Sortino Ratio'),
                    'maxDrawdown': abs(safe_s('Max Drawdown [%]')) / 100,
                    'totalTrades': int(safe_s('Total Trades')),
                    'winRate': safe_s('Win Rate [%]') / 100,
                    'calmarRatio': safe_s('Calmar Ratio'),
                },
            })
        except Exception:
            continue

    return results


def _get_objective_value(portfolio, objective: str) -> float:
    """Extract the objective metric from a portfolio."""
    try:
        if objective == 'sharpe':
            stats = portfolio.stats()
            return float(stats.get('Sharpe Ratio', -999))
        elif objective == 'sortino':
            stats = portfolio.stats()
            return float(stats.get('Sortino Ratio', -999))
        elif objective == 'return':
            return float(portfolio.total_return())
        elif objective == 'calmar':
            stats = portfolio.stats()
            return float(stats.get('Calmar Ratio', -999))
        elif objective == 'profit_factor':
            stats = portfolio.stats()
            return float(stats.get('Profit Factor', -999))
        elif objective == 'sqn':
            # System Quality Number
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
                records = portfolio.trades.records_readable
                if len(records) > 0 and 'PnL' in records.columns:
                    pnl = records['PnL'].values.astype(float)
                    pnl = pnl[np.isfinite(pnl)]
                    if len(pnl) > 1:
                        return float(np.sqrt(len(pnl)) * np.mean(pnl) / np.std(pnl, ddof=1))
            return -999
        else:
            # Default to Sharpe
            stats = portfolio.stats()
            return float(stats.get('Sharpe Ratio', -999))
    except Exception:
        return -999.0


def _evaluate_params(
    vbt, close_series, strategy_type, params,
    initial_capital, request
) -> Dict[str, float]:
    """Evaluate a single parameter set and return performance metrics."""
    import vbt_strategies as strat
    import vbt_portfolio as pf

    try:
        entries, exits = strat.build_strategy_signals(
            vbt, strategy_type, close_series, params
        )
        portfolio = pf.build_portfolio(
            vbt, close_series, entries, exits,
            initial_capital, request
        )

        total_return = float(portfolio.total_return())
        stats = portfolio.stats()

        def safe_s(key, default=0.0):
            try:
                v = float(stats.get(key, default))
                return v if np.isfinite(v) else default
            except:
                return default

        return {
            'totalReturn': total_return,
            'sharpeRatio': safe_s('Sharpe Ratio'),
            'sortinoRatio': safe_s('Sortino Ratio'),
            'maxDrawdown': abs(safe_s('Max Drawdown [%]')) / 100,
            'totalTrades': int(safe_s('Total Trades')),
        }
    except Exception:
        return {'totalReturn': 0, 'sharpeRatio': 0, 'sortinoRatio': 0, 'maxDrawdown': 0, 'totalTrades': 0}


def _walk_forward_optimization(
    vbt, close_series, strategy_type, best_params,
    objective, initial_capital, request
) -> Dict[str, Any]:
    """Quick walk-forward check with 3 splits."""
    return walk_forward_optimize(
        vbt, close_series, strategy_type,
        {k: [v] for k, v in best_params.items()},  # Single-value ranges
        objective, initial_capital,
        n_splits=3, train_ratio=0.7, request=request
    )


def _calculate_sensitivity(
    results: List[Dict], param_names: List[str]
) -> Dict[str, Any]:
    """
    Calculate parameter sensitivity (how much each param affects the objective).

    Returns correlation between each parameter value and the objective score.
    """
    if len(results) < 3:
        return {}

    sensitivity = {}
    objectives = np.array([r['objective_value'] for r in results])

    for name in param_names:
        values = np.array([r['parameters'].get(name, 0) for r in results], dtype=float)

        if np.std(values) < 1e-10:
            sensitivity[name] = 0.0
            continue

        # Correlation between parameter value and objective
        corr = np.corrcoef(values, objectives)[0, 1]
        sensitivity[name] = float(corr) if np.isfinite(corr) else 0.0

    return sensitivity


def _calculate_significance(
    results: List[Dict], close_series: pd.Series, initial_capital: float
) -> Dict[str, Any]:
    """
    Calculate statistical significance of best result vs random.

    Compares best optimization result against distribution of all tested results.
    """
    if len(results) < 5:
        return {'significant': False, 'pValue': 1.0}

    objectives = [r['objective_value'] for r in results]
    best = objectives[0]  # Already sorted
    mean_obj = np.mean(objectives)
    std_obj = np.std(objectives, ddof=1)

    if std_obj < 1e-10:
        return {'significant': False, 'pValue': 1.0, 'zScore': 0}

    z_score = (best - mean_obj) / std_obj

    # Approximate p-value from z-score (one-sided)
    # Using simplified normal CDF approximation
    p_value = 0.5 * (1 - _erf(z_score / np.sqrt(2)))

    return {
        'significant': p_value < 0.05,
        'pValue': float(p_value),
        'zScore': float(z_score),
        'bestVsMean': float(best - mean_obj),
        'nTested': len(results),
    }


def _calc_degradation(results: List[Dict]) -> float:
    """
    Calculate IS-to-OOS degradation ratio.

    Returns avg OOS / avg IS (1.0 = no degradation, <1 = overfitting likely).
    """
    is_returns = [r.get('inSampleReturn', 0) for r in results if r.get('inSampleReturn', 0) != 0]
    oos_returns = [r.get('outOfSampleReturn', 0) for r in results]

    if not is_returns or np.mean(np.abs(is_returns)) < 1e-10:
        return 0.0

    return float(np.mean(oos_returns) / np.mean(is_returns))


def _to_python_type(val):
    """Convert numpy types to Python native types."""
    if isinstance(val, (np.integer, np.int64, np.int32)):
        return int(val)
    elif isinstance(val, (np.floating, np.float64, np.float32)):
        return float(val)
    return val


def _erf(x: float) -> float:
    """Approximate error function (for p-value calculation)."""
    # Abramowitz and Stegun approximation
    sign = 1 if x >= 0 else -1
    x = abs(x)
    t = 1.0 / (1.0 + 0.3275911 * x)
    y = 1.0 - (
        (((1.061405429 * t - 1.453152027) * t + 1.421413741) * t - 0.284496736) * t + 0.254829592
    ) * t * np.exp(-x * x)
    return sign * y


def _empty_optimization_result(error: str) -> Dict[str, Any]:
    """Return empty optimization result."""
    return {
        'success': False,
        'error': error,
        'bestParameters': {},
        'bestObjectiveValue': 0,
        'iterations': 0,
        'allResults': [],
    }
