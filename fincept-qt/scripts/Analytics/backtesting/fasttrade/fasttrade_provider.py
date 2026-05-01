"""
Fast-Trade Provider Implementation

Fast, low-code backtesting library utilizing pandas and technical analysis indicators.
JSON-based strategy definition with extensive indicator support via FINTA.

FEATURES IMPLEMENTED:
=====================

Core Backtesting:
- run_backtest() - Execute fast backtests with comprehensive statistics
- JSON-based strategy definition - No coding required for basic strategies
- Performance metrics - Sharpe, Sortino, Calmar, drawdown, win rate, etc.
- Trade tracking - Entry/exit prices, P&L, holding periods

Configuration Options:
- Initial capital (base_balance parameter)
- Commission (fixed percentage per trade)
- Trailing stop loss (percentage-based risk management)
- Timeframe/frequency support (1Min, 5Min, 1H, 1D, etc.)
- Enter/exit logic with conditional operators
- Any_enter/any_exit (OR logic vs AND logic)

Strategy Features:
- Logic-based entry/exit (>, <, =, >=, <=)
- Lookback periods for signal confirmation
- Custom datapoints (technical indicators)
- Trailing stop loss
- Rules system for result filtering

Optimization:
- Not currently implemented in fast-trade core
- Can run multiple backtests with different parameters

Indicators (via FINTA):
- 80+ technical indicators
- SMA, EMA, WMA, HMA, KAMA (moving averages)
- RSI, MACD, Stochastic (oscillators)
- Bollinger Bands, ATR, Keltner Channels (volatility)
- OBV, MFI, ADL (volume indicators)
- And many more...

Data Handling:
- Yahoo Finance data loading via yfinance
- OHLCV data support (pandas DataFrame)
- Built-in Archive for Binance/Coinbase data
- Synthetic data generation (fallback for testing)
- Date range filtering
- Multiple timeframe support

FEATURES NOT IMPLEMENTED:
=========================
- Live trading integration
- Advanced optimization (grid search, genetic algorithms)
- Real-time data streaming
- Multiple asset backtesting in single run
- Custom Python strategy classes (uses JSON logic only)

LIMITATIONS:
============
- Strategy logic is JSON-based (limited compared to full Python code)
- No built-in portfolio management
- Single asset per backtest
- Results depend on data quality
"""

import sys
import json
import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional
from pathlib import Path
from datetime import datetime, timedelta
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
    json_response,
    parse_json_input
)

# Import fast-trade wrapper modules
# Fix: Use absolute imports instead of relative imports for CLI execution
ft_module_path = Path(__file__).parent
sys.path.insert(0, str(ft_module_path))

from ft_backtest import (
    run_backtest as ft_run,
    validate_backtest as ft_validate,
    run_multiple_backtests,
)
from ft_data import (
    generate_synthetic_ohlcv,
    load_basic_df_from_csv,
    standardize_df,
    load_yfinance_data,
)
from ft_summary import (
    build_summary,
    calculate_drawdown_metrics,
    calculate_trade_quality,
    calculate_trade_streaks,
    create_trade_log,
)
from ft_indicators import (
    list_available_indicators,
    get_transformer_map,
)
from ft_strategies import (
    sma_crossover as sma_crossover_config,
    ema_crossover as ema_crossover_config,
    rsi_strategy as rsi_config,
    macd_strategy as macd_config,
    bollinger_bands_strategy as bb_config,
    build_custom_strategy,
    list_strategies,
)
from ft_evaluate import evaluate_rules
from ft_utils import resample, trending_up, trending_down


