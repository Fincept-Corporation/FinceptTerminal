
"""Economic Policy Analysis Module
=============================

Economic policy impact assessment

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


"""

from decimal import Decimal
from typing import Dict, List, Any
from .core import EconomicsBase, ValidationError


class FiscalPolicyAnalyzer(EconomicsBase):
    """Fiscal policy analysis and impact assessment"""

    def compare_fiscal_monetary(self) -> Dict[str, Any]:
        """Compare fiscal and monetary policy characteristics"""
        return {
            'fiscal_policy': {
                'authority': 'Government (legislative/executive)',
                'tools': ['Government spending', 'Taxation', 'Transfer payments'],
                'targets': ['Economic growth', 'Employment', 'Income distribution'],
                'transmission': 'Direct impact on aggregate demand',
                'lag_time': 'Long (6-18 months)',
                'political_influence': 'High',
                'flexibility': 'Low (requires legislative approval)'
            },
            'monetary_policy': {
                'authority': 'Central bank',
                'tools': ['Interest rates', 'Money supply', 'Reserve requirements'],
                'targets': ['Price stability', 'Economic growth', 'Financial stability'],
                'transmission': 'Indirect through financial markets',
                'lag_time': 'Medium (3-12 months)',
                'political_influence': 'Low (independent)',
                'flexibility': 'High (quick implementation)'
            },
            'interaction_effects': {
                'complementary': 'Both expansionary during recession',
                'conflicting': 'Fiscal expansion with monetary tightening',
                'coordination_importance': 'Critical for policy effectiveness'
            }
        }

    def analyze_fiscal_tools(self, policy_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze fiscal policy tools and their effects"""
        tools_analysis = {
            'government_spending': {
                'multiplier_effect': self._calculate_spending_multiplier(policy_data),
                'advantages': ['Direct job creation', 'Infrastructure investment', 'Quick stimulus'],
                'disadvantages': ['Crowding out private investment', 'Debt accumulation', 'Political interference'],
                'effectiveness': 'High during recessions, moderate during expansions'
            },
            'taxation': {
                'multiplier_effect': self._calculate_tax_multiplier(policy_data),
                'advantages': ['Broad-based impact', 'Revenue generation', 'Incentive alignment'],
                'disadvantages': ['Lagged response', 'Political constraints', 'Distortionary effects'],
                'effectiveness': 'Moderate, depends on tax type and economic conditions'
            },
            'transfer_payments': {
                'multiplier_effect': self._calculate_transfer_multiplier(policy_data),
                'advantages': ['Targeted support', 'Automatic stabilizers', 'Social safety net'],
                'disadvantages': ['Potential dependency', 'Fiscal burden', 'Limited growth impact'],
                'effectiveness': 'High for consumption support, moderate for growth'
            }
        }

        return {
            'tools_analysis': tools_analysis,
            'implementation_challenges': self._assess_implementation_challenges(),
            'policy_recommendation': self._recommend_fiscal_mix(policy_data)
        }

    def assess_debt_sustainability(self, debt_data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess whether national debt relative to GDP matters"""
        debt_gdp = self.to_decimal(debt_data.get('debt_to_gdp_ratio', 0))
        gdp_growth = self.to_decimal(debt_data.get('gdp_growth_rate', 0))
        interest_rate = self.to_decimal(debt_data.get('avg_interest_rate', 0))
        primary_balance = self.to_decimal(debt_data.get('primary_balance_gdp', 0))

        # Debt sustainability condition: r < g + primary_balance_ratio
        sustainability_gap = interest_rate - gdp_growth - primary_balance

        # Risk thresholds
        if debt_gdp > self.to_decimal(100):
            risk_level = 'Very High'
        elif debt_gdp > self.to_decimal(60):
            risk_level = 'High'
        elif debt_gdp > self.to_decimal(40):
            risk_level = 'Moderate'
        else:
            risk_level = 'Low'

        return {
            'debt_to_gdp': debt_gdp,
            'sustainability_gap': sustainability_gap,
            'sustainable': sustainability_gap < self.to_decimal(0),
            'risk_level': risk_level,
            'debt_dynamics': {
                'interest_burden': interest_rate * debt_gdp / self.to_decimal(100),
                'growth_benefit': gdp_growth * debt_gdp / self.to_decimal(100),
                'primary_contribution': primary_balance
            },
            'implications': self._get_debt_implications(debt_gdp, sustainability_gap)
        }

    def identify_policy_stance(self, fiscal_indicators: Dict[str, Any]) -> Dict[str, Any]:
        """Identify if fiscal policy is expansionary or contractionary"""
        spending_change = self.to_decimal(fiscal_indicators.get('spending_change_percent', 0))
        tax_change = self.to_decimal(fiscal_indicators.get('tax_change_percent', 0))
        deficit_change = self.to_decimal(fiscal_indicators.get('deficit_change_gdp', 0))

        # Calculate fiscal impulse
        fiscal_impulse = spending_change - tax_change

        if fiscal_impulse > self.to_decimal(1):
            stance = 'Expansionary'
            description = 'Government increasing spending more than taxes'
        elif fiscal_impulse < self.to_decimal(-1):
            stance = 'Contractionary'
            description = 'Government reducing spending or increasing taxes significantly'
        else:
            stance = 'Neutral'
            description = 'Minimal net fiscal impact'

        return {
            'fiscal_stance': stance,
            'fiscal_impulse': fiscal_impulse,
            'description': description,
            'stance_indicators': {
                'spending_change': spending_change,
                'tax_change': tax_change,
                'deficit_change': deficit_change
            },
            'economic_impact': self._assess_stance_impact(stance, fiscal_impulse)
        }

    def _calculate_spending_multiplier(self, data: Dict[str, Any]) -> Decimal:
        """Calculate government spending multiplier"""
        mpc = self.to_decimal(data.get('marginal_propensity_consume', 0.8))
        return self.to_decimal(1) / (self.to_decimal(1) - mpc)

    def _calculate_tax_multiplier(self, data: Dict[str, Any]) -> Decimal:
        """Calculate tax multiplier"""
        mpc = self.to_decimal(data.get('marginal_propensity_consume', 0.8))
        return -mpc / (self.to_decimal(1) - mpc)

    def _calculate_transfer_multiplier(self, data: Dict[str, Any]) -> Decimal:
        """Calculate transfer payment multiplier"""
        mpc = self.to_decimal(data.get('marginal_propensity_consume', 0.8))
        return mpc / (self.to_decimal(1) - mpc)

    def _assess_implementation_challenges(self) -> List[str]:
        """Assess fiscal policy implementation difficulties"""
        return [
            'Recognition lag: Time to identify economic problems',
            'Legislative lag: Time for political approval',
            'Implementation lag: Time to execute policy',
            'Political constraints: Electoral and partisan considerations',
            'Crowding out: Government borrowing affects private investment',
            'Ricardian equivalence: Tax cuts offset by expected future taxes'
        ]

    def _recommend_fiscal_mix(self, data: Dict[str, Any]) -> Dict[str, str]:
        """Recommend optimal fiscal policy mix"""
        unemployment = self.to_decimal(data.get('unemployment_rate', 0))
        inflation = self.to_decimal(data.get('inflation_rate', 0))

        if unemployment > self.to_decimal(7):
            return {'recommendation': 'Expansionary', 'focus': 'Job creation and demand stimulus'}
        elif inflation > self.to_decimal(4):
            return {'recommendation': 'Contractionary', 'focus': 'Reduce demand pressures'}
        else:
            return {'recommendation': 'Neutral', 'focus': 'Maintain fiscal balance'}

    def _get_debt_implications(self, debt_gdp: Decimal, gap: Decimal) -> Dict[str, str]:
        """Get implications of debt sustainability analysis"""
        if gap > self.to_decimal(2):
            return {
                'fiscal_space': 'Limited',
                'interest_burden': 'High and rising',
                'policy_flexibility': 'Constrained',
                'investor_confidence': 'At risk'
            }
        else:
            return {
                'fiscal_space': 'Adequate',
                'interest_burden': 'Manageable',
                'policy_flexibility': 'Available',
                'investor_confidence': 'Stable'
            }

    def _assess_stance_impact(self, stance: str, impulse: Decimal) -> Dict[str, str]:
        """Assess economic impact of fiscal stance"""
        impacts = {
            'Expansionary': {
                'gdp_impact': 'Positive stimulus to growth',
                'employment_impact': 'Job creation likely',
                'inflation_risk': 'Potential upward pressure',
                'debt_impact': 'Increased deficit spending'
            },
            'Contractionary': {
                'gdp_impact': 'Negative drag on growth',
                'employment_impact': 'Potential job losses',
                'inflation_risk': 'Reduced price pressures',
                'debt_impact': 'Deficit reduction'
            },
            'Neutral': {
                'gdp_impact': 'Minimal direct impact',
                'employment_impact': 'Status quo maintained',
                'inflation_risk': 'No significant pressure',
                'debt_impact': 'Stable debt dynamics'
            }
        }
        return impacts.get(stance, {})

    def calculate(self, analysis_type: str = 'tools_analysis', **kwargs) -> Dict[str, Any]:
        """Main fiscal policy calculation dispatcher"""
        analyses = {
            'compare_policies': self.compare_fiscal_monetary,
            'tools_analysis': lambda: self.analyze_fiscal_tools(kwargs.get('policy_data', {})),
            'debt_sustainability': lambda: self.assess_debt_sustainability(kwargs.get('debt_data', {})),
            'policy_stance': lambda: self.identify_policy_stance(kwargs.get('fiscal_indicators', {}))
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        return result


class MonetaryPolicyAnalyzer(EconomicsBase):
    """Monetary policy analysis and transmission mechanism"""

    def analyze_central_bank_roles(self) -> Dict[str, Any]:
        """Describe central bank roles and objectives"""
        return {
            'primary_objectives': {
                'price_stability': 'Maintain low and stable inflation',
                'economic_growth': 'Support sustainable economic expansion',
                'financial_stability': 'Ensure stable financial system',
                'employment': 'Some central banks have explicit employment mandate'
            },
            'key_functions': {
                'monetary_policy': 'Set interest rates and control money supply',
                'banking_supervision': 'Regulate and supervise financial institutions',
                'lender_of_last_resort': 'Provide emergency liquidity to banks',
                'currency_issuance': 'Issue and manage national currency',
                'government_banker': 'Provide banking services to government'
            },
            'independence_importance': {
                'political_independence': 'Avoid short-term political pressures',
                'operational_independence': 'Freedom to choose policy tools',
                'accountability': 'Report to legislature on performance'
            }
        }

    def analyze_monetary_tools(self, policy_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze monetary policy tools and transmission mechanism"""
        return {
            'conventional_tools': {
                'policy_rate': {
                    'description': 'Central bank key interest rate',
                    'current_rate': policy_data.get('policy_rate', 'N/A'),
                    'transmission': 'Affects all market rates',
                    'effectiveness': 'High when rates above zero lower bound'
                },
                'reserve_requirements': {
                    'description': 'Banks required reserve ratio',
                    'current_ratio': policy_data.get('reserve_ratio', 'N/A'),
                    'transmission': 'Affects bank lending capacity',
                    'effectiveness': 'Powerful but rarely used'
                },
                'open_market_operations': {
                    'description': 'Buy/sell government securities',
                    'current_balance_sheet': policy_data.get('central_bank_balance_sheet', 'N/A'),
                    'transmission': 'Direct impact on money supply',
                    'effectiveness': 'Most frequently used tool'
                }
            },
            'unconventional_tools': {
                'quantitative_easing': 'Large-scale asset purchases',
                'forward_guidance': 'Communication about future policy',
                'negative_rates': 'Below-zero policy rates',
                'yield_curve_control': 'Target specific maturity yields'
            },
            'transmission_mechanism': self._analyze_transmission_mechanism(policy_data)
        }

    def analyze_targeting_strategies(self, strategy_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze different monetary policy targeting strategies"""
        strategies = {
            'inflation_targeting': {
                'target': strategy_data.get('inflation_target', '2%'),
                'advantages': ['Clear communication', 'Credible commitment', 'Flexible response'],
                'disadvantages': ['Ignores other variables', 'May miss asset bubbles', 'Measurement issues'],
                'effectiveness': 'High for anchoring expectations'
            },
            'interest_rate_targeting': {
                'target': strategy_data.get('interest_rate_target', 'Variable'),
                'advantages': ['Direct control', 'Clear signal', 'Quick transmission'],
                'disadvantages': ['May ignore inflation', 'Procyclical risks', 'Zero lower bound'],
                'effectiveness': 'High for short-term stabilization'
            },
            'exchange_rate_targeting': {
                'target': strategy_data.get('exchange_rate_target', 'N/A'),
                'advantages': ['Trade stability', 'Import price stability', 'Simple communication'],
                'disadvantages': ['Loss of monetary independence', 'Vulnerable to attacks', 'Limited flexibility'],
                'effectiveness': 'Moderate, depends on economic structure'
            }
        }

        return {
            'targeting_strategies': strategies,
            'strategy_comparison': self._compare_targeting_strategies(),
            'optimal_strategy_recommendation': self._recommend_targeting_strategy(strategy_data)
        }

    def assess_policy_effectiveness(self, effectiveness_data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess monetary policy effectiveness and limitations"""
        return {
            'effectiveness_factors': {
                'central_bank_credibility': effectiveness_data.get('credibility_index', 'N/A'),
                'financial_system_development': effectiveness_data.get('financial_development_index', 'N/A'),
                'economic_structure': effectiveness_data.get('economic_structure', 'N/A'),
                'inflation_expectations_anchoring': effectiveness_data.get('expectations_anchored', 'N/A')
            },
            'policy_limitations': {
                'zero_lower_bound': 'Cannot cut rates below certain level',
                'liquidity_trap': 'Money demand becomes perfectly elastic',
                'long_and_variable_lags': 'Policy effects take 6-18 months',
                'asset_bubbles': 'Difficulty identifying and responding to bubbles',
                'financial_stability': 'Trade-offs between price and financial stability'
            },
            'effectiveness_assessment': self._assess_current_effectiveness(effectiveness_data)
        }

    def analyze_policy_interaction(self, interaction_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze interaction between monetary and fiscal policy"""
        fiscal_stance = interaction_data.get('fiscal_stance', 'neutral')
        monetary_stance = interaction_data.get('monetary_stance', 'neutral')

        interaction_matrix = {
            ('expansionary', 'expansionary'): {
                'coordination': 'Aligned',
                'economic_impact': 'Strong stimulus',
                'risks': 'Overheating, inflation',
                'appropriate_when': 'Deep recession'
            },
            ('expansionary', 'contractionary'): {
                'coordination': 'Conflicting',
                'economic_impact': 'Uncertain, depends on relative strength',
                'risks': 'Policy ineffectiveness',
                'appropriate_when': 'Fiscal stimulus with inflation concerns'
            },
            ('contractionary', 'expansionary'): {
                'coordination': 'Conflicting',
                'economic_impact': 'Uncertain, mixed signals',
                'risks': 'Policy confusion',
                'appropriate_when': 'Fiscal consolidation with growth support'
            },
            ('contractionary', 'contractionary'): {
                'coordination': 'Aligned',
                'economic_impact': 'Strong contraction',
                'risks': 'Excessive slowdown',
                'appropriate_when': 'High inflation, overheating'
            }
        }

        current_interaction = interaction_matrix.get((fiscal_stance, monetary_stance), {
            'coordination': 'Unknown',
            'economic_impact': 'Uncertain',
            'risks': 'Unknown',
            'appropriate_when': 'Unclear'
        })

        return {
            'current_policy_mix': {
                'fiscal_stance': fiscal_stance,
                'monetary_stance': monetary_stance
            },
            'interaction_analysis': current_interaction,
            'coordination_quality': self._assess_coordination_quality(interaction_data),
            'policy_recommendations': self._recommend_policy_coordination(fiscal_stance, monetary_stance)
        }

    def _analyze_transmission_mechanism(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze monetary policy transmission channels"""
        return {
            'interest_rate_channel': {
                'mechanism': 'Policy rate → Market rates → Investment/Consumption',
                'strength': 'Strong in developed economies',
                'lag': '6-12 months'
            },
            'credit_channel': {
                'mechanism': 'Policy → Bank lending → Economic activity',
                'strength': 'Important for bank-dependent economies',
                'lag': '3-9 months'
            },
            'exchange_rate_channel': {
                'mechanism': 'Policy rate → Exchange rate → Net exports',
                'strength': 'Strong in open economies',
                'lag': '3-6 months'
            },
            'asset_price_channel': {
                'mechanism': 'Policy → Asset prices → Wealth → Consumption',
                'strength': 'Important with developed capital markets',
                'lag': '6-18 months'
            },
            'expectations_channel': {
                'mechanism': 'Policy communication → Expectations → Decisions',
                'strength': 'Critical for all economies',
                'lag': 'Immediate to 3 months'
            }
        }

    def _compare_targeting_strategies(self) -> Dict[str, Any]:
        """Compare different targeting strategies"""
        return {
            'flexibility_ranking': ['Inflation targeting', 'Interest rate targeting', 'Exchange rate targeting'],
            'credibility_ranking': ['Exchange rate targeting', 'Inflation targeting', 'Interest rate targeting'],
            'transparency_ranking': ['Inflation targeting', 'Exchange rate targeting', 'Interest rate targeting'],
            'current_popularity': 'Inflation targeting most widely adopted'
        }

    def _recommend_targeting_strategy(self, data: Dict[str, Any]) -> str:
        """Recommend optimal targeting strategy"""
        openness = data.get('trade_openness', 0.5)
        inflation_volatility = data.get('inflation_volatility', 0.02)

        if openness > 0.7 and inflation_volatility > 0.05:
            return 'Exchange rate targeting for trade-dependent economy'
        elif inflation_volatility > 0.03:
            return 'Inflation targeting for price stability'
        else:
            return 'Flexible inflation targeting with growth consideration'

    def _assess_current_effectiveness(self, data: Dict[str, Any]) -> str:
        """Assess current monetary policy effectiveness"""
        policy_rate = self.to_decimal(data.get('policy_rate', 2))
        inflation_expectations = data.get('expectations_anchored', True)

        if policy_rate < self.to_decimal(0.5) and not inflation_expectations:
            return 'Low effectiveness - at zero lower bound with unanchored expectations'
        elif policy_rate < self.to_decimal(0.5):
            return 'Moderate effectiveness - limited by zero lower bound'
        elif not inflation_expectations:
            return 'Moderate effectiveness - limited by unanchored expectations'
        else:
            return 'High effectiveness - conventional policy space available'

    def _assess_coordination_quality(self, data: Dict[str, Any]) -> str:
        """Assess quality of fiscal-monetary coordination"""
        coordination_score = data.get('coordination_index', 0.5)

        if coordination_score > 0.8:
            return 'Excellent coordination'
        elif coordination_score > 0.6:
            return 'Good coordination'
        elif coordination_score > 0.4:
            return 'Moderate coordination'
        else:
            return 'Poor coordination'

    def _recommend_policy_coordination(self, fiscal: str, monetary: str) -> List[str]:
        """Recommend improvements to policy coordination"""
        if fiscal == monetary:
            return ['Maintain current alignment', 'Monitor for potential overshooting']
        else:
            return [
                'Improve communication between authorities',
                'Clarify policy objectives and timing',
                'Consider joint policy statements'
            ]

    def calculate(self, analysis_type: str = 'tools_analysis', **kwargs) -> Dict[str, Any]:
        """Main monetary policy calculation dispatcher"""
        analyses = {
            'central_bank_roles': self.analyze_central_bank_roles,
            'tools_analysis': lambda: self.analyze_monetary_tools(kwargs.get('policy_data', {})),
            'targeting_strategies': lambda: self.analyze_targeting_strategies(kwargs.get('strategy_data', {})),
            'effectiveness_assessment': lambda: self.assess_policy_effectiveness(kwargs.get('effectiveness_data', {})),
            'policy_interaction': lambda: self.analyze_policy_interaction(kwargs.get('interaction_data', {}))
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        return result


class CentralBankAnalyzer(EconomicsBase):
    """Central bank effectiveness and quality analysis"""

    def assess_central_bank_quality(self, cb_data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess qualities of effective central banks"""
        quality_metrics = {
            'independence': {
                'score': self.to_decimal(cb_data.get('independence_index', 0.5)),
                'components': ['Political independence', 'Operational independence', 'Financial independence'],
                'importance': 'Critical for credibility and long-term focus'
            },
            'transparency': {
                'score': self.to_decimal(cb_data.get('transparency_index', 0.5)),
                'components': ['Clear communication', 'Regular reporting', 'Decision explanations'],
                'importance': 'Essential for expectation management'
            },
            'accountability': {
                'score': self.to_decimal(cb_data.get('accountability_index', 0.5)),
                'components': ['Legislative oversight', 'Performance reporting', 'Public scrutiny'],
                'importance': 'Democratic legitimacy and performance monitoring'
            },
            'technical_competence': {
                'score': self.to_decimal(cb_data.get('competence_index', 0.5)),
                'components': ['Staff expertise', 'Research capability', 'Analysis quality'],
                'importance': 'Effective policy design and implementation'
            }
        }

        overall_quality = sum(metric['score'] for metric in quality_metrics.values()) / self.to_decimal(4)

        return {
            'quality_metrics': quality_metrics,
            'overall_quality_score': overall_quality,
            'effectiveness_rating': self._rate_effectiveness(overall_quality),
            'improvement_recommendations': self._recommend_improvements(quality_metrics)
        }

    def _rate_effectiveness(self, score: Decimal) -> str:
        """Rate central bank effectiveness"""
        if score > self.to_decimal(0.8):
            return 'Highly Effective'
        elif score > self.to_decimal(0.6):
            return 'Effective'
        elif score > self.to_decimal(0.4):
            return 'Moderately Effective'
        else:
            return 'Needs Improvement'

    def _recommend_improvements(self, metrics: Dict[str, Any]) -> List[str]:
        """Recommend improvements based on quality metrics"""
        recommendations = []

        for metric, data in metrics.items():
            if data['score'] < self.to_decimal(0.6):
                if metric == 'independence':
                    recommendations.append('Strengthen legal framework for central bank independence')
                elif metric == 'transparency':
                    recommendations.append('Improve communication strategy and public reporting')
                elif metric == 'accountability':
                    recommendations.append('Enhance oversight mechanisms and performance targets')
                elif metric == 'technical_competence':
                    recommendations.append('Invest in staff training and research capabilities')

        return recommendations

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Calculate central bank quality assessment"""
        result = self.assess_central_bank_quality(kwargs.get('cb_data', {}))
        result['metadata'] = self.get_metadata()
        return result