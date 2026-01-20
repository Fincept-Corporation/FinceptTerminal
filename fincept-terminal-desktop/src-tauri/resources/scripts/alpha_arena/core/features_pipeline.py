"""
Features Pipeline for Alpha Arena

Provides market features (technical indicators, sentiment, etc.) to trading agents.
Integrates with existing technical indicators from agno_trading/tools.
"""

import sys
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
from datetime import datetime
from abc import ABC, abstractmethod

# Add parent paths for imports
scripts_dir = Path(__file__).parent.parent.parent
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))

from alpha_arena.types.models import MarketData
from alpha_arena.utils.logging import get_logger

logger = get_logger("features_pipeline")


@dataclass
class TechnicalFeatures:
    """Technical analysis features"""
    sma_20: Optional[float] = None
    sma_50: Optional[float] = None
    ema_12: Optional[float] = None
    ema_26: Optional[float] = None
    rsi_14: Optional[float] = None
    rsi_interpretation: str = "neutral"
    macd: Optional[float] = None
    macd_signal: Optional[float] = None
    macd_histogram: Optional[float] = None
    macd_interpretation: str = "neutral"
    bollinger_upper: Optional[float] = None
    bollinger_middle: Optional[float] = None
    bollinger_lower: Optional[float] = None
    bollinger_interpretation: str = "neutral"
    support_level: Optional[float] = None
    resistance_level: Optional[float] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "sma_20": self.sma_20,
            "sma_50": self.sma_50,
            "ema_12": self.ema_12,
            "ema_26": self.ema_26,
            "rsi_14": self.rsi_14,
            "rsi_interpretation": self.rsi_interpretation,
            "macd": self.macd,
            "macd_signal": self.macd_signal,
            "macd_histogram": self.macd_histogram,
            "macd_interpretation": self.macd_interpretation,
            "bollinger_upper": self.bollinger_upper,
            "bollinger_middle": self.bollinger_middle,
            "bollinger_lower": self.bollinger_lower,
            "bollinger_interpretation": self.bollinger_interpretation,
            "support_level": self.support_level,
            "resistance_level": self.resistance_level,
        }

    def to_prompt_context(self) -> str:
        """Convert to text for LLM prompt"""
        lines = ["TECHNICAL INDICATORS:"]

        if self.sma_20:
            lines.append(f"  SMA(20): ${self.sma_20:,.2f}")
        if self.sma_50:
            lines.append(f"  SMA(50): ${self.sma_50:,.2f}")
        if self.ema_12:
            lines.append(f"  EMA(12): ${self.ema_12:,.2f}")
        if self.ema_26:
            lines.append(f"  EMA(26): ${self.ema_26:,.2f}")
        if self.rsi_14:
            lines.append(f"  RSI(14): {self.rsi_14:.1f} ({self.rsi_interpretation})")
        if self.macd:
            lines.append(f"  MACD: {self.macd:.4f}, Signal: {self.macd_signal:.4f} ({self.macd_interpretation})")
        if self.bollinger_upper:
            lines.append(f"  Bollinger Bands: Upper ${self.bollinger_upper:,.2f}, Middle ${self.bollinger_middle:,.2f}, Lower ${self.bollinger_lower:,.2f}")
            lines.append(f"    ({self.bollinger_interpretation})")
        if self.support_level:
            lines.append(f"  Support: ${self.support_level:,.2f}")
        if self.resistance_level:
            lines.append(f"  Resistance: ${self.resistance_level:,.2f}")

        return "\n".join(lines)


