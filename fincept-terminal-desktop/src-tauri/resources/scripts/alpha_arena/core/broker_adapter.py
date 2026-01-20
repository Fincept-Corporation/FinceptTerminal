"""
Broker Adapter for Alpha Arena

Provides unified access to multiple exchange adapters via Tauri commands.
Extends market data capabilities to support live trading when enabled.
"""

import asyncio
from datetime import datetime
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
from enum import Enum

try:
    import httpx
    HTTPX_AVAILABLE = True
except ImportError:
    HTTPX_AVAILABLE = False

from alpha_arena.types.models import MarketData, TradeAction
from alpha_arena.utils.logging import get_logger

logger = get_logger("broker_adapter")


class BrokerType(str, Enum):
    """Supported broker types."""
    CRYPTO = "crypto"
    STOCKS = "stocks"
    FOREX = "forex"


class BrokerRegion(str, Enum):
    """Broker region classification."""
    GLOBAL = "global"
    US = "us"
    INDIA = "india"
    EUROPE = "europe"
    ASIA = "asia"


@dataclass
class BrokerInfo:
    """Broker metadata."""
    id: str
    name: str
    display_name: str
    broker_type: BrokerType
    region: BrokerRegion
    supports_spot: bool = True
    supports_margin: bool = False
    supports_futures: bool = False
    supports_options: bool = False
    max_leverage: float = 1.0
    default_symbols: List[str] = field(default_factory=list)
    maker_fee: float = 0.001
    taker_fee: float = 0.001
    websocket_enabled: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "display_name": self.display_name,
            "broker_type": self.broker_type.value,
            "region": self.region.value,
            "supports_spot": self.supports_spot,
            "supports_margin": self.supports_margin,
            "supports_futures": self.supports_futures,
            "supports_options": self.supports_options,
            "max_leverage": self.max_leverage,
            "default_symbols": self.default_symbols,
            "maker_fee": self.maker_fee,
            "taker_fee": self.taker_fee,
            "websocket_enabled": self.websocket_enabled,
        }


@dataclass
class OrderRequest:
    """Order request for execution."""
    symbol: str
    side: TradeAction  # BUY, SELL
    quantity: float
    order_type: str = "market"  # market, limit, stop, stop_limit
    price: Optional[float] = None  # For limit orders
    stop_price: Optional[float] = None  # For stop orders
    time_in_force: str = "gtc"  # gtc, ioc, fok


@dataclass
class OrderResult:
    """Order execution result."""
    success: bool
    order_id: Optional[str] = None
    status: str = ""  # open, filled, partial, cancelled, rejected
    filled_quantity: float = 0.0
    filled_price: float = 0.0
    fee: float = 0.0
    error: Optional[str] = None
    timestamp: datetime = field(default_factory=datetime.now)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": self.success,
            "order_id": self.order_id,
            "status": self.status,
            "filled_quantity": self.filled_quantity,
            "filled_price": self.filled_price,
            "fee": self.fee,
            "error": self.error,
            "timestamp": self.timestamp.isoformat(),
        }


# =============================================================================
# BROKER REGISTRY
# =============================================================================

