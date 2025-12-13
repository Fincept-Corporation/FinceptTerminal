"""
VectorBT Provider Implementation

Ultra-fast vectorized backtesting using VectorBT.
"""

import sys
import json
from typing import Dict, Any
from pathlib import Path

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


class VectorBTProvider(BacktestingProviderBase):
    """
    VectorBT Provider

    Executes ultra-fast vectorized backtests using VectorBT library.
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
        except:
            return "0.25.0"

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': True,
            'liveTrading': False,
            'research': True,
            'multiAsset': ['equity', 'crypto', 'forex'],
            'indicators': True,
            'customStrategies': True,
            'maxConcurrentBacktests': 10,
            'vectorization': True,
        }

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize VectorBT provider"""
        try:
            # Check if vectorbt is installed
            import vectorbt as vbt
            import pandas as pd
            import numpy as np

            self.initialized = True
            self.connected = True
            self.config = config

            self._log('VectorBT provider initialized successfully')
            return self._create_success_result(f'VectorBT {vbt.__version__} initialized')

        except ImportError as e:
            self._error('VectorBT not installed', e)
            return self._create_error_result('VectorBT not installed. Run: pip install vectorbt')
        except Exception as e:
            self._error('Initialization failed', e)
            return self._create_error_result(str(e))

    def test_connection(self) -> Dict[str, Any]:
        """Test VectorBT availability"""
        try:
            import vectorbt as vbt
            return self._create_success_result(f'VectorBT {vbt.__version__} is available')
        except ImportError:
            return self._create_error_result('VectorBT not installed')

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run backtest using VectorBT"""
        self._ensure_connected()

        backtest_id = self._generate_id()
        logs = []

        try:
            import vectorbt as vbt
            import pandas as pd
            import numpy as np
            import yfinance as yf

            logs.append(f'{self._current_timestamp()}: Starting VectorBT backtest {backtest_id}')

            # Extract request parameters
            strategy = request.get('strategy', {})
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 100000)
            assets = request.get('assets', [])

            # Download data using yfinance
            symbols = [asset['symbol'] for asset in assets]
            logs.append(f'{self._current_timestamp()}: Downloading data for {symbols}')

            data = yf.download(symbols, start=start_date, end=end_date)['Close']

            if data.empty:
                raise ValueError('No data downloaded')

            logs.append(f'{self._current_timestamp()}: Data downloaded: {len(data)} rows')

            # Execute strategy code
            # For VectorBT, we expect vectorized operations
            strategy_code = self._extract_strategy_code(strategy)

            # Create execution context
            context = {
                'vbt': vbt,
                'pd': pd,
                'np': np,
                'data': data,
                'initial_capital': initial_capital,
            }

            # Execute strategy
            exec(strategy_code, context)

            # Get portfolio from context (strategy should create 'portfolio')
            portfolio = context.get('portfolio')

            if portfolio is None:
                raise ValueError('Strategy must create a "portfolio" variable')

            logs.append(f'{self._current_timestamp()}: Backtest completed')

            # Extract results
            total_return = portfolio.total_return
            stats = portfolio.stats()

            # Build result
            performance = PerformanceMetrics(
                total_return=total_return,
                annualized_return=stats.get('Annualized Return [%]', 0) / 100,
                sharpe_ratio=stats.get('Sharpe Ratio', 0),
                sortino_ratio=stats.get('Sortino Ratio', 0),
                max_drawdown=abs(stats.get('Max Drawdown [%]', 0)) / 100,
                win_rate=stats.get('Win Rate [%]', 0) / 100,
                loss_rate=1 - (stats.get('Win Rate [%]', 0) / 100),
                profit_factor=stats.get('Profit Factor', 0),
                volatility=stats.get('Annualized Volatility [%]', 0) / 100,
                calmar_ratio=stats.get('Calmar Ratio', 0),
                total_trades=int(stats.get('Total Trades', 0)),
                winning_trades=0,  # Calculate from trades
                losing_trades=0,
                average_win=0,
                average_loss=0,
                largest_win=0,
                largest_loss=0,
                average_trade_return=0,
                expectancy=0,
            )

            statistics = BacktestStatistics(
                start_date=start_date,
                end_date=end_date,
                initial_capital=initial_capital,
                final_capital=portfolio.final_value,
                total_fees=0,
                total_slippage=0,
                total_trades=int(stats.get('Total Trades', 0)),
                winning_days=0,
                losing_days=0,
                average_daily_return=0,
                best_day=0,
                worst_day=0,
                consecutive_wins=0,
                consecutive_losses=0,
            )

            # Parse trades from portfolio
            trades = self._parse_trades(portfolio)

            # Equity curve with proper returns and drawdown
            equity = self._build_equity_curve(portfolio, initial_capital)

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

            return result.to_dict()

        except Exception as e:
            self._error(f'Backtest {backtest_id} failed', e)
            logs.append(f'{self._current_timestamp()}: Error: {str(e)}')

            return BacktestResult(
                id=backtest_id,
                status='failed',
                performance=self._get_empty_metrics(),
                trades=[],
                equity=[],
                statistics=self._get_empty_statistics(),
                logs=logs,
                error=str(e)
            ).to_dict()

    def get_historical_data(self, request: Dict[str, Any]) -> list:
        """Get historical data using yfinance"""
        try:
            import yfinance as yf
            import pandas as pd

            symbols = request.get('symbols', [])
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            timeframe = request.get('timeframe', 'daily')

            # Download data
            data = yf.download(symbols, start=start_date, end=end_date)

            if data.empty:
                return []

            # Convert to HistoricalData format
            result = []
            for symbol in symbols:
                symbol_data = data if len(symbols) == 1 else data[symbol]

                bars = []
                for date, row in symbol_data.iterrows():
                    bars.append({
                        'date': date.isoformat(),
                        'open': float(row['Open']) if 'Open' in row else 0,
                        'high': float(row['High']) if 'High' in row else 0,
                        'low': float(row['Low']) if 'Low' in row else 0,
                        'close': float(row['Close']) if 'Close' in row else 0,
                        'volume': int(row['Volume']) if 'Volume' in row else 0,
                    })

                result.append({
                    'symbol': symbol,
                    'timeframe': timeframe,
                    'data': bars
                })

            return result

        except Exception as e:
            self._error('Data fetch failed', e)
            raise

    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate indicator using VectorBT"""
        raise NotImplementedError('Use VectorBT indicators within backtests')

    def _extract_strategy_code(self, strategy: Dict[str, Any]) -> str:
        """Extract Python code from strategy"""
        strategy_type = strategy.get('type')

        if strategy_type == 'code':
            code = strategy.get('code', {})
            if code.get('language') == 'python':
                return code.get('source', '')
            else:
                raise ValueError('Only Python strategies supported')
        else:
            raise ValueError(f'Strategy type {strategy_type} not supported yet')

    def _get_empty_metrics(self) -> PerformanceMetrics:
        return PerformanceMetrics(
            total_return=0, annualized_return=0, sharpe_ratio=0, sortino_ratio=0,
            max_drawdown=0, win_rate=0, loss_rate=0, profit_factor=0, volatility=0,
            calmar_ratio=0, total_trades=0, winning_trades=0, losing_trades=0,
            average_win=0, average_loss=0, largest_win=0, largest_loss=0,
            average_trade_return=0, expectancy=0
        )

    def _get_empty_statistics(self) -> BacktestStatistics:
        return BacktestStatistics(
            start_date='', end_date='', initial_capital=0, final_capital=0,
            total_fees=0, total_slippage=0, total_trades=0, winning_days=0,
            losing_days=0, average_daily_return=0, best_day=0, worst_day=0,
            consecutive_wins=0, consecutive_losses=0
        )

    def _parse_trades(self, portfolio) -> list:
        """Parse trades from VectorBT portfolio"""
        trades = []
        try:
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records'):
                trade_records = portfolio.trades.records

                for idx, trade in enumerate(trade_records):
                    entry_idx = trade['entry_idx'] if hasattr(trade, '__getitem__') else getattr(trade, 'entry_idx', 0)
                    exit_idx = trade['exit_idx'] if hasattr(trade, '__getitem__') else getattr(trade, 'exit_idx', 0)
                    pnl = trade['pnl'] if hasattr(trade, '__getitem__') else getattr(trade, 'pnl', 0)

                    trades.append(Trade(
                        id=f'trade_{idx}',
                        symbol=str(getattr(trade, 'col', 'UNKNOWN')),
                        entry_date=str(entry_idx),
                        side='long' if pnl >= 0 else 'short',
                        quantity=float(getattr(trade, 'size', 0)),
                        entry_price=float(getattr(trade, 'entry_price', 0)),
                        commission=float(getattr(trade, 'fees', 0)),
                        slippage=0,
                        exit_date=str(exit_idx),
                        exit_price=float(getattr(trade, 'exit_price', 0)),
                        pnl=float(pnl),
                        pnl_percent=float(getattr(trade, 'return', 0)),
                        holding_period=int(exit_idx - entry_idx) if exit_idx > entry_idx else 0,
                        exit_reason='signal'
                    ))
        except Exception as e:
            self._error('Failed to parse trades', e)

        return trades

    def _build_equity_curve(self, portfolio, initial_capital: float) -> list:
        """Build equity curve with returns and drawdown"""
        equity = []
        try:
            value_series = portfolio.value()
            peak = initial_capital

            for date, value in value_series.items():
                # Calculate returns
                returns = (value - initial_capital) / initial_capital if initial_capital > 0 else 0

                # Calculate drawdown
                peak = max(peak, value)
                drawdown = (value - peak) / peak if peak > 0 else 0

                equity.append(EquityPoint(
                    date=str(date),
                    equity=float(value),
                    returns=float(returns),
                    drawdown=float(drawdown),
                ))
        except Exception as e:
            self._error('Failed to build equity curve', e)

        return equity


# CLI Entry Point
def main():
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: python vectorbt_provider.py <command> <json_args>'
        }))
        sys.exit(1)

    command = sys.argv[1]
    json_args = sys.argv[2]

    try:
        args = parse_json_input(json_args)
        provider = VectorBTProvider()

        if command == 'test_import':
            import vectorbt as vbt
            print(json_response({'success': True, 'version': vbt.__version__}))
        elif command == 'test_connection':
            result = provider.test_connection()
            print(json_response(result))
        elif command == 'initialize':
            result = provider.initialize(args)
            print(json_response(result))
        elif command == 'run_backtest':
            result = provider.run_backtest(args)
            print(json_response(result))
        else:
            print(json_response({'success': False, 'error': f'Unknown command: {command}'}))

    except Exception as e:
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': __import__('traceback').format_exc()
        }))
        sys.exit(1)


if __name__ == '__main__':
    main()
