"""
Python Strategy Backtest Engine

Loads and executes QCAlgorithm-compatible Python strategies from the Fincept
strategy library. Supports multi-symbol backtesting with proper portfolio tracking.

Usage:
    python python_backtest_engine.py \
        --strategy-id FCT-82A09B26 \
        --symbols "AAPL,MSFT,GOOGL" \
        --start-date 2023-01-01 \
        --end-date 2024-01-01 \
        --initial-cash 100000 \
        --data-provider yfinance \
        --parameters '{"fast_period": 10, "slow_period": 20}'

Output:
    JSON with trades, equity curve, and performance metrics
"""

import json
import sys
import argparse
import os
import traceback
import importlib.util
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional

# Debug log collector
DEBUG_LOG = []

def debug(msg):
    """Add a debug message to the log."""
    DEBUG_LOG.append(f"[py:python_backtest] {msg}")

debug("Script starting...")
debug(f"Python version: {sys.version}")
debug(f"Working directory: {os.getcwd()}")

# Add strategies folder to path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STRATEGIES_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "strategies")
sys.path.insert(0, STRATEGIES_DIR)
debug(f"Strategies dir: {STRATEGIES_DIR}")

try:
    import pandas as pd
    import numpy as np
    debug(f"pandas version: {pd.__version__}, numpy version: {np.__version__}")
except ImportError as e:
    debug(f"ERROR: Required library not installed: {e}")
    print(json.dumps({
        'success': False,
        'error': f'Required library not installed: {e}',
        'debug': DEBUG_LOG
    }))
    sys.exit(1)


def load_registry() -> Dict[str, Any]:
    """Load the strategy registry."""
    registry_path = os.path.join(STRATEGIES_DIR, "_registry.py")
    debug(f"Loading registry from: {registry_path}")

    if not os.path.exists(registry_path):
        raise FileNotFoundError(f"Registry not found: {registry_path}")

    # Execute registry file to get STRATEGY_REGISTRY dict
    registry_globals = {}
    with open(registry_path, 'r', encoding='utf-8') as f:
        exec(f.read(), registry_globals)

    return registry_globals.get('STRATEGY_REGISTRY', {})


def load_strategy_class(strategy_id: str, custom_code: Optional[str] = None):
    """Load a strategy class by ID or from custom code."""
    if custom_code:
        debug("Loading from custom code")
        # Execute custom code to get the strategy class
        strategy_globals = {}
        exec(custom_code, strategy_globals)
        # Find the QCAlgorithm subclass
        for name, obj in strategy_globals.items():
            if isinstance(obj, type) and hasattr(obj, 'initialize') and name != 'QCAlgorithm':
                return obj
        raise ValueError("No valid QCAlgorithm subclass found in custom code")

    registry = load_registry()
    if strategy_id not in registry:
        raise ValueError(f"Strategy not found: {strategy_id}")

    strategy_info = registry[strategy_id]
    strategy_path = os.path.join(STRATEGIES_DIR, strategy_info['path'])
    debug(f"Loading strategy from: {strategy_path}")

    if not os.path.exists(strategy_path):
        raise FileNotFoundError(f"Strategy file not found: {strategy_path}")

    # Load the module
    spec = importlib.util.spec_from_file_location(strategy_info['name'], strategy_path)
    module = importlib.util.module_from_spec(spec)

    # Execute the module
    spec.loader.exec_module(module)

    # Find the QCAlgorithm subclass
    for name in dir(module):
        obj = getattr(module, name)
        if isinstance(obj, type) and hasattr(obj, 'initialize') and name != 'QCAlgorithm':
            return obj

    raise ValueError(f"No valid QCAlgorithm subclass found in {strategy_info['path']}")


