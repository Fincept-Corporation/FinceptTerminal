"""
Backtesting.py Provider Implementation

Lightweight, fast backtesting using the backtesting.py library.
Event-driven bar-by-bar processing with comprehensive metrics.

FEATURES IMPLEMENTED:
=====================

Core Backtesting:
- Backtest.run() - Execute backtests with comprehensive statistics
- Strategy execution - Event-driven bar-by-bar processing
- Performance metrics - Sharpe, Sortino, Calmar, max drawdown, etc.
- Trade tracking - Entry/exit prices, P&L, holding periods

Configuration Options:
- Initial capital (cash parameter)
- Commission (fixed/percentage)
- Trade on close (fill at closing price vs next open)
- Hedging (simultaneous long/short positions)
- Exclusive orders (auto-close previous trades)
- Margin/Leverage (margin parameter for leveraged trading)

Strategy Features:
- SMA Crossover (default strategy)
- Custom Python strategies (via code parameter)
- Stop-loss orders (sl parameter in buy/sell)
- Take-profit orders (tp parameter in buy/sell)
- Position sizing (fractional or absolute)
- Long and short trading

Optimization:
- Grid search optimization
- Parameter ranges (min, max, step)
- Multiple objectives (Sharpe, return, profit factor, etc.)
- Constraint support

Indicators:
- SMA (Simple Moving Average)
- EMA (Exponential Moving Average)
- RSI (Relative Strength Index)
- Custom indicators via I() method

Data Handling:
- OHLCV data support
- GOOG test data (built-in from backtesting.py)
- Synthetic data generation (fallback)
- Date range filtering

FEATURES NOT IMPLEMENTED:
=========================
- plot() - Interactive HTML charts (requires browser integration)
- SAMBO optimization (model-based optimization)
- Heatmap generation
- Live trading integration
- Real-time data fetching (uses synthetic data)

LIMITATIONS:
============
- Custom strategies require valid Python code
- No built-in data providers (uses synthetic data)
- Results depend on data quality
"""

import sys
import json
import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional
from pathlib import Path
from datetime import datetime
from dataclasses import asdict

# Import base provider
sys.path.append(str(Path(__file__).parent.parent))
from base.base_provider import (
    BacktestingProviderBase,
    BacktestResult,
    PerformanceMetrics,
    Trade,
    EquityPoint,
    BacktestStatistics,
    OptimizationResult,
    json_response,
    parse_json_input
)


