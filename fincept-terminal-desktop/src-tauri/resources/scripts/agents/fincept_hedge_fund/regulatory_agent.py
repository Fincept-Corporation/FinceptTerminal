# -*- coding: utf-8 -*-
# agents/regulatory_agent.py - Regulatory Intelligence Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - regulatory announcements
#   - compliance updates
#   - policy changes
#   - enforcement actions
#
# OUTPUT:
#   - Regulatory impact assessments
#   - Compliance requirement updates
#   - Policy change analysis
#   - Risk mitigation recommendations
#
# PARAMETERS:
#   - regulatory_sources
#   - compliance_frameworks
#   - risk_categories
#   - monitoring_frequency
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
from collections import defaultdict

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class RegulatorySignal:
    """Regulatory change signal structure"""
    regulation_type: str
    jurisdiction: str
    impact_level: int  # 1-10 scale
    implementation_timeline: str
    affected_sectors: List[str]
    market_impact: Dict[str, float]
    compliance_cost: float
    competitive_advantage: Dict[str, float]
    confidence: float


@dataclass
class PolicyChange:
    """Policy change monitoring structure"""
    policy_area: str
    change_type: str  # proposed, enacted, implemented
    probability_passage: float
    economic_impact: Dict[str, float]
    sector_winners: List[str]
    sector_losers: List[str]
    timeline: str