def fetch_historical_data(symbols: List[str], start_date: str, end_date: str,
                          provider: str = 'yfinance') -> Dict[str, pd.DataFrame]:
    """Fetch historical data for multiple symbols."""
    debug(f"Fetching data: symbols={symbols}, start={start_date}, end={end_date}, provider={provider}")
    data = {}

    if provider == 'yfinance':
        try:
            import yfinance as yf
            debug(f"yfinance version: {yf.__version__}")

            for symbol in symbols:
                debug(f"Fetching {symbol}...")
                ticker = yf.Ticker(symbol)
                df = ticker.history(start=start_date, end=end_date)

                if df.empty:
                    debug(f"No data for {symbol}")
                    continue

                # Normalize column names
                df.columns = [c.lower() for c in df.columns]
                df = df[['open', 'high', 'low', 'close', 'volume']].copy()
                df.index = pd.to_datetime(df.index)
                data[symbol.upper()] = df
                debug(f"Fetched {len(df)} bars for {symbol}")
        except ImportError:
            raise ImportError("yfinance not installed. Install with: pip install yfinance")
    else:
        raise ValueError(f"Unsupported data provider: {provider}")

    return data


class BacktestSlice:
    """Represents a single time slice of market data."""

    def __init__(self, timestamp: datetime, bars: Dict[str, Any]):
        self._timestamp = timestamp
        self._bars = bars

    @property
    def time(self) -> datetime:
        return self._timestamp

    def __contains__(self, symbol):
        return str(symbol).upper() in self._bars

    def __getitem__(self, symbol):
        ticker = str(symbol).upper()
        if ticker not in self._bars:
            return None
        return self._bars[ticker]

    def keys(self):
        return self._bars.keys()

    def items(self):
        return self._bars.items()


class BacktestBar:
    """Represents a single OHLCV bar."""

    def __init__(self, symbol: str, row: pd.Series, timestamp: datetime):
        self.symbol = symbol
        self.time = timestamp
        self.open = float(row['open'])
        self.high = float(row['high'])
        self.low = float(row['low'])
        self.close = float(row['close'])
        self.volume = float(row['volume'])
        self.price = self.close