SUPPORTED_BROKERS: Dict[str, BrokerInfo] = {
    # Crypto exchanges
    "kraken": BrokerInfo(
        id="kraken",
        name="kraken",
        display_name="Kraken",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        max_leverage=5.0,
        default_symbols=["BTC/USD", "ETH/USD", "SOL/USD", "XRP/USD"],
        maker_fee=0.0016,
        taker_fee=0.0026,
        websocket_enabled=True,
    ),
    "binance": BrokerInfo(
        id="binance",
        name="binance",
        display_name="Binance",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=125.0,
        default_symbols=["BTC/USDT", "ETH/USDT", "BNB/USDT", "SOL/USDT"],
        maker_fee=0.001,
        taker_fee=0.001,
        websocket_enabled=True,
    ),
    "okx": BrokerInfo(
        id="okx",
        name="okx",
        display_name="OKX",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=125.0,
        default_symbols=["BTC/USDT", "ETH/USDT", "SOL/USDT"],
        maker_fee=0.0008,
        taker_fee=0.001,
        websocket_enabled=True,
    ),
    "coinbase": BrokerInfo(
        id="coinbase",
        name="coinbase",
        display_name="Coinbase",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.US,
        supports_margin=False,
        max_leverage=1.0,
        default_symbols=["BTC/USD", "ETH/USD", "SOL/USD"],
        maker_fee=0.004,
        taker_fee=0.006,
        websocket_enabled=True,
    ),
    "bybit": BrokerInfo(
        id="bybit",
        name="bybit",
        display_name="Bybit",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=100.0,
        default_symbols=["BTC/USDT", "ETH/USDT", "SOL/USDT"],
        maker_fee=0.001,
        taker_fee=0.001,
        websocket_enabled=True,
    ),
    "kucoin": BrokerInfo(
        id="kucoin",
        name="kucoin",
        display_name="KuCoin",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        max_leverage=100.0,
        default_symbols=["BTC/USDT", "ETH/USDT", "SOL/USDT"],
        maker_fee=0.001,
        taker_fee=0.001,
        websocket_enabled=True,
    ),
    "gateio": BrokerInfo(
        id="gateio",
        name="gateio",
        display_name="Gate.io",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=100.0,
        default_symbols=["BTC/USDT", "ETH/USDT", "SOL/USDT"],
        maker_fee=0.002,
        taker_fee=0.002,
        websocket_enabled=True,
    ),
    "hyperliquid": BrokerInfo(
        id="hyperliquid",
        name="hyperliquid",
        display_name="HyperLiquid",
        broker_type=BrokerType.CRYPTO,
        region=BrokerRegion.GLOBAL,
        supports_margin=False,
        supports_futures=True,
        max_leverage=50.0,
        default_symbols=["BTC/USD", "ETH/USD", "SOL/USD"],
        maker_fee=0.00015,
        taker_fee=0.00035,
        websocket_enabled=True,
    ),

    # Indian stock brokers
    "zerodha": BrokerInfo(
        id="zerodha",
        name="zerodha",
        display_name="Zerodha (Kite)",
        broker_type=BrokerType.STOCKS,
        region=BrokerRegion.INDIA,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=5.0,
        default_symbols=["RELIANCE", "TCS", "HDFCBANK", "INFY"],
        maker_fee=0.0,
        taker_fee=0.0003,
        websocket_enabled=True,
    ),
    "fyers": BrokerInfo(
        id="fyers",
        name="fyers",
        display_name="Fyers",
        broker_type=BrokerType.STOCKS,
        region=BrokerRegion.INDIA,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=5.0,
        default_symbols=["RELIANCE", "TCS", "HDFCBANK", "INFY"],
        maker_fee=0.0,
        taker_fee=0.0003,
        websocket_enabled=True,
    ),

    # US stock brokers
    "alpaca": BrokerInfo(
        id="alpaca",
        name="alpaca",
        display_name="Alpaca",
        broker_type=BrokerType.STOCKS,
        region=BrokerRegion.US,
        supports_margin=True,
        supports_futures=False,
        max_leverage=4.0,
        default_symbols=["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"],
        maker_fee=0.0,
        taker_fee=0.0,
        websocket_enabled=True,
    ),
    "ibkr": BrokerInfo(
        id="ibkr",
        name="ibkr",
        display_name="Interactive Brokers",
        broker_type=BrokerType.STOCKS,
        region=BrokerRegion.GLOBAL,
        supports_margin=True,
        supports_futures=True,
        supports_options=True,
        max_leverage=4.0,
        default_symbols=["AAPL", "MSFT", "GOOGL", "AMZN"],
        maker_fee=0.0,
        taker_fee=0.0005,
        websocket_enabled=True,
    ),
    "tradier": BrokerInfo(
        id="tradier",
        name="tradier",
        display_name="Tradier",
        broker_type=BrokerType.STOCKS,
        region=BrokerRegion.US,
        supports_margin=True,
        supports_options=True,
        max_leverage=2.0,
        default_symbols=["AAPL", "MSFT", "GOOGL", "AMZN"],
        maker_fee=0.0,
        taker_fee=0.0,
        websocket_enabled=True,
    ),
}


