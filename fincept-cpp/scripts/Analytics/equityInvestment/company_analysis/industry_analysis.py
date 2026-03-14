
"""Equity Investment Industry Analysis Module
======================================

Industry and sector analysis tools

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Market price data and trading volume information
  - Industry reports and competitive analysis data
  - Management guidance and analyst estimates
  - Economic indicators affecting equity markets

OUTPUT:
  - Equity valuation models and fair value estimates
  - Fundamental analysis metrics and financial ratios
  - Investment recommendations and target prices
  - Risk assessments and portfolio implications
  - Sector and industry comparative analysis

PARAMETERS:
  - valuation_method: Primary valuation methodology (default: 'DCF')
  - discount_rate: Discount rate for valuation (default: 0.10)
  - terminal_growth: Terminal growth rate assumption (default: 0.025)
  - earnings_multiple: Target earnings multiple (default: 15.0)
  - reporting_currency: Reporting currency (default: 'USD')
"""



import numpy as np
import pandas as pd
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
import warnings

from .base_models import (
    BaseCompanyAnalysisModel, CompanyData, ValidationError,
    FinceptAnalyticsError
)


class IndustryClassificationSystem(Enum):
    """Industry classification systems"""
    GICS = "Global Industry Classification Standard"
    ICB = "Industry Classification Benchmark"
    NAICS = "North American Industry Classification System"
    SIC = "Standard Industrial Classification"


class CompetitivePosition(Enum):
    """Competitive position categories"""
    MARKET_LEADER = "market_leader"
    STRONG_COMPETITOR = "strong_competitor"
    NICHE_PLAYER = "niche_player"
    STRUGGLING_COMPETITOR = "struggling_competitor"


@dataclass
class IndustryMetrics:
    """Industry-level metrics and characteristics"""
    industry_name: str
    total_market_size: float
    growth_rate: float
    number_of_companies: int
    concentration_ratio: float
    average_roe: float
    average_margin: float
    cyclicality: str
    capital_intensity: str
    regulatory_environment: str


@dataclass
class CompetitorProfile:
    """Individual competitor profile"""
    company_name: str
    symbol: str
    market_cap: float
    market_share: float
    revenue: float
    profit_margin: float
    competitive_advantages: List[str]
    weaknesses: List[str]


@dataclass
class PortersFiveForcesAnalysis:
    """Porter's Five Forces analysis structure"""
    threat_of_new_entrants: Dict[str, Any]
    bargaining_power_suppliers: Dict[str, Any]
    bargaining_power_buyers: Dict[str, Any]
    threat_of_substitutes: Dict[str, Any]
    competitive_rivalry: Dict[str, Any]
    overall_attractiveness: str


@dataclass
class PESTLEAnalysis:
    """PESTLE analysis structure"""
    political_factors: List[str]
    economic_factors: List[str]
    social_factors: List[str]
    technological_factors: List[str]
    legal_factors: List[str]
    environmental_factors: List[str]
    overall_impact: str


class IndustryClassifier:
    """Industry classification and grouping"""

    def __init__(self):
        # GICS sector and industry mappings (simplified)
        self.gics_sectors = {
            'Energy': ['Oil & Gas Exploration', 'Oil & Gas Refining', 'Coal & Consumable Fuels'],
            'Materials': ['Chemicals', 'Metals & Mining', 'Paper & Forest Products'],
            'Industrials': ['Aerospace & Defense', 'Airlines', 'Construction & Engineering'],
            'Consumer Discretionary': ['Automobiles', 'Hotels & Restaurants', 'Retail'],
            'Consumer Staples': ['Food & Beverages', 'Household Products', 'Tobacco'],
            'Health Care': ['Biotechnology', 'Pharmaceuticals', 'Health Care Equipment'],
            'Financials': ['Banks', 'Insurance', 'Real Estate'],
            'Information Technology': ['Software', 'Semiconductors', 'IT Services'],
            'Communication Services': ['Telecommunications', 'Media & Entertainment'],
            'Utilities': ['Electric Utilities', 'Gas Utilities', 'Water Utilities'],
            'Real Estate': ['REITs', 'Real Estate Management']
        }

        # Industry characteristics
        self.industry_characteristics = {
            'Cyclical': ['Energy', 'Materials', 'Industrials', 'Consumer Discretionary'],
            'Defensive': ['Consumer Staples', 'Health Care', 'Utilities'],
            'Growth': ['Information Technology', 'Communication Services', 'Health Care'],
            'Value': ['Financials', 'Energy', 'Materials']
        }

    def classify_company(self, company_data: CompanyData) -> Dict[str, str]:
        """Classify company by various systems"""

        sector = company_data.sector
        industry = company_data.industry

        # GICS classification
        gics_sector = sector
        gics_industry = industry

        # Determine industry characteristics
        characteristics = []
        for char_type, sectors in self.industry_characteristics.items():
            if sector in sectors:
                characteristics.append(char_type)

        return {
            'gics_sector': gics_sector,
            'gics_industry': gics_industry,
            'industry_characteristics': characteristics,
            'business_cycle_sensitivity': self.determine_cycle_sensitivity(sector),
            'regulatory_intensity': self.determine_regulatory_intensity(sector)
        }

    def determine_cycle_sensitivity(self, sector: str) -> str:
        """Determine business cycle sensitivity"""
        high_sensitivity = ['Energy', 'Materials', 'Industrials', 'Consumer Discretionary', 'Financials']
        low_sensitivity = ['Consumer Staples', 'Health Care', 'Utilities']

        if sector in high_sensitivity:
            return "High"
        elif sector in low_sensitivity:
            return "Low"
        else:
            return "Medium"

    def determine_regulatory_intensity(self, sector: str) -> str:
        """Determine regulatory intensity"""
        high_regulation = ['Health Care', 'Financials', 'Utilities', 'Energy']
        medium_regulation = ['Information Technology', 'Communication Services']

        if sector in high_regulation:
            return "High"
        elif sector in medium_regulation:
            return "Medium"
        else:
            return "Low"


