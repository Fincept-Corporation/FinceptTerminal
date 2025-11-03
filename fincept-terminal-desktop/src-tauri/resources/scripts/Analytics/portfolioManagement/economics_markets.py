
"""Portfolio Economics Markets Module
===============================

Economic analysis for portfolio management

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings and transaction history
  - Asset price data and market returns
  - Benchmark indices and market data
  - Investment policy statements and constraints
  - Risk tolerance and preference parameters

OUTPUT:
  - Portfolio performance metrics and attribution
  - Risk analysis and diversification metrics
  - Rebalancing recommendations and optimization
  - Portfolio analytics reports and visualizations
  - Investment strategy recommendations

PARAMETERS:
  - optimization_method: Portfolio optimization method (default: 'mean_variance')
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - rebalance_frequency: Portfolio rebalancing frequency (default: 'quarterly')
  - max_weight: Maximum single asset weight (default: 0.10)
  - benchmark: Portfolio benchmark index (default: 'market_index')
"""



import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Union, Tuple
from enum import Enum

from config import MathConstants

class BusinessCyclePhase(Enum):
    EXPANSION = "expansion"
    PEAK = "peak"
    CONTRACTION = "contraction"
    TROUGH = "trough"

class MarketValuationFactors:
    """Economic factors affecting market values"""

    @staticmethod
    def analyze_valuation_drivers() -> Dict:
        """Analyze three key drivers of market valuation"""
        return {
            "default_free_rates": {
                "description": "Risk-free interest rates across maturities affect discount rates",
                "impact_mechanism": "Higher rates reduce present value of future cash flows",
                "yield_curve_importance": "Term structure affects valuation of different duration assets",
                "policy_influence": "Central bank policy directly impacts short-term rates"
            },
            "cash_flow_expectations": {
                "description": "Expected timing and magnitude of cash flows",
                "earnings_growth": "Corporate earnings growth drives equity valuations",
                "dividend_expectations": "Dividend growth affects equity value",
                "credit_quality": "Default expectations affect bond valuations"
            },
            "risk_premiums": {
                "description": "Additional return required for bearing risk",
                "equity_risk_premium": "Excess return demanded for equity over risk-free rate",
                "credit_spreads": "Additional yield for credit risk",
                "liquidity_premium": "Compensation for illiquidity"
            }
        }

    @staticmethod
    def expectations_impact_analysis() -> Dict:
        """Analyze role of expectations in market valuation"""
        return {
            "expectations_formation": {
                "rational_expectations": "Based on all available information",
                "adaptive_expectations": "Based on historical patterns",
                "behavioral_factors": "Psychological biases affect expectations"
            },
            "expectation_changes": {
                "surprise_announcements": "Unexpected news causes immediate price adjustments",
                "gradual_revision": "Slow information incorporation causes trend following",
                "regime_changes": "Structural changes require expectation resets"
            },
            "market_efficiency": {
                "efficient_market": "Prices reflect all available information",
                "semi_strong_form": "Public information quickly incorporated",
                "behavioral_deviations": "Systematic biases create opportunities"
            }
        }

class EconomicGrowthAnalysis:
    """Long-term growth and interest rate relationships"""

    @staticmethod
    def growth_volatility_rates_relationship() -> Dict:
        """Analyze relationship between growth, volatility, and real rates"""
        return {
            "theoretical_framework": {
                "growth_rate_impact": "Higher long-term growth supports higher real rates",
                "volatility_impact": "Higher growth volatility increases risk premiums",
                "equilibrium_relationship": "Real rates ≈ Long-term growth + risk premium"
            },
            "empirical_relationships": {
                "positive_growth_correlation": "Real rates tend to correlate with long-term growth",
                "volatility_premium": "Volatile economies typically have higher real rates",
                "international_comparison": "Developed economies show stronger correlation"
            },
            "policy_implications": {
                "central_bank_targeting": "Policy rates consider long-term growth potential",
                "fiscal_policy": "Government debt sustainability linked to growth",
                "investment_decisions": "Real rate environment affects capital allocation"
            }
        }

    @staticmethod
    def calculate_equilibrium_real_rate(long_term_growth: float,
                                      growth_volatility: float,
                                      risk_aversion: float = 2.0) -> Dict:
        """Calculate equilibrium real interest rate"""
        # Simplified model: r* = g + risk_premium
        growth_risk_premium = risk_aversion * (growth_volatility ** 2) / 2
        equilibrium_rate = long_term_growth + growth_risk_premium

        return {
            "equilibrium_real_rate": equilibrium_rate,
            "components": {
                "long_term_growth": long_term_growth,
                "growth_risk_premium": growth_risk_premium,
                "volatility_impact": growth_volatility
            },
            "sensitivity_analysis": {
                "growth_sensitivity": 1.0,  # 1:1 relationship
                "volatility_sensitivity": risk_aversion * growth_volatility
            }
        }

