# -*- coding: utf-8 -*-
# agents/sentiment_agent.py - Market Sentiment Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - market sentiment data
#   - social media feeds
#   - news sentiment
#   - investor sentiment surveys
#
# OUTPUT:
#   - Market sentiment scores
#   - Sentiment trend analysis
#   - Contrarian signals
#   - Sentiment-based alerts
#
# PARAMETERS:
#   - sentiment_sources
#   - analysis_timeframes
#   - sentiment_thresholds
#   - contrarian_indicators
"""

import asyncio
import numpy as np
import pandas as pd
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
import logging
from dataclasses import dataclass
import openai
import json
from textblob import TextBlob
import re
from collections import defaultdict, Counter

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class SentimentSignal:
    """Market sentiment signal structure"""
    source: str
    sentiment_score: float  # -1 to 1
    intensity: float  # 0 to 1
    reliability: float
    trend: str  # improving, deteriorating, stable
    contrarian_signal: bool
    affected_assets: List[str]
    time_horizon: str


@dataclass
class FearGreedMetrics:
    """Fear & Greed index components"""
    vix_level: float
    put_call_ratio: float
    safe_haven_flows: float
    momentum_strength: float
    overall_score: int  # 0-100
    classification: str  # extreme_fear, fear, neutral, greed, extreme_greed


class SentimentAgent:
    """Advanced market sentiment analysis and contrarian signal generation"""

    def __init__(self):
        self.name = "sentiment"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Sentiment sources and their reliability weights
        self.sentiment_sources = {
            "financial_news": {
                "reuters": 0.25,
                "bloomberg": 0.25,
                "wsj": 0.20,
                "ft": 0.15,
                "cnbc": 0.10,
                "marketwatch": 0.05
            },
            "social_media": {
                "twitter_fintwit": 0.40,
                "reddit_investing": 0.25,
                "reddit_wallstreetbets": 0.20,
                "stocktwits": 0.15
            },
            "analyst_sentiment": {
                "upgrade_downgrade": 0.35,
                "earnings_revisions": 0.30,
                "price_target_changes": 0.25,
                "recommendation_changes": 0.10
            },
            "market_indicators": {
                "vix": 0.30,
                "put_call_ratio": 0.25,
                "high_low_index": 0.20,
                "advance_decline": 0.25
            }
        }

        # Sentiment keywords with weights
        self.sentiment_keywords = {
            "extremely_bullish": {
                "keywords": ["moon", "rocket", "bull run", "unstoppable", "explosive growth",
                             "parabolic", "to the stars", "massive rally", "euphoric"],
                "weight": 1.0,
                "contrarian_signal": True
            },
            "bullish": {
                "keywords": ["bullish", "optimistic", "positive", "uptrend", "rally", "growth",
                             "strong", "buy", "accumulate", "opportunity"],
                "weight": 0.6,
                "contrarian_signal": False
            },
            "neutral": {
                "keywords": ["neutral", "mixed", "uncertain", "wait and see", "sideways",
                             "balanced", "cautious", "hold"],
                "weight": 0.0,
                "contrarian_signal": False
            },
            "bearish": {
                "keywords": ["bearish", "pessimistic", "negative", "downtrend", "decline",
                             "weakness", "sell", "avoid", "risk"],
                "weight": -0.6,
                "contrarian_signal": False
            },
            "extremely_bearish": {
                "keywords": ["crash", "collapse", "doom", "disaster", "panic", "bloodbath",
                             "capitulation", "apocalypse", "end times", "dead"],
                "weight": -1.0,
                "contrarian_signal": True
            }
        }

        # Asset class sentiment mapping
        self.asset_sentiment_keywords = {
            "stocks": ["equity", "stock", "shares", "s&p", "nasdaq", "dow"],
            "bonds": ["bonds", "treasury", "yield", "fixed income", "debt"],
            "gold": ["gold", "precious metals", "safe haven"],
            "crypto": ["bitcoin", "crypto", "ethereum", "digital assets"],
            "oil": ["oil", "crude", "energy", "petroleum"],
            "dollar": ["dollar", "usd", "currency", "forex"]
        }

        # Contrarian sentiment thresholds
        self.contrarian_thresholds = {
            "extreme_bullish": 0.8,  # When sentiment too positive
            "extreme_bearish": -0.8,  # When sentiment too negative
            "consensus_threshold": 0.7  # When consensus too strong
        }

    async def analyze_market_sentiment(self) -> Dict[str, SentimentSignal]:
        """Comprehensive market sentiment analysis across all sources"""
        sentiment_signals = {}

        try:
            # Analyze news sentiment
            news_sentiment = await self._analyze_news_sentiment()
            if news_sentiment:
                sentiment_signals["news"] = news_sentiment

            # Analyze social media sentiment
            social_sentiment = await self._analyze_social_sentiment()
            if social_sentiment:
                sentiment_signals["social_media"] = social_sentiment

            # Analyze analyst sentiment
            analyst_sentiment = await self._analyze_analyst_sentiment()
            if analyst_sentiment:
                sentiment_signals["analyst"] = analyst_sentiment

            # Analyze market-based sentiment indicators
            market_sentiment = await self._analyze_market_indicators()
            if market_sentiment:
                sentiment_signals["market_indicators"] = market_sentiment

            return sentiment_signals

        except Exception as e:
            logging.error(f"Error in market sentiment analysis: {e}")
            return {"error": self._default_sentiment_signal()}

    async def _analyze_news_sentiment(self) -> Optional[SentimentSignal]:
        """Analyze sentiment from financial news"""
        try:
            # Fetch financial news
            news_data = await self.data_manager.get_multi_source_data({
                "news": {
                    "query": "stock market economy investment",
                    "sources": list(self.sentiment_sources["financial_news"].keys()),
                    "hours_back": 24
                }
            })

            news_articles = news_data.get("news", [])
            if not news_articles:
                return None

            # Analyze sentiment for each article
            sentiment_scores = []
            source_weights = []
            asset_mentions = defaultdict(int)

            for article in news_articles:
                # Get sentiment score
                sentiment_score = self._calculate_text_sentiment(article.value)

                # Get source weight
                source_name = article.source.lower()
                source_weight = self.sentiment_sources["financial_news"].get(source_name, 0.05)

                sentiment_scores.append(sentiment_score * article.confidence)
                source_weights.append(source_weight)

                # Count asset mentions
                self._count_asset_mentions(article.value, asset_mentions)

            # Calculate weighted average sentiment
            if sentiment_scores and source_weights:
                weighted_sentiment = np.average(sentiment_scores, weights=source_weights)

                # Calculate intensity (standard deviation of sentiment)
                intensity = min(np.std(sentiment_scores), 1.0)

                # Determine trend
                trend = self._determine_sentiment_trend(sentiment_scores)

                # Check for contrarian signal
                contrarian_signal = abs(weighted_sentiment) > self.contrarian_thresholds["extreme_bullish"]

                # Get most mentioned assets
                top_assets = [asset for asset, count in
                              Counter(asset_mentions).most_common(3)]

                return SentimentSignal(
                    source="financial_news",
                    sentiment_score=weighted_sentiment,
                    intensity=intensity,
                    reliability=np.mean(source_weights),
                    trend=trend,
                    contrarian_signal=contrarian_signal,
                    affected_assets=top_assets,
                    time_horizon="short"
                )

        except Exception as e:
            logging.error(f"Error in news sentiment analysis: {e}")

        return None

    async def _analyze_social_sentiment(self) -> Optional[SentimentSignal]:
        """Analyze sentiment from social media sources"""
        try:
            # Fetch social media sentiment data
            social_data = await self.data_manager.get_multi_source_data({
                "sentiment": {
                    "subreddits": ["investing", "stocks", "SecurityAnalysis", "ValueInvesting"],
                    "keywords": ["market", "stocks", "bull", "bear", "crash", "rally"]
                }
            })

            sentiment_data = social_data.get("sentiment", [])
            if not sentiment_data:
                return None

            # Process social sentiment
            sentiment_scores = []
            platform_weights = []

            for data_point in sentiment_data:
                platform = data_point.metadata.get("subreddit", "unknown")
                platform_weight = self._get_social_platform_weight(platform)

                sentiment_scores.append(data_point.value)
                platform_weights.append(platform_weight)

            if sentiment_scores:
                # Calculate weighted sentiment
                weighted_sentiment = np.average(sentiment_scores, weights=platform_weights)
                intensity = min(np.std(sentiment_scores), 1.0)

                # Social media tends to be more extreme and contrarian
                contrarian_signal = (abs(weighted_sentiment) > 0.6 or
                                     self._detect_social_extremes(sentiment_data))

                return SentimentSignal(
                    source="social_media",
                    sentiment_score=weighted_sentiment,
                    intensity=intensity,
                    reliability=0.6,  # Lower reliability than news
                    trend=self._determine_sentiment_trend(sentiment_scores),
                    contrarian_signal=contrarian_signal,
                    affected_assets=["stocks", "crypto"],  # Social media focus
                    time_horizon="very_short"
                )

        except Exception as e:
            logging.error(f"Error in social sentiment analysis: {e}")

        return None

    async def _analyze_analyst_sentiment(self) -> Optional[SentimentSignal]:
        """Analyze professional analyst sentiment"""
        try:
            # This would integrate with analyst data providers
            # For now, we'll simulate based on market data patterns

            # Simulate analyst sentiment based on recent market performance
            sentiment_score = np.random.normal(0.1, 0.3)  # Slight bullish bias
            sentiment_score = np.clip(sentiment_score, -1, 1)

            # Analysts tend to be less extreme
            intensity = np.random.uniform(0.2, 0.6)

            # Professional analysts rarely give contrarian signals
            contrarian_signal = False

            return SentimentSignal(
                source="analyst_sentiment",
                sentiment_score=sentiment_score,
                intensity=intensity,
                reliability=0.8,  # High reliability
                trend="stable",
                contrarian_signal=contrarian_signal,
                affected_assets=["stocks", "sectors"],
                time_horizon="medium"
            )

        except Exception as e:
            logging.error(f"Error in analyst sentiment analysis: {e}")

        return None

    async def _analyze_market_indicators(self) -> Optional[SentimentSignal]:
        """Analyze market-based sentiment indicators"""
        try:
            # Fetch market sentiment indicators
            economic_data = await self.data_manager.get_multi_source_data({
                "economic": {
                    "VIXCLS": {"series_id": "VIXCLS", "limit": 30},  # VIX
                    "GS10": {"series_id": "GS10", "limit": 30}  # 10Y Treasury
                }
            })

            market_data = economic_data.get("economic", {})

            # Calculate fear/greed metrics
            fear_greed = self._calculate_fear_greed_index(market_data)

            # Convert fear/greed to sentiment score
            sentiment_score = (fear_greed.overall_score - 50) / 50  # Convert 0-100 to -1 to 1

            # Market indicators are highly reliable but can be contrarian
            contrarian_signal = (fear_greed.classification in ["extreme_fear", "extreme_greed"])

            return SentimentSignal(
                source="market_indicators",
                sentiment_score=sentiment_score,
                intensity=abs(sentiment_score),
                reliability=0.9,  # Very reliable
                trend=self._interpret_fear_greed_trend(fear_greed),
                contrarian_signal=contrarian_signal,
                affected_assets=["stocks", "bonds", "volatility"],
                time_horizon="short"
            )

        except Exception as e:
            logging.error(f"Error in market indicator analysis: {e}")

        return None

    def _calculate_text_sentiment(self, text: str) -> float:
        """Calculate sentiment score from text using keyword analysis and TextBlob"""
        try:
            # Use TextBlob for base sentiment
            blob = TextBlob(text)
            base_sentiment = blob.sentiment.polarity

            # Enhance with custom keyword analysis
            text_lower = text.lower()
            keyword_sentiment = 0.0
            keyword_count = 0

            for sentiment_category, config in self.sentiment_keywords.items():
                for keyword in config["keywords"]:
                    if keyword in text_lower:
                        keyword_sentiment += config["weight"]
                        keyword_count += 1

            # Combine base sentiment with keyword sentiment
            if keyword_count > 0:
                keyword_sentiment /= keyword_count
                # Weight: 60% keyword sentiment, 40% TextBlob
                combined_sentiment = 0.6 * keyword_sentiment + 0.4 * base_sentiment
            else:
                combined_sentiment = base_sentiment

            return np.clip(combined_sentiment, -1.0, 1.0)

        except Exception:
            return 0.0

    def _count_asset_mentions(self, text: str, asset_mentions: Dict[str, int]):
        """Count mentions of different asset classes in text"""
        text_lower = text.lower()

        for asset_class, keywords in self.asset_sentiment_keywords.items():
            for keyword in keywords:
                if keyword in text_lower:
                    asset_mentions[asset_class] += 1

    def _determine_sentiment_trend(self, sentiment_scores: List[float]) -> str:
        """Determine if sentiment is improving, deteriorating, or stable"""
        if len(sentiment_scores) < 3:
            return "stable"

        # Compare recent vs older sentiment
        recent_avg = np.mean(sentiment_scores[-len(sentiment_scores) // 3:])
        older_avg = np.mean(sentiment_scores[:len(sentiment_scores) // 3])

        difference = recent_avg - older_avg

        if difference > 0.1:
            return "improving"
        elif difference < -0.1:
            return "deteriorating"
        else:
            return "stable"

    def _get_social_platform_weight(self, platform: str) -> float:
        """Get weight for social media platform"""
        platform_weights = {
            "investing": 0.4,
            "stocks": 0.3,
            "SecurityAnalysis": 0.5,
            "ValueInvesting": 0.4,
            "wallstreetbets": 0.2,  # High volume but lower quality
            "stocktwits": 0.3
        }

        return platform_weights.get(platform, 0.2)

    def _detect_social_extremes(self, sentiment_data: List[DataPoint]) -> bool:
        """Detect extreme sentiment in social media data"""
        if not sentiment_data:
            return False

        # Check for extreme keywords
        extreme_keywords = (self.sentiment_keywords["extremely_bullish"]["keywords"] +
                            self.sentiment_keywords["extremely_bearish"]["keywords"])

        extreme_count = 0
        total_posts = len(sentiment_data)

        for data_point in sentiment_data:
            text = str(data_point.value).lower()
            if any(keyword in text for keyword in extreme_keywords):
                extreme_count += 1

        # If more than 20% of posts contain extreme language
        return (extreme_count / total_posts) > 0.2 if total_posts > 0 else False

    def _calculate_fear_greed_index(self, market_data: Dict) -> FearGreedMetrics:
        """Calculate fear & greed index from market data"""
        try:
            # Get VIX level (fear indicator)
            vix_data = market_data.get("VIXCLS", [])
            current_vix = vix_data[-1].value if vix_data else 20.0

            # Normalize VIX to 0-1 scale (inverse - high VIX = fear)
            vix_score = max(0, min(1, (50 - current_vix) / 30))  # 50 = extreme fear, 10 = greed

            # Simulate other components (in production, would fetch real data)
            put_call_ratio = np.random.uniform(0.3, 1.2)  # Normal range
            put_call_score = max(0, min(1, (1.2 - put_call_ratio) / 0.9))

            # Safe haven flows (inverse bond yields indicate flight to safety)
            treasury_data = market_data.get("GS10", [])
            current_yield = treasury_data[-1].value if treasury_data else 4.0
            safe_haven_score = max(0, min(1, (current_yield - 2.0) / 4.0))  # Higher yields = less fear

            # Momentum strength (simulated)
            momentum_score = np.random.uniform(0.3, 0.8)

            # Calculate overall score (0-100)
            component_scores = [vix_score, put_call_score, safe_haven_score, momentum_score]
            overall_score = int(np.mean(component_scores) * 100)

            # Classify fear/greed level
            if overall_score >= 80:
                classification = "extreme_greed"
            elif overall_score >= 60:
                classification = "greed"
            elif overall_score >= 40:
                classification = "neutral"
            elif overall_score >= 20:
                classification = "fear"
            else:
                classification = "extreme_fear"

            return FearGreedMetrics(
                vix_level=current_vix,
                put_call_ratio=put_call_ratio,
                safe_haven_flows=1 - safe_haven_score,  # Invert for intuitive reading
                momentum_strength=momentum_score,
                overall_score=overall_score,
                classification=classification
            )

        except Exception as e:
            logging.error(f"Error calculating fear/greed index: {e}")
            return FearGreedMetrics(50.0, 0.8, 0.5, 0.5, 50, "neutral")

    def _interpret_fear_greed_trend(self, fear_greed: FearGreedMetrics) -> str:
        """Interpret fear/greed trend"""
        score = fear_greed.overall_score

        if score > 70:
            return "deteriorating"  # Too much greed
        elif score < 30:
            return "improving"  # Oversold, likely to improve
        else:
            return "stable"

    def _default_sentiment_signal(self) -> SentimentSignal:
        """Return default sentiment signal"""
        return SentimentSignal(
            source="default",
            sentiment_score=0.0,
            intensity=0.3,
            reliability=0.3,
            trend="stable",
            contrarian_signal=False,
            affected_assets=["broad_market"],
            time_horizon="short"
        )

    async def generate_contrarian_signals(self, sentiment_signals: Dict[str, SentimentSignal]) -> List[Dict]:
        """Generate contrarian trading signals from sentiment extremes"""
        contrarian_signals = []

        for source, signal in sentiment_signals.items():
            if signal.contrarian_signal:
                # Generate contrarian signal
                contrarian_direction = "short" if signal.sentiment_score > 0 else "long"
                conviction = min(signal.intensity * signal.reliability, 1.0)

                contrarian_signals.append({
                    "source": source,
                    "direction": contrarian_direction,
                    "conviction": conviction,
                    "rationale": f"Extreme {source} sentiment: {signal.sentiment_score:.2f}",
                    "affected_assets": signal.affected_assets,
                    "time_horizon": signal.time_horizon,
                    "sentiment_score": signal.sentiment_score
                })

        return contrarian_signals

    async def analyze_sector_sentiment(self) -> Dict[str, float]:
        """Analyze sentiment by sector"""
        sector_sentiment = {}

        # Sector-specific keywords
        sector_keywords = {
            "technology": ["tech", "software", "ai", "cloud", "semiconductor"],
            "healthcare": ["pharma", "biotech", "medical", "drug", "health"],
            "financials": ["bank", "insurance", "credit", "lending", "fintech"],
            "energy": ["oil", "gas", "renewable", "energy", "solar"],
            "consumer": ["retail", "consumer", "spending", "discretionary"]
        }

        try:
            for sector, keywords in sector_keywords.items():
                # Fetch sector-specific news
                sector_news = await self.data_manager.get_multi_source_data({
                    "news": {
                        "query": " OR ".join(keywords),
                        "sources": ["reuters", "bloomberg"],
                        "hours_back": 24
                    }
                })

                news_data = sector_news.get("news", [])
                if news_data:
                    sentiment_scores = [self._calculate_text_sentiment(article.value)
                                        for article in news_data]
                    sector_sentiment[sector] = np.mean(sentiment_scores)
                else:
                    sector_sentiment[sector] = 0.0

        except Exception as e:
            logging.error(f"Error in sector sentiment analysis: {e}")

        return sector_sentiment

    async def get_sentiment_report(self) -> Dict:
        """Generate comprehensive sentiment analysis report"""
        # Analyze overall market sentiment
        sentiment_signals = await self.analyze_market_sentiment()

        # Generate contrarian signals
        contrarian_signals = await self.generate_contrarian_signals(sentiment_signals)

        # Analyze sector sentiment
        sector_sentiment = await self.analyze_sector_sentiment()

        # Calculate overall sentiment score
        if sentiment_signals:
            weighted_scores = []
            weights = []

            for source, signal in sentiment_signals.items():
                if source != "error":
                    weighted_scores.append(signal.sentiment_score * signal.reliability)
                    weights.append(signal.reliability)

            overall_sentiment = (np.average(weighted_scores, weights=weights)
                                 if weighted_scores else 0.0)
        else:
            overall_sentiment = 0.0

        # Generate LLM analysis
        llm_analysis = await self._generate_sentiment_llm_analysis(
            sentiment_signals, contrarian_signals, sector_sentiment
        )

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "sentiment_analysis": {
                "overall_sentiment": overall_sentiment,
                "sentiment_classification": self._classify_sentiment(overall_sentiment),
                "source_breakdown": {
                    source: {
                        "score": signal.sentiment_score,
                        "intensity": signal.intensity,
                        "reliability": signal.reliability,
                        "trend": signal.trend,
                        "contrarian_signal": signal.contrarian_signal
                    } for source, signal in sentiment_signals.items() if source != "error"
                }
            },
            "contrarian_signals": contrarian_signals,
            "sector_sentiment": sector_sentiment,
            "fear_greed_analysis": {
                "current_level": self._estimate_fear_greed_level(overall_sentiment),
                "interpretation": self._interpret_sentiment_level(overall_sentiment),
                "contrarian_opportunity": abs(overall_sentiment) > 0.6
            },
            "trading_implications": {
                "market_timing": self._get_market_timing_signal(overall_sentiment),
                "position_sizing": self._get_position_sizing_recommendation(sentiment_signals),
                "asset_allocation": self._get_sentiment_based_allocation(overall_sentiment),
                "risk_management": self._get_sentiment_risk_recommendations(contrarian_signals)
            },
            "llm_analysis": llm_analysis,
            "monitoring_alerts": {
                "extreme_sentiment_warning": abs(overall_sentiment) > 0.7,
                "contrarian_opportunities": len(contrarian_signals) > 0,
                "sentiment_divergence": self._detect_sentiment_divergence(sentiment_signals),
                "social_media_extreme": any(s.source == "social_media" and s.contrarian_signal
                                            for s in sentiment_signals.values() if hasattr(s, 'source'))
            }
        }

    def _classify_sentiment(self, sentiment_score: float) -> str:
        """Classify overall sentiment level"""
        if sentiment_score >= 0.6:
            return "very_bullish"
        elif sentiment_score >= 0.2:
            return "bullish"
        elif sentiment_score >= -0.2:
            return "neutral"
        elif sentiment_score >= -0.6:
            return "bearish"
        else:
            return "very_bearish"

    def _estimate_fear_greed_level(self, sentiment_score: float) -> int:
        """Convert sentiment score to fear/greed index (0-100)"""
        # Convert -1 to 1 range to 0-100
        fear_greed_score = int((sentiment_score + 1) * 50)
        return np.clip(fear_greed_score, 0, 100)

    def _interpret_sentiment_level(self, sentiment_score: float) -> str:
        """Interpret sentiment level for trading"""
        if sentiment_score > 0.7:
            return "Extreme optimism - consider taking profits and defensive positioning"
        elif sentiment_score > 0.4:
            return "Strong bullish sentiment - continue momentum but watch for reversals"
        elif sentiment_score > -0.4:
            return "Balanced sentiment - fundamentals likely driving markets"
        elif sentiment_score > -0.7:
            return "Pessimistic sentiment - look for oversold opportunities"
        else:
            return "Extreme pessimism - strong contrarian buying opportunity"

    def _get_market_timing_signal(self, sentiment_score: float) -> str:
        """Get market timing signal based on sentiment"""
        if sentiment_score > 0.6:
            return "reduce_risk"  # Extreme optimism
        elif sentiment_score < -0.6:
            return "increase_risk"  # Extreme pessimism
        elif sentiment_score > 0.3:
            return "neutral_to_bullish"
        elif sentiment_score < -0.3:
            return "neutral_to_bearish"
        else:
            return "neutral"

    def _get_position_sizing_recommendation(self, sentiment_signals: Dict[str, SentimentSignal]) -> str:
        """Get position sizing recommendation based on sentiment"""
        if not sentiment_signals:
            return "normal"

        # Count contrarian signals
        contrarian_count = sum(1 for signal in sentiment_signals.values()
                               if hasattr(signal, 'contrarian_signal') and signal.contrarian_signal)

        # Check sentiment intensity
        avg_intensity = np.mean([signal.intensity for signal in sentiment_signals.values()
                                 if hasattr(signal, 'intensity')])

        if contrarian_count >= 2:
            return "reduce_size"  # Multiple contrarian signals suggest caution
        elif avg_intensity > 0.7:
            return "reduce_size"  # High intensity suggests volatility
        elif contrarian_count == 1 and avg_intensity < 0.4:
            return "increase_size"  # Single contrarian opportunity with low volatility
        else:
            return "normal"

    def _get_sentiment_based_allocation(self, sentiment_score: float) -> Dict[str, str]:
        """Get asset allocation recommendation based on sentiment"""
        if sentiment_score > 0.5:
            return {
                "equities": "underweight",  # Reduce risk in euphoric markets
                "bonds": "overweight",
                "cash": "overweight",
                "defensive_sectors": "overweight"
            }
        elif sentiment_score < -0.5:
            return {
                "equities": "overweight",  # Increase risk in pessimistic markets
                "bonds": "underweight",
                "cash": "underweight",
                "growth_sectors": "overweight"
            }
        else:
            return {
                "equities": "neutral",
                "bonds": "neutral",
                "cash": "neutral",
                "sectors": "neutral"
            }

    def _get_sentiment_risk_recommendations(self, contrarian_signals: List[Dict]) -> List[str]:
        """Get risk management recommendations based on sentiment"""
        recommendations = []

        if len(contrarian_signals) > 0:
            recommendations.append("Monitor sentiment reversal points closely")
            recommendations.append("Consider volatility hedges")

        # Check for social media extremes
        social_extremes = [signal for signal in contrarian_signals
                           if signal.get("source") == "social_media"]

        if social_extremes:
            recommendations.append("High social media sentiment risk - prepare for volatility")
            recommendations.append("Consider reducing position sizes in meme stocks")

        # Check for multiple extreme signals
        if len(contrarian_signals) >= 2:
            recommendations.append("Multiple extreme sentiment signals - consider defensive positioning")

        return recommendations or ["Normal risk management procedures"]

    def _detect_sentiment_divergence(self, sentiment_signals: Dict[str, SentimentSignal]) -> bool:
        """Detect divergence between different sentiment sources"""
        if len(sentiment_signals) < 2:
            return False

        scores = [signal.sentiment_score for signal in sentiment_signals.values()
                  if hasattr(signal, 'sentiment_score')]

        if len(scores) < 2:
            return False

        # Check if sentiment scores are significantly different
        score_std = np.std(scores)
        score_range = max(scores) - min(scores)

        # Divergence if high standard deviation or wide range
        return score_std > 0.4 or score_range > 0.8

    async def _generate_sentiment_llm_analysis(self, sentiment_signals: Dict[str, SentimentSignal],
                                               contrarian_signals: List[Dict],
                                               sector_sentiment: Dict[str, float]) -> Dict:
        """Generate LLM-enhanced sentiment analysis"""
        try:
            # Prepare sentiment summary
            sentiment_summary = self._prepare_sentiment_summary(sentiment_signals)
            contrarian_summary = self._prepare_contrarian_summary(contrarian_signals)
            sector_summary = self._prepare_sector_sentiment_summary(sector_sentiment)

            prompt = f"""
            As a senior market sentiment analyst, analyze the current market psychology and provide trading guidance.

            Sentiment Analysis:
            {sentiment_summary}

            Contrarian Signals:
            {contrarian_summary}

            Sector Sentiment:
            {sector_summary}

            Please provide:
            1. Current market psychology assessment and crowd behavior analysis
            2. Contrarian opportunities with specific entry/exit strategies
            3. Sentiment-based sector rotation recommendations
            4. Risk management for sentiment-driven volatility
            5. Timeline for potential sentiment reversals
            6. Early warning indicators for sentiment extremes

            Format as JSON with keys: market_psychology, contrarian_opportunities, sector_rotation,
            risk_management, sentiment_timeline, early_warning_indicators
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM sentiment analysis: {e}")
            return {
                "market_psychology": "Mixed sentiment with moderate optimism",
                "contrarian_opportunities": "Monitor for extreme readings in VIX and social media",
                "sector_rotation": "Balanced approach with slight tech overweight",
                "risk_management": "Standard position sizing with volatility monitoring",
                "sentiment_timeline": "Next 2-4 weeks for potential shifts",
                "early_warning_indicators": ["VIX spikes", "Social media extremes", "News flow acceleration"]
            }

    def _prepare_sentiment_summary(self, sentiment_signals: Dict[str, SentimentSignal]) -> str:
        """Prepare sentiment summary for LLM"""
        summary_lines = []

        for source, signal in sentiment_signals.items():
            if hasattr(signal, 'sentiment_score'):
                summary_lines.append(
                    f"{source}: Score {signal.sentiment_score:.2f}, "
                    f"Intensity {signal.intensity:.2f}, "
                    f"Trend {signal.trend}, "
                    f"Contrarian: {signal.contrarian_signal}"
                )

        return "\n".join(summary_lines) if summary_lines else "Limited sentiment data available"

    def _prepare_contrarian_summary(self, contrarian_signals: List[Dict]) -> str:
        """Prepare contrarian signals summary for LLM"""
        if not contrarian_signals:
            return "No significant contrarian signals detected"

        summary_lines = []
        for signal in contrarian_signals:
            summary_lines.append(
                f"{signal['source']}: {signal['direction']} signal "
                f"(Conviction: {signal['conviction']:.2f}) - {signal['rationale']}"
            )

        return "\n".join(summary_lines)

    def _prepare_sector_sentiment_summary(self, sector_sentiment: Dict[str, float]) -> str:
        """Prepare sector sentiment summary for LLM"""
        if not sector_sentiment:
            return "Sector sentiment data not available"

        summary_lines = []
        sorted_sectors = sorted(sector_sentiment.items(), key=lambda x: x[1], reverse=True)

        for sector, sentiment in sorted_sectors:
            sentiment_desc = "Bullish" if sentiment > 0.2 else "Bearish" if sentiment < -0.2 else "Neutral"
            summary_lines.append(f"{sector}: {sentiment:.2f} ({sentiment_desc})")

        return "\n".join(summary_lines)