
"""Economic Market Cycles Module
=============================

Business cycle identification and analysis

===== DATA SOURCES REQUIRED =====
INPUT:
  - Macroeconomic time series data from official sources
  - Central bank policy statements and interest rate data
  - International trade and balance of payments statistics
  - Market indicators and sentiment measures
  - Demographic and structural economic data

OUTPUT:
  - Economic trend analysis and forecasts
  - Policy impact assessment and scenario modeling
  - Market cycle identification and timing analysis
  - Cross-country economic comparisons and rankings
  - Investment recommendations based on economic outlook

PARAMETERS:
  - forecast_horizon: Economic forecast horizon (default: 12 months)
  - confidence_level: Confidence level for predictions (default: 0.90)
  - base_currency: Base currency for analysis (default: 'USD')
  - seasonal_adjustment: Seasonal adjustment method (default: true)
  - lookback_period: Historical analysis period (default: 10 years)


from decimal import Decimal
from typing import Dict, List, Tuple, Optional, Any, Union
from datetime import datetime, timedelta
"""

import pandas as pd
import numpy as np
from .core import EconomicsBase, ValidationError, CalculationError, DataError


class BusinessCycleAnalyzer(EconomicsBase):
    """Business cycle phases and economic indicator analysis"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)
        self.cycle_phases = ['expansion', 'peak', 'contraction', 'trough']

    def detect_cycle_phase(self, economic_indicators: Dict[str, Any]) -> Dict[str, Any]:
        """Detect current business cycle phase based on economic indicators"""

        # Extract key indicators
        gdp_growth = self.to_decimal(economic_indicators.get('gdp_growth_rate', 0))
        unemployment_rate = self.to_decimal(economic_indicators.get('unemployment_rate', 0))
        inflation_rate = self.to_decimal(economic_indicators.get('inflation_rate', 0))
        interest_rates = self.to_decimal(economic_indicators.get('interest_rate', 0))
        consumer_confidence = self.to_decimal(economic_indicators.get('consumer_confidence', 0))
        business_investment = self.to_decimal(economic_indicators.get('business_investment_growth', 0))

        # Phase detection scoring
        phase_scores = self._calculate_phase_scores(
            gdp_growth, unemployment_rate, inflation_rate,
            interest_rates, consumer_confidence, business_investment
        )

        # Determine most likely phase
        detected_phase = max(phase_scores.items(), key=lambda x: x[1]['score'])[0]

        return {
            'detected_phase': detected_phase,
            'phase_scores': phase_scores,
            'confidence_level': phase_scores[detected_phase]['score'],
            'indicator_analysis': self._analyze_indicators_by_phase(economic_indicators),
            'phase_characteristics': self._get_phase_characteristics(detected_phase),
            'expected_duration': self._estimate_phase_duration(detected_phase, economic_indicators),
            'investment_implications': self._get_investment_implications(detected_phase)
        }

    def _calculate_phase_scores(self, gdp_growth: Decimal, unemployment: Decimal,
                                inflation: Decimal, interest_rate: Decimal,
                                consumer_conf: Decimal, investment: Decimal) -> Dict[str, Any]:
        """Calculate likelihood scores for each business cycle phase"""

        scores = {}

        # Expansion phase indicators
        expansion_score = self.to_decimal(0)
        if gdp_growth > self.to_decimal(2): expansion_score += self.to_decimal(25)
        if unemployment < self.to_decimal(6): expansion_score += self.to_decimal(20)
        if consumer_conf > self.to_decimal(100): expansion_score += self.to_decimal(20)
        if investment > self.to_decimal(3): expansion_score += self.to_decimal(20)
        if inflation > self.to_decimal(1) and inflation < self.to_decimal(4): expansion_score += self.to_decimal(15)

        scores['expansion'] = {
            'score': expansion_score,
            'indicators': 'Positive GDP growth, low unemployment, high confidence'
        }

        # Peak phase indicators
        peak_score = self.to_decimal(0)
        if gdp_growth > self.to_decimal(1) and gdp_growth < self.to_decimal(3): peak_score += self.to_decimal(15)
        if unemployment < self.to_decimal(4): peak_score += self.to_decimal(25)
        if inflation > self.to_decimal(3): peak_score += self.to_decimal(25)
        if interest_rate > self.to_decimal(4): peak_score += self.to_decimal(20)
        if consumer_conf > self.to_decimal(110): peak_score += self.to_decimal(15)

        scores['peak'] = {
            'score': peak_score,
            'indicators': 'Slowing growth, very low unemployment, rising inflation'
        }

        # Contraction phase indicators
        contraction_score = self.to_decimal(0)
        if gdp_growth < self.to_decimal(0): contraction_score += self.to_decimal(30)
        if unemployment > self.to_decimal(7): contraction_score += self.to_decimal(25)
        if consumer_conf < self.to_decimal(90): contraction_score += self.to_decimal(20)
        if investment < self.to_decimal(0): contraction_score += self.to_decimal(25)

        scores['contraction'] = {
            'score': contraction_score,
            'indicators': 'Negative GDP growth, rising unemployment, low confidence'
        }

        # Trough phase indicators
        trough_score = self.to_decimal(0)
        if gdp_growth > self.to_decimal(-1) and gdp_growth < self.to_decimal(1): trough_score += self.to_decimal(20)
        if unemployment > self.to_decimal(8): trough_score += self.to_decimal(25)
        if inflation < self.to_decimal(2): trough_score += self.to_decimal(20)
        if interest_rate < self.to_decimal(3): trough_score += self.to_decimal(20)
        if consumer_conf < self.to_decimal(85): trough_score += self.to_decimal(15)

        scores['trough'] = {
            'score': trough_score,
            'indicators': 'Stabilizing negative growth, high unemployment, low rates'
        }

        return scores

    def _analyze_indicators_by_phase(self, indicators: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze how different economic indicators vary over business cycle"""

        return {
            'leading_indicators': {
                'description': 'Change before the economy changes direction',
                'examples': {
                    'stock_market': indicators.get('stock_market_performance', 'N/A'),
                    'consumer_confidence': indicators.get('consumer_confidence', 'N/A'),
                    'new_business_formation': indicators.get('new_business_starts', 'N/A'),
                    'yield_curve': indicators.get('yield_curve_slope', 'N/A')
                },
                'investment_utility': 'Most valuable for timing market entry/exit'
            },
            'coincident_indicators': {
                'description': 'Change at the same time as the economy',
                'examples': {
                    'gdp_growth': indicators.get('gdp_growth_rate', 'N/A'),
                    'employment': indicators.get('employment_rate', 'N/A'),
                    'industrial_production': indicators.get('industrial_production', 'N/A'),
                    'retail_sales': indicators.get('retail_sales_growth', 'N/A')
                },
                'investment_utility': 'Confirm current economic state'
            },
            'lagging_indicators': {
                'description': 'Change after the economy has changed direction',
                'examples': {
                    'unemployment_rate': indicators.get('unemployment_rate', 'N/A'),
                    'inflation_rate': indicators.get('inflation_rate', 'N/A'),
                    'interest_rates': indicators.get('interest_rate', 'N/A'),
                    'corporate_profits': indicators.get('corporate_profit_growth', 'N/A')
                },
                'investment_utility': 'Confirm cycle turning points after the fact'
            }
        }

    def _get_phase_characteristics(self, phase: str) -> Dict[str, Any]:
        """Get detailed characteristics of each business cycle phase"""

        characteristics = {
            'expansion': {
                'duration': '2-8 years typically',
                'gdp_growth': 'Positive and accelerating',
                'unemployment': 'Declining',
                'inflation': 'Gradually rising',
                'interest_rates': 'Rising as central bank tightens',
                'business_activity': 'Increasing investment, hiring, capacity utilization',
                'consumer_behavior': 'Rising confidence, increased spending',
                'financial_markets': 'Stock markets generally rising, credit expanding'
            },
            'peak': {
                'duration': 'Brief period (months)',
                'gdp_growth': 'Positive but slowing',
                'unemployment': 'At cyclical lows',
                'inflation': 'At or near cyclical highs',
                'interest_rates': 'At cyclical highs',
                'business_activity': 'Capacity constraints, labor shortages',
                'consumer_behavior': 'High confidence but spending slowing',
                'financial_markets': 'Stock markets vulnerable, tight credit'
            },
            'contraction': {
                'duration': '6 months to 2 years',
                'gdp_growth': 'Negative for at least 2 quarters',
                'unemployment': 'Rising sharply',
                'inflation': 'Falling due to weak demand',
                'interest_rates': 'Falling as central bank eases',
                'business_activity': 'Declining investment, layoffs, low capacity use',
                'consumer_behavior': 'Falling confidence, reduced spending',
                'financial_markets': 'Bear markets, credit contraction'
            },
            'trough': {
                'duration': 'Brief period (months)',
                'gdp_growth': 'Negative but stabilizing',
                'unemployment': 'At cyclical highs but stabilizing',
                'inflation': 'Low and stable',
                'interest_rates': 'At cyclical lows',
                'business_activity': 'Excess capacity, cautious investment',
                'consumer_behavior': 'Low confidence but stabilizing',
                'financial_markets': 'Markets often bottom before economy'
            }
        }

        return characteristics.get(phase, {})

    def _estimate_phase_duration(self, phase: str, indicators: Dict[str, Any]) -> str:
        """Estimate remaining duration of current phase"""

        phase_durations = {
            'expansion': 'Typically 2-8 years; current strength suggests 1-3 years remaining',
            'peak': 'Brief transition period; 3-12 months before contraction begins',
            'contraction': 'Typically 6-18 months; depth indicates 6-12 months remaining',
            'trough': 'Brief transition period; recovery likely within 3-9 months'
        }

        return phase_durations.get(phase, 'Duration uncertain')

    def _get_investment_implications(self, phase: str) -> Dict[str, Any]:
        """Get investment implications for each business cycle phase"""

        implications = {
            'expansion': {
                'equity_strategy': 'Favor cyclical stocks, growth sectors',
                'fixed_income': 'Shorter duration, higher yield focus',
                'sectors_to_favor': ['Technology', 'Consumer Discretionary', 'Industrials'],
                'sectors_to_avoid': ['Utilities', 'Consumer Staples'],
                'overall_risk': 'Moderate to High'
            },
            'peak': {
                'equity_strategy': 'Defensive positioning, quality focus',
                'fixed_income': 'Extend duration, prepare for rate cuts',
                'sectors_to_favor': ['Healthcare', 'Consumer Staples', 'Utilities'],
                'sectors_to_avoid': ['Cyclicals', 'Small caps'],
                'overall_risk': 'Reduce risk exposure'
            },
            'contraction': {
                'equity_strategy': 'Defensive stocks, dividend focus',
                'fixed_income': 'High quality bonds, government securities',
                'sectors_to_favor': ['Consumer Staples', 'Healthcare', 'Utilities'],
                'sectors_to_avoid': ['Financials', 'Materials', 'Energy'],
                'overall_risk': 'Low risk, capital preservation'
            },
            'trough': {
                'equity_strategy': 'Prepare for cyclical recovery, value opportunities',
                'fixed_income': 'Shorten duration, prepare for rate rises',
                'sectors_to_favor': ['Financials', 'Technology', 'Industrials'],
                'sectors_to_avoid': ['Defensive sectors becoming expensive'],
                'overall_risk': 'Gradually increase risk'
            }
        }

        return implications.get(phase, {})

    def analyze_sector_cyclicality(self, sector_data: Dict[str, List[Decimal]]) -> Dict[str, Any]:
        """Analyze how different sectors vary over business cycle"""

        sector_analysis = {}

        for sector, performance_data in sector_data.items():
            if len(performance_data) < 4:
                continue

            # Calculate volatility and correlation with cycle
            volatility = self._calculate_volatility(performance_data)

            # Classify sector cyclicality
            if volatility > self.to_decimal(20):
                cyclicality = 'Highly Cyclical'
                characteristics = 'Large swings with economic cycle'
            elif volatility > self.to_decimal(12):
                cyclicality = 'Moderately Cyclical'
                characteristics = 'Moderate sensitivity to economic changes'
            else:
                cyclicality = 'Defensive'
                characteristics = 'Stable performance through cycles'

            sector_analysis[sector] = {
                'volatility': volatility,
                'cyclicality': cyclicality,
                'characteristics': characteristics,
                'investment_timing': self._get_sector_timing(cyclicality)
            }

        return {
            'sector_analysis': sector_analysis,
            'summary': self._summarize_sector_cyclicality(sector_analysis)
        }

    def _calculate_volatility(self, data: List[Decimal]) -> Decimal:
        """Calculate volatility of performance data"""
        if len(data) < 2:
            return self.to_decimal(0)

        mean = sum(data) / self.to_decimal(len(data))
        variance = sum((x - mean) ** 2 for x in data) / self.to_decimal(len(data) - 1)
        return (variance.sqrt() * self.to_decimal(100))  # As percentage

    def _get_sector_timing(self, cyclicality: str) -> str:
        """Get optimal timing for sector investment"""
        timing_guide = {
            'Highly Cyclical': 'Buy at trough, sell at peak',
            'Moderately Cyclical': 'Overweight in expansion, underweight in contraction',
            'Defensive': 'Stable allocation, overweight during uncertainty'
        }
        return timing_guide.get(cyclicality, 'Standard allocation')

    def _summarize_sector_cyclicality(self, analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Summarize sector cyclicality analysis"""

        cyclical_count = sum(1 for sector in analysis.values()
                             if 'Cyclical' in sector['cyclicality'])
        defensive_count = len(analysis) - cyclical_count

        return {
            'total_sectors_analyzed': len(analysis),
            'cyclical_sectors': cyclical_count,
            'defensive_sectors': defensive_count,
            'portfolio_implication': 'Balance cyclical and defensive based on cycle phase'
        }

    def calculate(self, analysis_type: str = 'phase_detection', **kwargs) -> Dict[str, Any]:
        """Main business cycle calculation dispatcher"""

        if analysis_type == 'phase_detection':
            return self.detect_cycle_phase(kwargs['economic_indicators'])
        elif analysis_type == 'sector_cyclicality':
            return self.analyze_sector_cyclicality(kwargs['sector_data'])
        else:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")


class CreditCycleAnalyzer(EconomicsBase):
    """Credit cycle analysis and financial stability assessment"""

    def analyze_credit_cycle(self, credit_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze current credit cycle phase and characteristics"""

        # Extract credit indicators
        credit_growth = self.to_decimal(credit_data.get('credit_growth_rate', 0))
        loan_standards = credit_data.get('lending_standards', 'neutral')  # tight, neutral, loose
        credit_spreads = self.to_decimal(credit_data.get('credit_spreads_bps', 0))
        default_rates = self.to_decimal(credit_data.get('default_rate', 0))
        leverage_ratio = self.to_decimal(credit_data.get('leverage_ratio', 0))
        asset_prices = self.to_decimal(credit_data.get('asset_price_growth', 0))

        # Determine credit cycle phase
        cycle_phase = self._determine_credit_phase(
            credit_growth, loan_standards, credit_spreads, default_rates
        )

        return {
            'credit_cycle_phase': cycle_phase,
            'phase_characteristics': self._get_credit_phase_characteristics(cycle_phase),
            'risk_assessment': self._assess_credit_risks(
                credit_growth, leverage_ratio, default_rates, asset_prices
            ),
            'financial_stability_indicators': self._analyze_financial_stability(credit_data),
            'investment_implications': self._credit_cycle_investment_implications(cycle_phase),
            'policy_implications': self._credit_cycle_policy_implications(cycle_phase, credit_data)
        }

    def _determine_credit_phase(self, credit_growth: Decimal, standards: str,
                                spreads: Decimal, defaults: Decimal) -> str:
        """Determine current credit cycle phase"""

        # Credit expansion phase
        if (credit_growth > self.to_decimal(5) and
                standards == 'loose' and
                spreads < self.to_decimal(200)):
            return 'expansion'

        # Credit peak/bubble phase
        elif (credit_growth > self.to_decimal(8) and
              spreads < self.to_decimal(150) and
              defaults < self.to_decimal(2)):
            return 'peak'

        # Credit contraction phase
        elif (credit_growth < self.to_decimal(0) and
              standards == 'tight' and
              spreads > self.to_decimal(300)):
            return 'contraction'

        # Credit trough phase
        elif (credit_growth < self.to_decimal(2) and
              defaults > self.to_decimal(5) and
              spreads > self.to_decimal(400)):
            return 'trough'

        else:
            return 'transition'

    def _get_credit_phase_characteristics(self, phase: str) -> Dict[str, Any]:
        """Get characteristics of each credit cycle phase"""

        characteristics = {
            'expansion': {
                'credit_growth': 'Accelerating',
                'lending_standards': 'Loosening',
                'credit_spreads': 'Tightening',
                'default_rates': 'Low and declining',
                'asset_prices': 'Rising',
                'risk_appetite': 'Increasing',
                'typical_duration': '3-7 years'
            },
            'peak': {
                'credit_growth': 'Very high but potentially slowing',
                'lending_standards': 'Very loose',
                'credit_spreads': 'Very tight',
                'default_rates': 'Near cyclical lows',
                'asset_prices': 'Near peaks, potential bubbles',
                'risk_appetite': 'Excessive',
                'typical_duration': '6-18 months'
            },
            'contraction': {
                'credit_growth': 'Negative',
                'lending_standards': 'Tightening rapidly',
                'credit_spreads': 'Widening',
                'default_rates': 'Rising sharply',
                'asset_prices': 'Declining',
                'risk_appetite': 'Risk aversion',
                'typical_duration': '1-3 years'
            },
            'trough': {
                'credit_growth': 'Negative but stabilizing',
                'lending_standards': 'Very tight',
                'credit_spreads': 'Wide but stabilizing',
                'default_rates': 'High but peaking',
                'asset_prices': 'Depressed but stabilizing',
                'risk_appetite': 'Extremely low',
                'typical_duration': '6-18 months'
            }
        }

        return characteristics.get(phase, {})

    def _assess_credit_risks(self, credit_growth: Decimal, leverage: Decimal,
                             defaults: Decimal, asset_prices: Decimal) -> Dict[str, Any]:
        """Assess systemic credit risks"""

        risk_score = self.to_decimal(0)
        risk_factors = []

        # Credit growth risk
        if credit_growth > self.to_decimal(10):
            risk_score += self.to_decimal(25)
            risk_factors.append('Excessive credit growth')

        # Leverage risk
        if leverage > self.to_decimal(8):
            risk_score += self.to_decimal(25)
            risk_factors.append('High leverage ratios')

        # Default rate risk
        if defaults > self.to_decimal(4):
            risk_score += self.to_decimal(20)
            risk_factors.append('Rising default rates')

        # Asset price risk
        if asset_prices > self.to_decimal(15):
            risk_score += self.to_decimal(20)
            risk_factors.append('Asset price bubbles')

        # Determine risk level
        if risk_score > self.to_decimal(60):
            risk_level = 'High'
        elif risk_score > self.to_decimal(30):
            risk_level = 'Moderate'
        else:
            risk_level = 'Low'

        return {
            'overall_risk_score': risk_score,
            'risk_level': risk_level,
            'key_risk_factors': risk_factors,
            'systemic_risk_probability': self._calculate_systemic_risk_probability(risk_score)
        }

    def _calculate_systemic_risk_probability(self, risk_score: Decimal) -> str:
        """Calculate probability of systemic financial crisis"""

        if risk_score > self.to_decimal(70):
            return 'High (>30% in next 2 years)'
        elif risk_score > self.to_decimal(50):
            return 'Moderate (10-30% in next 2 years)'
        elif risk_score > self.to_decimal(30):
            return 'Low (5-10% in next 2 years)'
        else:
            return 'Very Low (<5% in next 2 years)'

    def _analyze_financial_stability(self, credit_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze financial stability indicators"""

        return {
            'banking_sector_health': {
                'capital_adequacy': credit_data.get('bank_capital_ratio', 'N/A'),
                'loan_loss_provisions': credit_data.get('loan_loss_provisions', 'N/A'),
                'profitability': credit_data.get('bank_roe', 'N/A'),
                'asset_quality': credit_data.get('npl_ratio', 'N/A')
            },
            'household_sector': {
                'debt_to_income': credit_data.get('household_debt_ratio', 'N/A'),
                'mortgage_defaults': credit_data.get('mortgage_default_rate', 'N/A'),
                'savings_rate': credit_data.get('household_savings_rate', 'N/A')
            },
            'corporate_sector': {
                'corporate_debt_ratio': credit_data.get('corporate_debt_gdp', 'N/A'),
                'interest_coverage': credit_data.get('interest_coverage_ratio', 'N/A'),
                'bankruptcy_rate': credit_data.get('corporate_bankruptcy_rate', 'N/A')
            },
            'government_sector': {
                'debt_to_gdp': credit_data.get('government_debt_gdp', 'N/A'),
                'deficit_ratio': credit_data.get('budget_deficit_gdp', 'N/A')
            }
        }

    def _credit_cycle_investment_implications(self, phase: str) -> Dict[str, Any]:
        """Investment implications for each credit cycle phase"""

        implications = {
            'expansion': {
                'credit_sensitive_sectors': 'Favor banks, real estate, consumer finance',
                'fixed_income': 'Corporate bonds outperform, credit spreads tighten',
                'equity_strategy': 'Growth and cyclical stocks perform well',
                'risk_management': 'Monitor leverage, prepare for cycle turn'
            },
            'peak': {
                'credit_sensitive_sectors': 'Begin reducing exposure to credit cyclicals',
                'fixed_income': 'Lock in credit spreads, extend duration',
                'equity_strategy': 'Rotate to defensive sectors',
                'risk_management': 'Reduce overall risk exposure'
            },
            'contraction': {
                'credit_sensitive_sectors': 'Avoid banks, real estate, high-yield bonds',
                'fixed_income': 'Government bonds, high-grade corporates',
                'equity_strategy': 'Defensive sectors, dividend stocks',
                'risk_management': 'Capital preservation focus'
            },
            'trough': {
                'credit_sensitive_sectors': 'Prepare for opportunistic investments',
                'fixed_income': 'Distressed debt opportunities',
                'equity_strategy': 'Value opportunities in beaten-down sectors',
                'risk_management': 'Begin rebuilding risk exposure'
            }
        }

        return implications.get(phase, {})

    def _credit_cycle_policy_implications(self, phase: str, credit_data: Dict[str, Any]) -> Dict[str, Any]:
        """Policy implications for each credit cycle phase"""

        base_implications = {
            'expansion': {
                'monetary_policy': 'Consider gradual tightening to prevent bubbles',
                'macroprudential': 'Implement countercyclical capital buffers',
                'regulatory': 'Monitor systemic risk buildup'
            },
            'peak': {
                'monetary_policy': 'Careful balancing to prevent hard landing',
                'macroprudential': 'Activate countercyclical buffers',
                'regulatory': 'Stress test financial institutions'
            },
            'contraction': {
                'monetary_policy': 'Aggressive easing to support credit flow',
                'macroprudential': 'Release countercyclical buffers',
                'regulatory': 'Temporary forbearance measures'
            },
            'trough': {
                'monetary_policy': 'Maintain accommodative stance',
                'macroprudential': 'Gradual rebuilding of buffers',
                'regulatory': 'Support credit intermediation'
            }
        }

        return base_implications.get(phase, {})

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Calculate credit cycle analysis"""
        return self.analyze_credit_cycle(kwargs['credit_data'])


class MarketStructureAnalyzer(EconomicsBase):
    """Market structure analysis and competitive dynamics"""

    def identify_market_structure(self, market_data: Dict[str, Any]) -> Dict[str, Any]:
        """Identify market structure type and characteristics"""

        # Extract market characteristics
        num_firms = int(market_data.get('number_of_firms', 0))
        market_concentration = self.to_decimal(market_data.get('herfindahl_index', 0))
        product_differentiation = market_data.get('product_differentiation', 'low')
        barriers_to_entry = market_data.get('barriers_to_entry', 'low')
        pricing_power = market_data.get('pricing_power', 'low')

        # Determine market structure
        structure_type = self._classify_market_structure(
            num_firms, market_concentration, product_differentiation, barriers_to_entry
        )

        return {
            'market_structure_type': structure_type,
            'structure_characteristics': self._get_structure_characteristics(structure_type),
            'concentration_analysis': self._analyze_concentration(market_concentration, num_firms),
            'competitive_dynamics': self._analyze_competitive_dynamics(structure_type, market_data),
            'pricing_analysis': self._analyze_pricing_behavior(structure_type, market_data),
            'efficiency_implications': self._analyze_efficiency_implications(structure_type),
            'regulatory_considerations': self._get_regulatory_considerations(structure_type)
        }

    def _classify_market_structure(self, num_firms: int, concentration: Decimal,
                                   differentiation: str, barriers: str) -> str:
        """Classify market structure based on key characteristics"""

        # Perfect competition
        if (num_firms > 100 and
                concentration < self.to_decimal(0.01) and
                differentiation == 'none' and
                barriers == 'low'):
            return 'perfect_competition'

        # Monopoly
        elif num_firms == 1 or concentration > self.to_decimal(0.9):
            return 'monopoly'

        # Oligopoly
        elif (num_firms <= 10 and
              concentration > self.to_decimal(0.6) and
              barriers in ['high', 'moderate']):
            return 'oligopoly'

        # Monopolistic competition
        else:
            return 'monopolistic_competition'

    def _get_structure_characteristics(self, structure_type: str) -> Dict[str, Any]:
        """Get detailed characteristics of each market structure"""

        characteristics = {
            'perfect_competition': {
                'number_of_firms': 'Many (hundreds or thousands)',
                'product_differentiation': 'None (homogeneous products)',
                'barriers_to_entry': 'None',
                'pricing_power': 'None (price takers)',
                'long_run_profits': 'Zero economic profits',
                'efficiency': 'Allocatively and productively efficient',
                'examples': 'Agricultural markets, commodity markets'
            },
            'monopolistic_competition': {
                'number_of_firms': 'Many (dozens to hundreds)',
                'product_differentiation': 'Some (differentiated products)',
                'barriers_to_entry': 'Low',
                'pricing_power': 'Limited (some control over price)',
                'long_run_profits': 'Zero economic profits',
                'efficiency': 'Not fully efficient due to excess capacity',
                'examples': 'Restaurants, retail clothing, personal services'
            },
            'oligopoly': {
                'number_of_firms': 'Few (typically 3-10)',
                'product_differentiation': 'May be homogeneous or differentiated',
                'barriers_to_entry': 'High',
                'pricing_power': 'Significant (price makers)',
                'long_run_profits': 'Positive economic profits possible',
                'efficiency': 'Generally inefficient, potential for collusion',
                'examples': 'Airlines, telecommunications, automobiles'
            },
            'monopoly': {
                'number_of_firms': 'One',
                'product_differentiation': 'Unique product (no close substitutes)',
                'barriers_to_entry': 'Very high or absolute',
                'pricing_power': 'Maximum (price maker)',
                'long_run_profits': 'Positive economic profits',
                'efficiency': 'Allocatively inefficient, may be productively efficient',
                'examples': 'Public utilities, patented drugs, natural monopolies'
            }
        }

        return characteristics.get(structure_type, {})

    def _analyze_concentration(self, hhi: Decimal, num_firms: int) -> Dict[str, Any]:
        """Analyze market concentration using various measures"""

        # Herfindahl-Hirschman Index interpretation
        if hhi > self.to_decimal(0.25):
            concentration_level = 'Highly Concentrated'
            antitrust_concern = 'High'
        elif hhi > self.to_decimal(0.15):
            concentration_level = 'Moderately Concentrated'
            antitrust_concern = 'Moderate'
        else:
            concentration_level = 'Unconcentrated'
            antitrust_concern = 'Low'

        # Calculate approximate market shares (simplified)
        if num_firms > 0:
            avg_market_share = self.to_decimal(1) / self.to_decimal(num_firms)
        else:
            avg_market_share = self.to_decimal(0)

        return {
            'herfindahl_index': hhi,
            'concentration_level': concentration_level,
            'antitrust_concern': antitrust_concern,
            'number_of_firms': num_firms,
            'average_market_share': avg_market_share * self.to_decimal(100),
            'concentration_interpretation': self._interpret_hhi(hhi)
        }

    def _interpret_hhi(self, hhi: Decimal) -> str:
        """Interpret HHI values"""
        if hhi > self.to_decimal(0.25):
            return "Market dominated by few large firms, potential monopoly power"
        elif hhi > self.to_decimal(0.15):
            return "Market moderately concentrated, some pricing power exists"
        elif hhi > self.to_decimal(0.1):
            return "Market somewhat concentrated, limited pricing power"
        else:
            return "Market unconcentrated, competitive pricing likely"

    def _analyze_competitive_dynamics(self, structure_type: str, market_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze competitive dynamics for each market structure"""

        dynamics = {
            'perfect_competition': {
                'competition_intensity': 'Maximum',
                'strategic_behavior': 'None (price takers)',
                'product_strategy': 'Focus on cost efficiency',
                'market_response': 'Immediate price adjustments',
                'long_term_strategy': 'Operational excellence'
            },
            'monopolistic_competition': {
                'competition_intensity': 'High but differentiated',
                'strategic_behavior': 'Product differentiation focus',
                'product_strategy': 'Brand building, innovation',
                'market_response': 'Gradual price and product adjustments',
                'long_term_strategy': 'Sustainable differentiation'
            },
            'oligopoly': {
                'competition_intensity': 'Strategic interdependence',
                'strategic_behavior': 'Game theory applies, potential collusion',
                'product_strategy': 'Innovation and differentiation',
                'market_response': 'Strategic reactions to competitors',
                'long_term_strategy': 'Market share protection'
            },
            'monopoly': {
                'competition_intensity': 'None or minimal',
                'strategic_behavior': 'Price and output optimization',
                'product_strategy': 'Innovation optional',
                'market_response': 'Independent pricing decisions',
                'long_term_strategy': 'Barrier maintenance'
            }
        }

        return dynamics.get(structure_type, {})

    def calculate_breakeven_shutdown_points(self, cost_data: Dict[str, Any],
                                            market_structure: str) -> Dict[str, Any]:
        """Calculate breakeven and shutdown points for different market structures"""

        # Extract cost data
        fixed_costs = self.to_decimal(cost_data.get('fixed_costs', 0))
        variable_cost_per_unit = self.to_decimal(cost_data.get('variable_cost_per_unit', 0))
        market_price = self.to_decimal(cost_data.get('market_price', 0))
        capacity = self.to_decimal(cost_data.get('capacity', 0))

        # Calculate key cost metrics
        total_costs = lambda q: fixed_costs + variable_cost_per_unit * q
        average_total_cost = lambda q: total_costs(q) / q if q > 0 else self.to_decimal(0)
        average_variable_cost = variable_cost_per_unit
        marginal_cost = variable_cost_per_unit  # Simplified assumption

        # Breakeven point (where economic profit = 0)
        if market_price > variable_cost_per_unit:
            breakeven_quantity = fixed_costs / (market_price - variable_cost_per_unit)
        else:
            breakeven_quantity = None  # Cannot break even

        # Shutdown point (where price < AVC)
        shutdown_price = average_variable_cost

        # Calculate optimal production for different market structures
        optimal_output = self._calculate_optimal_output(
            market_structure, market_price, marginal_cost, capacity
        )

        return {
            'cost_structure': {
                'fixed_costs': fixed_costs,
                'variable_cost_per_unit': variable_cost_per_unit,
                'average_variable_cost': average_variable_cost,
                'marginal_cost': marginal_cost
            },
            'breakeven_analysis': {
                'breakeven_quantity': breakeven_quantity,
                'breakeven_revenue': breakeven_quantity * market_price if breakeven_quantity else None,
                'contribution_margin': market_price - variable_cost_per_unit,
                'contribution_margin_ratio': ((market_price - variable_cost_per_unit) / market_price * self.to_decimal(
                    100)) if market_price > 0 else self.to_decimal(0)
            },
            'shutdown_analysis': {
                'shutdown_price': shutdown_price,
                'current_price': market_price,
                'should_shutdown': market_price < shutdown_price,
                'shutdown_loss': fixed_costs if market_price < shutdown_price else self.to_decimal(0)
            },
            'optimal_production': {
                'optimal_quantity': optimal_output,
                'total_revenue': optimal_output * market_price,
                'total_costs': total_costs(optimal_output),
                'economic_profit': optimal_output * market_price - total_costs(optimal_output)
            },
            'scale_economies': self._analyze_economies_of_scale(cost_data, optimal_output)
        }

    def _calculate_optimal_output(self, structure_type: str, price: Decimal,
                                  marginal_cost: Decimal, capacity: Decimal) -> Decimal:
        """Calculate optimal output for different market structures"""

        if structure_type == 'perfect_competition':
            # P = MC
            if price >= marginal_cost:
                return capacity  # Produce at capacity if profitable
            else:
                return self.to_decimal(0)  # Shutdown if price < MC

        elif structure_type == 'monopolistic_competition':
            # MR = MC, but with some pricing power
            # Simplified: produce where P > MC but not at full capacity
            if price > marginal_cost:
                return capacity * self.to_decimal(0.8)  # 80% of capacity
            else:
                return self.to_decimal(0)

        elif structure_type in ['oligopoly', 'monopoly']:
            # More complex optimization considering market power
            # Simplified: produce where MR = MC
            if price > marginal_cost * self.to_decimal(1.2):  # Account for markup
                return capacity * self.to_decimal(0.7)  # 70% of capacity
            else:
                return capacity * self.to_decimal(0.5)  # 50% of capacity

        else:
            return capacity * self.to_decimal(0.75)  # Default assumption

    def _analyze_economies_of_scale(self, cost_data: Dict[str, Any],
                                    current_output: Decimal) -> Dict[str, Any]:
        """Analyze economies and diseconomies of scale"""

        # Extract scale-related data
        min_efficient_scale = self.to_decimal(cost_data.get('min_efficient_scale', 0))
        capacity = self.to_decimal(cost_data.get('capacity', 0))

        # Determine scale position
        if current_output < min_efficient_scale:
            scale_position = 'Below minimum efficient scale'
            scale_effect = 'Economies of scale available'
            recommendation = 'Increase production to reduce average costs'
        elif current_output <= capacity * self.to_decimal(0.9):
            scale_position = 'At or near optimal scale'
            scale_effect = 'Constant returns to scale'
            recommendation = 'Current scale is efficient'
        else:
            scale_position = 'Above optimal scale'
            scale_effect = 'Potential diseconomies of scale'
            recommendation = 'Consider capacity expansion or efficiency improvements'

        return {
            'current_output': current_output,
            'minimum_efficient_scale': min_efficient_scale,
            'capacity_utilization': (
                        current_output / capacity * self.to_decimal(100)) if capacity > 0 else self.to_decimal(0),
            'scale_position': scale_position,
            'scale_effect': scale_effect,
            'recommendation': recommendation
        }

    def _analyze_pricing_behavior(self, structure_type: str, market_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze pricing behavior and strategies for each market structure"""

        pricing_behavior = {
            'perfect_competition': {
                'pricing_strategy': 'Price taking (no control)',
                'price_setting': 'Market determined',
                'demand_curve': 'Perfectly elastic',
                'profit_maximization': 'P = MC',
                'price_discrimination': 'Not possible'
            },
            'monopolistic_competition': {
                'pricing_strategy': 'Limited pricing power',
                'price_setting': 'Some control over price',
                'demand_curve': 'Downward sloping but elastic',
                'profit_maximization': 'MR = MC',
                'price_discrimination': 'Limited opportunities'
            },
            'oligopoly': {
                'pricing_strategy': 'Strategic pricing',
                'price_setting': 'Mutual interdependence',
                'demand_curve': 'Kinked demand curve possible',
                'profit_maximization': 'Game theory considerations',
                'price_discrimination': 'Possible with market segmentation'
            },
            'monopoly': {
                'pricing_strategy': 'Price maker',
                'price_setting': 'Full control subject to demand',
                'demand_curve': 'Downward sloping',
                'profit_maximization': 'MR = MC',
                'price_discrimination': 'Multiple types possible'
            }
        }

        return pricing_behavior.get(structure_type, {})

    def _analyze_efficiency_implications(self, structure_type: str) -> Dict[str, Any]:
        """Analyze efficiency implications of each market structure"""

        efficiency = {
            'perfect_competition': {
                'allocative_efficiency': 'Yes (P = MC)',
                'productive_efficiency': 'Yes (minimum ATC)',
                'dynamic_efficiency': 'Limited (low profits for R&D)',
                'consumer_surplus': 'Maximized',
                'deadweight_loss': 'None'
            },
            'monopolistic_competition': {
                'allocative_efficiency': 'No (P > MC)',
                'productive_efficiency': 'No (excess capacity)',
                'dynamic_efficiency': 'Moderate (innovation incentives)',
                'consumer_surplus': 'Reduced due to higher prices',
                'deadweight_loss': 'Small'
            },
            'oligopoly': {
                'allocative_efficiency': 'No (P > MC)',
                'productive_efficiency': 'Uncertain (may achieve scale economies)',
                'dynamic_efficiency': 'High (R&D competition)',
                'consumer_surplus': 'Significantly reduced',
                'deadweight_loss': 'Moderate to large'
            },
            'monopoly': {
                'allocative_efficiency': 'No (P > MC)',
                'productive_efficiency': 'Uncertain (may achieve scale economies)',
                'dynamic_efficiency': 'Low (limited competition pressure)',
                'consumer_surplus': 'Minimized',
                'deadweight_loss': 'Large'
            }
        }

        return efficiency.get(structure_type, {})

    def _get_regulatory_considerations(self, structure_type: str) -> Dict[str, Any]:
        """Get regulatory considerations for each market structure"""

        regulations = {
            'perfect_competition': {
                'antitrust_concern': 'None',
                'regulation_needed': 'Minimal',
                'focus': 'Maintain competitive conditions',
                'interventions': 'Prevent artificial barriers to entry'
            },
            'monopolistic_competition': {
                'antitrust_concern': 'Low',
                'regulation_needed': 'Light touch',
                'focus': 'Consumer protection, fair advertising',
                'interventions': 'Truth in advertising, quality standards'
            },
            'oligopoly': {
                'antitrust_concern': 'High',
                'regulation_needed': 'Active monitoring',
                'focus': 'Prevent collusion, monitor mergers',
                'interventions': 'Merger review, price fixing prevention'
            },
            'monopoly': {
                'antitrust_concern': 'Very high',
                'regulation_needed': 'Heavy regulation or breakup',
                'focus': 'Price regulation, service quality',
                'interventions': 'Rate regulation, structural remedies'
            }
        }

        return regulations.get(structure_type, {})

    def calculate(self, analysis_type: str = 'structure_identification', **kwargs) -> Dict[str, Any]:
        """Main market structure calculation dispatcher"""

        if analysis_type == 'structure_identification':
            return self.identify_market_structure(kwargs['market_data'])
        elif analysis_type == 'breakeven_shutdown':
            return self.calculate_breakeven_shutdown_points(
                kwargs['cost_data'], kwargs['market_structure']
            )
        else:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")