class BusinessCycleAnalysis:
    """Business cycle effects on markets and policy"""

    CYCLE_CHARACTERISTICS = {
        BusinessCyclePhase.EXPANSION: {
            "gdp_growth": "positive_accelerating",
            "unemployment": "declining",
            "inflation": "rising",
            "policy_stance": "neutral_to_tightening",
            "yield_curve": "steepening_then_flattening"
        },
        BusinessCyclePhase.PEAK: {
            "gdp_growth": "positive_slowing",
            "unemployment": "low",
            "inflation": "high",
            "policy_stance": "restrictive",
            "yield_curve": "flat_or_inverted"
        },
        BusinessCyclePhase.CONTRACTION: {
            "gdp_growth": "negative",
            "unemployment": "rising",
            "inflation": "falling",
            "policy_stance": "accommodative",
            "yield_curve": "steepening"
        },
        BusinessCyclePhase.TROUGH: {
            "gdp_growth": "negative_to_positive",
            "unemployment": "high_but_stabilizing",
            "inflation": "low",
            "policy_stance": "very_accommodative",
            "yield_curve": "steep"
        }
    }

    @staticmethod
    def analyze_cycle_effects(current_phase: BusinessCyclePhase) -> Dict:
        """Analyze business cycle effects on policy and markets"""
        characteristics = BusinessCycleAnalysis.CYCLE_CHARACTERISTICS[current_phase]

        return {
            "current_phase": current_phase.value,
            "phase_characteristics": characteristics,
            "policy_implications": BusinessCycleAnalysis._policy_implications(current_phase),
            "market_implications": BusinessCycleAnalysis._market_implications(current_phase),
            "yield_curve_effects": BusinessCycleAnalysis._yield_curve_effects(current_phase)
        }

    @staticmethod
    def _policy_implications(phase: BusinessCyclePhase) -> Dict:
        """Policy implications by cycle phase"""
        policy_map = {
            BusinessCyclePhase.EXPANSION: {
                "monetary_policy": "Gradually tighten to prevent overheating",
                "fiscal_policy": "Reduce deficits, build fiscal space",
                "regulatory_stance": "May tighten financial regulations"
            },
            BusinessCyclePhase.PEAK: {
                "monetary_policy": "Restrictive to combat inflation",
                "fiscal_policy": "Counter-cyclical tightening",
                "regulatory_stance": "Monitor financial stability"
            },
            BusinessCyclePhase.CONTRACTION: {
                "monetary_policy": "Aggressive easing to support economy",
                "fiscal_policy": "Stimulus spending to cushion recession",
                "regulatory_stance": "May ease to support lending"
            },
            BusinessCyclePhase.TROUGH: {
                "monetary_policy": "Maintain accommodation for recovery",
                "fiscal_policy": "Continue support but plan exit strategy",
                "regulatory_stance": "Gradual normalization"
            }
        }
        return policy_map[phase]

    @staticmethod
    def _market_implications(phase: BusinessCyclePhase) -> Dict:
        """Market implications by cycle phase"""
        market_map = {
            BusinessCyclePhase.EXPANSION: {
                "equity_performance": "Strong performance, cyclicals outperform",
                "bond_performance": "Rising yields hurt bond performance",
                "credit_spreads": "Tightening spreads",
                "sector_rotation": "Financials, industrials, materials favor"
            },
            BusinessCyclePhase.PEAK: {
                "equity_performance": "Volatile, defensive sectors outperform",
                "bond_performance": "Poor performance due to high yields",
                "credit_spreads": "Beginning to widen",
                "sector_rotation": "Utilities, consumer staples favor"
            },
            BusinessCyclePhase.CONTRACTION: {
                "equity_performance": "Poor performance, high volatility",
                "bond_performance": "Strong performance as yields fall",
                "credit_spreads": "Widening significantly",
                "sector_rotation": "Quality defensive names outperform"
            },
            BusinessCyclePhase.TROUGH: {
                "equity_performance": "Recovery begins, cyclicals lead",
                "bond_performance": "Moderate as yields stabilize",
                "credit_spreads": "Peak widening, beginning to stabilize",
                "sector_rotation": "Early cyclicals, technology, discretionary"
            }
        }
        return market_map[phase]

    @staticmethod
    def _yield_curve_effects(phase: BusinessCyclePhase) -> Dict:
        """Yield curve effects by cycle phase"""
        curve_map = {
            BusinessCyclePhase.EXPANSION: {
                "curve_shape": "Steepening early, flattening late",
                "short_rates": "Rising gradually",
                "long_rates": "Rising but less than short rates",
                "duration_performance": "Negative for longer durations"
            },
            BusinessCyclePhase.PEAK: {
                "curve_shape": "Flat or inverted",
                "short_rates": "Peak levels",
                "long_rates": "Lower than short rates",
                "duration_performance": "Poor across all durations"
            },
            BusinessCyclePhase.CONTRACTION: {
                "curve_shape": "Steepening dramatically",
                "short_rates": "Falling rapidly",
                "long_rates": "Falling but less than short rates",
                "duration_performance": "Positive, especially long duration"
            },
            BusinessCyclePhase.TROUGH: {
                "curve_shape": "Steep",
                "short_rates": "Near zero",
                "long_rates": "Low but above short rates",
                "duration_performance": "Positive but moderating"
            }
        }
        return curve_map[phase]

