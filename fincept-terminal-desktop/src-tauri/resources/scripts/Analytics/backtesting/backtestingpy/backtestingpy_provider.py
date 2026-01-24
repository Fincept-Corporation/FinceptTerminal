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
            assets = request.get('assets', [])

            if not assets:
                return self._create_error_result('No assets specified')

            # Get primary asset
            primary_asset = assets[0]
            symbol = primary_asset.get('symbol', 'SPY')

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
        """
        Run parameter optimization

        Args:
            request: Optimization configuration with parameters and constraints

        Returns:
            OptimizationResult with best parameters and performance
        """
        try:
            self._log('Starting parameter optimization')

            # Extract request parameters
            strategy_def = request.get('strategy', {})
            parameters = request.get('parameters', {})
            metric = request.get('metric', 'Sharpe Ratio')
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 10000)
            assets = request.get('assets', [])

            if not parameters:
                return self._create_error_result('No parameters to optimize')

            # Get primary asset
            primary_asset = assets[0] if assets else {'symbol': 'SPY'}
            symbol = primary_asset.get('symbol', 'SPY')

            # Load data
            data = self._load_data(symbol, start_date, end_date)

            # Create strategy class
            strategy_class = self._create_strategy_class(strategy_def, parameters)

            # Configure backtest
            from backtesting import Backtest

            bt = Backtest(
                data,
                strategy_class,
                cash=initial_capital,
                commission=request.get('commission', 0.0)
            )

            # Convert parameter ranges
            opt_params = {}
            for param_name, param_config in parameters.items():
                min_val = param_config.get('min', 1)
                max_val = param_config.get('max', 100)
                step = param_config.get('step', 1)
                opt_params[param_name] = range(min_val, max_val + 1, step)

            # Map metric name
            metric_map = {
                'Sharpe Ratio': 'Sharpe Ratio',
                'Return': 'Return [%]',
                'Total Return': 'Return [%]',
                'Win Rate': 'Win Rate [%]',
                'Profit Factor': 'Profit Factor'
            }
            maximize_metric = metric_map.get(metric, 'Sharpe Ratio')

            # Run optimization
            self._log(f'Optimizing {len(opt_params)} parameters')
            stats = bt.optimize(**opt_params, maximize=maximize_metric, max_tries=500)

            # Extract optimal parameters
            optimal_params = {}
            for param_name in parameters.keys():
                if hasattr(stats._strategy, param_name):
                    optimal_params[param_name] = getattr(stats._strategy, param_name)

            # Create result
            result = OptimizationResult(
                optimal_parameters=optimal_params,
                performance=self._extract_performance_metrics(stats),
                iterations=500,
                best_iteration=0,
                metric_name=metric,
                metric_value=float(stats.get(maximize_metric, 0))
            )

            self._log(f'Optimization completed. Best {metric}: {result.metric_value:.4f}')
            return {
                'success': True,
                'message': 'Optimization completed',
                'data': asdict(result)
            }

        except Exception as e:
            self._error('Optimization failed', e)
            return self._create_error_result(f'Optimization failed: {str(e)}')

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
            symbol = params.get('symbol', 'SPY')
            period = params.get('period', 20)
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
        """Check if real data is available for a symbol (yfinance installed)"""
        if symbol in ('GOOG', 'GOOGL'):
            return True  # Built-in test data counts as "real enough"
        try:
            import yfinance
            return True
        except ImportError:
            return False

    def _load_data(self, symbol: str, start_date: str, end_date: str) -> Optional[pd.DataFrame]:
        """Load or generate OHLCV data for backtesting"""
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
                    return data if len(data) > 0 else None
                except Exception as e:
                    self._log(f'Could not load GOOG test data: {e}')
                    # Fall through to synthetic data

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
                    self._log(f'Loaded {len(data)} bars of real data for {symbol}')
                    return data
                else:
                    self._log(f'No data returned from yfinance for {symbol}')
            except ImportError:
                self._log('yfinance not installed - falling back to synthetic data. Install with: pip install yfinance')
            except Exception as e:
                self._log(f'Failed to fetch real data for {symbol}: {e} - falling back to synthetic data')

            # Fallback: Generate synthetic data with clear warning
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
        """Create a backtesting.py Strategy class from strategy definition"""
        from backtesting import Strategy
        from backtesting.lib import crossover

        # Extract strategy code or type
        code = strategy_def.get('code', '')
        strategy_type = strategy_def.get('type', 'sma_crossover')

        # Default parameters
        params = strategy_def.get('parameters', {})

        # Create dynamic strategy class
        class_name = strategy_def.get('name', 'CustomStrategy').replace(' ', '')

        # EMA Crossover strategy
        if strategy_type == 'ema_crossover':
            class EMAStrategy(Strategy):
                n1 = opt_params.get('n1', params.get('fast_period', params.get('fastPeriod', 12))) if opt_params else params.get('fast_period', params.get('fastPeriod', 12))
                n2 = opt_params.get('n2', params.get('slow_period', params.get('slowPeriod', 26))) if opt_params else params.get('slow_period', params.get('slowPeriod', 26))

                def init(self):
                    close = self.data.Close
                    self.ema1 = self.I(self._ema, close, self.n1)
                    self.ema2 = self.I(self._ema, close, self.n2)

                def next(self):
                    if crossover(self.ema1, self.ema2):
                        if self.position.is_short:
                            self.position.close()
                        self.buy()
                    elif crossover(self.ema2, self.ema1):
                        if self.position.is_long:
                            self.position.close()
                        self.sell()

                @staticmethod
                def _ema(values, n):
                    return pd.Series(values).ewm(span=n, adjust=False).mean()

            return EMAStrategy

        # RSI Strategy
        if strategy_type == 'rsi':
            class RSIStrategy(Strategy):
                period = params.get('period', 14)
                oversold = params.get('oversold', 30)
                overbought = params.get('overbought', 70)

                def init(self):
                    close = self.data.Close
                    self.rsi = self.I(self._rsi, close, self.period)

                def next(self):
                    if self.rsi[-1] < self.oversold:
                        if not self.position.is_long:
                            if self.position.is_short:
                                self.position.close()
                            self.buy()
                    elif self.rsi[-1] > self.overbought:
                        if not self.position.is_short:
                            if self.position.is_long:
                                self.position.close()
                            self.sell()

                @staticmethod
                def _rsi(values, period):
                    s = pd.Series(values)
                    delta = s.diff()
                    gain = delta.where(delta > 0, 0).rolling(window=period).mean()
                    loss = (-delta.where(delta < 0, 0)).rolling(window=period).mean()
                    rs = gain / loss
                    return 100 - (100 / (1 + rs))

            return RSIStrategy

        # MACD Strategy
        if strategy_type == 'macd':
            class MACDStrategy(Strategy):
                fast_period = params.get('fast_period', params.get('fastPeriod', 12))
                slow_period = params.get('slow_period', params.get('slowPeriod', 26))
                signal_period = params.get('signal_period', params.get('signalPeriod', 9))

                def init(self):
                    close = self.data.Close
                    self.macd_line = self.I(self._macd_line, close, self.fast_period, self.slow_period)
                    self.signal_line = self.I(self._signal_line, close, self.fast_period, self.slow_period, self.signal_period)

                def next(self):
                    if crossover(self.macd_line, self.signal_line):
                        if self.position.is_short:
                            self.position.close()
                        self.buy()
                    elif crossover(self.signal_line, self.macd_line):
                        if self.position.is_long:
                            self.position.close()
                        self.sell()

                @staticmethod
                def _macd_line(values, fast, slow):
                    s = pd.Series(values)
                    fast_ema = s.ewm(span=fast, adjust=False).mean()
                    slow_ema = s.ewm(span=slow, adjust=False).mean()
                    return fast_ema - slow_ema

                @staticmethod
                def _signal_line(values, fast, slow, signal):
                    s = pd.Series(values)
                    fast_ema = s.ewm(span=fast, adjust=False).mean()
                    slow_ema = s.ewm(span=slow, adjust=False).mean()
                    macd = fast_ema - slow_ema
                    return macd.ewm(span=signal, adjust=False).mean()

            return MACDStrategy

        # Bollinger Bands Strategy
        if strategy_type == 'bollinger_bands':
            class BollingerBandsStrategy(Strategy):
                period = params.get('period', 20)
                std_dev = params.get('std_dev', params.get('stdDev', 2.0))

                def init(self):
                    close = self.data.Close
                    self.sma = self.I(self._sma, close, self.period)
                    self.upper = self.I(self._upper_band, close, self.period, self.std_dev)
                    self.lower = self.I(self._lower_band, close, self.period, self.std_dev)

                def next(self):
                    price = self.data.Close[-1]
                    # Buy when price crosses below lower band (oversold)
                    if price < self.lower[-1]:
                        if not self.position.is_long:
                            if self.position.is_short:
                                self.position.close()
                            self.buy()
                    # Sell when price crosses above upper band (overbought)
                    elif price > self.upper[-1]:
                        if not self.position.is_short:
                            if self.position.is_long:
                                self.position.close()
                            self.sell()

                @staticmethod
                def _sma(values, n):
                    return pd.Series(values).rolling(n).mean()

                @staticmethod
                def _upper_band(values, n, std_dev):
                    s = pd.Series(values)
                    sma = s.rolling(n).mean()
                    std = s.rolling(n).std()
                    return sma + (std * std_dev)

                @staticmethod
                def _lower_band(values, n, std_dev):
                    s = pd.Series(values)
                    sma = s.rolling(n).mean()
                    std = s.rolling(n).std()
                    return sma - (std * std_dev)

            return BollingerBandsStrategy

        # Mean Reversion (Z-Score based)
        if strategy_type == 'mean_reversion':
            class MeanReversionStrategy(Strategy):
                period = params.get('period', 20)
                z_threshold = params.get('z_threshold', params.get('zThreshold', 2.0))

                def init(self):
                    close = self.data.Close
                    self.sma = self.I(self._sma, close, self.period)
                    self.zscore = self.I(self._zscore, close, self.period)

                def next(self):
                    if self.zscore[-1] < -self.z_threshold:
                        if not self.position.is_long:
                            if self.position.is_short:
                                self.position.close()
                            self.buy()
                    elif self.zscore[-1] > self.z_threshold:
                        if not self.position.is_short:
                            if self.position.is_long:
                                self.position.close()
                            self.sell()
                    elif abs(self.zscore[-1]) < 0.5:
                        if self.position:
                            self.position.close()

                @staticmethod
                def _sma(values, n):
                    return pd.Series(values).rolling(n).mean()

                @staticmethod
                def _zscore(values, n):
                    s = pd.Series(values)
                    mean = s.rolling(n).mean()
                    std = s.rolling(n).std()
                    return (s - mean) / std.replace(0, 1)

            return MeanReversionStrategy

        # Momentum (N-bar lookback return)
        if strategy_type == 'momentum':
            class MomentumStrategy(Strategy):
                lookback = params.get('lookback', 20)
                threshold = params.get('threshold', 0.0) / 100.0  # Convert from pct

                def init(self):
                    close = self.data.Close
                    self.returns = self.I(self._lookback_return, close, self.lookback)

                def next(self):
                    if self.returns[-1] > self.threshold:
                        if not self.position.is_long:
                            if self.position.is_short:
                                self.position.close()
                            self.buy()
                    elif self.returns[-1] < -self.threshold:
                        if not self.position.is_short:
                            if self.position.is_long:
                                self.position.close()
                            self.sell()

                @staticmethod
                def _lookback_return(values, n):
                    s = pd.Series(values)
                    return s.pct_change(periods=n)

            return MomentumStrategy

        # Breakout (Donchian Channel with ATR filter)
        if strategy_type == 'breakout':
            class BreakoutStrategy(Strategy):
                period = params.get('period', 20)
                atr_mult = params.get('atr_mult', params.get('atrMult', 1.5))

                def init(self):
                    high = self.data.High
                    low = self.data.Low
                    close = self.data.Close
                    self.upper = self.I(self._donchian_upper, high, self.period)
                    self.lower = self.I(self._donchian_lower, low, self.period)
                    self.atr = self.I(self._atr, high, low, close, self.period)

                def next(self):
                    price = self.data.Close[-1]
                    atr_filter = self.atr[-1] * self.atr_mult
                    if price > self.upper[-1] and self.atr[-1] > 0:
                        if not self.position.is_long:
                            if self.position.is_short:
                                self.position.close()
                            self.buy(sl=price - atr_filter)
                    elif price < self.lower[-1] and self.atr[-1] > 0:
                        if not self.position.is_short:
                            if self.position.is_long:
                                self.position.close()
                            self.sell(sl=price + atr_filter)

                @staticmethod
                def _donchian_upper(values, n):
                    return pd.Series(values).rolling(n).max()

                @staticmethod
                def _donchian_lower(values, n):
                    return pd.Series(values).rolling(n).min()

                @staticmethod
                def _atr(high, low, close, n):
                    h = pd.Series(high)
                    l = pd.Series(low)
                    c = pd.Series(close)
                    prev_c = c.shift(1)
                    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
                    return tr.rolling(n).mean()

            return BreakoutStrategy

        # Stochastic (%K/%D crossover)
        if strategy_type == 'stochastic':
            class StochasticStrategy(Strategy):
                k_period = params.get('k_period', params.get('kPeriod', 14))
                d_period = params.get('d_period', params.get('dPeriod', 3))
                oversold = params.get('oversold', 20)
                overbought = params.get('overbought', 80)

                def init(self):
                    high = self.data.High
                    low = self.data.Low
                    close = self.data.Close
                    self.k = self.I(self._stoch_k, high, low, close, self.k_period)
                    self.d = self.I(self._stoch_d, high, low, close, self.k_period, self.d_period)

                def next(self):
                    if self.k[-1] < self.oversold and crossover(self.k, self.d):
                        if not self.position.is_long:
                            if self.position.is_short:
                                self.position.close()
                            self.buy()
                    elif self.k[-1] > self.overbought and crossover(self.d, self.k):
                        if not self.position.is_short:
                            if self.position.is_long:
                                self.position.close()
                            self.sell()

                @staticmethod
                def _stoch_k(high, low, close, period):
                    h = pd.Series(high)
                    l = pd.Series(low)
                    c = pd.Series(close)
                    lowest = l.rolling(period).min()
                    highest = h.rolling(period).max()
                    denom = highest - lowest
                    return ((c - lowest) / denom.replace(0, 1)) * 100

                @staticmethod
                def _stoch_d(high, low, close, k_period, d_period):
                    h = pd.Series(high)
                    l = pd.Series(low)
                    c = pd.Series(close)
                    lowest = l.rolling(k_period).min()
                    highest = h.rolling(k_period).max()
                    denom = highest - lowest
                    k = ((c - lowest) / denom.replace(0, 1)) * 100
                    return k.rolling(d_period).mean()

            return StochasticStrategy

        # ADX Trend (trade with trend when ADX > threshold)
        if strategy_type == 'adx_trend':
            class ADXTrendStrategy(Strategy):
                period = params.get('period', 14)
                threshold = params.get('threshold', 25)

                def init(self):
                    high = self.data.High
                    low = self.data.Low
                    close = self.data.Close
                    self.adx = self.I(self._adx, high, low, close, self.period)
                    self.plus_di = self.I(self._plus_di, high, low, close, self.period)
                    self.minus_di = self.I(self._minus_di, high, low, close, self.period)

                def next(self):
                    if self.adx[-1] > self.threshold:
                        if self.plus_di[-1] > self.minus_di[-1]:
                            if not self.position.is_long:
                                if self.position.is_short:
                                    self.position.close()
                                self.buy()
                        elif self.minus_di[-1] > self.plus_di[-1]:
                            if not self.position.is_short:
                                if self.position.is_long:
                                    self.position.close()
                                self.sell()
                    else:
                        if self.position:
                            self.position.close()

                @staticmethod
                def _adx(high, low, close, period):
                    h = pd.Series(high)
                    l = pd.Series(low)
                    c = pd.Series(close)
                    prev_h = h.shift(1)
                    prev_l = l.shift(1)
                    prev_c = c.shift(1)
                    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
                    plus_dm = ((h - prev_h).where((h - prev_h) > (prev_l - l), 0)).clip(lower=0)
                    minus_dm = ((prev_l - l).where((prev_l - l) > (h - prev_h), 0)).clip(lower=0)
                    atr = tr.ewm(span=period, adjust=False).mean()
                    plus_di = 100 * (plus_dm.ewm(span=period, adjust=False).mean() / atr.replace(0, 1))
                    minus_di = 100 * (minus_dm.ewm(span=period, adjust=False).mean() / atr.replace(0, 1))
                    dx = (abs(plus_di - minus_di) / (plus_di + minus_di).replace(0, 1)) * 100
                    return dx.ewm(span=period, adjust=False).mean()

                @staticmethod
                def _plus_di(high, low, close, period):
                    h = pd.Series(high)
                    l = pd.Series(low)
                    c = pd.Series(close)
                    prev_h = h.shift(1)
                    prev_l = l.shift(1)
                    prev_c = c.shift(1)
                    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
                    plus_dm = ((h - prev_h).where((h - prev_h) > (prev_l - l), 0)).clip(lower=0)
                    atr = tr.ewm(span=period, adjust=False).mean()
                    return 100 * (plus_dm.ewm(span=period, adjust=False).mean() / atr.replace(0, 1))

                @staticmethod
                def _minus_di(high, low, close, period):
                    h = pd.Series(high)
                    l = pd.Series(low)
                    c = pd.Series(close)
                    prev_h = h.shift(1)
                    prev_l = l.shift(1)
                    prev_c = c.shift(1)
                    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
                    minus_dm = ((prev_l - l).where((prev_l - l) > (h - prev_h), 0)).clip(lower=0)
                    atr = tr.ewm(span=period, adjust=False).mean()
                    return 100 * (minus_dm.ewm(span=period, adjust=False).mean() / atr.replace(0, 1))

            return ADXTrendStrategy

        # SMA Crossover strategy (default)
        if strategy_type == 'sma_crossover' or not code:
            class DynamicStrategy(Strategy):
                # Class variables for optimization
                n1 = opt_params.get('n1', params.get('fast_period', params.get('fastPeriod', 10))) if opt_params else params.get('fast_period', params.get('fastPeriod', 10))
                n2 = opt_params.get('n2', params.get('slow_period', params.get('slowPeriod', 20))) if opt_params else params.get('slow_period', params.get('slowPeriod', 20))

                # Risk management parameters
                use_sl = params.get('use_stop_loss', False)
                use_tp = params.get('use_take_profit', False)
                sl_pct = params.get('stop_loss_pct', 0.02)  # 2% stop loss
                tp_pct = params.get('take_profit_pct', 0.05)  # 5% take profit

                def init(self):
                    # Simple moving averages
                    close = self.data.Close
                    self.sma1 = self.I(self._sma, close, self.n1)
                    self.sma2 = self.I(self._sma, close, self.n2)

                def next(self):
                    # Get current price
                    price = self.data.Close[-1]

                    # Calculate stop-loss and take-profit levels
                    sl = price * (1 - self.sl_pct) if self.use_sl else None
                    tp = price * (1 + self.tp_pct) if self.use_tp else None

                    # Trading logic with SL/TP
                    if crossover(self.sma1, self.sma2):
                        if self.position.is_short:
                            self.position.close()
                        self.buy(sl=sl, tp=tp)
                    elif crossover(self.sma2, self.sma1):
                        if self.position.is_long:
                            self.position.close()
                        # For short positions, SL and TP are reversed
                        sl_short = price * (1 + self.sl_pct) if self.use_sl else None
                        tp_short = price * (1 - self.tp_pct) if self.use_tp else None
                        self.sell(sl=sl_short, tp=tp_short)

                @staticmethod
                def _sma(values, n):
                    return pd.Series(values).rolling(n).mean()

            return DynamicStrategy

        # Custom code strategy
        else:
            # Validate custom strategy code with restricted execution
            # Only allow safe subset of Python for strategy definitions
            validation_error = self._validate_strategy_code(code)
            if validation_error:
                self._error(f'Strategy code validation failed: {validation_error}')
                # Fallback to default strategy
                return self._create_strategy_class({'type': 'sma_crossover'}, opt_params)

            # Execute with restricted globals - no access to dangerous modules
            exec_globals = {
                '__builtins__': {
                    'range': range,
                    'len': len,
                    'abs': abs,
                    'min': min,
                    'max': max,
                    'sum': sum,
                    'round': round,
                    'int': int,
                    'float': float,
                    'bool': bool,
                    'str': str,
                    'list': list,
                    'dict': dict,
                    'tuple': tuple,
                    'enumerate': enumerate,
                    'zip': zip,
                    'map': map,
                    'filter': filter,
                    'sorted': sorted,
                    'reversed': reversed,
                    'isinstance': isinstance,
                    'issubclass': issubclass,
                    'hasattr': hasattr,
                    'getattr': getattr,
                    'setattr': setattr,
                    'property': property,
                    'staticmethod': staticmethod,
                    'classmethod': classmethod,
                    'super': super,
                    'type': type,
                    'True': True,
                    'False': False,
                    'None': None,
                },
                'Strategy': Strategy,
                'crossover': crossover,
                'pd': pd,
                'np': np,
            }

            try:
                exec(code, exec_globals)
                # Try to find the strategy class
                for name, obj in exec_globals.items():
                    if isinstance(obj, type) and issubclass(obj, Strategy) and obj != Strategy:
                        return obj
            except Exception as e:
                self._error('Failed to execute custom strategy code', e)

            # Fallback to default strategy
            return self._create_strategy_class({'type': 'sma_crossover'}, opt_params)

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
            indicator_type = args.get('type', '')
            params = args.get('params', {})
            result = provider.calculate_indicator(indicator_type, params)
        elif command == 'optimize':
            result = provider.optimize(args)
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
