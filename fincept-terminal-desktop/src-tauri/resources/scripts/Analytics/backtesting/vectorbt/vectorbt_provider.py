"""
VectorBT Provider - Slim Orchestrator

Delegates to specialized modules:
- vbt_strategies: 20+ strategy implementations
- vbt_indicators: All VBT indicators + custom indicators
- vbt_portfolio: Portfolio construction (from_signals, from_orders, stops, sizing)
- vbt_metrics: Full metrics extraction (100+ metrics)
- vbt_optimization: Vectorized parameter optimization + walk-forward
"""

import sys
import json
from typing import Dict, Any
from pathlib import Path

# Setup paths for both direct execution and package import
_SCRIPT_DIR = Path(__file__).parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent

# Add parent dirs to sys.path so absolute imports work when run directly
if str(_BACKTESTING_DIR) not in sys.path:
    sys.path.insert(0, str(_BACKTESTING_DIR))
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from base.base_provider import (
    BacktestingProviderBase,
    BacktestResult,
    PerformanceMetrics,
    Trade,
    EquityPoint,
    BacktestStatistics,
    json_response,
    parse_json_input
)


class VectorBTProvider(BacktestingProviderBase):
    """
    VectorBT Provider - Ultra-fast vectorized backtesting.

    Thin orchestrator that delegates to specialized modules
    for strategies, portfolio construction, metrics, and optimization.
    """

    def __init__(self):
        super().__init__()

    @property
    def name(self) -> str:
        return "VectorBT"

    @property
    def version(self) -> str:
        try:
            import vectorbt as vbt
            return vbt.__version__
        except Exception:
            return "pure-numpy"

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
            'maxConcurrentBacktests': 50,
            'vectorization': True,
            'walkForward': True,
            'randomBenchmarking': True,
            'stopLoss': True,
            'takeProfit': True,
            'positionSizing': True,
        }

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize VectorBT provider."""
        self.config = config
        vbt = self._import_vbt()
        return self._create_success_result(f'VectorBT {vbt.__version__} ready')

    def test_connection(self) -> Dict[str, Any]:
        """Test VectorBT availability."""
        vbt = self._import_vbt()
        return self._create_success_result(f'VectorBT {vbt.__version__} is available')

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run backtest using VectorBT with full feature support."""
        import pandas as pd
        import numpy as np

        backtest_id = self._generate_id()
        logs = []

        # Debug: confirm run_backtest is entered
        print(f'[PYTHON] === run_backtest entered ===', file=sys.stderr)
        print(f'[PYTHON] Request keys: {list(request.keys())}', file=sys.stderr)
        print(f'[PYTHON] marketData present: {"marketData" in request and request["marketData"] is not None}', file=sys.stderr)
        if 'marketData' in request and request['marketData']:
            md = request['marketData']
            print(f'[PYTHON] marketData keys: {list(md.keys()) if isinstance(md, dict) else type(md)}', file=sys.stderr)
            for k, v in md.items():
                print(f'[PYTHON]   {k}: {len(v)} bars', file=sys.stderr)

        try:
            vbt = self._import_vbt()
        except Exception as e:
            return self._create_error_result(str(e))

        try:
            import vbt_strategies as strat
            import vbt_portfolio as pf
            import vbt_metrics as metrics

            logs.append(f'{self._current_timestamp()}: Starting VectorBT backtest {backtest_id}')

            # --- Extract request parameters ---
            strategy = request.get('strategy', {})
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 100000)
            assets = request.get('assets', [])
            parameters = strategy.get('parameters', {})
            strategy_type = strategy.get('type', 'sma_crossover')

            # --- Download market data ---
            symbols = [asset['symbol'] for asset in assets]
            market_data = request.get('marketData')  # Pre-fetched data from our yfinance script (via stdin for large payloads)

            if market_data:
                logs.append(f'{self._current_timestamp()}: Using pre-fetched market data for {symbols}')
            else:
                logs.append(f'{self._current_timestamp()}: Downloading data for {symbols}')
                logs.append(f'{self._current_timestamp()}: Normalized symbols: {self._normalize_symbols(symbols)}')

            close_series, using_synthetic = self._load_market_data(
                symbols, start_date, end_date, market_data
            )
            logs.append(f'{self._current_timestamp()}: Data: {len(close_series)} bars, '
                        f'range: {close_series.index[0]} to {close_series.index[-1]}, '
                        f'price range: {close_series.min():.2f} - {close_series.max():.2f}, '
                        f'synthetic={using_synthetic}')

            # --- Handle custom code strategy ---
            if strategy_type == 'code':
                portfolio = self._run_custom_code(
                    vbt, strategy, close_series, initial_capital
                )
            else:
                # --- Build strategy signals ---
                logs.append(f'{self._current_timestamp()}: Building signals for strategy: {strategy_type}, params: {parameters}')
                print(f'[PYTHON] Strategy: {strategy_type}, params: {parameters}', file=sys.stderr)
                print(f'[PYTHON] close_series: len={len(close_series)}, dtype={close_series.dtype}, first5={close_series.head().tolist()}, last5={close_series.tail().tolist()}', file=sys.stderr)

                entries, exits = strat.build_strategy_signals(
                    vbt, strategy_type, close_series, parameters
                )

                n_entries = int(entries.sum()) if hasattr(entries, 'sum') else 0
                n_exits = int(exits.sum()) if hasattr(exits, 'sum') else 0
                logs.append(f'{self._current_timestamp()}: Signals generated: {n_entries} entries, {n_exits} exits')
                print(f'[PYTHON] Signals: {n_entries} entries, {n_exits} exits', file=sys.stderr)

                if n_entries == 0:
                    logs.append(f'{self._current_timestamp()}: WARNING: No entry signals generated! '
                                f'Check strategy parameters or data length ({len(close_series)} bars)')
                    print(f'[PYTHON] WARNING: No entry signals! Data len={len(close_series)}, strategy={strategy_type}', file=sys.stderr)

                # --- Build portfolio with all features ---
                portfolio = pf.build_portfolio(
                    vbt, close_series, entries, exits,
                    initial_capital, request
                )

            n_trades = len(portfolio.trades.records_readable) if hasattr(portfolio.trades, 'records_readable') else 0
            print(f'[PYTHON] Portfolio built: trades={n_trades}, final_value={portfolio.final_value():.2f}, total_return={portfolio.total_return():.6f}', file=sys.stderr)
            logs.append(f'{self._current_timestamp()}: Backtest completed: {n_trades} trades, '
                        f'final value: {portfolio.final_value():.2f}, '
                        f'return: {portfolio.total_return() * 100:.2f}%')

            # --- Extract comprehensive metrics ---
            all_metrics = metrics.extract_full_metrics(
                portfolio, initial_capital, close_series, vbt
            )

            # --- Build result data structures ---
            perf = all_metrics['performance']
            stats = all_metrics['statistics']
            extended = all_metrics['extended_stats']

            performance = PerformanceMetrics(
                total_return=perf['totalReturn'],
                annualized_return=perf['annualizedReturn'],
                sharpe_ratio=perf['sharpeRatio'],
                sortino_ratio=perf['sortinoRatio'],
                max_drawdown=perf['maxDrawdown'],
                win_rate=perf['winRate'],
                loss_rate=perf['lossRate'],
                profit_factor=perf['profitFactor'],
                volatility=perf['volatility'],
                calmar_ratio=perf['calmarRatio'],
                total_trades=perf['totalTrades'],
                winning_trades=perf['winningTrades'],
                losing_trades=perf['losingTrades'],
                average_win=perf['averageWin'],
                average_loss=perf['averageLoss'],
                largest_win=perf['largestWin'],
                largest_loss=perf['largestLoss'],
                average_trade_return=perf['averageTradeReturn'],
                expectancy=perf['expectancy'],
            )

            statistics = BacktestStatistics(
                start_date=stats['startDate'],
                end_date=stats['endDate'],
                initial_capital=stats['initialCapital'],
                final_capital=stats['finalCapital'],
                total_fees=stats['totalFees'],
                total_slippage=stats['totalSlippage'],
                total_trades=stats['totalTrades'],
                winning_days=stats['winningDays'],
                losing_days=stats['losingDays'],
                average_daily_return=stats['averageDailyReturn'],
                best_day=stats['bestDay'],
                worst_day=stats['worstDay'],
                consecutive_wins=stats['consecutiveWins'],
                consecutive_losses=stats['consecutiveLosses'],
            )

            # Parse trades
            trades = self._parse_trades(portfolio, symbols)

            # Build equity curve
            equity = self._build_equity_curve(portfolio, initial_capital)

            # Assemble result
            result = BacktestResult(
                id=backtest_id,
                status='completed',
                performance=performance,
                trades=trades,
                equity=equity,
                statistics=statistics,
                logs=logs,
                start_time=self._current_timestamp(),
                end_time=self._current_timestamp(),
            )

            result_dict = result.to_dict()
            result_dict['using_synthetic_data'] = using_synthetic
            result_dict['extended_stats'] = extended

            # Include detailed analysis sub-results
            result_dict['trade_analysis'] = all_metrics.get('trade_analysis', {})
            result_dict['drawdown_analysis'] = all_metrics.get('drawdown_analysis', {})
            result_dict['returns_analysis'] = all_metrics.get('returns_analysis', {})
            result_dict['risk_metrics'] = all_metrics.get('risk_metrics', {})

            # --- Advanced Metrics (benchmark, rolling, monthly) ---
            self._add_advanced_metrics(
                result_dict, portfolio, request, initial_capital,
                start_date, end_date
            )

            return {'success': True, 'message': 'Backtest completed', 'data': result_dict}

        except Exception as e:
            self._error(f'Backtest {backtest_id} failed', e)
            import traceback
            tb = traceback.format_exc()
            logs.append(f'{self._current_timestamp()}: Error: {str(e)}')
            logs.append(f'{self._current_timestamp()}: Traceback: {tb}')
            error_result = self._create_error_result(f'VectorBT backtest failed: {str(e)}')
            error_result['data'] = error_result.get('data', {})
            if isinstance(error_result.get('data'), dict):
                error_result['data']['logs'] = logs
            else:
                error_result['logs'] = logs
            return error_result

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run parameter optimization."""
        try:
            vbt = self._import_vbt()
        except Exception as e:
            return self._create_error_result(str(e))

        try:
            import pandas as pd
            import vbt_optimization as opt

            strategy = request.get('strategy', {})
            strategy_type = strategy.get('type', 'sma_crossover')
            parameters = request.get('parameters', {})
            objective = request.get('objective', 'sharpe')
            initial_capital = request.get('initialCapital', 100000)
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            assets = request.get('assets', [])
            method = request.get('method', 'grid')
            max_iterations = request.get('maxIterations', 500)

            symbols = [asset['symbol'] for asset in assets]
            close_series, _ = self._load_market_data(symbols, start_date, end_date, request.get("marketData"))

            result = opt.optimize(
                vbt, close_series, strategy_type, parameters,
                objective, initial_capital, method, max_iterations, request
            )

            return {'success': True, 'data': result}

        except Exception as e:
            self._error('Optimization failed', e)
            return self._create_error_result(f'Optimization failed: {str(e)}')

    def walk_forward(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run walk-forward optimization."""
        try:
            vbt = self._import_vbt()
        except Exception as e:
            return self._create_error_result(str(e))

        try:
            import vbt_optimization as opt

            strategy = request.get('strategy', {})
            strategy_type = strategy.get('type', 'sma_crossover')
            parameters = request.get('parameters', {})
            objective = request.get('objective', 'sharpe')
            initial_capital = request.get('initialCapital', 100000)
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            assets = request.get('assets', [])
            n_splits = request.get('nSplits', 5)
            train_ratio = request.get('trainRatio', 0.7)

            symbols = [asset['symbol'] for asset in assets]
            close_series, _ = self._load_market_data(symbols, start_date, end_date, request.get("marketData"))

            result = opt.walk_forward_optimize(
                vbt, close_series, strategy_type, parameters,
                objective, initial_capital, n_splits, train_ratio, request
            )

            return {'success': True, 'data': result}

        except Exception as e:
            self._error('Walk-forward optimization failed', e)
            return self._create_error_result(f'Walk-forward failed: {str(e)}')

    def get_strategies(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return available strategy catalog."""
        import vbt_strategies as strat
        return {'success': True, 'data': strat.get_strategy_catalog()}

    def get_indicators(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return available indicator catalog."""
        import vbt_indicators as ind
        return {'success': True, 'data': ind.get_indicator_catalog()}

    def get_historical_data(self, request: Dict[str, Any]) -> list:
        """Get historical data using yfinance."""
        try:
            import yfinance as yf
            import pandas as pd

            symbols = request.get('symbols', [])
            normalized = self._normalize_symbols(symbols)
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            timeframe = request.get('timeframe', 'daily')

            data = yf.download(
                normalized if len(normalized) > 1 else normalized[0],
                start=start_date, end=end_date, progress=False
            )

            if data is None or data.empty:
                return []

            result = []
            for i, symbol in enumerate(symbols):
                norm_sym = normalized[i] if i < len(normalized) else symbol
                try:
                    if len(normalized) == 1:
                        symbol_data = data
                    elif isinstance(data.columns, pd.MultiIndex):
                        # MultiIndex: extract per-symbol OHLCV
                        symbol_data = data.xs(norm_sym, axis=1, level=1) if norm_sym in data.columns.get_level_values(1) else data
                    else:
                        symbol_data = data

                    bars = []
                    for date, row in symbol_data.iterrows():
                        bars.append({
                            'date': date.isoformat(),
                            'open': float(row.get('Open', 0) if hasattr(row, 'get') else 0),
                            'high': float(row.get('High', 0) if hasattr(row, 'get') else 0),
                            'low': float(row.get('Low', 0) if hasattr(row, 'get') else 0),
                            'close': float(row.get('Close', 0) if hasattr(row, 'get') else 0),
                            'volume': int(row.get('Volume', 0) if hasattr(row, 'get') else 0),
                        })
                    result.append({'symbol': symbol, 'timeframe': timeframe, 'data': bars})
                except Exception:
                    result.append({'symbol': symbol, 'timeframe': timeframe, 'data': []})

            return result
        except Exception as e:
            self._error('Data fetch failed', e)
            raise

    def generate_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate trading signals using vbt_signals module."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_signals as sig

            generator_type = request.get('generatorType', 'RAND')
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            params = request.get('params', {})

            # Load market data to get a close series
            close_series, using_synthetic = self._load_market_data(
                self._normalize_symbols(symbols), start_date, end_date
            )

            gen_map = {
                'RAND': lambda: sig.RAND(close_series, n=params.get('n', 10), seed=params.get('seed')),
                'RANDX': lambda: sig.RANDX(close_series, n=params.get('n', 10), seed=params.get('seed')),
                'RANDNX': lambda: sig.RANDNX(close_series, n=params.get('n', 10), seed=params.get('seed'),
                                              min_hold=params.get('min_hold', 1), max_hold=params.get('max_hold', 20)),
                'RPROB': lambda: sig.RPROB(close_series, entry_prob=params.get('entry_prob', 0.1), seed=params.get('seed')),
                'RPROBX': lambda: sig.RPROBX(close_series, entry_prob=params.get('entry_prob', 0.1),
                                             exit_prob=params.get('exit_prob', 0.1), seed=params.get('seed')),
                'RPROBCX': lambda: sig.RPROBCX(close_series, entry_prob=params.get('entry_prob', 0.1),
                                               exit_prob=params.get('exit_prob', 0.1),
                                               cooldown=params.get('cooldown', 5), seed=params.get('seed')),
                'RPROBNX': lambda: sig.RPROBNX(close_series, n=params.get('n', 10),
                                               entry_prob=params.get('entry_prob', 0.1),
                                               exit_prob=params.get('exit_prob', 0.2), seed=params.get('seed')),
            }

            result_data = {'generatorType': generator_type, 'totalBars': len(close_series),
                           'usingSyntheticData': using_synthetic}

            if generator_type in gen_map:
                output = gen_map[generator_type]()
                if isinstance(output, tuple):
                    entries, exits = output
                    result_data['entryCount'] = int(entries.sum()) if hasattr(entries, 'sum') else int(np.sum(entries))
                    result_data['exitCount'] = int(exits.sum()) if hasattr(exits, 'sum') else int(np.sum(exits))
                    result_data['entries'] = [str(d) for d in entries[entries].index[:50]] if hasattr(entries, 'index') else []
                    result_data['exits'] = [str(d) for d in exits[exits].index[:50]] if hasattr(exits, 'index') else []
                else:
                    entries = output
                    result_data['entryCount'] = int(entries.sum()) if hasattr(entries, 'sum') else int(np.sum(entries))
                    result_data['entries'] = [str(d) for d in entries[entries].index[:50]] if hasattr(entries, 'index') else []
            elif generator_type in ('STX', 'STCX', 'OHLCSTX', 'OHLCSTCX'):
                # Stop-based generators need entry signals first
                entries_base = sig.RPROB(close_series, entry_prob=0.05, seed=42)
                if generator_type == 'STX':
                    exits = sig.STX(close_series, entries_base,
                                    stop_loss=params.get('stop_loss'), take_profit=params.get('take_profit'))
                elif generator_type == 'STCX':
                    exits = sig.STCX(close_series, entries_base,
                                     stop_loss=params.get('stop_loss'), take_profit=params.get('take_profit'),
                                     trailing_stop=params.get('trailing_stop'))
                else:
                    exits = sig.STX(close_series, entries_base,
                                    stop_loss=params.get('stop_loss'), take_profit=params.get('take_profit'))
                result_data['baseEntryCount'] = int(entries_base.sum()) if hasattr(entries_base, 'sum') else 0
                result_data['exitCount'] = int(exits.sum()) if hasattr(exits, 'sum') else int(np.sum(exits))
                result_data['exits'] = [str(d) for d in exits[exits].index[:50]] if hasattr(exits, 'index') else []
            else:
                return {'success': False, 'error': f'Unknown generator type: {generator_type}'}

            # Apply clean_signals post-processing if requested
            clean = request.get('params', {}).get('clean', False) or request.get('clean', False)
            if clean and 'entryCount' in result_data:
                try:
                    cleaned_entries, cleaned_exits = sig.clean_signals(
                        entries if isinstance(entries, pd.Series) else pd.Series(entries, index=close_series.index),
                        exits if isinstance(exits, pd.Series) else pd.Series(exits, index=close_series.index) if 'exits' in dir() else pd.Series(False, index=close_series.index)
                    )
                    result_data['cleanedEntryCount'] = int(cleaned_entries.sum())
                    result_data['cleanedExitCount'] = int(cleaned_exits.sum())
                    result_data['cleaned'] = True
                except Exception:
                    result_data['cleaned'] = False

            return {'success': True, 'data': result_data}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def generate_labels(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate ML labels using vbt_labels module."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_labels as lb

            label_type = request.get('labelType', 'FIXLB')
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            params = request.get('params', {})

            close_series, using_synthetic = self._load_market_data(
                self._normalize_symbols(symbols), start_date, end_date
            )

            label_map = {
                'FIXLB': lambda: lb.FIXLB(close_series, horizon=params.get('horizon', 5),
                                           threshold=params.get('threshold', 0.0)),
                'MEANLB': lambda: lb.MEANLB(close_series, window=params.get('window', 20),
                                            threshold=params.get('threshold', 1.0)),
                'LEXLB': lambda: lb.LEXLB(close_series, window=params.get('window', 5)),
                'TRENDLB': lambda: lb.TRENDLB(close_series, window=params.get('window', 20),
                                              threshold=params.get('threshold', 0.0)),
                'BOLB': lambda: lb.BOLB(close_series, window=params.get('window', 20),
                                        alpha=params.get('alpha', 2.0)),
            }

            if label_type not in label_map:
                return {'success': False, 'error': f'Unknown label type: {label_type}'}

            labels = label_map[label_type]()

            # Build distribution summary
            unique, counts = np.unique(labels.dropna().values if hasattr(labels, 'dropna') else labels[~np.isnan(labels)],
                                       return_counts=True)
            distribution = {str(int(v)): int(c) for v, c in zip(unique, counts)}

            result_data = {
                'labelType': label_type,
                'totalBars': len(close_series),
                'labeledBars': int(labels.notna().sum()) if hasattr(labels, 'notna') else int(np.sum(~np.isnan(labels))),
                'usingSyntheticData': using_synthetic,
                'distribution': distribution,
                'sampleLabels': [
                    {'date': str(labels.index[i]), 'label': int(labels.iloc[i])}
                    for i in range(min(50, len(labels))) if not pd.isna(labels.iloc[i])
                ],
            }

            return {'success': True, 'data': result_data}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def generate_splits(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate cross-validation splits using vbt_splitters module."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_splitters as sp

            splitter_type = request.get('splitterType', 'RollingSplitter')
            symbols = request.get('symbols', [])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            total_bars = request.get('totalBars')
            params = request.get('params', {})

            # Build an index - from market data if available, else synthetic
            if symbols and start_date and end_date:
                close_series, using_synthetic = self._load_market_data(
                    self._normalize_symbols(symbols), start_date, end_date
                )
                index = close_series.index
            elif total_bars:
                index = pd.date_range(start=start_date or '2020-01-01', periods=total_bars, freq='B')
            else:
                index = pd.date_range(start='2020-01-01', end='2024-01-01', freq='B')

            splitter_map = {
                'RollingSplitter': lambda: sp.RollingSplitter(
                    window_len=params.get('window_len', 252),
                    test_len=params.get('test_len', 63),
                    step=params.get('step', 21),
                ),
                'ExpandingSplitter': lambda: sp.ExpandingSplitter(
                    min_len=params.get('min_len', 252),
                    test_len=params.get('test_len', 63),
                    step=params.get('step', 21),
                ),
                'PurgedKFoldSplitter': lambda: sp.PurgedKFoldSplitter(
                    n_splits=params.get('n_splits', 5),
                    purge_len=params.get('purge_len', 5),
                    embargo_len=params.get('embargo_len', 5),
                ),
            }

            # RangeSplitter: user supplies date ranges directly
            if splitter_type == 'RangeSplitter':
                ranges_raw = params.get('ranges', [])
                if not ranges_raw:
                    return {'success': False, 'error': 'RangeSplitter requires "ranges" array with train/test date ranges'}
                splitter = sp.RangeSplitter(ranges=[(r['trainStart'], r['trainEnd'], r['testStart'], r['testEnd']) for r in ranges_raw])
                splits = []
                for i, (train_mask, test_mask) in enumerate(splitter.split(index)):
                    train_idx = np.where(train_mask)[0] if hasattr(train_mask, '__len__') else train_mask
                    test_idx = np.where(test_mask)[0] if hasattr(test_mask, '__len__') else test_mask
                    splits.append({
                        'fold': i,
                        'trainStart': str(index[train_idx[0]]) if len(train_idx) > 0 else '',
                        'trainEnd': str(index[train_idx[-1]]) if len(train_idx) > 0 else '',
                        'trainSize': len(train_idx),
                        'testStart': str(index[test_idx[0]]) if len(test_idx) > 0 else '',
                        'testEnd': str(index[test_idx[-1]]) if len(test_idx) > 0 else '',
                        'testSize': len(test_idx),
                    })
                return {'success': True, 'data': {
                    'splitterType': splitter_type, 'totalBars': len(index),
                    'nSplits': len(splits), 'indexStart': str(index[0]), 'indexEnd': str(index[-1]),
                    'splits': splits,
                }}

            if splitter_type not in splitter_map:
                return {'success': False, 'error': f'Unknown splitter type: {splitter_type}'}

            splitter = splitter_map[splitter_type]()
            splits = []
            for i, (train_idx, test_idx) in enumerate(splitter.split(index)):
                splits.append({
                    'fold': i,
                    'trainStart': str(index[train_idx[0]]),
                    'trainEnd': str(index[train_idx[-1]]),
                    'trainSize': len(train_idx),
                    'testStart': str(index[test_idx[0]]),
                    'testEnd': str(index[test_idx[-1]]),
                    'testSize': len(test_idx),
                })

            result_data = {
                'splitterType': splitter_type,
                'totalBars': len(index),
                'nSplits': len(splits),
                'indexStart': str(index[0]),
                'indexEnd': str(index[-1]),
                'splits': splits,
            }

            return {'success': True, 'data': result_data}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def analyze_returns(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze returns using vbt_returns (ReturnsAccessor) + vbt_generic (Drawdowns, Ranges)."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_returns as ret
            import vbt_generic as gen

            analysis_type = request.get('analysisType', 'returns_stats')
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            benchmark = request.get('benchmark', '')
            params = request.get('params', {})

            close_series, using_synthetic = self._load_market_data(
                self._normalize_symbols(symbols), start_date, end_date
            )
            returns = close_series.pct_change().dropna()

            # Load benchmark if provided
            benchmark_rets = None
            if benchmark and benchmark.strip():
                try:
                    bench_close, _ = self._load_market_data(
                        self._normalize_symbols([benchmark]), start_date, end_date
                    )
                    benchmark_rets = bench_close.pct_change().dropna()
                    # Align lengths
                    common_idx = returns.index.intersection(benchmark_rets.index)
                    returns = returns.loc[common_idx]
                    benchmark_rets = benchmark_rets.loc[common_idx]
                except Exception:
                    benchmark_rets = None

            result_data = {
                'analysisType': analysis_type,
                'totalBars': len(close_series),
                'returnBars': len(returns),
                'usingSyntheticData': using_synthetic,
            }

            if analysis_type == 'returns_stats':
                risk_free = params.get('risk_free', 0.0)
                n_trials = params.get('n_trials', 1)
                omega_threshold = params.get('omega_threshold', 0.0)

                acc = ret.ReturnsAccessor(returns, benchmark_rets=benchmark_rets)
                stats = acc.stats()
                result_data['stats'] = {k: float(v) if isinstance(v, (int, float, np.floating, np.integer)) else str(v) for k, v in stats.items()}

                # Add extra metrics
                try:
                    result_data['stats']['Deflated Sharpe'] = float(acc.deflated_sharpe_ratio(n_trials=n_trials))
                except Exception:
                    pass
                try:
                    result_data['stats']['Up Capture'] = float(acc.up_capture())
                    result_data['stats']['Down Capture'] = float(acc.down_capture())
                    result_data['stats']['Up/Down Ratio'] = float(acc.up_down_ratio())
                except Exception:
                    pass

                # Cumulative returns as sample
                cum = acc.cumulative()
                result_data['cumulativeReturns'] = [
                    {'date': str(cum.index[i]), 'value': float(cum.iloc[i])}
                    for i in range(0, len(cum), max(1, len(cum) // 100))
                ]

            elif analysis_type == 'drawdowns':
                equity = (1 + returns).cumprod()
                dd = gen.Drawdowns.from_ts(equity)
                result_data['stats'] = {k: float(v) if isinstance(v, (int, float, np.floating, np.integer)) else str(v) for k, v in dd.stats().items()}
                result_data['totalDrawdowns'] = dd.count
                result_data['maxDrawdown'] = float(dd.max_drawdown())
                result_data['avgDrawdown'] = float(dd.avg_drawdown())
                result_data['activeDrawdown'] = float(dd.active_drawdown())
                result_data['activeDuration'] = int(dd.active_duration())
                # Records
                records_df = dd.records_readable
                if records_df is not None and len(records_df) > 0:
                    result_data['records'] = records_df.head(50).to_dict('records')
                # Drawdown series (sampled)
                dd_series = dd.drawdown()
                result_data['drawdownSeries'] = [
                    {'date': str(dd_series.index[i]), 'value': float(dd_series.iloc[i])}
                    for i in range(0, len(dd_series), max(1, len(dd_series) // 100))
                ]

            elif analysis_type == 'ranges':
                threshold = params.get('threshold', 0.0)
                rng = gen.Ranges.from_ts(returns, threshold=threshold)
                result_data['stats'] = {k: float(v) if isinstance(v, (int, float, np.floating, np.integer)) else str(v) for k, v in rng.stats().items()}
                result_data['totalRanges'] = rng.count
                result_data['avgDuration'] = float(rng.avg_duration())
                result_data['maxDuration'] = int(rng.max_duration())
                result_data['coverage'] = float(rng.coverage())
                records_df = rng.records_readable
                if records_df is not None and len(records_df) > 0:
                    result_data['records'] = records_df.head(50).to_dict('records')

            elif analysis_type == 'rolling':
                window = params.get('window', 252)
                risk_free = params.get('risk_free', 0.0)
                metric = params.get('metric', 'sharpe')

                acc = ret.ReturnsAccessor(returns, benchmark_rets=benchmark_rets)
                rolling_map = {
                    'total': lambda: acc.rolling_total(window),
                    'annualized': lambda: acc.rolling_annualized(window),
                    'volatility': lambda: acc.rolling_annualized_volatility(window),
                    'sharpe': lambda: acc.rolling_sharpe_ratio(window, risk_free),
                    'sortino': lambda: acc.rolling_sortino_ratio(window, risk_free),
                    'calmar': lambda: acc.rolling_calmar_ratio(window),
                    'omega': lambda: acc.rolling_omega_ratio(window),
                    'info_ratio': lambda: acc.rolling_information_ratio(window),
                    'downside_risk': lambda: acc.rolling_downside_risk(window),
                }

                if metric not in rolling_map:
                    return {'success': False, 'error': f'Unknown rolling metric: {metric}'}

                series = rolling_map[metric]()
                series = series.dropna()
                result_data['metric'] = metric
                result_data['window'] = window
                result_data['dataPoints'] = len(series)
                result_data['series'] = [
                    {'date': str(series.index[i]), 'value': float(series.iloc[i])}
                    for i in range(0, len(series), max(1, len(series) // 200))
                ]
            else:
                return {'success': False, 'error': f'Unknown analysis type: {analysis_type}'}

            return {'success': True, 'data': result_data}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def indicator_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate indicator-based signals (crossover, threshold, breakout, mean_reversion, filter)."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_indicators as ind

            mode = request.get('mode', 'crossover_signals')
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            indicator = request.get('indicator', 'rsi')
            params = request.get('params', {})

            close_series, using_synthetic = self._load_market_data(
                self._normalize_symbols(symbols), start_date, end_date
            )

            result_data = {'mode': mode, 'totalBars': len(close_series), 'usingSyntheticData': using_synthetic}

            if mode == 'crossover_signals':
                fast_ind = params.get('fast_indicator', 'ma')
                fast_period = params.get('fast_period', 10)
                slow_ind = params.get('slow_indicator', 'ma')
                slow_period = params.get('slow_period', 20)

                calc_map = {'ma': ind.calculate_ma, 'ema': lambda s, p: ind.calculate_ma(s, p, ma_type='ema')}
                fast_line = calc_map.get(fast_ind, ind.calculate_ma)(close_series, fast_period)
                slow_line = calc_map.get(slow_ind, ind.calculate_ma)(close_series, slow_period)

                entries, exits = ind.generate_crossover_signals(fast_line, slow_line, close_series.index)
                result_data['entryCount'] = int(entries.sum())
                result_data['exitCount'] = int(exits.sum())
                result_data['entries'] = [str(d) for d in entries[entries].index[:50]]
                result_data['exits'] = [str(d) for d in exits[exits].index[:50]]

            elif mode == 'threshold_signals':
                period = params.get('period', 14)
                lower = params.get('lower', 30)
                upper = params.get('upper', 70)

                ind_calc = {'rsi': ind.calculate_rsi, 'cci': ind.calculate_cci, 'williams_r': ind.calculate_williams_r,
                            'stoch': lambda s, p: ind.calculate_stoch(s, s, s, p)[0]}
                calc_fn = ind_calc.get(indicator, ind.calculate_rsi)
                values = calc_fn(close_series, period)

                entries, exits = ind.generate_threshold_signals(values, lower, upper, close_series.index)
                result_data['entryCount'] = int(entries.sum())
                result_data['exitCount'] = int(exits.sum())
                result_data['entries'] = [str(d) for d in entries[entries].index[:50]]
                result_data['exits'] = [str(d) for d in exits[exits].index[:50]]

            elif mode == 'breakout_signals':
                channel = params.get('channel', 'donchian')
                period = params.get('period', 20)

                if channel == 'donchian':
                    ch = ind.calculate_donchian(close_series, period)
                    upper_ch = ch['upper'] if isinstance(ch, dict) else ch[0]
                    lower_ch = ch['lower'] if isinstance(ch, dict) else ch[1]
                elif channel == 'bbands':
                    bb = ind.calculate_bbands(close_series, period)
                    upper_ch = bb['upper'] if isinstance(bb, dict) else bb[0]
                    lower_ch = bb['lower'] if isinstance(bb, dict) else bb[2]
                else:
                    ch = ind.calculate_keltner(close_series, close_series, close_series, period)
                    upper_ch = ch['upper'] if isinstance(ch, dict) else ch[0]
                    lower_ch = ch['lower'] if isinstance(ch, dict) else ch[2]

                entries, exits = ind.generate_breakout_signals(close_series.values, upper_ch, lower_ch, close_series.index)
                result_data['entryCount'] = int(entries.sum())
                result_data['exitCount'] = int(exits.sum())
                result_data['entries'] = [str(d) for d in entries[entries].index[:50]]
                result_data['exits'] = [str(d) for d in exits[exits].index[:50]]

            elif mode == 'mean_reversion_signals':
                period = params.get('period', 20)
                z_entry = params.get('z_entry', 2.0)
                z_exit = params.get('z_exit', 0.0)

                zscore = ind.calculate_zscore(close_series, period)
                entries, exits = ind.generate_mean_reversion_signals(zscore, z_entry, z_exit, close_series.index)
                result_data['entryCount'] = int(entries.sum())
                result_data['exitCount'] = int(exits.sum())
                result_data['entries'] = [str(d) for d in entries[entries].index[:50]]
                result_data['exits'] = [str(d) for d in exits[exits].index[:50]]

            elif mode == 'signal_filter':
                base_indicator = params.get('base_indicator', 'rsi')
                base_period = params.get('base_period', 14)
                filter_indicator = params.get('filter_indicator', 'adx')
                filter_period = params.get('filter_period', 14)
                filter_threshold = params.get('filter_threshold', 25)
                filter_type = params.get('filter_type', 'above')

                # Compute base indicator and generate threshold signals
                base_calc = {'rsi': ind.calculate_rsi, 'cci': ind.calculate_cci, 'williams_r': ind.calculate_williams_r,
                             'momentum': ind.calculate_momentum, 'stoch': lambda s, p: ind.calculate_stoch(s, s, s, p)[0],
                             'macd': lambda s, p: ind.calculate_macd(s, p, p * 2, 9)[0]}
                base_fn = base_calc.get(base_indicator, ind.calculate_rsi)
                base_values = base_fn(close_series, base_period)
                entries, exits = ind.generate_threshold_signals(base_values, 30, 70, close_series.index)

                # Compute filter indicator
                filter_calc = {'adx': ind.calculate_adx, 'atr': ind.calculate_atr, 'mstd': ind.calculate_mstd,
                               'zscore': ind.calculate_zscore, 'rsi': ind.calculate_rsi}
                filter_fn = filter_calc.get(filter_indicator, ind.calculate_adx)
                filter_values = filter_fn(close_series, filter_period)

                filtered_entries, filtered_exits = ind.apply_signal_filter(
                    entries, exits, filter_values, filter_threshold, filter_type
                )
                result_data['originalEntryCount'] = int(entries.sum())
                result_data['filteredEntryCount'] = int(filtered_entries.sum())
                result_data['exitCount'] = int(filtered_exits.sum())
                result_data['entries'] = [str(d) for d in filtered_entries[filtered_entries].index[:50]]
                result_data['exits'] = [str(d) for d in filtered_exits[filtered_exits].index[:50]]

            else:
                return {'success': False, 'error': f'Unknown indicator signal mode: {mode}'}

            return {'success': True, 'data': result_data}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def indicator_param_sweep(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run indicator parameter sweep using IndicatorFactory.run_combs()."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_indicators as ind

            indicator = request.get('indicator', 'rsi')
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            param_ranges = request.get('paramRanges', {})

            close_series, using_synthetic = self._load_market_data(
                self._normalize_symbols(symbols), start_date, end_date
            )

            # Build parameter lists from ranges
            param_lists = {}
            for name, rng in param_ranges.items():
                mn, mx, st = rng.get('min', 5), rng.get('max', 50), rng.get('step', 5)
                param_lists[name] = list(range(int(mn), int(mx) + 1, int(st))) if st >= 1 else list(np.arange(mn, mx + st, st))

            # Map indicator to factory-compatible function
            calc_map = {
                'rsi': lambda s, period=14: ind.calculate_rsi(s, period),
                'ma': lambda s, period=20: ind.calculate_ma(s, period),
                'ema': lambda s, period=20: ind.calculate_ma(s, period, ma_type='ema'),
                'atr': lambda s, period=14: ind.calculate_atr(s, period),
                'mstd': lambda s, period=20: ind.calculate_mstd(s, period),
                'zscore': lambda s, period=20: ind.calculate_zscore(s, period),
                'adx': lambda s, period=14: ind.calculate_adx(s, period),
                'momentum': lambda s, lookback=20: ind.calculate_momentum(s, lookback),
                'cci': lambda s, period=20: ind.calculate_cci(s, period),
                'williams_r': lambda s, period=14: ind.calculate_williams_r(s, period),
            }

            if indicator not in calc_map:
                return {'success': False, 'error': f'Indicator {indicator} not supported for param sweep'}

            factory = ind.IndicatorFactory(
                input_names=['close'],
                param_names=list(param_lists.keys()),
                output_names=['output'],
                short_name=indicator,
            )
            factory_fn = factory.from_custom_func(lambda close, **kw: calc_map[indicator](close, **kw))
            results = factory_fn.run_combs(close_series, param_ranges=param_lists)

            # Summarize results
            summaries = []
            for r in results[:200]:  # Limit output
                params_dict = r.get('params', {})
                output = r.get('output')
                summary = {'params': params_dict}
                if output is not None and hasattr(output, '__len__') and len(output) > 0:
                    vals = np.array(output, dtype=float)
                    vals = vals[~np.isnan(vals)]
                    if len(vals) > 0:
                        summary['mean'] = float(np.mean(vals))
                        summary['std'] = float(np.std(vals))
                        summary['min'] = float(np.min(vals))
                        summary['max'] = float(np.max(vals))
                        summary['last'] = float(vals[-1])
                summaries.append(summary)

            return {'success': True, 'data': {
                'indicator': indicator,
                'totalCombinations': len(results),
                'usingSyntheticData': using_synthetic,
                'results': summaries,
            }}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def labels_to_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate labels then convert to trading signals using labels_to_signals()."""
        try:
            import numpy as np
            import pandas as pd
            import vbt_labels as lb

            label_type = request.get('labelType', 'FIXLB')
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            params = request.get('params', {})
            entry_label = request.get('entryLabel', 1)
            exit_label = request.get('exitLabel', -1)

            close_series, using_synthetic = self._load_market_data(
                self._normalize_symbols(symbols), start_date, end_date
            )

            label_map = {
                'FIXLB': lambda: lb.FIXLB(close_series, horizon=params.get('horizon', 5),
                                           threshold=params.get('threshold', 0.0)),
                'MEANLB': lambda: lb.MEANLB(close_series, window=params.get('window', 20),
                                            threshold=params.get('threshold', 1.0)),
                'LEXLB': lambda: lb.LEXLB(close_series, window=params.get('window', 5)),
                'TRENDLB': lambda: lb.TRENDLB(close_series, window=params.get('window', 20),
                                              threshold=params.get('threshold', 0.0)),
                'BOLB': lambda: lb.BOLB(close_series, window=params.get('window', 20),
                                        alpha=params.get('alpha', 2.0)),
            }

            if label_type not in label_map:
                return {'success': False, 'error': f'Unknown label type: {label_type}'}

            labels = label_map[label_type]()
            entries, exits = lb.labels_to_signals(labels, entry_label=entry_label, exit_label=exit_label)

            result_data = {
                'labelType': label_type,
                'entryLabel': entry_label,
                'exitLabel': exit_label,
                'totalBars': len(close_series),
                'usingSyntheticData': using_synthetic,
                'entryCount': int(entries.sum()),
                'exitCount': int(exits.sum()),
                'entries': [str(d) for d in entries[entries].index[:50]],
                'exits': [str(d) for d in exits[exits].index[:50]],
            }

            return {'success': True, 'data': result_data}
        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': __import__('traceback').format_exc()}

    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate a technical indicator."""
        raise NotImplementedError('Use VectorBT indicators within backtests')

    # ========================================================================
    # Private Helpers
    # ========================================================================

    def _import_vbt(self):
        """Import VectorBT or return a stub (indicators/portfolio are now pure numpy/pandas)."""
        try:
            import vectorbt as vbt
            # Verify it's a usable install (some partial installs lack __version__)
            _ = vbt.__version__
            return vbt
        except (ImportError, AttributeError):
            # All indicators and portfolio logic are now pure numpy/pandas,
            # so vectorbt is no longer required. Return a simple namespace stub.
            import types
            stub = types.SimpleNamespace(__version__='pure-numpy')
            return stub

    def _load_market_data(self, symbols: list, start_date: str, end_date: str, market_data: dict = None):
        """
        Load market data, falling back to synthetic if unavailable.

        Args:
            symbols: List of ticker symbols
            start_date: Start date string
            end_date: End date string
            market_data: Optional dict of pre-fetched data {symbol: [{datetime, open, high, low, close, volume}, ...]}

        Returns:
            (close_series, using_synthetic) tuple
        """
        import pandas as pd
        import numpy as np

        using_synthetic = False

        # --- Use pre-fetched market data if available ---
        if market_data:
            try:
                import sys
                symbol = symbols[0] if symbols else 'SPY'
                data_list = market_data.get(symbol, [])

                print(f'[PYTHON] _load_market_data: market_data keys = {list(market_data.keys())}', file=sys.stderr)
                print(f'[PYTHON] _load_market_data: Looking for symbol = {symbol}', file=sys.stderr)
                print(f'[PYTHON] _load_market_data: data_list length = {len(data_list) if data_list else 0}', file=sys.stderr)

                if not data_list:
                    raise ValueError(f'No pre-fetched data for {symbol}')

                # yfinance_data.py format: [{timestamp, open, high, low, close, volume, ...}, ...]
                if data_list:
                    print(f'[PYTHON] First data point: {data_list[0]}', file=sys.stderr)

                dates = [pd.to_datetime(bar['timestamp'], unit='s') for bar in data_list]
                closes = [bar['close'] for bar in data_list]

                print(f'[PYTHON] Parsed {len(closes)} closes, date range: {dates[0]} to {dates[-1]}', file=sys.stderr)

                close_series = pd.Series(closes, index=dates, name='Close')

                if len(close_series) < 5:
                    raise ValueError(f'Insufficient data: only {len(close_series)} bars')

                print(f'[PYTHON] Using pre-fetched data: {len(close_series)} bars', file=sys.stderr)
                return close_series, using_synthetic

            except Exception as e:
                print(f'[PYTHON] Failed to use pre-fetched data: {e}', file=sys.stderr)
                self._error(f'Failed to use pre-fetched data: {e}', e)
                # Fall through to yfinance download

        # --- Normalize symbols for yfinance compatibility ---
        normalized_symbols = self._normalize_symbols(symbols)

        try:
            import yfinance as yf

            # Try downloading with normalized symbols
            raw_data = yf.download(
                normalized_symbols if len(normalized_symbols) > 1 else normalized_symbols[0],
                start=start_date, end=end_date, progress=False
            )

            if raw_data is None or (hasattr(raw_data, 'empty') and raw_data.empty):
                raise ValueError(f'No data returned for {normalized_symbols}')

            close_data = self._extract_close_column(raw_data, normalized_symbols)

            if close_data is None or (hasattr(close_data, 'empty') and close_data.empty):
                raise ValueError(f'No Close data for {normalized_symbols}')

            if isinstance(close_data, pd.DataFrame):
                # Multi-asset: take first column for strategy signals
                # (weighted portfolio logic is in the provider)
                close_data = close_data.iloc[:, 0].dropna()

            if len(close_data) < 5:
                raise ValueError(f'Insufficient data: only {len(close_data)} bars for {normalized_symbols}')

            close_series = pd.Series(
                close_data.values.astype(float).flatten(),
                index=close_data.index,
                name='Close'
            )

        except ImportError:
            self._error('yfinance not installed', None)
            using_synthetic = True
            close_series = self._generate_synthetic_data(symbols, start_date, end_date)
        except Exception as e:
            self._error(f'Data download failed for {normalized_symbols}: {e}', e)
            using_synthetic = True
            close_series = self._generate_synthetic_data(symbols, start_date, end_date)

        return close_series, using_synthetic

    @staticmethod
    def _normalize_symbols(symbols: list) -> list:
        """
        Clean up symbol list.

        Symbols are expected to already include correct exchange suffixes
        (e.g., PIDILITIND.NS, AAPL) as resolved at portfolio-add time.
        This method just strips whitespace and uppercases.
        """
        normalized = []
        for sym in symbols:
            sym = sym.strip().upper()
            if sym:
                normalized.append(sym)
        return normalized if normalized else symbols

    @staticmethod
    def _extract_close_column(raw_data, symbols: list):
        """
        Robustly extract Close price data from yfinance output.

        Handles:
        - Single-symbol download (plain columns: Open, High, Low, Close, ...)
        - Multi-symbol download with MultiIndex columns (Price, Ticker)
        - Newer yfinance versions that changed column structure
        """
        import pandas as pd

        columns = raw_data.columns

        # Case 1: Plain columns (single symbol download)
        if isinstance(columns, pd.Index) and not isinstance(columns, pd.MultiIndex):
            if 'Close' in columns:
                return raw_data['Close']
            # Fallback to first column
            return raw_data.iloc[:, 0] if len(columns) > 0 else None

        # Case 2: MultiIndex columns (multi-symbol download)
        if isinstance(columns, pd.MultiIndex):
            level_0_vals = columns.get_level_values(0).unique().tolist()
            level_1_vals = columns.get_level_values(1).unique().tolist()

            # yfinance 0.2.31+: columns are (Price, Ticker) e.g. ('Close', 'AAPL')
            if 'Close' in level_0_vals:
                close_data = raw_data['Close']
                if isinstance(close_data, pd.Series):
                    return close_data
                # DataFrame with ticker columns
                return close_data.dropna(how='all')

            # Some versions: columns are (Ticker, Price)
            if 'Close' in level_1_vals:
                close_data = raw_data.xs('Close', axis=1, level=1)
                if isinstance(close_data, pd.Series):
                    return close_data
                return close_data.dropna(how='all')

            # Fallback: take first level-0 group
            first_group = level_0_vals[0]
            return raw_data[first_group]

        # Fallback
        return raw_data.iloc[:, 0] if len(raw_data.columns) > 0 else None

    @staticmethod
    def _generate_synthetic_data(symbols: list, start_date: str, end_date: str):
        """Generate synthetic price data as fallback."""
        import pandas as pd
        import numpy as np

        dates = pd.date_range(start=start_date, end=end_date, freq='B')
        if len(dates) == 0:
            dates = pd.date_range(start='2023-01-01', periods=252, freq='B')

        np.random.seed(hash(symbols[0]) % 2**31 if symbols else 42)
        price = 100.0
        prices = []
        for _ in range(len(dates)):
            price *= (1 + np.random.normal(0.0003, 0.015))
            prices.append(price)
        return pd.Series(prices, index=dates, name='Close', dtype=float)

    def _run_custom_code(self, vbt, strategy: dict, close_series, initial_capital: float):
        """Execute custom strategy code."""
        import pandas as pd
        import numpy as np

        code = strategy.get('code', {})
        source = code.get('source', '') if isinstance(code, dict) else str(code or '')

        if not source:
            raise ValueError('Custom strategy requires code')

        context = {
            'vbt': vbt, 'pd': pd, 'np': np,
            'data': close_series,
            'initial_capital': initial_capital,
        }
        exec(source, context)
        portfolio = context.get('portfolio')
        if portfolio is None:
            raise ValueError('Custom strategy must create a "portfolio" variable')
        return portfolio

    def _parse_trades(self, portfolio, symbols: list) -> list:
        """Parse trades from VBT portfolio into Trade objects."""
        import numpy as np
        trades = []
        symbol = symbols[0] if symbols else 'UNKNOWN'

        try:
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
                records = portfolio.trades.records_readable
                value_index = portfolio.value().index

                for idx in range(len(records)):
                    row = records.iloc[idx]

                    # Get entry/exit dates from index
                    entry_idx = int(row.get('Entry Idx', 0))
                    exit_idx = int(row.get('Exit Idx', entry_idx))

                    entry_date = str(value_index[entry_idx]).split(' ')[0] if entry_idx < len(value_index) else ''
                    exit_date = str(value_index[exit_idx]).split(' ')[0] if exit_idx < len(value_index) else ''

                    pnl = float(row.get('PnL', 0))
                    entry_price = float(row.get('Entry Price', 0))
                    exit_price = float(row.get('Exit Price', 0))
                    size = float(row.get('Size', 0))
                    fees = float(row.get('Entry Fees', 0)) + float(row.get('Exit Fees', 0))
                    ret = float(row.get('Return', 0))

                    # Determine exit reason
                    exit_reason = 'signal'
                    if 'Status' in records.columns:
                        status = str(row.get('Status', ''))
                        if 'StopLoss' in status:
                            exit_reason = 'stop_loss'
                        elif 'TakeProfit' in status:
                            exit_reason = 'take_profit'

                    trades.append(Trade(
                        id=f'trade_{idx}',
                        symbol=symbol,
                        entry_date=entry_date,
                        side='long',
                        quantity=size,
                        entry_price=entry_price,
                        commission=fees,
                        slippage=0,
                        exit_date=exit_date,
                        exit_price=exit_price,
                        pnl=pnl,
                        pnl_percent=ret,
                        holding_period=max(0, exit_idx - entry_idx),
                        exit_reason=exit_reason,
                    ))

            elif hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records'):
                # Fallback to raw records
                records = portfolio.trades.records
                for idx, trade in enumerate(records):
                    pnl = float(trade.get('pnl', 0) if hasattr(trade, 'get') else getattr(trade, 'pnl', 0))
                    trades.append(Trade(
                        id=f'trade_{idx}',
                        symbol=symbol,
                        entry_date=str(getattr(trade, 'entry_idx', 0)),
                        side='long',
                        quantity=float(getattr(trade, 'size', 0)),
                        entry_price=float(getattr(trade, 'entry_price', 0)),
                        commission=float(getattr(trade, 'fees', 0)),
                        slippage=0,
                        exit_date=str(getattr(trade, 'exit_idx', 0)),
                        exit_price=float(getattr(trade, 'exit_price', 0)),
                        pnl=pnl,
                        pnl_percent=float(getattr(trade, 'return', 0)),
                        holding_period=0,
                        exit_reason='signal',
                    ))
        except Exception as e:
            self._error('Failed to parse trades', e)

        return trades

    def _build_equity_curve(self, portfolio, initial_capital: float) -> list:
        """Build equity curve with returns and drawdown."""
        equity = []
        try:
            value_series = portfolio.value()
            peak = initial_capital

            for date, value in value_series.items():
                returns = (value - initial_capital) / initial_capital if initial_capital > 0 else 0
                peak = max(peak, value)
                drawdown = (value - peak) / peak if peak > 0 else 0

                equity.append(EquityPoint(
                    date=str(date).split(' ')[0],
                    equity=float(value),
                    returns=float(returns),
                    drawdown=float(drawdown),
                ))
        except Exception as e:
            self._error('Failed to build equity curve', e)

        return equity

    def _add_advanced_metrics(
        self, result_dict: dict, portfolio, request: dict,
        initial_capital: float, start_date: str, end_date: str
    ):
        """Add advanced metrics, benchmark overlay, monthly returns, rolling metrics."""
        import numpy as np

        try:
            from base.advanced_metrics import calculate_all as calc_advanced

            equity_values = portfolio.value().values.astype(float)
            dates_list = [str(d).split(' ')[0] for d in portfolio.value().index]

            # Load benchmark
            benchmark_symbol = request.get('benchmark', '')
            benchmark_normalized = None
            if benchmark_symbol:
                benchmark_normalized = self._load_benchmark(
                    benchmark_symbol, start_date, end_date, len(equity_values)
                )
                # Add benchmark to equity curve
                if benchmark_normalized is not None and 'equity' in result_dict:
                    for i, point in enumerate(result_dict['equity']):
                        if i < len(benchmark_normalized):
                            point['benchmark'] = float(benchmark_normalized[i] * initial_capital)

            # Benchmark in equity scale for alpha/beta
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

            # Random benchmark p-value
            if request.get('randomBenchmark', False):
                try:
                    import vbt_portfolio as pf_mod
                    vbt = self._import_vbt()
                    close_series = portfolio.close
                    if close_series is not None:
                        rand_stats = pf_mod.get_random_benchmark_stats(
                            vbt, close_series, initial_capital, n_trials=100
                        )
                        strategy_return = float(portfolio.total_return())
                        # P-value: % of random strategies that beat this one
                        rand_stats['strategyReturn'] = strategy_return
                        rand_stats['pValue'] = float(
                            np.mean(np.array([rand_stats['mean']]) > strategy_return)
                        )
                        result_dict['random_benchmark'] = rand_stats
                except Exception:
                    pass

        except Exception as adv_err:
            self._error('Advanced metrics failed (non-fatal)', adv_err)

    def _load_benchmark(self, symbol: str, start_date: str, end_date: str, target_len: int):
        """Load benchmark equity series normalized to start at 1.0."""
        import numpy as np
        import pandas as pd
        try:
            import yfinance as yf
            # Normalize benchmark symbol too
            norm_sym = self._normalize_symbols([symbol])
            bench_symbol = norm_sym[0] if norm_sym else symbol
            raw = yf.download(bench_symbol, start=start_date, end=end_date, progress=False)
            bench_data = self._extract_close_column(raw, [bench_symbol])
            if isinstance(bench_data, pd.DataFrame):
                bench_data = bench_data.iloc[:, 0]
            if bench_data is not None and len(bench_data) > 0:
                values = bench_data.values.astype(float).flatten()
                normalized = values / values[0]
                if len(normalized) != target_len:
                    indices = np.linspace(0, len(normalized) - 1, target_len).astype(int)
                    normalized = normalized[indices]
                return normalized
        except Exception as e:
            self._error(f'Benchmark load failed for {symbol}', e)
        return None


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    import io
    import os
    import warnings

    # Suppress all warnings (they can corrupt stdout JSON)
    warnings.filterwarnings('ignore')
    os.environ['PYTHONWARNINGS'] = 'ignore'

    # Check if we're receiving data via stdin (--stdin flag)
    use_stdin = '--stdin' in sys.argv

    if use_stdin:
        # stdin mode: command is argv[1], JSON comes from stdin
        if len(sys.argv) < 2:
            print(json_response({
                'success': False,
                'error': 'Usage: python vectorbt_provider.py <command> --stdin < data.json'
            }))
            return

        command = sys.argv[1]
        # Read JSON from stdin
        json_args = sys.stdin.read()
        print(f'[PYTHON] Received {len(json_args)} bytes from stdin', file=sys.stderr)
    else:
        # Standard mode: command and JSON both from argv
        if len(sys.argv) < 3:
            print(json_response({
                'success': False,
                'error': 'Usage: python vectorbt_provider.py <command> <json_args>'
            }))
            return

        command = sys.argv[1]
        json_args = sys.argv[2]

    # Redirect stdout to a buffer during execution so that any stray
    # print() calls from libraries (numpy, pandas, yfinance, vectorbt)
    # don't corrupt the JSON output. We capture them and only emit the
    # final JSON response on the real stdout.
    real_stdout = sys.stdout
    captured = io.StringIO()

    try:
        sys.stdout = captured
        args = parse_json_input(json_args)
        provider = VectorBTProvider()

        if command == 'test_import':
            vbt = provider._import_vbt()
            result_str = json_response({'success': True, 'version': vbt.__version__})
        elif command == 'test_connection':
            result = provider.test_connection()
            result_str = json_response(result)
        elif command == 'initialize':
            result = provider.initialize(args)
            result_str = json_response(result)
        elif command == 'run_backtest':
            result = provider.run_backtest(args)
            result_str = json_response(result)
        elif command == 'optimize':
            result = provider.optimize(args)
            result_str = json_response(result)
        elif command == 'walk_forward':
            result = provider.walk_forward(args)
            result_str = json_response(result)
        elif command == 'get_strategies':
            result = provider.get_strategies(args)
            result_str = json_response(result)
        elif command == 'get_indicators':
            result = provider.get_indicators(args)
            result_str = json_response(result)
        elif command == 'generate_signals':
            result = provider.generate_signals(args)
            result_str = json_response(result)
        elif command == 'generate_labels':
            result = provider.generate_labels(args)
            result_str = json_response(result)
        elif command == 'generate_splits':
            result = provider.generate_splits(args)
            result_str = json_response(result)
        elif command == 'get_historical_data':
            result = provider.get_historical_data(args)
            result_str = json_response({'success': True, 'data': result})
        elif command == 'analyze_returns':
            result = provider.analyze_returns(args)
            result_str = json_response(result)
        elif command == 'indicator_signals':
            result = provider.indicator_signals(args)
            result_str = json_response(result)
        elif command == 'indicator_param_sweep':
            result = provider.indicator_param_sweep(args)
            result_str = json_response(result)
        elif command == 'labels_to_signals':
            result = provider.labels_to_signals(args)
            result_str = json_response(result)
        else:
            result_str = json_response({'success': False, 'error': f'Unknown command: {command}'})

        # Emit only the JSON on the real stdout
        sys.stdout = real_stdout
        print(result_str)

    except Exception as e:
        sys.stdout = real_stdout
        # Dump captured output to stderr for debugging
        stray_output = captured.getvalue()
        if stray_output:
            print(stray_output, file=sys.stderr)
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': __import__('traceback').format_exc()
        }))


if __name__ == '__main__':
    main()