# =============================================================================
# MULTI-EXCHANGE MARKET DATA
# =============================================================================

class MultiExchangeDataProvider:
    """
    Market data provider supporting multiple exchanges.

    Extends Alpha Arena's market data capabilities.
    """

    def __init__(self, default_exchange: str = "kraken"):
        self.default_exchange = default_exchange
        self._client: Optional[Any] = None
        self._cache: Dict[str, MarketData] = {}
        self._cache_ttl = 5
        self._initialized = False

    async def initialize(self):
        """Initialize HTTP client."""
        if HTTPX_AVAILABLE:
            self._client = httpx.AsyncClient(timeout=10.0)
        self._initialized = True
        logger.info(f"Multi-exchange provider initialized (default: {self.default_exchange})")

    async def close(self):
        """Close HTTP client."""
        if self._client:
            await self._client.aclose()
            self._client = None

    async def get_ticker(
        self,
        symbol: str,
        exchange: Optional[str] = None,
    ) -> MarketData:
        """
        Get ticker data from specified exchange.

        Args:
            symbol: Trading pair
            exchange: Exchange ID (defaults to self.default_exchange)

        Returns:
            MarketData
        """
        exchange = exchange or self.default_exchange
        cache_key = f"{exchange}:{symbol}"

        # Check cache
        if cache_key in self._cache:
            cached = self._cache[cache_key]
            age = (datetime.now() - cached.timestamp).total_seconds()
            if age < self._cache_ttl:
                return cached

        # Fetch based on exchange
        try:
            if exchange == "kraken":
                data = await self._fetch_kraken(symbol)
            elif exchange == "binance":
                data = await self._fetch_binance(symbol)
            elif exchange == "okx":
                data = await self._fetch_okx(symbol)
            elif exchange == "coinbase":
                data = await self._fetch_coinbase(symbol)
            elif exchange == "bybit":
                data = await self._fetch_bybit(symbol)
            elif exchange == "kucoin":
                data = await self._fetch_kucoin(symbol)
            elif exchange == "gateio":
                data = await self._fetch_gateio(symbol)
            elif exchange == "hyperliquid":
                data = await self._fetch_hyperliquid(symbol)
            else:
                # For stock brokers, use mock data (live would require Rust bridge)
                logger.info(f"Using mock data for {exchange}:{symbol} (stock broker or unsupported)")
                data = self._mock_data(symbol)

            self._cache[cache_key] = data
            return data

        except Exception as e:
            logger.error(f"Error fetching {symbol} from {exchange}: {e}")
            return self._mock_data(symbol)

    async def _fetch_kraken(self, symbol: str) -> MarketData:
        """Fetch from Kraken API."""
        pair = symbol.replace("/", "")
        if pair.endswith("USD"):
            pair = pair.replace("USD", "ZUSD")
        if pair.startswith("BTC"):
            pair = pair.replace("BTC", "XBT")

        url = f"https://api.kraken.com/0/public/Ticker?pair={pair}"

        if not self._client:
            return self._mock_data(symbol)

        response = await self._client.get(url)
        data = response.json()

        if data.get("error"):
            return self._mock_data(symbol)

        result = list(data.get("result", {}).values())[0]

        return MarketData(
            symbol=symbol,
            price=float(result["c"][0]),
            bid=float(result["b"][0]),
            ask=float(result["a"][0]),
            high_24h=float(result["h"][1]),
            low_24h=float(result["l"][1]),
            volume_24h=float(result["v"][1]),
            timestamp=datetime.now(),
        )

    async def _fetch_binance(self, symbol: str) -> MarketData:
        """Fetch from Binance API."""
        pair = symbol.replace("/", "")
        url = f"https://api.binance.com/api/v3/ticker/24hr?symbol={pair}"

        if not self._client:
            return self._mock_data(symbol)

        response = await self._client.get(url)
        data = response.json()

        return MarketData(
            symbol=symbol,
            price=float(data["lastPrice"]),
            bid=float(data["bidPrice"]),
            ask=float(data["askPrice"]),
            high_24h=float(data["highPrice"]),
            low_24h=float(data["lowPrice"]),
            volume_24h=float(data["volume"]),
            timestamp=datetime.now(),
        )

    async def _fetch_okx(self, symbol: str) -> MarketData:
        """Fetch from OKX API."""
        pair = symbol.replace("/", "-")
        url = f"https://www.okx.com/api/v5/market/ticker?instId={pair}"

        if not self._client:
            return self._mock_data(symbol)

        response = await self._client.get(url)
        data = response.json()

        if data.get("code") != "0" or not data.get("data"):
            return self._mock_data(symbol)

        ticker = data["data"][0]

        return MarketData(
            symbol=symbol,
            price=float(ticker["last"]),
            bid=float(ticker["bidPx"]),
            ask=float(ticker["askPx"]),
            high_24h=float(ticker["high24h"]),
            low_24h=float(ticker["low24h"]),
            volume_24h=float(ticker["vol24h"]),
            timestamp=datetime.now(),
        )

    async def _fetch_coinbase(self, symbol: str) -> MarketData:
        """Fetch from Coinbase API."""
        pair = symbol.replace("/", "-")
        url = f"https://api.exchange.coinbase.com/products/{pair}/ticker"

        if not self._client:
            return self._mock_data(symbol)

        response = await self._client.get(url)
        data = response.json()

        return MarketData(
            symbol=symbol,
            price=float(data.get("price", 0)),
            bid=float(data.get("bid", 0)),
            ask=float(data.get("ask", 0)),
            high_24h=float(data.get("price", 0)) * 1.02,
            low_24h=float(data.get("price", 0)) * 0.98,
            volume_24h=float(data.get("volume", 0)),
            timestamp=datetime.now(),
        )

    async def _fetch_bybit(self, symbol: str) -> MarketData:
        """Fetch from Bybit API."""
        pair = symbol.replace("/", "")
        url = f"https://api.bybit.com/v5/market/tickers?category=spot&symbol={pair}"

        if not self._client:
            return self._mock_data(symbol)

        response = await self._client.get(url)
        data = response.json()

        if data.get("retCode") != 0 or not data.get("result", {}).get("list"):
            return self._mock_data(symbol)

        ticker = data["result"]["list"][0]

        return MarketData(
            symbol=symbol,
            price=float(ticker["lastPrice"]),
            bid=float(ticker["bid1Price"]),
            ask=float(ticker["ask1Price"]),
            high_24h=float(ticker["highPrice24h"]),
            low_24h=float(ticker["lowPrice24h"]),
            volume_24h=float(ticker["volume24h"]),
            timestamp=datetime.now(),
        )

    async def _fetch_kucoin(self, symbol: str) -> MarketData:
        """Fetch from KuCoin API."""
        pair = symbol.replace("/", "-")
        url = f"https://api.kucoin.com/api/v1/market/stats?symbol={pair}"

        if not self._client:
            return self._mock_data(symbol)

        response = await self._client.get(url)
        data = response.json()

        if data.get("code") != "200000" or not data.get("data"):
            return self._mock_data(symbol)

        ticker = data["data"]

        return MarketData(
            symbol=symbol,
            price=float(ticker.get("last", 0)),
            bid=float(ticker.get("buy", 0)),
            ask=float(ticker.get("sell", 0)),
            high_24h=float(ticker.get("high", 0)),
            low_24h=float(ticker.get("low", 0)),
            volume_24h=float(ticker.get("vol", 0)),
            timestamp=datetime.now(),
        )

    async def _fetch_gateio(self, symbol: str) -> MarketData:
        """Fetch from Gate.io API."""
        pair = symbol.replace("/", "_")
        url = f"https://api.gateio.ws/api/v4/spot/tickers?currency_pair={pair}"

        if not self._client:
            return self._mock_data(symbol)

        try:
            response = await self._client.get(url)
            data = response.json()

            if not data or len(data) == 0:
                return self._mock_data(symbol)

            ticker = data[0]

            return MarketData(
                symbol=symbol,
                price=float(ticker.get("last", 0)),
                bid=float(ticker.get("highest_bid", 0)),
                ask=float(ticker.get("lowest_ask", 0)),
                high_24h=float(ticker.get("high_24h", 0)),
                low_24h=float(ticker.get("low_24h", 0)),
                volume_24h=float(ticker.get("base_volume", 0)),
                timestamp=datetime.now(),
            )
        except Exception as e:
            logger.error(f"Gate.io fetch error: {e}")
            return self._mock_data(symbol)

    async def _fetch_hyperliquid(self, symbol: str) -> MarketData:
        """Fetch from HyperLiquid API."""
        # HyperLiquid uses a different symbol format (e.g., BTC for BTC/USD)
        base_symbol = symbol.split("/")[0]
        url = "https://api.hyperliquid.xyz/info"

        if not self._client:
            return self._mock_data(symbol)

        try:
            # HyperLiquid requires POST request with JSON body
            payload = {
                "type": "allMids"
            }
            response = await self._client.post(url, json=payload)
            data = response.json()

            if base_symbol not in data:
                return self._mock_data(symbol)

            mid_price = float(data[base_symbol])
            spread = mid_price * 0.0001  # 0.01% spread estimate

            return MarketData(
                symbol=symbol,
                price=mid_price,
                bid=mid_price - spread,
                ask=mid_price + spread,
                high_24h=mid_price * 1.02,
                low_24h=mid_price * 0.98,
                volume_24h=0,  # HyperLiquid doesn't provide volume in allMids
                timestamp=datetime.now(),
            )
        except Exception as e:
            logger.error(f"HyperLiquid fetch error: {e}")
            return self._mock_data(symbol)

    def _mock_data(self, symbol: str) -> MarketData:
        """Generate mock data."""
        import random

        base_prices = {
            "BTC/USD": 95000.0, "BTC/USDT": 95000.0,
            "ETH/USD": 3500.0, "ETH/USDT": 3500.0,
            "SOL/USD": 180.0, "SOL/USDT": 180.0,
            "XRP/USD": 2.5, "XRP/USDT": 2.5,
        }

        base_price = base_prices.get(symbol, 100.0)
        variation = base_price * 0.001
        price = base_price + random.uniform(-variation, variation)
        spread = price * 0.0005

        return MarketData(
            symbol=symbol,
            price=price,
            bid=price - spread,
            ask=price + spread,
            high_24h=price * 1.02,
            low_24h=price * 0.98,
            volume_24h=random.uniform(1000, 10000),
            timestamp=datetime.now(),
        )


