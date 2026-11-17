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

from base.base_provider import check_cancelled
import numpy as np
import pandas as pd
from typing import Dict, Any, List, Optional, Tuple
from itertools import product

# Upper bound on optimisation rows returned to the UI (each row is ~300 bytes of JSON).
MAX_RETURNED_RESULTS = 10000


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

    # Limit combinations. The full grid size is reported as totalCombinations so the
    # UI can say how much of the space was searched (both fields used to be the
    # truncated count).
    total_combinations = len(combinations)
    if len(combinations) > max_iterations:
        if method == 'random':
            np.random.shuffle(combinations)
            combinations = combinations[:max_iterations]
        else:
            # An evenly strided subset of the grid: the first N in product order only
            # ever varied the last parameter, leaving the rest of the space untested.
            pick = np.unique(np.linspace(0, len(combinations) - 1, max_iterations).round().astype(int))
            combinations = [combinations[i] for i in pick]

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
        'iterations': len(results),
        'totalCombinations': total_combinations,
        'truncated': total_combinations > len(combinations),
        'method': method,
        'objective': objective,
        # Every evaluated combination (sorted best first) so the UI can draw the full
        # parameter heatmap; capped only as a payload guard. It used to be the top 100,
        # which left most of a 2-parameter grid blank.
        'allResults': results[:MAX_RETURNED_RESULTS],
        'resultsReturned': min(len(results), MAX_RETURNED_RESULTS),
        'paramNames': param_names,
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
    oos_points = []

    for i in range(n_splits):
        check_cancelled(request)
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

        # Validate on test data. The strategy runs over the test window PLUS the bars
        # before it that its indicators need: run on the test bars alone, a 20-bar SMA
        # never warmed up inside a 15-bar window, so every window traded nothing and
        # the walk-forward read 0.00 % everywhere. Metrics are then taken from the
        # test-window part of the equity curve only.
        test_start_pos = start_idx + train_end
        warm = _warmup_bars(best_params)
        eval_data = close_series.iloc[max(0, test_start_pos - warm):end_idx]
        test_perf, test_values = _evaluate_params(
            vbt, eval_data, strategy_type, best_params,
            initial_capital, request, with_equity=True
        )
        if test_values is not None and len(eval_data) > len(test_data):
            test_values = test_values.loc[test_data.index[0]:]
            test_perf = _perf_from_equity(test_values, test_perf)

        # Stitched out-of-sample equity: each test window's curve, rescaled to start
        # where the previous one ended (what trading the walk-forward live would have
        # produced, gaps between windows excluded).
        if test_values is not None and len(test_values) > 1 and float(test_values.iloc[0]) > 0:
            scale = (oos_points[-1]['equity'] if oos_points else float(initial_capital)) / float(test_values.iloc[0])
            for ts, v in test_values.items():
                oos_points.append({'date': str(ts).split(' ')[0] if _daily_index(test_values.index) else str(ts)[:19],
                                   'equity': float(v) * scale, 'window': i + 1})

        best_perf = opt_result.get('bestPerformance', {})
        results.append({
            'window': i + 1,
            'trainStart': str(train_data.index[0]).split(' ')[0],
            'trainEnd': str(train_data.index[-1]).split(' ')[0],
            'testStart': str(test_data.index[0]).split(' ')[0],
            'testEnd': str(test_data.index[-1]).split(' ')[0],
            'trainBars': int(len(train_data)),
            'testBars': int(len(test_data)),
            'bestParams': best_params,
            'inSampleReturn': best_perf.get('totalReturn', 0),
            'inSampleSharpe': best_perf.get('sharpeRatio', 0),
            'inSampleMaxDrawdown': best_perf.get('maxDrawdown', 0),
            'outOfSampleReturn': test_perf.get('totalReturn', 0),
            'outOfSampleSharpe': test_perf.get('sharpeRatio', 0),
            'outOfSampleMaxDrawdown': test_perf.get('maxDrawdown', 0),
            'outOfSampleTrades': test_perf.get('totalTrades', 0),
        })

    if not results:
        return {'success': False, 'error': 'Walk-forward produced no valid windows'}

    # Aggregate OOS results
    oos_returns = [r['outOfSampleReturn'] for r in results]
    oos_sharpes = [r['outOfSampleSharpe'] for r in results]

    return {
        'success': True,
        'oosEquity': oos_points,
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


def _signals_and_portfolio(vbt, close_series, strategy_type, params, initial_capital, request):
    """Signals + simulated portfolio for one parameter set on one window.

    Uses the real OHLC the provider loaded (request['_ohlc_frame'], a Python-only
    key) so optimisation fills at the next bar's open and H/L strategies see real
    highs/lows - exactly like a single backtest. Without it the optimiser and the
    backtest disagreed on the same parameters.
    """
    import vbt_strategies as strat
    import vbt_portfolio as pf

    frame = (request or {}).get('_ohlc_frame')
    o_s = h_s = l_s = v_s = None
    if frame is not None:
        f = frame.reindex(close_series.index)
        o_s = f['Open'] if 'Open' in f.columns else None
        h_s = f['High'] if 'High' in f.columns else None
        l_s = f['Low'] if 'Low' in f.columns else None
        v_s = f['Volume'] if 'Volume' in f.columns else None
    entries, exits = strat.build_strategy_signals(
        vbt, strategy_type, close_series, params,
        high_series=h_s, low_series=l_s, volume_series=v_s,
    )
    return pf.build_portfolio(vbt, close_series, entries, exits, initial_capital, request, open_series=o_s)


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

    results = []

    for combo in combinations:
        params = dict(zip(param_names, [_to_python_type(v) for v in combo]))

        check_cancelled(request)
        try:
            portfolio = _signals_and_portfolio(vbt, close_series, strategy_type, params, initial_capital, request)

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


def _daily_index(index) -> bool:
    """True when no bar of `index` carries a time of day."""
    try:
        idx = pd.DatetimeIndex(index)
        return not bool(((idx.hour != 0) | (idx.minute != 0) | (idx.second != 0)).any())
    except Exception:
        return True


def _warmup_bars(params: Dict[str, Any]) -> int:
    """Bars of history a parameter set's indicators need before they produce values:
    the largest integer period-like parameter, doubled (EMA / MACD settle slowly),
    at least 30."""
    longest = 0
    for v in (params or {}).values():
        try:
            f = float(v)
        except (TypeError, ValueError):
            continue
        if np.isfinite(f) and f >= 2 and float(f).is_integer():
            longest = max(longest, int(f))
    return max(30, 2 * longest)


def _perf_from_equity(values: pd.Series, fallback: Dict[str, Any]) -> Dict[str, Any]:
    """Return / Sharpe / Sortino / max drawdown of an equity slice (rebased to its first
    value); the trade count stays the evaluated run's."""
    perf = dict(fallback)
    v = values.dropna()
    if len(v) < 2 or float(v.iloc[0]) <= 0:
        return perf
    rets = v.pct_change().dropna()
    # Bars per year from the slice itself (252 for stocks, 365 for crypto, more intraday).
    ann = np.sqrt(252.0)
    try:
        span_days = (v.index[-1] - v.index[0]).total_seconds() / 86400.0
        if span_days > 0:
            ann = np.sqrt(max(1.0, (len(v) - 1) / span_days * 365.25))
    except Exception:
        pass
    std = float(rets.std()) if len(rets) > 1 else 0.0
    down = rets[rets < 0]
    dstd = float(down.std()) if len(down) > 1 else 0.0
    perf['totalReturn'] = float(v.iloc[-1] / v.iloc[0] - 1.0)
    perf['sharpeRatio'] = float(rets.mean() / std * ann) if std > 0 else 0.0
    perf['sortinoRatio'] = float(rets.mean() / dstd * ann) if dstd > 0 else 0.0
    perf['maxDrawdown'] = float(abs((v / v.cummax() - 1.0).min()))
    return perf


def _evaluate_params(
    vbt, close_series, strategy_type, params,
    initial_capital, request, with_equity: bool = False
):
    """Evaluate a single parameter set and return performance metrics
    (and, with `with_equity`, a (metrics, equity Series or None) pair)."""

    try:
        portfolio = _signals_and_portfolio(vbt, close_series, strategy_type, params, initial_capital, request)

        total_return = float(portfolio.total_return())
        stats = portfolio.stats()

        def safe_s(key, default=0.0):
            try:
                v = float(stats.get(key, default))
                return v if np.isfinite(v) else default
            except:
                return default

        perf = {
            'totalReturn': total_return,
            'sharpeRatio': safe_s('Sharpe Ratio'),
            'sortinoRatio': safe_s('Sortino Ratio'),
            'maxDrawdown': abs(safe_s('Max Drawdown [%]')) / 100,
            'totalTrades': int(safe_s('Total Trades')),
        }
        if with_equity:
            try:
                values = portfolio.value()
            except Exception:
                values = None
            return perf, values
        return perf
    except Exception:
        perf = {'totalReturn': 0, 'sharpeRatio': 0, 'sortinoRatio': 0, 'maxDrawdown': 0, 'totalTrades': 0}
        return (perf, None) if with_equity else perf


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
        # bool(): a numpy bool is serialised by json_response's default=str as "True".
        'significant': bool(p_value < 0.05),
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
