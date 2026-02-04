#!/usr/bin/env python3
"""
Fincept Terminal - Universal Strategy Runner
Executes strategies from the Fincept Engine registry across all backtesting providers.

Supports: VectorBT, Backtesting.py, Fast-Trade, Zipline, BT
"""

import sys
import json
import os
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional

# Add strategies directory to path
STRATEGIES_DIR = Path(__file__).resolve().parent.parent.parent.parent / "strategies"
sys.path.insert(0, str(STRATEGIES_DIR))

# Import Fincept Engine
from fincept_engine import QCAlgorithm, Symbol, TradeBar, Slice
from fincept_engine.enums import Resolution, SecurityType
from strategies._registry import STRATEGY_REGISTRY


class FinceptStrategyRunner:
    """
    Universal runner for Fincept Terminal strategies.
    Loads strategies from _registry.py and executes them with historical data.
    """

    def __init__(self):
        self.strategy_registry = STRATEGY_REGISTRY
        self.strategies_dir = STRATEGIES_DIR

    def get_strategy_info(self, strategy_id: str) -> Optional[Dict[str, str]]:
        """Get strategy metadata from registry."""
        return self.strategy_registry.get(strategy_id)

    def load_strategy_class(self, strategy_id: str):
        """Dynamically load strategy class from file."""
        info = self.get_strategy_info(strategy_id)
        if not info:
            raise ValueError(f"Strategy {strategy_id} not found in registry")

        strategy_path = self.strategies_dir / info['path']
        if not strategy_path.exists():
            raise FileNotFoundError(f"Strategy file not found: {strategy_path}")

        # Read and execute strategy file
        with open(strategy_path, 'r', encoding='utf-8') as f:
            code = f.read()

        # Create namespace for execution
        namespace = {
            'QCAlgorithm': QCAlgorithm,
            'Symbol': Symbol,
            'TradeBar': TradeBar,
            'Slice': Slice,
            'Resolution': Resolution,
            'SecurityType': SecurityType,
        }

        # Execute strategy file
        exec(code, namespace)

        # Find the algorithm class (inherits from QCAlgorithm)
        strategy_class = None
        for name, obj in namespace.items():
            if (isinstance(obj, type) and
                issubclass(obj, QCAlgorithm) and
                obj is not QCAlgorithm):
                strategy_class = obj
                break

        if not strategy_class:
            raise ValueError(f"No QCAlgorithm subclass found in {info['path']}")

        return strategy_class

    def fetch_historical_data(self, symbols: List[str], start_date: str,
                             end_date: str, resolution: str = 'daily') -> Dict[str, List[Dict]]:
        """
        Fetch historical data for backtesting.
        Uses yfinance for simplicity. Can be extended to CCXT, custom data, etc.
        """
        try:
            import yfinance as yf
        except ImportError:
            raise ImportError("yfinance not installed. Install: pip install yfinance")

        historical_data = {}

        for symbol in symbols:
            try:
                ticker = yf.Ticker(symbol)
                interval = '1d' if resolution == 'daily' else '1h'
                df = ticker.history(start=start_date, end=end_date, interval=interval)

                if df.empty:
                    print(f"Warning: No data for {symbol}", file=sys.stderr)
                    continue

                # Convert to list of dicts
                bars = []
                for idx, row in df.iterrows():
                    bars.append({
                        'time': idx.strftime('%Y-%m-%d %H:%M:%S'),
                        'open': float(row['Open']),
                        'high': float(row['High']),
                        'low': float(row['Low']),
                        'close': float(row['Close']),
                        'volume': float(row['Volume'])
                    })

                historical_data[symbol] = bars

            except Exception as e:
                print(f"Error fetching data for {symbol}: {e}", file=sys.stderr)

        return historical_data

    def execute_strategy(self, strategy_id: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute a strategy with given parameters.

        Args:
            strategy_id: Strategy ID from registry (e.g., "FCT-C45FB406")
            params: Backtest parameters
                - symbols: List[str] - Tickers to trade
                - start_date: str - Start date (YYYY-MM-DD)
                - end_date: str - End date (YYYY-MM-DD)
                - initial_cash: float - Starting capital
                - resolution: str - Data resolution (daily/hourly)
                - strategy_params: Dict - Strategy-specific parameters

        Returns:
            Dict with performance metrics, trades, and equity curve
        """
        try:
            # Extract parameters (no defaults - require explicit values)
            symbols = params.get('symbols')
            start_date = params.get('start_date')
            end_date = params.get('end_date')
            initial_cash = params.get('initial_cash')
            resolution = params.get('resolution', 'daily')
            strategy_params = params.get('strategy_params', {})

            # Validate required parameters
            if not symbols:
                raise ValueError("symbols parameter is required")
            if not start_date:
                raise ValueError("start_date parameter is required")
            if not end_date:
                raise ValueError("end_date parameter is required")
            if initial_cash is None:
                raise ValueError("initial_cash parameter is required")

            # Load strategy class
            strategy_class = self.load_strategy_class(strategy_id)

            # Instantiate strategy
            algorithm = strategy_class()

            # Set initial cash
            algorithm.set_cash(initial_cash)

            # Initialize strategy (call Initialize method)
            if hasattr(algorithm, 'initialize'):
                algorithm.initialize()
            elif hasattr(algorithm, 'Initialize'):
                algorithm.Initialize()

            # Fetch historical data
            print(f"Fetching data for {symbols}...", file=sys.stderr)
            historical_data = self.fetch_historical_data(symbols, start_date, end_date, resolution)

            if not historical_data:
                raise ValueError("No historical data fetched")

            # Run backtest (feed data bar by bar)
            print(f"Running backtest for {strategy_id}...", file=sys.stderr)
            equity_curve = []
            trades = []

            # Get all timestamps across all symbols
            all_timestamps = set()
            for symbol_data in historical_data.values():
                for bar in symbol_data:
                    all_timestamps.add(bar['time'])

            timestamps = sorted(all_timestamps)

            for timestamp_str in timestamps:
                timestamp = datetime.strptime(timestamp_str, '%Y-%m-%d %H:%M:%S')

                # Create Slice for this timestamp
                slice_data = Slice(timestamp)

                for symbol_str, bars in historical_data.items():
                    # Find bar for this timestamp
                    bar_data = next((b for b in bars if b['time'] == timestamp_str), None)
                    if bar_data:
                        symbol = Symbol.create(symbol_str)
                        bar = TradeBar(
                            time=timestamp,
                            symbol=symbol,
                            open=bar_data['open'],
                            high=bar_data['high'],
                            low=bar_data['low'],
                            close=bar_data['close'],
                            volume=bar_data['volume']
                        )
                        slice_data.add(symbol_str, bar)

                # Update securities with current prices
                for symbol_str in slice_data._data.keys():
                    if symbol_str in algorithm.securities:
                        price = slice_data[symbol_str].close
                        algorithm.securities[symbol_str].update_price(price)

                # Call OnData
                if hasattr(algorithm, 'on_data'):
                    algorithm.on_data(slice_data)
                elif hasattr(algorithm, 'OnData'):
                    algorithm.OnData(slice_data)

                # Record equity
                portfolio_value = algorithm.portfolio.total_portfolio_value
                equity_curve.append({
                    'time': timestamp_str,
                    'equity': portfolio_value
                })

            # Calculate performance metrics
            final_equity = equity_curve[-1]['equity'] if equity_curve else initial_cash
            total_return = ((final_equity - initial_cash) / initial_cash) * 100

            # Extract trades from algorithm
            for order_id, order in algorithm.transactions._orders.items():
                if order.status.name == 'FILLED':
                    trades.append({
                        'time': order.time.strftime('%Y-%m-%d %H:%M:%S'),
                        'symbol': str(order.symbol),
                        'quantity': order.quantity,
                        'price': order.average_fill_price,
                        'type': order.order_type.name
                    })

            # Return results
            return {
                'success': True,
                'data': {
                    'performance': {
                        'total_return': total_return,
                        'final_equity': final_equity,
                        'initial_cash': initial_cash,
                        'total_trades': len(trades),
                        'strategy_id': strategy_id,
                        'strategy_name': self.get_strategy_info(strategy_id)['name']
                    },
                    'trades': trades,
                    'equity': equity_curve
                }
            }

        except Exception as e:
            import traceback
            return {
                'success': False,
                'error': str(e),
                'traceback': traceback.format_exc()
            }

    def list_strategies(self) -> List[Dict[str, str]]:
        """List all available strategies from registry."""
        strategies = []
        for strategy_id, info in self.strategy_registry.items():
            strategies.append({
                'id': strategy_id,
                'name': info['name'],
                'category': info['category'],
                'path': info['path']
            })
        return strategies


def main():
    """CLI entry point for testing."""
    if len(sys.argv) < 2:
        print("Usage: python fincept_strategy_runner.py <command> [args]")
        print("Commands:")
        print("  list - List all strategies")
        print("  run <strategy_id> <params_json> - Run a strategy")
        sys.exit(1)

    runner = FinceptStrategyRunner()
    command = sys.argv[1]

    if command == 'list':
        strategies = runner.list_strategies()
        print(json.dumps({'success': True, 'data': strategies, 'count': len(strategies)}, indent=2))

    elif command == 'run':
        if len(sys.argv) < 4:
            print("Usage: python fincept_strategy_runner.py run <strategy_id> <params_json>")
            sys.exit(1)

        strategy_id = sys.argv[2]
        params = json.loads(sys.argv[3])

        result = runner.execute_strategy(strategy_id, params)
        print(json.dumps(result, indent=2))

    else:
        print(f"Unknown command: {command}")
        sys.exit(1)


if __name__ == '__main__':
    main()
