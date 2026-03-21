"""
Sentiment Analysis Agent for Alpha Arena

Integrates with existing news fetching tools to provide
sentiment analysis for trading decisions.
"""

import sys
from pathlib import Path
from typing import Dict, List, Optional, Any, AsyncGenerator
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum

# Add scripts directory to path for imports
scripts_dir = Path(__file__).parent.parent.parent
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))

from alpha_arena.core.base_agent import BaseAgent
from alpha_arena.types.responses import StreamResponse
from alpha_arena.utils.logging import get_logger

logger = get_logger("sentiment_agent")


class SentimentLevel(str, Enum):
    """Sentiment classification levels."""
    VERY_BEARISH = "very_bearish"
    BEARISH = "bearish"
    NEUTRAL = "neutral"
    BULLISH = "bullish"
    VERY_BULLISH = "very_bullish"


@dataclass
class NewsArticle:
    """Represents a news article."""
    title: str
    description: str
    url: str
    publisher: str
    published_date: str
    sentiment_score: float = 0.0
    sentiment_label: str = "neutral"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "title": self.title,
            "description": self.description,
            "url": self.url,
            "publisher": self.publisher,
            "published_date": self.published_date,
            "sentiment_score": self.sentiment_score,
            "sentiment_label": self.sentiment_label,
        }


@dataclass
class SentimentResult:
    """Result of sentiment analysis."""
    symbol: str
    timestamp: datetime = field(default_factory=datetime.now)
    articles_analyzed: int = 0
    overall_sentiment: SentimentLevel = SentimentLevel.NEUTRAL
    sentiment_score: float = 0.0  # -1.0 (bearish) to 1.0 (bullish)
    confidence: float = 0.0
    bullish_count: int = 0
    bearish_count: int = 0
    neutral_count: int = 0
    key_headlines: List[str] = field(default_factory=list)
    sources: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "symbol": self.symbol,
            "timestamp": self.timestamp.isoformat(),
            "articles_analyzed": self.articles_analyzed,
            "overall_sentiment": self.overall_sentiment.value,
            "sentiment_score": self.sentiment_score,
            "confidence": self.confidence,
            "bullish_count": self.bullish_count,
            "bearish_count": self.bearish_count,
            "neutral_count": self.neutral_count,
            "key_headlines": self.key_headlines,
            "sources": self.sources,
        }

    def to_prompt_context(self) -> str:
        """Format for LLM context."""
        lines = [f"SENTIMENT ANALYSIS: {self.symbol}"]
        lines.append(f"Overall: {self.overall_sentiment.value} (score: {self.sentiment_score:.2f})")
        lines.append(f"Confidence: {self.confidence:.0%}")
        lines.append(f"Articles: {self.articles_analyzed} ({self.bullish_count} bullish, {self.bearish_count} bearish, {self.neutral_count} neutral)")

        if self.key_headlines:
            lines.append("\nKey Headlines:")
            for headline in self.key_headlines[:3]:
                lines.append(f"  - {headline[:100]}")

        return "\n".join(lines)