class InflationAnalysis:
    """Inflation and bond market analysis"""

    @staticmethod
    def yield_spread_analysis() -> Dict:
        """Analyze factors affecting yield spreads"""
        return {
            "inflation_expectations": {
                "tips_vs_nominal": "TIPS spread indicates inflation expectations",
                "breakeven_inflation": "Market-implied inflation expectations",
                "survey_measures": "Professional forecaster expectations"
            },
            "liquidity_factors": {
                "tips_liquidity": "TIPS markets less liquid than nominal bonds",
                "seasonal_patterns": "Tax timing affects TIPS demand",
                "market_depth": "Smaller TIPS market affects pricing"
            },
            "technical_factors": {
                "supply_considerations": "TIPS issuance patterns",
                "demand_patterns": "Institutional demand for inflation protection",
                "relative_value": "TIPS vs nominal bond valuations"
            }
        }

    @staticmethod
    def calculate_breakeven_inflation(nominal_yield: float, tips_yield: float) -> Dict:
        """Calculate breakeven inflation rate"""
        breakeven_rate = nominal_yield - tips_yield

        return {
            "breakeven_inflation": breakeven_rate,
            "components": {
                "nominal_yield": nominal_yield,
                "tips_yield": tips_yield
            },
            "interpretation": {
                "market_expectation": f"Market expects {breakeven_rate:.2%} average inflation",
                "relative_value": "TIPS attractive if actual inflation exceeds breakeven" if breakeven_rate > 0.02 else "Nominal bonds may be preferred"
            }
        }

