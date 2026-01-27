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
            logs.append(f'{self._current_timestamp()}: Downloading data for {symbols}')
            logs.append(f'{self._current_timestamp()}: Normalized symbols: {self._normalize_symbols(symbols)}')

            close_series, using_synthetic = self._load_market_data(
                symbols, start_date, end_date
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
                entries, exits = strat.build_strategy_signals(
                    vbt, strategy_type, close_series, parameters
                )

                n_entries = int(entries.sum()) if hasattr(entries, 'sum') else 0
                n_exits = int(exits.sum()) if hasattr(exits, 'sum') else 0
                logs.append(f'{self._current_timestamp()}: Signals generated: {n_entries} entries, {n_exits} exits')

                if n_entries == 0:
                    logs.append(f'{self._current_timestamp()}: WARNING: No entry signals generated! '
                                f'Check strategy parameters or data length ({len(close_series)} bars)')

                # --- Build portfolio with all features ---
                portfolio = pf.build_portfolio(
                    vbt, close_series, entries, exits,
                    initial_capital, request
                )

            n_trades = len(portfolio.trades.records_readable) if hasattr(portfolio.trades, 'records_readable') else 0
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
            close_series, _ = self._load_market_data(symbols, start_date, end_date)

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
            close_series, _ = self._load_market_data(symbols, start_date, end_date)

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

    def _load_market_data(self, symbols: list, start_date: str, end_date: str):
        """
        Load market data, falling back to synthetic if unavailable.

        Handles:
        - Indian stocks (auto-appends .NS suffix for NSE tickers)
        - Multi-symbol downloads with newer yfinance MultiIndex columns
        - Weighted composite series for multi-asset portfolios

        Returns:
            (close_series, using_synthetic) tuple
        """
        import pandas as pd
        import numpy as np

        using_synthetic = False

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