def run_backtest(
    strategy_class,
    historical_data: Dict[str, pd.DataFrame],
    initial_cash: float,
    parameters: Dict[str, str],
    start_date: str,
    end_date: str
) -> Dict[str, Any]:
    """Execute the backtest."""
    debug(f"Starting backtest with {len(historical_data)} symbols")

    # Create algorithm instance
    algo = strategy_class()

    # Set parameters
    algo.set_parameters(parameters)

    # Override dates and cash
    algo.set_start_date(datetime.strptime(start_date, '%Y-%m-%d'))
    algo.set_end_date(datetime.strptime(end_date, '%Y-%m-%d'))
    algo.set_cash(initial_cash)

    # Combine all data into a single timeline
    all_dates = set()
    for symbol, df in historical_data.items():
        all_dates.update(df.index.tolist())
    all_dates = sorted(all_dates)
    debug(f"Total trading days: {len(all_dates)}")

    # Initialize algorithm
    debug("Calling initialize()...")
    try:
        algo.initialize()
    except Exception as e:
        debug(f"initialize() error: {e}\n{traceback.format_exc()}")
        return {
            'success': False,
            'error': f'Strategy initialization failed: {e}',
            'trades': [],
            'equity_curve': [],
            'metrics': {}
        }

    # Track trades and equity
    trades = []
    equity_curve = []
    trade_id = 0

    # Add securities that exist in data
    for symbol in historical_data.keys():
        algo.add_equity(symbol)

    # Initialize security prices
    for symbol, df in historical_data.items():
        if len(df) > 0:
            algo.securities[symbol].set_market_price(df['close'].iloc[0])

    # Track entry info per symbol for trade recording
    entry_info: Dict[str, Dict] = {}

    # Walk through each bar
    for i, timestamp in enumerate(all_dates):
        # Update algorithm time
        algo._time = pd.Timestamp(timestamp).to_pydatetime()

        # Build slice for this timestamp
        bars = {}
        for symbol, df in historical_data.items():
            if timestamp in df.index:
                row = df.loc[timestamp]
                bar = BacktestBar(symbol, row, timestamp)
                bars[symbol] = bar
                # Update security price
                if symbol in algo.securities:
                    algo.securities[symbol].set_market_price(bar.close)

        slice_data = BacktestSlice(timestamp, bars)

        # Record positions before on_data
        positions_before = {sym: algo.portfolio[sym].quantity for sym in historical_data.keys()}

        # Call on_data
        try:
            algo.on_data(slice_data)
        except Exception as e:
            debug(f"on_data() error at {timestamp}: {e}")
            # Continue execution despite errors

        # Check for position changes (trades)
        for symbol in historical_data.keys():
            pos_before = positions_before.get(symbol, 0)
            pos_after = algo.portfolio[symbol].quantity

            if pos_before != pos_after:
                qty_change = pos_after - pos_before
                price = algo.securities[symbol].price if symbol in algo.securities else 0

                if qty_change > 0:  # Entry or add
                    if symbol not in entry_info:
                        entry_info[symbol] = {
                            'entry_time': timestamp,
                            'entry_price': price,
                            'entry_bar': i,
                            'quantity': qty_change
                        }
                    else:
                        # Adding to position - average price
                        old_qty = entry_info[symbol]['quantity']
                        old_price = entry_info[symbol]['entry_price']
                        new_qty = old_qty + qty_change
                        entry_info[symbol]['entry_price'] = (old_price * old_qty + price * qty_change) / new_qty
                        entry_info[symbol]['quantity'] = new_qty
                else:  # Exit or reduce
                    if symbol in entry_info:
                        entry = entry_info[symbol]
                        exit_qty = min(abs(qty_change), entry['quantity'])
                        pnl = (price - entry['entry_price']) * exit_qty

                        trade_id += 1
                        trades.append({
                            'id': str(trade_id),
                            'symbol': symbol,
                            'side': 'BUY',
                            'quantity': exit_qty,
                            'entry_price': round(entry['entry_price'], 2),
                            'exit_price': round(price, 2),
                            'pnl': round(pnl, 2),
                            'entry_time': entry['entry_time'].isoformat() if hasattr(entry['entry_time'], 'isoformat') else str(entry['entry_time']),
                            'exit_time': timestamp.isoformat() if hasattr(timestamp, 'isoformat') else str(timestamp),
                            'bars_held': i - entry['entry_bar']
                        })

                        # Update or remove entry info
                        remaining = entry['quantity'] - exit_qty
                        if remaining <= 0:
                            del entry_info[symbol]
                        else:
                            entry_info[symbol]['quantity'] = remaining

        # Record equity
        equity = algo.portfolio.total_portfolio_value
        equity_curve.append({
            'time': timestamp.isoformat() if hasattr(timestamp, 'isoformat') else str(timestamp),
            'value': round(equity, 2)
        })

    # Call on_end_of_algorithm
    try:
        algo.on_end_of_algorithm()
    except Exception as e:
        debug(f"on_end_of_algorithm() error: {e}")

    # Close any remaining positions
    for symbol, entry in list(entry_info.items()):
        if symbol in algo.securities:
            price = algo.securities[symbol].price
            pnl = (price - entry['entry_price']) * entry['quantity']

            trade_id += 1
            trades.append({
                'id': str(trade_id),
                'symbol': symbol,
                'side': 'BUY',
                'quantity': entry['quantity'],
                'entry_price': round(entry['entry_price'], 2),
                'exit_price': round(price, 2),
                'pnl': round(pnl, 2),
                'entry_time': entry['entry_time'].isoformat() if hasattr(entry['entry_time'], 'isoformat') else str(entry['entry_time']),
                'exit_time': all_dates[-1].isoformat() if hasattr(all_dates[-1], 'isoformat') else str(all_dates[-1]),
                'bars_held': len(all_dates) - entry['entry_bar']
            })

    # Calculate metrics
    metrics = calculate_metrics(trades, equity_curve, initial_cash)

    return {
        'success': True,
        'trades': trades,
        'equity_curve': equity_curve[-100:],  # Last 100 points for chart
        'metrics': metrics,
        'debug': DEBUG_LOG
    }


