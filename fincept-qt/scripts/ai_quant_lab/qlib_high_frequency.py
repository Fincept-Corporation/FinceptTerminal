"""
AI Quant Lab - High Frequency Trading Module
HFT operations: tick data processing, microstructure analysis, latency optimization
Order book dynamics, market making strategies, and ultra-low latency execution
"""

import json
import sys
import os
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional, Union, Tuple
from collections import deque
import warnings
warnings.filterwarnings('ignore')

import numpy as np
import pandas as pd

# Qlib high-frequency imports
HFT_AVAILABLE = False
HFT_ERROR = None

try:
    import qlib
    from qlib.contrib.ops.high_freq import *
    from qlib.data import D
    from qlib.config import REG_CN, REG_US
    HFT_AVAILABLE = True
except ImportError as e:
    HFT_ERROR = str(e)


class OrderBook:
    """
    Order Book simulator for high-frequency trading
    Maintains bid/ask levels and provides microstructure features
    """

    def __init__(self, depth: int = 10):
        self.depth = depth
        self.bids = {}  # {price: volume}
        self.asks = {}  # {price: volume}
        self.trades = deque(maxlen=1000)
        self.timestamp = None

    def update(self, bids: Dict[float, float], asks: Dict[float, float], timestamp: datetime):
        """Update order book with new levels"""
        self.bids = bids
        self.asks = asks
        self.timestamp = timestamp

    def add_trade(self, price: float, volume: float, side: str, timestamp: datetime):
        """Record a trade"""
        self.trades.append({
            'price': price,
            'volume': volume,
            'side': side,
            'timestamp': timestamp
        })

    def get_best_bid(self) -> Optional[Tuple[float, float]]:
        """Get best bid (highest buy price)"""
        if not self.bids:
            return None
        best_price = max(self.bids.keys())
        return best_price, self.bids[best_price]

    def get_best_ask(self) -> Optional[Tuple[float, float]]:
        """Get best ask (lowest sell price)"""
        if not self.asks:
            return None
        best_price = min(self.asks.keys())
        return best_price, self.asks[best_price]

    def get_spread(self) -> Optional[float]:
        """Get bid-ask spread"""
        best_bid = self.get_best_bid()
        best_ask = self.get_best_ask()
        if best_bid and best_ask:
            return best_ask[0] - best_bid[0]
        return None

    def get_mid_price(self) -> Optional[float]:
        """Get mid price"""
        best_bid = self.get_best_bid()
        best_ask = self.get_best_ask()
        if best_bid and best_ask:
            return (best_bid[0] + best_ask[0]) / 2
        return None

    def get_vwap(self, n_levels: int = 5) -> Dict[str, float]:
        """Volume-weighted average price for top N levels"""
        bid_prices = sorted(self.bids.keys(), reverse=True)[:n_levels]
        ask_prices = sorted(self.asks.keys())[:n_levels]

        bid_vwap = sum(p * self.bids[p] for p in bid_prices) / sum(self.bids[p] for p in bid_prices) if bid_prices else 0
        ask_vwap = sum(p * self.asks[p] for p in ask_prices) / sum(self.asks[p] for p in ask_prices) if ask_prices else 0

        return {'bid_vwap': bid_vwap, 'ask_vwap': ask_vwap}

    def get_depth_imbalance(self, n_levels: int = 5) -> float:
        """
        Order book imbalance
        Positive = more buy pressure, Negative = more sell pressure
        """
        bid_prices = sorted(self.bids.keys(), reverse=True)[:n_levels]
        ask_prices = sorted(self.asks.keys())[:n_levels]

        total_bid_volume = sum(self.bids[p] for p in bid_prices)
        total_ask_volume = sum(self.asks[p] for p in ask_prices)

        if total_bid_volume + total_ask_volume == 0:
            return 0.0

        return (total_bid_volume - total_ask_volume) / (total_bid_volume + total_ask_volume)

    def get_microstructure_features(self) -> Dict[str, float]:
        """Calculate microstructure features"""
        features = {
            'spread': self.get_spread() or 0.0,
            'mid_price': self.get_mid_price() or 0.0,
            'depth_imbalance': self.get_depth_imbalance(),
            'total_bid_volume': sum(self.bids.values()),
            'total_ask_volume': sum(self.asks.values()),
        }

        # VWAP features
        vwap = self.get_vwap()
        features.update(vwap)

        # Recent trade features
        if len(self.trades) > 0:
            recent_trades = list(self.trades)[-10:]
            features['recent_trade_price_mean'] = np.mean([t['price'] for t in recent_trades])
            features['recent_trade_price_std'] = np.std([t['price'] for t in recent_trades])
            features['recent_trade_volume_sum'] = sum(t['volume'] for t in recent_trades)

        return features


