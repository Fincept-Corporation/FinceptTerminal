# -*- coding: utf-8 -*-
# agents/behavioral_agent.py - Behavioral Finance Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - market sentiment data
#   - behavioral indicators
#   - psychological market patterns
#
# OUTPUT:
#   - Behavioral finance signals
#   - Market sentiment analysis
#   - Anomaly detection results
#   - Behavioral pattern recognition
#
# PARAMETERS:
#   - sentiment_data_sources
#   - behavioral_models
#   - anomaly_thresholds
#   - psychological_indicators
"""

import asyncio
import numpy as np
from datetime import datetime, timedelta
from typing import Dict, List, Optional
import logging
from dataclasses import dataclass
import openai

from config import CONFIG
from data_feeds import DataFeedManager


@dataclass
class BehaviorSignal:
    """Behavioral finance signal"""
    behavior_type: str
    intensity: float  # 0-1
    market_impact: str  # bullish, bearish, neutral
    contrarian_opportunity: bool
    affected_assets: List[str]
    confidence: float


class BehavioralAgent:
    """Behavioral finance analysis - crowd psychology and market inefficiencies"""

    def __init__(self):
        self.name = "behavioral"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        self.behavioral_patterns = {
            "herd_behavior": {
                "indicators": ["consensus_trades", "crowded_positions", "momentum_chasing"],
                "contrarian_threshold": 0.7
            },
            "overconfidence": {
                "indicators": ["excessive_trading", "narrow_spreads", "low_volatility"],
                "contrarian_threshold": 0.6
            },
            "loss_aversion": {
                "indicators": ["selling_pressure", "defensive_positioning", "cash_hoarding"],
                "contrarian_threshold": 0.8
            },
            "anchoring": {
                "indicators": ["resistance_levels", "round_numbers", "historical_highs"],
                "contrarian_threshold": 0.5
            },
            "recency_bias": {
                "indicators": ["trend_extrapolation", "momentum_investing", "recent_news_focus"],
                "contrarian_threshold": 0.6
            }
        }

        self.market_psychology_indicators = {
            "fear": ["VIX", "put_call_ratio", "safe_haven_flows"],
            "greed": ["margin_debt", "IPO_activity", "speculative_trading"],
            "complacency": ["low_volatility", "compressed_spreads", "leverage_buildup"],
            "panic": ["forced_selling", "liquidity_crisis", "correlation_spike"]
        }

    async def analyze_behavioral_patterns(self) -> List[BehaviorSignal]:
        """Analyze behavioral finance patterns in markets"""
        signals = []

        for pattern, config in self.behavioral_patterns.items():
            try:
                signal = await self._analyze_behavioral_pattern(pattern, config)
                if signal:
                    signals.append(signal)
            except Exception as e:
                logging.error(f"Error analyzing {pattern}: {e}")
                continue

        return signals

    async def _analyze_behavioral_pattern(self, pattern: str, config: Dict) -> Optional[BehaviorSignal]:
        """Analyze specific behavioral pattern"""
        # Simulate behavioral pattern analysis
        # In production, would analyze actual market data for these patterns

        # Generate realistic behavioral intensity
        base_intensity = np.random.uniform(0.2, 0.8)

        # Adjust based on pattern type
        pattern_adjustments = {
            "herd_behavior": np.random.normal(0.1, 0.2),
            "overconfidence": np.random.normal(-0.1, 0.15),
            "loss_aversion": np.random.normal(0.05, 0.25),
            "anchoring": np.random.normal(0, 0.1),
            "recency_bias": np.random.normal(0.08, 0.18)
        }

        intensity = np.clip(base_intensity + pattern_adjustments.get(pattern, 0), 0, 1)

        # Determine market impact
        if pattern in ["herd_behavior", "overconfidence", "recency_bias"]:
            if intensity > 0.6:
                market_impact = "bearish"  # Contrarian signal
            else:
                market_impact = "bullish"
        elif pattern == "loss_aversion":
            market_impact = "bullish" if intensity > 0.6 else "bearish"
        else:
            market_impact = "neutral"

        # Check for contrarian opportunity
        contrarian_opportunity = intensity > config["contrarian_threshold"]

        # Determine affected assets
        affected_assets = self._get_affected_assets(pattern, intensity)

        return BehaviorSignal(
            behavior_type=pattern,
            intensity=intensity,
            market_impact=market_impact,
            contrarian_opportunity=contrarian_opportunity,
            affected_assets=affected_assets,
            confidence=0.5 + intensity * 0.3  # Higher intensity = higher confidence
        )

    def _get_affected_assets(self, pattern: str, intensity: float) -> List[str]:
        """Determine which assets are affected by behavioral pattern"""
        if pattern == "herd_behavior":
            if intensity > 0.7:
                return ["momentum_stocks", "growth_stocks", "crypto"]
            else:
                return ["broad_market"]
        elif pattern == "overconfidence":
            return ["individual_stocks", "options", "leveraged_products"]
        elif pattern == "loss_aversion":
            return ["defensive_stocks", "bonds", "gold", "utilities"]
        elif pattern == "anchoring":
            return ["stocks_near_highs", "round_number_levels"]
        elif pattern == "recency_bias":
            return ["trending_sectors", "recent_winners"]
        else:
            return ["broad_market"]

    async def analyze_market_psychology(self) -> Dict[str, float]:
        """Analyze overall market psychology"""
        psychology_scores = {}

        for emotion, indicators in self.market_psychology_indicators.items():
            # Simulate psychology analysis
            # In production, would analyze actual market indicators
            score = np.random.uniform(0.2, 0.8)

            # Add some correlation between emotions
            if emotion == "fear" and "greed" in psychology_scores:
                score = max(0.1, 1.0 - psychology_scores["greed"] + np.random.normal(0, 0.1))
            elif emotion == "greed" and "fear" in psychology_scores:
                score = max(0.1, 1.0 - psychology_scores["fear"] + np.random.normal(0, 0.1))

            psychology_scores[emotion] = np.clip(score, 0, 1)

        return psychology_scores

    def _detect_behavioral_anomalies(self, signals: List[BehaviorSignal]) -> List[str]:
        """Detect behavioral anomalies in the market"""
        anomalies = []

        # High intensity behavioral patterns
        high_intensity_patterns = [s for s in signals if s.intensity > 0.7]
        if len(high_intensity_patterns) >= 2:
            anomalies.append("multiple_behavioral_extremes")

        # Strong contrarian opportunities
        contrarian_signals = [s for s in signals if s.contrarian_opportunity]
        if len(contrarian_signals) >= 3:
            anomalies.append("multiple_contrarian_opportunities")

        # Conflicting behavioral signals
        bullish_signals = [s for s in signals if s.market_impact == "bullish"]
        bearish_signals = [s for s in signals if s.market_impact == "bearish"]

        if len(bullish_signals) > 0 and len(bearish_signals) > 0:
            if abs(len(bullish_signals) - len(bearish_signals)) <= 1:
                anomalies.append("conflicting_behavioral_signals")

        return anomalies

    async def get_behavioral_report(self) -> Dict:
        """Generate comprehensive behavioral finance report"""
        # Analyze behavioral patterns
        behavioral_signals = await self.analyze_behavioral_patterns()

        # Analyze market psychology
        psychology_scores = await self.analyze_market_psychology()

        # Detect anomalies
        anomalies = self._detect_behavioral_anomalies(behavioral_signals)

        # Calculate overall behavioral sentiment
        overall_sentiment = self._calculate_behavioral_sentiment(behavioral_signals, psychology_scores)

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "behavioral_analysis": {
                "overall_sentiment": overall_sentiment,
                "sentiment_classification": self._classify_sentiment(overall_sentiment),
                "behavioral_patterns": [{
                    "pattern": signal.behavior_type,
                    "intensity": signal.intensity,
                    "market_impact": signal.market_impact,
                    "contrarian_opportunity": signal.contrarian_opportunity,
                    "affected_assets": signal.affected_assets,
                    "confidence": signal.confidence
                } for signal in behavioral_signals],
                "market_psychology": psychology_scores,
                "behavioral_anomalies": anomalies
            },
            "trading_implications": {
                "contrarian_opportunities": [s.behavior_type for s in behavioral_signals
                                             if s.contrarian_opportunity],
                "behavioral_momentum": [s.behavior_type for s in behavioral_signals
                                        if s.intensity > 0.6 and not s.contrarian_opportunity],
                "risk_management": self._get_behavioral_risk_management(behavioral_signals),
                "position_sizing": self._get_behavioral_position_sizing(behavioral_signals)
            },
            "monitoring_priorities": {
                "extreme_behaviors": [s.behavior_type for s in behavioral_signals if s.intensity > 0.7],
                "contrarian_signals": [s.behavior_type for s in behavioral_signals if s.contrarian_opportunity],
                "psychology_extremes": [emotion for emotion, score in psychology_scores.items()
                                        if score > 0.7 or score < 0.3]
            }
        }

    def _calculate_behavioral_sentiment(self, signals: List[BehaviorSignal],
                                        psychology: Dict[str, float]) -> float:
        """Calculate overall behavioral sentiment"""
        if not signals and not psychology:
            return 0.0

        # Weight behavioral signals
        signal_sentiment = 0.0
        if signals:
            bullish_weight = sum(s.intensity for s in signals if s.market_impact == "bullish")
            bearish_weight = sum(s.intensity for s in signals if s.market_impact == "bearish")
            total_weight = bullish_weight + bearish_weight

            if total_weight > 0:
                signal_sentiment = (bullish_weight - bearish_weight) / total_weight

        # Weight psychology scores
        psychology_sentiment = 0.0
        if psychology:
            greed_score = psychology.get("greed", 0.5)
            fear_score = psychology.get("fear", 0.5)
            psychology_sentiment = greed_score - fear_score

        # Combine (60% signals, 40% psychology)
        if signals and psychology:
            combined_sentiment = 0.6 * signal_sentiment + 0.4 * psychology_sentiment
        elif signals:
            combined_sentiment = signal_sentiment
        elif psychology:
            combined_sentiment = psychology_sentiment
        else:
            combined_sentiment = 0.0

        return np.clip(combined_sentiment, -1.0, 1.0)

    def _classify_sentiment(self, sentiment: float) -> str:
        """Classify behavioral sentiment"""
        if sentiment > 0.4:
            return "behavioral_euphoria"
        elif sentiment > 0.1:
            return "behavioral_optimism"
        elif sentiment > -0.1:
            return "behavioral_neutral"
        elif sentiment > -0.4:
            return "behavioral_pessimism"
        else:
            return "behavioral_despair"

    def _get_behavioral_risk_management(self, signals: List[BehaviorSignal]) -> List[str]:
        """Get behavioral risk management recommendations"""
        recommendations = []

        # Check for high-intensity patterns
        high_intensity = [s for s in signals if s.intensity > 0.7]
        if high_intensity:
            recommendations.append("Monitor for behavioral reversals")
            recommendations.append("Consider reducing position sizes")

        # Check for herd behavior
        herd_signals = [s for s in signals if s.behavior_type == "herd_behavior" and s.intensity > 0.6]
        if herd_signals:
            recommendations.append("Prepare for trend reversals")
            recommendations.append("Avoid momentum chasing")

        # Check for overconfidence
        overconfidence_signals = [s for s in signals if s.behavior_type == "overconfidence" and s.intensity > 0.5]
        if overconfidence_signals:
            recommendations.append("Increase due diligence")
            recommendations.append("Diversify more broadly")

        return recommendations or ["Standard behavioral risk monitoring"]

    def _get_behavioral_position_sizing(self, signals: List[BehaviorSignal]) -> str:
        """Get position sizing recommendation based on behavioral patterns"""
        contrarian_count = sum(1 for s in signals if s.contrarian_opportunity)
        high_intensity_count = sum(1 for s in signals if s.intensity > 0.7)

        if contrarian_count >= 2:
            return "reduce_size"  # Multiple contrarian signals suggest caution
        elif high_intensity_count >= 2:
            return "reduce_size"  # Multiple extreme behaviors suggest volatility
        elif contrarian_count == 1 and high_intensity_count == 0:
            return "increase_size"  # Single contrarian opportunity with low extremes
        else:
            return "normal"