class CreditAnalysis:
    """Credit spreads and business cycle relationship"""

    @staticmethod
    def credit_cycle_analysis(cycle_phase: BusinessCyclePhase) -> Dict:
        """Analyze credit performance by business cycle phase"""
        credit_characteristics = {
            BusinessCyclePhase.EXPANSION: {
                "credit_spreads": "Tightening",
                "default_rates": "Declining",
                "credit_quality": "Improving",
                "issuance_activity": "High, deteriorating quality toward end"
            },
            BusinessCyclePhase.PEAK: {
                "credit_spreads": "Tight but beginning to widen",
                "default_rates": "Low but rising",
                "credit_quality": "Peak quality, starting to deteriorate",
                "issuance_activity": "High volume, lower quality"
            },
            BusinessCyclePhase.CONTRACTION: {
                "credit_spreads": "Widening significantly",
                "default_rates": "Rising sharply",
                "credit_quality": "Deteriorating rapidly",
                "issuance_activity": "Limited to high-quality issuers"
            },
            BusinessCyclePhase.TROUGH: {
                "credit_spreads": "Wide but stabilizing",
                "default_rates": "Peak levels",
                "credit_quality": "Stabilizing at low levels",
                "issuance_activity": "Low volume, high quality"
            }
        }

        return {
            "cycle_phase": cycle_phase.value,
            "credit_characteristics": credit_characteristics[cycle_phase],
            "investment_strategy": CreditAnalysis._credit_strategy(cycle_phase),
            "sector_considerations": CreditAnalysis._sector_credit_analysis(cycle_phase)
        }

    @staticmethod
    def _credit_strategy(phase: BusinessCyclePhase) -> Dict:
        """Credit investment strategy by phase"""
        strategy_map = {
            BusinessCyclePhase.EXPANSION: {
                "duration_strategy": "Shorter duration as rates rise",
                "credit_quality": "Can take more credit risk",
                "sector_allocation": "Cyclical sectors benefit"
            },
            BusinessCyclePhase.PEAK: {
                "duration_strategy": "Neutral duration",
                "credit_quality": "Begin moving to higher quality",
                "sector_allocation": "Reduce cyclical exposure"
            },
            BusinessCyclePhase.CONTRACTION: {
                "duration_strategy": "Longer duration benefits from falling rates",
                "credit_quality": "Focus on highest quality",
                "sector_allocation": "Defensive sectors only"
            },
            BusinessCyclePhase.TROUGH: {
                "duration_strategy": "Long duration still beneficial",
                "credit_quality": "Begin adding credit risk selectively",
                "sector_allocation": "Early cyclical opportunities"
            }
        }
        return strategy_map[phase]

    @staticmethod
    def _sector_credit_analysis(phase: BusinessCyclePhase) -> Dict:
        """Sector-specific credit analysis"""
        return {
            "cyclical_sectors": {
                "performance": "Strong in expansion, weak in contraction",
                "examples": ["Manufacturing", "Construction", "Retail"],
                "credit_characteristics": "Credit quality correlates with cycle"
            },
            "defensive_sectors": {
                "performance": "Stable across cycle",
                "examples": ["Utilities", "Healthcare", "Consumer staples"],
                "credit_characteristics": "More stable credit metrics"
            },
            "financial_sector": {
                "performance": "Benefits from steepening curve, hurt by credit losses",
                "credit_characteristics": "Sensitive to credit cycle and regulations",
                "special_considerations": "Regulatory capital requirements"
            }
        }

class EquityRiskPremium:
    """Equity risk premium and consumption hedging analysis"""

    @staticmethod
    def consumption_hedging_analysis() -> Dict:
        """Analyze equity risk premium through consumption hedging lens"""
        return {
            "theoretical_framework": {
                "consumption_capm": "Risk premium based on consumption risk",
                "hedging_properties": "Assets that hedge consumption risk command lower returns",
                "pro_cyclical_assets": "Assets correlated with consumption require higher returns"
            },
            "equity_characteristics": {
                "consumption_correlation": "Equities generally pro-cyclical with consumption",
                "recession_performance": "Poor performance when consumption falls",
                "required_premium": "Higher premium for poor consumption hedging"
            },
            "bond_characteristics": {
                "consumption_hedge": "Bonds often provide consumption hedging",
                "flight_to_quality": "Bonds rally when consumption threatened",
                "lower_premium": "Lower risk premium due to hedging properties"
            }
        }

    @staticmethod
    def estimate_equity_risk_premium(equity_volatility: float,
                                   consumption_volatility: float,
                                   correlation: float,
                                   risk_aversion: float = 3.0) -> Dict:
        """Estimate equity risk premium using consumption CAPM"""
        # Simplified consumption CAPM: ERP = γ * σ_c * σ_e * ρ
        risk_premium = risk_aversion * consumption_volatility * equity_volatility * correlation

        return {
            "estimated_risk_premium": risk_premium,
            "components": {
                "risk_aversion": risk_aversion,
                "consumption_volatility": consumption_volatility,
                "equity_volatility": equity_volatility,
                "correlation": correlation
            },
            "interpretation": {
                "premium_level": "High" if risk_premium > 0.06 else "Moderate" if risk_premium > 0.04 else "Low",
                "hedging_value": "Poor consumption hedge" if correlation > 0.5 else "Moderate hedge" if correlation > 0 else "Good hedge"
            }
        }

