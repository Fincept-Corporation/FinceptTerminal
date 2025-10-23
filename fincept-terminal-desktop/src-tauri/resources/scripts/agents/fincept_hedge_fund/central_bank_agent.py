# -*- coding: utf-8 -*-
# agents/central_bank_agent.py - Central Bank Policy Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - central bank announcements
#   - monetary policy statements
#   - interest rate decisions
#   - economic indicators
#
# OUTPUT:
#   - Central bank policy signals
#   - Monetary policy stance analysis
#   - Interest rate forecasts
#   - Market impact assessments
#
# PARAMETERS:
#   - policy_indicators
#   - economic_data_sources
#   - central_bank_watchlist
#   - forecast_horizons
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
import re
from textblob import TextBlob

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class PolicySignal:
    """Central bank policy signal structure"""
    bank: str
    policy_stance: str  # dovish, neutral, hawkish
    rate_direction: str  # cutting, holding, raising
    probability: float
    next_meeting_date: Optional[datetime]
    key_factors: List[str]
    market_impact: Dict[str, float]
    confidence: float


class CentralBankAgent:
    """Advanced central bank policy analysis and prediction"""

    def __init__(self):
        self.name = "central_bank"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Central bank meeting schedules (approximate)
        self.meeting_schedules = {
            "FED": [
                "2025-01-29", "2025-03-19", "2025-04-30", "2025-06-11",
                "2025-07-30", "2025-09-17", "2025-10-29", "2025-12-17"
            ],
            "ECB": [
                "2025-01-23", "2025-03-06", "2025-04-10", "2025-06-05",
                "2025-07-17", "2025-09-11", "2025-10-23", "2025-12-11"
            ],
            "BOJ": [
                "2025-01-23", "2025-03-19", "2025-04-28", "2025-06-14",
                "2025-07-31", "2025-09-20", "2025-10-31", "2025-12-19"
            ]
        }

        # Policy keywords and their sentiment weights
        self.policy_keywords = {
            "hawkish": {
                "aggressive", "tighten", "restrictive", "combat inflation",
                "rate hikes", "cooling", "overheating", "price pressures",
                "vigilant", "determined", "forceful", "anchor expectations"
            },
            "dovish": {
                "accommodate", "supportive", "gradual", "patient", "flexible",
                "employment", "growth", "stimulus", "easy", "cautious",
                "measured", "data dependent", "appropriate", "monitor"
            },
            "neutral": {
                "balanced", "assess", "evaluate", "conditions", "outlook",
                "appropriate", "meetings", "review", "consider", "analyze"
            }
        }

        # Economic indicators that influence policy
        self.policy_indicators = {
            "inflation": {
                "CPIAUCSL": 0.25,  # CPI
                "CPILFESL": 0.25,  # Core CPI
                "PCEPI": 0.20,  # PCE Price Index
                "PCEPILFE": 0.20,  # Core PCE
                "DFEDTARU": 0.10  # Fed Target Rate
            },
            "employment": {
                "UNRATE": 0.35,  # Unemployment Rate
                "PAYEMS": 0.25,  # Nonfarm Payrolls
                "AWHMAN": 0.15,  # Average Weekly Hours
                "AHETPI": 0.25  # Average Hourly Earnings
            },
            "growth": {
                "GDPC1": 0.40,  # Real GDP
                "INDPRO": 0.30,  # Industrial Production
                "RSAFS": 0.30  # Retail Sales
            },
            "financial": {
                "GS10": 0.25,  # 10-Year Treasury
                "GS2": 0.25,  # 2-Year Treasury
                "VIXCLS": 0.20,  # VIX
                "DEXUSEU": 0.15,  # USD/EUR
                "DTWEXBGS": 0.15  # Trade Weighted USD
            }
        }

        # Market impact coefficients by asset class
        self.market_impact_coefficients = {
            "bonds": {
                "hawkish": -0.8,  # Negative for bond prices
                "neutral": 0.0,
                "dovish": 0.6
            },
            "stocks": {
                "hawkish": -0.4,  # Generally negative but less than bonds
                "neutral": 0.1,
                "dovish": 0.7
            },
            "dollar": {
                "hawkish": 0.9,  # Strong positive for USD
                "neutral": 0.0,
                "dovish": -0.6
            },
            "gold": {
                "hawkish": -0.7,  # Negative for gold
                "neutral": 0.0,
                "dovish": 0.5
            }
        }

    async def analyze_fed_policy(self) -> PolicySignal:
        """Comprehensive Federal Reserve policy analysis"""
        try:
            # Fetch economic indicators
            economic_data = await self._fetch_policy_indicators()

            # Get recent Fed communications
            fed_communications = await self._fetch_fed_communications()

            # Analyze policy stance from data
            data_stance = self._analyze_data_driven_stance(economic_data)

            # Analyze communications sentiment
            comm_stance = self._analyze_communication_sentiment(fed_communications)

            # Combine signals with weights
            combined_stance = self._combine_policy_signals(data_stance, comm_stance)

            # Predict rate direction
            rate_direction, probability = self._predict_rate_direction(economic_data, combined_stance)

            # Get next meeting date
            next_meeting = self._get_next_meeting_date("FED")

            # Calculate market impact
            market_impact = self._calculate_market_impact(combined_stance, rate_direction)

            # Generate LLM analysis
            llm_analysis = await self._generate_policy_analysis(economic_data, fed_communications, combined_stance)

            return PolicySignal(
                bank="FED",
                policy_stance=combined_stance["stance"],
                rate_direction=rate_direction,
                probability=probability,
                next_meeting_date=next_meeting,
                key_factors=llm_analysis.get("key_factors", []),
                market_impact=market_impact,
                confidence=combined_stance["confidence"]
            )

        except Exception as e:
            logging.error(f"Error in Fed policy analysis: {e}")
            return self._default_policy_signal("FED")

    async def _fetch_policy_indicators(self) -> Dict[str, pd.Series]:
        """Fetch economic indicators relevant to monetary policy"""
        data_requests = {}

        # Collect all indicators
        all_indicators = set()
        for category in self.policy_indicators.values():
            all_indicators.update(category.keys())

        # Create requests
        for indicator in all_indicators:
            data_requests[indicator] = {
                "series_id": indicator,
                "limit": 60  # 5 years of monthly data
            }

        try:
            raw_data = await self.data_manager.get_multi_source_data({
                "economic": data_requests
            })

            # Process into time series
            processed_data = {}
            economic_data = raw_data.get("economic", {})

            for key, data_points in economic_data.items():
                if data_points and len(data_points) > 0:
                    # Create DataFrame from data points
                    df_data = []
                    for dp in data_points:
                        df_data.append({
                            'date': dp.timestamp,
                            'value': dp.value
                        })

                    if df_data:
                        df = pd.DataFrame(df_data)
                        df['date'] = pd.to_datetime(df['date'])
                        df = df.sort_values('date').set_index('date')
                        processed_data[key] = df['value']

            return processed_data

        except Exception as e:
            logging.error(f"Error fetching policy indicators: {e}")
            return {}

    async def _fetch_fed_communications(self) -> List[DataPoint]:
        """Fetch recent Federal Reserve communications and speeches"""
        keywords = ["federal reserve", "jerome powell", "fed policy", "interest rates", "monetary policy"]

        communications = []
        try:
            for keyword in keywords:
                news_data = await self.data_manager.get_multi_source_data({
                    "news": {
                        "query": keyword,
                        "sources": ["reuters", "bloomberg", "wsj"],
                        "hours_back": 168  # 1 week
                    }
                })

                if "news" in news_data and news_data["news"]:
                    communications.extend(news_data["news"])

            # Filter for actual Fed communications
            fed_communications = []
            for comm in communications:
                if any(term in comm.value.lower() for term in ["fed", "federal reserve", "powell", "fomc"]):
                    fed_communications.append(comm)

            return fed_communications

        except Exception as e:
            logging.error(f"Error fetching Fed communications: {e}")
            return []

    def _analyze_data_driven_stance(self, data: Dict[str, pd.Series]) -> Dict:
        """Analyze policy stance based on economic data"""
        stance_scores = []
        confidence_factors = []

        try:
            # Inflation analysis
            inflation_score = self._analyze_inflation_pressure(data)
            stance_scores.append(inflation_score * 0.4)  # 40% weight
            confidence_factors.append(0.9 if inflation_score != 0 else 0.3)

            # Employment analysis
            employment_score = self._analyze_employment_conditions(data)
            stance_scores.append(employment_score * 0.3)  # 30% weight
            confidence_factors.append(0.8 if employment_score != 0 else 0.3)

            # Growth analysis
            growth_score = self._analyze_growth_momentum(data)
            stance_scores.append(growth_score * 0.2)  # 20% weight
            confidence_factors.append(0.7 if growth_score != 0 else 0.3)

            # Financial conditions
            financial_score = self._analyze_financial_conditions(data)
            stance_scores.append(financial_score * 0.1)  # 10% weight
            confidence_factors.append(0.6 if financial_score != 0 else 0.3)

            # Calculate overall stance
            overall_score = np.sum(stance_scores)
            confidence = np.mean(confidence_factors)

            if overall_score > 0.3:
                stance = "hawkish"
            elif overall_score < -0.3:
                stance = "dovish"
            else:
                stance = "neutral"

            return {
                "stance": stance,
                "score": overall_score,
                "confidence": confidence,
                "components": {
                    "inflation": inflation_score,
                    "employment": employment_score,
                    "growth": growth_score,
                    "financial": financial_score
                }
            }

        except Exception as e:
            logging.error(f"Error in data-driven stance analysis: {e}")
            return {"stance": "neutral", "score": 0.0, "confidence": 0.3, "components": {}}

    def _analyze_inflation_pressure(self, data: Dict[str, pd.Series]) -> float:
        """Analyze inflation pressure for policy implications"""
        inflation_indicators = self.policy_indicators["inflation"]
        scores = []

        for indicator, weight in inflation_indicators.items():
            if indicator in data and not data[indicator].empty and len(data[indicator]) >= 12:
                series = data[indicator]

                # Calculate year-over-year change
                yoy_change = series.pct_change(12).iloc[-1] * 100

                # Fed target is typically around 2%
                if indicator in ["CPIAUCSL", "CPILFESL", "PCEPI", "PCEPILFE"]:
                    if yoy_change > 3.0:
                        score = 1.0  # Strong hawkish signal
                    elif yoy_change > 2.5:
                        score = 0.5  # Moderate hawkish
                    elif yoy_change < 1.5:
                        score = -0.5  # Moderate dovish
                    elif yoy_change < 1.0:
                        score = -1.0  # Strong dovish
                    else:
                        score = 0.0  # Neutral
                else:
                    score = 0.0

                scores.append(score * weight)

        return np.sum(scores) if scores else 0.0

    def _analyze_employment_conditions(self, data: Dict[str, pd.Series]) -> float:
        """Analyze employment conditions for policy implications"""
        employment_indicators = self.policy_indicators["employment"]
        scores = []

        for indicator, weight in employment_indicators.items():
            if indicator in data and not data[indicator].empty:
                series = data[indicator]

                if indicator == "UNRATE":
                    current_rate = series.iloc[-1]
                    if current_rate < 3.5:
                        score = 1.0  # Very tight labor market - hawkish
                    elif current_rate < 4.0:
                        score = 0.5  # Tight labor market
                    elif current_rate > 5.5:
                        score = -0.5  # Loose labor market - dovish
                    elif current_rate > 6.5:
                        score = -1.0  # Very loose labor market
                    else:
                        score = 0.0
                elif indicator == "PAYEMS":
                    # Payroll growth
                    if len(series) >= 3:
                        avg_growth = series.pct_change().iloc[-3:].mean() * 100
                        if avg_growth > 0.4:  # >400k jobs/month
                            score = 0.5  # Strong growth - slightly hawkish
                        elif avg_growth < 0.1:  # <100k jobs/month
                            score = -0.5  # Weak growth - dovish
                        else:
                            score = 0.0
                    else:
                        score = 0.0
                else:
                    score = 0.0

                scores.append(score * weight)

        return np.sum(scores) if scores else 0.0

    def _analyze_growth_momentum(self, data: Dict[str, pd.Series]) -> float:
        """Analyze economic growth momentum"""
        growth_indicators = self.policy_indicators["growth"]
        scores = []

        for indicator, weight in growth_indicators.items():
            if indicator in data and not data[indicator].empty and len(data[indicator]) >= 4:
                series = data[indicator]

                # Calculate quarterly growth rate
                quarterly_growth = series.pct_change(3).iloc[-1] * 100

                if indicator == "GDPC1":
                    if quarterly_growth > 3.0:
                        score = 0.5  # Strong growth - slightly hawkish
                    elif quarterly_growth < 1.0:
                        score = -0.5  # Weak growth - dovish
                    elif quarterly_growth < 0:
                        score = -1.0  # Contraction - very dovish
                    else:
                        score = 0.0
                else:
                    # For other indicators, use similar logic
                    if quarterly_growth > 2.0:
                        score = 0.3
                    elif quarterly_growth < 0:
                        score = -0.5
                    else:
                        score = 0.0

                scores.append(score * weight)

        return np.sum(scores) if scores else 0.0

    def _analyze_financial_conditions(self, data: Dict[str, pd.Series]) -> float:
        """Analyze financial market conditions"""
        financial_indicators = self.policy_indicators["financial"]
        scores = []

        for indicator, weight in financial_indicators.items():
            if indicator in data and not data[indicator].empty:
                series = data[indicator]

                if indicator == "GS10":
                    # 10-year treasury yield
                    current_yield = series.iloc[-1]
                    if current_yield > 5.0:
                        score = 0.5  # High yields - less need for tightening
                    elif current_yield < 3.0:
                        score = -0.3  # Low yields - room for accommodation
                    else:
                        score = 0.0
                elif indicator == "VIXCLS":
                    # VIX volatility index
                    current_vix = series.iloc[-1]
                    if current_vix > 30:
                        score = -0.8  # High volatility - dovish signal
                    elif current_vix < 15:
                        score = 0.3  # Low volatility - can be hawkish
                    else:
                        score = 0.0
                else:
                    score = 0.0

                scores.append(score * weight)

        return np.sum(scores) if scores else 0.0

    def _analyze_communication_sentiment(self, communications: List[DataPoint]) -> Dict:
        """Analyze sentiment from Fed communications"""
        if not communications:
            return {"stance": "neutral", "confidence": 0.2, "score": 0.0}

        stance_scores = []
        confidence_scores = []

        for comm in communications:
            text = comm.value.lower()

            # Count keyword occurrences
            hawkish_count = sum(1 for keyword in self.policy_keywords["hawkish"] if keyword in text)
            dovish_count = sum(1 for keyword in self.policy_keywords["dovish"] if keyword in text)
            neutral_count = sum(1 for keyword in self.policy_keywords["neutral"] if keyword in text)

            total_keywords = hawkish_count + dovish_count + neutral_count

            if total_keywords > 0:
                # Calculate sentiment score
                sentiment_score = (hawkish_count - dovish_count) / total_keywords
                stance_scores.append(sentiment_score * comm.confidence)
                confidence_scores.append(comm.confidence)

        if not stance_scores:
            return {"stance": "neutral", "confidence": 0.2, "score": 0.0}

        # Weight by confidence and recency
        weighted_score = np.average(stance_scores, weights=confidence_scores)
        avg_confidence = np.mean(confidence_scores)

        if weighted_score > 0.2:
            stance = "hawkish"
        elif weighted_score < -0.2:
            stance = "dovish"
        else:
            stance = "neutral"

        return {
            "stance": stance,
            "confidence": avg_confidence,
            "score": weighted_score
        }

    def _combine_policy_signals(self, data_stance: Dict, comm_stance: Dict) -> Dict:
        """Combine data-driven and communication-based policy signals"""
        # Weight data more heavily than communications
        data_weight = 0.7
        comm_weight = 0.3

        combined_score = (data_stance["score"] * data_weight +
                          comm_stance["score"] * comm_weight)

        combined_confidence = (data_stance["confidence"] * data_weight +
                               comm_stance["confidence"] * comm_weight)

        if combined_score > 0.25:
            stance = "hawkish"
        elif combined_score < -0.25:
            stance = "dovish"
        else:
            stance = "neutral"

        return {
            "stance": stance,
            "score": combined_score,
            "confidence": combined_confidence,
            "data_component": data_stance,
            "communication_component": comm_stance
        }

    def _predict_rate_direction(self, data: Dict[str, pd.Series], stance: Dict) -> Tuple[str, float]:
        """Predict rate direction and probability"""
        current_rate = 5.25  # Approximate current fed funds rate

        # Get current inflation and unemployment
        current_inflation = 0.0
        current_unemployment = 0.0

        if "CPIAUCSL" in data and not data["CPIAUCSL"].empty and len(data["CPIAUCSL"]) >= 12:
            current_inflation = data["CPIAUCSL"].pct_change(12).iloc[-1] * 100

        if "UNRATE" in data and not data["UNRATE"].empty:
            current_unemployment = data["UNRATE"].iloc[-1]

        # Decision logic based on dual mandate
        if stance["stance"] == "hawkish":
            if current_inflation > 3.0 or current_unemployment < 3.5:
                direction = "raising"
                probability = 0.75 + stance["confidence"] * 0.2
            else:
                direction = "holding"
                probability = 0.60 + stance["confidence"] * 0.15
        elif stance["stance"] == "dovish":
            if current_inflation < 1.5 or current_unemployment > 5.0:
                direction = "cutting"
                probability = 0.70 + stance["confidence"] * 0.2
            else:
                direction = "holding"
                probability = 0.55 + stance["confidence"] * 0.15
        else:
            direction = "holding"
            probability = 0.65 + stance["confidence"] * 0.1

        # Adjust for current rate level
        if current_rate > 6.0 and direction == "raising":
            probability *= 0.8  # Less likely to raise from high levels
        elif current_rate < 2.0 and direction == "cutting":
            probability *= 0.7  # Less room to cut from low levels

        return direction, min(probability, 0.95)

    def _get_next_meeting_date(self, bank: str) -> Optional[datetime]:
        """Get next central bank meeting date"""
        if bank not in self.meeting_schedules:
            return None

        today = datetime.now()
        try:
            meeting_dates = [datetime.strptime(date, "%Y-%m-%d") for date in self.meeting_schedules[bank]]
            future_meetings = [date for date in meeting_dates if date > today]
            return min(future_meetings) if future_meetings else None
        except Exception as e:
            logging.error(f"Error parsing meeting dates: {e}")
            return None

    def _calculate_market_impact(self, stance: Dict, rate_direction: str) -> Dict[str, float]:
        """Calculate expected market impact of policy stance"""
        stance_name = stance["stance"]
        impact_strength = abs(stance["score"]) * stance["confidence"]

        market_impacts = {}

        for asset_class, coefficients in self.market_impact_coefficients.items():
            base_impact = coefficients[stance_name]

            # Adjust for rate direction
            if rate_direction == "raising" and stance_name == "hawkish":
                base_impact *= 1.2
            elif rate_direction == "cutting" and stance_name == "dovish":
                base_impact *= 1.2

            # Scale by impact strength
            final_impact = base_impact * impact_strength
            market_impacts[asset_class] = np.clip(final_impact, -1.0, 1.0)

        return market_impacts

    async def _generate_policy_analysis(self, data: Dict[str, pd.Series],
                                        communications: List[DataPoint],
                                        stance: Dict) -> Dict:
        """Generate LLM-enhanced policy analysis"""
        try:
            # Prepare data summary
            data_summary = self._prepare_policy_data_summary(data)
            comm_summary = self._prepare_communication_summary(communications)

            prompt = f"""
            As a Federal Reserve policy expert, analyze the current monetary policy outlook.

            Current Policy Stance: {stance["stance"]} (confidence: {stance["confidence"]:.2f})

            Economic Data Summary:
            {data_summary}

            Recent Fed Communications:
            {comm_summary}

            Please provide:
            1. Key factors driving current policy stance
            2. Most important risks to monitor
            3. Probability of policy error (too tight/loose)
            4. Market positioning recommendations
            5. Timeline for next major policy shift

            Format as JSON with keys: key_factors, risks, policy_error_risk, market_recommendations, timeline
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM policy analysis: {e}")
            return {
                "key_factors": ["Inflation trends", "Employment conditions"],
                "risks": ["Policy lag effects", "Market volatility"],
                "policy_error_risk": 0.3,
                "market_recommendations": "Maintain balanced positioning",
                "timeline": "Next 3-6 months"
            }

    def _prepare_policy_data_summary(self, data: Dict[str, pd.Series]) -> str:
        """Prepare policy-relevant data summary"""
        summary_lines = []

        # Key indicators for Fed policy
        key_indicators = {
            "CPIAUCSL": "Core Inflation",
            "UNRATE": "Unemployment Rate",
            "PAYEMS": "Job Growth",
            "GDPC1": "GDP Growth",
            "GS10": "10Y Treasury Yield"
        }

        for indicator, name in key_indicators.items():
            if indicator in data and not data[indicator].empty:
                latest = data[indicator].iloc[-1]

                if indicator in ["CPIAUCSL"] and len(data[indicator]) >= 12:
                    yoy_change = data[indicator].pct_change(12).iloc[-1] * 100
                    summary_lines.append(f"{name}: {yoy_change:.1f}% YoY")
                elif indicator == "PAYEMS" and len(data[indicator]) >= 1:
                    monthly_change = data[indicator].diff().iloc[-1]
                    summary_lines.append(f"{name}: {monthly_change:.0f}k (monthly)")
                else:
                    summary_lines.append(f"{name}: {latest:.2f}")

        return "\n".join(summary_lines)

    def _prepare_communication_summary(self, communications: List[DataPoint]) -> str:
        """Prepare Fed communications summary"""
        if not communications:
            return "No recent Fed communications available"

        # Get most recent and relevant communications
        recent_comms = sorted(communications, key=lambda x: x.timestamp, reverse=True)[:5]

        summary_lines = []
        for comm in recent_comms:
            date_str = comm.timestamp.strftime("%Y-%m-%d")
            title = comm.value[:100] + "..." if len(comm.value) > 100 else comm.value
            summary_lines.append(f"{date_str}: {title}")

        return "\n".join(summary_lines)

    def _default_policy_signal(self, bank: str) -> PolicySignal:
        """Return default policy signal in case of errors"""
        return PolicySignal(
            bank=bank,
            policy_stance="neutral",
            rate_direction="holding",
            probability=0.5,
            next_meeting_date=self._get_next_meeting_date(bank),
            key_factors=["Data dependency", "Inflation monitoring"],
            market_impact={
                "bonds": 0.0,
                "stocks": 0.0,
                "dollar": 0.0,
                "gold": 0.0
            },
            confidence=0.3
        )

    async def get_policy_report(self) -> Dict:
        """Generate comprehensive central bank policy report"""
        fed_signal = await self.analyze_fed_policy()

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "fed_analysis": {
                "policy_stance": fed_signal.policy_stance,
                "rate_direction": fed_signal.rate_direction,
                "probability": fed_signal.probability,
                "confidence": fed_signal.confidence,
                "next_meeting": fed_signal.next_meeting_date.isoformat() if fed_signal.next_meeting_date else None,
                "key_factors": fed_signal.key_factors
            },
            "market_impact": fed_signal.market_impact,
            "trading_implications": {
                "duration_positioning": "short" if fed_signal.policy_stance == "hawkish" else "long",
                "sector_rotation": self._get_sector_implications(fed_signal),
                "currency_bias": "bullish_usd" if fed_signal.policy_stance == "hawkish" else "bearish_usd"
            },
            "risk_factors": {
                "policy_uncertainty": 1 - fed_signal.confidence,
                "communication_risk": 0.3,
                "market_reaction_risk": 0.4
            }
        }

    def _get_sector_implications(self, signal: PolicySignal) -> Dict[str, str]:
        """Get sector rotation implications from policy stance"""
        if signal.policy_stance == "hawkish":
            return {
                "financials": "bullish",  # Higher rates benefit banks
                "technology": "bearish",  # Higher discount rates hurt growth stocks
                "utilities": "bearish",  # Rate-sensitive sector
                "real_estate": "bearish"  # REITs hurt by higher rates
            }
        elif signal.policy_stance == "dovish":
            return {
                "technology": "bullish",  # Lower rates help growth stocks
                "real_estate": "bullish",  # REITs benefit from lower rates
                "utilities": "bullish",  # Rate-sensitive sectors benefit
                "financials": "bearish"  # Lower rates hurt bank margins
            }
        else:
            return {
                "balanced": "neutral",
                "defensive": "slight_overweight"
            }