import os
from datetime import datetime, timedelta
from typing import List, Dict, Optional, Union
import pandas as pd
import logging
from zoneinfo import ZoneInfo

# Try to load .env file if it exists
try:
    from dotenv import load_dotenv

    load_dotenv()  # This will load variables from .env file
except ImportError:
    pass  # dotenv not installed, continue without it

from alpaca.trading.client import TradingClient
from alpaca.trading.requests import (
    MarketOrderRequest, LimitOrderRequest, StopOrderRequest,
    StopLimitOrderRequest, TrailingStopOrderRequest, GetOrdersRequest
)
from alpaca.trading.enums import OrderSide, TimeInForce, OrderType, QueryOrderStatus
from alpaca.data.historical import StockHistoricalDataClient
from alpaca.data.requests import StockBarsRequest, StockTradesRequest, StockQuotesRequest
from alpaca.data.timeframe import TimeFrame, TimeFrameUnit
from alpaca.data.live import StockDataStream

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class AlpacaWrapper:
    """Complete wrapper for Alpaca trading and data operations"""

    def __init__(self, api_key: str = None, secret_key: str = None, paper: bool = True):
        # Use environment variables for security
        self.api_key = api_key or os.getenv('ALPACA_API_KEY')
        self.secret_key = secret_key or os.getenv('ALPACA_SECRET_KEY')

        if not self.api_key or not self.secret_key:
            raise ValueError("API keys must be provided or set as environment variables")

        self.paper = paper

        # Initialize clients with error handling
        try:
            self.trading_client = TradingClient(self.api_key, self.secret_key, paper=paper)
            self.data_client = StockHistoricalDataClient(self.api_key, self.secret_key)
            self.stream = StockDataStream(self.api_key, self.secret_key)
            logger.info(f"Initialized Alpaca clients (Paper: {paper})")
        except Exception as e:
            logger.error(f"Failed to initialize Alpaca clients: {e}")
            raise

    # MARKET STATUS METHODS
    def is_market_open(self) -> bool:
        """Check if market is currently open"""
        try:
            clock = self.trading_client.get_clock()
            return clock.is_open
        except Exception as e:
            logger.error(f"Failed to get market status: {e}")
            return False

    def get_market_clock(self) -> Dict:
        """Get detailed market status"""
        try:
            clock = self.trading_client.get_clock()
            return {
                'is_open': clock.is_open,
                'timestamp': clock.timestamp,
                'next_open': clock.next_open,
                'next_close': clock.next_close
            }
        except Exception as e:
            logger.error(f"Failed to get market clock: {e}")
            return {}

    # ACCOUNT & PORTFOLIO METHODS
    def get_account(self) -> Dict:
        """Get account information"""
        try:
            account = self.trading_client.get_account()
            return {
                'equity': float(account.equity),
                'buying_power': float(account.buying_power),
                'cash': float(account.cash),
                'portfolio_value': float(account.portfolio_value),
                'day_trade_count': int(account.daytrade_count),
                'pattern_day_trader': account.pattern_day_trader
            }
        except Exception as e:
            logger.error(f"Failed to get account info: {e}")
            return {}

    def get_positions(self) -> pd.DataFrame:
        """Get all positions as DataFrame"""
        try:
            positions = self.trading_client.get_all_positions()
            if not positions:
                return pd.DataFrame()

            data = []
            for pos in positions:
                data.append({
                    'symbol': pos.symbol,
                    'qty': float(pos.qty),
                    'market_value': float(pos.market_value),
                    'cost_basis': float(pos.cost_basis),
                    'unrealized_pl': float(pos.unrealized_pl),
                    'unrealized_plpc': float(pos.unrealized_plpc),
                    'avg_entry_price': float(pos.avg_entry_price),
                    'side': pos.side
                })
            return pd.DataFrame(data)
        except Exception as e:
            logger.error(f"Failed to get positions: {e}")
            return pd.DataFrame()

    def get_position(self, symbol: str) -> Optional[Dict]:
        """Get specific position"""
        try:
            pos = self.trading_client.get_open_position(symbol)
            return {
                'symbol': pos.symbol,
                'qty': float(pos.qty),
                'market_value': float(pos.market_value),
                'avg_entry_price': float(pos.avg_entry_price),
                'unrealized_pl': float(pos.unrealized_pl),
                'unrealized_plpc': float(pos.unrealized_plpc),
                'cost_basis': float(pos.cost_basis)
            }
        except Exception as e:
            logger.warning(f"No position found for {symbol}: {e}")
            return None

    # TRADING METHODS
    def buy(self, symbol: str, qty: Union[int, float], order_type: str = 'market',
            limit_price: float = None, stop_price: float = None,
            extended_hours: bool = False) -> Optional[str]:
        """Place buy order"""
        return self._place_order(symbol, qty, OrderSide.BUY, order_type,
                                 limit_price, stop_price, extended_hours)

    def sell(self, symbol: str, qty: Union[int, float], order_type: str = 'market',
             limit_price: float = None, stop_price: float = None,
             extended_hours: bool = False) -> Optional[str]:
        """Place sell order"""
        return self._place_order(symbol, qty, OrderSide.SELL, order_type,
                                 limit_price, stop_price, extended_hours)

    def _place_order(self, symbol: str, qty: Union[int, float], side: OrderSide,
                     order_type: str, limit_price: float = None,
                     stop_price: float = None, extended_hours: bool = False) -> Optional[str]:
        """Internal order placement with improved error handling"""
        try:
            # Check market status
            if not extended_hours and not self.is_market_open():
                logger.warning("Market is closed. Use extended_hours=True for after-hours trading")

            order_type = order_type.lower()

            if order_type == 'market':
                request = MarketOrderRequest(
                    symbol=symbol,
                    qty=qty,
                    side=side,
                    time_in_force=TimeInForce.DAY,
                    extended_hours=extended_hours
                )
            elif order_type == 'limit':
                if limit_price is None:
                    raise ValueError("limit_price required for limit orders")
                request = LimitOrderRequest(
                    symbol=symbol,
                    qty=qty,
                    side=side,
                    time_in_force=TimeInForce.DAY,
                    limit_price=limit_price,
                    extended_hours=extended_hours
                )
            elif order_type == 'stop':
                if stop_price is None:
                    raise ValueError("stop_price required for stop orders")
                request = StopOrderRequest(
                    symbol=symbol,
                    qty=qty,
                    side=side,
                    time_in_force=TimeInForce.DAY,
                    stop_price=stop_price
                )
            elif order_type == 'stop_limit':
                if limit_price is None or stop_price is None:
                    raise ValueError("Both limit_price and stop_price required for stop-limit orders")
                request = StopLimitOrderRequest(
                    symbol=symbol,
                    qty=qty,
                    side=side,
                    time_in_force=TimeInForce.DAY,
                    limit_price=limit_price,
                    stop_price=stop_price
                )
            else:
                raise ValueError(f"Unsupported order type: {order_type}")

            order = self.trading_client.submit_order(request)
            logger.info(f"Order placed: {order.id} - {side.value} {qty} {symbol}")
            return order.id

        except Exception as e:
            logger.error(f"Failed to place order: {e}")
            return None

    def place_trailing_stop(self, symbol: str, qty: Union[int, float], side: OrderSide,
                            trail_percent: float = None, trail_price: float = None) -> Optional[str]:
        """Place trailing stop order"""
        try:
            if trail_percent is None and trail_price is None:
                raise ValueError("Either trail_percent or trail_price must be specified")

            request = TrailingStopOrderRequest(
                symbol=symbol,
                qty=qty,
                side=side,
                time_in_force=TimeInForce.GTC,
                trail_percent=trail_percent,
                trail_price=trail_price
            )

            order = self.trading_client.submit_order(request)
            logger.info(f"Trailing stop order placed: {order.id}")
            return order.id

        except Exception as e:
            logger.error(f"Failed to place trailing stop order: {e}")
            return None

    def cancel_order(self, order_id: str) -> bool:
        """Cancel order by ID"""
        try:
            self.trading_client.cancel_order_by_id(order_id)
            logger.info(f"Order cancelled: {order_id}")
            return True
        except Exception as e:
            logger.error(f"Failed to cancel order {order_id}: {e}")
            return False

    def cancel_all_orders(self) -> bool:
        """Cancel all open orders"""
        try:
            self.trading_client.cancel_orders()
            logger.info("All orders cancelled")
            return True
        except Exception as e:
            logger.error(f"Failed to cancel all orders: {e}")
            return False

    def get_orders(self, status: str = 'all', symbols: List[str] = None, limit: int = 100) -> pd.DataFrame:
        """Get order history with improved filtering"""
        try:
            # Map status strings to enum - FILLED doesn't exist, use CLOSED instead
            status_map = {
                'all': QueryOrderStatus.ALL,
                'open': QueryOrderStatus.OPEN,
                'closed': QueryOrderStatus.CLOSED,
                'filled': QueryOrderStatus.CLOSED,  # Use CLOSED for filled orders
                'cancelled': QueryOrderStatus.CANCELED  # Note: CANCELED not CANCELLED
            }

            req = GetOrdersRequest(
                status=status_map.get(status.lower(), QueryOrderStatus.ALL),
                symbols=symbols,
                limit=limit
            )

            orders = self.trading_client.get_orders(req)
            if not orders:
                return pd.DataFrame()

            data = []
            for order in orders:
                data.append({
                    'id': order.id,
                    'symbol': order.symbol,
                    'qty': float(order.qty),
                    'side': order.side.value,
                    'order_type': order.order_type.value,
                    'status': order.status.value,
                    'created_at': order.created_at,
                    'filled_price': float(order.filled_avg_price) if order.filled_avg_price else None,
                    'filled_qty': float(order.filled_qty) if order.filled_qty else 0,
                    'limit_price': float(order.limit_price) if order.limit_price else None,
                    'stop_price': float(order.stop_price) if order.stop_price else None
                })
            return pd.DataFrame(data)
        except Exception as e:
            logger.error(f"Failed to get orders: {e}")
            return pd.DataFrame()

    def close_position(self, symbol: str, qty: Optional[Union[int, float]] = None) -> bool:
        """Close specific position (all or partial)"""
        try:
            if qty:
                # Close partial position
                from alpaca.trading.requests import ClosePositionRequest
                self.trading_client.close_position(
                    symbol,
                    close_options=ClosePositionRequest(qty=str(qty))
                )
            else:
                # Close entire position
                self.trading_client.close_position(symbol)

            logger.info(f"Position closed for {symbol}")
            return True
        except Exception as e:
            logger.error(f"Failed to close position for {symbol}: {e}")
            return False

    def close_all_positions(self) -> bool:
        """Close all positions"""
        try:
            self.trading_client.close_all_positions(cancel_orders=True)
            logger.info("All positions closed")
            return True
        except Exception as e:
            logger.error(f"Failed to close all positions: {e}")
            return False

    # DATA METHODS
    def get_bars(self, symbols: Union[str, List[str]], timeframe: str = '1Day',
                 start: datetime = None, end: datetime = None, limit: int = 1000) -> pd.DataFrame:
        """Get historical bar data with improved timeframe handling"""
        try:
            if isinstance(symbols, str):
                symbols = [symbols]

            if start is None:
                start = datetime.now() - timedelta(days=365)

            # Enhanced timeframe mapping
            tf_map = {
                '1Min': TimeFrame(1, TimeFrameUnit.Minute),
                '5Min': TimeFrame(5, TimeFrameUnit.Minute),
                '15Min': TimeFrame(15, TimeFrameUnit.Minute),
                '30Min': TimeFrame(30, TimeFrameUnit.Minute),
                '1Hour': TimeFrame(1, TimeFrameUnit.Hour),
                '4Hour': TimeFrame(4, TimeFrameUnit.Hour),
                '1Day': TimeFrame(1, TimeFrameUnit.Day),
                '1Week': TimeFrame(1, TimeFrameUnit.Week),
                '1Month': TimeFrame(1, TimeFrameUnit.Month)
            }

            timeframe_obj = tf_map.get(timeframe)
            if timeframe_obj is None:
                raise ValueError(f"Unsupported timeframe: {timeframe}")

            request = StockBarsRequest(
                symbol_or_symbols=symbols,
                timeframe=timeframe_obj,
                start=start,
                end=end,
                limit=limit
            )

            bars = self.data_client.get_stock_bars(request)
            return bars.df

        except Exception as e:
            logger.error(f"Failed to get bars: {e}")
            return pd.DataFrame()

    def get_latest_price(self, symbol: str) -> Optional[float]:
        """Get latest price for symbol"""
        try:
            # Use the correct method without StockBarsRequest for latest bar
            bars = self.data_client.get_stock_latest_bar([symbol])
            return float(bars[symbol].close)
        except Exception as e:
            logger.error(f"Failed to get latest price for {symbol}: {e}")
            return None

    def get_latest_quote(self, symbol: str) -> Optional[Dict]:
        """Get latest bid/ask quote"""
        try:
            # Use the correct method without StockQuotesRequest for latest quote
            quote = self.data_client.get_stock_latest_quote([symbol])
            q = quote[symbol]
            return {
                'symbol': symbol,
                'bid': float(q.bid_price),
                'ask': float(q.ask_price),
                'bid_size': int(q.bid_size),
                'ask_size': int(q.ask_size),
                'timestamp': q.timestamp
            }
        except Exception as e:
            logger.error(f"Failed to get latest quote for {symbol}: {e}")
            return None

    def get_quotes(self, symbols: Union[str, List[str]],
                   start: datetime = None, limit: int = 1000) -> pd.DataFrame:
        """Get historical quote data"""
        try:
            if isinstance(symbols, str):
                symbols = [symbols]

            if start is None:
                start = datetime.now() - timedelta(days=1)

            request = StockQuotesRequest(
                symbol_or_symbols=symbols,
                start=start,
                limit=limit
            )

            quotes = self.data_client.get_stock_quotes(request)
            return quotes.df
        except Exception as e:
            logger.error(f"Failed to get quotes: {e}")
            return pd.DataFrame()

    def get_trades(self, symbols: Union[str, List[str]],
                   start: datetime = None, limit: int = 1000) -> pd.DataFrame:
        """Get historical trade data"""
        try:
            if isinstance(symbols, str):
                symbols = [symbols]

            if start is None:
                start = datetime.now() - timedelta(days=1)

            request = StockTradesRequest(
                symbol_or_symbols=symbols,
                start=start,
                limit=limit
            )

            trades = self.data_client.get_stock_trades(request)
            return trades.df
        except Exception as e:
            logger.error(f"Failed to get trades: {e}")
            return pd.DataFrame()

    # PORTFOLIO ANALYTICS
    def get_portfolio_history(self, period: str = '1M', timeframe: str = '1D') -> pd.DataFrame:
        """Get portfolio performance history"""
        try:
            history = self.trading_client.get_portfolio_history(period=period, timeframe=timeframe)

            data = {
                'timestamp': history.timestamp,
                'equity': history.equity,
                'profit_loss': history.profit_loss,
                'profit_loss_pct': history.profit_loss_pct
            }

            df = pd.DataFrame(data)
            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
            return df.set_index('timestamp')
        except Exception as e:
            logger.error(f"Failed to get portfolio history: {e}")
            return pd.DataFrame()

    # STREAMING (REAL-TIME DATA)
    def start_stream(self, symbols: List[str], on_bar=None, on_trade=None, on_quote=None):
        """Start real-time data stream"""
        try:
            if on_bar:
                self.stream.subscribe_bars(on_bar, *symbols)
            if on_trade:
                self.stream.subscribe_trades(on_trade, *symbols)
            if on_quote:
                self.stream.subscribe_quotes(on_quote, *symbols)

            logger.info(f"Starting stream for {symbols}")
            self.stream.run()
        except Exception as e:
            logger.error(f"Failed to start stream: {e}")

    def stop_stream(self):
        """Stop real-time data stream"""
        try:
            self.stream.stop()
            logger.info("Stream stopped")
        except Exception as e:
            logger.error(f"Failed to stop stream: {e}")

    # UTILITY METHODS
    def get_watchlist(self, watchlist_id: str = None) -> pd.DataFrame:
        """Get watchlist"""
        try:
            if watchlist_id:
                watchlist = self.trading_client.get_watchlist_by_id(watchlist_id)
            else:
                watchlists = self.trading_client.get_watchlists()
                if not watchlists:
                    return pd.DataFrame()
                watchlist = watchlists[0]  # Get first watchlist

            symbols = [asset.symbol for asset in watchlist.assets]
            return pd.DataFrame({'symbol': symbols})
        except Exception as e:
            logger.error(f"Failed to get watchlist: {e}")
            return pd.DataFrame()


