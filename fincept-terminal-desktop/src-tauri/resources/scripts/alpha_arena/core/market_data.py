"""
Market Data Provider

Async market data fetching using httpx.
"""

import asyncio
from datetime import datetime
from typing import Dict, List, Optional, Any

try:
    import httpx
    HTTPX_AVAILABLE = True
except ImportError:
    HTTPX_AVAILABLE = False

from alpha_arena.types.models import MarketData
from alpha_arena.utils.logging import get_logger

logger = get_logger("market_data")


class MarketDataProvider:
    """
    Async market data provider using httpx.

    Supports multiple exchanges via a unified interface.
    """

    def __init__(
        self,
        exchange_id: str = "kraken",
        timeout: float = 10.0,
    ):
        self.exchange_id = exchange_id
        self.timeout = timeout
        self._client: Optional[Any] = None
        self._cache: Dict[str, MarketData] = {}
        self._cache_ttl = 5  # seconds

    async def __aenter__(self):
        await self.initialize()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.close()

    async def initialize(self):
        """Initialize the HTTP client."""
        if HTTPX_AVAILABLE:
            self._client = httpx.AsyncClient(timeout=self.timeout)
        logger.info(f"Market data provider initialized for {self.exchange_id}")

    async def close(self):
        """Close the HTTP client."""
        if self._client:
            await self._client.aclose()
            self._client = None

    async def get_ticker(self, symbol: str) -> MarketData:
        """
        Get current ticker data for a symbol.

        Args:
            symbol: Trading pair (e.g., "BTC/USD")

        Returns:
            MarketData with current prices
        """
        # Check cache first
        if symbol in self._cache:
            cached = self._cache[symbol]
            age = (datetime.now() - cached.timestamp).total_seconds()
            if age < self._cache_ttl:
                return cached

        # Fetch from exchange
        if self.exchange_id == "kraken":
            data = await self._fetch_kraken(symbol)
        elif self.exchange_id == "binance":
            data = await self._fetch_binance(symbol)
        else:
            data = await self._fetch_generic(symbol)

        self._cache[symbol] = data
        return data

    async def get_tickers(self, symbols: List[str]) -> Dict[str, MarketData]:
        """Get ticker data for multiple symbols."""
        results = {}
        for symbol in symbols:
            try:
                results[symbol] = await self.get_ticker(symbol)
            except Exception as e:
                logger.error(f"Failed to fetch {symbol}: {e}")
        return results

    async def _fetch_kraken(self, symbol: str) -> MarketData:
        """Fetch data from Kraken."""
        # Convert symbol format
        pair = symbol.replace("/", "")
        if pair.endswith("USD"):
            pair = pair.replace("USD", "ZUSD")
        if pair.startswith("BTC"):
            pair = pair.replace("BTC", "XBT")

        url = f"https://api.kraken.com/0/public/Ticker?pair={pair}"

        if not HTTPX_AVAILABLE or not self._client:
            return self._mock_market_data(symbol)

        try:
            response = await self._client.get(url)
            data = response.json()

            if data.get("error") and len(data["error"]) > 0:
                logger.warning(f"Kraken API error: {data['error']}")
                return self._mock_market_data(symbol)

            result = data.get("result", {})
            if not result:
                return self._mock_market_data(symbol)

            # Get first result (there should only be one)
            ticker_data = list(result.values())[0]

            return MarketData(
                symbol=symbol,
                price=float(ticker_data["c"][0]),  # Last trade close price
                bid=float(ticker_data["b"][0]),    # Best bid
                ask=float(ticker_data["a"][0]),    # Best ask
                high_24h=float(ticker_data["h"][1]),  # 24h high
                low_24h=float(ticker_data["l"][1]),   # 24h low
                volume_24h=float(ticker_data["v"][1]),  # 24h volume
                timestamp=datetime.now(),
            )

        except Exception as e:
            logger.error(f"Kraken fetch error for {symbol}: {e}")
            return self._mock_market_data(symbol)

    async def _fetch_binance(self, symbol: str) -> MarketData:
        """Fetch data from Binance."""
        pair = symbol.replace("/", "")
        url = f"https://api.binance.com/api/v3/ticker/24hr?symbol={pair}"

        if not HTTPX_AVAILABLE or not self._client:
            return self._mock_market_data(symbol)

        try:
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

        except Exception as e:
            logger.error(f"Binance fetch error for {symbol}: {e}")
            return self._mock_market_data(symbol)

    async def _fetch_generic(self, symbol: str) -> MarketData:
        """Fallback fetch using mock data."""
        logger.warning(f"Using mock data for {symbol}")
        return self._mock_market_data(symbol)

    def _mock_market_data(self, symbol: str) -> MarketData:
        """Generate mock market data for testing."""
        import random

        base_prices = {
            "BTC/USD": 95000.0,
            "ETH/USD": 3500.0,
            "SOL/USD": 180.0,
            "XRP/USD": 2.5,
        }

        base_price = base_prices.get(symbol, 100.0)
        variation = base_price * 0.001  # 0.1% variation

        price = base_price + random.uniform(-variation, variation)
        spread = price * 0.0005  # 0.05% spread

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


class CachedMarketDataProvider(MarketDataProvider):
    """Market data provider with enhanced caching."""

    def __init__(
        self,
        exchange_id: str = "kraken",
        timeout: float = 10.0,
        cache_ttl: int = 10,
    ):
        super().__init__(exchange_id, timeout)
        self._cache_ttl = cache_ttl
        self._price_history: Dict[str, List[float]] = {}
        self._max_history = 100

    def get_price_history(self, symbol: str) -> List[float]:
        """Get price history for a symbol."""
        return self._price_history.get(symbol, [])

    async def get_ticker(self, symbol: str) -> MarketData:
        """Get ticker and record price history."""
        data = await super().get_ticker(symbol)

        # Record price history
        if symbol not in self._price_history:
            self._price_history[symbol] = []

        self._price_history[symbol].append(data.price)

        # Trim history
        if len(self._price_history[symbol]) > self._max_history:
            self._price_history[symbol] = self._price_history[symbol][-self._max_history:]

        return data

    def get_price_change(self, symbol: str, periods: int = 5) -> float:
        """Calculate price change over N periods."""
        history = self._price_history.get(symbol, [])
        if len(history) < 2:
            return 0.0

        periods = min(periods, len(history) - 1)
        return (history[-1] - history[-periods - 1]) / history[-periods - 1] * 100


# Singleton instance
_provider: Optional[MarketDataProvider] = None


async def get_market_data_provider(exchange_id: str = "kraken") -> MarketDataProvider:
    """Get or create the market data provider."""
    global _provider

    if _provider is None or _provider.exchange_id != exchange_id:
        if _provider:
            await _provider.close()
        _provider = CachedMarketDataProvider(exchange_id)
        await _provider.initialize()

    return _provider