class EarningsAnalysis:
    """Earnings growth expectations and business cycle"""

    @staticmethod
    def earnings_cycle_analysis(cycle_phase: BusinessCyclePhase) -> Dict:
        """Analyze earnings expectations by cycle phase"""
        earnings_map = {
            BusinessCyclePhase.EXPANSION: {
                "short_term_growth": "Strong and accelerating",
                "long_term_growth": "Optimistic revisions upward",
                "earnings_quality": "Improving margins and volumes",
                "analyst_behavior": "Positive revisions, raising estimates"
            },
            BusinessCyclePhase.PEAK: {
                "short_term_growth": "Strong but decelerating",
                "long_term_growth": "Peak optimism, beginning to moderate",
                "earnings_quality": "Margins under pressure",
                "analyst_behavior": "Mixed revisions, uncertainty increasing"
            },
            BusinessCyclePhase.CONTRACTION: {
                "short_term_growth": "Negative and deteriorating",
                "long_term_growth": "Pessimistic, significant downgrades",
                "earnings_quality": "Weak across all metrics",
                "analyst_behavior": "Negative revisions, lowering estimates"
            },
            BusinessCyclePhase.TROUGH: {
                "short_term_growth": "Negative but stabilizing",
                "long_term_growth": "Beginning to look ahead to recovery",
                "earnings_quality": "Stabilizing at low levels",
                "analyst_behavior": "Stabilizing estimates, early optimism"
            }
        }

        return {
            "cycle_phase": cycle_phase.value,
            "earnings_characteristics": earnings_map[cycle_phase],
            "valuation_implications": EarningsAnalysis._valuation_implications(cycle_phase)
        }

    @staticmethod
    def _valuation_implications(phase: BusinessCyclePhase) -> Dict:
        """Valuation implications of earnings cycle"""
        valuation_map = {
            BusinessCyclePhase.EXPANSION: {
                "pe_multiples": "Expanding as growth accelerates",
                "forward_vs_trailing": "Forward PE below trailing as growth expected",
                "sector_dispersion": "Cyclicals commanding premium multiples"
            },
            BusinessCyclePhase.PEAK: {
                "pe_multiples": "Peak multiples but vulnerable",
                "forward_vs_trailing": "Forward PE rising as growth slows",
                "sector_dispersion": "Defensive sectors gaining multiple premium"
            },
            BusinessCyclePhase.CONTRACTION: {
                "pe_multiples": "Contracting significantly",
                "forward_vs_trailing": "Trailing PE very high due to depressed earnings",
                "sector_dispersion": "Quality and defensive at premium"
            },
            BusinessCyclePhase.TROUGH: {
                "pe_multiples": "Beginning to expand on recovery hopes",
                "forward_vs_trailing": "Forward PE much lower than trailing",
                "sector_dispersion": "Early cyclicals beginning to rerate"
            }
        }
        return valuation_map[phase]

class ValuationMultiples:
    """Cyclical effects on valuation multiples"""

    @staticmethod
    def multiple_analysis() -> Dict:
        """Analyze cyclical effects on valuation multiples"""
        return {
            "pe_ratio_cycles": {
                "expansion": "Rising P/E as growth expectations improve",
                "peak": "Peak P/E but vulnerable to disappointment",
                "contraction": "Falling P/E as earnings decline",
                "trough": "Low P/E but earnings depressed"
            },
            "ev_ebitda_cycles": {
                "advantages": "Less affected by depreciation and financial structure",
                "cycle_behavior": "Similar pattern to P/E but less volatile",
                "sector_utility": "Particularly useful for capital-intensive sectors"
            },
            "price_to_book": {
                "cycle_stability": "More stable through cycles",
                "financial_sectors": "Key metric for banks and insurers",
                "asset_quality": "Reflects asset quality and franchise value"
            }
        }

    @staticmethod
    def calculate_normalized_multiples(current_earnings: float,
                                     normalized_earnings: float,
                                     current_price: float) -> Dict:
        """Calculate normalized valuation multiples"""
        current_pe = current_price / current_earnings if current_earnings > 0 else float('inf')
        normalized_pe = current_price / normalized_earnings if normalized_earnings > 0 else float('inf')

        return {
            "current_pe": current_pe,
            "normalized_pe": normalized_pe,
            "valuation_assessment": {
                "current_basis": "Expensive" if current_pe > 20 else "Fair" if current_pe > 15 else "Cheap",
                "normalized_basis": "Expensive" if normalized_pe > 18 else "Fair" if normalized_pe > 12 else "Cheap"
            },
            "cycle_adjustment": normalized_pe - current_pe
        }

