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

from alpha_arena.types.models import MarketData, PolymarketInfo
from alpha_arena.utils.logging import get_logger

logger = get_logger("market_data")

# Polymarket API endpoints
POLYMARKET_GAMMA_API = "https://gamma-api.polymarket.com"
POLYMARKET_CLOB_API = "https://clob.polymarket.com"


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
        errors = []
        for symbol in symbols:
            try:
                results[symbol] = await self.get_ticker(symbol)
            except Exception as e:
                logger.error(f"Failed to fetch {symbol}: {e}")
                errors.append(f"{symbol}: {e}")
        if errors and not results:
            raise RuntimeError(f"Failed to fetch any market data: {'; '.join(errors)}")
        return results

    async def _fetch_kraken(self, symbol: str) -> MarketData:
        """Fetch data from Kraken."""
        # Convert symbol format - just strip the slash.
        # Kraken accepts simple pairs like BTCUSD, ETHUSD, etc.
        pair = symbol.replace("/", "")

        url = f"https://api.kraken.com/0/public/Ticker?pair={pair}"

        if not HTTPX_AVAILABLE:
            raise RuntimeError("httpx is not installed. Install it with: pip install httpx")
        if not self._client:
            raise RuntimeError("HTTP client not initialized. Call initialize() first.")

        try:
            response = await self._client.get(url)
            data = response.json()

            if data.get("error") and len(data["error"]) > 0:
                raise RuntimeError(f"Kraken API error for {symbol}: {data['error']}")

            result = data.get("result", {})
            if not result:
                raise RuntimeError(f"No data returned from Kraken for {symbol}")

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

        except RuntimeError:
            raise
        except Exception as e:
            logger.error(f"Kraken fetch error for {symbol}: {e}")
            raise RuntimeError(f"Failed to fetch {symbol} from Kraken: {e}") from e

    async def _fetch_binance(self, symbol: str) -> MarketData:
        """Fetch data from Binance."""
        pair = symbol.replace("/", "")
        url = f"https://api.binance.com/api/v3/ticker/24hr?symbol={pair}"

        if not HTTPX_AVAILABLE:
            raise RuntimeError("httpx is not installed. Install it with: pip install httpx")
        if not self._client:
            raise RuntimeError("HTTP client not initialized. Call initialize() first.")

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

        except RuntimeError:
            raise
        except Exception as e:
            logger.error(f"Binance fetch error for {symbol}: {e}")
            raise RuntimeError(f"Failed to fetch {symbol} from Binance: {e}") from e

    async def _fetch_generic(self, symbol: str) -> MarketData:
        """Fallback for unsupported exchanges - raises error."""
        raise RuntimeError(
            f"Unsupported exchange '{self.exchange_id}' for {symbol}. "
            f"Supported exchanges: kraken, binance. Configure a supported exchange."
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
            try:
                await _provider.close()
            except Exception as e:
                logger.warning(f"Error closing previous market data provider: {e}")
        _provider = CachedMarketDataProvider(exchange_id)
        await _provider.initialize()

    return _provider


async def reset_market_data_provider():
    """Reset the market data provider singleton (useful between competitions)."""
    global _provider
    if _provider:
        try:
            await _provider.close()
        except Exception as e:
            logger.warning(f"Error closing market data provider: {e}")
        _provider = None
        logger.info("Market data provider reset")


# =============================================================================
# Polymarket Data Provider
# =============================================================================

class PolymarketDataProvider:
    """
    Async Polymarket data provider using httpx.

    Fetches prediction market data from Polymarket's Gamma and CLOB APIs.
    """

    def __init__(self, timeout: float = 10.0):
        self.timeout = timeout
        self._client: Optional[Any] = None
        self._cache: Dict[str, PolymarketInfo] = {}
        self._cache_ttl = 5  # seconds
        self._last_fetch: Dict[str, datetime] = {}

    async def __aenter__(self):
        await self.initialize()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.close()

    async def initialize(self):
        """Initialize the HTTP client."""
        if HTTPX_AVAILABLE:
            self._client = httpx.AsyncClient(timeout=self.timeout)
        logger.info("Polymarket data provider initialized")

    async def close(self):
        """Close the HTTP client."""
        if self._client:
            await self._client.aclose()
            self._client = None

    async def get_market(self, market_id: str) -> PolymarketInfo:
        """
        Get current data for a Polymarket market.

        Args:
            market_id: Polymarket market ID (condition_id)

        Returns:
            PolymarketInfo with current prices and metadata
        """
        # Check cache first
        if market_id in self._cache and market_id in self._last_fetch:
            age = (datetime.now() - self._last_fetch[market_id]).total_seconds()
            if age < self._cache_ttl:
                return self._cache[market_id]

        if not HTTPX_AVAILABLE:
            raise RuntimeError("httpx is not installed")
        if not self._client:
            raise RuntimeError("HTTP client not initialized")

        try:
            # Fetch from Gamma API
            url = f"{POLYMARKET_GAMMA_API}/markets/{market_id}"
            response = await self._client.get(url)
            data = response.json()

            # Parse outcomes and prices
            outcomes = ["Yes", "No"]
            outcome_prices = [0.5, 0.5]
            token_ids = []

            if "outcomes" in data:
                raw_outcomes = data["outcomes"]
                if isinstance(raw_outcomes, str):
                    import json
                    outcomes = json.loads(raw_outcomes)
                elif isinstance(raw_outcomes, list):
                    outcomes = raw_outcomes

            if "outcomePrices" in data:
                raw_prices = data["outcomePrices"]
                if isinstance(raw_prices, str):
                    import json
                    outcome_prices = [float(p) for p in json.loads(raw_prices)]
                elif isinstance(raw_prices, list):
                    outcome_prices = [float(p) for p in raw_prices]

            if "clobTokenIds" in data:
                raw_tokens = data["clobTokenIds"]
                if isinstance(raw_tokens, str):
                    import json
                    token_ids = json.loads(raw_tokens)
                elif isinstance(raw_tokens, list):
                    token_ids = raw_tokens

            market_info = PolymarketInfo(
                id=market_id,
                question=data.get("question", "Unknown"),
                description=data.get("description"),
                outcomes=outcomes,
                outcome_prices=outcome_prices,
                token_ids=token_ids,
                volume=float(data.get("volumeNum", data.get("volume", 0)) or 0),
                liquidity=float(data.get("liquidityNum", data.get("liquidity", 0)) or 0),
                end_date=data.get("endDate"),
                category=data.get("tags", [{}])[0].get("label") if data.get("tags") else None,
            )

            self._cache[market_id] = market_info
            self._last_fetch[market_id] = datetime.now()
            return market_info

        except Exception as e:
            logger.error(f"Polymarket fetch error for {market_id}: {e}")
            raise RuntimeError(f"Failed to fetch market {market_id}: {e}") from e

    async def get_markets(self, market_ids: List[str]) -> Dict[str, PolymarketInfo]:
        """Get data for multiple markets."""
        results = {}
        errors = []
        for market_id in market_ids:
            try:
                results[market_id] = await self.get_market(market_id)
            except Exception as e:
                logger.error(f"Failed to fetch {market_id}: {e}")
                errors.append(f"{market_id}: {e}")
        if errors and not results:
            raise RuntimeError(f"Failed to fetch any markets: {'; '.join(errors)}")
        return results

    async def get_live_prices(self, token_ids: List[str]) -> Dict[str, float]:
        """
        Fetch live prices from CLOB API for given token IDs.

        Returns dict of token_id -> price
        """
        if not token_ids:
            return {}

        if not HTTPX_AVAILABLE or not self._client:
            return {}

        try:
            # CLOB prices endpoint
            url = f"{POLYMARKET_CLOB_API}/prices"
            params = {"token_ids": ",".join(token_ids)}
            response = await self._client.get(url, params=params)
            data = response.json()

            prices = {}
            for token_id, price_data in data.items():
                if isinstance(price_data, dict) and "price" in price_data:
                    prices[token_id] = float(price_data["price"])
                elif isinstance(price_data, (int, float, str)):
                    prices[token_id] = float(price_data)

            return prices

        except Exception as e:
            logger.warning(f"Failed to fetch live prices: {e}")
            return {}


# Singleton instance for Polymarket
_polymarket_provider: Optional[PolymarketDataProvider] = None


async def get_polymarket_provider() -> PolymarketDataProvider:
    """Get or create the Polymarket data provider."""
    global _polymarket_provider

    if _polymarket_provider is None:
        _polymarket_provider = PolymarketDataProvider()
        await _polymarket_provider.initialize()

    return _polymarket_provider


async def reset_polymarket_provider():
    """Reset the Polymarket data provider singleton."""
    global _polymarket_provider
    if _polymarket_provider:
        try:
            await _polymarket_provider.close()
        except Exception as e:
            logger.warning(f"Error closing Polymarket provider: {e}")
        _polymarket_provider = None
        logger.info("Polymarket data provider reset")
