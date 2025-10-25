"""
Capital Flows and Foreign Exchange Analytics Module
===================================================

Comprehensive analysis of capital flows, foreign exchange markets, and exchange rate regimes. Implements CFA Institute curriculum for capital flow analysis, FX market structure, currency movements, and balance of payments dynamics.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Balance of payments statistics and current account data
  - Capital flow breakdowns (FDI, portfolio, other investments)
  - Foreign exchange market data and trading volumes
  - Exchange rate series and currency pair information
  - Interest rate differentials and monetary policy data
  - Central bank intervention and reserve data
  - Capital restriction and regulatory information

OUTPUT:
  - Capital flow analysis and sustainability assessments
  - FX market structure and participant analysis
  - Exchange rate regime evaluation and policy recommendations
  - Currency movement calculations and economic impact assessments
  - Balance of payments equilibrium analysis
  - Capital restriction effectiveness evaluations
  - Early warning indicators for currency crises

PARAMETERS:
  - fdi_inflows_gdp: FDI inflows as percentage of GDP
  - portfolio_flows_gdp: Portfolio flows as percentage of GDP
  - current_account_gdp: Current account balance as percentage of GDP
  - capital_account_gdp: Capital account balance as percentage of GDP
  - foreign_debt_gdp: External debt as percentage of GDP
  - reserves_months_imports: Foreign reserves in months of imports
  - daily_volume: Daily FX trading volume
  - nominal_rate: Nominal exchange rate
  - initial_rate: Initial exchange rate for change calculations
  - final_rate: Final exchange rate for change calculations
  - flow_data: Dictionary containing capital flow statistics
  - bop_data: Dictionary containing balance of payments data
  - restriction_data: Dictionary containing capital restriction information
"""

from decimal import Decimal
from typing import Dict, List, Any, Tuple
from datetime import datetime
from .core import EconomicsBase, ValidationError, CalculationError