class BacktestingPyProvider(BacktestingProviderBase):
    """
    Backtesting.py Provider

    Executes fast event-driven backtests using backtesting.py library.
    Simple API, interactive visualizations, parameter optimization.
    """

    def __init__(self):
        super().__init__()
        self.bt_module = None
        self.Strategy = None

    # ========================================================================
    # Properties
    # ========================================================================

    @property
    def name(self) -> str:
        return "Backtesting.py"

    @property
    def version(self) -> str:
        try:
            import backtesting
            return backtesting.__version__
        except:
            return "0.3.3"

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': True,
            'liveTrading': False,
            'research': True,
            'multiAsset': ['equity', 'crypto', 'forex', 'futures'],
            'indicators': True,
            'customStrategies': True,
            'maxConcurrentBacktests': 20,
            'eventDriven': True,
            'interactiveCharts': True,
            'vectorization': False,  # Event-driven, not vectorized
        }

    # ========================================================================
    # Core Methods
    # ========================================================================

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize Backtesting.py provider"""
        try:
            # Import backtesting library
            import backtesting
            from backtesting import Backtest, Strategy
            from backtesting.lib import crossover

            self.bt_module = backtesting
            self.Strategy = Strategy
            self.crossover = crossover

            self.config = config
            return self._create_success_result(f'Backtesting.py {self.version} ready')

        except ImportError as e:
            self._error('Backtesting.py not installed', e)
            return self._create_error_result('Backtesting.py not installed. Run: pip install backtesting')
        except Exception as e:
            self._error('Failed to initialize Backtesting.py', e)
            return self._create_error_result(f'Initialization error: {str(e)}')

    def test_connection(self) -> Dict[str, Any]:
        """Test if backtesting.py is available"""
        try:
            import backtesting
            return self._create_success_result('Backtesting.py is available')
        except ImportError:
            return self._create_error_result('Backtesting.py not installed')

    def disconnect(self) -> Dict[str, Any]:
        """No-op for subprocess providers"""
        return self._create_success_result('Disconnected')

    # ========================================================================
    # Backtest Execution
    # ========================================================================

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """
        Run backtest using backtesting.py

        Args:
            request: Backtest configuration with strategy, data, settings

        Returns:
            BacktestResult with performance metrics, trades, equity curve
        """
        try:
            self._log('Starting backtest execution')

            # Extract request parameters
            strategy_def = request.get('strategy', {})
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 10000)

            # Canonical input is `symbols: [str]` (sent by the C++ screen).
            # `assets: [{symbol, ...}]` is accepted as a fallback for legacy
            # callers that pass per-asset metadata.
            symbols = request.get('symbols', [])
            if not symbols:
                assets = request.get('assets', [])
                symbols = [a.get('symbol') for a in assets if isinstance(a, dict) and a.get('symbol')]

            if not symbols:
                return self._create_error_result('No symbols specified')

            symbol = symbols[0]

            # Load or generate data
            data = self._load_data(symbol, start_date, end_date)

            if data is None or len(data) == 0:
                return self._create_error_result(f'No data available for {symbol}')

            # Create strategy class from definition
            strategy_class = self._create_strategy_class(strategy_def)

            # Configure backtest
            from backtesting import Backtest

            # Extract advanced parameters
            commission = request.get('commission', 0.0)
            trade_on_close = request.get('tradeOnClose', False)
            hedging = request.get('hedging', False)
            exclusive_orders = request.get('exclusiveOrders', False)
            margin = request.get('margin', 1.0)  # 1.0 = no leverage

            bt = Backtest(
                data,
                strategy_class,
                cash=initial_capital,
                commission=commission,
                trade_on_close=trade_on_close,
                hedging=hedging,
                exclusive_orders=exclusive_orders,
                margin=margin
            )

            # Run backtest
            self._log(f'Running backtest for {symbol} from {start_date} to {end_date}')
            stats = bt.run()

            # Detect if synthetic data was used (no yfinance available or fetch failed)
            using_synthetic = not self._has_real_data(symbol)

            # Convert results to our format
            result = self._convert_results(stats, symbol, start_date, end_date, initial_capital)

            self._log('Backtest completed successfully')
            result_dict = asdict(result)
            result_dict['using_synthetic_data'] = using_synthetic

            if using_synthetic:
                result_dict['synthetic_data_warning'] = (
                    'WARNING: This backtest used SYNTHETIC (fake) data because real market data '
                    'could not be loaded. Install yfinance (pip install yfinance) and ensure '
                    'internet connectivity for real results. These results have NO financial meaning.'
                )

            # Add backtesting.py-specific extended stats
            result_dict['extended_stats'] = {
                'sqn': self._safe_stat(stats, 'SQN', 0),
                'kellyCriterion': self._safe_stat(stats, 'Kelly Criterion', 0),
                'exposureTime': self._safe_stat(stats, 'Exposure Time [%]', 0) / 100.0,
                'buyAndHoldReturn': self._safe_stat(stats, 'Buy & Hold Return [%]', 0) / 100.0,
                'avgDrawdown': self._safe_stat(stats, 'Avg. Drawdown [%]', 0) / 100.0,
                'maxDrawdownDuration': str(stats.get('Max. Drawdown Duration', '')),
                'avgDrawdownDuration': str(stats.get('Avg. Drawdown Duration', '')),
                'avgTradeDuration': str(stats.get('Avg. Trade Duration', '')),
                'maxTradeDuration': str(stats.get('Max. Trade Duration', '')),
                'cagr': self._safe_stat(stats, 'CAGR [%]', 0) / 100.0,
            }

            # --- Advanced Metrics ---
            try:
                from base.advanced_metrics import calculate_all as calc_advanced

                # Get equity array and dates
                equity_curve_data = stats.get('_equity_curve')
                if equity_curve_data is not None and len(equity_curve_data) > 0:
                    equity_values = equity_curve_data['Equity'].values.astype(float)
                    dates_list = [str(d).split(' ')[0] for d in equity_curve_data.index]

                    # Load benchmark if requested
                    benchmark_symbol = request.get('benchmark', '')
                    benchmark_normalized = None
                    if benchmark_symbol:
                        benchmark_normalized = self._load_benchmark(
                            benchmark_symbol, start_date, end_date, len(equity_values)
                        )
                        # Add benchmark equity to equity curve points (scaled to initial_capital)
                        if benchmark_normalized is not None and 'equity' in result_dict:
                            for i, point in enumerate(result_dict['equity']):
                                if i < len(benchmark_normalized):
                                    point['benchmark'] = float(benchmark_normalized[i] * initial_capital)

                    # For advanced_metrics, pass benchmark as equity-scale values
                    # (same scale as equity_values) so alpha/beta calculation is correct
                    benchmark_equity = None
                    if benchmark_normalized is not None:
                        benchmark_equity = benchmark_normalized * equity_values[0]

                    advanced = calc_advanced(
                        equity_values,
                        benchmark_series=benchmark_equity,
                        risk_free_rate=0.0,
                        dates=dates_list,
                    )
                    result_dict['advanced_metrics'] = advanced
                    result_dict['monthly_returns'] = advanced.pop('monthlyReturns', [])
                    result_dict['rolling_metrics'] = {
                        'rollingSharpe': advanced.pop('rollingSharpe', []),
                        'rollingVolatility': advanced.pop('rollingVolatility', []),
                        'rollingDrawdown': advanced.pop('rollingDrawdown', []),
                    }
            except Exception as adv_err:
                self._error('Advanced metrics calculation failed (non-fatal)', adv_err)

            return {
                'success': True,
                'message': 'Backtest completed',
                'data': result_dict
            }

        except Exception as e:
            self._error('Backtest execution failed', e)
            return self._create_error_result(f'Backtest failed: {str(e)}')

    # ========================================================================
    # Optimization
    # ========================================================================

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run parameter optimization via backtesting.py's `bt.optimize`.

        The frontend sweeps strategy params under their *frontend* names (e.g.
        `fastPeriod`); backtesting.py optimizes class-level attribute names
        (e.g. `n1`). `btp_strategies.STRATEGY_OPTIMIZE_PARAM_MAP` translates.

        Returns the same shape as run_backtest (`data.performance`, `data.trades`,
        `data.equity`) so the existing screen renderer works unchanged, plus
        optimization-specific fields under `data.optimization`.
        """
        try:
            self._log('Starting parameter optimization')

            strategy_def = request.get('strategy', {})
            strategy_type = strategy_def.get('type', 'sma_crossover')
            # Frontend canonical name is `paramRanges`; legacy callers used `parameters`.
            param_ranges = request.get('paramRanges', {}) or request.get('parameters', {})
            objective = request.get('optimizeObjective') or request.get('metric', 'sharpe')
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 10000)
            max_iterations = int(request.get('maxIterations', 500) or 500)

            # Canonical: `symbols: [str]`. Fallback: `assets: [{symbol, ...}]`.
            symbols = request.get('symbols', [])
            if not symbols:
                assets = request.get('assets', [])
                symbols = [a.get('symbol') for a in assets if isinstance(a, dict) and a.get('symbol')]

            if not param_ranges:
                return self._create_error_result('No parameter ranges supplied (expected paramRanges)')

            symbol = symbols[0] if symbols else 'SPY'
            data = self._load_data(symbol, start_date, end_date)
            if data is None or len(data) == 0:
                return self._create_error_result(f'No data available for {symbol}')

            # Build the strategy class with default params; bt.optimize will
            # sweep class-level attributes from there.
            strategy_class = self._create_strategy_class(strategy_def)

            # Translate frontend param names -> class attribute names per strategy.
            import btp_strategies as _strat
            mapped_ranges = _strat.map_optimize_params(strategy_type, param_ranges)

            # Build python ranges. Drop any range that targets an attribute the
            # strategy class doesn't declare — backtesting.py would reject it
            # otherwise and we'd get the cryptic "missing parameter X" error.
            opt_kwargs = {}
            skipped: List[str] = []
            for cattr, cfg in mapped_ranges.items():
                if not hasattr(strategy_class, cattr):
                    skipped.append(cattr)
                    continue
                try:
                    lo = int(cfg.get('min', 1))
                    hi = int(cfg.get('max', 100))
                    step = max(1, int(cfg.get('step', 1)))
                except Exception:
                    continue
                if hi < lo:
                    lo, hi = hi, lo
                opt_kwargs[cattr] = range(lo, hi + 1, step)

            if not opt_kwargs:
                return self._create_error_result(
                    f"No optimizable parameters for strategy '{strategy_type}'. "
                    f"Mapped attrs not present on class: {skipped}"
                )

            if skipped:
                self._log(f'Skipped non-class attrs during optimize: {skipped}')

            from backtesting import Backtest
            bt = Backtest(
                data, strategy_class,
                cash=initial_capital,
                commission=request.get('commission', 0.0),
            )

            # Map frontend objective to backtesting.py stat key.
            metric_map = {
                'sharpe':  'Sharpe Ratio',
                'sortino': 'Sortino Ratio',
                'calmar':  'Calmar Ratio',
                'return':  'Return [%]',
                # Tolerate human-readable variants for callers outside the screen.
                'Sharpe Ratio': 'Sharpe Ratio',
                'Sortino Ratio': 'Sortino Ratio',
                'Calmar Ratio': 'Calmar Ratio',
                'Total Return': 'Return [%]',
            }
            maximize_metric = metric_map.get(objective, 'Sharpe Ratio')

            self._log(f'Optimizing {len(opt_kwargs)} attrs (max_tries={max_iterations}) maximize={maximize_metric}')
            stats = bt.optimize(**opt_kwargs, maximize=maximize_metric, max_tries=max_iterations)

            # Extract chosen optimal params back under frontend names.
            inverse_map = {cattr: fname for fname, cattr in
                           _strat.STRATEGY_OPTIMIZE_PARAM_MAP.get(strategy_type, {}).items()}
            optimal_params: Dict[str, Any] = {}
            for cattr in opt_kwargs.keys():
                if not hasattr(stats._strategy, cattr):
                    continue
                fname = inverse_map.get(cattr, cattr)
                val = getattr(stats._strategy, cattr)
                optimal_params[fname] = int(val) if hasattr(val, '__int__') else val

            # Wire shape: same as run_backtest so display_result() renders performance/trades.
            result_dict = self._convert_results(
                stats, symbol, start_date, end_date, initial_capital
            ).to_dict()

            using_synthetic = not self._has_real_data(symbol)
            result_dict['using_synthetic_data'] = using_synthetic
            result_dict['optimization'] = {
                'objective': objective,
                'objective_value': float(stats.get(maximize_metric, 0)),
                'best_params': optimal_params,
                'iterations': int(max_iterations),
                'attrs_optimized': list(opt_kwargs.keys()),
                'attrs_skipped': skipped,
            }

            self._log(
                f"Optimization done: best {maximize_metric} = "
                f"{result_dict['optimization']['objective_value']:.4f} with {optimal_params}"
            )
            return {'success': True, 'message': 'Optimization completed', 'data': result_dict}

        except Exception as e:
            self._error('Optimization failed', e)
            import traceback
            return self._create_error_result(f'Optimization failed: {str(e)}\n{traceback.format_exc()}')

    def walk_forward(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Anchored walk-forward optimization.

        Split the price history into N folds. For each fold:
          - Train segment = first `trainRatio` portion of the fold's window.
          - Test segment  = the remainder.
          - Optimize on train, then run a single backtest on test using the
            best params found on train (genuine out-of-sample evaluation).

        Aggregate metric = mean Sharpe Ratio across the test segments. The
        returned shape mirrors run_backtest (`performance`, `trades`, `equity`)
        on the final fold's test run, so the screen renders without changes.
        Per-fold detail goes into `data.walkForward.folds`.
        """
        try:
            self._log('Starting walk-forward optimization')

            strategy_def = request.get('strategy', {})
            strategy_type = strategy_def.get('type', 'sma_crossover')
            param_ranges = request.get('paramRanges', {}) or request.get('parameters', {})
            objective = request.get('optimizeObjective') or request.get('metric', 'sharpe')
            n_splits = int(request.get('wfSplits', request.get('nSplits', 5)) or 5)
            train_ratio = float(request.get('wfTrainRatio', request.get('trainRatio', 0.7)) or 0.7)
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 10000)
            commission = request.get('commission', 0.0)

            symbols = request.get('symbols', [])
            if not symbols:
                assets = request.get('assets', [])
                symbols = [a.get('symbol') for a in assets if isinstance(a, dict) and a.get('symbol')]
            if not param_ranges:
                return self._create_error_result('No parameter ranges supplied (expected paramRanges)')
            if not (1 <= n_splits <= 20):
                return self._create_error_result(f'wfSplits must be between 1 and 20 (got {n_splits})')
            if not (0.1 <= train_ratio <= 0.95):
                return self._create_error_result(f'wfTrainRatio must be between 0.1 and 0.95 (got {train_ratio})')

            symbol = symbols[0] if symbols else 'SPY'
            full_data = self._load_data(symbol, start_date, end_date)
            if full_data is None or len(full_data) < n_splits * 20:
                return self._create_error_result(
                    f'Need at least {n_splits * 20} bars for {n_splits} folds; got {0 if full_data is None else len(full_data)}'
                )

            import btp_strategies as _strat
            from backtesting import Backtest

            metric_map = {
                'sharpe': 'Sharpe Ratio', 'sortino': 'Sortino Ratio',
                'calmar': 'Calmar Ratio', 'return': 'Return [%]',
            }
            maximize_metric = metric_map.get(objective, 'Sharpe Ratio')

            n = len(full_data)
            fold_size = n // n_splits
            folds: List[Dict[str, Any]] = []
            test_returns: List[float] = []
            test_sharpes: List[float] = []
            last_test_stats = None
            last_test_data = None

            for i in range(n_splits):
                fold_start = i * fold_size
                fold_end = (i + 1) * fold_size if i < n_splits - 1 else n
                window = full_data.iloc[fold_start:fold_end]
                if len(window) < 20:
                    continue
                split = int(len(window) * train_ratio)
                train, test = window.iloc[:split], window.iloc[split:]
                if len(train) < 10 or len(test) < 5:
                    continue

                # Train: optimize on this slice.
                train_class = self._create_strategy_class(strategy_def)
                mapped = _strat.map_optimize_params(strategy_type, param_ranges)
                opt_kwargs: Dict[str, Any] = {}
                for cattr, cfg in mapped.items():
                    if not hasattr(train_class, cattr):
                        continue
                    try:
                        lo, hi = int(cfg.get('min', 1)), int(cfg.get('max', 100))
                        step = max(1, int(cfg.get('step', 1)))
                    except Exception:
                        continue
                    if hi < lo:
                        lo, hi = hi, lo
                    opt_kwargs[cattr] = range(lo, hi + 1, step)

                if not opt_kwargs:
                    continue

                bt_train = Backtest(train, train_class, cash=initial_capital, commission=commission)
                train_stats = bt_train.optimize(**opt_kwargs, maximize=maximize_metric, max_tries=100)

                # Best params back -> frontend names.
                inverse = {c: f for f, c in _strat.STRATEGY_OPTIMIZE_PARAM_MAP.get(strategy_type, {}).items()}
                best_params: Dict[str, Any] = {}
                for cattr in opt_kwargs.keys():
                    if hasattr(train_stats._strategy, cattr):
                        v = getattr(train_stats._strategy, cattr)
                        best_params[inverse.get(cattr, cattr)] = int(v) if hasattr(v, '__int__') else v

                # Test: backtest the *test* slice with those params.
                test_strategy_def = dict(strategy_def)
                test_strategy_def['params'] = {**(strategy_def.get('params') or {}), **best_params}
                test_class = self._create_strategy_class(test_strategy_def)
                bt_test = Backtest(test, test_class, cash=initial_capital, commission=commission)
                test_stats = bt_test.run()
                last_test_stats = test_stats
                last_test_data = test

                test_ret = float(test_stats.get('Return [%]', 0))
                test_sharpe = float(test_stats.get('Sharpe Ratio', 0))
                test_returns.append(test_ret)
                test_sharpes.append(test_sharpe)

                folds.append({
                    'fold': i,
                    'trainStart': str(train.index[0]),
                    'trainEnd': str(train.index[-1]),
                    'trainBars': len(train),
                    'testStart': str(test.index[0]),
                    'testEnd': str(test.index[-1]),
                    'testBars': len(test),
                    'bestParams': best_params,
                    'trainObjective': float(train_stats.get(maximize_metric, 0)),
                    'testReturnPct': test_ret,
                    'testSharpe': test_sharpe,
                })

            if not folds or last_test_stats is None:
                return self._create_error_result('No usable folds — try fewer splits or a longer date range')

            # Surface the LAST fold's test result via the standard
            # run_backtest shape so display_result() renders something useful.
            result_dict = self._convert_results(
                last_test_stats, symbol,
                str(last_test_data.index[0]), str(last_test_data.index[-1]),
                initial_capital,
            ).to_dict()
            result_dict['using_synthetic_data'] = not self._has_real_data(symbol)
            result_dict['walk_forward'] = {
                'objective': objective,
                'n_splits': n_splits,
                'train_ratio': train_ratio,
                'folds': folds,
                'mean_test_return_pct': sum(test_returns) / len(test_returns),
                'mean_test_sharpe': sum(test_sharpes) / len(test_sharpes),
            }

            self._log(
                f'Walk-forward done: {len(folds)} folds, mean test Sharpe = '
                f'{result_dict["walk_forward"]["mean_test_sharpe"]:.4f}'
            )
            return {'success': True, 'message': 'Walk-forward completed', 'data': result_dict}

        except Exception as e:
            self._error('Walk-forward failed', e)
            import traceback
            return self._create_error_result(f'Walk-forward failed: {str(e)}\n{traceback.format_exc()}')

    # ========================================================================
    # Data and Indicators (Required Abstract Methods)
    # ========================================================================

    def get_historical_data(self, request: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Get historical price data"""
        try:
            symbols = request.get('symbols', [])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            timeframe = request.get('timeframe', 'daily')

            results = []
            for symbol in symbols:
                data = self._load_data(symbol, start_date, end_date)
                if data is not None:
                    # Convert DataFrame to list of dictionaries
                    data_list = []
                    for idx, row in data.iterrows():
                        data_list.append({
                            'date': str(idx),
                            'open': float(row['Open']),
                            'high': float(row['High']),
                            'low': float(row['Low']),
                            'close': float(row['Close']),
                            'volume': int(row['Volume'])
                        })

                    results.append({
                        'symbol': symbol,
                        'timeframe': timeframe,
                        'data': data_list
                    })

            return results

        except Exception as e:
            self._error('Failed to get historical data', e)
            return []

    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate technical indicator"""
        try:
            # Accept both `symbol: str` (legacy) and `symbols: [str]` (canonical).
            symbol = params.get('symbol')
            if not symbol:
                syms = params.get('symbols') or []
                symbol = syms[0] if syms else 'SPY'
            # Support nested params dict (legacy) and flat top-level args (canonical).
            inner = params.get('params') if isinstance(params.get('params'), dict) else params
            period = inner.get('period', 20)
            start_date = params.get('startDate')
            end_date = params.get('endDate')

            # Load data
            data = self._load_data(symbol, start_date, end_date)

            if data is None:
                return self._create_error_result(f'No data available for {symbol}')

            # Calculate indicator
            values = []
            if indicator_type.lower() in ['sma', 'simple_moving_average']:
                indicator_values = data['Close'].rolling(window=period).mean()

                for idx, value in indicator_values.items():
                    if not np.isnan(value):
                        values.append({
                            'date': str(idx),
                            'value': float(value)
                        })

            elif indicator_type.lower() in ['ema', 'exponential_moving_average']:
                indicator_values = data['Close'].ewm(span=period, adjust=False).mean()

                for idx, value in indicator_values.items():
                    if not np.isnan(value):
                        values.append({
                            'date': str(idx),
                            'value': float(value)
                        })

            elif indicator_type.lower() == 'rsi':
                # Simple RSI calculation
                delta = data['Close'].diff()
                gain = (delta.where(delta > 0, 0)).rolling(window=period).mean()
                loss = (-delta.where(delta < 0, 0)).rolling(window=period).mean()
                rs = gain / loss
                rsi = 100 - (100 / (1 + rs))

                for idx, value in rsi.items():
                    if not np.isnan(value):
                        values.append({
                            'date': str(idx),
                            'value': float(value)
                        })

            else:
                return self._create_error_result(f'Indicator type {indicator_type} not supported')

            return {
                'success': True,
                'message': 'Indicator calculated',
                'data': {
                    'indicator': indicator_type,
                    'symbol': symbol,
                    'values': values
                }
            }

        except Exception as e:
            self._error('Failed to calculate indicator', e)
            return self._create_error_result(f'Indicator calculation failed: {str(e)}')

    def get_strategies(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return strategy catalog in BT shape: {strategies: {category: [...]}}."""
        import btp_strategies as _strat
        catalog = _strat.get_strategy_catalog()
        return {'success': True, 'data': {'provider': 'backtestingpy', 'strategies': catalog}}

    def get_indicators(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return indicator catalog normalized to {indicators: {Category: [{id, name}]}}."""
        try:
            import btp_indicators as _ind
            flat = _ind.get_catalog()
            grouped: Dict[str, Any] = {}
            for entry in flat:
                cat = entry.get('category', 'Other')
                grouped.setdefault(cat, []).append({
                    'id':   entry.get('id', ''),
                    'name': entry.get('label', entry.get('id', '')),
                })
        except Exception:
            grouped = {}
        return {'indicators': grouped}

    def get_command_options(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return provider-specific option lists for all command dropdowns."""
        return {
            'success': True,
            'data': {
                'position_sizing_methods': ['percent', 'fixed', 'kelly'],
                'optimize_objectives':     ['sharpe', 'sortino', 'calmar', 'return'],
                'optimize_methods':        ['grid', 'random'],
                'label_types':             [],
                'splitter_types':          [],
                'signal_generators':       [],
                'indicator_signal_modes':  ['crossover', 'threshold', 'breakout', 'mean_reversion', 'filter'],
                'returns_analysis_types':  [],
            },
        }

    # ========================================================================
    # Helper Methods
    # ========================================================================

    def _load_benchmark(self, symbol: str, start_date: str, end_date: str, target_len: int, initial_capital: float = None) -> Optional[np.ndarray]:
        """Load benchmark equity series.

        Returns normalized benchmark (starting at 1.0) for advanced_metrics calculations.
        The caller scales this to initial_capital for benchmark equity display.
        """
        try:
            import yfinance as yf
            bench_data = yf.Ticker(symbol).history(start=start_date, end=end_date, auto_adjust=True)
            if bench_data is not None and len(bench_data) > 0:
                close = bench_data['Close'].values.astype(float)
                # Normalized: start at 1.0 (relative performance)
                normalized = close / close[0]
                # Resample to match target length if needed
                if len(normalized) != target_len:
                    indices = np.linspace(0, len(normalized) - 1, target_len).astype(int)
                    normalized = normalized[indices]
                return normalized
        except Exception as e:
            self._error(f'Failed to load benchmark {symbol}', e)
        return None

    def _has_real_data(self, symbol: str) -> bool:
        """Check if real data was actually used (not synthetic fallback).

        This tracks whether _load_data ended up using real data or fell back
        to synthetic generation. The flag is set during _load_data execution.
        """
        return getattr(self, '_last_load_was_real', False)

    def _load_data(self, symbol: str, start_date: str, end_date: str) -> Optional[pd.DataFrame]:
        """Load or generate OHLCV data for backtesting.

        Sets self._last_load_was_real to track whether real or synthetic data was used.
        """
        self._last_load_was_real = False  # Assume synthetic until proven otherwise
        try:
            # Try to use test data if available
            if symbol == 'GOOG' or symbol == 'GOOGL':
                try:
                    from backtesting.test import GOOG
                    data = GOOG.copy()

                    # Filter by date if specified
                    if start_date and start_date != '':
                        try:
                            start_ts = pd.Timestamp(start_date)
                            data = data[data.index >= start_ts]
                        except:
                            pass

                    if end_date and end_date != '':
                        try:
                            end_ts = pd.Timestamp(end_date)
                            data = data[data.index <= end_ts]
                        except:
                            pass

                    self._log(f'Loaded {len(data)} bars of GOOG test data (from {data.index[0]} to {data.index[-1]})')
                    if len(data) > 0:
                        self._last_load_was_real = True
                        return data
                    return None
                except Exception as e:
                    self._log(f'Could not load GOOG test data: {e}')
                    # Fall through to yfinance / synthetic data

            # Try to fetch real data using yfinance if available
            try:
                import yfinance as yf
                self._log(f'Fetching real market data for {symbol} via yfinance')
                ticker = yf.Ticker(symbol)
                data = ticker.history(
                    start=start_date or '2020-01-01',
                    end=end_date or None,
                    auto_adjust=True
                )
                if data is not None and len(data) > 0:
                    # Ensure column names match expected format
                    data.columns = [c.capitalize() if c.lower() in ['open', 'high', 'low', 'close', 'volume'] else c for c in data.columns]
                    # Keep only OHLCV columns
                    ohlcv_cols = ['Open', 'High', 'Low', 'Close', 'Volume']
                    available_cols = [c for c in ohlcv_cols if c in data.columns]
                    data = data[available_cols]
                    # Round to 4 decimal places to eliminate float32 rounding noise
                    for col in ['Open', 'High', 'Low', 'Close']:
                        if col in data.columns:
                            data[col] = data[col].round(4)
                    self._log(f'Loaded {len(data)} bars of real data for {symbol}')
                    self._last_load_was_real = True
                    return data
                else:
                    self._log(f'No data returned from yfinance for {symbol}')
            except ImportError:
                self._log('yfinance not installed - falling back to synthetic data. Install with: pip install yfinance')
            except Exception as e:
                self._log(f'Failed to fetch real data for {symbol}: {e} - falling back to synthetic data')

            # Fallback: Generate synthetic data with clear warning
            self._last_load_was_real = False
            import sys
            print(f'[WARNING] Using SYNTHETIC data for {symbol}. Results are NOT based on real market data. '
                  f'Install yfinance (pip install yfinance) for real data.', file=sys.stderr)
            self._log(f'WARNING: Generating SYNTHETIC data for {symbol} - results will not reflect real market behavior')

            date_range = pd.date_range(
                start=start_date or '2020-01-01',
                end=end_date or '2024-12-31',
                freq='B'  # Business days only for more realistic data
            )

            # Generate synthetic price data with random seed based on symbol
            # so different symbols produce different data
            seed = sum(ord(c) for c in symbol) % (2**31)
            rng = np.random.default_rng(seed)
            returns = rng.normal(0.0003, 0.015, len(date_range))  # Realistic daily returns
            close = 100 * np.exp(np.cumsum(returns))

            data = pd.DataFrame({
                'Open': close * (1 + rng.normal(0, 0.005, len(date_range))),
                'High': close * (1 + np.abs(rng.normal(0, 0.01, len(date_range)))),
                'Low': close * (1 - np.abs(rng.normal(0, 0.01, len(date_range)))),
                'Close': close,
                'Volume': rng.integers(1000000, 10000000, len(date_range))
            }, index=date_range)

            # Ensure High is highest and Low is lowest
            data['High'] = data[['Open', 'High', 'Close']].max(axis=1)
            data['Low'] = data[['Open', 'Low', 'Close']].min(axis=1)

            return data

        except Exception as e:
            self._error(f'Failed to load data for {symbol}', e)
            return None

    def _create_strategy_class(self, strategy_def: Dict[str, Any], opt_params: Dict = None):
        """Build a backtesting.py Strategy class.

        For registered strategy ids (the 26 in btp_strategies.STRATEGY_BUILDERS)
        the builder lives in btp_strategies and uses class-level attributes that
        the optimize path can sweep. Frontend params (e.g. `fastPeriod`) are
        consumed by the builder; class-level names (e.g. `n1`, `n2`) are what
        backtesting.py's `bt.optimize(**ranges)` operates on.

        For custom user code we keep the existing sandboxed exec path.
        """
        from backtesting import Strategy
        from backtesting.lib import crossover  # noqa: F401  (used by user code)

        strategy_type = strategy_def.get('type', 'sma_crossover')
        # Frontend sends `params`; legacy callers used `parameters`.
        params = strategy_def.get('params') or strategy_def.get('parameters') or {}
        code = strategy_def.get('code', '')

        # Registered strategy id -> delegate to btp_strategies builder.
        import btp_strategies as _strat
        if strategy_type in _strat.STRATEGY_BUILDERS:
            return _strat.STRATEGY_BUILDERS[strategy_type](params, opt_params)

        # Custom user-supplied strategy code (sandboxed).
        if code:
            validation_error = self._validate_strategy_code(code)
            if validation_error:
                self._error(f'Strategy code validation failed: {validation_error}')
                return _strat.STRATEGY_BUILDERS['sma_crossover'](params, opt_params)
            exec_globals = {
                '__builtins__': {
                    'range': range, 'len': len, 'abs': abs, 'min': min, 'max': max,
                    'sum': sum, 'round': round, 'int': int, 'float': float, 'bool': bool,
                    'str': str, 'list': list, 'dict': dict, 'tuple': tuple,
                    'enumerate': enumerate, 'zip': zip, 'map': map, 'filter': filter,
                    'sorted': sorted, 'reversed': reversed, 'isinstance': isinstance,
                    'issubclass': issubclass, 'hasattr': hasattr, 'getattr': getattr,
                    'setattr': setattr, 'property': property, 'staticmethod': staticmethod,
                    'classmethod': classmethod, 'super': super, 'type': type,
                    'True': True, 'False': False, 'None': None,
                },
                'Strategy': Strategy,
                'crossover': crossover,
                'pd': pd,
                'np': np,
            }
            try:
                exec(code, exec_globals)
                for _name, obj in exec_globals.items():
                    if isinstance(obj, type) and issubclass(obj, Strategy) and obj is not Strategy:
                        return obj
            except Exception as e:
                self._error('Failed to execute custom strategy code', e)

        # Unknown id and no code -> sensible default so the run isn't a hard fail.
        return _strat.STRATEGY_BUILDERS['sma_crossover'](params, opt_params)


    def _validate_strategy_code(self, code: str) -> Optional[str]:
        """
        Validate custom strategy code for safety.
        Returns error message if code is unsafe, None if valid.
        """
        import ast

        # Forbidden module imports
        forbidden_modules = {
            'os', 'sys', 'subprocess', 'shutil', 'socket', 'http',
            'urllib', 'requests', 'ftplib', 'smtplib', 'telnetlib',
            'ctypes', 'multiprocessing', 'threading', 'signal',
            'importlib', 'builtins', '__builtin__', 'code', 'codeop',
            'compile', 'compileall', 'py_compile', 'zipimport',
            'pkgutil', 'modulefinder', 'runpy', 'pathlib', 'glob',
            'tempfile', 'io', 'pickle', 'shelve', 'marshal',
            'dbm', 'sqlite3', 'webbrowser', 'cmd', 'pdb',
        }

        # Forbidden function calls
        forbidden_calls = {
            'exec', 'eval', 'compile', '__import__', 'open',
            'input', 'breakpoint', 'exit', 'quit',
            'globals', 'locals', 'vars', 'dir',
            'delattr', 'object.__subclasses__',
        }

        # Forbidden attribute access patterns
        forbidden_attrs = {
            '__class__', '__subclasses__', '__bases__', '__mro__',
            '__globals__', '__code__', '__closure__', '__func__',
            '__self__', '__module__', '__import__', '__builtins__',
            '__qualname__', '__wrapped__',
        }

        try:
            tree = ast.parse(code)
        except SyntaxError as e:
            return f'Syntax error in strategy code: {e}'

        for node in ast.walk(tree):
            # Check imports
            if isinstance(node, ast.Import):
                for alias in node.names:
                    module_name = alias.name.split('.')[0]
                    if module_name in forbidden_modules:
                        return f'Forbidden import: {alias.name}'

            if isinstance(node, ast.ImportFrom):
                if node.module:
                    module_name = node.module.split('.')[0]
                    if module_name in forbidden_modules:
                        return f'Forbidden import from: {node.module}'

            # Check function calls
            if isinstance(node, ast.Call):
                if isinstance(node.func, ast.Name):
                    if node.func.id in forbidden_calls:
                        return f'Forbidden function call: {node.func.id}'
                elif isinstance(node.func, ast.Attribute):
                    if node.func.attr in forbidden_calls:
                        return f'Forbidden method call: {node.func.attr}'

            # Check attribute access
            if isinstance(node, ast.Attribute):
                if node.attr in forbidden_attrs:
                    return f'Forbidden attribute access: {node.attr}'

        # Check code length (prevent extremely large code blocks)
        if len(code) > 10000:
            return 'Strategy code too long (max 10000 characters)'

        return None

    def _convert_results(self, stats, symbol: str, start_date: str, end_date: str, initial_capital: float) -> BacktestResult:
        """Convert backtesting.py stats to BacktestResult"""

        # Extract performance metrics
        metrics = self._extract_performance_metrics(stats)

        # Extract trades
        trades = self._extract_trades(stats, symbol=symbol)

        # Extract equity curve
        equity_curve = self._extract_equity_curve(stats)

        # Extract statistics including daily analysis
        winning_days = 0
        losing_days = 0
        avg_daily_return = 0.0
        best_day = 0.0
        worst_day = 0.0
        consecutive_wins = 0
        consecutive_losses = 0

        equity_curve_df = stats.get('_equity_curve')
        if equity_curve_df is not None and len(equity_curve_df) > 1:
            equity_vals = equity_curve_df['Equity'].values.astype(float)
            daily_returns = np.diff(equity_vals) / equity_vals[:-1]
            daily_returns = daily_returns[np.isfinite(daily_returns)]

            if len(daily_returns) > 0:
                winning_days = int(np.sum(daily_returns > 0))
                losing_days = int(np.sum(daily_returns < 0))
                avg_daily_return = float(np.mean(daily_returns))
                best_day = float(np.max(daily_returns))
                worst_day = float(np.min(daily_returns))

                # Calculate consecutive wins/losses
                max_consec_wins = 0
                max_consec_losses = 0
                cur_wins = 0
                cur_losses = 0
                for r in daily_returns:
                    if r > 0:
                        cur_wins += 1
                        cur_losses = 0
                        max_consec_wins = max(max_consec_wins, cur_wins)
                    elif r < 0:
                        cur_losses += 1
                        cur_wins = 0
                        max_consec_losses = max(max_consec_losses, cur_losses)
                    else:
                        cur_wins = 0
                        cur_losses = 0
                consecutive_wins = max_consec_wins
                consecutive_losses = max_consec_losses

        # Commissions from stats (if available)
        total_fees = self._safe_stat(stats, 'Commissions [$]', 0.0)

        statistics = BacktestStatistics(
            start_date=start_date,
            end_date=end_date,
            initial_capital=initial_capital,
            final_capital=float(self._safe_stat(stats, 'Equity Final [$]', initial_capital)),
            total_fees=total_fees,
            total_slippage=0.0,
            total_trades=int(self._safe_stat(stats, '# Trades', 0)),
            winning_days=winning_days,
            losing_days=losing_days,
            average_daily_return=avg_daily_return,
            best_day=best_day,
            worst_day=worst_day,
            consecutive_wins=consecutive_wins,
            consecutive_losses=consecutive_losses,
        )

        # Create result
        result = BacktestResult(
            id='bt-' + str(int(datetime.now().timestamp())),
            status='completed',
            performance=metrics,
            trades=trades,
            equity=equity_curve,
            statistics=statistics,
            logs=[],
            start_time=start_date,
            end_time=end_date
        )

        return result

    def _extract_performance_metrics(self, stats) -> PerformanceMetrics:
        """Extract performance metrics from backtesting.py stats"""

        total_trades = int(stats.get('# Trades', 0))
        win_rate = float(self._safe_stat(stats, 'Win Rate [%]', 0)) / 100.0

        # Extract trade-level metrics from _trades DataFrame for accuracy
        trades_df = stats.get('_trades')
        winning_trades = 0
        losing_trades = 0
        avg_win = 0.0
        avg_loss = 0.0
        largest_win = 0.0
        largest_loss = 0.0

        if trades_df is not None and len(trades_df) > 0:
            returns_col = trades_df['ReturnPct'] if 'ReturnPct' in trades_df.columns else None
            pnl_col = trades_df['PnL'] if 'PnL' in trades_df.columns else None

            if returns_col is not None:
                winners = returns_col[returns_col > 0]
                losers = returns_col[returns_col <= 0]
                winning_trades = len(winners)
                losing_trades = len(losers)
                avg_win = float(winners.mean()) if len(winners) > 0 else 0.0
                avg_loss = float(losers.mean()) if len(losers) > 0 else 0.0
                largest_win = float(returns_col.max()) if len(returns_col) > 0 else 0.0
                largest_loss = float(returns_col.min()) if len(returns_col) > 0 else 0.0
            elif pnl_col is not None:
                winners = pnl_col[pnl_col > 0]
                losers = pnl_col[pnl_col <= 0]
                winning_trades = len(winners)
                losing_trades = len(losers)
        else:
            # Fallback to computed values
            winning_trades = int(total_trades * win_rate)
            losing_trades = total_trades - winning_trades

        # Extract annualized metrics - handle NaN/inf from backtesting.py
        ann_return = self._safe_stat(stats, 'Return (Ann.) [%]', 0) / 100.0
        volatility = self._safe_stat(stats, 'Volatility (Ann.) [%]', 0) / 100.0
        sharpe = self._safe_stat(stats, 'Sharpe Ratio', 0)
        sortino = self._safe_stat(stats, 'Sortino Ratio', 0)
        calmar = self._safe_stat(stats, 'Calmar Ratio', 0)

        return PerformanceMetrics(
            total_return=self._safe_stat(stats, 'Return [%]', 0) / 100.0,
            annualized_return=ann_return,
            sharpe_ratio=sharpe,
            sortino_ratio=sortino,
            max_drawdown=abs(self._safe_stat(stats, 'Max. Drawdown [%]', 0)) / 100.0,
            win_rate=win_rate,
            loss_rate=1.0 - win_rate,
            profit_factor=self._safe_stat(stats, 'Profit Factor', 0),
            volatility=volatility,
            calmar_ratio=calmar,
            total_trades=total_trades,
            winning_trades=winning_trades,
            losing_trades=losing_trades,
            average_win=avg_win,
            average_loss=avg_loss,
            largest_win=largest_win,
            largest_loss=largest_loss,
            average_trade_return=self._safe_stat(stats, 'Avg. Trade [%]', 0) / 100.0,
            expectancy=self._safe_stat(stats, 'Expectancy [%]', 0) / 100.0,
        )

    def _safe_stat(self, stats, key: str, default=0.0) -> float:
        """Safely extract a stat value, handling NaN/inf/None"""
        try:
            val = stats.get(key, default)
            if val is None:
                return float(default)
            val = float(val)
            if np.isnan(val) or np.isinf(val):
                return float(default)
            return val
        except (TypeError, ValueError):
            return float(default)

    def _extract_trades(self, stats, symbol: str = 'UNKNOWN') -> List[Trade]:
        """Extract trade list from backtesting.py stats"""
        trades = []

        try:
            trades_df = stats.get('_trades')
            if trades_df is not None and len(trades_df) > 0:
                for idx, row in trades_df.iterrows():
                    # Calculate duration in bars/days
                    duration_bars = 0
                    if 'Duration' in trades_df.columns:
                        dur = row['Duration']
                        if hasattr(dur, 'days'):
                            duration_bars = dur.days
                        elif hasattr(dur, 'total_seconds'):
                            duration_bars = int(dur.total_seconds() / 86400)
                        else:
                            try:
                                duration_bars = int(dur)
                            except:
                                duration_bars = 0

                    # Size determines direction
                    size = float(row.get('Size', 0)) if 'Size' in trades_df.columns else 0
                    side = 'long' if size > 0 else 'short'

                    # Entry/exit times
                    entry_time = str(row['EntryTime']).split(' ')[0] if 'EntryTime' in trades_df.columns else ''
                    exit_time = str(row['ExitTime']).split(' ')[0] if 'ExitTime' in trades_df.columns else ''

                    # PnL
                    pnl = float(row['PnL']) if 'PnL' in trades_df.columns else 0.0
                    pnl_pct = float(row['ReturnPct']) if 'ReturnPct' in trades_df.columns else 0.0

                    # Entry/exit prices
                    entry_price = float(row['EntryPrice']) if 'EntryPrice' in trades_df.columns else 0.0
                    exit_price = float(row['ExitPrice']) if 'ExitPrice' in trades_df.columns else 0.0

                    # Exit reason from Tag if available
                    tag_val = row.get('Tag', None) if 'Tag' in trades_df.columns else None
                    exit_reason = str(tag_val) if tag_val is not None and str(tag_val) != 'None' else 'signal'

                    trade = Trade(
                        id=f'trade_{idx}',
                        symbol=symbol,
                        entry_date=entry_time,
                        side=side,
                        quantity=abs(size),
                        entry_price=entry_price,
                        commission=0.0,
                        slippage=0.0,
                        exit_date=exit_time,
                        exit_price=exit_price,
                        pnl=pnl,
                        pnl_percent=pnl_pct,
                        holding_period=duration_bars,
                        exit_reason=exit_reason,
                    )
                    trades.append(trade)
        except Exception as e:
            self._error('Failed to extract trades', e)

        return trades

    def _extract_equity_curve(self, stats) -> List[EquityPoint]:
        """Extract equity curve from backtesting.py stats"""
        equity_curve = []

        try:
            equity_df = stats.get('_equity_curve')
            if equity_df is not None and len(equity_df) > 0:
                prev_equity = equity_df['Equity'].iloc[0] if len(equity_df) > 0 else 0

                for idx, row in equity_df.iterrows():
                    equity = float(row.get('Equity', 0))

                    # Calculate returns
                    returns = 0.0
                    if prev_equity > 0:
                        returns = (equity - prev_equity) / prev_equity

                    # DrawdownPct from backtesting.py is already a fraction (0 to 1)
                    dd = float(row.get('DrawdownPct', 0)) if 'DrawdownPct' in equity_df.columns else 0.0
                    point = EquityPoint(
                        date=str(idx),
                        equity=equity,
                        returns=returns,
                        drawdown=-abs(dd),  # Store as negative (underwater)
                    )
                    equity_curve.append(point)
                    prev_equity = equity

        except Exception as e:
            self._error('Failed to extract equity curve', e)

        return equity_curve


# ============================================================================
# CLI Entry Point (for direct Python execution)
# ============================================================================

def main():
    """Main entry point for CLI execution"""
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: python backtestingpy_provider.py <command> <json_args>'
        }))
        sys.exit(1)

    command = sys.argv[1]
    json_args = sys.argv[2]

    try:
        args = parse_json_input(json_args)
        provider = BacktestingPyProvider()

        # Auto-initialize on every invocation since each CLI call is a fresh process.
        # This ensures the provider is always ready regardless of command order.
        if command not in ('initialize', 'test_connection', 'disconnect'):
            init_result = provider.initialize(args.get('_config', {}))
            if not init_result.get('success', False):
                # If backtesting.py isn't installed, we can't proceed
                print(json_response(init_result))
                sys.exit(1)

        if command == 'initialize':
            result = provider.initialize(args)
        elif command == 'test_connection':
            result = provider.test_connection()
        elif command == 'run_backtest':
            result = provider.run_backtest(args)
        elif command == 'get_historical_data':
            result = provider.get_historical_data(args)
        elif command == 'calculate_indicator':
            # Frontend sends top-level `indicator` (id), `symbols`, `startDate`,
            # `endDate`, plus a nested `params` dict with indicator-specific
            # tuning (`period`, etc.). Pass the full args through so
            # calculate_indicator can read both kinds.
            indicator_type = args.get('indicator') or args.get('type', '')
            result = provider.calculate_indicator(indicator_type, args)
        elif command == 'optimize':
            result = provider.optimize(args)
        elif command == 'walk_forward':
            result = provider.walk_forward(args)
        elif command == 'get_strategies':
            result = provider.get_strategies(args)
        elif command == 'get_indicators':
            result = provider.get_indicators(args)
        elif command == 'get_command_options':
            result = provider.get_command_options(args)
        elif command == 'disconnect':
            provider.disconnect()
            result = {'success': True, 'message': 'Disconnected'}
        else:
            result = {'success': False, 'error': f'Unknown command: {command}'}

        print(json_response(result))

    except Exception as e:
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': __import__('traceback').format_exc()
        }))
        sys.exit(1)


if __name__ == '__main__':
    main()