def calculate_metrics(trades: List[Dict], equity_curve: List[Dict], initial_cash: float) -> Dict[str, Any]:
    """Calculate backtest performance metrics."""
    if not equity_curve:
        return {
            'total_trades': 0,
            'winning_trades': 0,
            'losing_trades': 0,
            'win_rate': 0,
            'total_return': 0,
            'total_return_pct': 0,
            'max_drawdown': 0,
            'max_drawdown_pct': 0,
            'sharpe_ratio': 0,
            'profit_factor': 0,
            'avg_pnl': 0,
            'avg_bars_held': 0
        }

    final_equity = equity_curve[-1]['value']
    total_return = final_equity - initial_cash
    total_return_pct = (total_return / initial_cash) * 100

    # Max drawdown
    peak = initial_cash
    max_dd = 0
    max_dd_pct = 0
    for point in equity_curve:
        equity = point['value']
        if equity > peak:
            peak = equity
        dd = peak - equity
        dd_pct = (dd / peak) * 100 if peak > 0 else 0
        if dd_pct > max_dd_pct:
            max_dd = dd
            max_dd_pct = dd_pct

    # Trade statistics
    total_trades = len(trades)
    if total_trades == 0:
        return {
            'total_trades': 0,
            'winning_trades': 0,
            'losing_trades': 0,
            'win_rate': 0,
            'total_return': round(total_return, 2),
            'total_return_pct': round(total_return_pct, 2),
            'max_drawdown': round(max_dd, 2),
            'max_drawdown_pct': round(max_dd_pct, 2),
            'sharpe_ratio': 0,
            'profit_factor': 0,
            'avg_pnl': 0,
            'avg_bars_held': 0
        }

    wins = [t for t in trades if t['pnl'] > 0]
    losses = [t for t in trades if t['pnl'] <= 0]
    win_rate = (len(wins) / total_trades) * 100

    gross_profit = sum(t['pnl'] for t in wins) if wins else 0
    gross_loss = abs(sum(t['pnl'] for t in losses)) if losses else 0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0 else (999.99 if gross_profit > 0 else 0)

    avg_pnl = sum(t['pnl'] for t in trades) / total_trades
    avg_bars_held = sum(t['bars_held'] for t in trades) / total_trades

    # Sharpe ratio (simplified daily calculation)
    if len(equity_curve) > 1:
        returns = []
        for i in range(1, len(equity_curve)):
            prev = equity_curve[i-1]['value']
            curr = equity_curve[i]['value']
            if prev > 0:
                returns.append((curr - prev) / prev)

        if returns:
            mean_ret = np.mean(returns)
            std_ret = np.std(returns)
            sharpe = (mean_ret / std_ret) * np.sqrt(252) if std_ret > 0 else 0
        else:
            sharpe = 0
    else:
        sharpe = 0

    return {
        'total_trades': total_trades,
        'winning_trades': len(wins),
        'losing_trades': len(losses),
        'win_rate': round(win_rate, 1),
        'total_return': round(total_return, 2),
        'total_return_pct': round(total_return_pct, 2),
        'max_drawdown': round(max_dd, 2),
        'max_drawdown_pct': round(max_dd_pct, 2),
        'sharpe_ratio': round(sharpe, 2),
        'profit_factor': round(min(profit_factor, 999.99), 2),
        'avg_pnl': round(avg_pnl, 2),
        'avg_bars_held': round(avg_bars_held, 1)
    }


