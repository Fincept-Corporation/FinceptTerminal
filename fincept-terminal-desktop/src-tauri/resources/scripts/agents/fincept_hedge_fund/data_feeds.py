# -*- coding: utf-8 -*-
# data_feeds.py - Production Data Sources and Feed Management

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - end_date (string)
#   - FINANCIAL_DATASETS_API_KEY
#
# OUTPUT:
#   - Financial metrics: ROE, D/E, Current Ratio
#   - Financial line items: Revenue, Net Income, FCF, Debt, Equity, Retained Earnings, Operating Margin, R&D, Shares
#   - Market Cap
#
# PARAMETERS:
#   - period="annual"
#   - limit=10 years
"""

import asyncio
import aiohttp
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any, Union
import json
import logging
from dataclasses import dataclass
import redis
from config import CONFIG
import time
import hashlib


@dataclass
class DataPoint:
    """Standardized data point structure"""
    timestamp: datetime
    source: str
    data_type: str
    value: Union[float, str, Dict]
    confidence: float
    metadata: Dict[str, Any]


class DataCache:
    """Redis-based caching for data feeds"""

    def __init__(self):
        self.redis_client = redis.Redis(host='localhost', port=6379, db=0, decode_responses=True)
        self.default_ttl = CONFIG.agent.cache_ttl

    def get(self, key: str) -> Optional[Any]:
        """Get cached data"""
        try:
            data = self.redis_client.get(key)
            return json.loads(data) if data else None
        except Exception:
            return None

    def set(self, key: str, value: Any, ttl: Optional[int] = None) -> bool:
        """Set cached data"""
        try:
            ttl = ttl or self.default_ttl
            return self.redis_client.setex(key, ttl, json.dumps(value, default=str))
        except Exception:
            return False

    def generate_key(self, source: str, params: Dict) -> str:
        """Generate cache key from parameters"""
        param_str = json.dumps(params, sort_keys=True)
        return f"{source}:{hashlib.md5(param_str.encode()).hexdigest()}"


class EconomicDataFeed:
    """Federal Reserve Economic Data (FRED) API integration"""

    def __init__(self):
        self.base_url = "https://api.stlouisfed.org/fred"
        self.api_key = CONFIG.api.fred_api_key
        self.cache = DataCache()
        self.session = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def get_series(self, series_id: str, limit: int = 100) -> List[DataPoint]:
        """Get economic time series data"""
        cache_key = self.cache.generate_key("fred", {"series": series_id, "limit": limit})
        cached = self.cache.get(cache_key)

        if cached and CONFIG.agent.enable_caching:
            return [DataPoint(**dp) for dp in cached]

        url = f"{self.base_url}/series/observations"
        params = {
            "series_id": series_id,
            "api_key": self.api_key,
            "file_type": "json",
            "limit": limit,
            "sort_order": "desc"
        }

        try:
            async with self.session.get(url, params=params) as response:
                data = await response.json()
                observations = data.get("observations", [])

                data_points = []
                for obs in observations:
                    if obs["value"] != ".":
                        dp = DataPoint(
                            timestamp=datetime.strptime(obs["date"], "%Y-%m-%d"),
                            source="FRED",
                            data_type=series_id,
                            value=float(obs["value"]),
                            confidence=0.95,
                            metadata={"series_id": series_id}
                        )
                        data_points.append(dp)

                # Cache the results
                self.cache.set(cache_key, [dp.__dict__ for dp in data_points])
                return data_points

        except Exception as e:
            logging.error(f"Error fetching FRED data for {series_id}: {e}")
            return []


class NewsDataFeed:
    """News API integration for sentiment and events"""

    def __init__(self):
        self.api_key = CONFIG.api.newsapi_key
        self.base_url = "https://newsapi.org/v2"
        self.cache = DataCache()
        self.session = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def get_headlines(self, query: str, sources: List[str] = None,
                            hours_back: int = 24) -> List[DataPoint]:
        """Get news headlines with relevance scoring"""
        cache_key = self.cache.generate_key("news", {
            "query": query,
            "sources": sources,
            "hours": hours_back
        })
        cached = self.cache.get(cache_key)

        if cached and CONFIG.agent.enable_caching:
            return [DataPoint(**dp) for dp in cached]

        from_date = (datetime.now() - timedelta(hours=hours_back)).isoformat()

        params = {
            "q": query,
            "from": from_date,
            "sortBy": "relevancy",
            "apiKey": self.api_key,
            "language": "en",
            "pageSize": 100
        }

        if sources:
            params["sources"] = ",".join(sources)

        try:
            async with self.session.get(f"{self.base_url}/everything", params=params) as response:
                data = await response.json()
                articles = data.get("articles", [])

                data_points = []
                for article in articles:
                    # Calculate relevance confidence based on source reliability
                    source_name = article.get("source", {}).get("name", "").lower()
                    confidence = CONFIG.news_sources.get(source_name, 0.50)

                    dp = DataPoint(
                        timestamp=datetime.fromisoformat(article["publishedAt"].replace("Z", "+00:00")),
                        source=source_name,
                        data_type="news_headline",
                        value=article["title"],
                        confidence=confidence,
                        metadata={
                            "description": article.get("description", ""),
                            "url": article.get("url", ""),
                            "author": article.get("author", ""),
                            "query": query
                        }
                    )
                    data_points.append(dp)

                self.cache.set(cache_key, [dp.__dict__ for dp in data_points])
                return data_points

        except Exception as e:
            logging.error(f"Error fetching news data: {e}")
            return []


class MarketDataFeed:
    """Financial market data from multiple sources"""

    def __init__(self):
        self.finnhub_key = CONFIG.api.finnhub_api_key
        self.polygon_key = CONFIG.api.polygon_api_key
        self.cache = DataCache()
        self.session = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def get_institutional_trades(self, symbol: str) -> List[DataPoint]:
        """Get institutional trading data from Finnhub"""
        cache_key = self.cache.generate_key("institutional", {"symbol": symbol})
        cached = self.cache.get(cache_key)

        if cached and CONFIG.agent.enable_caching:
            return [DataPoint(**dp) for dp in cached]

        url = f"https://finnhub.io/api/v1/stock/institutional-portfolio"
        params = {
            "symbol": symbol,
            "token": self.finnhub_key
        }

        try:
            async with self.session.get(url, params=params) as response:
                data = await response.json()

                data_points = []
                for filing in data.get("data", []):
                    dp = DataPoint(
                        timestamp=datetime.now(),
                        source="Finnhub",
                        data_type="institutional_holding",
                        value=filing.get("change", 0),
                        confidence=0.85,
                        metadata={
                            "symbol": symbol,
                            "institution": filing.get("name", ""),
                            "shares": filing.get("share", 0),
                            "portfolio_percent": filing.get("portfolioPercent", 0)
                        }
                    )
                    data_points.append(dp)

                self.cache.set(cache_key, [dp.__dict__ for dp in data_points])
                return data_points

        except Exception as e:
            logging.error(f"Error fetching institutional data: {e}")
            return []

    async def get_insider_trades(self, symbol: str, days_back: int = 30) -> List[DataPoint]:
        """Get insider trading data"""
        cache_key = self.cache.generate_key("insider", {"symbol": symbol, "days": days_back})
        cached = self.cache.get(cache_key)

        if cached and CONFIG.agent.enable_caching:
            return [DataPoint(**dp) for dp in cached]

        from_date = (datetime.now() - timedelta(days=days_back)).strftime("%Y-%m-%d")
        to_date = datetime.now().strftime("%Y-%m-%d")

        url = f"https://finnhub.io/api/v1/stock/insider-transactions"
        params = {
            "symbol": symbol,
            "from": from_date,
            "to": to_date,
            "token": self.finnhub_key
        }

        try:
            async with self.session.get(url, params=params) as response:
                data = await response.json()

                data_points = []
                for trade in data.get("data", []):
                    dp = DataPoint(
                        timestamp=datetime.strptime(trade["transactionDate"], "%Y-%m-%d"),
                        source="Finnhub",
                        data_type="insider_trade",
                        value=trade.get("change", 0),
                        confidence=0.90,
                        metadata={
                            "symbol": symbol,
                            "name": trade.get("name", ""),
                            "share": trade.get("share", 0),
                            "transaction_code": trade.get("transactionCode", ""),
                            "transaction_price": trade.get("transactionPrice", 0)
                        }
                    )
                    data_points.append(dp)

                self.cache.set(cache_key, [dp.__dict__ for dp in data_points])
                return data_points

        except Exception as e:
            logging.error(f"Error fetching insider data: {e}")
            return []


class SentimentDataFeed:
    """Social media and sentiment data aggregation"""

    def __init__(self):
        self.reddit_id = CONFIG.api.reddit_client_id
        self.reddit_secret = CONFIG.api.reddit_client_secret
        self.twitter_token = CONFIG.api.twitter_bearer_token
        self.cache = DataCache()
        self.session = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def get_reddit_sentiment(self, subreddits: List[str], keywords: List[str]) -> List[DataPoint]:
        """Get Reddit sentiment for specific keywords"""
        cache_key = self.cache.generate_key("reddit", {
            "subreddits": subreddits,
            "keywords": keywords
        })
        cached = self.cache.get(cache_key)

        if cached and CONFIG.agent.enable_caching:
            return [DataPoint(**dp) for dp in cached]

        # Reddit API implementation would go here
        # For now, return mock structure
        data_points = []

        try:
            # This would be actual Reddit API calls
            for subreddit in subreddits:
                for keyword in keywords:
                    dp = DataPoint(
                        timestamp=datetime.now(),
                        source="Reddit",
                        data_type="social_sentiment",
                        value=np.random.uniform(-1, 1),  # Placeholder sentiment score
                        confidence=0.60,
                        metadata={
                            "subreddit": subreddit,
                            "keyword": keyword,
                            "post_count": np.random.randint(10, 100),
                            "engagement_score": np.random.uniform(0, 1)
                        }
                    )
                    data_points.append(dp)

            self.cache.set(cache_key, [dp.__dict__ for dp in data_points])
            return data_points

        except Exception as e:
            logging.error(f"Error fetching Reddit sentiment: {e}")
            return []


class GeopoliticalDataFeed:
    """Geopolitical events and risk monitoring"""

    def __init__(self):
        self.cache = DataCache()
        self.session = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def get_conflict_data(self) -> List[DataPoint]:
        """Get global conflict and tension data"""
        cache_key = self.cache.generate_key("geopolitical", {"type": "conflicts"})
        cached = self.cache.get(cache_key)

        if cached and CONFIG.agent.enable_caching:
            return [DataPoint(**dp) for dp in cached]

        # This would integrate with GDELT, ACLED, or similar sources
        # For production, you'd implement actual API calls
        data_points = []

        try:
            # Mock implementation - replace with real geopolitical data sources
            conflicts = ["Russia-Ukraine", "China-Taiwan", "Middle East", "North Korea"]

            for conflict in conflicts:
                dp = DataPoint(
                    timestamp=datetime.now(),
                    source="GDELT",
                    data_type="geopolitical_tension",
                    value=np.random.uniform(0, 10),  # Tension score 0-10
                    confidence=0.75,
                    metadata={
                        "conflict_region": conflict,
                        "escalation_risk": np.random.choice(["low", "medium", "high"]),
                        "affected_sectors": ["defense", "energy", "tech"],
                        "sanctions_risk": np.random.uniform(0, 1)
                    }
                )
                data_points.append(dp)

            self.cache.set(cache_key, [dp.__dict__ for dp in data_points])
            return data_points

        except Exception as e:
            logging.error(f"Error fetching geopolitical data: {e}")
            return []


class DataFeedManager:
    """Central manager for all data feeds"""

    def __init__(self):
        self.feeds = {
            "economic": EconomicDataFeed(),
            "news": NewsDataFeed(),
            "market": MarketDataFeed(),
            "sentiment": SentimentDataFeed(),
            "geopolitical": GeopoliticalDataFeed()
        }

    async def get_multi_source_data(self, data_requests: Dict[str, Dict]) -> Dict[str, List[DataPoint]]:
        """Get data from multiple sources concurrently"""
        results = {}

        async def fetch_data(feed_name: str, feed_obj: Any, request_params: Dict):
            async with feed_obj as feed:
                if feed_name == "economic":
                    data = await feed.get_series(**request_params)
                elif feed_name == "news":
                    data = await feed.get_headlines(**request_params)
                elif feed_name == "market":
                    if request_params.get("data_type") == "institutional":
                        data = await feed.get_institutional_trades(request_params["symbol"])
                    else:
                        data = await feed.get_insider_trades(**request_params)
                elif feed_name == "sentiment":
                    data = await feed.get_reddit_sentiment(**request_params)
                elif feed_name == "geopolitical":
                    data = await feed.get_conflict_data()
                else:
                    data = []

                results[feed_name] = data

        tasks = []
        for feed_name, request_params in data_requests.items():
            if feed_name in self.feeds:
                feed_obj = self.feeds[feed_name]
                tasks.append(fetch_data(feed_name, feed_obj, request_params))

        await asyncio.gather(*tasks, return_exceptions=True)
        return results

    def get_latest_data_summary(self) -> Dict[str, Any]:
        """Get summary of latest data availability"""
        summary = {
            "timestamp": datetime.now().isoformat(),
            "data_sources": list(self.feeds.keys()),
            "cache_status": "enabled" if CONFIG.agent.enable_caching else "disabled",
            "api_rate_limits": {
                "economic": f"{CONFIG.agent.max_api_calls_per_hour}/hour",
                "news": f"{CONFIG.agent.max_api_calls_per_hour}/hour",
                "market": f"{CONFIG.agent.max_api_calls_per_hour}/hour"
            }
        }
        return summary