class FastTradeProvider(BacktestingProviderBase):
    """
    Fast-Trade Provider

    Executes fast, low-code backtests using fast-trade library.
    JSON-based strategy definition with extensive indicator support.
    """

    def __init__(self):
        super().__init__()
        self.fast_trade = None

    # ========================================================================
    # Properties
    # ========================================================================

    @property
    def name(self) -> str:
        return "Fast-Trade"

    @property
    def version(self) -> str:
        try:
            import fast_trade
            return fast_trade.__version__
        except:
            return "0.4.0"

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': False,  # Manual parameter iteration only
            'liveTrading': False,
            'research': True,
            'multiAsset': ['stocks', 'crypto', 'forex', 'futures'],
            'indicators': True,
            'customStrategies': True,  # JSON-based logic
            'maxConcurrentBacktests': 50,  # Very fast
            'supportedTimeframes': ['minute', 'hour', 'daily'],
            'supportedMarkets': ['us-equity', 'crypto', 'forex', 'futures'],
            'jsonBasedStrategy': True,
            'trailingStopLoss': True,
            'conditionalLogic': True,
            'fintaIndicators': True,
        }

    # ========================================================================
    # Core Methods
    # ========================================================================

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize Fast-Trade provider"""
        try:
            # Import fast-trade library
            import fast_trade
            from fast_trade import run_backtest, validate_backtest

            self.fast_trade = fast_trade
            self.run_backtest = run_backtest
            self.validate_backtest = validate_backtest

            self.config = config
            return self._create_success_result(f'Fast-Trade {self.version} ready')

        except ImportError as e:
            self._error('Fast-Trade not installed', e)
            return self._create_error_result('Fast-Trade not installed. Run: pip install fast-trade')
        except Exception as e:
            self._error('Failed to initialize Fast-Trade', e)
            return self._create_error_result(f'Initialization error: {str(e)}')

    def test_connection(self) -> Dict[str, Any]:
        """Test if fast-trade is available"""
        try:
            import fast_trade
            return self._create_success_result(f'Fast-Trade {fast_trade.__version__} is available')
        except ImportError:
            return self._create_error_result('Fast-Trade not installed. Run: pip install fast-trade')
        except Exception as e:
            return self._create_error_result(str(e))

    def disconnect(self) -> None:
        """No-op for subprocess providers"""
        pass

    # ========================================================================
    # Backtest Execution
    # ========================================================================

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """
        Run backtest using fast-trade

        Args:
            request: Backtest configuration with strategy, data, settings

        Returns:
            BacktestResult with performance metrics, trades, equity curve
        """
        try:
            # Use our custom wrapper instead of library's run_backtest
            # This allows passing data directly without download
            self._log('Starting Fast-Trade backtest execution')

            # Extract request parameters
            strategy_def = request.get('strategy', {})
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 10000)
            # Frontend sends params under `params`; legacy callers used `parameters`.
            parameters = strategy_def.get('params') or strategy_def.get('parameters', {})

            # Canonical input: `symbols: [str]`. Fallback: `assets: [{symbol}]`.
            symbols = request.get('symbols', [])
            if not symbols:
                assets = request.get('assets', [])
                symbols = [a.get('symbol') for a in assets if isinstance(a, dict) and a.get('symbol')]
            # _prepare_data still expects an assets-like list; build a minimal one.
            assets_for_data = [{'symbol': s} for s in symbols] if symbols else []

            # Generate or load data
            data = self._prepare_data(assets_for_data, start_date, end_date)

            # Build fast-trade configuration (without data in config)
            ft_config = self._build_fasttrade_config(
                strategy_def, parameters, initial_capital, None, start_date, end_date
            )

            # Validate configuration
            validation = ft_validate(ft_config)
            if not validation.get('valid', True):
                errors = validation.get('errors', [])
                return self._create_error_result(f'Strategy validation failed: {errors}')

            # Prepare data for fast-trade (needs 'date' column, not index)
            if 'date' not in data.columns:
                data_ft = data.reset_index()
                if data_ft.columns[0] != 'date':
                    data_ft = data_ft.rename(columns={data_ft.columns[0]: 'date'})
            else:
                data_ft = data.copy()

            # Run backtest using our wrapper (pass data separately).
            #
            # fast-trade 2.0.0 has a bug in build_summary -> calculate_trade_streaks
            # that crashes with IndexError when zero trades are produced. Retry
            # without summary in that case and synthesise the missing summary
            # ourselves so the user still gets a usable empty result instead
            # of an error banner.
            self._log('Executing fast-trade backtest...')
            try:
                result = ft_run(backtest=ft_config, df=data_ft, summary=True)
            except IndexError as ie:
                self._log(f'fast-trade summary crashed (likely zero trades): {ie}; retrying without summary')
                result = ft_run(backtest=ft_config, df=data_ft, summary=False)
                # Provide a minimal empty summary so _convert_result has something to read.
                if isinstance(result, dict) and 'summary' not in result:
                    result['summary'] = {
                        'num_trades': 0, 'win_perc': 0, 'loss_perc': 0,
                        'total_return': 0, 'avg_win': 0, 'avg_loss': 0,
                        'best_trade': 0, 'worst_trade': 0, 'sharpe': 0,
                        'sortino': 0, 'max_drawdown': 0, 'calmar': 0,
                    }

            # Convert result to standard format
            symbol_for_trades = symbols[0] if symbols else 'ASSET'
            backtest_result = self._convert_result(
                result, request.get('id', self._generate_id()),
                symbol=symbol_for_trades,
            )

            result_dict = backtest_result.to_dict()
            using_synthetic = getattr(self, '_using_synthetic', False)
            result_dict['using_synthetic_data'] = using_synthetic

            if using_synthetic:
                result_dict['synthetic_data_warning'] = (
                    'WARNING: This backtest used SYNTHETIC (fake) data because real market data '
                    'could not be loaded. Install yfinance (pip install yfinance) and ensure '
                    'internet connectivity for real results. These results have NO financial meaning.'
                )

            return {
                'success': True,
                'message': 'Backtest completed successfully',
                'data': result_dict
            }

        except Exception as e:
            self._error('Backtest execution failed', e)
            import traceback
            return self._create_error_result(
                f'Backtest failed: {str(e)}\n{traceback.format_exc()}'
            )

    # ========================================================================
    # Data Handling
    # ========================================================================

    def _prepare_data(
        self,
        assets: List[Dict[str, Any]],
        start_date: Optional[str],
        end_date: Optional[str]
    ) -> pd.DataFrame:
        """
        Prepare OHLCV data for backtesting

        Tries to load from yfinance first, falls back to synthetic data if unavailable.
        """
        self._log('Preparing market data...')

        # Get symbol from assets
        symbol = 'SPY'  # Default
        if assets and len(assets) > 0:
            symbol = assets[0].get('symbol', 'SPY')

        # Try to load from yfinance
        self._log(f'Attempting to load {symbol} from Yahoo Finance...')
        data = load_yfinance_data(
            symbol=symbol,
            start_date=start_date or '2020-01-01',
            end_date=end_date,
            interval='1d'  # Daily data by default
        )

        # Fallback to synthetic data if yfinance fails
        if data.empty:
            import sys
            # Use deterministic seed based on symbol name (not constant 42)
            sym_seed = sum(ord(c) for c in symbol) % (2**31)
            print(f'[WARNING] Using SYNTHETIC data for {symbol} (seed={sym_seed}). '
                  f'Results are NOT based on real market data. '
                  f'Install yfinance (pip install yfinance) for real data.', file=sys.stderr)
            self._log(f'WARNING: Yahoo Finance failed, generating SYNTHETIC data for {symbol}')
            data = generate_synthetic_ohlcv(
                periods=len(pd.date_range(
                    start=start_date or '2023-01-01',
                    end=end_date or '2024-01-01',
                    freq='1D'
                )),
                start_date=start_date or '2023-01-01',
                freq='1D',
                initial_price=100.0,
                volatility=0.02,
                drift=0.0002,
                seed=sym_seed,
            )
            self._using_synthetic = True
        else:
            self._log(f'Loaded {len(data)} periods of data from Yahoo Finance for {symbol}')
            self._using_synthetic = False

        return data

    # ========================================================================
    # Fast-Trade Configuration
    # ========================================================================

    def _build_fasttrade_config(
        self,
        strategy_def: Dict[str, Any],
        parameters: Dict[str, Any],
        initial_capital: float,
        data: pd.DataFrame,
        start_date: Optional[str] = None,
        end_date: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Build fast-trade backtest configuration from strategy definition

        Converts our standard strategy format to fast-trade's JSON format.
        """
        strategy_type = strategy_def.get('type', 'sma_crossover')
        trailing = parameters.get('trailingStopLoss', 0.05)

        # Use ft_strategies module for pre-built configs
        if strategy_type == 'sma_crossover':
            config = sma_crossover_config(
                fast_period=parameters.get('fastPeriod', 9),
                slow_period=parameters.get('slowPeriod', 21),
                initial_capital=initial_capital,
                trailing_stop=trailing,
            )

        elif strategy_type == 'rsi':
            config = rsi_config(
                period=parameters.get('period', 14),
                oversold=parameters.get('oversold', 30),
                overbought=parameters.get('overbought', 70),
                initial_capital=initial_capital,
                trailing_stop=trailing,
            )

        elif strategy_type == 'ema_crossover':
            config = ema_crossover_config(
                fast_period=parameters.get('fastPeriod', 12),
                slow_period=parameters.get('slowPeriod', 26),
                initial_capital=initial_capital,
                trailing_stop=trailing,
            )

        elif strategy_type == 'macd':
            config = macd_config(
                fast=parameters.get('fastPeriod', 12),
                slow=parameters.get('slowPeriod', 26),
                signal=parameters.get('signalPeriod', 9),
                initial_capital=initial_capital,
                trailing_stop=trailing,
            )

        elif strategy_type == 'bollinger_bands':
            config = bb_config(
                period=parameters.get('period', 20),
                std_dev=parameters.get('stdDev', 2),
                initial_capital=initial_capital,
                trailing_stop=trailing,
            )

        elif strategy_type == 'custom' and 'fastTradeConfig' in strategy_def:
            # User provided raw fast-trade configuration
            config = strategy_def['fastTradeConfig']
            config['base_balance'] = initial_capital

        else:
            # Default to simple buy and hold. We feed daily yfinance data
            # below ('1d' interval), so the freq must match — '1D' (capital)
            # is fast-trade's accepted form for daily.
            config = {
                'base_balance': initial_capital,
                'freq': '1D',
                'comission': 0.001,
                'datapoints': [],
                'enter': [['close', '>', 0]],
                'exit': [['close', '<', 0]],
            }

        # Do NOT add data to config - it will be passed separately to ft_run(backtest, df=data)
        # This prevents fast-trade from trying to download data

        # The ft_strategies builders default to `freq='1H'`, but our yfinance
        # loader fetches daily bars. Override here so fast-trade's resample
        # step doesn't choke (also note pandas 2.2 deprecated lowercase 'h';
        # fast-trade accepts the capital '1D' form for daily).
        config['freq'] = '1D'

        # fast-trade ≥0.2 requires `start` and `stop` (not the deprecated
        # `start_date`/`end_date`). The dataframe is the source of truth, so
        # use the data's actual range when caller-supplied dates are missing.
        if start_date:
            config['start'] = start_date
        if end_date:
            config['stop'] = end_date

        return config

    # ========================================================================
    # Result Conversion
    # ========================================================================

    def _convert_result(self, ft_result: Dict[str, Any], backtest_id: str, symbol: str = 'ASSET') -> BacktestResult:
        """Convert a fast-trade 2.0 result dict to our standard BacktestResult.

        Fast-trade emits metrics in a few different places:
          - top-level summary (`return_perc`, `sharpe_ratio`, `max_drawdown`
            in DOLLARS, `win_perc`, `loss_perc`, `num_trades`, ...)
          - `summary.risk_metrics` (`sortino_ratio`, `calmar_ratio`,
            `annualized_volatility`)
          - `summary.drawdown_metrics` (`max_drawdown_pct` in PERCENT,
            `max_drawdown_duration` in bars)
          - `summary.trade_quality` (`profit_factor`,
            `largest_winning_trade`, `largest_losing_trade`)

        We translate them all to the canonical PerformanceMetrics dataclass
        whose values are fractions in [-1, 1] (or magnitudes for drawdown),
        matching the convention used by every other provider.
        """
        summary = ft_result.get('summary', {}) or {}
        df = ft_result.get('df', pd.DataFrame())
        trade_df = ft_result.get('trade_df', pd.DataFrame())

        risk = summary.get('risk_metrics') or {}
        dd = summary.get('drawdown_metrics') or {}
        quality = summary.get('trade_quality') or {}

        # Total return: fast-trade emits as a percent (e.g. 10.85 for 10.85%).
        return_perc = float(summary.get('return_perc', 0) or 0)
        total_return = return_perc / 100.0

        # Days in the test: prefer counting bars in df since `total_days`
        # doesn't exist in fast-trade 2.0's summary.
        if not df.empty:
            try:
                first = pd.Timestamp(summary.get('first_tic') or df.index[0])
                last = pd.Timestamp(summary.get('last_tic') or df.index[-1])
                total_days = max(1, (last - first).days)
            except Exception:
                total_days = max(1, len(df))
        else:
            total_days = 365

        win_perc = float(summary.get('win_perc', 0) or 0)
        loss_perc = float(summary.get('loss_perc', 0) or 0)
        num_trades = int(summary.get('num_trades', 0) or 0)

        # max_drawdown: fast-trade reports the dollar magnitude at top level
        # but a percent under drawdown_metrics — we want a positive fraction.
        dd_pct = dd.get('max_drawdown_pct')
        if dd_pct is not None:
            max_drawdown = abs(float(dd_pct)) / 100.0
        else:
            max_drawdown = 0.0

        performance = PerformanceMetrics(
            total_return=total_return,
            annualized_return=self._calculate_annualized_return(total_return, total_days),
            sharpe_ratio=float(summary.get('sharpe_ratio', 0) or 0),
            sortino_ratio=float(risk.get('sortino_ratio', 0) or 0),
            max_drawdown=max_drawdown,
            win_rate=win_perc / 100.0,
            loss_rate=loss_perc / 100.0,
            profit_factor=float(quality.get('profit_factor', 0) or 0),
            volatility=float(risk.get('annualized_volatility', 0) or 0),
            calmar_ratio=float(risk.get('calmar_ratio', 0) or 0),
            total_trades=num_trades,
            winning_trades=int(summary.get('total_num_winning_trades', 0) or 0),
            losing_trades=int(summary.get('total_num_losing_trades', 0) or 0),
            average_win=float(summary.get('avg_win_perc', 0) or 0) / 100.0,
            average_loss=float(summary.get('avg_loss_perc', 0) or 0) / 100.0,
            largest_win=float(quality.get('largest_winning_trade', summary.get('best_trade_perc', 0)) or 0),
            largest_loss=float(quality.get('largest_losing_trade', summary.get('min_trade_perc', 0)) or 0),
            average_trade_return=float(summary.get('mean_trade_perc', 0) or 0),
            expectancy=0.0,  # not reported by fast-trade
            alpha=None,
            beta=None,
            max_drawdown_duration=int(dd.get('max_drawdown_duration', 0) or 0),
            information_ratio=None,
            treynor_ratio=None,
        )

        # Extract trades
        trades = self._extract_trades(trade_df, symbol=symbol)

        # Extract equity curve
        equity = self._extract_equity_curve(df)

        # Build statistics
        # Fast-trade 2.0 puts equity at top level: equity_peak/equity_final.
        # Initial capital comes from the config we sent (base_balance).
        initial_capital = float(ft_result.get('backtest', {}).get('base_balance', 10000) or 10000)
        statistics = BacktestStatistics(
            start_date=str(summary.get('first_tic', df.index[0] if len(df) else '')),
            end_date=str(summary.get('last_tic', df.index[-1] if len(df) else '')),
            initial_capital=initial_capital,
            final_capital=float(summary.get('equity_final', initial_capital) or initial_capital),
            total_fees=float(summary.get('total_fees', 0) or 0),
            total_slippage=0.0,  # Not tracked by fast-trade
            total_trades=int(summary.get('num_trades', 0) or 0),
            winning_days=0,  # Not directly provided
            losing_days=0,
            average_daily_return=0.0,
            best_day=0.0,
            worst_day=0.0,
            consecutive_wins=int(summary.get('trade_streaks', {}).get('max_win_streak', 0) or 0),
            consecutive_losses=int(summary.get('trade_streaks', {}).get('max_loss_streak', 0) or 0),
            max_drawdown_date=None,
            recovery_time=None,
        )

        # Build result
        result = BacktestResult(
            id=backtest_id,
            status='completed',
            performance=performance,
            trades=trades,
            equity=equity,
            statistics=statistics,
            logs=[
                f'Fast-Trade backtest completed',
                f'Total trades: {summary.get("num_trades", 0)}',
                f'Win rate: {summary.get("win_perc", 0):.2f}%',
                f'Return: {summary.get("return_perc", 0):.2f}%',
            ],
            error=None,
            start_time=self._current_timestamp(),
            end_time=self._current_timestamp(),
            duration=0,
            charts=None
        )

        return result

    def _extract_trades(self, trade_df: pd.DataFrame, symbol: str = 'ASSET') -> List[Trade]:
        """Extract trade list from fast-trade 2.0's enriched dataframe.

        fast-trade emits a single dataframe (returned as both `df` and
        `trade_df`) where the `action` column carries one-letter codes:
          'e' = enter, 'x' = exit, 'h' = hold (no signal).
        Position size is in `aux`, equity in `account_value`/`adj_account_value`.
        We pair each enter with the next subsequent exit to form Trade records.
        """
        trades: List[Trade] = []
        if trade_df.empty or 'action' not in trade_df.columns:
            return trades

        # Walk in time order, keeping at most one open position at a time.
        open_entry = None
        for ts, row in trade_df.iterrows():
            action = row.get('action')
            close = float(row.get('close', 0) or 0)
            if action == 'e' and open_entry is None:
                open_entry = (ts, close, float(row.get('aux', 0) or 0), float(row.get('fee', 0) or 0))
            elif action == 'x' and open_entry is not None:
                entry_ts, entry_price, qty, entry_fee = open_entry
                exit_fee = float(row.get('fee', 0) or 0)
                pnl = (close - entry_price) * qty - (entry_fee + exit_fee)
                pnl_pct = ((close / entry_price) - 1.0) if entry_price else 0.0
                try:
                    holding = (pd.Timestamp(ts) - pd.Timestamp(entry_ts)).days
                except Exception:
                    holding = None
                trades.append(Trade(
                    id=f'trade-{len(trades) + 1}',
                    symbol=symbol,
                    entry_date=str(entry_ts),
                    side='long',
                    quantity=qty,
                    entry_price=entry_price,
                    commission=entry_fee + exit_fee,
                    slippage=0.0,
                    exit_date=str(ts),
                    exit_price=close,
                    pnl=pnl,
                    pnl_percent=pnl_pct,
                    holding_period=holding,
                    exit_reason='signal',
                ))
                open_entry = None
        return trades

    def _extract_equity_curve(self, df: pd.DataFrame) -> List[EquityPoint]:
        """Extract equity curve from fast-trade 2.0 dataframe.

        Equity column is `adj_account_value` (commission-adjusted) preferred
        over `account_value`. Drawdown is computed from the running maximum.
        """
        equity_points: List[EquityPoint] = []
        if df.empty:
            return equity_points

        equity_col = 'adj_account_value' if 'adj_account_value' in df.columns else (
            'account_value' if 'account_value' in df.columns else None
        )
        if equity_col is None:
            return equity_points

        equity_series = df[equity_col].astype(float)
        running_max = equity_series.cummax()
        # Avoid divide-by-zero on the first bar / non-positive equity.
        rm_safe = running_max.where(running_max > 0, 1.0)
        drawdown = (equity_series - running_max) / rm_safe
        returns = equity_series.pct_change().fillna(0.0)

        for date, eq, ret, dd in zip(df.index, equity_series, returns, drawdown):
            equity_points.append(EquityPoint(
                date=str(date),
                equity=float(eq),
                returns=float(ret),
                drawdown=float(dd),
                benchmark=None,
            ))
        return equity_points

    def _calculate_annualized_return(self, total_return: float, days: int) -> float:
        """Calculate annualized return from total return"""
        if days == 0:
            return 0.0
        years = days / 365.25
        return (1 + total_return) ** (1 / years) - 1 if years > 0 else 0.0

    def _calculate_profit_factor(self, summary: Dict[str, Any]) -> float:
        """Calculate profit factor from summary"""
        avg_win = summary.get('avg_win', 0)
        avg_loss = abs(summary.get('avg_loss', 0))
        win_perc = summary.get('win_perc', 0) / 100
        loss_perc = summary.get('loss_perc', 0) / 100

        if loss_perc == 0 or avg_loss == 0:
            return 0.0

        return (avg_win * win_perc) / (avg_loss * loss_perc)

    # ========================================================================
    # Optional Methods (Not Implemented)
    # ========================================================================

    def get_historical_data(self, request: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Get historical data - not implemented"""
        raise NotImplementedError('Historical data fetching not supported by Fast-Trade provider')

    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate indicator - not implemented"""
        raise NotImplementedError('Standalone indicator calculation not supported')

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Optimization - not implemented"""
        raise NotImplementedError('Optimization not natively supported by Fast-Trade')

    def get_strategies(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return strategy catalog in BT shape: {strategies: {category: [...]}}."""
        from ft_strategies import get_strategy_catalog
        catalog = get_strategy_catalog()
        return {'success': True, 'data': {'provider': 'fasttrade', 'strategies': catalog}}

    def get_indicators(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return indicator catalog normalized to {indicators: {Category: [{id, name}]}}."""
        from ft_indicators import get_catalog
        return get_catalog()

    def get_command_options(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return provider-specific option lists. FastTrade only supports backtest."""
        return {
            'success': True,
            'data': {
                'position_sizing_methods': ['percent', 'fixed'],
                'optimize_objectives':     [],
                'optimize_methods':        [],
                'label_types':             [],
                'splitter_types':          [],
                'signal_generators':       [],
                'indicator_signal_modes':  [],
                'returns_analysis_types':  [],
            },
        }


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    """Main entry point for CLI execution"""
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: python fasttrade_provider.py <command> <json_args>'
        }))
        sys.exit(1)

    command = sys.argv[1]
    json_args = sys.argv[2]

    try:
        args = parse_json_input(json_args)
        provider = FastTradeProvider()

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