class HighFrequencyTrader:
    """
    High Frequency Trading System
    Implements HFT strategies, tick data processing, and market making
    """

    def __init__(self):
        self.qlib_initialized = False
        self.order_books = {}
        self.tick_data = {}
        self.strategies = {}
        self.positions = {}
        self.pnl = 0.0

    def initialize(self,
                  provider_uri: str = "~/.qlib/qlib_data/cn_data",
                  region: str = "cn") -> Dict[str, Any]:
        """Initialize Qlib for HFT"""
        try:
            if region == "cn":
                qlib.init(provider_uri=provider_uri, region=REG_CN)
            else:
                qlib.init(provider_uri=provider_uri, region=REG_US)

            self.qlib_initialized = True
            return {
                'success': True,
                'message': 'Qlib initialized for HFT',
                'hft_module_available': HFT_AVAILABLE
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def create_order_book(self,
                         symbol: str,
                         depth: int = 10) -> Dict[str, Any]:
        """Create order book for a symbol"""
        try:
            self.order_books[symbol] = OrderBook(depth=depth)
            self.positions[symbol] = 0.0

            return {
                'success': True,
                'symbol': symbol,
                'depth': depth,
                'message': f'Order book created for {symbol}'
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def update_order_book(self,
                         symbol: str,
                         bids: Dict[float, float],
                         asks: Dict[float, float],
                         timestamp: Optional[datetime] = None) -> Dict[str, Any]:
        """Update order book with new data"""
        if symbol not in self.order_books:
            return {'success': False, 'error': f'Order book for {symbol} not found'}

        try:
            timestamp = timestamp or datetime.now()
            self.order_books[symbol].update(bids, asks, timestamp)

            features = self.order_books[symbol].get_microstructure_features()

            return {
                'success': True,
                'symbol': symbol,
                'timestamp': timestamp.isoformat(),
                'features': features
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def add_trade(self,
                 symbol: str,
                 price: float,
                 volume: float,
                 side: str,
                 timestamp: Optional[datetime] = None) -> Dict[str, Any]:
        """Record a trade"""
        if symbol not in self.order_books:
            return {'success': False, 'error': f'Order book for {symbol} not found'}

        try:
            timestamp = timestamp or datetime.now()
            self.order_books[symbol].add_trade(price, volume, side, timestamp)

            return {
                'success': True,
                'symbol': symbol,
                'trade': {
                    'price': price,
                    'volume': volume,
                    'side': side,
                    'timestamp': timestamp.isoformat()
                }
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def calculate_market_making_quotes(self,
                                      symbol: str,
                                      inventory: float = 0.0,
                                      risk_aversion: float = 0.01,
                                      spread_multiplier: float = 1.5) -> Dict[str, Any]:
        """
        Calculate market making bid/ask quotes
        Uses Avellaneda-Stoikov model for optimal quoting
        """
        if symbol not in self.order_books:
            return {'success': False, 'error': f'Order book for {symbol} not found'}

        try:
            order_book = self.order_books[symbol]
            mid_price = order_book.get_mid_price()
            spread = order_book.get_spread()

            if mid_price is None or spread is None:
                return {'success': False, 'error': 'Insufficient market data'}

            # Adjust for inventory (inventory risk)
            inventory_adjustment = risk_aversion * inventory * spread

            # Calculate optimal bid/ask
            optimal_spread = spread * spread_multiplier
            bid_price = mid_price - (optimal_spread / 2) - inventory_adjustment
            ask_price = mid_price + (optimal_spread / 2) - inventory_adjustment

            # Calculate quote size based on market depth
            depth_imbalance = order_book.get_depth_imbalance()
            base_size = 100  # Base order size

            bid_size = base_size * (1 + depth_imbalance) if depth_imbalance > 0 else base_size
            ask_size = base_size * (1 - depth_imbalance) if depth_imbalance < 0 else base_size

            return {
                'success': True,
                'symbol': symbol,
                'mid_price': mid_price,
                'spread': spread,
                'quotes': {
                    'bid': {'price': round(bid_price, 2), 'size': int(bid_size)},
                    'ask': {'price': round(ask_price, 2), 'size': int(ask_size)}
                },
                'inventory': inventory,
                'depth_imbalance': depth_imbalance
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def detect_toxic_flow(self,
                         symbol: str,
                         window_size: int = 100) -> Dict[str, Any]:
        """
        Detect informed trading / toxic order flow
        High volume imbalance + price movement suggests informed traders
        """
        if symbol not in self.order_books:
            return {'success': False, 'error': f'Order book for {symbol} not found'}

        try:
            order_book = self.order_books[symbol]

            # Analyze recent trades
            if len(order_book.trades) < window_size:
                return {'success': False, 'error': 'Insufficient trade history'}

            recent_trades = list(order_book.trades)[-window_size:]

            buy_volume = sum(t['volume'] for t in recent_trades if t['side'] == 'buy')
            sell_volume = sum(t['volume'] for t in recent_trades if t['side'] == 'sell')

            volume_imbalance = (buy_volume - sell_volume) / (buy_volume + sell_volume) if (buy_volume + sell_volume) > 0 else 0

            # Price momentum
            prices = [t['price'] for t in recent_trades]
            price_change = (prices[-1] - prices[0]) / prices[0] if prices[0] != 0 else 0

            # Toxicity score: high when volume imbalance aligns with price movement
            toxicity_score = abs(volume_imbalance) * abs(price_change) * 100

            is_toxic = toxicity_score > 0.5  # Threshold for toxic flow

            return {
                'success': True,
                'symbol': symbol,
                'toxicity_score': float(toxicity_score),
                'is_toxic': is_toxic,
                'volume_imbalance': float(volume_imbalance),
                'price_change': float(price_change),
                'action': 'widen_spreads' if is_toxic else 'normal'
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def execute_aggressive_order(self,
                                symbol: str,
                                side: str,
                                size: float,
                                max_slippage: float = 0.001) -> Dict[str, Any]:
        """
        Execute aggressive order by crossing the spread
        Implements smart order routing with slippage control
        """
        if symbol not in self.order_books:
            return {'success': False, 'error': f'Order book for {symbol} not found'}

        try:
            order_book = self.order_books[symbol]

            if side == 'buy':
                best_ask = order_book.get_best_ask()
                if not best_ask:
                    return {'success': False, 'error': 'No asks available'}

                execution_price = best_ask[0]
                available_size = best_ask[1]

            else:  # sell
                best_bid = order_book.get_best_bid()
                if not best_bid:
                    return {'success': False, 'error': 'No bids available'}

                execution_price = best_bid[0]
                available_size = best_bid[1]

            # Check slippage
            mid_price = order_book.get_mid_price()
            slippage = abs(execution_price - mid_price) / mid_price if mid_price else 0

            if slippage > max_slippage:
                return {
                    'success': False,
                    'error': f'Slippage {slippage:.4f} exceeds maximum {max_slippage}'
                }

            # Execute
            executed_size = min(size, available_size)
            self.positions[symbol] += executed_size if side == 'buy' else -executed_size

            return {
                'success': True,
                'symbol': symbol,
                'side': side,
                'executed_price': execution_price,
                'executed_size': executed_size,
                'slippage': float(slippage),
                'position': self.positions[symbol]
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_latency_stats(self) -> Dict[str, Any]:
        """Get latency statistics (simulated for now)"""
        return {
            'success': True,
            'latency_stats': {
                'mean_latency_us': 150,  # microseconds
                'p50_latency_us': 120,
                'p99_latency_us': 300,
                'p999_latency_us': 500,
                'max_latency_us': 1000
            },
            'message': 'Latency stats (simulated)'
        }

    def get_order_book_snapshot(self, symbol: str, n_levels: int = 5) -> Dict[str, Any]:
        """Get order book snapshot"""
        if symbol not in self.order_books:
            return {'success': False, 'error': f'Order book for {symbol} not found'}

        try:
            order_book = self.order_books[symbol]

            bid_prices = sorted(order_book.bids.keys(), reverse=True)[:n_levels]
            ask_prices = sorted(order_book.asks.keys())[:n_levels]

            bids = [{'price': p, 'size': order_book.bids[p]} for p in bid_prices]
            asks = [{'price': p, 'size': order_book.asks[p]} for p in ask_prices]

            return {
                'success': True,
                'symbol': symbol,
                'timestamp': order_book.timestamp.isoformat() if order_book.timestamp else None,
                'bids': bids,
                'asks': asks,
                'spread': order_book.get_spread(),
                'mid_price': order_book.get_mid_price()
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            'success': False,
            'error': 'Usage: python qlib_high_frequency.py <command> [args...]'
        }))
        return

    command = sys.argv[1]
    trader = HighFrequencyTrader()

    if command == 'initialize':
        provider_uri = sys.argv[2] if len(sys.argv) > 2 else "~/.qlib/qlib_data/cn_data"
        region = sys.argv[3] if len(sys.argv) > 3 else "cn"
        result = trader.initialize(provider_uri, region)

    elif command == 'create_orderbook':
        symbol = sys.argv[2]
        depth = int(sys.argv[3]) if len(sys.argv) > 3 else 10
        result = trader.create_order_book(symbol, depth)

    elif command == 'update_orderbook':
        params = json.loads(sys.argv[2])
        result = trader.update_order_book(**params)

    elif command == 'market_making_quotes':
        params = json.loads(sys.argv[2])
        result = trader.calculate_market_making_quotes(**params)

    elif command == 'detect_toxic':
        symbol = sys.argv[2]
        result = trader.detect_toxic_flow(symbol)

    elif command == 'execute_order':
        params = json.loads(sys.argv[2])
        result = trader.execute_aggressive_order(**params)

    elif command == 'latency_stats':
        result = trader.get_latency_stats()

    elif command == 'snapshot':
        symbol = sys.argv[2]
        n_levels = int(sys.argv[3]) if len(sys.argv) > 3 else 5
        result = trader.get_order_book_snapshot(symbol, n_levels)

    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