# =============================================================================
# BROKER ADAPTER INTERFACE
# =============================================================================

class BrokerAdapter:
    """
    Base broker adapter for Alpha Arena.

    Provides unified interface for live trading when enabled.
    Paper trading uses simulated execution.
    """

    def __init__(
        self,
        broker_id: str,
        api_key: Optional[str] = None,
        api_secret: Optional[str] = None,
        is_paper: bool = True,
    ):
        if broker_id not in SUPPORTED_BROKERS:
            raise ValueError(f"Unsupported broker: {broker_id}")

        self.broker_id = broker_id
        self.info = SUPPORTED_BROKERS[broker_id]
        self.api_key = api_key
        self.api_secret = api_secret
        self.is_paper = is_paper
        self._data_provider: Optional[MultiExchangeDataProvider] = None
        self._initialized = False

    async def initialize(self) -> bool:
        """Initialize the broker adapter."""
        self._data_provider = MultiExchangeDataProvider(self.broker_id)
        await self._data_provider.initialize()
        self._initialized = True
        logger.info(f"Broker adapter initialized: {self.broker_id} (paper={self.is_paper})")
        return True

    async def close(self):
        """Close the broker adapter."""
        if self._data_provider:
            await self._data_provider.close()

    async def get_ticker(self, symbol: str) -> MarketData:
        """Get current market data."""
        if not self._data_provider:
            raise RuntimeError("Adapter not initialized")
        return await self._data_provider.get_ticker(symbol, self.broker_id)

    async def place_order(self, order: OrderRequest) -> OrderResult:
        """
        Place an order.

        In paper mode, simulates execution.
        In live mode, routes to actual exchange API.
        """
        if self.is_paper:
            return await self._paper_execute(order)
        else:
            return await self._live_execute(order)

    async def _paper_execute(self, order: OrderRequest) -> OrderResult:
        """Simulate order execution for paper trading."""
        # Get current market data
        market = await self.get_ticker(order.symbol)

        # Simulate execution
        if order.order_type == "market":
            fill_price = market.ask if order.side == TradeAction.BUY else market.bid
        elif order.order_type == "limit" and order.price:
            # Check if limit would fill
            if order.side == TradeAction.BUY:
                if order.price >= market.ask:
                    fill_price = market.ask
                else:
                    return OrderResult(
                        success=True,
                        status="open",
                        error="Limit order placed (not filled)",
                    )
            else:
                if order.price <= market.bid:
                    fill_price = market.bid
                else:
                    return OrderResult(
                        success=True,
                        status="open",
                        error="Limit order placed (not filled)",
                    )
        else:
            fill_price = market.price

        # Calculate fee
        fee = order.quantity * fill_price * self.info.taker_fee

        return OrderResult(
            success=True,
            order_id=f"paper_{datetime.now().timestamp()}",
            status="filled",
            filled_quantity=order.quantity,
            filled_price=fill_price,
            fee=fee,
        )

    async def _live_execute(self, order: OrderRequest) -> OrderResult:
        """
        Execute order on live exchange.

        NOTE: Live trading requires proper API integration.
        This method would typically call Tauri commands that
        route to the TypeScript broker adapters.
        """
        logger.warning(f"Live execution not implemented for {self.broker_id}")
        return OrderResult(
            success=False,
            error="Live trading not yet implemented - use paper trading mode",
        )


