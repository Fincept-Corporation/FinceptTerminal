"""
Backtesting Engine for FinceptTerminal
Walk-forward validation and strategy performance testing
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Tuple, Optional, Callable
import json
import sys
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')


class BacktestingEngine:
    """
    Comprehensive backtesting framework
    Supports walk-forward validation, performance metrics, and strategy testing
    """

    def __init__(self, initial_capital: float = 100000,
                 commission: float = 0.001,
                 slippage: float = 0.0005):
        """
        Initialize backtesting engine

        Args:
            initial_capital: Starting capital
            commission: Commission rate (0.001 = 0.1%)
            slippage: Slippage rate (0.0005 = 0.05%)
        """
        self.initial_capital = initial_capital
        self.commission = commission
        self.slippage = slippage
        self.reset()

    def reset(self):
        """Reset backtest state"""
        self.capital = self.initial_capital
        self.positions = {}
        self.trades = []
        self.equity_curve = []
        self.returns = []
        self.current_date = None

    def execute_trade(self, symbol: str, action: str, quantity: int,
                      price: float, date: datetime) -> Dict:
        """
        Execute a trade

        Args:
            symbol: Trading symbol
            action: 'BUY' or 'SELL'
            quantity: Number of shares
            price: Execution price
            date: Trade date

        Returns:
            Trade result
        """
        # Apply slippage
        if action == 'BUY':
            actual_price = price * (1 + self.slippage)
        else:
            actual_price = price * (1 - self.slippage)

        # Calculate cost
        gross_value = actual_price * quantity
        commission_cost = gross_value * self.commission
        total_cost = gross_value + commission_cost

        trade_result = {
            'date': str(date),
            'symbol': symbol,
            'action': action,
            'quantity': quantity,
            'price': float(actual_price),
            'gross_value': float(gross_value),
            'commission': float(commission_cost),
            'total_cost': float(total_cost)
        }

        if action == 'BUY':
            if total_cost > self.capital:
                trade_result['status'] = 'REJECTED'
                trade_result['reason'] = 'Insufficient capital'
                return trade_result

            self.capital -= total_cost

            if symbol in self.positions:
                # Update average price
                current_qty = self.positions[symbol]['quantity']
                current_avg = self.positions[symbol]['avg_price']
                new_avg = (current_avg * current_qty + actual_price * quantity) / (current_qty + quantity)

                self.positions[symbol]['quantity'] += quantity
                self.positions[symbol]['avg_price'] = new_avg
            else:
                self.positions[symbol] = {
                    'quantity': quantity,
                    'avg_price': actual_price,
                    'entry_date': date
                }

            trade_result['status'] = 'EXECUTED'

        elif action == 'SELL':
            if symbol not in self.positions or self.positions[symbol]['quantity'] < quantity:
                trade_result['status'] = 'REJECTED'
                trade_result['reason'] = 'Insufficient position'
                return trade_result

            # Calculate P&L
            avg_buy_price = self.positions[symbol]['avg_price']
            pnl = (actual_price - avg_buy_price) * quantity
            pnl_percent = (actual_price / avg_buy_price - 1) * 100

            self.capital += gross_value - commission_cost

            self.positions[symbol]['quantity'] -= quantity

            if self.positions[symbol]['quantity'] == 0:
                del self.positions[symbol]

            trade_result['status'] = 'EXECUTED'
            trade_result['pnl'] = float(pnl)
            trade_result['pnl_percent'] = float(pnl_percent)

        self.trades.append(trade_result)
        return trade_result

    def calculate_portfolio_value(self, current_prices: Dict[str, float]) -> float:
        """
        Calculate total portfolio value

        Args:
            current_prices: Dict of symbol -> current price

        Returns:
            Total portfolio value
        """
        positions_value = sum(
            pos['quantity'] * current_prices.get(symbol, pos['avg_price'])
            for symbol, pos in self.positions.items()
        )

        return self.capital + positions_value

    def run_backtest(self, data: pd.DataFrame, strategy_func: Callable,
                     strategy_params: Dict = None) -> Dict:
        """
        Run backtest with given strategy

        Args:
            data: Historical price data (OHLCV)
            strategy_func: Strategy function that returns signals
            strategy_params: Parameters for strategy

        Returns:
            Backtest results
        """
        self.reset()
        strategy_params = strategy_params or {}

        start_date = data.index[0]
        end_date = data.index[-1]

        for current_date, row in data.iterrows():
            self.current_date = current_date

            # Get strategy signal
            signal = strategy_func(data, current_date, self.positions, strategy_params)

            # Execute trades based on signal
            if signal and 'action' in signal:
                if signal['action'] in ['BUY', 'SELL']:
                    self.execute_trade(
                        symbol=signal.get('symbol', 'UNKNOWN'),
                        action=signal['action'],
                        quantity=signal.get('quantity', 1),
                        price=row.get('close', row.get('price', 0)),
                        date=current_date
                    )

            # Record equity
            current_prices = {signal.get('symbol', 'UNKNOWN'): row.get('close', row.get('price', 0))}
            portfolio_value = self.calculate_portfolio_value(current_prices)
            self.equity_curve.append({
                'date': str(current_date),
                'value': float(portfolio_value)
            })

            # Calculate returns
            if len(self.equity_curve) > 1:
                daily_return = (portfolio_value / self.equity_curve[-2]['value'] - 1)
                self.returns.append(daily_return)

        # Calculate performance metrics
        metrics = self.calculate_performance_metrics()

        return {
            'success': True,
            'start_date': str(start_date),
            'end_date': str(end_date),
            'initial_capital': self.initial_capital,
            'final_capital': float(self.capital),
            'final_portfolio_value': float(portfolio_value),
            'total_trades': len(self.trades),
            'equity_curve': self.equity_curve,
            'trades': self.trades,
            'metrics': metrics
        }

    def calculate_performance_metrics(self) -> Dict:
        """
        Calculate comprehensive performance metrics

        Returns:
            Dict with performance metrics
        """
        if not self.equity_curve:
            return {}

        # Total return
        initial_value = self.equity_curve[0]['value']
        final_value = self.equity_curve[-1]['value']
        total_return = (final_value / initial_value - 1) * 100

        # Returns array
        returns = np.array(self.returns) if self.returns else np.array([0])

        # Sharpe Ratio (annualized, assuming 252 trading days)
        if len(returns) > 0 and np.std(returns) > 0:
            sharpe_ratio = np.mean(returns) / np.std(returns) * np.sqrt(252)
        else:
            sharpe_ratio = 0

        # Sortino Ratio (downside deviation)
        downside_returns = returns[returns < 0]
        if len(downside_returns) > 0:
            downside_std = np.std(downside_returns)
            sortino_ratio = np.mean(returns) / downside_std * np.sqrt(252) if downside_std > 0 else 0
        else:
            sortino_ratio = 0

        # Maximum Drawdown
        equity_values = [eq['value'] for eq in self.equity_curve]
        peak = np.maximum.accumulate(equity_values)
        drawdown = (equity_values - peak) / peak
        max_drawdown = np.min(drawdown) * 100

        # Calmar Ratio
        calmar_ratio = total_return / abs(max_drawdown) if max_drawdown != 0 else 0

        # Win Rate
        winning_trades = [t for t in self.trades if t.get('pnl', 0) > 0]
        total_closed_trades = len([t for t in self.trades if 'pnl' in t])
        win_rate = len(winning_trades) / total_closed_trades * 100 if total_closed_trades > 0 else 0

        # Average Win/Loss
        if winning_trades:
            avg_win = np.mean([t['pnl'] for t in winning_trades])
        else:
            avg_win = 0

        losing_trades = [t for t in self.trades if t.get('pnl', 0) < 0]
        if losing_trades:
            avg_loss = np.mean([abs(t['pnl']) for t in losing_trades])
        else:
            avg_loss = 0

        # Profit Factor
        total_wins = sum([t['pnl'] for t in winning_trades]) if winning_trades else 0
        total_losses = sum([abs(t['pnl']) for t in losing_trades]) if losing_trades else 0
        profit_factor = total_wins / total_losses if total_losses > 0 else float('inf')

        # Volatility (annualized)
        volatility = np.std(returns) * np.sqrt(252) * 100 if len(returns) > 0 else 0

        return {
            'total_return': float(total_return),
            'total_return_dollar': float(final_value - initial_value),
            'sharpe_ratio': float(sharpe_ratio),
            'sortino_ratio': float(sortino_ratio),
            'max_drawdown': float(max_drawdown),
            'calmar_ratio': float(calmar_ratio),
            'win_rate': float(win_rate),
            'total_trades': total_closed_trades,
            'winning_trades': len(winning_trades),
            'losing_trades': len(losing_trades),
            'avg_win': float(avg_win),
            'avg_loss': float(avg_loss),
            'profit_factor': float(profit_factor) if profit_factor != float('inf') else 999.99,
            'volatility': float(volatility),
            'avg_daily_return': float(np.mean(returns) * 100) if len(returns) > 0 else 0
        }

    def walk_forward_analysis(self, data: pd.DataFrame, strategy_func: Callable,
                              train_window: int = 252, test_window: int = 63,
                              step_size: int = 21, strategy_params: Dict = None) -> Dict:
        """
        Perform walk-forward analysis

        Args:
            data: Historical data
            strategy_func: Strategy function
            train_window: Training window size (days)
            test_window: Testing window size (days)
            step_size: Step size for rolling window
            strategy_params: Strategy parameters

        Returns:
            Walk-forward results
        """
        results = []
        strategy_params = strategy_params or {}

        total_length = len(data)
        current_start = 0

        while current_start + train_window + test_window <= total_length:
            # Training period
            train_end = current_start + train_window
            test_end = min(train_end + test_window, total_length)

            train_data = data.iloc[current_start:train_end]
            test_data = data.iloc[train_end:test_end]

            # Run backtest on test period
            backtest_result = self.run_backtest(test_data, strategy_func, strategy_params)

            results.append({
                'train_start': str(train_data.index[0]),
                'train_end': str(train_data.index[-1]),
                'test_start': str(test_data.index[0]),
                'test_end': str(test_data.index[-1]),
                'metrics': backtest_result['metrics']
            })

            current_start += step_size

        # Aggregate results
        if results:
            avg_metrics = {
                'avg_total_return': np.mean([r['metrics']['total_return'] for r in results]),
                'avg_sharpe_ratio': np.mean([r['metrics']['sharpe_ratio'] for r in results]),
                'avg_max_drawdown': np.mean([r['metrics']['max_drawdown'] for r in results]),
                'avg_win_rate': np.mean([r['metrics']['win_rate'] for r in results]),
                'num_windows': len(results),
                'consistency': len([r for r in results if r['metrics']['total_return'] > 0]) / len(results) * 100
            }
        else:
            avg_metrics = {}

        return {
            'success': True,
            'walk_forward_results': results,
            'average_metrics': avg_metrics,
            'parameters': {
                'train_window': train_window,
                'test_window': test_window,
                'step_size': step_size
            }
        }


# Example Strategy Functions

def moving_average_crossover_strategy(data: pd.DataFrame, current_date: datetime,
                                      positions: Dict, params: Dict) -> Optional[Dict]:
    """
    Simple moving average crossover strategy

    Args:
        data: Historical data
        current_date: Current date in backtest
        positions: Current positions
        params: Strategy parameters (fast_period, slow_period)

    Returns:
        Signal dict or None
    """
    fast_period = params.get('fast_period', 20)
    slow_period = params.get('slow_period', 50)
    symbol = params.get('symbol', 'UNKNOWN')

    # Get data up to current date
    historical = data.loc[:current_date]

    if len(historical) < slow_period:
        return None

    # Calculate moving averages
    fast_ma = historical['close'].rolling(window=fast_period).mean().iloc[-1]
    slow_ma = historical['close'].rolling(window=slow_period).mean().iloc[-1]
    prev_fast_ma = historical['close'].rolling(window=fast_period).mean().iloc[-2]
    prev_slow_ma = historical['close'].rolling(window=slow_period).mean().iloc[-2]

    # Generate signals
    if prev_fast_ma <= prev_slow_ma and fast_ma > slow_ma:
        # Bullish crossover - BUY
        if symbol not in positions:
            return {
                'action': 'BUY',
                'symbol': symbol,
                'quantity': 100,
                'reason': 'MA Crossover - Bullish'
            }

    elif prev_fast_ma >= prev_slow_ma and fast_ma < slow_ma:
        # Bearish crossover - SELL
        if symbol in positions and positions[symbol]['quantity'] > 0:
            return {
                'action': 'SELL',
                'symbol': symbol,
                'quantity': positions[symbol]['quantity'],
                'reason': 'MA Crossover - Bearish'
            }

    return None


def rsi_strategy(data: pd.DataFrame, current_date: datetime,
                 positions: Dict, params: Dict) -> Optional[Dict]:
    """
    RSI mean reversion strategy

    Args:
        data: Historical data
        current_date: Current date
        positions: Current positions
        params: Strategy parameters (rsi_period, oversold, overbought)

    Returns:
        Signal dict or None
    """
    rsi_period = params.get('rsi_period', 14)
    oversold = params.get('oversold', 30)
    overbought = params.get('overbought', 70)
    symbol = params.get('symbol', 'UNKNOWN')

    historical = data.loc[:current_date]

    if len(historical) < rsi_period + 1:
        return None

    # Calculate RSI
    delta = historical['close'].diff()
    gain = (delta.where(delta > 0, 0)).rolling(window=rsi_period).mean()
    loss = (-delta.where(delta < 0, 0)).rolling(window=rsi_period).mean()
    rs = gain / loss
    rsi = 100 - (100 / (1 + rs))

    current_rsi = rsi.iloc[-1]

    # Generate signals
    if current_rsi < oversold and symbol not in positions:
        return {
            'action': 'BUY',
            'symbol': symbol,
            'quantity': 100,
            'reason': f'RSI Oversold: {current_rsi:.2f}'
        }

    elif current_rsi > overbought and symbol in positions:
        return {
            'action': 'SELL',
            'symbol': symbol,
            'quantity': positions[symbol]['quantity'],
            'reason': f'RSI Overbought: {current_rsi:.2f}'
        }

    return None


def main():
    """
    Command-line interface
    Input JSON:
    {
        "data": {
            "dates": [...],
            "close": [...],
            "high": [...],
            "low": [...],
            "volume": [...]
        },
        "strategy": "ma_crossover" | "rsi",
        "strategy_params": {...},
        "initial_capital": 100000,
        "commission": 0.001,
        "action": "backtest" | "walk_forward"
    }
    """
    try:
        input_data = json.loads(sys.stdin.read())

        # Parse data
        data_raw = input_data.get('data', {})
        df = pd.DataFrame({
            'close': data_raw.get('close', []),
            'high': data_raw.get('high', data_raw.get('close', [])),
            'low': data_raw.get('low', data_raw.get('close', [])),
            'volume': data_raw.get('volume', [0] * len(data_raw.get('close', [])))
        }, index=pd.to_datetime(data_raw.get('dates', [])))

        # Initialize engine
        engine = BacktestingEngine(
            initial_capital=input_data.get('initial_capital', 100000),
            commission=input_data.get('commission', 0.001),
            slippage=input_data.get('slippage', 0.0005)
        )

        # Select strategy
        strategy_name = input_data.get('strategy', 'ma_crossover')
        strategy_params = input_data.get('strategy_params', {})

        if strategy_name == 'ma_crossover':
            strategy_func = moving_average_crossover_strategy
        elif strategy_name == 'rsi':
            strategy_func = rsi_strategy
        else:
            print(json.dumps({'error': f'Unknown strategy: {strategy_name}'}))
            return

        # Run backtest or walk-forward
        action = input_data.get('action', 'backtest')

        if action == 'backtest':
            result = engine.run_backtest(df, strategy_func, strategy_params)
        elif action == 'walk_forward':
            result = engine.walk_forward_analysis(
                df, strategy_func,
                train_window=input_data.get('train_window', 252),
                test_window=input_data.get('test_window', 63),
                step_size=input_data.get('step_size', 21),
                strategy_params=strategy_params
            )
        else:
            result = {'error': f'Unknown action: {action}'}

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({'error': str(e)}))


if __name__ == '__main__':
    main()