@dataclass
class SentimentFeatures:
    """News and sentiment features"""
    overall_sentiment: str = "neutral"  # bullish, bearish, neutral
    sentiment_score: float = 0.0  # -1 to 1
    news_count: int = 0
    recent_headlines: List[str] = field(default_factory=list)
    social_sentiment: Optional[float] = None
    fear_greed_index: Optional[float] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "overall_sentiment": self.overall_sentiment,
            "sentiment_score": self.sentiment_score,
            "news_count": self.news_count,
            "recent_headlines": self.recent_headlines,
            "social_sentiment": self.social_sentiment,
            "fear_greed_index": self.fear_greed_index,
        }

    def to_prompt_context(self) -> str:
        """Convert to text for LLM prompt"""
        lines = ["MARKET SENTIMENT:"]
        lines.append(f"  Overall: {self.overall_sentiment.upper()} (score: {self.sentiment_score:+.2f})")

        if self.fear_greed_index is not None:
            fg_label = "Extreme Fear" if self.fear_greed_index < 25 else \
                       "Fear" if self.fear_greed_index < 45 else \
                       "Neutral" if self.fear_greed_index < 55 else \
                       "Greed" if self.fear_greed_index < 75 else "Extreme Greed"
            lines.append(f"  Fear & Greed Index: {self.fear_greed_index:.0f} ({fg_label})")

        if self.recent_headlines:
            lines.append(f"  Recent Headlines ({self.news_count} articles):")
            for headline in self.recent_headlines[:3]:
                lines.append(f"    - {headline[:100]}")

        return "\n".join(lines)


@dataclass
class MarketFeatures:
    """Combined market features for decision making"""
    timestamp: datetime = field(default_factory=datetime.now)
    symbol: str = ""
    current_price: float = 0.0
    price_change_24h: float = 0.0
    price_change_pct_24h: float = 0.0
    volume_24h: float = 0.0
    high_24h: float = 0.0
    low_24h: float = 0.0

    # Feature components
    technical: TechnicalFeatures = field(default_factory=TechnicalFeatures)
    sentiment: SentimentFeatures = field(default_factory=SentimentFeatures)

    # Computed signals
    trend_direction: str = "neutral"  # bullish, bearish, neutral
    trend_strength: float = 0.0  # 0 to 1
    volatility_level: str = "normal"  # low, normal, high, extreme

    def to_dict(self) -> Dict[str, Any]:
        return {
            "timestamp": self.timestamp.isoformat(),
            "symbol": self.symbol,
            "current_price": self.current_price,
            "price_change_24h": self.price_change_24h,
            "price_change_pct_24h": self.price_change_pct_24h,
            "volume_24h": self.volume_24h,
            "high_24h": self.high_24h,
            "low_24h": self.low_24h,
            "technical": self.technical.to_dict(),
            "sentiment": self.sentiment.to_dict(),
            "trend_direction": self.trend_direction,
            "trend_strength": self.trend_strength,
            "volatility_level": self.volatility_level,
        }

    def to_prompt_context(self) -> str:
        """Convert to comprehensive text for LLM prompt"""
        lines = [
            f"MARKET OVERVIEW ({self.symbol}):",
            f"  Current Price: ${self.current_price:,.2f}",
            f"  24h Change: ${self.price_change_24h:+,.2f} ({self.price_change_pct_24h:+.2f}%)",
            f"  24h Range: ${self.low_24h:,.2f} - ${self.high_24h:,.2f}",
            f"  24h Volume: {self.volume_24h:,.2f}",
            f"  Trend: {self.trend_direction.upper()} (strength: {self.trend_strength:.2f})",
            f"  Volatility: {self.volatility_level.upper()}",
            "",
            self.technical.to_prompt_context(),
            "",
            self.sentiment.to_prompt_context(),
        ]
        return "\n".join(lines)


class FeaturesPipeline(ABC):
    """Abstract base class for feature computation pipelines"""

    @abstractmethod
    async def compute(
        self,
        market_data: MarketData,
        price_history: Optional[List[float]] = None,
    ) -> MarketFeatures:
        """Compute features from market data"""
        raise NotImplementedError