class IndustryAnalyzer(BaseCompanyAnalysisModel):
    """Comprehensive industry analysis framework"""

    def __init__(self):
        super().__init__("Industry Analyzer", "Comprehensive industry and competitive analysis")
        self.classifier = IndustryClassifier()

    def validate_inputs(self, **kwargs) -> bool:
        """Validate inputs for industry analysis"""
        company_data = kwargs.get('company_data')
        if not isinstance(company_data, CompanyData):
            raise ValidationError("Valid CompanyData object required")
        return True

    def analyze_company(self, company_data: CompanyData) -> Dict[str, Any]:
        """Comprehensive industry analysis for a company"""

        analysis = {
            'industry_overview': self.analyze_industry_overview(company_data),
            'industry_structure': self.analyze_industry_structure(company_data),
            'competitive_landscape': self.analyze_competitive_landscape(company_data),
            'porters_five_forces': self.perform_porters_analysis(company_data),
            'pestle_analysis': self.perform_pestle_analysis(company_data),
            'company_positioning': self.analyze_company_positioning(company_data),
            'industry_trends': self.identify_industry_trends(company_data),
            'investment_implications': self.assess_investment_implications(company_data)
        }

        return analysis

    def analyze_industry_overview(self, company_data: CompanyData) -> Dict[str, Any]:
        """Analyze industry overview and characteristics"""

        classification = self.classifier.classify_company(company_data)

        # Industry size estimation (simplified - would use actual data)
        market_cap = company_data.market_cap
        estimated_market_size = self.estimate_industry_size(company_data.sector, market_cap)

        # Growth characteristics
        growth_profile = self.determine_growth_profile(company_data.sector)

        return {
            'classification': classification,
            'industry_size': {
                'estimated_total_market': estimated_market_size,
                'company_market_share': self.estimate_market_share(market_cap, estimated_market_size),
                'size_category': self.categorize_industry_size(estimated_market_size)
            },
            'growth_characteristics': growth_profile,
            'industry_lifecycle': self.determine_industry_lifecycle(company_data),
            'key_success_factors': self.identify_success_factors(company_data.sector),
            'industry_risks': self.identify_industry_risks(company_data.sector)
        }

    def estimate_industry_size(self, sector: str, company_market_cap: float) -> float:
        """Estimate total industry market size"""

        # Simplified multipliers based on sector characteristics
        multipliers = {
            'Information Technology': 50,
            'Health Care': 40,
            'Financials': 30,
            'Consumer Discretionary': 35,
            'Consumer Staples': 25,
            'Industrials': 30,
            'Energy': 20,
            'Materials': 15,
            'Utilities': 10,
            'Communication Services': 25,
            'Real Estate': 20
        }

        multiplier = multipliers.get(sector, 25)
        return company_market_cap * multiplier

    def estimate_market_share(self, company_market_cap: float, industry_size: float) -> float:
        """Estimate company's market share"""
        if industry_size > 0:
            return (company_market_cap / industry_size) * 100
        return 0

    def categorize_industry_size(self, industry_size: float) -> str:
        """Categorize industry by size"""
        if industry_size > 1_000_000_000_000:  # $1T+
            return "Very Large"
        elif industry_size > 500_000_000_000:  # $500B+
            return "Large"
        elif industry_size > 100_000_000_000:  # $100B+
            return "Medium"
        else:
            return "Small"

    def determine_growth_profile(self, sector: str) -> Dict[str, Any]:
        """Determine industry growth profile"""

        # Historical growth patterns by sector (simplified)
        growth_profiles = {
            'Information Technology': {'historical_growth': 0.12, 'volatility': 'High', 'trend': 'Growing'},
            'Health Care': {'historical_growth': 0.08, 'volatility': 'Medium', 'trend': 'Growing'},
            'Consumer Discretionary': {'historical_growth': 0.06, 'volatility': 'High', 'trend': 'Cyclical'},
            'Financials': {'historical_growth': 0.05, 'volatility': 'High', 'trend': 'Cyclical'},
            'Consumer Staples': {'historical_growth': 0.04, 'volatility': 'Low', 'trend': 'Stable'},
            'Industrials': {'historical_growth': 0.05, 'volatility': 'Medium', 'trend': 'Cyclical'},
            'Energy': {'historical_growth': 0.02, 'volatility': 'Very High', 'trend': 'Declining'},
            'Materials': {'historical_growth': 0.03, 'volatility': 'High', 'trend': 'Cyclical'},
            'Utilities': {'historical_growth': 0.02, 'volatility': 'Low', 'trend': 'Stable'},
            'Communication Services': {'historical_growth': 0.07, 'volatility': 'Medium', 'trend': 'Growing'},
            'Real Estate': {'historical_growth': 0.04, 'volatility': 'Medium', 'trend': 'Cyclical'}
        }

        return growth_profiles.get(sector, {'historical_growth': 0.05, 'volatility': 'Medium', 'trend': 'Stable'})

    def determine_industry_lifecycle(self, company_data: CompanyData) -> str:
        """Determine industry lifecycle stage"""

        sector = company_data.sector
        growth_profile = self.determine_growth_profile(sector)

        growth_rate = growth_profile['historical_growth']

        if growth_rate > 0.10:
            return "Growth"
        elif growth_rate > 0.05:
            return "Mature"
        elif growth_rate > 0:
            return "Mature/Stable"
        else:
            return "Declining"

    def identify_success_factors(self, sector: str) -> List[str]:
        """Identify key success factors by sector"""

        success_factors = {
            'Information Technology': [
                'Innovation and R&D capability',
                'Talent acquisition and retention',
                'Scalable technology platforms',
                'Network effects',
                'Speed to market'
            ],
            'Health Care': [
                'R&D pipeline strength',
                'Regulatory approval capabilities',
                'Patent protection',
                'Clinical trial success rates',
                'Market access and distribution'
            ],
            'Financials': [
                'Risk management capabilities',
                'Regulatory compliance',
                'Technology infrastructure',
                'Customer relationships',
                'Capital adequacy'
            ],
            'Consumer Discretionary': [
                'Brand strength and recognition',
                'Distribution network',
                'Product innovation',
                'Supply chain efficiency',
                'Customer experience'
            ],
            'Energy': [
                'Reserve quality and quantity',
                'Operational efficiency',
                'Technology and innovation',
                'Environmental compliance',
                'Geographic diversification'
            ]
        }

        return success_factors.get(sector, [
            'Operational efficiency',
            'Market position',
            'Financial strength',
            'Innovation capability',
            'Customer relationships'
        ])

    def identify_industry_risks(self, sector: str) -> List[str]:
        """Identify key industry risks by sector"""

        industry_risks = {
            'Information Technology': [
                'Technological obsolescence',
                'Cybersecurity threats',
                'Regulatory changes',
                'Talent shortage',
                'Market saturation'
            ],
            'Health Care': [
                'Regulatory approval risks',
                'Patent cliff exposure',
                'Pricing pressures',
                'Clinical trial failures',
                'Regulatory changes'
            ],
            'Financials': [
                'Interest rate risk',
                'Credit risk',
                'Regulatory changes',
                'Economic cycles',
                'Technology disruption'
            ],
            'Energy': [
                'Commodity price volatility',
                'Environmental regulations',
                'Geopolitical risks',
                'Stranded asset risk',
                'Energy transition'
            ]
        }

        return industry_risks.get(sector, [
            'Economic cycles',
            'Competitive pressure',
            'Regulatory changes',
            'Technology disruption',
            'Supply chain disruption'
        ])

    def analyze_industry_structure(self, company_data: CompanyData) -> Dict[str, Any]:
        """Analyze industry structure and concentration"""

        # Market concentration analysis (simplified)
        market_cap = company_data.market_cap
        estimated_industry_size = self.estimate_industry_size(company_data.sector, market_cap)

        # Estimate concentration based on sector characteristics
        concentration_level = self.estimate_concentration(company_data.sector)

        # Barriers to entry
        entry_barriers = self.assess_entry_barriers(company_data.sector)

        # Industry profitability
        profitability_profile = self.assess_industry_profitability(company_data.sector)

        return {
            'market_concentration': {
                'concentration_level': concentration_level,
                'estimated_hhi': self.estimate_hhi(concentration_level),
                'market_structure': self.determine_market_structure(concentration_level)
            },
            'barriers_to_entry': entry_barriers,
            'profitability_profile': profitability_profile,
            'competitive_dynamics': self.assess_competitive_dynamics(company_data.sector),
            'industry_maturity': self.assess_industry_maturity(company_data.sector)
        }

    def estimate_concentration(self, sector: str) -> str:
        """Estimate industry concentration level"""

        high_concentration = ['Utilities', 'Communication Services', 'Aerospace & Defense']
        medium_concentration = ['Energy', 'Materials', 'Industrials', 'Health Care']
        low_concentration = ['Information Technology', 'Consumer Discretionary', 'Financials']

        if sector in high_concentration:
            return "High"
        elif sector in medium_concentration:
            return "Medium"
        else:
            return "Low"

    def estimate_hhi(self, concentration_level: str) -> int:
        """Estimate Herfindahl-Hirschman Index"""

        hhi_ranges = {
            'High': 2000,
            'Medium': 1200,
            'Low': 800
        }

        return hhi_ranges.get(concentration_level, 1000)

    def determine_market_structure(self, concentration_level: str) -> str:
        """Determine market structure type"""

        if concentration_level == "High":
            return "Oligopoly"
        elif concentration_level == "Medium":
            return "Monopolistic Competition"
        else:
            return "Perfect Competition"

    def assess_entry_barriers(self, sector: str) -> Dict[str, str]:
        """Assess barriers to entry"""

        barrier_assessments = {
            'Information Technology': {
                'capital_requirements': 'Medium',
                'regulatory_barriers': 'Low',
                'technology_barriers': 'High',
                'brand_loyalty': 'Medium',
                'network_effects': 'High'
            },
            'Health Care': {
                'capital_requirements': 'Very High',
                'regulatory_barriers': 'Very High',
                'technology_barriers': 'High',
                'brand_loyalty': 'High',
                'network_effects': 'Low'
            },
            'Utilities': {
                'capital_requirements': 'Very High',
                'regulatory_barriers': 'Very High',
                'technology_barriers': 'Medium',
                'brand_loyalty': 'Low',
                'network_effects': 'High'
            },
            'Financials': {
                'capital_requirements': 'Very High',
                'regulatory_barriers': 'Very High',
                'technology_barriers': 'Medium',
                'brand_loyalty': 'Medium',
                'network_effects': 'Medium'
            }
        }

        return barrier_assessments.get(sector, {
            'capital_requirements': 'Medium',
            'regulatory_barriers': 'Medium',
            'technology_barriers': 'Medium',
            'brand_loyalty': 'Medium',
            'network_effects': 'Low'
        })

    def assess_industry_profitability(self, sector: str) -> Dict[str, Any]:
        """Assess industry profitability characteristics"""

        # Historical average margins by sector (simplified)
        profitability_data = {
            'Information Technology': {'avg_margin': 0.15, 'margin_stability': 'Medium', 'trend': 'Stable'},
            'Health Care': {'avg_margin': 0.12, 'margin_stability': 'High', 'trend': 'Declining'},
            'Financials': {'avg_margin': 0.20, 'margin_stability': 'Low', 'trend': 'Cyclical'},
            'Consumer Staples': {'avg_margin': 0.08, 'margin_stability': 'High', 'trend': 'Stable'},
            'Energy': {'avg_margin': 0.05, 'margin_stability': 'Very Low', 'trend': 'Volatile'},
            'Utilities': {'avg_margin': 0.10, 'margin_stability': 'High', 'trend': 'Stable'}
        }

        return profitability_data.get(sector, {
            'avg_margin': 0.08,
            'margin_stability': 'Medium',
            'trend': 'Stable'
        })

    def assess_competitive_dynamics(self, sector: str) -> Dict[str, str]:
        """Assess competitive dynamics"""

        dynamics = {
            'Information Technology': {
                'intensity': 'Very High',
                'basis': 'Innovation and Speed',
                'pricing_power': 'Medium',
                'differentiation': 'High'
            },
            'Health Care': {
                'intensity': 'High',
                'basis': 'Innovation and Quality',
                'pricing_power': 'High',
                'differentiation': 'Very High'
            },
            'Utilities': {
                'intensity': 'Low',
                'basis': 'Regulation and Service',
                'pricing_power': 'Low',
                'differentiation': 'Low'
            },
            'Energy': {
                'intensity': 'High',
                'basis': 'Cost and Efficiency',
                'pricing_power': 'Low',
                'differentiation': 'Low'
            }
        }

        return dynamics.get(sector, {
            'intensity': 'Medium',
            'basis': 'Price and Quality',
            'pricing_power': 'Medium',
            'differentiation': 'Medium'
        })

    def assess_industry_maturity(self, sector: str) -> str:
        """Assess industry maturity level"""

        mature_industries = ['Utilities', 'Consumer Staples', 'Energy', 'Materials']
        growth_industries = ['Information Technology', 'Health Care', 'Communication Services']

        if sector in mature_industries:
            return "Mature"
        elif sector in growth_industries:
            return "Growth"
        else:
            return "Transitional"