class RealEstateAnalysis:
    """Commercial real estate economic factors"""

    @staticmethod
    def real_estate_economic_factors() -> Dict:
        """Analyze economic factors affecting commercial real estate"""
        return {
            "demand_factors": {
                "economic_growth": "GDP growth drives space demand",
                "employment_growth": "Job creation increases office/retail demand",
                "population_growth": "Demographics drive residential demand",
                "business_formation": "New businesses need commercial space"
            },
            "supply_factors": {
                "construction_costs": "Material and labor costs affect new supply",
                "land_availability": "Zoning and development restrictions",
                "permitting_process": "Regulatory approval timelines",
                "developer_financing": "Credit availability for development"
            },
            "financial_factors": {
                "interest_rates": "Affect both cap rates and financing costs",
                "credit_availability": "Lending standards impact transactions",
                "equity_markets": "REIT performance affects capital flows",
                "inflation": "Affects both costs and rents"
            }
        }

    @staticmethod
    def real_estate_cycle_analysis(cycle_phase: BusinessCyclePhase) -> Dict:
        """Analyze real estate performance by economic cycle"""
        re_characteristics = {
            BusinessCyclePhase.EXPANSION: {
                "occupancy_rates": "Rising",
                "rent_growth": "Accelerating",
                "cap_rates": "Declining (rising values)",
                "development_activity": "Increasing"
            },
            BusinessCyclePhase.PEAK: {
                "occupancy_rates": "Peak levels",
                "rent_growth": "Strong but moderating",
                "cap_rates": "Low",
                "development_activity": "Peak levels, potential overbuilding"
            },
            BusinessCyclePhase.CONTRACTION: {
                "occupancy_rates": "Declining",
                "rent_growth": "Negative",
                "cap_rates": "Rising (falling values)",
                "development_activity": "Declining sharply"
            },
            BusinessCyclePhase.TROUGH: {
                "occupancy_rates": "Low but stabilizing",
                "rent_growth": "Flat to slightly negative",
                "cap_rates": "High but stabilizing",
                "development_activity": "Minimal"
            }
        }

        return {
            "cycle_phase": cycle_phase.value,
            "re_characteristics": re_characteristics[cycle_phase],
            "investment_strategy": RealEstateAnalysis._re_investment_strategy(cycle_phase)
        }

    @staticmethod
    def _re_investment_strategy(phase: BusinessCyclePhase) -> Dict:
        """Real estate investment strategy by cycle phase"""
        strategy_map = {
            BusinessCyclePhase.EXPANSION: {
                "property_types": "Core and value-add opportunities",
                "geographic_focus": "High-growth markets",
                "leverage_strategy": "Moderate leverage appropriate",
                "timing": "Good time to sell mature assets"
            },
            BusinessCyclePhase.PEAK: {
                "property_types": "Focus on core, stable properties",
                "geographic_focus": "Defensive markets",
                "leverage_strategy": "Reduce leverage exposure",
                "timing": "Opportune time to sell"
            },
            BusinessCyclePhase.CONTRACTION: {
                "property_types": "Avoid development, focus on income",
                "geographic_focus": "Defensive, diversified markets",
                "leverage_strategy": "Low leverage, preserve liquidity",
                "timing": "Hold existing, avoid new investments"
            },
            BusinessCyclePhase.TROUGH: {
                "property_types": "Opportunistic investments available",
                "geographic_focus": "Recovery markets",
                "leverage_strategy": "Conservative leverage for opportunities",
                "timing": "Excellent buying opportunities"
            }
        }
        return strategy_map[phase]