class CapitalFlowAnalyzer(EconomicsBase):
    """Capital flows analysis and balance of payments impact"""

    def analyze_capital_flow_types(self, flow_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze different types of capital flows and their characteristics"""
        return {
            'foreign_direct_investment': {
                'definition': 'Long-term investment for control or significant influence (>10% ownership)',
                'characteristics': [
                    'Long-term commitment and stability',
                    'Technology and knowledge transfer',
                    'Management expertise and best practices',
                    'Difficult to reverse quickly'
                ],
                'economic_impact': {
                    'positive': 'Productivity gains, employment creation, export growth',
                    'negative': 'Potential crowding out of domestic investment',
                    'volatility': 'Low - stable funding source'
                },
                'current_flows': self._analyze_fdi_flows(flow_data),
                'policy_implications': 'Generally welcomed, policies focus on attraction and retention'
            },
            'portfolio_investment': {
                'definition': 'Investment in securities without control (<10% ownership)',
                'characteristics': [
                    'Liquid and easily reversible',
                    'Driven by return differentials and risk appetite',
                    'Sensitive to market sentiment',
                    'Includes equity and debt securities'
                ],
                'economic_impact': {
                    'positive': 'Capital market development, financing access',
                    'negative': 'Volatility and sudden stops risk',
                    'volatility': 'High - subject to rapid reversals'
                },
                'current_flows': self._analyze_portfolio_flows(flow_data),
                'policy_implications': 'Requires robust regulatory framework and macroprudential policies'
            },
            'other_investment': {
                'definition': 'Bank lending, trade credits, and other financial flows',
                'characteristics': [
                    'Includes bank loans and deposits',
                    'Trade finance and short-term credits',
                    'Interbank and intercompany lending',
                    'Often procyclical'
                ],
                'economic_impact': {
                    'positive': 'Trade finance facilitation, liquidity provision',
                    'negative': 'Banking sector vulnerabilities, sudden stops',
                    'volatility': 'Medium to High - depends on banking conditions'
                },
                'current_flows': self._analyze_other_flows(flow_data),
                'policy_implications': 'Banking supervision and capital flow management'
            },
            'official_flows': {
                'definition': 'Central bank and government transactions',
                'characteristics': [
                    'Reserve accumulation/depletion',
                    'Official development assistance',
                    'Bilateral government lending',
                    'IMF and multilateral lending'
                ],
                'economic_impact': {
                    'positive': 'Crisis support, development financing',
                    'negative': 'May create moral hazard',
                    'volatility': 'Low to Medium - policy driven'
                },
                'policy_implications': 'Part of macroeconomic management and development strategy'
            },
            'flow_determinants': self._analyze_flow_determinants(flow_data),
            'volatility_comparison': self._compare_flow_volatility(),
            'crisis_behavior': self._analyze_crisis_behavior()
        }

    def analyze_balance_of_payments_impact(self, bop_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze how BOP flows affect exchange rates"""
        return {
            'current_account_impact': {
                'trade_balance': {
                    'surplus_effect': 'Creates demand for domestic currency',
                    'deficit_effect': 'Creates supply of domestic currency',
                    'elasticity_considerations': 'J-curve effect in short run',
                    'current_balance': self._assess_trade_balance_impact(bop_data)
                },
                'income_flows': {
                    'investment_income': 'Returns on foreign investments affect currency demand',
                    'compensation': 'Worker remittances and cross-border wages',
                    'impact_assessment': self._assess_income_flows_impact(bop_data)
                },
                'transfers': {
                    'remittances': 'Significant for many developing countries',
                    'official_transfers': 'Aid and government transfers',
                    'impact_assessment': self._assess_transfer_impact(bop_data)
                }
            },
            'capital_account_impact': {
                'direct_investment': {
                    'fx_impact': 'Usually strengthens recipient currency',
                    'timing': 'Gradual impact as investments are made',
                    'sustainability': 'Most stable form of capital flow'
                },
                'portfolio_investment': {
                    'fx_impact': 'Can cause rapid currency movements',
                    'timing': 'Immediate impact on exchange rates',
                    'volatility': 'High sensitivity to sentiment changes'
                },
                'financial_derivatives': {
                    'fx_impact': 'Complex, depends on underlying positions',
                    'hedging_flows': 'May offset other capital flows'
                },
                'reserve_changes': {
                    'intervention_impact': 'Central bank buying/selling affects rates',
                    'signaling_effect': 'Indicates policy stance and credibility'
                }
            },
            'bop_equilibrium_analysis': self._analyze_bop_equilibrium(bop_data),
            'sustainability_assessment': self._assess_bop_sustainability(bop_data),
            'policy_responses': self._recommend_bop_policies(bop_data)
        }

    def assess_capital_restrictions(self, restriction_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze government capital restrictions and their objectives"""
        return {
            'restriction_types': {
                'inflow_controls': {
                    'objectives': [
                        'Prevent asset bubbles from hot money',
                        'Maintain monetary policy independence',
                        'Reduce financial stability risks',
                        'Prevent real exchange rate appreciation'
                    ],
                    'instruments': [
                        'Unremunerated reserve requirements',
                        'Taxes on foreign investment',
                        'Minimum holding periods',
                        'Limits on foreign ownership'
                    ],
                    'effectiveness': self._assess_inflow_control_effectiveness(restriction_data)
                },
                'outflow_controls': {
                    'objectives': [
                        'Prevent capital flight during crises',
                        'Preserve foreign exchange reserves',
                        'Maintain exchange rate stability',
                        'Support domestic financing needs'
                    ],
                    'instruments': [
                        'Approval requirements for foreign investment',
                        'Limits on foreign currency holdings',
                        'Restrictions on overseas deposits',
                        'Export surrender requirements'
                    ],
                    'effectiveness': self._assess_outflow_control_effectiveness(restriction_data)
                }
            },
            'common_objectives': {
                'macroeconomic_stability': 'Maintain stable exchange rates and inflation',
                'financial_stability': 'Prevent excessive risk-taking and bubbles',
                'monetary_independence': 'Preserve domestic monetary policy effectiveness',
                'development_goals': 'Channel capital toward productive investments',
                'crisis_prevention': 'Reduce vulnerability to sudden stops'
            },
            'effectiveness_factors': {
                'comprehensiveness': 'Controls must cover all relevant channels',
                'enforceability': 'Administrative capacity and compliance monitoring',
                'market_development': 'May hinder financial market development',
                'evasion_potential': 'Sophisticated investors can often circumvent controls',
                'international_coordination': 'Effectiveness increases with coordination'
            },
            'costs_and_benefits': self._analyze_restriction_costs_benefits(),
            'optimal_design_principles': self._recommend_optimal_design(),
            'current_trends': self._analyze_current_restriction_trends()
        }

    def _analyze_fdi_flows(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze FDI flow characteristics"""
        fdi_inflows = self.to_decimal(data.get('fdi_inflows_gdp', 0))
        fdi_outflows = self.to_decimal(data.get('fdi_outflows_gdp', 0))

        return {
            'inflow_level': f"{fdi_inflows:.1f}% of GDP",
            'outflow_level': f"{fdi_outflows:.1f}% of GDP",
            'net_position': f"{fdi_inflows - fdi_outflows:.1f}% of GDP",
            'assessment': self._assess_fdi_level(fdi_inflows),
            'sectoral_distribution': data.get('fdi_sectors', 'Mixed across sectors')
        }

    def _analyze_portfolio_flows(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze portfolio flow characteristics"""
        portfolio_flows = self.to_decimal(data.get('portfolio_flows_gdp', 0))
        volatility = data.get('portfolio_volatility', 'High')

        return {
            'flow_level': f"{portfolio_flows:.1f}% of GDP",
            'volatility_assessment': volatility,
            'composition': data.get('portfolio_composition', 'Mixed equity and debt'),
            'vulnerability_indicator': self._assess_portfolio_vulnerability(portfolio_flows)
        }

    def _analyze_other_flows(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze other investment flows"""
        other_flows = self.to_decimal(data.get('other_investment_gdp', 0))

        return {
            'flow_level': f"{other_flows:.1f}% of GDP",
            'banking_component': data.get('banking_flows_share', 'Significant'),
            'trade_finance_component': data.get('trade_finance_share', 'Moderate'),
            'stability_assessment': self._assess_other_flow_stability(other_flows)
        }

    def _analyze_flow_determinants(self, data: Dict[str, Any]) -> Dict[str, List[str]]:
        """Analyze determinants of capital flows"""
        return {
            'push_factors': [
                'Global risk appetite and liquidity conditions',
                'Advanced economy interest rates',
                'Global growth and commodity prices',
                'Investor risk tolerance'
            ],
            'pull_factors': [
                'Domestic economic fundamentals',
                'Interest rate differentials',
                'Exchange rate expectations',
                'Political and institutional quality',
                'Market development and accessibility'
            ],
            'structural_factors': [
                'Trade openness and integration',
                'Financial market development',
                'Capital account openness',
                'Institutional quality and governance'
            ]
        }

    def _compare_flow_volatility(self) -> Dict[str, str]:
        """Compare volatility across flow types"""
        return {
            'most_volatile': 'Portfolio investment (especially equity)',
            'moderately_volatile': 'Other investment (banking flows)',
            'least_volatile': 'Foreign direct investment',
            'crisis_behavior': 'Portfolio flows show strongest sudden stop tendency'
        }

    def _analyze_crisis_behavior(self) -> Dict[str, str]:
        """Analyze capital flow behavior during crises"""
        return {
            'sudden_stops': 'Rapid reversal of portfolio and banking flows',
            'flight_to_quality': 'Shift from emerging to developed markets',
            'fdi_resilience': 'FDI typically more stable during crises',
            'contagion_channels': 'Capital flows can transmit crises across countries'
        }

    def _assess_trade_balance_impact(self, data: Dict[str, Any]) -> str:
        """Assess trade balance impact on exchange rates"""
        trade_balance = self.to_decimal(data.get('trade_balance_gdp', 0))

        if trade_balance > self.to_decimal(2):
            return 'Large surplus likely supporting currency'
        elif trade_balance < self.to_decimal(-5):
            return 'Large deficit creating downward pressure on currency'
        else:
            return 'Moderate trade balance with limited FX impact'

    def _assess_income_flows_impact(self, data: Dict[str, Any]) -> str:
        """Assess income flows impact"""
        income_balance = self.to_decimal(data.get('income_balance_gdp', 0))

        if income_balance > self.to_decimal(1):
            return 'Positive income flows supporting currency'
        elif income_balance < self.to_decimal(-2):
            return 'Negative income flows pressuring currency'
        else:
            return 'Income flows have moderate impact'

    def _assess_transfer_impact(self, data: Dict[str, Any]) -> str:
        """Assess transfer impact on currency"""
        transfers = self.to_decimal(data.get('transfers_gdp', 0))

        if transfers > self.to_decimal(3):
            return 'Significant remittances providing currency support'
        else:
            return 'Transfers have limited currency impact'

    def _analyze_bop_equilibrium(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze balance of payments equilibrium"""
        current_account = self.to_decimal(data.get('current_account_gdp', 0))
        capital_account = self.to_decimal(data.get('capital_account_gdp', 0))

        return {
            'current_account_balance': f"{current_account:.1f}% of GDP",
            'capital_account_balance': f"{capital_account:.1f}% of GDP",
            'overall_balance': f"{current_account + capital_account:.1f}% of GDP",
            'equilibrium_assessment': self._assess_bop_equilibrium_status(current_account, capital_account),
            'reserve_implications': self._assess_reserve_implications(current_account + capital_account)
        }

    def _assess_bop_sustainability(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess BOP sustainability"""
        current_account = self.to_decimal(data.get('current_account_gdp', 0))
        foreign_debt = self.to_decimal(data.get('foreign_debt_gdp', 0))

        return {
            'current_account_sustainability': self._assess_ca_sustainability(current_account),
            'external_debt_sustainability': self._assess_debt_sustainability(foreign_debt),
            'vulnerability_indicators': self._identify_vulnerability_indicators(data),
            'early_warning_signals': self._identify_early_warning_signals(data)
        }

    def _recommend_bop_policies(self, data: Dict[str, Any]) -> List[str]:
        """Recommend BOP adjustment policies"""
        current_account = self.to_decimal(data.get('current_account_gdp', 0))

        if current_account < self.to_decimal(-5):
            return [
                'Fiscal consolidation to reduce domestic absorption',
                'Structural reforms to improve competitiveness',
                'Exchange rate adjustment if overvalued',
                'Capital flow management measures if needed'
            ]
        elif current_account > self.to_decimal(5):
            return [
                'Fiscal expansion to increase domestic demand',
                'Infrastructure investment to utilize surplus',
                'Currency appreciation to restore balance',
                'Gradual capital account liberalization'
            ]
        else:
            return [
                'Maintain current policy stance',
                'Monitor for emerging imbalances',
                'Strengthen economic fundamentals'
            ]

    def _assess_inflow_control_effectiveness(self, data: Dict[str, Any]) -> str:
        """Assess effectiveness of capital inflow controls"""
        control_intensity = data.get('inflow_control_index', 0.5)

        if control_intensity > 0.7:
            return 'Comprehensive controls - moderately effective but may reduce efficiency'
        elif control_intensity > 0.3:
            return 'Selective controls - limited effectiveness, some circumvention'
        else:
            return 'Minimal controls - market-based allocation but potential volatility'

    def _assess_outflow_control_effectiveness(self, data: Dict[str, Any]) -> str:
        """Assess effectiveness of capital outflow controls"""
        control_intensity = data.get('outflow_control_index', 0.5)

        if control_intensity > 0.7:
            return 'Strict controls - effective short-term but high economic costs'
        elif control_intensity > 0.3:
            return 'Moderate controls - some effectiveness with manageable costs'
        else:
            return 'Light controls - limited effectiveness but preserves market efficiency'

    def _analyze_restriction_costs_benefits(self) -> Dict[str, Dict[str, List[str]]]:
        """Analyze costs and benefits of capital restrictions"""
        return {
            'benefits': {
                'macroeconomic': ['Exchange rate stability', 'Monetary policy independence', 'Reduced volatility'],
                'financial': ['Reduced systemic risk', 'Prevented asset bubbles', 'Banking stability'],
                'developmental': ['Capital allocated to development priorities', 'Reduced inequality']
            },
            'costs': {
                'efficiency': ['Reduced capital allocation efficiency', 'Higher cost of capital',
                               'Innovation constraints'],
                'market_development': ['Slower financial market development', 'Reduced competition',
                                       'Limited diversification'],
                'administrative': ['High enforcement costs', 'Bureaucratic burden', 'Corruption risks']
            }
        }

    def _recommend_optimal_design(self) -> List[str]:
        """Recommend optimal design principles for capital controls"""
        return [
            'Targeted rather than blanket restrictions',
            'Temporary rather than permanent measures',
            'Price-based rather than quantity-based controls',
            'Comprehensive coverage to prevent evasion',
            'Regular review and adjustment of measures',
            'Clear communication of objectives and duration'
        ]

    def _analyze_current_restriction_trends(self) -> Dict[str, str]:
        """Analyze current trends in capital restrictions"""
        return {
            'developing_countries': 'Increased use of macroprudential measures',
            'developed_countries': 'Generally maintain open capital accounts',
            'crisis_response': 'Temporary restrictions during financial stress',
            'international_coordination': 'Growing recognition of spillover effects',
            'institutional_view': 'IMF more accepting of capital flow management'
        }

    def _assess_fdi_level(self, fdi_inflows: Decimal) -> str:
        """Assess FDI inflow level"""
        if fdi_inflows > self.to_decimal(5):
            return 'High FDI inflows indicating strong investment climate'
        elif fdi_inflows > self.to_decimal(2):
            return 'Moderate FDI inflows'
        else:
            return 'Low FDI inflows, may indicate investment barriers'

    def _assess_portfolio_vulnerability(self, flows: Decimal) -> str:
        """Assess portfolio flow vulnerability"""
        if abs(flows) > self.to_decimal(5):
            return 'High vulnerability to sudden stops'
        elif abs(flows) > self.to_decimal(2):
            return 'Moderate vulnerability'
        else:
            return 'Low vulnerability to portfolio flow reversals'

    def _assess_other_flow_stability(self, flows: Decimal) -> str:
        """Assess other investment flow stability"""
        if abs(flows) > self.to_decimal(3):
            return 'Volatile other investment flows'
        else:
            return 'Relatively stable other investment flows'

    def _assess_bop_equilibrium_status(self, ca: Decimal, ka: Decimal) -> str:
        """Assess BOP equilibrium status"""
        overall = ca + ka

        if abs(overall) < self.to_decimal(1):
            return 'Balanced position'
        elif overall > self.to_decimal(2):
            return 'Surplus position - reserve accumulation'
        else:
            return 'Deficit position - reserve depletion or borrowing'

    def _assess_reserve_implications(self, balance: Decimal) -> str:
        """Assess reserve implications of BOP position"""
        if balance > self.to_decimal(2):
            return 'Reserve accumulation, potential sterilization needs'
        elif balance < self.to_decimal(-2):
            return 'Reserve depletion, potential sustainability concerns'
        else:
            return 'Stable reserve position'

    def _assess_ca_sustainability(self, ca: Decimal) -> str:
        """Assess current account sustainability"""
        if ca < self.to_decimal(-5):
            return 'Large deficit raises sustainability concerns'
        elif ca < self.to_decimal(-3):
            return 'Moderate deficit requires monitoring'
        else:
            return 'Sustainable current account position'

    def _assess_debt_sustainability(self, debt: Decimal) -> str:
        """Assess external debt sustainability"""
        if debt > self.to_decimal(60):
            return 'High external debt raises sustainability concerns'
        elif debt > self.to_decimal(40):
            return 'Moderate external debt requires monitoring'
        else:
            return 'Manageable external debt level'

    def _identify_vulnerability_indicators(self, data: Dict[str, Any]) -> List[str]:
        """Identify BOP vulnerability indicators"""
        return [
            'Current account deficit > 5% of GDP',
            'Short-term external debt > reserves',
            'High dependence on volatile capital flows',
            'Real exchange rate overvaluation',
            'Rapid credit growth and asset price increases'
        ]

    def _identify_early_warning_signals(self, data: Dict[str, Any]) -> List[str]:
        """Identify early warning signals of BOP crisis"""
        return [
            'Sudden stop in capital inflows',
            'Rapid reserve depletion',
            'Currency under pressure',
            'Rising sovereign risk premiums',
            'Bank deposit outflows'
        ]

    def calculate(self, analysis_type: str = 'capital_flows', **kwargs) -> Dict[str, Any]:
        """Main capital flows calculation dispatcher"""
        analyses = {
            'capital_flows': lambda: self.analyze_capital_flow_types(kwargs.get('flow_data', {})),
            'bop_impact': lambda: self.analyze_balance_of_payments_impact(kwargs.get('bop_data', {})),
            'capital_restrictions': lambda: self.assess_capital_restrictions(kwargs.get('restriction_data', {}))
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        return result


class FXMarketAnalyzer(EconomicsBase):
    """Foreign exchange market structure and functionality analysis"""

    def analyze_fx_market_structure(self, market_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze foreign exchange market functions and participants"""
        return {
            'market_functions': {
                'price_discovery': {
                    'description': 'Determining exchange rates through supply and demand',
                    'mechanism': 'Continuous trading by global participants',
                    'efficiency': 'Generally efficient due to high liquidity and participation',
                    'factors': ['Economic fundamentals', 'Market sentiment', 'Technical factors']
                },
                'risk_management': {
                    'description': 'Hedging currency exposure for businesses and investors',
                    'instruments': ['Spot transactions', 'Forward contracts', 'Options', 'Swaps'],
                    'participants': 'Multinational corporations, banks, institutional investors',
                    'importance': 'Critical for international trade and investment'
                },
                'speculation': {
                    'description': 'Profit-seeking from currency movements',
                    'participants': 'Hedge funds, proprietary traders, retail investors',
                    'impact': 'Provides liquidity but can increase volatility',
                    'regulation': 'Subject to various regulatory constraints'
                },
                'arbitrage': {
                    'description': 'Exploiting price differences across markets',
                    'types': ['Spatial arbitrage', 'Triangular arbitrage', 'Covered interest arbitrage'],
                    'function': 'Ensures price consistency across markets',
                    'technology_role': 'High-frequency trading dominates arbitrage'
                }
            },
            'market_participants': self._analyze_market_participants(market_data),
            'market_structure': self._analyze_market_microstructure(market_data),
            'trading_mechanisms': self._analyze_trading_mechanisms(),
            'liquidity_analysis': self._analyze_market_liquidity(market_data)
        }

    def distinguish_nominal_real_rates(self, rate_data: Dict[str, Any]) -> Dict[str, Any]:
        """Distinguish between nominal and real exchange rates"""
        return {
            'nominal_exchange_rate': {
                'definition': 'Price of one currency in terms of another currency',
                'example': '1 USD = 1.20 EUR (Euro per US Dollar)',
                'characteristics': [
                    'Directly observable in markets',
                    'Used for actual transactions',
                    'Affected by monetary policy and market sentiment',
                    'Can be quoted as direct or indirect'
                ],
                'calculation': 'Market determined through trading',
                'current_rate': rate_data.get('nominal_rate', 'N/A')
            },
            'real_exchange_rate': {
                'definition': 'Nominal rate adjusted for price level differences',
                'formula': 'Real Rate = Nominal Rate × (Foreign Price Level / Domestic Price Level)',
                'characteristics': [
                    'Measures relative purchasing power',
                    'Indicates competitiveness',
                    'Not directly tradeable',
                    'Important for trade flows'
                ],
                'calculation': self._calculate_real_exchange_rate(rate_data),
                'interpretation': self._interpret_real_rate_changes(rate_data)
            },
            'relationship_analysis': {
                'short_run': 'Nominal and real rates can diverge significantly',
                'long_run': 'Tend to move together due to purchasing power parity',
                'policy_implications': 'Real rates matter more for trade competitiveness',
                'investment_relevance': 'Both rates important for different investment decisions'
            },
            'practical_applications': self._describe_rate_applications()
        }

    def calculate_currency_percentage_change(self, change_data: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate and interpret currency percentage changes"""
        initial_rate = self.to_decimal(change_data.get('initial_rate', 1))
        final_rate = self.to_decimal(change_data.get('final_rate', 1))
        base_currency = change_data.get('base_currency', 'USD')
        quote_currency = change_data.get('quote_currency', 'EUR')
        quote_convention = change_data.get('quote_convention', 'direct')

        # Calculate percentage change
        percentage_change = ((final_rate - initial_rate) / initial_rate) * self.to_decimal(100)

        # Determine currency movement
        if quote_convention == 'direct':
            # Direct quote: domestic currency per unit of foreign currency
            # Increase means domestic currency weakening
            if percentage_change > 0:
                movement = f"{base_currency} weakened by {percentage_change:.2f}%"
                description = f"{quote_currency} appreciated against {base_currency}"
            else:
                movement = f"{base_currency} strengthened by {abs(percentage_change):.2f}%"
                description = f"{quote_currency} depreciated against {base_currency}"
        else:
            # Indirect quote: foreign currency per unit of domestic currency
            # Increase means domestic currency strengthening
            if percentage_change > 0:
                movement = f"{base_currency} strengthened by {percentage_change:.2f}%"
                description = f"{base_currency} appreciated against {quote_currency}"
            else:
                movement = f"{base_currency} weakened by {abs(percentage_change):.2f}%"
                description = f"{base_currency} depreciated against {quote_currency}"

        return {
            'calculation_details': {
                'initial_rate': initial_rate,
                'final_rate': final_rate,
                'absolute_change': final_rate - initial_rate,
                'percentage_change': percentage_change,
                'quote_convention': quote_convention
            },
            'currency_movement': {
                'summary': movement,
                'detailed_description': description,
                'direction': 'appreciation' if percentage_change > 0 else 'depreciation',
                'magnitude': self._assess_change_magnitude(abs(percentage_change))
            },
            'economic_implications': self._analyze_currency_change_implications(
                percentage_change, base_currency, quote_currency
            ),
            'trade_impact': self._assess_trade_impact(percentage_change, quote_convention),
            'investment_implications': self._assess_investment_implications(percentage_change)
        }

    def _analyze_market_participants(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze FX market participants"""
        return {
            'commercial_banks': {
                'role': 'Market makers and dealers',
                'market_share': '~75% of daily volume',
                'functions': ['Provide liquidity', 'Client transactions', 'Proprietary trading'],
                'importance': 'Core of interbank market'
            },
            'central_banks': {
                'role': 'Policy implementation and intervention',
                'market_share': '~5% of daily volume',
                'functions': ['Monetary policy', 'Reserve management', 'Market intervention'],
                'impact': 'Significant influence despite small volume'
            },
            'institutional_investors': {
                'role': 'Hedging and investment',
                'market_share': '~10% of daily volume',
                'participants': ['Pension funds', 'Mutual funds', 'Insurance companies'],
                'motivation': 'Risk management and portfolio optimization'
            },
            'hedge_funds': {
                'role': 'Speculation and arbitrage',
                'market_share': '~5% of daily volume',
                'strategies': ['Carry trades', 'Momentum', 'Mean reversion'],
                'impact': 'High influence on short-term volatility'
            },
            'corporations': {
                'role': 'Commercial hedging',
                'market_share': '~3% of daily volume',
                'needs': ['Trade settlement', 'Risk hedging', 'Cash management'],
                'patterns': 'Often predictable timing'
            },
            'retail_traders': {
                'role': 'Small-scale speculation',
                'market_share': '~2% of daily volume',
                'access': 'Through brokers and online platforms',
                'characteristics': 'High leverage, short-term focus'
            }
        }

    def _analyze_market_microstructure(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze FX market microstructure"""
        return {
            'market_type': 'Over-the-counter (OTC) decentralized market',
            'trading_hours': '24 hours, 5 days a week across global time zones',
            'major_centers': ['London (43%)', 'New York (17%)', 'Singapore (8%)', 'Tokyo (7%)'],
            'market_size': data.get('daily_volume', '$7.5 trillion daily volume'),
            'concentration': 'Top 10 banks account for ~75% of volume',
            'electronic_trading': '~95% of transactions are electronic',
            'settlement': 'T+2 standard settlement cycle'
        }

    def _analyze_trading_mechanisms(self) -> Dict[str, Any]:
        """Analyze FX trading mechanisms"""
        return {
            'spot_market': {
                'definition': 'Immediate delivery (T+2 settlement)',
                'characteristics': 'Highest liquidity, benchmark for other rates',
                'participants': 'All market participants',
                'pricing': 'Continuous price discovery'
            },
            'forward_market': {
                'definition': 'Future delivery at predetermined rate',
                'characteristics': 'Customizable terms, no upfront payment',
                'participants': 'Banks, corporations, institutional investors',
                'pricing': 'Based on interest rate differentials'
            },
            'futures_market': {
                'definition': 'Standardized forward contracts on exchanges',
                'characteristics': 'Margin requirements, daily mark-to-market',
                'participants': 'Speculators, hedgers, arbitrageurs',
                'pricing': 'Exchange-determined, transparent'
            },
            'options_market': {
                'definition': 'Right but not obligation to exchange currencies',
                'characteristics': 'Premium payment, asymmetric payoff',
                'participants': 'Sophisticated institutional investors',
                'pricing': 'Based on volatility and time value'
            },
            'swap_market': {
                'definition': 'Combination of spot and forward transactions',
                'characteristics': 'Manages liquidity without FX risk',
                'participants': 'Central banks, commercial banks',
                'pricing': 'Interest rate differential based'
            }
        }

    def _analyze_market_liquidity(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze FX market liquidity"""
        return {
            'liquidity_measures': {
                'bid_ask_spreads': data.get('avg_spread_bps', '1-3 basis points for major pairs'),
                'market_depth': 'High depth due to large participant base',
                'resilience': 'Quick recovery from temporary imbalances',
                'immediacy': 'Instant execution for standard sizes'
            },
            'liquidity_hierarchy': {
                'tier_1': 'EUR/USD, USD/JPY, GBP/USD (most liquid)',
                'tier_2': 'USD/CHF, AUD/USD, USD/CAD',
                'tier_3': 'Cross rates between major currencies',
                'tier_4': 'Emerging market currencies (lower liquidity)'
            },
            'factors_affecting_liquidity': [
                'Time of day (overlap of major centers)',
                'Economic news and events',
                'Market volatility and uncertainty',
                'Regulatory changes',
                'Central bank interventions'
            ],
            'liquidity_risk': {
                'normal_times': 'Minimal liquidity risk for major pairs',
                'stress_periods': 'Can experience temporary liquidity shortages',
                'emerging_markets': 'Higher liquidity risk, especially during crises'
            }
        }

    def _calculate_real_exchange_rate(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate real exchange rate"""
        nominal_rate = self.to_decimal(data.get('nominal_rate', 1))
        domestic_cpi = self.to_decimal(data.get('domestic_price_level', 100))
        foreign_cpi = self.to_decimal(data.get('foreign_price_level', 100))

        real_rate = nominal_rate * (foreign_cpi / domestic_cpi)

        return {
            'real_exchange_rate': real_rate,
            'calculation': f"{nominal_rate} × ({foreign_cpi}/{domestic_cpi}) = {real_rate}",
            'interpretation': self._interpret_real_rate_level(real_rate, data.get('historical_average', 1))
        }

    def _interpret_real_rate_changes(self, data: Dict[str, Any]) -> str:
        """Interpret real exchange rate changes"""
        real_rate_change = self.to_decimal(data.get('real_rate_change_percent', 0))

        if real_rate_change > self.to_decimal(5):
            return 'Significant real appreciation - loss of competitiveness'
        elif real_rate_change < self.to_decimal(-5):
            return 'Significant real depreciation - gain in competitiveness'
        else:
            return 'Moderate real exchange rate change'

    def _interpret_real_rate_level(self, current_rate: Decimal, historical_avg: float) -> str:
        """Interpret real exchange rate level"""
        historical = self.to_decimal(historical_avg)
        deviation = ((current_rate - historical) / historical) * self.to_decimal(100)

        if deviation > self.to_decimal(10):
            return 'Real exchange rate appears overvalued'
        elif deviation < self.to_decimal(-10):
            return 'Real exchange rate appears undervalued'
        else:
            return 'Real exchange rate near historical average'

    def _describe_rate_applications(self) -> Dict[str, str]:
        """Describe practical applications of nominal vs real rates"""
        return {
            'nominal_rates': 'Used for actual currency transactions, hedging, and short-term speculation',
            'real_rates': 'Used for competitiveness analysis, long-term investment decisions, and trade policy',
            'portfolio_management': 'Nominal rates for immediate hedging, real rates for strategic allocation',
            'trade_analysis': 'Real rates better predict trade flow changes over time',
            'central_bank_policy': 'Both rates considered, real rates for competitiveness assessment'
        }

    def _assess_change_magnitude(self, abs_change: Decimal) -> str:
        """Assess magnitude of currency change"""
        if abs_change > self.to_decimal(10):
            return 'Major currency movement'
        elif abs_change > self.to_decimal(5):
            return 'Significant currency movement'
        elif abs_change > self.to_decimal(2):
            return 'Moderate currency movement'
        else:
            return 'Minor currency movement'

    def _analyze_currency_change_implications(self, change: Decimal, base: str, quote: str) -> Dict[str, str]:
        """Analyze economic implications of currency changes"""
        return {
            'trade_balance': 'Depreciation improves trade balance over time (J-curve effect)',
            'inflation': 'Depreciation can increase import price inflation',
            'competitiveness': 'Depreciation improves export competitiveness',
            'debt_burden': 'Depreciation increases foreign currency debt burden',
            'tourism': 'Depreciation makes country more attractive to foreign tourists',
            'investment_flows': 'Large changes may trigger capital flow reversals'
        }

    def _assess_trade_impact(self, change: Decimal, convention: str) -> str:
        """Assess trade impact of currency change"""
        if convention == 'direct':
            if change > self.to_decimal(5):
                return 'Currency weakness should improve trade balance over 12-18 months'
            elif change < self.to_decimal(-5):
                return 'Currency strength may worsen trade balance'
            else:
                return 'Limited impact on trade balance expected'
        else:
            if change > self.to_decimal(5):
                return 'Currency strength may worsen trade balance'
            elif change < self.to_decimal(-5):
                return 'Currency weakness should improve trade balance over 12-18 months'
            else:
                return 'Limited impact on trade balance expected'

    def _assess_investment_implications(self, change: Decimal) -> List[str]:
        """Assess investment implications of currency changes"""
        implications = []

        if abs(change) > self.to_decimal(5):
            implications.extend([
                'Significant impact on foreign investment returns',
                'May trigger portfolio rebalancing by international investors',
                'Hedging strategies should be reviewed'
            ])

        if change > self.to_decimal(10):
            implications.append('Large appreciation may deter foreign direct investment')
        elif change < self.to_decimal(-10):
            implications.append('Large depreciation may attract foreign direct investment')

        return implications

    def calculate(self, analysis_type: str = 'market_structure', **kwargs) -> Dict[str, Any]:
        """Main FX market calculation dispatcher"""
        analyses = {
            'market_structure': lambda: self.analyze_fx_market_structure(kwargs.get('market_data', {})),
            'nominal_real_rates': lambda: self.distinguish_nominal_real_rates(kwargs.get('rate_data', {})),
            'percentage_change': lambda: self.calculate_currency_percentage_change(kwargs.get('change_data', {}))
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        return result


class ExchangeRegimeAnalyzer(EconomicsBase):
    """Exchange rate regime analysis and policy implications"""

    def analyze_exchange_rate_regimes(self, regime_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze different exchange rate regimes and their effects"""
        return {
            'fixed_exchange_rate': {
                'definition': 'Currency pegged to another currency or basket',
                'characteristics': [
                    'Minimal exchange rate volatility',
                    'Requires central bank intervention',
                    'Limited monetary policy independence',
                    'Vulnerable to speculative attacks'
                ],
                'advantages': [
                    'Reduces transaction costs for trade',
                    'Provides nominal anchor for inflation',
                    'Reduces exchange rate uncertainty',
                    'Facilitates international investment'
                ],
                'disadvantages': [
                    'Loss of monetary policy independence',
                    'Requires large foreign exchange reserves',
                    'Vulnerable to balance of payments crises',
                    'May lead to real exchange rate misalignment'
                ],
                'examples': ['Hong Kong Dollar', 'Danish Krone', 'Gulf States'],
                'sustainability_factors': self._assess_fixed_regime_sustainability(regime_data)
            },
            'floating_exchange_rate': {
                'definition': 'Currency value determined by market forces',
                'characteristics': [
                    'High exchange rate volatility',
                    'Automatic adjustment mechanism',
                    'Full monetary policy independence',
                    'Requires developed financial markets'
                ],
                'advantages': [
                    'Monetary policy independence',
                    'Automatic adjustment to shocks',
                    'No need for large reserves',
                    'Reduces moral hazard in lending'
                ],
                'disadvantages': [
                    'Exchange rate volatility and uncertainty',
                    'May complicate international trade',
                    'Potential for destabilizing speculation',
                    'Pass-through to domestic prices'
                ],
                'examples': ['US Dollar', 'Euro', 'Japanese Yen', 'British Pound'],
                'effectiveness_factors': self._assess_floating_regime_effectiveness(regime_data)
            },
            'managed_float': {
                'definition': 'Market determination with occasional intervention',
                'characteristics': [
                    'Moderate exchange rate volatility',
                    'Discretionary intervention',
                    'Some monetary policy independence',
                    'Requires judgment on intervention timing'
                ],
                'advantages': [
                    'Balances flexibility and stability',
                    'Allows gradual adjustment',
                    'Retains some policy independence',
                    'Can prevent excessive volatility'
                ],
                'disadvantages': [
                    'Uncertainty about intervention policy',
                    'May delay necessary adjustments',
                    'Requires significant expertise',
                    'Potential for policy mistakes'
                ],
                'examples': ['Chinese Yuan', 'Indian Rupee', 'Brazilian Real'],
                'success_factors': self._identify_managed_float_success_factors()
            },
            'currency_union': {
                'definition': 'Countries share common currency',
                'characteristics': [
                    'No exchange rate within union',
                    'Common monetary policy',
                    'Requires fiscal coordination',
                    'Irreversible commitment'
                ],
                'advantages': [
                    'Eliminates exchange rate risk within union',
                    'Reduces transaction costs',
                    'Promotes trade and investment',
                    'Provides credible commitment'
                ],
                'disadvantages': [
                    'Loss of national monetary policy',
                    'Asymmetric shock vulnerability',
                    'Requires fiscal transfers or flexibility',
                    'Difficult exit mechanism'
                ],
                'examples': ['Eurozone', 'West African CFA Franc'],
                'optimum_currency_area_criteria': self._assess_oca_criteria(regime_data)
            },
            'regime_choice_factors': self._analyze_regime_choice_factors(),
            'trade_capital_flow_effects': self._analyze_regime_effects_on_flows(regime_data)
        }

    def _assess_fixed_regime_sustainability(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess sustainability factors for fixed exchange rate regime"""
        return {
            'foreign_reserves': {
                'level': data.get('reserves_months_imports', 'N/A'),
                'adequacy': 'Should cover 3-6 months of imports',
                'assessment': self._assess_reserve_adequacy(data.get('reserves_months_imports', 3))
            },
            'fiscal_position': {
                'deficit': data.get('fiscal_deficit_gdp', 'N/A'),
                'debt': data.get('government_debt_gdp', 'N/A'),
                'sustainability': 'Fiscal discipline critical for credibility'
            },
            'current_account': {
                'balance': data.get('current_account_gdp', 'N/A'),
                'sustainability': 'Large deficits threaten sustainability'
            },
            'political_commitment': {
                'importance': 'Strong political will essential',
                'indicators': ['Central bank independence', 'Policy consistency', 'Reform commitment']
            }
        }

    def _assess_floating_regime_effectiveness(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess effectiveness factors for floating exchange rate regime"""
        return {
            'financial_market_development': {
                'depth': data.get('financial_market_depth_index', 'N/A'),
                'importance': 'Deep markets reduce volatility',
                'requirements': ['Large participant base', 'Diverse instruments', 'Good regulation']
            },
            'institutional_quality': {
                'central_bank_credibility': data.get('cb_credibility_index', 'N/A'),
                'importance': 'Credible monetary policy anchors expectations',
                'factors': ['Independence', 'Transparency', 'Track record']
            },
            'pass_through_management': {
                'inflation_targeting': 'Helps manage pass-through effects',
                'communication': 'Clear policy communication important',
                'credibility': 'Credible commitment to low inflation'
            }
        }

    def _identify_managed_float_success_factors(self) -> List[str]:
        """Identify success factors for managed float regimes"""
        return [
            'Clear intervention objectives and communication',
            'Adequate foreign exchange reserves',
            'Flexible fiscal and monetary policies',
            'Well-developed financial markets',
            'Strong institutional capacity',
            'Appropriate intervention timing and scale'
        ]

    def _assess_oca_criteria(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess optimum currency area criteria"""
        return {
            'labor_mobility': {
                'assessment': data.get('labor_mobility_index', 'Low'),
                'importance': 'High mobility helps adjustment to asymmetric shocks',
                'barriers': ['Language differences', 'Cultural factors', 'Regulatory barriers']
            },
            'trade_integration': {
                'level': data.get('intra_union_trade_share', 'N/A'),
                'importance': 'High trade integration reduces asymmetric shocks',
                'measurement': 'Share of trade within currency union'
            },
            'business_cycle_synchronization': {
                'correlation': data.get('business_cycle_correlation', 'N/A'),
                'importance': 'Synchronized cycles reduce need for independent policy',
                'factors': ['Similar economic structures', 'Common shocks', 'Policy coordination']
            },
            'fiscal_transfers': {
                'mechanism': data.get('fiscal_transfer_mechanism', 'Limited'),
                'importance': 'Transfers help adjustment to asymmetric shocks',
                'examples': ['Federal systems', 'EU structural funds', 'Automatic stabilizers']
            },
            'price_wage_flexibility': {
                'level': data.get('price_wage_flexibility_index', 'N/A'),
                'importance': 'Flexibility substitutes for exchange rate adjustment',
                'barriers': ['Labor market rigidities', 'Price stickiness', 'Regulatory constraints']
            }
        }

    def _analyze_regime_choice_factors(self) -> Dict[str, List[str]]:
        """Analyze factors influencing exchange rate regime choice"""
        return {
            'economic_factors': [
                'Size and openness of economy',
                'Trade pattern and partner concentration',
                'Financial market development',
                'Inflation history and credibility',
                'Fiscal position and discipline'
            ],
            'institutional_factors': [
                'Central bank independence and credibility',
                'Political stability and consensus',
                'Administrative capacity',
                'Legal and regulatory framework',
                'International integration'
            ],
            'external_factors': [
                'International capital mobility',
                'Regional integration arrangements',
                'Major trading partner regimes',
                'Global financial conditions',
                'International monetary system'
            ]
        }

    def _analyze_regime_effects_on_flows(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze effects of exchange rate regimes on trade and capital flows"""
        return {
            'trade_effects': {
                'fixed_regime': {
                    'trade_volume': 'Generally higher due to reduced uncertainty',
                    'trade_composition': 'May favor short-term over long-term contracts',
                    'price_competitiveness': 'May lead to misalignment over time'
                },
                'floating_regime': {
                    'trade_volume': 'May be lower due to exchange rate risk',
                    'trade_composition': 'Encourages hedging and risk management',
                    'price_competitiveness': 'Maintains competitiveness through adjustment'
                }
            },
            'capital_flow_effects': {
                'fixed_regime': {
                    'portfolio_flows': 'May attract flows but vulnerable to sudden stops',
                    'fdi_flows': 'Reduced exchange rate risk may encourage FDI',
                    'speculative_flows': 'Vulnerable to one-way bets against the peg'
                },
                'floating_regime': {
                    'portfolio_flows': 'More volatile but self-correcting',
                    'fdi_flows': 'Exchange rate risk may deter some investment',
                    'speculative_flows': 'Two-way risk reduces speculative pressure'
                }
            },
            'crisis_vulnerability': {
                'fixed_regime': 'High vulnerability to balance of payments crises',
                'floating_regime': 'Lower crisis probability but higher volatility',
                'managed_float': 'Intermediate vulnerability depending on credibility'
            }
        }

    def _assess_reserve_adequacy(self, months_imports: float) -> str:
        """Assess foreign reserve adequacy"""
        if months_imports >= 6:
            return 'Adequate reserves for fixed regime'
        elif months_imports >= 3:
            return 'Borderline adequate reserves'
        else:
            return 'Insufficient reserves for sustainable fixed regime'

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Calculate exchange rate regime analysis"""
        result = self.analyze_exchange_rate_regimes(kwargs.get('regime_data', {}))
        result['metadata'] = self.get_metadata()
        return result