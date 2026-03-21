"""
Features pipeline for computing technical indicators.
"""

import time
import numpy as np
from typing import List, Dict, Optional
from .base_features import BaseFeaturesPipeline
from .market_data_source import CCXTMarketDataSource
from .types import FeaturesResult, FeatureVector, Candle, MarketSnapshot


class TechnicalFeaturesPipeline(BaseFeaturesPipeline):
    """
    Features pipeline that computes technical indicators.

    Indicators:
    - RSI (Relative Strength Index)
    - MACD (Moving Average Convergence Divergence)
    - Bollinger Bands
    - ATR (Average True Range)
    - Volatility
    """

    def __init__(
        self,
        market_data_source: CCXTMarketDataSource,
        symbols: List[str],
        interval: str = "1m",
        lookback: int = 100
    ):
        """
        Initialize features pipeline.

        Args:
            market_data_source: Market data source
            symbols: List of symbols to track
            interval: Candle interval
            lookback: Number of candles to fetch
        """
        self.market_data_source = market_data_source
        self.symbols = symbols
        self.interval = interval
        self.lookback = lookback

    async def build(self) -> FeaturesResult:
        """
        Build feature vectors with technical indicators.
        """
        timestamp_ms = int(time.time() * 1000)

        # Fetch candles
        candles_map = await self.market_data_source.get_recent_candles(
            self.symbols,
            self.interval,
            self.lookback
        )

        # Fetch market snapshots
        snapshots = await self.market_data_source.get_market_snapshot(self.symbols)
        snapshot_map = {snap.symbol: snap for snap in snapshots}

        # Compute features for each symbol
        features = []

        for symbol in self.symbols:
            candles = candles_map.get(symbol, [])
            snapshot = snapshot_map.get(symbol)

            if not candles or not snapshot:
                continue

            # Latest candle
            latest = candles[-1]

            # Compute technical indicators
            indicators = self._compute_indicators(candles)

            # Calculate spread in basis points
            spread_bps = 0.0
            if snapshot.ask > 0 and snapshot.bid > 0:
                spread_bps = ((snapshot.ask - snapshot.bid) / snapshot.bid) * 10000.0

            features.append(FeatureVector(
                symbol=symbol,
                timestamp_ms=timestamp_ms,
                close=latest.close,
                open=latest.open,
                high=latest.high,
                low=latest.low,
                volume=latest.volume,
                bid=snapshot.bid,
                ask=snapshot.ask,
                spread_bps=spread_bps,
                rsi_14=indicators.get('rsi_14'),
                macd=indicators.get('macd'),
                macd_signal=indicators.get('macd_signal'),
                macd_histogram=indicators.get('macd_histogram'),
                bb_upper=indicators.get('bb_upper'),
                bb_middle=indicators.get('bb_middle'),
                bb_lower=indicators.get('bb_lower'),
                atr_14=indicators.get('atr_14'),
                volatility_24h=indicators.get('volatility_24h'),
            ))

        return FeaturesResult(
            features=features,
            timestamp_ms=timestamp_ms,
            lookback_periods=self.lookback
        )

    def _compute_indicators(self, candles: List[Candle]) -> Dict[str, Optional[float]]:
        """
        Compute technical indicators from candles.
        """
        if len(candles) < 30:
            return {}

        closes = np.array([c.close for c in candles])
        highs = np.array([c.high for c in candles])
        lows = np.array([c.low for c in candles])

        indicators = {}

        try:
            # RSI (14-period)
            indicators['rsi_14'] = self._compute_rsi(closes, period=14)

            # MACD (12, 26, 9)
            macd_data = self._compute_macd(closes, fast=12, slow=26, signal=9)
            indicators.update(macd_data)

            # Bollinger Bands (20, 2)
            bb_data = self._compute_bollinger_bands(closes, period=20, std_dev=2.0)
            indicators.update(bb_data)

            # ATR (14-period)
            indicators['atr_14'] = self._compute_atr(highs, lows, closes, period=14)

            # Volatility (rolling standard deviation)
            indicators['volatility_24h'] = float(np.std(closes[-24:]) if len(closes) >= 24 else 0.0)

        except Exception as e:
            print(f"Error computing indicators: {e}", flush=True)

        return indicators

    def _compute_rsi(self, prices: np.ndarray, period: int = 14) -> Optional[float]:
        """Compute RSI (Relative Strength Index)."""
        if len(prices) < period + 1:
            return None

        deltas = np.diff(prices)
        gains = np.where(deltas > 0, deltas, 0)
        losses = np.where(deltas < 0, -deltas, 0)

        avg_gain = np.mean(gains[-period:])
        avg_loss = np.mean(losses[-period:])

        if avg_loss == 0:
            return 100.0

        rs = avg_gain / avg_loss
        rsi = 100.0 - (100.0 / (1.0 + rs))

        return float(rsi)

    def _compute_macd(
        self,
        prices: np.ndarray,
        fast: int = 12,
        slow: int = 26,
        signal: int = 9
    ) -> Dict[str, Optional[float]]:
        """Compute MACD (Moving Average Convergence Divergence)."""
        if len(prices) < slow + signal:
            return {
                'macd': None,
                'macd_signal': None,
                'macd_histogram': None
            }

        # Exponential moving averages
        ema_fast = self._ema(prices, fast)
        ema_slow = self._ema(prices, slow)

        macd_line = ema_fast - ema_slow

        # Signal line (EMA of MACD)
        macd_values = []
        for i in range(len(prices) - slow + 1):
            macd_values.append(ema_fast - ema_slow)

        signal_line = self._ema(np.array(macd_values), signal)

        histogram = macd_line - signal_line

        return {
            'macd': float(macd_line),
            'macd_signal': float(signal_line),
            'macd_histogram': float(histogram)
        }

    def _compute_bollinger_bands(
        self,
        prices: np.ndarray,
        period: int = 20,
        std_dev: float = 2.0
    ) -> Dict[str, Optional[float]]:
        """Compute Bollinger Bands."""
        if len(prices) < period:
            return {
                'bb_upper': None,
                'bb_middle': None,
                'bb_lower': None
            }

        recent = prices[-period:]
        middle = float(np.mean(recent))
        std = float(np.std(recent))

        upper = middle + (std_dev * std)
        lower = middle - (std_dev * std)

        return {
            'bb_upper': upper,
            'bb_middle': middle,
            'bb_lower': lower
        }

    def _compute_atr(
        self,
        highs: np.ndarray,
        lows: np.ndarray,
        closes: np.ndarray,
        period: int = 14
    ) -> Optional[float]:
        """Compute ATR (Average True Range)."""
        if len(highs) < period + 1:
            return None

        true_ranges = []
        for i in range(1, len(highs)):
            high_low = highs[i] - lows[i]
            high_close = abs(highs[i] - closes[i - 1])
            low_close = abs(lows[i] - closes[i - 1])
            true_range = max(high_low, high_close, low_close)
            true_ranges.append(true_range)

        atr = float(np.mean(true_ranges[-period:]))
        return atr

    def _ema(self, prices: np.ndarray, period: int) -> float:
        """Calculate Exponential Moving Average."""
        if len(prices) < period:
            return float(np.mean(prices))

        multiplier = 2.0 / (period + 1.0)
        ema = float(np.mean(prices[:period]))

        for price in prices[period:]:
            ema = (price * multiplier) + (ema * (1.0 - multiplier))

        return ema

    async def update_symbols(self, symbols: List[str]) -> None:
        """Update the list of symbols to track."""
        self.symbols = symbols