class RegulatoryAgent:
    """Advanced regulatory intelligence and policy change analysis"""

    def __init__(self):
        self.name = "regulatory"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Regulatory domains and their market impact weights
        self.regulatory_domains = {
            "financial_regulation": {
                "banking_rules": 0.25,
                "securities_regulation": 0.20,
                "derivatives_oversight": 0.15,
                "crypto_regulation": 0.15,
                "payment_systems": 0.10,
                "systemic_risk": 0.15
            },
            "environmental_regulation": {
                "carbon_pricing": 0.30,
                "renewable_mandates": 0.25,
                "emission_standards": 0.20,
                "plastic_bans": 0.10,
                "water_regulation": 0.08,
                "biodiversity_rules": 0.07
            },
            "technology_regulation": {
                "data_privacy": 0.25,
                "ai_governance": 0.20,
                "platform_regulation": 0.20,
                "cybersecurity_rules": 0.15,
                "semiconductor_controls": 0.20
            },
            "healthcare_regulation": {
                "drug_pricing": 0.30,
                "medical_device_approval": 0.25,
                "telehealth_rules": 0.15,
                "insurance_regulation": 0.15,
                "public_health_policy": 0.15
            },
            "tax_policy": {
                "corporate_tax_rates": 0.35,
                "international_tax_coordination": 0.25,
                "digital_services_tax": 0.20,
                "carbon_tax": 0.20
            }
        }

        # Sector impact mapping from regulatory changes
        self.sector_impact_map = {
            "financial_regulation": {
                "banks": -0.6,
                "insurance": -0.4,
                "asset_management": -0.3,
                "fintech": -0.5,
                "real_estate": -0.2,
                "consumer_finance": -0.4
            },
            "environmental_regulation": {
                "oil_gas": -0.8,
                "utilities": 0.3,
                "renewable_energy": 0.7,
                "automotive": -0.4,
                "chemicals": -0.5,
                "materials": -0.3,
                "industrials": -0.2
            },
            "technology_regulation": {
                "big_tech": -0.7,
                "social_media": -0.8,
                "semiconductors": -0.4,
                "cybersecurity": 0.5,
                "cloud_services": -0.3,
                "telecommunications": -0.2
            },
            "healthcare_regulation": {
                "pharmaceuticals": -0.5,
                "biotechnology": -0.3,
                "medical_devices": -0.4,
                "health_insurance": -0.6,
                "hospitals": -0.3,
                "telehealth": 0.4
            },
            "tax_policy": {
                "multinational_corps": -0.6,
                "domestic_focused": 0.2,
                "high_margin_tech": -0.5,
                "capital_intensive": 0.1,
                "dividend_stocks": -0.3
            }
        }

        # Regulatory keyword patterns
        self.regulatory_keywords = {
            "proposal_stage": [
                "proposed rule", "draft regulation", "consultation paper",
                "request for comment", "policy proposal", "legislative draft"
            ],
            "advancement_stage": [
                "committee approval", "senate passage", "house passage",
                "regulatory approval", "agency adoption", "cabinet approval"
            ],
            "implementation_stage": [
                "effective date", "compliance deadline", "enforcement begins",
                "mandatory compliance", "phased implementation", "grace period"
            ],
            "enforcement_action": [
                "regulatory fine", "enforcement action", "compliance violation",
                "penalty imposed", "consent order", "regulatory sanction"
            ]
        }

        # Jurisdictional weights based on global market impact
        self.jurisdiction_weights = {
            "United_States": 0.35,
            "European_Union": 0.25,
            "China": 0.20,
            "United_Kingdom": 0.10,
            "Japan": 0.05,
            "Other": 0.05
        }

    async def analyze_regulatory_landscape(self) -> List[RegulatorySignal]:
        """Comprehensive regulatory landscape analysis"""
        signals = []

        try:
            # Analyze each regulatory domain
            for domain, regulations in self.regulatory_domains.items():
                domain_signals = await self._analyze_regulatory_domain(domain, regulations)
                signals.extend(domain_signals)

            # Sort by impact level and confidence
            signals.sort(key=lambda x: x.impact_level * x.confidence, reverse=True)

            return signals[:15]  # Return top 15 regulatory signals

        except Exception as e:
            logging.error(f"Error in regulatory landscape analysis: {e}")
            return [self._default_regulatory_signal()]

    async def _analyze_regulatory_domain(self, domain: str, regulations: Dict[str, float]) -> List[RegulatorySignal]:
        """Analyze specific regulatory domain"""
        domain_signals = []

        # Get news data for this domain
        news_data = await self._fetch_regulatory_news(domain, list(regulations.keys()))

        for regulation, weight in regulations.items():
            try:
                # Filter news for this specific regulation
                relevant_news = [news for news in news_data
                                 if self._is_relevant_to_regulation(news, regulation, domain)]

                if not relevant_news:
                    continue

                # Analyze regulatory stage and probability
                stage_analysis = self._analyze_regulatory_stage(relevant_news)

                # Calculate impact level
                impact_level = self._calculate_regulatory_impact(relevant_news, regulation, domain, weight)

                # Determine affected sectors
                affected_sectors = self._get_affected_sectors(domain, impact_level)

                # Calculate market impact
                market_impact = self._calculate_market_impact(domain, regulation, impact_level)

                # Estimate compliance costs
                compliance_cost = self._estimate_compliance_cost(regulation, domain, impact_level)

                # Identify competitive advantages
                competitive_advantage = self._analyze_competitive_impact(domain, regulation, relevant_news)

                # Generate timeline
                timeline = self._estimate_implementation_timeline(stage_analysis, relevant_news)

                # Calculate confidence
                confidence = self._calculate_regulatory_confidence(relevant_news, stage_analysis)

                signal = RegulatorySignal(
                    regulation_type=f"{domain}_{regulation}",
                    jurisdiction=self._determine_jurisdiction(relevant_news),
                    impact_level=impact_level,
                    implementation_timeline=timeline,
                    affected_sectors=affected_sectors,
                    market_impact=market_impact,
                    compliance_cost=compliance_cost,
                    competitive_advantage=competitive_advantage,
                    confidence=confidence
                )

                domain_signals.append(signal)

            except Exception as e:
                logging.error(f"Error analyzing regulation {regulation}: {e}")
                continue

        return domain_signals

    async def _fetch_regulatory_news(self, domain: str, regulations: List[str]) -> List[DataPoint]:
        """Fetch regulatory news for specific domain"""
        all_news = []

        # Create domain-specific search queries
        domain_queries = {
            "financial_regulation": ["banking regulation", "financial oversight", "securities rules",
                                     "fintech regulation"],
            "environmental_regulation": ["environmental regulation", "carbon policy", "emissions standards",
                                         "green legislation"],
            "technology_regulation": ["tech regulation", "data privacy", "ai governance", "platform oversight"],
            "healthcare_regulation": ["healthcare policy", "drug regulation", "medical device approval",
                                      "health insurance"],
            "tax_policy": ["tax reform", "corporate tax", "international taxation", "digital tax"]
        }

        queries = domain_queries.get(domain, [domain.replace("_", " ")])

        for query in queries:
            try:
                news_data = await self.data_manager.get_multi_source_data({
                    "news": {
                        "query": query,
                        "sources": ["reuters", "bloomberg", "wsj", "ft"],
                        "hours_back": 168  # 1 week
                    }
                })

                if "news" in news_data:
                    all_news.extend(news_data["news"])

            except Exception as e:
                logging.error(f"Error fetching regulatory news for {query}: {e}")
                continue

        return all_news

    def _is_relevant_to_regulation(self, news: DataPoint, regulation: str, domain: str) -> bool:
        """Check if news is relevant to specific regulation"""
        text = (news.value + " " + news.metadata.get("description", "")).lower()

        # Create regulation-specific keywords
        regulation_keywords = {
            "banking_rules": ["basel", "capital requirements", "stress test", "bank supervision"],
            "carbon_pricing": ["carbon tax", "cap and trade", "emissions trading", "carbon border"],
            "data_privacy": ["gdpr", "data protection", "privacy regulation", "data governance"],
            "drug_pricing": ["prescription drug", "medicare negotiation", "drug pricing", "pharmaceutical"],
            "ai_governance": ["artificial intelligence", "ai regulation", "algorithmic oversight", "ai safety"]
        }

        keywords = regulation_keywords.get(regulation, [regulation.replace("_", " ")])

        # Also check for regulatory action keywords
        regulatory_action_keywords = [
            "regulation", "policy", "rule", "legislation", "bill", "act",
            "oversight", "compliance", "enforcement", "approval", "mandate"
        ]

        has_regulation_keyword = any(keyword in text for keyword in keywords)
        has_action_keyword = any(keyword in text for keyword in regulatory_action_keywords)

        return has_regulation_keyword and has_action_keyword

    def _analyze_regulatory_stage(self, news: List[DataPoint]) -> Dict[str, float]:
        """Analyze what stage regulations are in"""
        stage_scores = {
            "proposal": 0.0,
            "advancement": 0.0,
            "implementation": 0.0,
            "enforcement": 0.0
        }

        total_articles = len(news)
        if total_articles == 0:
            return stage_scores

        for article in news:
            text = article.value.lower()

            # Count stage indicators
            for stage, keywords in self.regulatory_keywords.items():
                stage_name = stage.replace("_stage", "").replace("_action", "")
                for keyword in keywords:
                    if keyword in text:
                        stage_scores[stage_name] += article.confidence / total_articles

        return stage_scores

    def _calculate_regulatory_impact(self, news: List[DataPoint], regulation: str,
                                     domain: str, weight: float) -> int:
        """Calculate regulatory impact level (1-10)"""
        if not news:
            return 3

        # Base impact levels by regulation type
        base_impacts = {
            "banking_rules": 7,
            "carbon_pricing": 8,
            "data_privacy": 6,
            "drug_pricing": 8,
            "corporate_tax_rates": 9,
            "ai_governance": 5,
            "securities_regulation": 6
        }

        base_impact = base_impacts.get(regulation, 5)

        # Analyze news intensity and scope
        high_impact_keywords = [
            "sweeping changes", "major reform", "significant impact", "industry transformation",
            "mandatory compliance", "substantial penalties", "widespread adoption",
            "market disruption", "regulatory overhaul", "unprecedented"
        ]

        scope_indicators = [
            "global", "international", "nationwide", "industry-wide", "comprehensive",
            "all companies", "every business", "mandatory for all"
        ]

        intensity_score = 0
        scope_score = 0

        for article in news:
            text = article.value.lower()
            intensity_score += sum(1 for keyword in high_impact_keywords if keyword in text)
            scope_score += sum(1 for keyword in scope_indicators if keyword in text)

        # Adjust base impact
        if news:
            intensity_factor = min(intensity_score / len(news), 2.0)
            scope_factor = min(scope_score / len(news), 1.5)
            weight_factor = weight * 2  # Domain weight influence

            adjusted_impact = base_impact + intensity_factor + scope_factor + weight_factor
        else:
            adjusted_impact = base_impact

        return int(np.clip(adjusted_impact, 1, 10))

    def _get_affected_sectors(self, domain: str, impact_level: int) -> List[str]:
        """Get sectors affected by regulatory domain"""
        if domain not in self.sector_impact_map:
            return ["broad_market"]

        sector_impacts = self.sector_impact_map[domain]

        # Filter sectors by impact threshold
        threshold = 0.2 if impact_level > 7 else 0.3 if impact_level > 5 else 0.4

        affected_sectors = []
        for sector, impact in sector_impacts.items():
            if abs(impact) >= threshold:
                affected_sectors.append(sector)

        return affected_sectors or ["broad_market"]

    def _calculate_market_impact(self, domain: str, regulation: str, impact_level: int) -> Dict[str, float]:
        """Calculate expected market impact by asset class"""
        if domain not in self.sector_impact_map:
            return {"broad_market": 0.0}

        sector_impacts = self.sector_impact_map[domain]

        # Scale impacts by regulation impact level
        impact_multiplier = impact_level / 10.0

        market_impacts = {}
        for sector, base_impact in sector_impacts.items():
            scaled_impact = base_impact * impact_multiplier
            market_impacts[sector] = np.clip(scaled_impact, -1.0, 1.0)

        return market_impacts

    def _estimate_compliance_cost(self, regulation: str, domain: str, impact_level: int) -> float:
        """Estimate compliance cost as percentage of revenue"""
        # Base compliance costs by regulation type (% of revenue)
        base_costs = {
            "banking_rules": 0.015,  # 1.5% of revenue
            "data_privacy": 0.008,  # 0.8% of revenue
            "emissions_standards": 0.012,  # 1.2% of revenue
            "drug_pricing": 0.005,  # 0.5% of revenue
            "cybersecurity_rules": 0.010  # 1.0% of revenue
        }

        base_cost = base_costs.get(regulation, 0.005)

        # Scale by impact level
        impact_multiplier = impact_level / 5.0  # Normalize around level 5

        return base_cost * impact_multiplier

    def _analyze_competitive_impact(self, domain: str, regulation: str,
                                    news: List[DataPoint]) -> Dict[str, float]:
        """Analyze which companies/sectors gain competitive advantage"""
        competitive_advantages = {}

        # Analyze news for competitive implications
        advantage_keywords = {
            "large_companies": ["economies of scale", "large companies benefit", "big players advantage"],
            "incumbents": ["existing players", "incumbent advantage", "barriers to entry"],
            "tech_leaders": ["technology advantage", "innovation leaders", "digital transformation"],
            "compliance_specialists": ["compliance expertise", "regulatory experience", "specialized knowledge"]
        }

        for company_type, keywords in advantage_keywords.items():
            advantage_score = 0.0

            for article in news:
                text = article.value.lower()
                keyword_count = sum(1 for keyword in keywords if keyword in text)
                advantage_score += keyword_count * article.confidence

            if news and advantage_score > 0:
                competitive_advantages[company_type] = min(advantage_score / len(news), 1.0)

        return competitive_advantages

    def _estimate_implementation_timeline(self, stage_analysis: Dict[str, float],
                                          news: List[DataPoint]) -> str:
        """Estimate regulatory implementation timeline"""
        # Determine current stage
        current_stage = max(stage_analysis.items(), key=lambda x: x[1])[0]

        # Timeline estimates by stage
        timeline_estimates = {
            "proposal": "12-24 months",
            "advancement": "6-18 months",
            "implementation": "3-12 months",
            "enforcement": "Immediate"
        }

        base_timeline = timeline_estimates.get(current_stage, "12-24 months")

        # Look for specific timeline mentions in news
        timeline_patterns = [
            r"(\d+)\s*months?",
            r"(\d+)\s*years?",
            r"by\s+(\d{4})",
            r"effective\s+(\w+\s+\d{1,2},?\s+\d{4})"
        ]

        for article in news:
            text = article.value
            for pattern in timeline_patterns:
                matches = re.findall(pattern, text, re.IGNORECASE)
                if matches:
                    # Found specific timeline mention
                    return f"Specific timeline mentioned: {matches[0]}"

        return base_timeline

    def _determine_jurisdiction(self, news: List[DataPoint]) -> str:
        """Determine primary jurisdiction from news"""
        jurisdiction_keywords = {
            "United_States": ["usa", "united states", "sec", "fed", "congress", "senate", "house"],
            "European_Union": ["eu", "european union", "brussels", "european commission", "esma"],
            "China": ["china", "beijing", "pboc", "csrc", "chinese regulators"],
            "United_Kingdom": ["uk", "united kingdom", "fca", "pra", "bank of england"],
            "Japan": ["japan", "jfsa", "boj", "bank of japan"]
        }

        jurisdiction_scores = {j: 0 for j in jurisdiction_keywords.keys()}

        for article in news:
            text = article.value.lower()
            for jurisdiction, keywords in jurisdiction_keywords.items():
                score = sum(1 for keyword in keywords if keyword in text)
                jurisdiction_scores[jurisdiction] += score * article.confidence

        if any(jurisdiction_scores.values()):
            return max(jurisdiction_scores.items(), key=lambda x: x[1])[0]

        return "Multiple/Global"

    def _calculate_regulatory_confidence(self, news: List[DataPoint],
                                         stage_analysis: Dict[str, float]) -> float:
        """Calculate confidence in regulatory analysis"""
        confidence_factors = []

        # News volume and quality
        if news:
            news_confidence = min(len(news) / 10, 1.0) * 0.3  # Optimal around 10 articles
            source_quality = np.mean([article.confidence for article in news]) * 0.4
            confidence_factors.extend([news_confidence, source_quality])
        else:
            confidence_factors.extend([0.1, 0.2])

        # Stage clarity - clearer stage = higher confidence
        max_stage_score = max(stage_analysis.values()) if stage_analysis else 0
        stage_confidence = max_stage_score * 0.3
        confidence_factors.append(stage_confidence)

        return np.sum(confidence_factors)

    def _default_regulatory_signal(self) -> RegulatorySignal:
        """Return default regulatory signal"""
        return RegulatorySignal(
            regulation_type="general_regulatory_uncertainty",
            jurisdiction="Global",
            impact_level=5,
            implementation_timeline="12-24 months",
            affected_sectors=["broad_market"],
            market_impact={"broad_market": 0.0},
            compliance_cost=0.005,
            competitive_advantage={},
            confidence=0.3
        )

    async def analyze_policy_changes(self) -> List[PolicyChange]:
        """Analyze upcoming policy changes and their market implications"""
        policy_changes = []

        policy_areas = [
            "tax_reform", "healthcare_policy", "infrastructure_spending",
            "trade_policy", "energy_policy", "immigration_policy"
        ]

        for policy_area in policy_areas:
            try:
                change = await self._analyze_specific_policy(policy_area)
                if change:
                    policy_changes.append(change)
            except Exception as e:
                logging.error(f"Error analyzing policy {policy_area}: {e}")
                continue

        return policy_changes

    async def _analyze_specific_policy(self, policy_area: str) -> Optional[PolicyChange]:
        """Analyze specific policy area for changes"""
        # Get policy-related news
        news_data = await self.data_manager.get_multi_source_data({
            "news": {
                "query": policy_area.replace("_", " "),
                "sources": ["reuters", "bloomberg", "wsj"],
                "hours_back": 72
            }
        })

        news = news_data.get("news", [])
        if not news:
            return None

        # Analyze policy change type and probability
        change_type = self._determine_policy_change_type(news)
        probability = self._calculate_passage_probability(news, policy_area)

        if probability < 0.2:  # Skip low-probability changes
            return None

        # Calculate economic impact
        economic_impact = self._calculate_policy_economic_impact(policy_area, news)

        # Identify winners and losers
        winners, losers = self._identify_policy_winners_losers(policy_area)

        # Estimate timeline
        timeline = self._estimate_policy_timeline(news, change_type)

        return PolicyChange(
            policy_area=policy_area,
            change_type=change_type,
            probability_passage=probability,
            economic_impact=economic_impact,
            sector_winners=winners,
            sector_losers=losers,
            timeline=timeline
        )

    def _determine_policy_change_type(self, news: List[DataPoint]) -> str:
        """Determine type of policy change"""
        change_keywords = {
            "proposed": ["proposed", "draft", "plan", "considering", "exploring"],
            "enacted": ["passed", "approved", "enacted", "signed", "adopted"],
            "implemented": ["effective", "implementation", "rollout", "enforcement"]
        }

        change_scores = {change_type: 0 for change_type in change_keywords.keys()}

        for article in news:
            text = article.value.lower()
            for change_type, keywords in change_keywords.items():
                score = sum(1 for keyword in keywords if keyword in text)
                change_scores[change_type] += score

        return max(change_scores.items(), key=lambda x: x[1])[0] if any(change_scores.values()) else "proposed"

    def _calculate_passage_probability(self, news: List[DataPoint], policy_area: str) -> float:
        """Calculate probability of policy passage"""
        # Base probabilities by policy area and political difficulty
        base_probabilities = {
            "tax_reform": 0.4,
            "healthcare_policy": 0.3,
            "infrastructure_spending": 0.6,
            "trade_policy": 0.5,
            "energy_policy": 0.4,
            "immigration_policy": 0.2
        }

        base_prob = base_probabilities.get(policy_area, 0.4)

        # Analyze news sentiment for passage likelihood
        positive_indicators = ["bipartisan support", "broad agreement", "likely to pass", "strong support"]
        negative_indicators = ["opposition", "unlikely to pass", "political deadlock", "controversy"]

        positive_score = 0
        negative_score = 0

        for article in news:
            text = article.value.lower()
            positive_score += sum(1 for indicator in positive_indicators if indicator in text)
            negative_score += sum(1 for indicator in negative_indicators if indicator in text)

        # Adjust probability based on sentiment
        if news:
            sentiment_adjustment = (positive_score - negative_score) / len(news) * 0.3
            adjusted_prob = base_prob + sentiment_adjustment
        else:
            adjusted_prob = base_prob

        return np.clip(adjusted_prob, 0.0, 1.0)

    def _calculate_policy_economic_impact(self, policy_area: str, news: List[DataPoint]) -> Dict[str, float]:
        """Calculate economic impact of policy changes"""
        # Base economic impacts by policy area (% GDP impact)
        base_impacts = {
            "tax_reform": {"gdp_growth": 0.3, "business_investment": 0.5, "consumer_spending": 0.2},
            "infrastructure_spending": {"gdp_growth": 0.8, "employment": 0.6, "productivity": 0.4},
            "healthcare_policy": {"healthcare_costs": -0.3, "business_costs": 0.2, "innovation": -0.1},
            "trade_policy": {"trade_volumes": 0.4, "manufacturing": 0.3, "consumer_prices": -0.2},
            "energy_policy": {"energy_costs": -0.2, "green_investment": 0.6, "traditional_energy": -0.4}
        }

        return base_impacts.get(policy_area, {"gdp_growth": 0.1})

    def _identify_policy_winners_losers(self, policy_area: str) -> Tuple[List[str], List[str]]:
        """Identify sector winners and losers from policy changes"""
        policy_impacts = {
            "tax_reform": {
                "winners": ["domestic_companies", "small_business", "manufacturing"],
                "losers": ["multinationals", "high_tax_rate_sectors"]
            },
            "infrastructure_spending": {
                "winners": ["construction", "materials", "industrials", "transportation"],
                "losers": ["bond_investors"]  # Higher deficits
            },
            "healthcare_policy": {
                "winners": ["healthcare_providers", "medical_devices"],
                "losers": ["pharmaceuticals", "health_insurance"]
            },
            "energy_policy": {
                "winners": ["renewable_energy", "electric_vehicles", "grid_infrastructure"],
                "losers": ["fossil_fuels", "traditional_utilities"]
            }
        }

        impacts = policy_impacts.get(policy_area, {"winners": [], "losers": []})
        return impacts["winners"], impacts["losers"]

    def _estimate_policy_timeline(self, news: List[DataPoint], change_type: str) -> str:
        """Estimate policy implementation timeline"""
        timeline_map = {
            "proposed": "6-18 months to passage",
            "enacted": "3-12 months to implementation",
            "implemented": "Currently rolling out"
        }

        return timeline_map.get(change_type, "12-24 months")

    async def get_regulatory_report(self) -> Dict:
        """Generate comprehensive regulatory intelligence report"""
        # Analyze regulatory landscape
        regulatory_signals = await self.analyze_regulatory_landscape()

        # Analyze policy changes
        policy_changes = await self.analyze_policy_changes()

        # Generate LLM analysis
        llm_analysis = await self._generate_regulatory_llm_analysis(regulatory_signals, policy_changes)

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "regulatory_landscape": {
                "high_impact_regulations": [{
                    "regulation": signal.regulation_type,
                    "jurisdiction": signal.jurisdiction,
                    "impact_level": signal.impact_level,
                    "timeline": signal.implementation_timeline,
                    "affected_sectors": signal.affected_sectors,
                    "market_impact": signal.market_impact
                } for signal in regulatory_signals[:5]],
                "compliance_cost_estimate": np.mean([s.compliance_cost for s in regulatory_signals]),
                "overall_regulatory_pressure": np.mean([s.impact_level for s in regulatory_signals])
            },
            "policy_changes": [{
                "policy_area": change.policy_area,
                "change_type": change.change_type,
                "probability": change.probability_passage,
                "economic_impact": change.economic_impact,
                "winners": change.sector_winners,
                "losers": change.sector_losers,
                "timeline": change.timeline
            } for change in policy_changes],
            "investment_implications": {
                "defensive_sectors": self._get_defensive_regulatory_sectors(regulatory_signals),
                "regulatory_beneficiaries": self._get_regulatory_beneficiaries(regulatory_signals),
                "compliance_leaders": self._get_compliance_advantage_sectors(regulatory_signals),
                "regulatory_risk_sectors": self._get_high_risk_sectors(regulatory_signals)
            },
            "llm_analysis": llm_analysis,
            "monitoring_priorities": {
                "key_regulations": [s.regulation_type for s in regulatory_signals[:3]],
                "high_probability_policies": [c.policy_area for c in policy_changes if c.probability_passage > 0.6],
                "immediate_compliance_deadlines": [s.regulation_type for s in regulatory_signals
                                                   if "months" in s.implementation_timeline and
                                                   int(s.implementation_timeline.split()[0]) <= 6]
            }
        }

    async def _generate_regulatory_llm_analysis(self, signals: List[RegulatorySignal],
                                                policy_changes: List[PolicyChange]) -> Dict:
        """Generate LLM-enhanced regulatory analysis"""
        try:
            # Prepare regulatory summary
            reg_summary = self._prepare_regulatory_summary(signals)
            policy_summary = self._prepare_policy_summary(policy_changes)

            prompt = f"""
            As a senior regulatory affairs analyst, analyze the current regulatory landscape and provide investment guidance.

            Key Regulatory Developments:
            {reg_summary}

            Policy Changes:
            {policy_summary}

            Please provide:
            1. Most critical regulatory risks requiring immediate attention
            2. Regulatory arbitrage opportunities (sectors benefiting from regulatory changes)
            3. Compliance cost leaders vs laggards investment strategy
            4. Timeline for next major regulatory wave
            5. Sector rotation recommendations based on regulatory trends
            6. Early warning indicators for regulatory surprises

            Format as JSON with keys: critical_risks, arbitrage_opportunities, compliance_strategy, 
            regulatory_timeline, sector_rotation, early_warning_indicators
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM regulatory analysis: {e}")
            return {
                "critical_risks": ["Financial regulation tightening", "ESG compliance requirements"],
                "arbitrage_opportunities": ["Compliance technology", "Renewable energy"],
                "compliance_strategy": "Focus on established players with regulatory expertise",
                "regulatory_timeline": "Next 12-18 months",
                "sector_rotation": "Overweight compliance leaders, underweight regulatory laggards",
                "early_warning_indicators": ["Congressional hearings", "Agency enforcement actions"]
            }

    def _prepare_regulatory_summary(self, signals: List[RegulatorySignal]) -> str:
        """Prepare regulatory summary for LLM"""
        summary_lines = []

        for signal in signals[:5]:  # Top 5 signals
            summary_lines.append(
                f"{signal.regulation_type} ({signal.jurisdiction}): "
                f"Impact Level {signal.impact_level}/10, "
                f"Timeline: {signal.implementation_timeline}, "
                f"Sectors: {', '.join(signal.affected_sectors[:3])}, "
                f"Compliance Cost: {signal.compliance_cost:.1%}"
            )

        return "\n".join(summary_lines)

    def _prepare_policy_summary(self, policy_changes: List[PolicyChange]) -> str:
        """Prepare policy changes summary for LLM"""
        summary_lines = []

        for policy in policy_changes:
            summary_lines.append(
                f"{policy.policy_area}: {policy.change_type} "
                f"(Probability: {policy.probability_passage:.0%}), "
                f"Winners: {', '.join(policy.sector_winners[:2])}, "
                f"Losers: {', '.join(policy.sector_losers[:2])}"
            )

        return "\n".join(summary_lines)

    def _get_defensive_regulatory_sectors(self, signals: List[RegulatorySignal]) -> List[str]:
        """Get sectors that are defensive against regulatory risk"""
        defensive_sectors = []

        # Sectors typically less affected by regulation
        inherently_defensive = ["utilities", "consumer_staples", "healthcare_providers"]

        # Sectors with positive regulatory tailwinds
        for signal in signals:
            for sector, impact in signal.market_impact.items():
                if impact > 0.3:  # Positive regulatory impact
                    defensive_sectors.append(sector)

        all_defensive = inherently_defensive + defensive_sectors
        return list(set(all_defensive))  # Remove duplicates

    def _get_regulatory_beneficiaries(self, signals: List[RegulatorySignal]) -> List[str]:
        """Get sectors that benefit from regulatory changes"""
        beneficiaries = []

        for signal in signals:
            for sector, impact in signal.market_impact.items():
                if impact > 0.4:  # Strong positive impact
                    beneficiaries.append(sector)

            # Check competitive advantages
            for advantage_type, score in signal.competitive_advantage.items():
                if score > 0.5 and advantage_type == "compliance_specialists":
                    beneficiaries.extend(["legal_services", "compliance_tech", "consulting"])

        return list(set(beneficiaries))

    def _get_compliance_advantage_sectors(self, signals: List[RegulatorySignal]) -> List[str]:
        """Get sectors with compliance competitive advantages"""
        advantage_sectors = []

        for signal in signals:
            for advantage_type, score in signal.competitive_advantage.items():
                if score > 0.4:
                    if advantage_type == "large_companies":
                        advantage_sectors.extend(["mega_cap_stocks", "blue_chip_companies"])
                    elif advantage_type == "tech_leaders":
                        advantage_sectors.extend(["technology", "software"])
                    elif advantage_type == "incumbents":
                        advantage_sectors.extend(["established_players"])

        return list(set(advantage_sectors))

    def _get_high_risk_sectors(self, signals: List[RegulatorySignal]) -> List[str]:
        """Get sectors with high regulatory risk"""
        high_risk_sectors = []

        for signal in signals:
            if signal.impact_level >= 7:  # High impact regulations
                for sector, impact in signal.market_impact.items():
                    if impact < -0.3:  # Negative impact
                        high_risk_sectors.append(sector)

        return list(set(high_risk_sectors))