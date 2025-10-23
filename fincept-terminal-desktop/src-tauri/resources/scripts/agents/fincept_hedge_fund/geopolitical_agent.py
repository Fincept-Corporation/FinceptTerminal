# -*- coding: utf-8 -*-
# agents/geopolitical_agent.py - Geopolitical Risk Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - geopolitical events
#   - political risk indicators
#   - international relations data
#   - regional stability metrics
#
# OUTPUT:
#   - Geopolitical risk assessments
#   - Regional stability analysis
#   - Political event impact forecasts
#   - Cross-border investment recommendations
#
# PARAMETERS:
#   - risk_indicators
#   - regions_monitoring
#   - event_types
#   - impact_timeframes
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
from collections import defaultdict
import re

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class GeopoliticalRisk:
    """Geopolitical risk assessment structure"""
    region: str
    risk_type: str
    current_level: int  # 1-10 scale
    trend: str  # escalating, stable, de-escalating
    probability_impact: float  # 0-1
    affected_sectors: List[str]
    timeline: str
    mitigation_strategies: List[str]
    confidence: float


@dataclass
class ConflictSignal:
    """Military conflict monitoring signal"""
    conflict_name: str
    escalation_risk: float
    economic_impact: Dict[str, float]
    supply_chain_risk: Dict[str, float]
    market_sectors: Dict[str, float]
    duration_estimate: str