class DefaultFeaturesPipeline(FeaturesPipeline):
    """
    Default features pipeline using existing technical indicators.

    Integrates with:
    - agno_trading/tools/technical_indicators.py
    - agno_trading/tools/news_sentiment.py
    """

    def __init__(
        self,
        enable_technical: bool = True,
        enable_sentiment: bool = True,
        price_history_length: int = 100,
    ):
        self.enable_technical = enable_technical
        self.enable_sentiment = enable_sentiment
        self.price_history_length = price_history_length
        self._price_history: Dict[str, List[float]] = {}

    def add_price(self, symbol: str, price: float):
        """Add a price to history for technical analysis"""
        if symbol not in self._price_history:
            self._price_history[symbol] = []

        self._price_history[symbol].append(price)

        # Keep only recent history
        if len(self._price_history[symbol]) > self.price_history_length:
            self._price_history[symbol] = self._price_history[symbol][-self.price_history_length:]

    def get_price_history(self, symbol: str) -> List[float]:
        """Get price history for a symbol"""
        return self._price_history.get(symbol, [])

    async def compute(
        self,
        market_data: MarketData,
        price_history: Optional[List[float]] = None,
    ) -> MarketFeatures:
        """Compute all features from market data"""

        # Add current price to history
        self.add_price(market_data.symbol, market_data.price)

        # Use provided history or internal history
        prices = price_history or self.get_price_history(market_data.symbol)

        # Compute technical features
        technical = await self._compute_technical(prices, market_data.price)

        # Compute sentiment features (placeholder for now)
        sentiment = await self._compute_sentiment(market_data.symbol)

        # Compute trend and volatility
        trend_direction, trend_strength = self._compute_trend(technical, prices)
        volatility_level = self._compute_volatility(market_data, prices)

        # Build features
        return MarketFeatures(
            timestamp=datetime.now(),
            symbol=market_data.symbol,
            current_price=market_data.price,
            price_change_24h=market_data.price - (prices[0] if prices else market_data.price),
            price_change_pct_24h=((market_data.price / prices[0]) - 1) * 100 if prices and prices[0] > 0 else 0,
            volume_24h=market_data.volume_24h,
            high_24h=market_data.high_24h,
            low_24h=market_data.low_24h,
            technical=technical,
            sentiment=sentiment,
            trend_direction=trend_direction,
            trend_strength=trend_strength,
            volatility_level=volatility_level,
        )

    async def _compute_technical(
        self,
        prices: List[float],
        current_price: float,
    ) -> TechnicalFeatures:
        """Compute technical indicators using existing tools"""

        if not self.enable_technical or len(prices) < 20:
            return TechnicalFeatures()

        features = TechnicalFeatures()

        try:
            # Import existing technical indicator tools
            from agno_trading.tools.technical_indicators import (
                calculate_sma,
                calculate_ema,
                calculate_rsi,
                calculate_macd,
                calculate_bollinger_bands,
                detect_support_resistance,
            )

            # SMA
            sma_20 = calculate_sma(prices, 20)
            if "current" in sma_20 and sma_20["current"]:
                features.sma_20 = sma_20["current"]

            if len(prices) >= 50:
                sma_50 = calculate_sma(prices, 50)
                if "current" in sma_50 and sma_50["current"]:
                    features.sma_50 = sma_50["current"]

            # EMA
            ema_12 = calculate_ema(prices, 12)
            if "current" in ema_12 and ema_12["current"]:
                features.ema_12 = ema_12["current"]

            ema_26 = calculate_ema(prices, 26)
            if "current" in ema_26 and ema_26["current"]:
                features.ema_26 = ema_26["current"]

            # RSI
            rsi = calculate_rsi(prices, 14)
            if "current" in rsi and rsi["current"]:
                features.rsi_14 = rsi["current"]
                features.rsi_interpretation = rsi.get("interpretation", "neutral")

            # MACD
            macd = calculate_macd(prices)
            if "current_macd" in macd and macd["current_macd"]:
                features.macd = macd["current_macd"]
                features.macd_signal = macd.get("current_signal")
                features.macd_histogram = macd.get("current_histogram")
                features.macd_interpretation = macd.get("interpretation", "neutral")

            # Bollinger Bands
            bb = calculate_bollinger_bands(prices)
            if "current_upper" in bb and bb["current_upper"]:
                features.bollinger_upper = bb["current_upper"]
                features.bollinger_middle = bb.get("current_middle")
                features.bollinger_lower = bb.get("current_lower")
                features.bollinger_interpretation = bb.get("interpretation", "neutral")

            # Support/Resistance
            sr = detect_support_resistance(prices)
            if "nearest_support" in sr:
                features.support_level = sr.get("nearest_support")
                features.resistance_level = sr.get("nearest_resistance")

        except ImportError as e:
            logger.warning(f"Technical indicators not available: {e}")
        except Exception as e:
            logger.warning(f"Error computing technical indicators: {e}")

        return features

    async def _compute_sentiment(self, symbol: str) -> SentimentFeatures:
        """Compute sentiment features using sentiment agent"""

        if not self.enable_sentiment:
            return SentimentFeatures()

        features = SentimentFeatures()

        try:
            # Try the new sentiment agent first
            from alpha_arena.core.sentiment_agent import get_sentiment_agent

            agent = await get_sentiment_agent()
            result = await agent.analyze_sentiment(symbol, use_cache=True, max_articles=10)

            features.overall_sentiment = result.overall_sentiment.value
            features.sentiment_score = result.sentiment_score
            features.news_count = result.articles_analyzed
            features.recent_headlines = result.key_headlines[:3]

        except ImportError:
            # Fallback to basic sentiment tools
            try:
                from agno_trading.tools.news_sentiment import get_market_sentiment

                sentiment = get_market_sentiment(symbol)
                features.overall_sentiment = sentiment.get("overall_sentiment", "neutral")
                features.sentiment_score = sentiment.get("sentiment_score", 0.0)

            except ImportError:
                logger.debug("Sentiment tools not available")
        except Exception as e:
            logger.warning(f"Error computing sentiment: {e}")

        return features

    def _compute_trend(
        self,
        technical: TechnicalFeatures,
        prices: List[float],
    ) -> tuple[str, float]:
        """Compute trend direction and strength"""

        signals = []

        # SMA cross
        if technical.sma_20 and technical.sma_50:
            if technical.sma_20 > technical.sma_50:
                signals.append(1)  # bullish
            else:
                signals.append(-1)  # bearish

        # RSI
        if technical.rsi_14:
            if technical.rsi_14 > 60:
                signals.append(1)
            elif technical.rsi_14 < 40:
                signals.append(-1)
            else:
                signals.append(0)

        # MACD
        if technical.macd and technical.macd_signal:
            if technical.macd > technical.macd_signal:
                signals.append(1)
            else:
                signals.append(-1)

        # Price vs Bollinger
        if technical.bollinger_upper and technical.bollinger_lower and prices:
            current = prices[-1]
            if current > technical.bollinger_upper:
                signals.append(1)
            elif current < technical.bollinger_lower:
                signals.append(-1)
            else:
                signals.append(0)

        if not signals:
            return "neutral", 0.0

        avg_signal = sum(signals) / len(signals)

        if avg_signal > 0.3:
            direction = "bullish"
        elif avg_signal < -0.3:
            direction = "bearish"
        else:
            direction = "neutral"

        strength = abs(avg_signal)

        return direction, min(strength, 1.0)

    def _compute_volatility(
        self,
        market_data: MarketData,
        prices: List[float],
    ) -> str:
        """Compute volatility level"""

        if not prices or len(prices) < 10:
            return "normal"

        # Calculate recent price changes
        changes = []
        for i in range(1, min(20, len(prices))):
            pct_change = abs((prices[i] - prices[i-1]) / prices[i-1]) * 100
            changes.append(pct_change)

        if not changes:
            return "normal"

        avg_change = sum(changes) / len(changes)

        if avg_change < 0.5:
            return "low"
        elif avg_change < 2.0:
            return "normal"
        elif avg_change < 5.0:
            return "high"
        else:
            return "extreme"


# Singleton instance
_default_pipeline: Optional[DefaultFeaturesPipeline] = None


def get_features_pipeline() -> DefaultFeaturesPipeline:
    """Get the default features pipeline instance"""
    global _default_pipeline
    if _default_pipeline is None:
        _default_pipeline = DefaultFeaturesPipeline()
    return _default_pipeline