class PortersFiveForcesAnalyzer:
    """Porter's Five Forces analysis framework"""

    def analyze_five_forces(self, company_data: CompanyData) -> PortersFiveForcesAnalysis:
        """Perform comprehensive Porter's Five Forces analysis"""

        sector = company_data.sector

        threat_new_entrants = self.analyze_threat_of_new_entrants(sector)
        supplier_power = self.analyze_supplier_power(sector)
        buyer_power = self.analyze_buyer_power(sector)
        threat_substitutes = self.analyze_threat_of_substitutes(sector)
        competitive_rivalry = self.analyze_competitive_rivalry(sector)

        # Overall industry attractiveness
        forces_scores = [
            threat_new_entrants['threat_level_score'],
            supplier_power['power_level_score'],
            buyer_power['power_level_score'],
            threat_substitutes['threat_level_score'],
            competitive_rivalry['intensity_score']
        ]

        avg_score = np.mean(forces_scores)

        if avg_score <= 2:
            attractiveness = "Highly Attractive"
        elif avg_score <= 3:
            attractiveness = "Moderately Attractive"
        elif avg_score <= 4:
            attractiveness = "Average Attractiveness"
        else:
            attractiveness = "Unattractive"

        return PortersFiveForcesAnalysis(
            threat_of_new_entrants=threat_new_entrants,
            bargaining_power_suppliers=supplier_power,
            bargaining_power_buyers=buyer_power,
            threat_of_substitutes=threat_substitutes,
            competitive_rivalry=competitive_rivalry,
            overall_attractiveness=attractiveness
        )

    def analyze_threat_of_new_entrants(self, sector: str) -> Dict[str, Any]:
        """Analyze threat of new entrants"""

        # Sector-specific entry barrier analysis
        entry_barriers = {
            'Information Technology': {
                'capital_requirements': 3,  # 1-5 scale (1=low barrier, 5=high barrier)
                'technology_complexity': 4,
                'regulatory_requirements': 2,
                'brand_loyalty': 3,
                'network_effects': 4,
                'access_to_distribution': 3
            },
            'Health Care': {
                'capital_requirements': 5,
                'technology_complexity': 5,
                'regulatory_requirements': 5,
                'brand_loyalty': 4,
                'network_effects': 2,
                'access_to_distribution': 4
            },
            'Utilities': {
                'capital_requirements': 5,
                'technology_complexity': 3,
                'regulatory_requirements': 5,
                'brand_loyalty': 2,
                'network_effects': 5,
                'access_to_distribution': 5
            }
        }

        barriers = entry_barriers.get(sector, {
            'capital_requirements': 3,
            'technology_complexity': 3,
            'regulatory_requirements': 3,
            'brand_loyalty': 3,
            'network_effects': 3,
            'access_to_distribution': 3
        })

        avg_barrier_strength = np.mean(list(barriers.values()))
        threat_level_score = 6 - avg_barrier_strength  # Inverse relationship

        if threat_level_score <= 2:
            threat_level = "Low"
        elif threat_level_score <= 3:
            threat_level = "Medium"
        else:
            threat_level = "High"

        return {
            'entry_barriers': barriers,
            'threat_level': threat_level,
            'threat_level_score': threat_level_score,
            'key_barriers': [k for k, v in barriers.items() if v >= 4],
            'analysis': f"Entry barriers are {'strong' if avg_barrier_strength >= 4 else 'moderate' if avg_barrier_strength >= 3 else 'weak'}"
        }

    def analyze_supplier_power(self, sector: str) -> Dict[str, Any]:
        """Analyze bargaining power of suppliers"""

        supplier_power_factors = {
            'Information Technology': {
                'supplier_concentration': 3,
                'switching_costs': 3,
                'input_importance': 4,
                'substitute_inputs': 3,
                'forward_integration_threat': 2
            },
            'Health Care': {
                'supplier_concentration': 4,
                'switching_costs': 4,
                'input_importance': 5,
                'substitute_inputs': 2,
                'forward_integration_threat': 2
            },
            'Automotive': {
                'supplier_concentration': 4,
                'switching_costs': 4,
                'input_importance': 4,
                'substitute_inputs': 3,
                'forward_integration_threat': 3
            }
        }

        factors = supplier_power_factors.get(sector, {
            'supplier_concentration': 3,
            'switching_costs': 3,
            'input_importance': 3,
            'substitute_inputs': 3,
            'forward_integration_threat': 3
        })

        avg_power = np.mean(list(factors.values()))

        if avg_power >= 4:
            power_level = "High"
        elif avg_power >= 3:
            power_level = "Medium"
        else:
            power_level = "Low"

        return {
            'power_factors': factors,
            'power_level': power_level,
            'power_level_score': avg_power,
            'key_factors': [k for k, v in factors.items() if v >= 4],
            'analysis': f"Supplier power is {power_level.lower()} due to {', '.join([k.replace('_', ' ') for k, v in factors.items() if v >= 4])}"
        }

    def analyze_buyer_power(self, sector: str) -> Dict[str, Any]:
        """Analyze bargaining power of buyers"""

        buyer_power_factors = {
            'Information Technology': {
                'buyer_concentration': 3,
                'switching_costs': 2,
                'product_importance': 4,
                'substitute_products': 3,
                'backward_integration_threat': 2,
                'price_sensitivity': 3
            },
            'Health Care': {
                'buyer_concentration': 4,
                'switching_costs': 4,
                'product_importance': 5,
                'substitute_products': 2,
                'backward_integration_threat': 1,
                'price_sensitivity': 4
            },
            'Retail': {
                'buyer_concentration': 2,
                'switching_costs': 1,
                'product_importance': 2,
                'substitute_products': 4,
                'backward_integration_threat': 2,
                'price_sensitivity': 5
            }
        }

        factors = buyer_power_factors.get(sector, {
            'buyer_concentration': 3,
            'switching_costs': 3,
            'product_importance': 3,
            'substitute_products': 3,
            'backward_integration_threat': 3,
            'price_sensitivity': 3
        })

        # Adjust for factors that reduce buyer power
        power_reducing_factors = ['switching_costs', 'product_importance']
        for factor in power_reducing_factors:
            if factor in factors:
                factors[factor] = 6 - factors[factor]  # Invert scale

        avg_power = np.mean(list(factors.values()))

        if avg_power >= 4:
            power_level = "High"
        elif avg_power >= 3:
            power_level = "Medium"
        else:
            power_level = "Low"

        return {
            'power_factors': buyer_power_factors.get(sector, factors),
            'power_level': power_level,
            'power_level_score': avg_power,
            'key_factors': [k for k, v in factors.items() if v >= 4],
            'analysis': f"Buyer power is {power_level.lower()}"
        }

    def analyze_threat_of_substitutes(self, sector: str) -> Dict[str, Any]:
        """Analyze threat of substitute products"""

        substitute_factors = {
            'Information Technology': {
                'substitute_availability': 4,
                'switching_costs': 3,
                'substitute_performance': 3,
                'price_performance': 3
            },
            'Energy': {
                'substitute_availability': 4,
                'switching_costs': 4,
                'substitute_performance': 3,
                'price_performance': 4
            },
            'Transportation': {
                'substitute_availability': 3,
                'switching_costs': 3,
                'substitute_performance': 3,
                'price_performance': 3
            }
        }

        factors = substitute_factors.get(sector, {
            'substitute_availability': 3,
            'switching_costs': 3,
            'substitute_performance': 3,
            'price_performance': 3
        })

        # Switching costs reduce threat
        factors['switching_costs'] = 6 - factors['switching_costs']

        avg_threat = np.mean(list(factors.values()))

        if avg_threat >= 4:
            threat_level = "High"
        elif avg_threat >= 3:
            threat_level = "Medium"
        else:
            threat_level = "Low"

        return {
            'substitute_factors': substitute_factors.get(sector, factors),
            'threat_level': threat_level,
            'threat_level_score': avg_threat,
            'key_factors': [k for k, v in factors.items() if v >= 4],
            'analysis': f"Substitute threat is {threat_level.lower()}"
        }

    def analyze_competitive_rivalry(self, sector: str) -> Dict[str, Any]:
        """Analyze intensity of competitive rivalry"""

        rivalry_factors = {
            'Information Technology': {
                'number_of_competitors': 5,
                'industry_growth': 2,  # High growth reduces rivalry
                'product_differentiation': 3,
                'switching_costs': 2,
                'exit_barriers': 3,
                'fixed_costs': 3
            },
            'Airlines': {
                'number_of_competitors': 4,
                'industry_growth': 3,
                'product_differentiation': 2,
                'switching_costs': 1,
                'exit_barriers': 5,
                'fixed_costs': 5
            },
            'Utilities': {
                'number_of_competitors': 2,
                'industry_growth': 2,
                'product_differentiation': 1,
                'switching_costs': 4,
                'exit_barriers': 5,
                'fixed_costs': 5
            }
        }

        factors = rivalry_factors.get(sector, {
            'number_of_competitors': 4,
            'industry_growth': 3,
            'product_differentiation': 3,
            'switching_costs': 3,
            'exit_barriers': 3,
            'fixed_costs': 3
        })

        # Factors that reduce rivalry (invert scale)
        rivalry_reducing = ['industry_growth', 'product_differentiation', 'switching_costs']
        for factor in rivalry_reducing:
            if factor in factors:
                factors[factor] = 6 - factors[factor]

        avg_intensity = np.mean(list(factors.values()))

        if avg_intensity >= 4:
            intensity = "Very High"
        elif avg_intensity >= 3.5:
            intensity = "High"
        elif avg_intensity >= 2.5:
            intensity = "Medium"
        else:
            intensity = "Low"

        return {
            'rivalry_factors': rivalry_factors.get(sector, factors),
            'intensity': intensity,
            'intensity_score': avg_intensity,
            'key_factors': [k for k, v in factors.items() if v >= 4],
            'analysis': f"Competitive rivalry is {intensity.lower()}"
        }