class EconomicsMarkets:
    """Main economics and markets interface"""

    def __init__(self):
        self.valuation_factors = MarketValuationFactors()
        self.growth_analysis = EconomicGrowthAnalysis()
        self.cycle_analysis = BusinessCycleAnalysis()
        self.inflation_analysis = InflationAnalysis()
        self.credit_analysis = CreditAnalysis()
        self.equity_premium = EquityRiskPremium()
        self.earnings_analysis = EarningsAnalysis()
        self.multiples_analysis = ValuationMultiples()
        self.real_estate = RealEstateAnalysis()

    def comprehensive_economics_analysis(self, current_cycle_phase: BusinessCyclePhase,
                                       economic_data: Dict) -> Dict:
        """Comprehensive economics and markets analysis"""

        # Business cycle analysis
        cycle_effects = self.cycle_analysis.analyze_cycle_effects(current_cycle_phase)

        # Credit analysis
        credit_analysis = self.credit_analysis.credit_cycle_analysis(current_cycle_phase)

        # Earnings analysis
        earnings_analysis = self.earnings_analysis.earnings_cycle_analysis(current_cycle_phase)

        # Real estate analysis
        re_analysis = self.real_estate.real_estate_cycle_analysis(current_cycle_phase)

        # Valuation factors
        valuation_drivers = self.valuation_factors.analyze_valuation_drivers()
        expectations_analysis = self.valuation_factors.expectations_impact_analysis()

        return {
            "current_cycle_phase": current_cycle_phase.value,
            "business_cycle_analysis": cycle_effects,
            "credit_market_analysis": credit_analysis,
            "earnings_expectations": earnings_analysis,
            "real_estate_analysis": re_analysis,
            "valuation_framework": {
                "valuation_drivers": valuation_drivers,
                "expectations_role": expectations_analysis
            },
            "investment_implications": self._synthesize_investment_implications(
                current_cycle_phase, cycle_effects, credit_analysis, earnings_analysis
            )
        }

    def _synthesize_investment_implications(self, phase: BusinessCyclePhase,
                                          cycle_effects: Dict,
                                          credit_analysis: Dict,
                                          earnings_analysis: Dict) -> Dict:
        """Synthesize investment implications across all analyses"""
        return {
            "asset_allocation_guidance": {
                "equity_allocation": self._equity_allocation_guidance(phase),
                "fixed_income_allocation": self._fixed_income_guidance(phase),
                "alternative_investments": self._alternatives_guidance(phase)
            },
            "sector_rotation": cycle_effects["market_implications"]["sector_rotation"],
            "duration_strategy": credit_analysis["investment_strategy"]["duration_strategy"],
            "credit_quality_preference": credit_analysis["investment_strategy"]["credit_quality"],
            "valuation_approach": earnings_analysis["valuation_implications"]
        }

    def _equity_allocation_guidance(self, phase: BusinessCyclePhase) -> str:
        """Equity allocation guidance by cycle phase"""
        guidance_map = {
            BusinessCyclePhase.EXPANSION: "Overweight equities, favor cyclicals",
            BusinessCyclePhase.PEAK: "Neutral to underweight, shift to defensives",
            BusinessCyclePhase.CONTRACTION: "Underweight equities, quality focus",
            BusinessCyclePhase.TROUGH: "Begin overweighting, early cyclical exposure"
        }
        return guidance_map[phase]

    def _fixed_income_guidance(self, phase: BusinessCyclePhase) -> str:
        """Fixed income guidance by cycle phase"""
        guidance_map = {
            BusinessCyclePhase.EXPANSION: "Shorter duration, higher quality",
            BusinessCyclePhase.PEAK: "Neutral duration, reduce credit risk",
            BusinessCyclePhase.CONTRACTION: "Longer duration, highest quality only",
            BusinessCyclePhase.TROUGH: "Long duration, selective credit opportunities"
        }
        return guidance_map[phase]

    def _alternatives_guidance(self, phase: BusinessCyclePhase) -> str:
        """Alternative investments guidance by cycle phase"""
        guidance_map = {
            BusinessCyclePhase.EXPANSION: "Real estate and commodities attractive",
            BusinessCyclePhase.PEAK: "Reduce cyclical alternatives exposure",
            BusinessCyclePhase.CONTRACTION: "Defensive alternatives, opportunistic funds",
            BusinessCyclePhase.TROUGH: "Distressed opportunities, value investing"
        }
        return guidance_map[phase]