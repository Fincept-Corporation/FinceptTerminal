# -*- coding: utf-8 -*-
# agents/supply_chain_agent.py - Supply Chain Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - supply chain data
#   - logistics metrics
#   - inventory levels
#   - shipping information
#
# OUTPUT:
#   - Supply chain risk assessments
#   - Disruption alerts
#   - Efficiency metrics
#   - Supplier risk analysis
#
# PARAMETERS:
#   - supply_chain_tiers
#   - risk_indicators
#   - monitoring_regions
#   - disruption_thresholds
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

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class SupplyChainRisk:
    """Supply chain risk assessment"""
    disruption_type: str
    affected_regions: List[str]
    risk_level: int  # 1-10
    affected_sectors: List[str]
    price_impact: Dict[str, float]
    duration_estimate: str
    mitigation_difficulty: str


class SupplyChainAgent:
    """Supply chain disruption monitoring and investment impact analysis"""

    def __init__(self):
        self.name = "supply_chain"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Critical supply chain chokepoints
        self.chokepoints = {
            "shipping": ["Suez Canal", "Panama Canal", "Strait of Hormuz", "Strait of Malacca"],
            "semiconductors": ["Taiwan", "South Korea", "Netherlands", "Japan"],
            "rare_earths": ["China", "Australia", "Myanmar"],
            "energy": ["Russia", "Saudi Arabia", "Norway", "Qatar"],
            "agriculture": ["Ukraine", "Russia", "Brazil", "Argentina"]
        }

        # Sector impact mapping
        self.sector_impacts = {
            "shipping_disruption": {
                "retail": -0.6, "automotive": -0.8, "electronics": -0.7,
                "energy": 0.3, "logistics": -0.9, "commodities": 0.4
            },
            "semiconductor_shortage": {
                "automotive": -0.9, "electronics": -0.8, "telecommunications": -0.6,
                "industrial": -0.5, "healthcare": -0.3
            },
            "energy_supply_shock": {
                "airlines": -0.8, "chemicals": -0.7, "materials": -0.6,
                "energy": 0.8, "utilities": -0.4, "transportation": -0.7
            }
        }

    async def analyze_supply_chains(self) -> List[SupplyChainRisk]:
        """Analyze global supply chain risks"""
        risks = []

        # Shipping disruptions
        shipping_risk = await self._analyze_shipping_disruptions()
        if shipping_risk:
            risks.append(shipping_risk)

        # Semiconductor supply
        chip_risk = await self._analyze_semiconductor_supply()
        if chip_risk:
            risks.append(chip_risk)

        # Energy supply
        energy_risk = await self._analyze_energy_supply()
        if energy_risk:
            risks.append(energy_risk)

        # Critical materials
        materials_risk = await self._analyze_critical_materials()
        if materials_risk:
            risks.append(materials_risk)

        return risks

    async def _analyze_shipping_disruptions(self) -> Optional[SupplyChainRisk]:
        """Analyze global shipping and logistics disruptions"""
        try:
            # Fetch shipping-related news
            shipping_news = await self.data_manager.get_multi_source_data({
                "news": {
                    "query": "shipping disruption supply chain logistics",
                    "sources": ["reuters", "bloomberg"],
                    "hours_back": 72
                }
            })

            news_data = shipping_news.get("news", [])

            # Calculate risk level from news intensity
            disruption_keywords = [
                "port congestion", "shipping delay", "container shortage",
                "suez canal", "panama canal", "strike", "blockage"
            ]

            risk_score = 0
            for article in news_data:
                text = article.value.lower()
                keyword_count = sum(1 for keyword in disruption_keywords if keyword in text)
                risk_score += keyword_count * article.confidence

            if news_data:
                normalized_risk = min(risk_score / len(news_data) * 2, 10)
                risk_level = int(normalized_risk) if normalized_risk > 3 else 5
            else:
                risk_level = 4  # Baseline moderate risk

            # Determine affected regions and sectors
            affected_regions = ["Global", "Asia-Pacific", "Europe", "North America"]
            affected_sectors = ["retail", "automotive", "electronics", "consumer goods"]

            # Price impacts
            price_impacts = {
                "shipping_costs": 0.2 * (risk_level / 10),
                "commodity_prices": 0.15 * (risk_level / 10),
                "consumer_goods": 0.1 * (risk_level / 10)
            }

            return SupplyChainRisk(
                disruption_type="shipping_logistics",
                affected_regions=affected_regions,
                risk_level=risk_level,
                affected_sectors=affected_sectors,
                price_impact=price_impacts,
                duration_estimate="3-9 months" if risk_level > 6 else "1-6 months",
                mitigation_difficulty="moderate"
            )

        except Exception as e:
            logging.error(f"Error analyzing shipping disruptions: {e}")
            return None

    async def _analyze_semiconductor_supply(self) -> Optional[SupplyChainRisk]:
        """Analyze semiconductor supply chain risks"""
        try:
            # Fetch semiconductor news
            chip_news = await self.data_manager.get_multi_source_data({
                "news": {
                    "query": "semiconductor chip shortage taiwan TSMC",
                    "sources": ["reuters", "bloomberg"],
                    "hours_back": 168  # 1 week
                }
            })

            news_data = chip_news.get("news", [])

            # Semiconductor risk factors
            risk_keywords = [
                "chip shortage", "semiconductor", "taiwan tensions", "TSMC",
                "fabrication", "wafer", "supply constraint"
            ]

            geopolitical_keywords = [
                "taiwan strait", "china taiwan", "semiconductor sanctions",
                "export controls", "technology war"
            ]

            supply_risk = 0
            geopolitical_risk = 0

            for article in news_data:
                text = article.value.lower()
                supply_risk += sum(1 for keyword in risk_keywords if keyword in text)
                geopolitical_risk += sum(1 for keyword in geopolitical_keywords if keyword in text)

            # Combine risks
            total_articles = len(news_data) if news_data else 1
            combined_risk = (supply_risk + geopolitical_risk * 2) / total_articles
            risk_level = min(int(combined_risk * 1.5) + 4, 10)  # Base 4, scale up

            affected_regions = ["Global", "Taiwan", "South Korea", "Japan"]
            affected_sectors = ["automotive", "electronics", "telecommunications", "industrial"]

            price_impacts = {
                "semiconductor_prices": 0.3 * (risk_level / 10),
                "electronics": 0.2 * (risk_level / 10),
                "automotive": 0.25 * (risk_level / 10)
            }

            return SupplyChainRisk(
                disruption_type="semiconductor_shortage",
                affected_regions=affected_regions,
                risk_level=risk_level,
                affected_sectors=affected_sectors,
                price_impact=price_impacts,
                duration_estimate="12-24 months" if risk_level > 7 else "6-18 months",
                mitigation_difficulty="high"  # Long lead times for new fabs
            )

        except Exception as e:
            logging.error(f"Error analyzing semiconductor supply: {e}")
            return None

    async def _analyze_energy_supply(self) -> Optional[SupplyChainRisk]:
        """Analyze energy supply chain risks"""
        try:
            # Fetch energy supply news
            energy_news = await self.data_manager.get_multi_source_data({
                "news": {
                    "query": "energy supply oil gas pipeline russia ukraine",
                    "sources": ["reuters", "bloomberg"],
                    "hours_back": 96
                }
            })

            news_data = energy_news.get("news", [])

            # Energy supply risk indicators
            supply_risk_keywords = [
                "pipeline", "refinery shutdown", "oil supply", "gas supply",
                "energy crisis", "power shortage", "fuel shortage"
            ]

            geopolitical_keywords = [
                "russia sanctions", "opec", "energy weapon", "gas cutoff",
                "strategic reserve", "energy security"
            ]

            supply_disruption_score = 0
            geopolitical_score = 0

            for article in news_data:
                text = article.value.lower()
                supply_disruption_score += sum(1 for keyword in supply_risk_keywords if keyword in text)
                geopolitical_score += sum(1 for keyword in geopolitical_keywords if keyword in text)

            total_articles = len(news_data) if news_data else 1
            combined_score = (supply_disruption_score + geopolitical_score * 1.5) / total_articles
            risk_level = min(int(combined_score * 1.2) + 3, 10)

            affected_regions = ["Europe", "Asia", "Global"]
            affected_sectors = ["energy", "chemicals", "transportation", "utilities", "airlines"]

            price_impacts = {
                "oil_prices": 0.4 * (risk_level / 10),
                "gas_prices": 0.5 * (risk_level / 10),
                "electricity": 0.3 * (risk_level / 10),
                "petrochemicals": 0.25 * (risk_level / 10)
            }

            return SupplyChainRisk(
                disruption_type="energy_supply_shock",
                affected_regions=affected_regions,
                risk_level=risk_level,
                affected_sectors=affected_sectors,
                price_impact=price_impacts,
                duration_estimate="6-18 months" if risk_level > 6 else "3-12 months",
                mitigation_difficulty="moderate"
            )

        except Exception as e:
            logging.error(f"Error analyzing energy supply: {e}")
            return None

    async def _analyze_critical_materials(self) -> Optional[SupplyChainRisk]:
        """Analyze critical materials and rare earth supply"""
        try:
            # Fetch critical materials news
            materials_news = await self.data_manager.get_multi_source_data({
                "news": {
                    "query": "rare earth metals lithium copper critical minerals",
                    "sources": ["reuters", "bloomberg"],
                    "hours_back": 120
                }
            })

            news_data = materials_news.get("news", [])

            # Critical materials risk factors
            supply_keywords = [
                "rare earth", "lithium shortage", "copper supply", "nickel",
                "cobalt", "critical minerals", "mining disruption"
            ]

            geopolitical_keywords = [
                "china rare earth", "mining sanctions", "export restrictions",
                "strategic materials", "supply security"
            ]

            supply_risk = 0
            geo_risk = 0

            for article in news_data:
                text = article.value.lower()
                supply_risk += sum(1 for keyword in supply_keywords if keyword in text)
                geo_risk += sum(1 for keyword in geopolitical_keywords if keyword in text)

            total_articles = len(news_data) if news_data else 1
            combined_risk = (supply_risk + geo_risk * 1.3) / total_articles
            risk_level = min(int(combined_risk * 1.1) + 4, 10)

            affected_regions = ["Global", "China", "Australia", "DRC"]
            affected_sectors = ["renewable_energy", "electric_vehicles", "electronics", "defense"]

            price_impacts = {
                "rare_earth_prices": 0.6 * (risk_level / 10),
                "battery_metals": 0.4 * (risk_level / 10),
                "renewable_energy_costs": 0.2 * (risk_level / 10)
            }

            return SupplyChainRisk(
                disruption_type="critical_materials_shortage",
                affected_regions=affected_regions,
                risk_level=risk_level,
                affected_sectors=affected_sectors,
                price_impact=price_impacts,
                duration_estimate="12-36 months" if risk_level > 7 else "6-24 months",
                mitigation_difficulty="high"  # Limited alternative sources
            )

        except Exception as e:
            logging.error(f"Error analyzing critical materials: {e}")
            return None

    async def get_supply_chain_report(self) -> Dict:
        """Generate comprehensive supply chain risk report"""
        # Analyze supply chain risks
        supply_risks = await self.analyze_supply_chains()

        # Calculate overall supply chain stress
        overall_stress = self._calculate_supply_chain_stress(supply_risks)

        # Generate investment implications
        investment_implications = self._generate_investment_implications(supply_risks)

        # Generate LLM analysis
        llm_analysis = await self._generate_supply_chain_llm_analysis(supply_risks)

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "supply_chain_analysis": {
                "overall_stress_level": overall_stress,
                "stress_classification": self._classify_stress_level(overall_stress),
                "major_risks": [{
                    "disruption_type": risk.disruption_type,
                    "risk_level": risk.risk_level,
                    "affected_regions": risk.affected_regions,
                    "affected_sectors": risk.affected_sectors,
                    "price_impact": risk.price_impact,
                    "duration": risk.duration_estimate,
                    "mitigation_difficulty": risk.mitigation_difficulty
                } for risk in supply_risks]
            },
            "investment_implications": investment_implications,
            "sector_impact_analysis": self._analyze_sector_impacts(supply_risks),
            "geographic_exposure": self._analyze_geographic_exposure(supply_risks),
            "llm_analysis": llm_analysis,
            "monitoring_priorities": {
                "high_risk_disruptions": [risk.disruption_type for risk in supply_risks if risk.risk_level >= 7],
                "critical_chokepoints": self._identify_critical_chokepoints(supply_risks),
                "early_warning_indicators": [
                    "Shipping container rates", "Semiconductor lead times",
                    "Energy price volatility", "Critical mineral inventories"
                ]
            }
        }

    def _calculate_supply_chain_stress(self, risks: List[SupplyChainRisk]) -> float:
        """Calculate overall supply chain stress level"""
        if not risks:
            return 3.0  # Baseline stress

        # Weight risks by severity and mitigation difficulty
        weighted_stress = 0
        total_weight = 0

        difficulty_multipliers = {"low": 0.8, "moderate": 1.0, "high": 1.3}

        for risk in risks:
            multiplier = difficulty_multipliers.get(risk.mitigation_difficulty, 1.0)
            weight = risk.risk_level * multiplier
            weighted_stress += weight
            total_weight += 1

        return weighted_stress / total_weight if total_weight > 0 else 3.0

    def _classify_stress_level(self, stress_level: float) -> str:
        """Classify supply chain stress level"""
        if stress_level >= 8:
            return "critical"
        elif stress_level >= 6:
            return "high"
        elif stress_level >= 4:
            return "moderate"
        else:
            return "low"

    def _generate_investment_implications(self, risks: List[SupplyChainRisk]) -> Dict:
        """Generate investment implications from supply chain risks"""
        implications = {
            "beneficiary_sectors": [],
            "vulnerable_sectors": [],
            "geographic_allocation": {},
            "commodity_exposure": {},
            "hedging_strategies": []
        }

        # Aggregate sector impacts
        sector_impact_totals = {}

        for risk in risks:
            # Get sector impacts for this disruption type
            if risk.disruption_type in self.sector_impacts:
                sector_impacts = self.sector_impacts[risk.disruption_type]
                for sector, impact in sector_impacts.items():
                    sector_impact_totals[sector] = sector_impact_totals.get(sector, 0) + impact * (risk.risk_level / 10)

        # Classify sectors
        for sector, total_impact in sector_impact_totals.items():
            if total_impact > 0.2:
                implications["beneficiary_sectors"].append(sector)
            elif total_impact < -0.2:
                implications["vulnerable_sectors"].append(sector)

        # Geographic allocation
        high_risk_regions = set()
        for risk in risks:
            if risk.risk_level >= 6:
                high_risk_regions.update(risk.affected_regions)

        implications["geographic_allocation"] = {
            "avoid": list(high_risk_regions),
            "prefer": ["North America", "Australia"] if "Asia" in high_risk_regions else ["Domestic markets"]
        }

        # Commodity exposure
        for risk in risks:
            for commodity, impact in risk.price_impact.items():
                if impact > 0.1:
                    implications["commodity_exposure"][commodity] = "positive" if impact > 0 else "negative"

        # Hedging strategies
        if any(risk.risk_level >= 7 for risk in risks):
            implications["hedging_strategies"].extend([
                "Commodity price hedging",
                "Supply chain diversification",
                "Inventory buffers"
            ])

        return implications

    def _analyze_sector_impacts(self, risks: List[SupplyChainRisk]) -> Dict[str, Dict]:
        """Analyze impact on different sectors"""
        sector_analysis = {}

        # Aggregate all affected sectors
        all_sectors = set()
        for risk in risks:
            all_sectors.update(risk.affected_sectors)

        for sector in all_sectors:
            sector_risks = [risk for risk in risks if sector in risk.affected_sectors]

            if sector_risks:
                avg_risk_level = np.mean([risk.risk_level for risk in sector_risks])
                max_risk_level = max(risk.risk_level for risk in sector_risks)

                sector_analysis[sector] = {
                    "average_risk_level": avg_risk_level,
                    "maximum_risk_level": max_risk_level,
                    "number_of_risks": len(sector_risks),
                    "primary_risks": [risk.disruption_type for risk in sector_risks[:3]],
                    "investment_recommendation": self._get_sector_recommendation(avg_risk_level)
                }

        return sector_analysis

    def _get_sector_recommendation(self, risk_level: float) -> str:
        """Get investment recommendation based on sector risk level"""
        if risk_level >= 7:
            return "avoid"
        elif risk_level >= 5:
            return "underweight"
        elif risk_level >= 3:
            return "neutral"
        else:
            return "overweight"

    def _analyze_geographic_exposure(self, risks: List[SupplyChainRisk]) -> Dict[str, Dict]:
        """Analyze geographic exposure to supply chain risks"""
        geographic_analysis = {}

        # Aggregate all affected regions
        all_regions = set()
        for risk in risks:
            all_regions.update(risk.affected_regions)

        for region in all_regions:
            region_risks = [risk for risk in risks if region in risk.affected_regions]

            if region_risks:
                avg_risk = np.mean([risk.risk_level for risk in region_risks])

                geographic_analysis[region] = {
                    "average_risk_level": avg_risk,
                    "number_of_risks": len(region_risks),
                    "primary_disruption_types": list(set(risk.disruption_type for risk in region_risks)),
                    "investment_stance": "defensive" if avg_risk >= 6 else "neutral" if avg_risk >= 4 else "opportunistic"
                }

        return geographic_analysis

    def _identify_critical_chokepoints(self, risks: List[SupplyChainRisk]) -> List[str]:
        """Identify critical supply chain chokepoints"""
        critical_points = []

        for risk in risks:
            if risk.risk_level >= 6 and risk.mitigation_difficulty == "high":
                # These are critical chokepoints
                if risk.disruption_type == "semiconductor_shortage":
                    critical_points.extend(["Taiwan semiconductor fabs", "ASML lithography"])
                elif risk.disruption_type == "shipping_logistics":
                    critical_points.extend(["Suez Canal", "Panama Canal", "Shanghai ports"])
                elif risk.disruption_type == "energy_supply_shock":
                    critical_points.extend(["Russian pipelines", "Strait of Hormuz"])
                elif risk.disruption_type == "critical_materials_shortage":
                    critical_points.extend(["China rare earth production", "DRC cobalt mines"])

        return list(set(critical_points))  # Remove duplicates

    async def _generate_supply_chain_llm_analysis(self, risks: List[SupplyChainRisk]) -> Dict:
        """Generate LLM-enhanced supply chain analysis"""
        try:
            # Prepare risk summary
            risk_summary = self._prepare_risk_summary(risks)

            prompt = f"""
            As a senior supply chain risk analyst, analyze current global supply chain disruptions and provide investment guidance.

            Current Supply Chain Risks:
            {risk_summary}

            Please provide:
            1. Most critical supply chain vulnerabilities requiring immediate attention
            2. Investment opportunities arising from supply chain disruptions
            3. Sector rotation strategy based on supply chain resilience
            4. Geographic diversification recommendations
            5. Timeline for supply chain normalization
            6. Early warning indicators for supply chain crises

            Format as JSON with keys: critical_vulnerabilities, investment_opportunities, sector_strategy,
            geographic_diversification, normalization_timeline, early_warning_indicators
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM supply chain analysis: {e}")
            return {
                "critical_vulnerabilities": ["Semiconductor supply", "Energy security", "Shipping bottlenecks"],
                "investment_opportunities": ["Supply chain technology", "Regional suppliers", "Inventory management"],
                "sector_strategy": "Overweight supply chain resilient sectors, underweight vulnerable ones",
                "geographic_diversification": "Reduce Asia concentration, increase nearshoring exposure",
                "normalization_timeline": "18-36 months for full normalization",
                "early_warning_indicators": ["Container shipping rates", "Commodity inventories", "Trade flow data"]
            }

    def _prepare_risk_summary(self, risks: List[SupplyChainRisk]) -> str:
        """Prepare risk summary for LLM analysis"""
        summary_lines = []

        for risk in risks:
            summary_lines.append(
                f"{risk.disruption_type}: Risk Level {risk.risk_level}/10, "
                f"Regions: {', '.join(risk.affected_regions[:3])}, "
                f"Duration: {risk.duration_estimate}, "
                f"Sectors: {', '.join(risk.affected_sectors[:3])}, "
                f"Mitigation: {risk.mitigation_difficulty}"
            )

        return "\n".join(summary_lines) if summary_lines else "No major supply chain risks identified"