class PESTLEAnalyzer:
    """PESTLE analysis framework"""

    def perform_pestle_analysis(self, company_data: CompanyData) -> PESTLEAnalysis:
        """Perform PESTLE analysis for industry"""

        sector = company_data.sector

        political_factors = self.analyze_political_factors(sector)
        economic_factors = self.analyze_economic_factors(sector)
        social_factors = self.analyze_social_factors(sector)
        technological_factors = self.analyze_technological_factors(sector)
        legal_factors = self.analyze_legal_factors(sector)
        environmental_factors = self.analyze_environmental_factors(sector)

        # Overall impact assessment
        factor_impacts = [
            len([f for f in political_factors if 'positive' not in f.lower()]),
            len([f for f in economic_factors if 'positive' not in f.lower()]),
            len([f for f in social_factors if 'positive' not in f.lower()]),
            len([f for f in technological_factors if 'threat' in f.lower() or 'disruption' in f.lower()]),
            len([f for f in legal_factors if 'risk' in f.lower() or 'compliance' in f.lower()]),
            len([f for f in environmental_factors if 'regulation' in f.lower() or 'pressure' in f.lower()])
        ]

        total_negative_factors = sum(factor_impacts)

        if total_negative_factors <= 5:
            overall_impact = "Favorable"
        elif total_negative_factors <= 10:
            overall_impact = "Mixed"
        else:
            overall_impact = "Challenging"

        return PESTLEAnalysis(
            political_factors=political_factors,
            economic_factors=economic_factors,
            social_factors=social_factors,
            technological_factors=technological_factors,
            legal_factors=legal_factors,
            environmental_factors=environmental_factors,
            overall_impact=overall_impact
        )

    def analyze_political_factors(self, sector: str) -> List[str]:
        """Analyze political factors affecting the industry"""

        political_factors_by_sector = {
            'Health Care': [
                'Healthcare policy changes',
                'Drug pricing regulations',
                'FDA approval processes',
                'International trade policies',
                'Government healthcare spending'
            ],
            'Energy': [
                'Energy policy and subsidies',
                'Environmental regulations',
                'Geopolitical stability',
                'Carbon pricing policies',
                'Infrastructure investment'
            ],
            'Financials': [
                'Banking regulations',
                'Monetary policy',
                'Tax policy changes',
                'International sanctions',
                'Political stability'
            ],
            'Information Technology': [
                'Data privacy regulations',
                'Antitrust scrutiny',
                'International trade tensions',
                'Cybersecurity policies',
                'Government technology spending'
            ]
        }

        return political_factors_by_sector.get(sector, [
            'Regulatory changes',
            'Tax policy shifts',
            'Government spending',
            'Political stability',
            'International relations'
        ])

    def analyze_economic_factors(self, sector: str) -> List[str]:
        """Analyze economic factors affecting the industry"""

        economic_factors_by_sector = {
            'Consumer Discretionary': [
                'Consumer spending trends',
                'Employment levels',
                'Disposable income changes',
                'Interest rate environment',
                'Economic growth rates'
            ],
            'Financials': [
                'Interest rate cycles',
                'Credit market conditions',
                'Economic growth outlook',
                'Inflation expectations',
                'Currency exchange rates'
            ],
            'Real Estate': [
                'Interest rate environment',
                'Economic growth trends',
                'Employment levels',
                'Population demographics',
                'Government housing policies'
            ],
            'Energy': [
                'Commodity price cycles',
                'Global economic growth',
                'Currency fluctuations',
                'Supply-demand dynamics',
                'Energy transition economics'
            ]
        }

        return economic_factors_by_sector.get(sector, [
            'Economic growth rates',
            'Interest rate environment',
            'Inflation trends',
            'Employment levels',
            'Consumer confidence'
        ])

    def analyze_social_factors(self, sector: str) -> List[str]:
        """Analyze social factors affecting the industry"""

        social_factors_by_sector = {
            'Consumer Staples': [
                'Health and wellness trends',
                'Demographic shifts',
                'Lifestyle changes',
                'Cultural preferences',
                'Social responsibility expectations'
            ],
            'Health Care': [
                'Aging population trends',
                'Health awareness levels',
                'Lifestyle disease prevalence',
                'Healthcare access expectations',
                'Social healthcare attitudes'
            ],
            'Information Technology': [
                'Digital adoption rates',
                'Remote work trends',
                'Privacy concerns',
                'Social media usage',
                'Educational technology needs'
            ],
            'Automotive': [
                'Transportation preferences',
                'Environmental consciousness',
                'Urbanization trends',
                'Mobility service adoption',
                'Safety awareness'
            ]
        }

        return social_factors_by_sector.get(sector, [
            'Demographic changes',
            'Cultural shifts',
            'Lifestyle trends',
            'Social values evolution',
            'Consumer behavior changes'
        ])

    def analyze_technological_factors(self, sector: str) -> List[str]:
        """Analyze technological factors affecting the industry"""

        tech_factors_by_sector = {
            'Information Technology': [
                'AI and machine learning advancement',
                'Cloud computing evolution',
                'Cybersecurity developments',
                'Quantum computing potential',
                'IoT expansion'
            ],
            'Health Care': [
                'Biotechnology advances',
                'Digital health solutions',
                'Precision medicine development',
                'Medical device innovation',
                'Telemedicine growth'
            ],
            'Automotive': [
                'Electric vehicle technology',
                'Autonomous driving development',
                'Battery technology advances',
                'Connected car features',
                'Manufacturing automation'
            ],
            'Financial Services': [
                'Fintech innovation',
                'Blockchain technology',
                'Digital payment systems',
                'Robo-advisory platforms',
                'Regulatory technology'
            ]
        }

        return tech_factors_by_sector.get(sector, [
            'Automation advances',
            'Digital transformation',
            'Innovation pace',
            'Technology disruption threats',
            'R&D developments'
        ])

    def analyze_legal_factors(self, sector: str) -> List[str]:
        """Analyze legal factors affecting the industry"""

        legal_factors_by_sector = {
            'Health Care': [
                'FDA regulatory requirements',
                'Patent law changes',
                'Healthcare compliance',
                'International drug regulations',
                'Liability and malpractice laws'
            ],
            'Financials': [
                'Banking compliance requirements',
                'Consumer protection laws',
                'Anti-money laundering regulations',
                'International banking standards',
                'Data protection compliance'
            ],
            'Energy': [
                'Environmental compliance',
                'Safety regulations',
                'Land use rights',
                'International energy agreements',
                'Carbon emission regulations'
            ],
            'Information Technology': [
                'Data privacy laws',
                'Intellectual property protection',
                'Antitrust regulations',
                'International data transfer rules',
                'Cybersecurity compliance'
            ]
        }

        return legal_factors_by_sector.get(sector, [
            'Regulatory compliance requirements',
            'Industry-specific laws',
            'International regulations',
            'Intellectual property protection',
            'Employment law changes'
        ])

    def analyze_environmental_factors(self, sector: str) -> List[str]:
        """Analyze environmental factors affecting the industry"""

        environmental_factors_by_sector = {
            'Energy': [
                'Climate change policies',
                'Carbon emission regulations',
                'Renewable energy transition',
                'Environmental impact assessments',
                'Sustainability reporting requirements'
            ],
            'Materials': [
                'Resource scarcity concerns',
                'Recycling and waste management',
                'Environmental regulations',
                'Sustainable sourcing requirements',
                'Carbon footprint pressures'
            ],
            'Automotive': [
                'Emission standards',
                'Electric vehicle mandates',
                'Fuel efficiency requirements',
                'End-of-life vehicle regulations',
                'Sustainable manufacturing'
            ],
            'Consumer Staples': [
                'Sustainable packaging requirements',
                'Organic and natural trends',
                'Water usage regulations',
                'Supply chain sustainability',
                'Carbon footprint disclosure'
            ]
        }

        return environmental_factors_by_sector.get(sector, [
            'Environmental regulations',
            'Climate change impact',
            'Sustainability requirements',
            'Resource availability',
            'Waste management regulations'
        ])