def main():
    debug("main() started")
    debug(f"sys.argv: {sys.argv}")

    parser = argparse.ArgumentParser(description='Python Strategy Backtest Engine')
    parser.add_argument('--strategy-id', required=True, help='Strategy ID (e.g., FCT-82A09B26)')
    parser.add_argument('--custom-code', default=None, help='Optional custom strategy code (base64 or file path)')
    parser.add_argument('--symbols', required=True, help='Comma-separated symbols')
    parser.add_argument('--start-date', required=True, help='Start date (YYYY-MM-DD)')
    parser.add_argument('--end-date', required=True, help='End date (YYYY-MM-DD)')
    parser.add_argument('--initial-cash', type=float, default=100000, help='Starting capital')
    parser.add_argument('--data-provider', default='yfinance', help='Data provider: yfinance')
    parser.add_argument('--parameters', default='{}', help='JSON parameters for strategy')

    try:
        args = parser.parse_args()
        debug(f"Parsed args: strategy_id={args.strategy_id}, symbols={args.symbols}")
        debug(f"start_date={args.start_date}, end_date={args.end_date}, initial_cash={args.initial_cash}")
    except Exception as e:
        debug(f"Argument parsing error: {e}")
        print(json.dumps({'success': False, 'error': f'Argument parsing error: {e}', 'debug': DEBUG_LOG}))
        sys.exit(1)

    # Parse symbols
    symbols = [s.strip().upper() for s in args.symbols.split(',') if s.strip()]
    if not symbols:
        print(json.dumps({'success': False, 'error': 'No symbols provided', 'debug': DEBUG_LOG}))
        sys.exit(1)
    debug(f"Symbols: {symbols}")

    # Parse parameters
    try:
        parameters = json.loads(args.parameters)
    except json.JSONDecodeError as e:
        debug(f"Parameters JSON error: {e}")
        parameters = {}
    debug(f"Parameters: {parameters}")

    # Load strategy class
    try:
        custom_code = None
        if args.custom_code:
            if os.path.isfile(args.custom_code):
                with open(args.custom_code, 'r', encoding='utf-8') as f:
                    custom_code = f.read()
            else:
                # Assume base64 encoded
                import base64
                custom_code = base64.b64decode(args.custom_code).decode('utf-8')

        strategy_class = load_strategy_class(args.strategy_id, custom_code)
        debug(f"Loaded strategy class: {strategy_class.__name__}")
    except Exception as e:
        debug(f"Strategy loading error: {e}\n{traceback.format_exc()}")
        print(json.dumps({
            'success': False,
            'error': f'Failed to load strategy: {e}',
            'debug': DEBUG_LOG
        }))
        sys.exit(1)

    # Fetch historical data
    try:
        historical_data = fetch_historical_data(
            symbols, args.start_date, args.end_date, args.data_provider
        )
        if not historical_data:
            print(json.dumps({
                'success': False,
                'error': 'No historical data available for the specified symbols',
                'debug': DEBUG_LOG
            }))
            sys.exit(1)
    except Exception as e:
        debug(f"Data fetching error: {e}\n{traceback.format_exc()}")
        print(json.dumps({
            'success': False,
            'error': f'Failed to fetch historical data: {e}',
            'debug': DEBUG_LOG
        }))
        sys.exit(1)

    # Run backtest
    try:
        result = run_backtest(
            strategy_class=strategy_class,
            historical_data=historical_data,
            initial_cash=args.initial_cash,
            parameters=parameters,
            start_date=args.start_date,
            end_date=args.end_date
        )
    except Exception as e:
        debug(f"Backtest error: {e}\n{traceback.format_exc()}")
        print(json.dumps({
            'success': False,
            'error': f'Backtest execution failed: {e}',
            'debug': DEBUG_LOG
        }))
        sys.exit(1)

    debug(f"Backtest completed: {result.get('metrics', {}).get('total_trades', 'N/A')} trades")
    print(json.dumps(result))


if __name__ == '__main__':
    main()