class GeopoliticalAgent:
    """Advanced geopolitical risk monitoring and analysis"""

    def __init__(self):
        self.name = "geopolitical"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Geopolitical risk factors and their weights
        self.risk_factors = {
            "military_conflicts": {
                "Russia-Ukraine": 0.25,
                "China-Taiwan": 0.20,
                "Middle_East": 0.15,
                "North_Korea": 0.10,
                "India-Pakistan": 0.08,
                "Balkans": 0.05,
                "Arctic": 0.07,
                "Cyber_warfare": 0.10
            },
            "trade_tensions": {
                "US-China": 0.30,
                "US-EU": 0.15,
                "China-EU": 0.15,
                "US-Russia": 0.20,
                "Brexit_aftermath": 0.10,
                "USMCA_issues": 0.10
            },
            "sanctions_regime": {
                "Russia_sanctions": 0.35,
                "China_tech_sanctions": 0.25,
                "Iran_sanctions": 0.15,
                "North_Korea_sanctions": 0.10,
                "Secondary_sanctions": 0.15
            },
            "political_instability": {
                "US_domestic": 0.20,
                "EU_integration": 0.18,
                "China_internal": 0.15,
                "Middle_East_regimes": 0.15,
                "Latin_America": 0.12,
                "Africa_coups": 0.10,
                "Democratic_backsliding": 0.10
            }
        }

        # Sector impact mapping from geopolitical events
        self.sector_impact_map = {
            "military_conflicts": {
                "defense": 0.8,
                "energy": 0.6,
                "commodities": 0.7,
                "technology": -0.3,
                "tourism": -0.6,
                "airlines": -0.5,
                "insurance": -0.4
            },
            "trade_tensions": {
                "technology": -0.7,
                "manufacturing": -0.5,
                "agriculture": -0.4,
                "automotive": -0.6,
                "semiconductors": -0.8,
                "renewable_energy": 0.3
            },
            "sanctions_regime": {
                "energy": -0.8,
                "banking": -0.6,
                "technology": -0.5,
                "shipping": -0.4,
                "commodities": 0.4,
                "defense": 0.5
            },
            "political_instability": {
                "healthcare": 0.2,
                "utilities": 0.3,
                "consumer_staples": 0.4,
                "financials": -0.4,
                "real_estate": -0.3,
                "currency": -0.6
            }
        }

        # Key geopolitical indicators to monitor
        self.geopolitical_keywords = {
            "conflict_escalation": [
                "military buildup", "troop movements", "weapon shipments",
                "border tensions", "missile tests", "naval exercises",
                "air defense", "invasion", "occupation", "blockade"
            ],
            "diplomatic_tensions": [
                "sanctions", "tariffs", "trade war", "diplomatic expulsion",
                "embassy closure", "summit cancellation", "treaty withdrawal",
                "alliance strain", "diplomatic protest"
            ],
            "regime_change": [
                "coup", "revolution", "civil unrest", "protests",
                "election fraud", "authoritarianism", "martial law",
                "state of emergency", "government collapse"
            ],
            "economic_warfare": [
                "currency manipulation", "debt trap", "supply chain disruption",
                "critical minerals", "technology transfer", "intellectual property",
                "cyber attacks", "financial warfare"
            ]
        }

        # Regional risk weightings based on global economic impact
        self.regional_weights = {
            "North_America": 0.25,
            "Europe": 0.20,
            "East_Asia": 0.25,
            "Middle_East": 0.15,
            "South_America": 0.08,
            "Africa": 0.05,
            "South_Asia": 0.02
        }

    async def analyze_global_risks(self) -> List[GeopoliticalRisk]:
        """Comprehensive global geopolitical risk analysis"""
        risks = []

        try:
            # Analyze each risk category
            for risk_category, risk_items in self.risk_factors.items():
                category_risks = await self._analyze_risk_category(risk_category, risk_items)
                risks.extend(category_risks)

            # Sort by risk level and confidence
            risks.sort(key=lambda x: x.current_level * x.confidence, reverse=True)

            return risks[:10]  # Return top 10 risks

        except Exception as e:
            logging.error(f"Error in global risk analysis: {e}")
            return [self._default_geopolitical_risk()]

    async def _analyze_risk_category(self, category: str, risk_items: Dict[str, float]) -> List[GeopoliticalRisk]:
        """Analyze specific risk category"""
        category_risks = []

        # Get news data for this category
        news_data = await self._fetch_category_news(category, list(risk_items.keys()))

        for risk_item, weight in risk_items.items():
            try:
                # Filter news for this specific risk
                relevant_news = [news for news in news_data
                                 if self._is_relevant_to_risk(news, risk_item)]

                # Calculate risk level
                risk_level = self._calculate_risk_level(relevant_news, risk_item, category)

                # Determine trend
                trend = self._determine_risk_trend(relevant_news)

                # Calculate probability and impact
                prob_impact = self._calculate_probability_impact(risk_level, weight, relevant_news)

                # Get affected sectors
                affected_sectors = self._get_affected_sectors(category, risk_level)

                # Generate timeline estimate
                timeline = self._estimate_timeline(risk_level, trend, relevant_news)

                # Get mitigation strategies
                mitigation = self._get_mitigation_strategies(risk_item, category)

                # Calculate confidence
                confidence = self._calculate_risk_confidence(relevant_news, risk_item)

                risk = GeopoliticalRisk(
                    region=self._get_region_from_risk(risk_item),
                    risk_type=category,
                    current_level=risk_level,
                    trend=trend,
                    probability_impact=prob_impact,
                    affected_sectors=affected_sectors,
                    timeline=timeline,
                    mitigation_strategies=mitigation,
                    confidence=confidence
                )

                category_risks.append(risk)

            except Exception as e:
                logging.error(f"Error analyzing {risk_item}: {e}")
                continue

        return category_risks

    async def _fetch_category_news(self, category: str, risk_items: List[str]) -> List[DataPoint]:
        """Fetch news data for specific risk category"""
        all_news = []

        # Create search queries for each risk item
        queries = []
        if category == "military_conflicts":
            queries = ["military conflict", "war", "invasion", "missile", "defense"]
        elif category == "trade_tensions":
            queries = ["trade war", "tariffs", "sanctions", "trade dispute"]
        elif category == "sanctions_regime":
            queries = ["sanctions", "embargo", "trade restrictions"]
        elif category == "political_instability":
            queries = ["political crisis", "coup", "election", "protests"]

        for query in queries:
            try:
                news_data = await self.data_manager.get_multi_source_data({
                    "news": {
                        "query": query,
                        "sources": ["reuters", "bloomberg", "wsj", "ft"],
                        "hours_back": 72  # 3 days
                    }
                })

                if "news" in news_data:
                    all_news.extend(news_data["news"])

            except Exception as e:
                logging.error(f"Error fetching news for {query}: {e}")
                continue

        return all_news

    def _is_relevant_to_risk(self, news: DataPoint, risk_item: str) -> bool:
        """Check if news article is relevant to specific risk"""
        text = (news.value + " " + news.metadata.get("description", "")).lower()

        # Create keywords for each risk item
        risk_keywords = {
            "Russia-Ukraine": ["russia", "ukraine", "putin", "zelensky", "kyiv", "moscow"],
            "China-Taiwan": ["china", "taiwan", "beijing", "taipei", "strait", "xi jinping"],
            "Middle_East": ["iran", "israel", "gaza", "lebanon", "syria", "yemen"],
            "North_Korea": ["north korea", "kim jong", "pyongyang", "dprk"],
            "US-China": ["us china", "trade war", "biden", "xi jinping", "tariff"],
            "Russia_sanctions": ["russia sanctions", "swift", "energy embargo"],
            "China_tech_sanctions": ["china tech", "semiconductor", "huawei", "tiktok"]
        }

        keywords = risk_keywords.get(risk_item, [risk_item.lower().replace("_", " ")])

        return any(keyword in text for keyword in keywords)

    def _calculate_risk_level(self, news: List[DataPoint], risk_item: str, category: str) -> int:
        """Calculate current risk level (1-10 scale)"""
        if not news:
            return 3  # Default moderate risk

        # Base risk levels for different risk items
        base_risks = {
            "Russia-Ukraine": 8,
            "China-Taiwan": 6,
            "Middle_East": 7,
            "US-China": 5,
            "North_Korea": 4
        }

        base_risk = base_risks.get(risk_item, 5)

        # Analyze news sentiment and intensity
        escalation_keywords = self.geopolitical_keywords.get("conflict_escalation", [])
        tension_keywords = self.geopolitical_keywords.get("diplomatic_tensions", [])

        escalation_count = 0
        total_articles = len(news)

        for article in news:
            text = article.value.lower()
            escalation_count += sum(1 for keyword in escalation_keywords if keyword in text)
            escalation_count += sum(1 for keyword in tension_keywords if keyword in text)

        # Adjust base risk based on recent news intensity
        if total_articles > 0:
            intensity_factor = min(escalation_count / total_articles, 2.0)
            adjusted_risk = base_risk + intensity_factor
        else:
            adjusted_risk = base_risk

        return int(np.clip(adjusted_risk, 1, 10))

    def _determine_risk_trend(self, news: List[DataPoint]) -> str:
        """Determine if risk is escalating, stable, or de-escalating"""
        if len(news) < 2:
            return "stable"

        # Sort news by date
        sorted_news = sorted(news, key=lambda x: x.timestamp)

        # Compare recent vs older news intensity
        midpoint = len(sorted_news) // 2
        older_news = sorted_news[:midpoint]
        recent_news = sorted_news[midpoint:]

        older_intensity = self._calculate_news_intensity(older_news)
        recent_intensity = self._calculate_news_intensity(recent_news)

        if recent_intensity > older_intensity * 1.2:
            return "escalating"
        elif recent_intensity < older_intensity * 0.8:
            return "de-escalating"
        else:
            return "stable"

    def _calculate_news_intensity(self, news: List[DataPoint]) -> float:
        """Calculate intensity score of news articles"""
        if not news:
            return 0.0

        intensity_score = 0.0
        escalation_keywords = (self.geopolitical_keywords.get("conflict_escalation", []) +
                               self.geopolitical_keywords.get("diplomatic_tensions", []))

        for article in news:
            text = article.value.lower()
            keyword_count = sum(1 for keyword in escalation_keywords if keyword in text)
            # Weight by article confidence
            intensity_score += keyword_count * article.confidence

        return intensity_score / len(news)

    def _calculate_probability_impact(self, risk_level: int, weight: float,
                                      news: List[DataPoint]) -> float:
        """Calculate probability-weighted impact"""
        # Convert risk level to probability (non-linear)
        probability = (risk_level / 10) ** 1.5

        # Weight by global economic impact
        economic_impact = weight

        # Adjust based on news recency and volume
        if news:
            avg_age_hours = np.mean([(datetime.now() - article.timestamp).total_seconds() / 3600
                                     for article in news])
            recency_factor = max(0.5, 1.0 - avg_age_hours / 168)  # Decay over 1 week
            volume_factor = min(1.5, len(news) / 10)  # More news = higher impact
        else:
            recency_factor = 0.5
            volume_factor = 0.5

        return probability * economic_impact * recency_factor * volume_factor

    def _get_affected_sectors(self, category: str, risk_level: int) -> List[str]:
        """Get sectors most affected by this risk category"""
        if category not in self.sector_impact_map:
            return ["broad_market"]

        sector_impacts = self.sector_impact_map[category]

        # Filter sectors by risk level threshold and impact magnitude
        threshold = 0.3 if risk_level > 7 else 0.4 if risk_level > 5 else 0.5

        affected_sectors = []
        for sector, impact in sector_impacts.items():
            if abs(impact) >= threshold:
                affected_sectors.append(sector)

        return affected_sectors or ["broad_market"]

    def _estimate_timeline(self, risk_level: int, trend: str, news: List[DataPoint]) -> str:
        """Estimate timeline for risk materialization"""
        base_timelines = {
            "escalating": {
                9: "1-3 months",
                7: "3-6 months",
                5: "6-12 months",
                3: "1-2 years"
            },
            "stable": {
                9: "3-6 months",
                7: "6-12 months",
                5: "1-2 years",
                3: "2+ years"
            },
            "de-escalating": {
                9: "6-12 months",
                7: "1-2 years",
                5: "2+ years",
                3: "Low probability"
            }
        }

        timeline_map = base_timelines.get(trend, base_timelines["stable"])

        # Find closest risk level
        for level in sorted(timeline_map.keys(), reverse=True):
            if risk_level >= level:
                return timeline_map[level]

        return "2+ years"

    def _get_mitigation_strategies(self, risk_item: str, category: str) -> List[str]:
        """Get investment mitigation strategies for specific risks"""
        strategies = {
            "military_conflicts": [
                "Increase defense sector allocation",
                "Hedge energy exposure",
                "Diversify geographically",
                "Add gold/commodities hedge",
                "Reduce emerging market exposure"
            ],
            "trade_tensions": [
                "Avoid single-country exposure",
                "Focus on domestic-oriented companies",
                "Hedge currency exposure",
                "Diversify supply chains",
                "Consider trade-war beneficiaries"
            ],
            "sanctions_regime": [
                "Avoid sanctioned sectors/countries",
                "Increase compliance monitoring",
                "Diversify banking relationships",
                "Focus on domestic markets",
                "Consider alternative payment systems"
            ],
            "political_instability": [
                "Increase safe-haven assets",
                "Reduce political-sensitive sectors",
                "Focus on multinational companies",
                "Add volatility hedges",
                "Maintain higher cash levels"
            ]
        }

        return strategies.get(category, ["Monitor closely", "Maintain diversification"])

    def _get_region_from_risk(self, risk_item: str) -> str:
        """Map risk item to geographical region"""
        region_mapping = {
            "Russia-Ukraine": "Europe",
            "China-Taiwan": "East_Asia",
            "Middle_East": "Middle_East",
            "North_Korea": "East_Asia",
            "US-China": "Global",
            "US-EU": "North_America",
            "Brexit": "Europe",
            "India-Pakistan": "South_Asia"
        }

        return region_mapping.get(risk_item, "Global")

    def _calculate_risk_confidence(self, news: List[DataPoint], risk_item: str) -> float:
        """Calculate confidence in risk assessment"""
        confidence_factors = []

        # News volume factor
        news_count = len(news)
        volume_confidence = min(1.0, news_count / 20)  # Optimal around 20 articles
        confidence_factors.append(volume_confidence * 0.3)

        # Source reliability
        if news:
            avg_source_confidence = np.mean([article.confidence for article in news])
            confidence_factors.append(avg_source_confidence * 0.4)
        else:
            confidence_factors.append(0.3 * 0.4)

        # News recency
        if news:
            most_recent = max(news, key=lambda x: x.timestamp)
            hours_since = (datetime.now() - most_recent.timestamp).total_seconds() / 3600
            recency_confidence = max(0.3, 1.0 - hours_since / 72)  # Decay over 3 days
            confidence_factors.append(recency_confidence * 0.3)
        else:
            confidence_factors.append(0.3 * 0.3)

        return np.sum(confidence_factors)

    async def analyze_conflict_escalation(self, conflict_name: str) -> ConflictSignal:
        """Deep analysis of specific military conflict"""
        try:
            # Fetch conflict-specific news
            conflict_news = await self.data_manager.get_multi_source_data({
                "news": {
                    "query": conflict_name,
                    "sources": ["reuters", "bloomberg", "wsj"],
                    "hours_back": 48
                }
            })

            news_data = conflict_news.get("news", [])

            # Calculate escalation risk
            escalation_risk = self._calculate_escalation_risk(news_data, conflict_name)

            # Assess economic impact
            economic_impact = self._assess_economic_impact(conflict_name, escalation_risk)

            # Analyze supply chain risks
            supply_chain_risk = self._analyze_supply_chain_impact(conflict_name)

            # Get market sector impacts
            market_sectors = self._get_market_sector_impacts(conflict_name, escalation_risk)

            # Estimate duration
            duration_estimate = self._estimate_conflict_duration(news_data, escalation_risk)

            return ConflictSignal(
                conflict_name=conflict_name,
                escalation_risk=escalation_risk,
                economic_impact=economic_impact,
                supply_chain_risk=supply_chain_risk,
                market_sectors=market_sectors,
                duration_estimate=duration_estimate
            )

        except Exception as e:
            logging.error(f"Error analyzing conflict {conflict_name}: {e}")
            return self._default_conflict_signal(conflict_name)

    def _calculate_escalation_risk(self, news: List[DataPoint], conflict_name: str) -> float:
        """Calculate probability of conflict escalation"""
        if not news:
            return 0.3  # Default moderate risk

        escalation_indicators = [
            "military buildup", "troop deployment", "weapon delivery",
            "alliance activation", "nuclear threat", "red line crossed",
            "ultimatum", "deadline", "mobilization", "intervention"
        ]

        escalation_score = 0.0

        for article in news:
            text = article.value.lower()
            indicator_count = sum(1 for indicator in escalation_indicators if indicator in text)
            escalation_score += indicator_count * article.confidence

        # Normalize by number of articles
        if news:
            normalized_score = escalation_score / len(news)
            # Convert to probability (0-1)
            return min(1.0, normalized_score / 3.0)  # Scale factor

        return 0.3

    def _assess_economic_impact(self, conflict_name: str, escalation_risk: float) -> Dict[str, float]:
        """Assess economic impact of conflict escalation"""
        # Base economic impacts by conflict
        base_impacts = {
            "Russia-Ukraine": {
                "global_gdp": -0.8,
                "inflation": 2.5,
                "energy_prices": 0.4,
                "food_prices": 0.3,
                "supply_chains": -0.6
            },
            "China-Taiwan": {
                "global_gdp": -2.5,
                "inflation": 1.8,
                "tech_supply": -0.8,
                "shipping": -0.7,
                "semiconductors": -0.9
            },
            "Middle_East": {
                "global_gdp": -0.5,
                "oil_prices": 0.6,
                "shipping": -0.4,
                "regional_markets": -0.8,
                "defense_spending": 0.3
            }
        }

        base_impact = base_impacts.get(conflict_name, {
            "global_gdp": -0.3,
            "inflation": 1.0,
            "volatility": 0.5
        })

        # Scale by escalation risk
        scaled_impact = {}
        for key, value in base_impact.items():
            scaled_impact[key] = value * escalation_risk

        return scaled_impact

    def _analyze_supply_chain_impact(self, conflict_name: str) -> Dict[str, float]:
        """Analyze supply chain disruption risks"""
        supply_chain_risks = {
            "Russia-Ukraine": {
                "energy": 0.8,
                "fertilizers": 0.7,
                "grains": 0.6,
                "metals": 0.5,
                "semiconductors": 0.3
            },
            "China-Taiwan": {
                "semiconductors": 0.9,
                "electronics": 0.8,
                "rare_earths": 0.7,
                "shipping": 0.6,
                "manufacturing": 0.7
            },
            "Middle_East": {
                "oil": 0.9,
                "gas": 0.7,
                "shipping": 0.6,
                "petrochemicals": 0.5
            }
        }

        return supply_chain_risks.get(conflict_name, {"general": 0.4})

    def _get_market_sector_impacts(self, conflict_name: str, escalation_risk: float) -> Dict[str, float]:
        """Get sector-specific market impacts"""
        sector_impacts = {
            "Russia-Ukraine": {
                "energy": 0.6,
                "defense": 0.8,
                "agriculture": 0.4,
                "technology": -0.3,
                "airlines": -0.6,
                "tourism": -0.7
            },
            "China-Taiwan": {
                "semiconductors": -0.9,
                "technology": -0.7,
                "defense": 0.8,
                "shipping": -0.6,
                "manufacturing": -0.5,
                "commodities": 0.3
            },
            "Middle_East": {
                "energy": 0.8,
                "defense": 0.6,
                "airlines": -0.5,
                "insurance": -0.4,
                "shipping": -0.3
            }
        }

        base_impacts = sector_impacts.get(conflict_name, {"broad_market": -0.2})

        # Scale by escalation risk
        scaled_impacts = {}
        for sector, impact in base_impacts.items():
            scaled_impacts[sector] = impact * escalation_risk

        return scaled_impacts

    def _estimate_conflict_duration(self, news: List[DataPoint], escalation_risk: float) -> str:
        """Estimate conflict duration based on escalation risk"""
        if escalation_risk > 0.8:
            return "6+ months (high intensity)"
        elif escalation_risk > 0.6:
            return "3-12 months (prolonged)"
        elif escalation_risk > 0.4:
            return "1-6 months (limited)"
        else:
            return "Weeks to months (contained)"

    def _default_geopolitical_risk(self) -> GeopoliticalRisk:
        """Return default risk in case of errors"""
        return GeopoliticalRisk(
            region="Global",
            risk_type="general_uncertainty",
            current_level=5,
            trend="stable",
            probability_impact=0.3,
            affected_sectors=["broad_market"],
            timeline="6-12 months",
            mitigation_strategies=["Maintain diversification", "Monitor developments"],
            confidence=0.3
        )

    def _default_conflict_signal(self, conflict_name: str) -> ConflictSignal:
        """Return default conflict signal"""
        return ConflictSignal(
            conflict_name=conflict_name,
            escalation_risk=0.3,
            economic_impact={"global_gdp": -0.1},
            supply_chain_risk={"general": 0.2},
            market_sectors={"broad_market": -0.1},
            duration_estimate="Unknown"
        )

    async def generate_llm_geopolitical_analysis(self, risks: List[GeopoliticalRisk]) -> Dict:
        """Generate LLM-enhanced geopolitical analysis"""
        try:
            # Prepare risk summary
            risk_summary = self._prepare_risk_summary(risks)

            prompt = f"""
            As a senior geopolitical risk analyst, analyze the current global risk landscape and provide investment guidance.

            Current Top Risks:
            {risk_summary}

            Please provide:
            1. Most critical risks requiring immediate attention
            2. Potential black swan scenarios (low probability, high impact)
            3. Portfolio hedging strategies
            4. Regional allocation recommendations
            5. Timeline for next major geopolitical shift
            6. Early warning indicators to monitor

            Format as JSON with keys: critical_risks, black_swan_scenarios, hedging_strategies, 
            regional_allocation, timeline, early_warning_indicators
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM geopolitical analysis: {e}")
            return {
                "critical_risks": ["Russia-Ukraine conflict", "US-China tensions"],
                "black_swan_scenarios": ["Taiwan invasion", "Nuclear incident"],
                "hedging_strategies": ["Increase defense allocation", "Add gold hedge"],
                "regional_allocation": "Underweight emerging markets",
                "timeline": "Next 6-12 months",
                "early_warning_indicators": ["Troop movements", "Diplomatic withdrawals"]
            }

    def _prepare_risk_summary(self, risks: List[GeopoliticalRisk]) -> str:
        """Prepare risk summary for LLM analysis"""
        summary_lines = []

        for risk in risks[:5]:  # Top 5 risks
            summary_lines.append(
                f"{risk.region} - {risk.risk_type}: Level {risk.current_level}/10 "
                f"({risk.trend}), Impact: {risk.probability_impact:.2f}, "
                f"Sectors: {', '.join(risk.affected_sectors[:3])}"
            )

        return "\n".join(summary_lines)

    async def get_geopolitical_report(self) -> Dict:
        """Generate comprehensive geopolitical risk report"""
        # Analyze global risks
        global_risks = await self.analyze_global_risks()

        # Analyze specific conflicts
        conflicts = ["Russia-Ukraine", "China-Taiwan", "Middle_East"]
        conflict_analyses = []

        for conflict in conflicts:
            signal = await self.analyze_conflict_escalation(conflict)
            conflict_analyses.append({
                "conflict": conflict,
                "escalation_risk": signal.escalation_risk,
                "economic_impact": signal.economic_impact,
                "duration_estimate": signal.duration_estimate
            })

        # Generate LLM analysis
        llm_analysis = await self.generate_llm_geopolitical_analysis(global_risks)

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "global_risk_assessment": {
                "overall_risk_level": np.mean([risk.current_level for risk in global_risks]),
                "top_risks": [{
                    "region": risk.region,
                    "type": risk.risk_type,
                    "level": risk.current_level,
                    "trend": risk.trend,
                    "timeline": risk.timeline,
                    "affected_sectors": risk.affected_sectors
                } for risk in global_risks[:5]]
            },
            "conflict_monitor": conflict_analyses,
            "investment_implications": {
                "defensive_positioning": self._calculate_defensive_score(global_risks),
                "sector_rotation": self._get_sector_rotation_recommendations(global_risks),
                "regional_weights": self._get_regional_weight_adjustments(global_risks),
                "hedging_requirements": self._get_hedging_requirements(global_risks)
            },
            "llm_analysis": llm_analysis,
            "risk_monitoring": {
                "update_frequency": "every 6 hours",
                "key_indicators": ["news_flow", "market_volatility", "diplomatic_activity"],
                "escalation_triggers": ["military_movement", "sanctions_announcement", "alliance_activation"]
            }
        }

    def _calculate_defensive_score(self, risks: List[GeopoliticalRisk]) -> float:
        """Calculate how defensive portfolio should be"""
        high_risk_count = sum(1 for risk in risks if risk.current_level >= 7)
        escalating_risks = sum(1 for risk in risks if risk.trend == "escalating")

        defensive_score = (high_risk_count * 0.15 + escalating_risks * 0.10)
        return min(1.0, defensive_score)

    def _get_sector_rotation_recommendations(self, risks: List[GeopoliticalRisk]) -> Dict[str, str]:
        """Get sector rotation based on geopolitical risks"""
        sector_scores = defaultdict(float)

        for risk in risks:
            weight = risk.current_level * risk.confidence / 10
            for sector in risk.affected_sectors:
                if sector in self.sector_impact_map.get(risk.risk_type, {}):
                    impact = self.sector_impact_map[risk.risk_type][sector]
                    sector_scores[sector] += impact * weight

        recommendations = {}
        for sector, score in sector_scores.items():
            if score > 0.2:
                recommendations[sector] = "overweight"
            elif score < -0.2:
                recommendations[sector] = "underweight"
            else:
                recommendations[sector] = "neutral"

        return recommendations

    def _get_regional_weight_adjustments(self, risks: List[GeopoliticalRisk]) -> Dict[str, float]:
        """Get regional allocation adjustments"""
        regional_adjustments = {}

        for region, base_weight in self.regional_weights.items():
            region_risks = [risk for risk in risks if risk.region == region]

            if region_risks:
                avg_risk = np.mean([risk.current_level for risk in region_risks])
                # Reduce allocation for high-risk regions
                adjustment = max(-0.5, (5 - avg_risk) / 10)
                regional_adjustments[region] = base_weight * (1 + adjustment)
            else:
                regional_adjustments[region] = base_weight

        # Normalize to sum to 1
        total_weight = sum(regional_adjustments.values())
        return {region: weight / total_weight for region, weight in regional_adjustments.items()}

    def _get_hedging_requirements(self, risks: List[GeopoliticalRisk]) -> List[str]:
        """Get specific hedging requirements"""
        hedges = []

        high_risks = [risk for risk in risks if risk.current_level >= 7]

        for risk in high_risks:
            if "energy" in risk.affected_sectors:
                hedges.append("Energy price hedge")
            if "technology" in risk.affected_sectors:
                hedges.append("Tech sector put options")
            if "currency" in risk.affected_sectors:
                hedges.append("USD strength hedge")
            if risk.risk_type == "military_conflicts":
                hedges.append("Gold allocation increase")
                hedges.append("Defense sector exposure")

        return list(set(hedges))  # Remove duplicates