class CompetitivePositionAnalyzer:
    """Analyze company's competitive position"""

    def analyze_competitive_position(self, company_data: CompanyData,
                                     industry_metrics: IndustryMetrics = None) -> Dict[str, Any]:
        """Analyze company's competitive position in industry"""

        financial_data = company_data.financial_data
        market_data = company_data.market_data

        # Market position metrics
        market_position = self.assess_market_position(company_data, industry_metrics)

        # Financial competitive metrics
        financial_position = self.assess_financial_position(company_data, industry_metrics)

        # Strategic position
        strategic_position = self.assess_strategic_position(company_data)

        # Overall competitive position
        position_score = self.calculate_position_score(market_position, financial_position, strategic_position)
        competitive_position = self.determine_competitive_position(position_score)

        return {
            'market_position': market_position,
            'financial_position': financial_position,
            'strategic_position': strategic_position,
            'overall_position': competitive_position,
            'position_score': position_score,
            'competitive_advantages': self.identify_competitive_advantages(company_data),
            'competitive_disadvantages': self.identify_competitive_disadvantages(company_data),
            'strategic_recommendations': self.generate_strategic_recommendations(company_data, competitive_position)
        }

    def assess_market_position(self, company_data: CompanyData,
                               industry_metrics: IndustryMetrics = None) -> Dict[str, Any]:
        """Assess market position"""

        market_cap = company_data.market_cap

        # Estimate market share (simplified)
        if industry_metrics:
            estimated_market_share = (market_cap / industry_metrics.total_market_size) * 100
        else:
            # Use sector-based estimation
            sector_multiplier = {
                'Information Technology': 50,
                'Health Care': 40,
                'Financials': 30
            }.get(company_data.sector, 35)

            estimated_industry_size = market_cap * sector_multiplier
            estimated_market_share = (market_cap / estimated_industry_size) * 100

        # Market position assessment
        if estimated_market_share > 20:
            market_position_category = "Market Leader"
            position_score = 5
        elif estimated_market_share > 10:
            market_position_category = "Major Player"
            position_score = 4
        elif estimated_market_share > 5:
            market_position_category = "Significant Player"
            position_score = 3
        elif estimated_market_share > 1:
            market_position_category = "Niche Player"
            position_score = 2
        else:
            market_position_category = "Small Player"
            position_score = 1

        return {
            'estimated_market_share': estimated_market_share,
            'market_position_category': market_position_category,
            'position_score': position_score,
            'market_cap_ranking': self.estimate_market_cap_ranking(company_data),
            'brand_strength': self.assess_brand_strength(company_data)
        }

    def assess_financial_position(self, company_data: CompanyData,
                                  industry_metrics: IndustryMetrics = None) -> Dict[str, Any]:
        """Assess financial competitive position"""

        financial_data = company_data.financial_data

        # Get industry averages
        if industry_metrics:
            industry_roe = industry_metrics.average_roe
            industry_margin = industry_metrics.average_margin
        else:
            # Default industry averages
            industry_roe = 0.10
            industry_margin = 0.08

        company_roe = financial_data.get('roe', 0)
        company_margin = financial_data.get('net_margin', 0)

        # Financial strength score
        financial_score = 0

        # ROE comparison
        if company_roe > industry_roe * 1.5:
            financial_score += 2
        elif company_roe > industry_roe:
            financial_score += 1
        elif company_roe < industry_roe * 0.5:
            financial_score -= 1

        # Margin comparison
        if company_margin > industry_margin * 1.5:
            financial_score += 2
        elif company_margin > industry_margin:
            financial_score += 1
        elif company_margin < industry_margin * 0.5:
            financial_score -= 1

        # Debt position
        debt_to_equity = financial_data.get('debt_to_equity', 0)
        if debt_to_equity < 0.3:
            financial_score += 1
        elif debt_to_equity > 1.5:
            financial_score -= 1

        return {
            'financial_strength_score': financial_score,
            'roe_vs_industry': company_roe / industry_roe if industry_roe > 0 else 1,
            'margin_vs_industry': company_margin / industry_margin if industry_margin > 0 else 1,
            'debt_position': 'Conservative' if debt_to_equity < 0.5 else 'Aggressive' if debt_to_equity > 1.5 else 'Moderate',
            'cash_position': self.assess_cash_position(company_data)
        }

    def assess_strategic_position(self, company_data: CompanyData) -> Dict[str, Any]:
        """Assess strategic competitive position"""

        # Innovation assessment (proxy metrics)
        market_data = company_data.market_data
        pb_ratio = market_data.get('pb_ratio', 0)

        # Innovation score based on market valuation premium
        if pb_ratio > 3:
            innovation_score = 3
        elif pb_ratio > 2:
            innovation_score = 2
        elif pb_ratio > 1:
            innovation_score = 1
        else:
            innovation_score = 0

        # Scale advantages
        market_cap = company_data.market_cap
        if market_cap > 50_000_000_000:  # $50B+
            scale_advantage = 3
        elif market_cap > 10_000_000_000:  # $10B+
            scale_advantage = 2
        elif market_cap > 1_000_000_000:  # $1B+
            scale_advantage = 1
        else:
            scale_advantage = 0

        return {
            'innovation_score': innovation_score,
            'scale_advantage': scale_advantage,
            'diversification': self.assess_diversification(company_data),
            'operational_efficiency': self.assess_operational_efficiency(company_data),
            'strategic_focus': self.assess_strategic_focus(company_data)
        }

    def calculate_position_score(self, market_position: Dict, financial_position: Dict,
                                 strategic_position: Dict) -> float:
        """Calculate overall competitive position score"""

        market_score = market_position['position_score']
        financial_score = max(0, financial_position['financial_strength_score'] + 3)  # Normalize to 0-5
        strategic_score = (strategic_position['innovation_score'] +
                           strategic_position['scale_advantage']) / 2

        # Weighted average
        total_score = (market_score * 0.4 + financial_score * 0.4 + strategic_score * 0.2)
        return total_score

    def determine_competitive_position(self, position_score: float) -> CompetitivePosition:
        """Determine competitive position category"""

        if position_score >= 4:
            return CompetitivePosition.MARKET_LEADER
        elif position_score >= 3:
            return CompetitivePosition.STRONG_COMPETITOR
        elif position_score >= 2:
            return CompetitivePosition.NICHE_PLAYER
        else:
            return CompetitivePosition.STRUGGLING_COMPETITOR

    def estimate_market_cap_ranking(self, company_data: CompanyData) -> str:
        """Estimate market cap ranking within sector"""

        market_cap = company_data.market_cap

        if market_cap > 100_000_000_000:  # $100B+
            return "Top 5"
        elif market_cap > 50_000_000_000:  # $50B+
            return "Top 10"
        elif market_cap > 10_000_000_000:  # $10B+
            return "Top 25"
        elif market_cap > 1_000_000_000:  # $1B+
            return "Top 100"
        else:
            return "Outside Top 100"

    def assess_brand_strength(self, company_data: CompanyData) -> str:
        """Assess brand strength"""

        market_data = company_data.market_data
        pb_ratio = market_data.get('pb_ratio', 0)

        # Use P/B as proxy for brand/intangible value
        if pb_ratio > 5:
            return "Very Strong"
        elif pb_ratio > 3:
            return "Strong"
        elif pb_ratio > 1.5:
            return "Moderate"
        else:
            return "Weak"

    def assess_cash_position(self, company_data: CompanyData) -> str:
        """Assess cash position strength"""

        financial_data = company_data.financial_data
        current_ratio = financial_data.get('current_ratio', 0)

        if current_ratio > 2.5:
            return "Very Strong"
        elif current_ratio > 1.5:
            return "Strong"
        elif current_ratio > 1.0:
            return "Adequate"
        else:
            return "Weak"

    def assess_diversification(self, company_data: CompanyData) -> str:
        """Assess business diversification"""

        # Simplified assessment based on company size
        # Larger companies typically more diversified
        market_cap = company_data.market_cap

        if market_cap > 50_000_000_000:
            return "Highly Diversified"
        elif market_cap > 10_000_000_000:
            return "Moderately Diversified"
        else:
            return "Focused"

    def assess_operational_efficiency(self, company_data: CompanyData) -> str:
        """Assess operational efficiency"""

        financial_data = company_data.financial_data
        asset_turnover = financial_data.get('revenue', 0) / financial_data.get('total_assets', 1)

        if asset_turnover > 1.5:
            return "High"
        elif asset_turnover > 1.0:
            return "Average"
        else:
            return "Low"

    def assess_strategic_focus(self, company_data: CompanyData) -> str:
        """Assess strategic focus"""

        # Simplified assessment
        return "Focused"  # Would require more detailed business analysis

    def identify_competitive_advantages(self, company_data: CompanyData) -> List[str]:
        """Identify key competitive advantages"""

        advantages = []
        financial_data = company_data.financial_data
        market_data = company_data.market_data

        # Financial advantages
        if financial_data.get('roe', 0) > 0.15:
            advantages.append("Strong profitability")

        if financial_data.get('debt_to_equity', 0) < 0.3:
            advantages.append("Strong balance sheet")

        # Market advantages
        if company_data.market_cap > 10_000_000_000:
            advantages.append("Scale advantages")

        if market_data.get('pb_ratio', 0) > 3:
            advantages.append("Strong brand/intangibles")

        # Operational advantages
        asset_turnover = financial_data.get('revenue', 0) / financial_data.get('total_assets', 1)
        if asset_turnover > 1.5:
            advantages.append("Operational efficiency")

        return advantages if advantages else ["No clear competitive advantages identified"]

    def identify_competitive_disadvantages(self, company_data: CompanyData) -> List[str]:
        """Identify competitive disadvantages"""

        disadvantages = []
        financial_data = company_data.financial_data

        if financial_data.get('roe', 0) < 0:
            disadvantages.append("Poor profitability")

        if financial_data.get('debt_to_equity', 0) > 2:
            disadvantages.append("High leverage")

        if financial_data.get('current_ratio', 0) < 1:
            disadvantages.append("Liquidity constraints")

        if company_data.market_cap < 1_000_000_000:
            disadvantages.append("Limited scale")

        return disadvantages if disadvantages else ["No significant competitive disadvantages identified"]

    def generate_strategic_recommendations(self, company_data: CompanyData,
                                           position: CompetitivePosition) -> List[str]:
        """Generate strategic recommendations based on competitive position"""

        recommendations = []

        if position == CompetitivePosition.MARKET_LEADER:
            recommendations = [
                "Maintain market leadership through innovation",
                "Consider strategic acquisitions for growth",
                "Invest in emerging markets or technologies",
                "Optimize operational efficiency"
            ]

        elif position == CompetitivePosition.STRONG_COMPETITOR:
            recommendations = [
                "Focus on differentiation strategies",
                "Identify niche market opportunities",
                "Strengthen core competencies",
                "Consider strategic partnerships"
            ]

        elif position == CompetitivePosition.NICHE_PLAYER:
            recommendations = [
                "Defend niche market position",
                "Explore adjacent market opportunities",
                "Build strategic alliances",
                "Focus on customer loyalty"
            ]

        else:  # STRUGGLING_COMPETITOR
            recommendations = [
                "Restructure operations for efficiency",
                "Consider strategic alternatives",
                "Focus on core profitable segments",
                "Improve financial position"
            ]

        return recommendations


# Convenience functions
def quick_industry_analysis(company_data: CompanyData) -> Dict[str, Any]:
    """Quick industry analysis"""
    analyzer = IndustryAnalyzer()
    return analyzer.analyze_company(company_data)


def porters_five_forces(company_data: CompanyData) -> PortersFiveForcesAnalysis:
    """Quick Porter's Five Forces analysis"""
    analyzer = PortersFiveForcesAnalyzer()
    return analyzer.analyze_five_forces(company_data)


def pestle_analysis(company_data: CompanyData) -> PESTLEAnalysis:
    """Quick PESTLE analysis"""
    analyzer = PESTLEAnalyzer()
    return analyzer.perform_pestle_analysis(company_data)


def competitive_position_analysis(company_data: CompanyData) -> Dict[str, Any]:
    """Quick competitive position analysis"""
    analyzer = CompetitivePositionAnalyzer()
    return analyzer.analyze_competitive_position(company_data)