# =============================================================================
# FACTORY FUNCTIONS
# =============================================================================

def get_supported_brokers() -> List[BrokerInfo]:
    """Get all supported brokers."""
    return list(SUPPORTED_BROKERS.values())


def get_broker_info(broker_id: str) -> Optional[BrokerInfo]:
    """Get broker info by ID."""
    return SUPPORTED_BROKERS.get(broker_id)


def get_brokers_by_type(broker_type: BrokerType) -> List[BrokerInfo]:
    """Get brokers by type."""
    return [b for b in SUPPORTED_BROKERS.values() if b.broker_type == broker_type]


def get_brokers_by_region(region: BrokerRegion) -> List[BrokerInfo]:
    """Get brokers by region."""
    return [b for b in SUPPORTED_BROKERS.values() if b.region == region]


async def create_broker_adapter(
    broker_id: str,
    api_key: Optional[str] = None,
    api_secret: Optional[str] = None,
    is_paper: bool = True,
) -> BrokerAdapter:
    """Create and initialize a broker adapter."""
    adapter = BrokerAdapter(broker_id, api_key, api_secret, is_paper)
    await adapter.initialize()
    return adapter


# Singleton provider
_multi_provider: Optional[MultiExchangeDataProvider] = None


async def get_multi_exchange_provider(default_exchange: str = "kraken") -> MultiExchangeDataProvider:
    """Get or create the multi-exchange data provider."""
    global _multi_provider

    if _multi_provider is None or _multi_provider.default_exchange != default_exchange:
        if _multi_provider is not None:
            try:
                await _multi_provider.close()
            except Exception as e:
                logger.warning(f"Error closing previous multi-exchange provider: {e}")
        _multi_provider = MultiExchangeDataProvider(default_exchange)
        await _multi_provider.initialize()

    return _multi_provider


async def reset_multi_exchange_provider():
    """Reset the multi-exchange provider singleton."""
    global _multi_provider
    if _multi_provider is not None:
        try:
            await _multi_provider.close()
        except Exception as e:
            logger.warning(f"Error closing multi-exchange provider: {e}")
        _multi_provider = None
        logger.info("Multi-exchange provider reset")