class SentimentAgent(BaseAgent):
    """
    Sentiment Analysis Agent using news and NLP.

    Integrates with existing fetch_company_news.py for news data.
    """

    def __init__(self, name: str = "SentimentAgent"):
        super().__init__(name)
        self._news_available = False
        self._cache: Dict[str, SentimentResult] = {}
        self._cache_ttl = 1800  # 30 minutes

        # Sentiment word lists for basic analysis
        self._bullish_words = {
            'surge', 'soar', 'rally', 'gains', 'growth', 'profit', 'rise', 'high',
            'bullish', 'positive', 'up', 'gain', 'beat', 'exceeds', 'strong',
            'record', 'upgrade', 'buy', 'optimistic', 'opportunity', 'breakout',
            'momentum', 'breakthrough', 'outperform', 'boom', 'upside'
        }
        self._bearish_words = {
            'plunge', 'crash', 'fall', 'drop', 'loss', 'decline', 'slump', 'low',
            'bearish', 'negative', 'down', 'miss', 'below', 'weak', 'cut',
            'downgrade', 'sell', 'pessimistic', 'risk', 'warning', 'concern',
            'fear', 'underperform', 'recession', 'downside', 'tumble'
        }

    async def initialize(self) -> bool:
        """Initialize the sentiment agent."""
        try:
            # Check if gnews is available
            import gnews
            self._news_available = True
            logger.info("GNews available for sentiment analysis")
        except ImportError:
            logger.warning("GNews not available - using limited sentiment analysis")
            self._news_available = False

        self._initialized = True
        return True

    async def stream(
        self,
        query: str,
        competition_id: str,
        cycle_number: int,
        dependencies: Optional[Dict] = None,
    ) -> AsyncGenerator[StreamResponse, None]:
        """Stream sentiment analysis results."""
        yield StreamResponse(content="Sentiment agent stream not implemented", event="info")

    def _analyze_text_sentiment(self, text: str) -> tuple[float, str]:
        """
        Analyze sentiment of text using word matching.

        Returns:
            Tuple of (score, label)
            Score ranges from -1.0 (very bearish) to 1.0 (very bullish)
        """
        if not text:
            return 0.0, "neutral"

        text_lower = text.lower()
        words = set(text_lower.split())

        bullish_matches = len(words & self._bullish_words)
        bearish_matches = len(words & self._bearish_words)

        total_matches = bullish_matches + bearish_matches
        if total_matches == 0:
            return 0.0, "neutral"

        # Calculate score
        score = (bullish_matches - bearish_matches) / max(total_matches, 1)
        score = max(-1.0, min(1.0, score))

        # Determine label
        if score > 0.5:
            label = "very_bullish"
        elif score > 0.15:
            label = "bullish"
        elif score < -0.5:
            label = "very_bearish"
        elif score < -0.15:
            label = "bearish"
        else:
            label = "neutral"

        return score, label

    async def fetch_news(
        self,
        query: str,
        max_results: int = 10,
        period: str = "7d",
    ) -> List[NewsArticle]:
        """
        Fetch news articles for analysis.

        Args:
            query: Search query (symbol, company name)
            max_results: Maximum articles to fetch
            period: Time period (7d, 1M, etc.)

        Returns:
            List of NewsArticle objects
        """
        if not self._news_available:
            return []

        try:
            import json
            import asyncio
            # Import and use existing fetch_company_news
            sys.path.insert(0, str(scripts_dir))
            from fetch_company_news import fetch_company_news

            # Run blocking news fetch in thread executor to avoid blocking event loop
            loop = asyncio.get_event_loop()
            result = await loop.run_in_executor(
                None, fetch_company_news, query, max_results, period
            )
            data = json.loads(result)

            if not data.get("success"):
                logger.warning(f"News fetch failed: {data.get('error')}")
                return []

            articles = []
            for article_data in data.get("data", []):
                # Analyze sentiment for each article
                title = article_data.get("title", "")
                desc = article_data.get("description", "")
                combined_text = f"{title} {desc}"

                score, label = self._analyze_text_sentiment(combined_text)

                articles.append(NewsArticle(
                    title=title,
                    description=desc,
                    url=article_data.get("url", ""),
                    publisher=article_data.get("publisher", "Unknown"),
                    published_date=article_data.get("published_date", ""),
                    sentiment_score=score,
                    sentiment_label=label,
                ))

            return articles

        except Exception as e:
            logger.error(f"Error fetching news: {e}")
            return []

    async def analyze_sentiment(
        self,
        symbol: str,
        use_cache: bool = True,
        max_articles: int = 15,
    ) -> SentimentResult:
        """
        Analyze market sentiment for a symbol.

        Args:
            symbol: Trading symbol (e.g., "AAPL", "BTC")
            use_cache: Whether to use cached result
            max_articles: Maximum articles to analyze

        Returns:
            SentimentResult with analysis
        """
        # Check cache
        if use_cache and symbol in self._cache:
            cached = self._cache[symbol]
            age = (datetime.now() - cached.timestamp).total_seconds()
            if age < self._cache_ttl:
                logger.info(f"Using cached sentiment for {symbol}")
                return cached

        logger.info(f"Analyzing sentiment for {symbol}")

        # Fetch and analyze news
        articles = await self.fetch_news(symbol, max_results=max_articles)

        if not articles:
            # Return neutral result if no articles
            result = SentimentResult(
                symbol=symbol,
                confidence=0.0,
            )
            return result

        # Aggregate sentiment
        total_score = 0.0
        bullish = 0
        bearish = 0
        neutral = 0
        headlines = []
        sources = set()

        for article in articles:
            total_score += article.sentiment_score

            if article.sentiment_label in ["bullish", "very_bullish"]:
                bullish += 1
            elif article.sentiment_label in ["bearish", "very_bearish"]:
                bearish += 1
            else:
                neutral += 1

            headlines.append(article.title)
            sources.add(article.publisher)

        # Calculate overall sentiment
        avg_score = total_score / len(articles) if articles else 0.0

        # Determine overall sentiment level
        if avg_score > 0.4:
            overall = SentimentLevel.VERY_BULLISH
        elif avg_score > 0.15:
            overall = SentimentLevel.BULLISH
        elif avg_score < -0.4:
            overall = SentimentLevel.VERY_BEARISH
        elif avg_score < -0.15:
            overall = SentimentLevel.BEARISH
        else:
            overall = SentimentLevel.NEUTRAL

        # Calculate confidence based on agreement
        max_count = max(bullish, bearish, neutral)
        confidence = max_count / len(articles) if articles else 0.0

        result = SentimentResult(
            symbol=symbol,
            articles_analyzed=len(articles),
            overall_sentiment=overall,
            sentiment_score=avg_score,
            confidence=confidence,
            bullish_count=bullish,
            bearish_count=bearish,
            neutral_count=neutral,
            key_headlines=headlines[:5],
            sources=list(sources),
        )

        # Cache result
        self._cache[symbol] = result

        return result

    async def get_market_mood(self, symbols: List[str]) -> Dict[str, Any]:
        """
        Get overall market mood from multiple symbols.

        Args:
            symbols: List of symbols to analyze

        Returns:
            Aggregated market mood
        """
        import asyncio

        results = {}
        total_score = 0.0
        analyzed_count = 0

        async def _analyze_one(sym: str) -> tuple:
            """Analyze a single symbol with timeout."""
            try:
                sentiment = await asyncio.wait_for(
                    self.analyze_sentiment(sym, max_articles=5),
                    timeout=10.0,
                )
                return sym, sentiment
            except asyncio.TimeoutError:
                logger.warning(f"Sentiment timeout for {sym}")
                return sym, SentimentResult(symbol=sym, confidence=0.0)
            except Exception as e:
                logger.warning(f"Sentiment error for {sym}: {e}")
                return sym, SentimentResult(symbol=sym, confidence=0.0)

        # Run all symbols concurrently
        tasks = [_analyze_one(s) for s in symbols]
        completed = await asyncio.gather(*tasks)

        for sym, sentiment in completed:
            results[sym] = sentiment.to_dict()
            total_score += sentiment.sentiment_score
            analyzed_count += 1

        avg_score = total_score / analyzed_count if analyzed_count else 0.0

        if avg_score > 0.3:
            overall_mood = "risk_on"
        elif avg_score < -0.3:
            overall_mood = "risk_off"
        else:
            overall_mood = "mixed"

        return {
            "symbols_analyzed": analyzed_count,
            "overall_mood": overall_mood,
            "average_sentiment": avg_score,
            "individual_sentiments": results,
        }

    def clear_cache(self, symbol: Optional[str] = None):
        """Clear sentiment cache."""
        if symbol:
            self._cache.pop(symbol, None)
        else:
            self._cache.clear()


# Singleton instance
_sentiment_agent: Optional[SentimentAgent] = None


async def get_sentiment_agent() -> SentimentAgent:
    """Get the sentiment agent singleton."""
    global _sentiment_agent
    if _sentiment_agent is None:
        _sentiment_agent = SentimentAgent()
        await _sentiment_agent.initialize()
    return _sentiment_agent


async def get_sentiment_context(symbol: str) -> str:
    """
    Get sentiment context for a symbol.

    Convenience function for adding sentiment to trading prompts.
    """
    agent = await get_sentiment_agent()
    result = await agent.analyze_sentiment(symbol)
    return result.to_prompt_context()