# USAGE EXAMPLES
if __name__ == "__main__":
    # Initialize wrapper (use environment variables for API keys)
    try:
        alpaca = AlpacaWrapper(paper=True)

        # Check market status
        clock = alpaca.get_market_clock()
        print(f"Market is open: {clock.get('is_open', False)}")

        # Get account info
        account = alpaca.get_account()
        if account:
            print(f"Portfolio Value: ${account['portfolio_value']:,.2f}")
            print(f"Buying Power: ${account['buying_power']:,.2f}")

        # Get historical data
        print("\nFetching historical data...")
        bars = alpaca.get_bars(['AAPL', 'GOOGL'], timeframe='1Day', limit=5)
        if not bars.empty:
            print("Latest bars:")
            print(bars.tail())
        else:
            print("No bar data returned")

        # Get latest price (with better error handling)
        print(f"\nFetching latest price...")
        price = alpaca.get_latest_price('AAPL')
        if price:
            print(f"AAPL Price: ${price:.2f}")
        else:
            print("Could not fetch AAPL price")

        # Try getting latest quote as alternative
        quote = alpaca.get_latest_quote('AAPL')
        if quote:
            print(f"AAPL Quote - Bid: ${quote['bid']:.2f}, Ask: ${quote['ask']:.2f}")

        # Get positions
        positions = alpaca.get_positions()
        if not positions.empty:
            print("\nCurrent positions:")
            print(positions)
        else:
            print("\nNo open positions")

        # Example: Place a small buy order (commented out for safety)
        # print("\nTesting order placement...")
        # if alpaca.is_market_open():
        #     order_id = alpaca.buy('AAPL', 1, 'limit', limit_price=200.0)
        #     if order_id:
        #         print(f"Order placed: {order_id}")
        # else:
        #     print("Market is closed - no order placed")

        # Get order history
        print("\nFetching order history...")
        orders = alpaca.get_orders(status='all', limit=5)
        if not orders.empty:
            print("Recent orders:")
            print(orders[['symbol', 'side', 'qty', 'status', 'created_at']])
        else:
            print("No recent orders found")

    except Exception as e:
        print(f"Error: {e}")
        print("Make sure to set ALPACA_API_KEY and ALPACA_SECRET_KEY environment variables")