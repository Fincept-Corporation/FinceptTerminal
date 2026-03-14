"""
Market data source using CCXT for fetching candles and snapshots.
"""

import ccxt
import time
import asyncio
from typing import List, Dict, Optional
from datetime import datetime
from .types import Candle, MarketSnapshot


class CCXTMarketDataSource:
    """
    Market data source using CCXT library.

    Supports multiple exchanges (Binance, Kraken, Coinbase, etc.)
    """

    def __init__(
        self,
        exchange_id: str = "kraken",
        testnet: bool = False,
        rate_limit: bool = True
    ):
        """
        Initialize market data source.

        Args:
            exchange_id: Exchange name (binance, kraken, coinbase, etc.)
            testnet: Use testnet/sandbox (if available)
            rate_limit: Enable rate limiting
        """
        self.exchange_id = exchange_id
        self.testnet = testnet
        self.rate_limit = rate_limit
        self._exchange: Optional[ccxt.Exchange] = None
        self._last_request_time = 0.0
        self._min_request_interval = 1.0  # 1 second between requests

    def _get_exchange(self) -> ccxt.Exchange:
        """Get or create exchange instance."""
        if self._exchange is None:
            exchange_class = getattr(ccxt, self.exchange_id)
            config = {
                'enableRateLimit': self.rate_limit,
                'timeout': 10000,
            }

            if self.testnet:
                config['sandbox'] = True

            self._exchange = exchange_class(config)

        return self._exchange

    async def _rate_limit_delay(self) -> None:
        """Enforce rate limiting."""
        if not self.rate_limit:
            return

        now = time.time()
        time_since_last = now - self._last_request_time

        if time_since_last < self._min_request_interval:
            await asyncio.sleep(self._min_request_interval - time_since_last)

        self._last_request_time = time.time()

    async def get_recent_candles(
        self,
        symbols: List[str],
        interval: str = "1m",
        lookback: int = 100
    ) -> Dict[str, List[Candle]]:
        """
        Fetch recent OHLCV candles for multiple symbols.

        Args:
            symbols: List of trading pairs (e.g., ["BTC/USD", "ETH/USD"])
            interval: Timeframe (1m, 5m, 15m, 1h, 1d)
            lookback: Number of candles to fetch

        Returns:
            Dict mapping symbol -> list of candles
        """
        exchange = self._get_exchange()
        result = {}

        for symbol in symbols:
            try:
                await self._rate_limit_delay()

                # Fetch OHLCV data
                ohlcv = exchange.fetch_ohlcv(
                    symbol=symbol,
                    timeframe=interval,
                    limit=lookback
                )

                # Convert to Candle objects
                candles = [
                    Candle(
                        timestamp=int(row[0]),
                        open=float(row[1]),
                        high=float(row[2]),
                        low=float(row[3]),
                        close=float(row[4]),
                        volume=float(row[5])
                    )
                    for row in ohlcv
                ]

                result[symbol] = candles

            except Exception as e:
                print(f"Error fetching candles for {symbol}: {e}", flush=True)
                result[symbol] = []

        return result

    async def get_market_snapshot(
        self,
        symbols: List[str]
    ) -> List[MarketSnapshot]:
        """
        Get real-time market snapshots for multiple symbols.

        Args:
            symbols: List of trading pairs

        Returns:
            List of MarketSnapshot objects
        """
        exchange = self._get_exchange()
        snapshots = []
        timestamp_ms = int(time.time() * 1000)

        for symbol in symbols:
            try:
                await self._rate_limit_delay()

                # Fetch ticker
                ticker = exchange.fetch_ticker(symbol)

                # Extract data
                last_price = float(ticker.get('last', 0))
                bid = float(ticker.get('bid', last_price))
                ask = float(ticker.get('ask', last_price))
                volume_24h = float(ticker.get('quoteVolume', 0))
                high_24h = float(ticker.get('high', last_price))
                low_24h = float(ticker.get('low', last_price))

                # Calculate 24h change
                open_24h = float(ticker.get('open', last_price))
                change_24h_pct = 0.0
                if open_24h > 0:
                    change_24h_pct = ((last_price - open_24h) / open_24h) * 100.0

                snapshots.append(MarketSnapshot(
                    symbol=symbol,
                    last_price=last_price,
                    bid=bid,
                    ask=ask,
                    volume_24h=volume_24h,
                    high_24h=high_24h,
                    low_24h=low_24h,
                    change_24h_pct=change_24h_pct,
                    timestamp_ms=timestamp_ms
                ))

            except Exception as e:
                print(f"Error fetching snapshot for {symbol}: {e}", flush=True)
                # Create default snapshot on error
                snapshots.append(MarketSnapshot(
                    symbol=symbol,
                    last_price=0.0,
                    bid=0.0,
                    ask=0.0,
                    volume_24h=0.0,
                    high_24h=0.0,
                    low_24h=0.0,
                    change_24h_pct=0.0,
                    timestamp_ms=timestamp_ms
                ))

        return snapshots

    async def close(self) -> None:
        """Close exchange connection."""
        if self._exchange is not None:
            await self._exchange.close()
            self._exchange = None
