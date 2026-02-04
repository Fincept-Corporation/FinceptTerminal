"""
BT Provider - Flexible Portfolio Backtesting

Portfolio-level strategy backtesting using bt library with composable
algo blocks. Supports equal-weight, inverse volatility, mean-variance,
risk parity, momentum selection, and standard technical strategies.

Falls back to pure-numpy simulation when bt is not installed.
"""

import sys
import io
import json
import math
import traceback
from typing import Dict, Any, List, Optional
from pathlib import Path

# Setup paths for both direct execution and package import
_SCRIPT_DIR = Path(__file__).parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent

if str(_BACKTESTING_DIR) not in sys.path:
    sys.path.insert(0, str(_BACKTESTING_DIR))
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from base.base_provider import (
    BacktestingProviderBase,
    json_response,
    parse_json_input,
)


# ============================================================================
# BT Provider
# ============================================================================

class BtProvider(BacktestingProviderBase):
    """
    Portfolio backtesting provider using bt library.

    bt provides composable algo blocks for building portfolio-level
    strategies. Key advantages over single-asset backtesting:
    - Portfolio allocation strategies (equal weight, risk parity, etc.)
    - Composable algo pipelines
    - ffn-based performance statistics
    """

    @property
    def name(self) -> str:
        return 'bt'

    @property
    def version(self) -> str:
        try:
            import bt
            return getattr(bt, '__version__', '1.1.2')
        except ImportError:
            return '1.1.2-fallback'

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': True,
            'walkForward': True,
            'portfolioAllocation': True,
            'multiAsset': ['stocks', 'etfs', 'crypto'],
            'indicators': True,
            'customStrategies': False,
            'algoBlocks': True,
            'riskParity': True,
            'meanVariance': True,
        }

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        return {'success': True, 'message': 'BT provider initialized'}

    def test_connection(self) -> Dict[str, Any]:
        try:
            import bt
            return {'success': True, 'message': f'bt {self.version} available'}
        except ImportError:
            return {
                'success': True,
                'message': 'bt not installed, using numpy fallback',
                'fallback': True,
            }

    # ========================================================================
    # Core: run_backtest
    # ========================================================================

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run a portfolio backtest using bt or numpy fallback."""
        import numpy as np

        try:
            strategy_info = request.get('strategy', {})
            strategy_type = strategy_info.get('type', 'equal_weight')
            strategy_params = strategy_info.get('parameters', {})
            start_date = request.get('startDate', '2020-01-01')
            end_date = request.get('endDate', '2024-01-01')
            initial_capital = float(request.get('initialCapital', 100000))
            commission = float(request.get('commission', 0.001))

            assets = request.get('assets', [])
            symbols = [a.get('symbol', 'SPY') for a in assets] if assets else ['SPY']
            if not symbols:
                symbols = ['SPY']

            print(f'[BT] run_backtest: strategy={strategy_type}, symbols={symbols}', file=sys.stderr)

            # Import strategies
            from bt_strategies import get_strategy

            # Try bt-native execution first
            result = self._try_bt_backtest(
                strategy_type, strategy_params, symbols,
                start_date, end_date, initial_capital, commission
            )
            if result is not None:
                return result

            # Fallback to numpy simulation
            print('[BT] Using numpy fallback simulation', file=sys.stderr)
            return self._numpy_simulation(
                strategy_type, strategy_params, symbols,
                start_date, end_date, initial_capital, commission
            )

        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'traceback': traceback.format_exc(),
            }

    def _try_bt_backtest(self, strategy_type, params, symbols,
                         start_date, end_date, initial_capital, commission):
        """Try to run backtest using actual bt library."""
        try:
            import bt
            import pandas as pd
            from bt_data import fetch_data
            from bt_strategies import get_strategy

            data = fetch_data(symbols, start_date, end_date)
            if data.empty:
                return None

            builder = get_strategy(strategy_type, params)
            strategy = builder(data, name=strategy_type)

            if strategy is None:
                return None

            test = bt.Backtest(strategy, data, initial_capital=initial_capital,
                               commissions=lambda q, p: abs(q) * p * commission)
            result = bt.run(test)

            # Extract stats from ffn
            stats = result.stats
            perf = result[strategy_type]

            # Build equity curve
            equity_series = perf.prices * initial_capital
            returns_series = perf.prices.pct_change().fillna(0)

            # Drawdown
            rolling_max = equity_series.cummax()
            drawdown = (equity_series - rolling_max) / rolling_max

            equity_points = []
            for i, (idx, val) in enumerate(equity_series.items()):
                date_str = idx.strftime('%Y-%m-%d') if hasattr(idx, 'strftime') else str(idx)
                equity_points.append({
                    'date': date_str,
                    'equity': float(val),
                    'returns': float(returns_series.iloc[i]) if i < len(returns_series) else 0.0,
                    'drawdown': float(drawdown.iloc[i]) if i < len(drawdown) else 0.0,
                })

            # Extract performance metrics from stats DataFrame
            def _safe_stat(col, row, default=0.0):
                try:
                    val = stats.loc[row, col]
                    if pd.isna(val) or (isinstance(val, float) and (math.isinf(val) or math.isnan(val))):
                        return default
                    return float(val)
                except (KeyError, IndexError):
                    return default

            col = strategy_type
            total_return = _safe_stat(col, 'total_return')
            annual_return = _safe_stat(col, 'cagr')
            max_dd = _safe_stat(col, 'max_drawdown')
            sharpe = _safe_stat(col, 'daily_sharpe')
            sortino = _safe_stat(col, 'daily_sortino')
            vol = _safe_stat(col, 'daily_vol')
            calmar = _safe_stat(col, 'calmar')

            # Compute win rate from returns
            daily_rets = returns_series.dropna()
            daily_rets = daily_rets[daily_rets != 0]
            total_trading_days = len(daily_rets)
            winning_days = int((daily_rets > 0).sum())
            losing_days = int((daily_rets < 0).sum())
            win_rate = winning_days / total_trading_days if total_trading_days > 0 else 0

            final_equity = float(equity_series.iloc[-1]) if len(equity_series) > 0 else initial_capital

            performance = {
                'totalReturn': total_return,
                'annualizedReturn': annual_return,
                'sharpeRatio': sharpe,
                'sortinoRatio': sortino,
                'maxDrawdown': max_dd,
                'winRate': win_rate,
                'lossRate': 1 - win_rate,
                'profitFactor': abs(float(daily_rets[daily_rets > 0].sum()) / float(daily_rets[daily_rets < 0].sum())) if losing_days > 0 else 0,
                'volatility': vol,
                'calmarRatio': calmar,
                'totalTrades': total_trading_days,
                'winningTrades': winning_days,
                'losingTrades': losing_days,
                'averageWin': float(daily_rets[daily_rets > 0].mean()) if winning_days > 0 else 0,
                'averageLoss': float(daily_rets[daily_rets < 0].mean()) if losing_days > 0 else 0,
                'largestWin': float(daily_rets.max()) if len(daily_rets) > 0 else 0,
                'largestLoss': float(daily_rets.min()) if len(daily_rets) > 0 else 0,
                'averageTradeReturn': float(daily_rets.mean()) if len(daily_rets) > 0 else 0,
                'expectancy': float(daily_rets.mean()) if len(daily_rets) > 0 else 0,
            }

            statistics = {
                'startDate': start_date,
                'endDate': end_date,
                'initialCapital': initial_capital,
                'finalCapital': final_equity,
                'totalFees': 0,
                'totalSlippage': 0,
                'totalTrades': total_trading_days,
                'winningDays': winning_days,
                'losingDays': losing_days,
                'averageDailyReturn': float(daily_rets.mean()) if len(daily_rets) > 0 else 0,
                'bestDay': float(daily_rets.max()) if len(daily_rets) > 0 else 0,
                'worstDay': float(daily_rets.min()) if len(daily_rets) > 0 else 0,
                'consecutiveWins': 0,
                'consecutiveLosses': 0,
            }

            return {
                'success': True,
                'data': {
                    'id': self._generate_id(),
                    'status': 'completed',
                    'performance': performance,
                    'trades': [],
                    'equity': equity_points,
                    'statistics': statistics,
                    'logs': [f'bt backtest completed: {strategy_type}'],
                },
            }

        except ImportError:
            print('[BT] bt library not available', file=sys.stderr)
            return None
        except Exception as e:
            print(f'[BT] bt execution failed: {e}', file=sys.stderr)
            print(traceback.format_exc(), file=sys.stderr)
            return None

    def _numpy_simulation(self, strategy_type, params, symbols,
                          start_date, end_date, initial_capital, commission):
        """Pure-numpy fallback simulation."""
        import numpy as np
        from bt_data import fetch_data

        data = fetch_data(symbols, start_date, end_date)
        if data.empty:
            return {'success': False, 'error': 'No data available'}

        close = data.values
        n = len(close)
        if close.ndim == 1:
            close = close.reshape(-1, 1)

        num_assets = close.shape[1]

        # Generate simple signals based on strategy type
        signals = self._generate_numpy_signals(close, strategy_type, params)

        # Portfolio returns: equal weight across selected assets
        daily_returns = np.diff(close, axis=0) / close[:-1]
        if daily_returns.ndim == 1:
            daily_returns = daily_returns.reshape(-1, 1)

        portfolio_returns = np.zeros(n - 1)
        for i in range(n - 1):
            sig = signals[i] if i < len(signals) else signals[-1]
            if sig.ndim == 0:
                sig = np.array([sig])
            active = sig > 0
            n_active = active.sum()
            if n_active > 0:
                weights = active.astype(float) / n_active
                portfolio_returns[i] = np.nansum(weights * daily_returns[i]) - commission * abs(float(np.nansum(np.diff(sig.reshape(-1)) if i > 0 else 0)))
            else:
                portfolio_returns[i] = 0.0

        # Build equity curve
        equity = np.zeros(n)
        equity[0] = initial_capital
        for i in range(1, n):
            equity[i] = equity[i - 1] * (1 + portfolio_returns[i - 1])

        # Metrics
        total_return = (equity[-1] / equity[0]) - 1.0
        trading_days = 252
        years = max(n / trading_days, 1.0 / trading_days)
        annual_return = (1 + total_return) ** (1 / years) - 1 if years > 0 else 0

        ret_std = np.std(portfolio_returns)
        sharpe = (np.mean(portfolio_returns) * np.sqrt(trading_days)) / ret_std if ret_std > 0 else 0

        neg_returns = portfolio_returns[portfolio_returns < 0]
        downside_std = np.std(neg_returns) if len(neg_returns) > 0 else 1e-8
        sortino = (np.mean(portfolio_returns) * np.sqrt(trading_days)) / downside_std

        rolling_max = np.maximum.accumulate(equity)
        drawdown = (equity - rolling_max) / np.where(rolling_max > 0, rolling_max, 1)
        max_dd = float(np.min(drawdown))

        calmar = annual_return / abs(max_dd) if abs(max_dd) > 1e-10 else 0

        winning = portfolio_returns[portfolio_returns > 0]
        losing = portfolio_returns[portfolio_returns < 0]
        win_rate = len(winning) / len(portfolio_returns) if len(portfolio_returns) > 0 else 0

        profit_factor = abs(winning.sum() / losing.sum()) if len(losing) > 0 and losing.sum() != 0 else 0

        # Build equity points
        dates = data.index
        equity_points = []
        for i in range(n):
            date_str = dates[i].strftime('%Y-%m-%d') if hasattr(dates[i], 'strftime') else str(dates[i])
            equity_points.append({
                'date': date_str,
                'equity': float(equity[i]),
                'returns': float(portfolio_returns[i - 1]) if i > 0 else 0.0,
                'drawdown': float(drawdown[i]),
            })

        performance = {
            'totalReturn': float(total_return),
            'annualizedReturn': float(annual_return),
            'sharpeRatio': float(sharpe),
            'sortinoRatio': float(sortino),
            'maxDrawdown': float(max_dd),
            'winRate': float(win_rate),
            'lossRate': float(1 - win_rate),
            'profitFactor': float(profit_factor),
            'volatility': float(ret_std * np.sqrt(trading_days)),
            'calmarRatio': float(calmar),
            'totalTrades': int(len(portfolio_returns)),
            'winningTrades': int(len(winning)),
            'losingTrades': int(len(losing)),
            'averageWin': float(winning.mean()) if len(winning) > 0 else 0,
            'averageLoss': float(losing.mean()) if len(losing) > 0 else 0,
            'largestWin': float(winning.max()) if len(winning) > 0 else 0,
            'largestLoss': float(losing.min()) if len(losing) > 0 else 0,
            'averageTradeReturn': float(portfolio_returns.mean()) if len(portfolio_returns) > 0 else 0,
            'expectancy': float(portfolio_returns.mean()) if len(portfolio_returns) > 0 else 0,
        }

        statistics = {
            'startDate': start_date,
            'endDate': end_date,
            'initialCapital': initial_capital,
            'finalCapital': float(equity[-1]),
            'totalFees': 0,
            'totalSlippage': 0,
            'totalTrades': int(len(portfolio_returns)),
            'winningDays': int(len(winning)),
            'losingDays': int(len(losing)),
            'averageDailyReturn': float(portfolio_returns.mean()),
            'bestDay': float(portfolio_returns.max()) if len(portfolio_returns) > 0 else 0,
            'worstDay': float(portfolio_returns.min()) if len(portfolio_returns) > 0 else 0,
            'consecutiveWins': 0,
            'consecutiveLosses': 0,
        }

        return {
            'success': True,
            'data': {
                'id': self._generate_id(),
                'status': 'completed',
                'performance': performance,
                'trades': [],
                'equity': equity_points,
                'statistics': statistics,
                'logs': [f'numpy fallback simulation: {strategy_type}'],
            },
        }

    def _generate_numpy_signals(self, close, strategy_type, params):
        """Generate trading signals using numpy for fallback simulation."""
        import numpy as np
        from bt_strategies import (
            _rolling_mean, _ema, _rsi, _macd_calc,
            _zscore_calc, _bollinger_bands, _momentum_calc,
        )

        n = close.shape[0]
        num_assets = close.shape[1] if close.ndim > 1 else 1

        if close.ndim == 1:
            close_2d = close.reshape(-1, 1)
        else:
            close_2d = close

        signals = np.ones((n, num_assets))

        if strategy_type in ('equal_weight', 'inv_vol', 'mean_var',
                             'risk_parity', 'target_vol', 'min_var'):
            # Portfolio strategies: always invested
            signals = np.ones((n, num_assets))

            if strategy_type == 'inv_vol' and num_assets > 1:
                lookback = int(params.get('lookback', 20))
                for i in range(lookback, n):
                    window = close_2d[i - lookback:i]
                    rets = np.diff(window, axis=0) / window[:-1]
                    vols = np.std(rets, axis=0)
                    inv_vols = np.where(vols > 0, 1.0 / vols, 0.0)
                    total = inv_vols.sum()
                    if total > 0:
                        signals[i] = inv_vols / total
                    else:
                        signals[i] = 1.0 / num_assets

        elif strategy_type == 'sma_crossover':
            fast = int(params.get('fastPeriod', 10))
            slow = int(params.get('slowPeriod', 20))
            for col in range(num_assets):
                fast_ma = _rolling_mean(close_2d[:, col], fast)
                slow_ma = _rolling_mean(close_2d[:, col], slow)
                signals[:, col] = np.where(fast_ma > slow_ma, 1, 0)

        elif strategy_type == 'ema_crossover':
            fast = int(params.get('fastPeriod', 10))
            slow = int(params.get('slowPeriod', 20))
            for col in range(num_assets):
                fast_ema = _ema(close_2d[:, col], fast)
                slow_ema = _ema(close_2d[:, col], slow)
                signals[:, col] = np.where(fast_ema > slow_ema, 1, 0)

        elif strategy_type == 'macd':
            fast = int(params.get('fastPeriod', 12))
            slow = int(params.get('slowPeriod', 26))
            sig = int(params.get('signalPeriod', 9))
            for col in range(num_assets):
                macd_line, signal_line, _ = _macd_calc(close_2d[:, col], fast, slow, sig)
                signals[:, col] = np.where(macd_line > signal_line, 1, 0)

        elif strategy_type == 'mean_reversion':
            window = int(params.get('window', 20))
            threshold = float(params.get('threshold', 2.0))
            for col in range(num_assets):
                z = _zscore_calc(close_2d[:, col], window)
                signals[:, col] = np.where(z < -threshold, 1, 0)

        elif strategy_type == 'bollinger_bands':
            period = int(params.get('period', 20))
            std_dev = float(params.get('stdDev', 2.0))
            for col in range(num_assets):
                upper, middle, lower = _bollinger_bands(close_2d[:, col], period, std_dev)
                signals[:, col] = np.where(close_2d[:, col] < lower, 1, 0)

        elif strategy_type == 'rsi':
            period = int(params.get('period', 14))
            oversold = float(params.get('oversold', 30))
            for col in range(num_assets):
                rsi_vals = _rsi(close_2d[:, col], period)
                signals[:, col] = np.where(rsi_vals < oversold, 1, 0)

        elif strategy_type in ('momentum', 'momentum_topn', 'momentum_inv_vol'):
            period = int(params.get('period', params.get('lookback', 12)))
            for col in range(num_assets):
                mom = _momentum_calc(close_2d[:, col], period)
                signals[:, col] = np.where(mom > 0, 1, 0)

        elif strategy_type == 'breakout':
            period = int(params.get('period', 20))
            for col in range(num_assets):
                rolling_high = np.full(n, np.nan)
                for i in range(period - 1, n):
                    rolling_high[i] = np.max(close_2d[i - period + 1:i + 1, col])
                signals[:, col] = np.where(close_2d[:, col] >= rolling_high, 1, 0)

        elif strategy_type == 'dual_momentum':
            abs_period = int(params.get('absolutePeriod', 12))
            for col in range(num_assets):
                mom = _momentum_calc(close_2d[:, col], abs_period)
                signals[:, col] = np.where(mom > 0, 1, 0)

        return signals

    # ========================================================================
    # Optimize
    # ========================================================================

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Grid or random search optimization."""
        import numpy as np

        try:
            strategy_info = request.get('strategy', {})
            strategy_type = strategy_info.get('type', 'sma_crossover')
            start_date = request.get('startDate', '2020-01-01')
            end_date = request.get('endDate', '2024-01-01')
            initial_capital = float(request.get('initialCapital', 100000))
            objective = request.get('objective', 'sharpe')
            method = request.get('method', 'grid')
            max_iter = int(request.get('maxIterations', 100))
            param_ranges = request.get('parameters', {})

            assets = request.get('assets', [])
            symbols = [a.get('symbol', 'SPY') for a in assets] if assets else ['SPY']

            combos = self._generate_param_combos(param_ranges, method, max_iter)

            best_score = -np.inf
            best_params = {}
            best_result = None
            all_results = []

            for i, combo in enumerate(combos):
                bt_request = {
                    'strategy': {'type': strategy_type, 'parameters': combo},
                    'startDate': start_date,
                    'endDate': end_date,
                    'initialCapital': initial_capital,
                    'commission': 0.001,
                    'assets': [{'symbol': s} for s in symbols],
                }

                result = self.run_backtest(bt_request)

                score = self._get_objective_score(result, objective)
                all_results.append({
                    'parameters': combo,
                    'score': score,
                    'iteration': i,
                })

                if score > best_score:
                    best_score = score
                    best_params = combo
                    best_result = result

            return {
                'success': True,
                'data': {
                    'id': self._generate_id(),
                    'status': 'completed',
                    'bestParameters': best_params,
                    'bestScore': float(best_score),
                    'bestResult': best_result.get('data', {}) if best_result else {},
                    'allResults': all_results[:50],
                    'iterations': len(combos),
                },
            }

        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': traceback.format_exc()}

    # ========================================================================
    # Walk Forward
    # ========================================================================

    def walk_forward(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Walk-forward analysis with train/test splits."""
        import numpy as np
        import pandas as pd

        try:
            strategy_info = request.get('strategy', {})
            strategy_type = strategy_info.get('type', 'sma_crossover')
            strategy_params = strategy_info.get('parameters', {})
            start_date = request.get('startDate', '2020-01-01')
            end_date = request.get('endDate', '2024-01-01')
            initial_capital = float(request.get('initialCapital', 100000))
            n_splits = int(request.get('nSplits', 5))
            train_ratio = float(request.get('trainRatio', 0.7))

            assets = request.get('assets', [])
            symbols = [a.get('symbol', 'SPY') for a in assets] if assets else ['SPY']

            from bt_data import fetch_data
            data = fetch_data(symbols, start_date, end_date)
            if data.empty:
                return {'success': False, 'error': 'No data available'}

            n = len(data)
            split_size = n // n_splits
            results = []

            for i in range(n_splits):
                split_start = i * split_size
                split_end = min((i + 1) * split_size, n)
                train_end = split_start + int((split_end - split_start) * train_ratio)

                train_start_date = data.index[split_start].strftime('%Y-%m-%d')
                train_end_date = data.index[min(train_end, n - 1)].strftime('%Y-%m-%d')
                test_start_date = data.index[min(train_end, n - 1)].strftime('%Y-%m-%d')
                test_end_date = data.index[min(split_end - 1, n - 1)].strftime('%Y-%m-%d')

                test_request = {
                    'strategy': {'type': strategy_type, 'parameters': strategy_params},
                    'startDate': test_start_date,
                    'endDate': test_end_date,
                    'initialCapital': initial_capital,
                    'commission': 0.001,
                    'assets': [{'symbol': s} for s in symbols],
                }

                result = self.run_backtest(test_request)
                perf = result.get('data', {}).get('performance', {})

                results.append({
                    'split': i + 1,
                    'trainStart': train_start_date,
                    'trainEnd': train_end_date,
                    'testStart': test_start_date,
                    'testEnd': test_end_date,
                    'totalReturn': perf.get('totalReturn', 0),
                    'sharpeRatio': perf.get('sharpeRatio', 0),
                    'maxDrawdown': perf.get('maxDrawdown', 0),
                })

            avg_return = np.mean([r['totalReturn'] for r in results])
            avg_sharpe = np.mean([r['sharpeRatio'] for r in results])

            return {
                'success': True,
                'data': {
                    'id': self._generate_id(),
                    'status': 'completed',
                    'splits': results,
                    'summary': {
                        'averageReturn': float(avg_return),
                        'averageSharpe': float(avg_sharpe),
                        'nSplits': n_splits,
                    },
                },
            }

        except Exception as e:
            return {'success': False, 'error': str(e), 'traceback': traceback.format_exc()}

    # ========================================================================
    # Data
    # ========================================================================

    def get_historical_data(self, request: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Fetch historical market data."""
        from bt_data import fetch_data, data_to_records

        symbols = request.get('symbols', ['SPY'])
        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]
        start_date = request.get('startDate', '2020-01-01')
        end_date = request.get('endDate', '2024-01-01')

        data = fetch_data(symbols, start_date, end_date)
        return data_to_records(data)

    # ========================================================================
    # Indicators
    # ========================================================================

    def calculate_indicator(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate a technical indicator."""
        import numpy as np
        from bt_data import fetch_data, fetch_ohlcv
        from bt_strategies import (
            _rolling_mean, _rolling_std, _ema, _rsi, _macd_calc,
            _bollinger_bands, _atr, _adx_calc, _momentum_calc,
            _stochastic_calc, _williams_r_calc, _cci_calc, _obv_calc,
            _donchian_calc, _keltner_calc, _zscore_calc, _vwap_calc,
        )

        indicator = request.get('indicator', 'ma')
        params = request.get('parameters', {})
        symbols = request.get('symbols', ['SPY'])
        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]
        start_date = request.get('startDate', '2020-01-01')
        end_date = request.get('endDate', '2024-01-01')

        # Fetch data
        needs_ohlcv = indicator in ('atr', 'adx', 'stoch', 'williams_r', 'cci', 'obv', 'vwap', 'keltner', 'donchian')

        all_results = {}

        if needs_ohlcv:
            ohlcv_data = fetch_ohlcv(symbols, start_date, end_date)
            for sym, df in ohlcv_data.items():
                h = df['high'].values
                l = df['low'].values
                c = df['close'].values
                v = df['volume'].values if 'volume' in df.columns else np.ones(len(c))
                dates = [d.strftime('%Y-%m-%d') if hasattr(d, 'strftime') else str(d) for d in df.index]

                if indicator == 'atr':
                    vals = _atr(h, l, c, int(params.get('period', 14)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'adx':
                    adx, pdi, mdi = _adx_calc(h, l, c, int(params.get('period', 14)))
                    all_results[sym] = [{'date': d, 'adx': self._safe_float(a), 'plusDi': self._safe_float(p), 'minusDi': self._safe_float(m)} for d, a, p, m in zip(dates, adx, pdi, mdi)]
                elif indicator == 'stoch':
                    k, d_vals = _stochastic_calc(h, l, c, int(params.get('kPeriod', 14)), int(params.get('dPeriod', 3)))
                    all_results[sym] = [{'date': d, 'k': self._safe_float(kv), 'd': self._safe_float(dv)} for d, kv, dv in zip(dates, k, d_vals)]
                elif indicator == 'williams_r':
                    wr = _williams_r_calc(h, l, c, int(params.get('period', 14)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, wr)]
                elif indicator == 'cci':
                    vals = _cci_calc(h, l, c, int(params.get('period', 20)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'obv':
                    vals = _obv_calc(c, v)
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'vwap':
                    vals = _vwap_calc(h, l, c, v)
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'keltner':
                    upper, middle, lower = _keltner_calc(h, l, c, int(params.get('period', 20)), float(params.get('atrMult', 2.0)))
                    all_results[sym] = [{'date': d, 'upper': self._safe_float(u), 'middle': self._safe_float(m), 'lower': self._safe_float(lo)} for d, u, m, lo in zip(dates, upper, middle, lower)]
                elif indicator == 'donchian':
                    upper, lower = _donchian_calc(h, l, int(params.get('period', 20)))
                    all_results[sym] = [{'date': d, 'upper': self._safe_float(u), 'lower': self._safe_float(lo)} for d, u, lo in zip(dates, upper, lower)]
        else:
            data = fetch_data(symbols, start_date, end_date)
            for sym in symbols:
                if sym not in data.columns:
                    continue
                c = data[sym].values
                dates = [d.strftime('%Y-%m-%d') if hasattr(d, 'strftime') else str(d) for d in data.index]

                if indicator == 'ma':
                    period = int(params.get('period', 20))
                    ewm = params.get('ewm', False)
                    vals = _ema(c, period) if ewm else _rolling_mean(c, period)
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'ema':
                    vals = _ema(c, int(params.get('period', 20)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'mstd':
                    vals = _rolling_std(c, int(params.get('period', 20)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'bbands':
                    upper, middle, lower = _bollinger_bands(c, int(params.get('period', 20)), float(params.get('alpha', 2.0)))
                    all_results[sym] = [{'date': d, 'upper': self._safe_float(u), 'middle': self._safe_float(m), 'lower': self._safe_float(lo)} for d, u, m, lo in zip(dates, upper, middle, lower)]
                elif indicator == 'rsi':
                    vals = _rsi(c, int(params.get('period', 14)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'macd':
                    macd_line, signal_line, histogram = _macd_calc(c, int(params.get('fast', 12)), int(params.get('slow', 26)), int(params.get('signal', 9)))
                    all_results[sym] = [{'date': d, 'macd': self._safe_float(m), 'signal': self._safe_float(s), 'histogram': self._safe_float(h)} for d, m, s, h in zip(dates, macd_line, signal_line, histogram)]
                elif indicator == 'zscore':
                    vals = _zscore_calc(c, int(params.get('window', 20)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                elif indicator == 'momentum':
                    vals = _momentum_calc(c, int(params.get('period', 12)))
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, vals)]
                else:
                    all_results[sym] = [{'date': d, 'value': self._safe_float(v)} for d, v in zip(dates, c)]

        return {
            'success': True,
            'data': {
                'indicator': indicator,
                'results': all_results,
            },
        }

    # ========================================================================
    # Indicator Signals
    # ========================================================================

    def indicator_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate trading signals from indicator values."""
        ind_result = self.calculate_indicator(request)
        if not ind_result.get('success'):
            return ind_result

        results = ind_result.get('data', {}).get('results', {})
        signals = {}

        for sym, values in results.items():
            sig = []
            prev_val = None
            for point in values:
                val = point.get('value', point.get('macd', point.get('k', 0)))
                if val is None:
                    sig.append({'date': point['date'], 'signal': 0})
                    continue

                signal = 0
                if prev_val is not None:
                    if val > prev_val:
                        signal = 1
                    elif val < prev_val:
                        signal = -1

                sig.append({'date': point['date'], 'signal': signal, 'value': val})
                prev_val = val

            signals[sym] = sig

        return {
            'success': True,
            'data': {
                'indicator': request.get('indicator', 'ma'),
                'signals': signals,
            },
        }

    # ========================================================================
    # Signal Generation
    # ========================================================================

    def generate_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate trading signals."""
        import numpy as np
        from bt_data import fetch_data

        symbols = request.get('symbols', ['SPY'])
        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]
        start_date = request.get('startDate', '2020-01-01')
        end_date = request.get('endDate', '2024-01-01')
        generator = request.get('generator', 'RAND')
        params = request.get('parameters', {})

        data = fetch_data(symbols, start_date, end_date)
        n = len(data)
        dates = [d.strftime('%Y-%m-%d') if hasattr(d, 'strftime') else str(d) for d in data.index]

        seed = int(params.get('seed', 42))
        np.random.seed(seed)

        signals = {}
        for sym in symbols:
            if generator == 'RAND':
                n_signals = min(int(params.get('n', 20)), n)
                sig_arr = np.zeros(n)
                indices = np.random.choice(n, n_signals, replace=False)
                sig_arr[indices] = np.random.choice([1, -1], n_signals)
            elif generator == 'RPROB':
                prob = float(params.get('prob', 0.05))
                sig_arr = np.where(np.random.random(n) < prob, 1, 0)
            else:
                sig_arr = np.zeros(n)
                sig_arr[np.random.random(n) < 0.05] = 1

            signals[sym] = [{'date': d, 'signal': int(s)} for d, s in zip(dates, sig_arr)]

        return {
            'success': True,
            'data': {'signals': signals, 'generator': generator},
        }

    # ========================================================================
    # Label Generation
    # ========================================================================

    def generate_labels(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate ML labels for supervised learning."""
        import numpy as np
        from bt_data import fetch_data

        symbols = request.get('symbols', ['SPY'])
        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]
        start_date = request.get('startDate', '2020-01-01')
        end_date = request.get('endDate', '2024-01-01')
        generator = request.get('generator', 'FIXLB')
        params = request.get('parameters', {})

        data = fetch_data(symbols, start_date, end_date)
        dates = [d.strftime('%Y-%m-%d') if hasattr(d, 'strftime') else str(d) for d in data.index]

        labels = {}
        for sym in symbols:
            if sym not in data.columns:
                continue
            close = data[sym].values
            n = len(close)

            if generator == 'FIXLB':
                horizon = int(params.get('horizon', 5))
                threshold = float(params.get('threshold', 0.01))
                lbl = np.zeros(n)
                for i in range(n - horizon):
                    ret = (close[i + horizon] - close[i]) / close[i]
                    if ret > threshold:
                        lbl[i] = 1
                    elif ret < -threshold:
                        lbl[i] = -1
            elif generator == 'TRENDLB':
                window = int(params.get('window', 20))
                threshold = float(params.get('threshold', 0.0))
                from bt_strategies import _rolling_mean
                ma = _rolling_mean(close, window)
                lbl = np.where(close > ma, 1, np.where(close < ma, -1, 0))
            elif generator == 'MEANLB':
                window = int(params.get('window', 20))
                threshold = float(params.get('threshold', 1.0))
                from bt_strategies import _zscore_calc
                z = _zscore_calc(close, window)
                lbl = np.where(z > threshold, -1, np.where(z < -threshold, 1, 0))
            else:
                lbl = np.zeros(n)

            labels[sym] = [{'date': d, 'label': int(l)} for d, l in zip(dates, lbl)]

        return {
            'success': True,
            'data': {'labels': labels, 'generator': generator},
        }

    # ========================================================================
    # Splits
    # ========================================================================

    def generate_splits(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate cross-validation splits."""
        import pandas as pd

        start_date = request.get('startDate', '2020-01-01')
        end_date = request.get('endDate', '2024-01-01')
        splitter = request.get('splitter', 'rolling')
        params = request.get('parameters', {})

        dates = pd.bdate_range(start=start_date, end=end_date)
        n = len(dates)

        splits = []

        if splitter == 'rolling':
            window_len = int(params.get('windowLen', 252))
            test_len = int(params.get('testLen', 63))
            step = int(params.get('step', 63))
            i = 0
            split_id = 1
            while i + window_len + test_len <= n:
                train_start = dates[i].strftime('%Y-%m-%d')
                train_end = dates[i + window_len - 1].strftime('%Y-%m-%d')
                test_start = dates[i + window_len].strftime('%Y-%m-%d')
                test_end = dates[min(i + window_len + test_len - 1, n - 1)].strftime('%Y-%m-%d')
                splits.append({
                    'id': split_id,
                    'trainStart': train_start, 'trainEnd': train_end,
                    'testStart': test_start, 'testEnd': test_end,
                })
                i += step
                split_id += 1

        elif splitter == 'expanding':
            min_len = int(params.get('minLen', 126))
            test_len = int(params.get('testLen', 63))
            step = int(params.get('step', 63))
            i = min_len
            split_id = 1
            while i + test_len <= n:
                train_start = dates[0].strftime('%Y-%m-%d')
                train_end = dates[i - 1].strftime('%Y-%m-%d')
                test_start = dates[i].strftime('%Y-%m-%d')
                test_end = dates[min(i + test_len - 1, n - 1)].strftime('%Y-%m-%d')
                splits.append({
                    'id': split_id,
                    'trainStart': train_start, 'trainEnd': train_end,
                    'testStart': test_start, 'testEnd': test_end,
                })
                i += step
                split_id += 1

        elif splitter == 'purged_kfold':
            n_splits = int(params.get('nSplits', 5))
            fold_size = n // n_splits
            for i in range(n_splits):
                test_start_idx = i * fold_size
                test_end_idx = min((i + 1) * fold_size - 1, n - 1)
                splits.append({
                    'id': i + 1,
                    'trainStart': dates[0].strftime('%Y-%m-%d'),
                    'trainEnd': dates[max(test_start_idx - 1, 0)].strftime('%Y-%m-%d'),
                    'testStart': dates[test_start_idx].strftime('%Y-%m-%d'),
                    'testEnd': dates[test_end_idx].strftime('%Y-%m-%d'),
                })

        else:  # range
            mid = n // 2
            splits.append({
                'id': 1,
                'trainStart': dates[0].strftime('%Y-%m-%d'),
                'trainEnd': dates[mid - 1].strftime('%Y-%m-%d'),
                'testStart': dates[mid].strftime('%Y-%m-%d'),
                'testEnd': dates[-1].strftime('%Y-%m-%d'),
            })

        return {
            'success': True,
            'data': {
                'splits': splits,
                'splitter': splitter,
                'totalDays': n,
            },
        }

    # ========================================================================
    # Returns Analysis
    # ========================================================================

    def analyze_returns(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze returns of assets."""
        import numpy as np
        from bt_data import fetch_data

        symbols = request.get('symbols', ['SPY'])
        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]
        start_date = request.get('startDate', '2020-01-01')
        end_date = request.get('endDate', '2024-01-01')

        data = fetch_data(symbols, start_date, end_date)
        results = {}

        for sym in symbols:
            if sym not in data.columns:
                continue
            close = data[sym].values
            returns = np.diff(close) / close[:-1]

            total_return = (close[-1] / close[0]) - 1 if len(close) > 1 else 0
            ann_return = (1 + total_return) ** (252 / max(len(returns), 1)) - 1
            vol = float(np.std(returns) * np.sqrt(252))
            sharpe = float(np.mean(returns) * np.sqrt(252) / np.std(returns)) if np.std(returns) > 0 else 0

            cum = np.cumprod(1 + returns)
            rolling_max = np.maximum.accumulate(cum)
            dd = (cum - rolling_max) / np.where(rolling_max > 0, rolling_max, 1)
            max_dd = float(np.min(dd))

            neg_returns = returns[returns < 0]
            downside = float(np.std(neg_returns) * np.sqrt(252)) if len(neg_returns) > 0 else 1e-8
            sortino = float(np.mean(returns) * np.sqrt(252) / (downside if downside > 0 else 1e-8))

            results[sym] = {
                'totalReturn': float(total_return),
                'annualizedReturn': float(ann_return),
                'volatility': vol,
                'sharpeRatio': sharpe,
                'sortinoRatio': sortino,
                'maxDrawdown': max_dd,
                'skewness': float(self._safe_stat_val(np.mean(((returns - np.mean(returns)) / np.std(returns)) ** 3) if np.std(returns) > 0 else 0)),
                'kurtosis': float(self._safe_stat_val(np.mean(((returns - np.mean(returns)) / np.std(returns)) ** 4) - 3 if np.std(returns) > 0 else 0)),
                'positiveDays': int((returns > 0).sum()),
                'negativeDays': int((returns < 0).sum()),
                'bestDay': float(returns.max()) if len(returns) > 0 else 0,
                'worstDay': float(returns.min()) if len(returns) > 0 else 0,
            }

        return {
            'success': True,
            'data': {
                'analysis': results,
                'symbols': symbols,
            },
        }

    # ========================================================================
    # Browse Strategies / Indicators
    # ========================================================================

    def get_strategies(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return strategy catalog."""
        from bt_strategies import get_strategy_catalog
        catalog = get_strategy_catalog()
        return {
            'success': True,
            'data': {
                'provider': 'bt',
                'strategies': catalog,
            },
        }

    def get_indicators(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return indicator catalog."""
        from bt_strategies import INDICATOR_CATALOG
        return {
            'success': True,
            'data': {
                'provider': 'bt',
                'indicators': INDICATOR_CATALOG,
            },
        }

    # ========================================================================
    # Labels to Signals
    # ========================================================================

    def labels_to_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Convert ML labels to entry/exit trading signals."""
        label_result = self.generate_labels(request)
        if not label_result.get('success'):
            return label_result

        labels_data = label_result.get('data', {}).get('labels', {})
        entry_label = int(request.get('entryLabel', 1))
        exit_label = int(request.get('exitLabel', -1))

        signals = {}
        for sym, label_list in labels_data.items():
            sig = []
            in_position = False
            for point in label_list:
                lbl = point['label']
                if not in_position and lbl == entry_label:
                    sig.append({'date': point['date'], 'signal': 1, 'action': 'entry'})
                    in_position = True
                elif in_position and lbl == exit_label:
                    sig.append({'date': point['date'], 'signal': -1, 'action': 'exit'})
                    in_position = False
                else:
                    sig.append({'date': point['date'], 'signal': 0, 'action': 'hold'})
            signals[sym] = sig

        return {
            'success': True,
            'data': {'signals': signals},
        }

    # ========================================================================
    # Helpers
    # ========================================================================

    def _safe_float(self, val):
        """Convert value to JSON-safe float."""
        if val is None:
            return None
        try:
            f = float(val)
            if math.isnan(f) or math.isinf(f):
                return None
            return f
        except (TypeError, ValueError):
            return None

    def _safe_stat_val(self, val):
        """Ensure stat value is safe for JSON."""
        if val is None or (isinstance(val, float) and (math.isnan(val) or math.isinf(val))):
            return 0.0
        return float(val)

    def _generate_param_combos(self, param_ranges, method, max_iter):
        """Generate parameter combinations for optimization."""
        import numpy as np
        import itertools

        if not param_ranges:
            return [{}]

        if method == 'grid':
            param_lists = {}
            for name, spec in param_ranges.items():
                if isinstance(spec, dict):
                    lo = float(spec.get('min', 0))
                    hi = float(spec.get('max', 100))
                    step = float(spec.get('step', 1))
                    param_lists[name] = list(np.arange(lo, hi + step / 2, step))
                elif isinstance(spec, list):
                    param_lists[name] = spec
                else:
                    param_lists[name] = [spec]

            names = list(param_lists.keys())
            values = list(param_lists.values())
            combos = [dict(zip(names, combo)) for combo in itertools.product(*values)]
            return combos[:max_iter]

        else:  # random
            combos = []
            for _ in range(max_iter):
                combo = {}
                for name, spec in param_ranges.items():
                    if isinstance(spec, dict):
                        lo = float(spec.get('min', 0))
                        hi = float(spec.get('max', 100))
                        combo[name] = float(np.random.uniform(lo, hi))
                    else:
                        combo[name] = spec
                combos.append(combo)
            return combos

    def _get_objective_score(self, result, objective):
        """Extract optimization objective score from backtest result."""
        if not result or not result.get('success'):
            return -1e10
        perf = result.get('data', {}).get('performance', {})
        if objective == 'sharpe':
            return perf.get('sharpeRatio', 0)
        elif objective == 'sortino':
            return perf.get('sortinoRatio', 0)
        elif objective == 'calmar':
            return perf.get('calmarRatio', 0)
        elif objective == 'return':
            return perf.get('totalReturn', 0)
        return perf.get('sharpeRatio', 0)


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: bt_provider.py <command> <json_args | --stdin>'
        }))
        sys.exit(1)

    command = sys.argv[1]

    # Support --stdin mode for large payloads
    if sys.argv[2] == '--stdin':
        json_args = sys.stdin.read()
    else:
        json_args = sys.argv[2]

    # Redirect stdout to buffer during execution to prevent stray prints
    real_stdout = sys.stdout
    captured = io.StringIO()

    try:
        sys.stdout = captured
        print(f'[BT-MAIN] command={command}, args_len={len(json_args)}', file=sys.stderr)
        args = parse_json_input(json_args)
        print(f'[BT-MAIN] parsed args keys: {sorted(args.keys()) if isinstance(args, dict) else type(args)}', file=sys.stderr)
        provider = BtProvider()

        if command == 'test_import':
            result_str = json_response({'success': True, 'version': provider.version})
        elif command == 'test_connection':
            result = provider.test_connection()
            result_str = json_response(result)
        elif command == 'initialize':
            result = provider.initialize(args)
            result_str = json_response(result)
        elif command == 'run_backtest':
            print(f'[BT-MAIN] Calling run_backtest...', file=sys.stderr)
            result = provider.run_backtest(args)
            print(f'[BT-MAIN] run_backtest returned, success={result.get("success")}', file=sys.stderr)
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
        elif command == 'get_historical_data':
            result = provider.get_historical_data(args)
            result_str = json_response({'success': True, 'data': result})
        elif command == 'calculate_indicator':
            result = provider.calculate_indicator(args)
            result_str = json_response(result)
        elif command == 'indicator_signals':
            result = provider.indicator_signals(args)
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
        elif command == 'analyze_returns':
            result = provider.analyze_returns(args)
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
        stray_output = captured.getvalue()
        if stray_output:
            print(stray_output, file=sys.stderr)
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': traceback.format_exc()
        }))


if __name__ == '__main__':
    main()
