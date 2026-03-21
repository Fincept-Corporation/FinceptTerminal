"""equity_indexed_annuities Module"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class EquityIndexedAnnuityAnalyzer(AlternativeInvestmentBase):
    """
    Equity-Indexed Annuity (EIA) Analyzer
    Also called Fixed Index Annuities (FIA)

    CFA Standards: Derivatives - Embedded Options, Insurance Products

    Key Concepts from Key insight: - Marketed as "upside of stocks, downside protection of bonds"
    - Complex option structure limits upside participation
    - Participation rates, caps, spreads reduce returns
    - High commissions (5-10%) create conflicts of interest
    - Surrender charges lock up money (7-15 years)
    - Better alternatives exist

    Verdict: "THE BAD" - Complex, expensive, mis-sold
    Among the worst financial products for consumers
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.premium = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('100000')

        # Index crediting parameters
        self.participation_rate = parameters.participation_rate if hasattr(parameters, 'participation_rate') else Decimal('0.80')  # 80%
        self.cap_rate = parameters.cap_rate if hasattr(parameters, 'cap_rate') else Decimal('0.06')  # 6% annual cap
        self.floor_rate = parameters.floor_rate if hasattr(parameters, 'floor_rate') else Decimal('0.0')  # 0% floor
        self.spread = parameters.spread if hasattr(parameters, 'spread') else Decimal('0.0')  # Sometimes used instead of participation rate

        # Contract terms
        self.term_years = parameters.term_years if hasattr(parameters, 'term_years') else 7
        self.surrender_charge_years = parameters.surrender_charge_years if hasattr(parameters, 'surrender_charge_years') else 7
        self.initial_surrender_charge = parameters.initial_surrender_charge if hasattr(parameters, 'initial_surrender_charge') else Decimal('0.10')  # 10%

        # Insurance company
        self.insurer = parameters.insurer if hasattr(parameters, 'insurer') else 'Insurance Company'
        self.insurer_rating = parameters.insurer_rating if hasattr(parameters, 'insurer_rating') else 'A'

    def calculate_credited_return(self, index_return: Decimal) -> Dict[str, Any]:
        """
        Calculate credited return based on index performance

        Key insight: This is where EIA complexity hides losses
        Formula varies by product type:
        - Point-to-Point: Return capped and reduced by participation rate
        - Monthly Average: Uses average vs point-to-point (reduces return)
        - Monthly Cap: Caps monthly returns (very limiting)

        Args:
            index_return: Actual S&P 500 return

        Returns:
            Credited return analysis
        """
        # Method 1: Point-to-Point with participation rate and cap
        if index_return < self.floor_rate:
            credited_return_basic = self.floor_rate
        else:
            participated_return = index_return * self.participation_rate
            credited_return_basic = min(participated_return, self.cap_rate)

        # Method 2: With spread (alternative structure)
        if self.spread > 0:
            spread_adjusted = max(self.floor_rate, index_return - self.spread)
            credited_return_spread = min(spread_adjusted, self.cap_rate)
        else:
            credited_return_spread = credited_return_basic

        # Upside capture
        upside_capture = credited_return_basic / index_return if index_return > 0 else Decimal('0')

        return {
            'index_return': float(index_return),
            'participation_rate': float(self.participation_rate),
            'cap_rate': float(self.cap_rate),
            'floor_rate': float(self.floor_rate),
            'spread': float(self.spread),
            'credited_return': float(credited_return_basic),
            'upside_capture_pct': float(upside_capture),
            'upside_given_up': float(index_return - credited_return_basic),
            'interpretation': self._interpret_crediting(index_return, credited_return_basic, upside_capture)
        }

    def _interpret_crediting(self, index_ret: Decimal, credited: Decimal, capture: Decimal) -> str:
        """Interpret crediting outcome"""
        if index_ret <= self.floor_rate:
            return f'Floor protection activated - received {float(self.floor_rate):.1%} vs {float(index_ret):.1%} index loss'
        elif credited == self.cap_rate:
            return f'Hit cap - received {float(self.cap_rate):.1%} vs {float(index_ret):.1%} index gain (capped)'
        else:
            return f'Partial participation - captured {float(capture):.0%} of upside'

    def upside_limitation_analysis(self, market_scenarios: List[Decimal]) -> Dict[str, Any]:
        """
        Analyze upside limitation across various market scenarios

        Key insight: The cap, participation rate, and spread dramatically limit returns
        In strong bull markets, EIA holders miss most gains

        Args:
            market_scenarios: List of potential S&P 500 returns

        Returns:
            Upside limitation analysis
        """
        results = []

        for scenario in market_scenarios:
            credited = self.calculate_credited_return(scenario)
            difference = scenario - Decimal(str(credited['credited_return']))

            results.append({
                'index_return': float(scenario),
                'credited_return': credited['credited_return'],
                'upside_captured': credited['upside_capture_pct'],
                'upside_lost': float(difference),
                'outcome': 'Capped' if credited['credited_return'] == float(self.cap_rate) else
                          'Floor' if scenario < self.floor_rate else 'Partial'
            })

        # Calculate average upside capture
        avg_capture = sum(r['upside_captured'] for r in results) / len(results) if results else 0

        return {
            'crediting_parameters': {
                'participation_rate': float(self.participation_rate),
                'cap_rate': float(self.cap_rate),
                'spread': float(self.spread)
            },
            'scenarios': results,
            'average_upside_capture': avg_capture,
            'analysis_criticism': f'On average, EIA captures only {avg_capture:.0%} of market upside - massive opportunity cost'
        }

    def surrender_charge_schedule(self) -> Dict[str, Any]:
        """
        Generate surrender charge schedule

        Key insight: Surrender charges lock up money and create huge exit costs
        Typical: 10% year 1, declining 1-2% per year to zero after 7-10 years

        Returns:
            Surrender charge schedule
        """
        schedule = []
        decline_per_year = self.initial_surrender_charge / Decimal(str(self.surrender_charge_years))

        for year in range(1, self.surrender_charge_years + 2):
            if year <= self.surrender_charge_years:
                charge = self.initial_surrender_charge - (decline_per_year * Decimal(str(year - 1)))
                charge = max(Decimal('0'), charge)
            else:
                charge = Decimal('0')

            # Calculate dollar amount assuming growth
            assumed_value = self.premium * (Decimal('1.04') ** Decimal(str(year - 1)))  # 4% assumed growth
            charge_dollar = assumed_value * charge

            schedule.append({
                'year': year,
                'surrender_charge_pct': float(charge),
                'assumed_value': float(assumed_value),
                'charge_dollar': float(charge_dollar),
                'net_if_surrendered': float(assumed_value - charge_dollar)
            })

        return {
            'initial_premium': float(self.premium),
            'initial_surrender_charge': float(self.initial_surrender_charge),
            'surrender_period_years': self.surrender_charge_years,
            'schedule': schedule,
            'analysis_warning': 'Surrender charges trap your money - early exit costs thousands'
        }

    def commission_impact_analysis(self, typical_commission: Decimal = Decimal('0.07')) -> Dict[str, Any]:
        """
        Analyze impact of sales commission

        Key insight: EIA commissions are HUGE - 5-10% of premium
        This creates massive conflict of interest
        Salesperson gets paid whether product is good for you or not

        Args:
            typical_commission: Typical commission rate (default 7%)

        Returns:
            Commission analysis
        """
        commission_dollar = self.premium * typical_commission

        # This commission must be recouped from participant
        # How? Lower crediting rates, caps, fees

        # Estimate years to recoup commission (assuming 1% annual margin)
        annual_margin = Decimal('0.01')
        years_to_recoup = typical_commission / annual_margin

        return {
            'premium': float(self.premium),
            'commission_rate': float(typical_commission),
            'commission_dollar': float(commission_dollar),
            'years_to_recoup': float(years_to_recoup),
            'annual_cost_to_participant': float(self.premium * annual_margin),
            'conflict_of_interest': {
                'salesperson_incentive': f'${float(commission_dollar):,.0f} paid upfront',
                'participant_cost': 'Lower returns via caps, participation rates, spreads',
                'alignment': 'ZERO - salesperson wins even if participant loses',
                'analysis_conclusion': 'This commission structure creates inherent conflict - product sold, not bought'
            }
        }

    def compare_to_direct_index(self, index_returns: List[Decimal],
                                dividend_yield: Decimal = Decimal('0.02')) -> Dict[str, Any]:
        """
        Compare EIA to direct S&P 500 index fund investment

        Key insight: Direct index ownership vastly superior
        - Full upside participation
        - Dividends included
        - Lower costs (0.03% vs hidden 1-2%)
        - Liquidity (no surrender charges)
        - Transparency

        Args:
            index_returns: Historical S&P 500 returns (price only)
            dividend_yield: Average dividend yield

        Returns:
            Comparison analysis
        """
        if not index_returns:
            return {'error': 'No index returns provided'}

        years = len(index_returns)

        # EIA outcome
        eia_wealth = self.premium
        eia_yearly = []

        for year, index_ret in enumerate(index_returns, 1):
            # Calculate credited return
            credited = self.calculate_credited_return(index_ret)['credited_return']
            eia_wealth = eia_wealth * (Decimal('1') + Decimal(str(credited)))

            eia_yearly.append({
                'year': year,
                'index_return': float(index_ret),
                'credited_return': credited,
                'eia_value': float(eia_wealth)
            })

        # Direct index outcome (with dividends reinvested)
        index_wealth = self.premium
        index_yearly = []

        for year, index_ret in enumerate(index_returns, 1):
            total_return = index_ret + dividend_yield
            index_wealth = index_wealth * (Decimal('1') + total_return)

            index_yearly.append({
                'year': year,
                'total_return': float(total_return),
                'index_value': float(index_wealth)
            })

        # Final comparison
        wealth_difference = index_wealth - eia_wealth
        eia_cagr = (eia_wealth / self.premium) ** (Decimal('1') / Decimal(str(years))) - Decimal('1')
        index_cagr = (index_wealth / self.premium) ** (Decimal('1') / Decimal(str(years))) - Decimal('1')

        return {
            'initial_investment': float(self.premium),
            'years': years,
            'eia_outcome': {
                'final_value': float(eia_wealth),
                'cagr': float(eia_cagr),
                'total_return': float((eia_wealth / self.premium) - Decimal('1'))
            },
            'index_outcome': {
                'final_value': float(index_wealth),
                'cagr': float(index_cagr),
                'total_return': float((index_wealth / self.premium) - Decimal('1'))
            },
            'difference': {
                'wealth_given_up': float(wealth_difference),
                'percentage_less': float(wealth_difference / index_wealth),
                'annual_return_gap': float(index_cagr - eia_cagr)
            },
            'yearly_comparison': [
                {
                    'year': i,
                    'eia_value': eia_yearly[i-1]['eia_value'],
                    'index_value': index_yearly[i-1]['index_value'],
                    'difference': index_yearly[i-1]['index_value'] - eia_yearly[i-1]['eia_value']
                }
                for i in range(1, years + 1)
            ],
            'analysis_conclusion': f'Direct index investing produces {float(wealth_difference):,.0f} MORE wealth - EIA caps destroy returns'
        }

    def tax_deferral_myth_analysis(self, taxable_index_return: Decimal,
                                   capital_gains_tax: Decimal = Decimal('0.15'),
                                   ordinary_tax: Decimal = Decimal('0.24'),
                                   years: int = 20) -> Dict[str, Any]:
        """
        Analyze tax deferral "benefit"

        Key insight: EIA sellers tout tax deferral, but:
        1. Index funds are already tax-efficient (qualified dividends, LTCG)
        2. EIA gains taxed as ORDINARY INCOME (higher rate)
        3. Tax deferral doesn't justify the cost/complexity

        Args:
            taxable_index_return: After-tax return on taxable index fund
            capital_gains_tax: Long-term capital gains rate
            ordinary_tax: Ordinary income tax rate
            years: Investment period

        Returns:
            Tax analysis
        """
        # EIA outcome (tax-deferred growth, ordinary income on withdrawal)
        eia_growth_rate = self.cap_rate * Decimal('0.75')  # Assume average 75% of cap due to limitations
        eia_pretax_wealth = self.premium * ((Decimal('1') + eia_growth_rate) ** Decimal(str(years)))
        eia_gains = eia_pretax_wealth - self.premium
        eia_tax = eia_gains * ordinary_tax
        eia_aftertax_wealth = eia_pretax_wealth - eia_tax

        # Taxable index fund (annual tax drag on dividends, LTCG on sale)
        index_aftertax_return = taxable_index_return * (Decimal('1') - capital_gains_tax * Decimal('0.30'))  # Assume 30% of return taxable annually
        index_pretax_wealth = self.premium * ((Decimal('1') + index_aftertax_return) ** Decimal(str(years)))
        # Final sale tax
        index_gains = index_pretax_wealth - self.premium
        index_final_tax = index_gains * capital_gains_tax
        index_aftertax_wealth = index_pretax_wealth - index_final_tax

        return {
            'assumptions': {
                'investment_period': years,
                'capital_gains_rate': float(capital_gains_tax),
                'ordinary_income_rate': float(ordinary_tax),
                'eia_average_return': float(eia_growth_rate),
                'index_return': float(taxable_index_return)
            },
            'eia_tax_treatment': {
                'growth': 'Tax-deferred',
                'withdrawal_taxation': f'Ordinary income at {float(ordinary_tax):.0%}',
                'final_after_tax_wealth': float(eia_aftertax_wealth),
                'effective_tax_rate': float(eia_tax / eia_gains) if eia_gains > 0 else 0
            },
            'index_fund_tax_treatment': {
                'growth': 'Some annual tax on dividends',
                'withdrawal_taxation': f'LTCG at {float(capital_gains_tax):.0%}',
                'final_after_tax_wealth': float(index_aftertax_wealth),
                'effective_tax_rate': float(index_final_tax / index_gains) if index_gains > 0 else 0
            },
            'comparison': {
                'eia_after_tax': float(eia_aftertax_wealth),
                'index_after_tax': float(index_aftertax_wealth),
                'winner': 'Index Fund' if index_aftertax_wealth > eia_aftertax_wealth else 'EIA',
                'difference': float(index_aftertax_wealth - eia_aftertax_wealth)
            },
            'analysis_reality': (
                'Tax deferral is NOT a benefit when: '
                '(1) EIA caps limit returns significantly, '
                '(2) Gains taxed as ordinary income vs favorable LTCG, '
                '(3) Index funds already tax-efficient. '
                'Tax tail should not wag investment dog.'
            )
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Equity-Indexed Annuities
        Based on "Alternative Investments Analysis"

        Category: "THE BAD"

        Returns:
            Complete verdict
        """
        return {
            'asset_class': 'Equity-Indexed Annuities (EIA) / Fixed Index Annuities (FIA)',
            'category': 'THE BAD',
            'overall_rating': '1/10 - Among the worst financial products',

            'the_good': [
                'Principal protection (floor typically 0%)',
                'Tax deferral (though less valuable than marketed)',
                'No annual fees (costs hidden in crediting)'
            ],

            'the_bad': [
                'Severely limited upside - caps, participation rates, spreads',
                'Average upside capture only 40-60% of market gains',
                'Surrender charges lock up money (7-15 years, 10% initial)',
                'High commissions (5-10%) create conflicts of interest',
                'Complex - most buyers don\'t understand what they own',
                'Dividends excluded from index tracking (huge omission)',
                'Tax deferral negated by ordinary income treatment',
                'Better alternatives exist for every stated goal'
            ],

            'the_ugly': [
                'Among the MOST mis-sold financial products in America',
                'Commissions drive sales, not client suitability',
                'Elderly and unsophisticated targeted by aggressive sales',
                'Marketed as "stock upside with bond downside" - COMPLETELY FALSE',
                'Complexity deliberately obscures costs and limitations',
                'Surrender charges trap money when investors realize mistake',
                'Insurance company gets ALL benefits of complexity',
                'Regulatory scrutiny due to widespread abuse'
            ],

            'key_findings': {
                'upside_capture': '40-60% (vs 100% in index fund)',
                'downside_protection': 'Real, but alternatives exist (bonds, diversification)',
                'costs': 'Hidden in crediting structure - effectively 1-3% annually',
                'suitability': 'Almost NO ONE - better alternatives always exist',
                'tax_efficiency': 'Worse than index funds despite deferral',
                'surrender_charges': 'Draconian - 10% for 7+ years typical'
            },

            'analysis_quote': (
                '"Equity-indexed annuities are financial products designed to benefit the insurance '
                'company and salesperson, not the consumer. They promise stock upside with bond downside. '
                'The reality: you get 40-60% of stock upside (due to caps, participation rates, and '
                'missing dividends) with moderate downside protection you could get cheaper elsewhere. '
                'Add 7-10% commissions, 10-year surrender charges, complexity that obscures costs, and '
                'ordinary income tax treatment, and you have one of the worst financial products sold '
                'to American investors. There is ALWAYS a better alternative."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'NO ONE - avoid completely',
                    'There are ALWAYS better alternatives'
                ],
                'not_suitable_for': [
                    'Everyone - seriously, everyone',
                    'Retirees (use immediate annuities or bonds instead)',
                    'Young investors (use index funds)',
                    'Anyone seeking market returns (use index funds)',
                    'Anyone seeking safety (use bonds, CDs)',
                    'Tax-deferred growth (use 401k, IRA with index funds)',
                    'Estate planning (use life insurance or trusts)'
                ],
                'better_alternatives': [
                    'For market exposure: Low-cost S&P 500 index fund',
                    'For safety: High-quality bonds or CDs',
                    'For tax deferral: Max out 401k/IRA with index funds',
                    'For income: Immediate annuities (SPIA) or bond ladder',
                    'For diversified portfolio: 60/40 stocks/bonds',
                    'For principal protection + growth: TIPS + stock index fund'
                ]
            },

            'regulatory_warnings': [
                'SEC and FINRA have issued multiple investor alerts',
                'High complaint rate vs other products',
                'Many states require special training to sell',
                'Class action lawsuits for mis-selling',
                'Insurance departments warn about sales abuses'
            ],

            'final_verdict': (
                'Equity-indexed annuities are THE BAD verging on THE UGLY. They are deliberately '
                'complex products that benefit insurance companies and salespeople at consumer expense. '
                'The promise of "stock upside with bond downside" is a lie - you get partial stock '
                'upside (40-60%) with moderate downside protection available cheaper elsewhere. Costs '
                'are hidden, commissions are huge (5-10%), surrender charges trap your money (10% for '
                '7+ years), and tax treatment is worse than index funds. Every stated goal - growth, '
                'safety, tax deferral, income, estate planning - is better achieved with simpler, '
                'cheaper alternatives. If a salesperson recommends an EIA, run. If you own one, explore '
                'exit options immediately (accept surrender charge if needed to escape). This product '
                'category should be banned.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive EIA metrics"""
        return {
            'product_type': 'Equity-Indexed Annuity (EIA)',
            'premium': float(self.premium),
            'crediting_parameters': {
                'participation_rate': float(self.participation_rate),
                'cap_rate': float(self.cap_rate),
                'floor_rate': float(self.floor_rate),
                'spread': float(self.spread)
            },
            'contract_terms': {
                'term_years': self.term_years,
                'surrender_period': self.surrender_charge_years,
                'initial_surrender_charge': float(self.initial_surrender_charge)
            },
            'insurer': self.insurer,
            'insurer_rating': self.insurer_rating,
            'analysis_category': 'THE BAD',
            'recommendation': 'AVOID COMPLETELY - among worst financial products for consumers'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.premium

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive EIA valuation summary"""
        return {
            "asset_overview": {
                "product_type": "Equity-Indexed Annuity",
                "premium": float(self.premium),
                "participation_rate": float(self.participation_rate),
                "cap_rate": float(self.cap_rate),
                "surrender_years": self.surrender_charge_years
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE BAD",
            "recommendation": "Avoid - use index funds for growth, bonds for safety"
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Calculate performance metrics"""
        # EIA performance depends heavily on market conditions
        return {
            'expected_return_range': '2-5% annually (between money market and bonds)',
            'upside_capture': '40-60% of S&P 500 gains',
            'downside_protection': '0% floor (principal protected)',
            'note': 'Returns significantly lag index funds due to caps and limitations',
            'analysis_reality': 'Delivers bond-like returns with moderate risk - not "stock upside with bond downside"'
        }


# Export
__all__ = ['EquityIndexedAnnuityAnalyzer']
