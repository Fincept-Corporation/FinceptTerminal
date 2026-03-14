"""stable_value Module"""

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


class StableValueFundAnalyzer(AlternativeInvestmentBase):
    """
    Stable Value Fund Analyzer

    CFA Standards: Fixed Income - Insurance-Wrapped Products, Retirement Plans

    Key Concepts from Key insight: - Found in 401(k)/403(b) plans as "stable value" option
    - Bond portfolio wrapped with insurance contract (wrap)
    - Book value accounting (not market value)
    - Promises principal protection + stable returns
    - Hidden risks: Liquidity, credit, opportunity cost
    - Market value vs book value disconnect

    Verdict: "The Flawed" - Useful in specific contexts but limited
    Better for older workers near retirement than young accumulators
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.fund_name = parameters.name if hasattr(parameters, 'name') else 'Stable Value Fund'

        # Portfolio characteristics
        self.book_value = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('100000')
        self.market_value = parameters.current_market_value if hasattr(parameters, 'current_market_value') else self.book_value
        self.crediting_rate = parameters.crediting_rate if hasattr(parameters, 'crediting_rate') else Decimal('0.03')

        # Wrap contract
        self.wrap_provider = parameters.wrap_provider if hasattr(parameters, 'wrap_provider') else 'Insurance Company'
        self.wrap_fee = parameters.wrap_fee if hasattr(parameters, 'wrap_fee') else Decimal('0.0025')  # 25 bps

        # Underlying portfolio
        self.portfolio_duration = parameters.portfolio_duration if hasattr(parameters, 'portfolio_duration') else Decimal('3.0')
        self.portfolio_yield = parameters.portfolio_yield if hasattr(parameters, 'portfolio_yield') else Decimal('0.04')

    def calculate_market_to_book_ratio(self) -> Dict[str, Any]:
        """
        Calculate market-to-book ratio

        Key insight: This ratio reveals hidden losses in rising rate environment
        Ratio < 1.0 means market value below book value (losses hidden by wrap)
        Ratio > 1.0 means market value above book value (gains smoothed)

        Returns:
            Market-to-book analysis
        """
        mtb_ratio = self.market_value / self.book_value if self.book_value > 0 else Decimal('1')

        # Difference
        market_book_diff = self.market_value - self.book_value

        # Assess situation
        if mtb_ratio < Decimal('0.95'):
            situation = 'Significant unrealized losses - wrap protecting participants'
            risk_level = 'High - Large potential losses if wrap fails'
        elif mtb_ratio < Decimal('0.98'):
            situation = 'Moderate unrealized losses'
            risk_level = 'Moderate - Some risk if wrap fails'
        elif mtb_ratio < Decimal('1.02'):
            situation = 'Market and book values aligned'
            risk_level = 'Low - Minimal disconnect'
        else:
            situation = 'Unrealized gains - participants not receiving full benefit'
            risk_level = 'Low risk, but gains being smoothed/deferred'

        return {
            'book_value': float(self.book_value),
            'market_value': float(self.market_value),
            'market_to_book_ratio': float(mtb_ratio),
            'difference': float(market_book_diff),
            'difference_percentage': float(market_book_diff / self.book_value),
            'situation': situation,
            'risk_level': risk_level,
            'analysis_insight': 'Rising rates create market < book. Wrap hides losses but increases risk.'
        }

    def crediting_rate_analysis(self, treasury_yield: Decimal,
                                credit_spread: Decimal) -> Dict[str, Any]:
        """
        Analyze crediting rate vs market rates

        CFA: Crediting rate smooths market fluctuations
        Formula: Crediting Rate ≈ Portfolio Yield - Wrap Fee - Amortization

        Args:
            treasury_yield: Current Treasury yield (comparable duration)
            credit_spread: Credit spread on underlying bonds

        Returns:
            Crediting rate analysis
        """
        # Theoretical market rate
        market_rate = treasury_yield + credit_spread

        # Crediting rate components
        gross_portfolio_yield = self.portfolio_yield
        wrap_cost = self.wrap_fee
        net_crediting_rate = self.crediting_rate

        # Amortization factor (smoothing of market value changes)
        implied_amortization = gross_portfolio_yield - wrap_cost - net_crediting_rate

        # Compare to alternatives
        money_market_rate = treasury_yield - Decimal('0.005')  # Typically slightly below
        short_term_bond_rate = treasury_yield + Decimal('0.005')

        return {
            'crediting_rate': float(net_crediting_rate),
            'components': {
                'gross_portfolio_yield': float(gross_portfolio_yield),
                'wrap_fee': float(wrap_cost),
                'amortization_smoothing': float(implied_amortization),
                'net_to_participant': float(net_crediting_rate)
            },
            'market_comparisons': {
                'treasury_yield': float(treasury_yield),
                'market_rate_bonds': float(market_rate),
                'money_market_rate': float(money_market_rate),
                'short_term_bond_rate': float(short_term_bond_rate)
            },
            'relative_value': {
                'vs_money_market': float(net_crediting_rate - money_market_rate),
                'vs_short_bond': float(net_crediting_rate - short_term_bond_rate),
                'vs_market_bond': float(net_crediting_rate - market_rate)
            },
            'interpretation': self._interpret_crediting_rate(net_crediting_rate, market_rate, money_market_rate)
        }

    def _interpret_crediting_rate(self, crediting: Decimal, market_bond: Decimal, money_market: Decimal) -> str:
        """Interpret crediting rate attractiveness"""
        if crediting > market_bond:
            return 'Attractive - Crediting rate above market (book > market scenario)'
        elif crediting > money_market + Decimal('0.005'):
            return 'Fair - Premium over money market'
        elif crediting > money_market:
            return 'Modest premium - Slight advantage over money market'
        else:
            return 'Unattractive - Not adequately compensating for duration risk'

    def wrap_contract_risk_analysis(self) -> Dict[str, Any]:
        """
        Analyze risks in wrap contract

        Key insight: Wrap contract has several risks often overlooked:
        1. Credit risk - Insurance company can fail
        2. Liquidity risk - Can't access funds immediately in crisis
        3. Put-back risk - Employer termination triggers book-to-market
        4. Competing fund transfer restrictions

        Returns:
            Wrap risk assessment
        """
        # Risk factors
        risk_factors = {
            'credit_risk': {
                'description': 'Wrap provider (insurance company) default risk',
                'mitigation': 'Multiple wrap providers diversify risk',
                'analysis_concern': 'AIG 2008 scare showed this is real risk'
            },
            'liquidity_risk': {
                'description': 'Restrictions on withdrawals (equity wash, 90-day notice)',
                'mitigation': 'Plan for holding period, don\'t count on immediate access',
                'analysis_concern': 'Can\'t flee when you need to - trapped during crises'
            },
            'put_back_risk': {
                'description': 'If plan terminates or wrap ends, participants get market value',
                'mitigation': 'Diversify across plans if possible',
                'analysis_concern': 'Losses realized when wrap protection disappears'
            },
            'competing_fund_restrictions': {
                'description': '90-day equity wash before/after transfers to stocks',
                'mitigation': 'Plan rebalancing with restrictions in mind',
                'analysis_concern': 'Limits tactical flexibility'
            }
        }

        # Market-to-book vulnerability
        mtb = self.market_value / self.book_value if self.book_value > 0 else Decimal('1')
        vulnerability = 'High' if mtb < Decimal('0.95') else 'Moderate' if mtb < Decimal('0.98') else 'Low'

        return {
            'wrap_provider': self.wrap_provider,
            'wrap_fee': float(self.wrap_fee),
            'risk_factors': risk_factors,
            'current_vulnerability': {
                'market_to_book': float(mtb),
                'vulnerability_level': vulnerability,
                'potential_loss_if_unwrapped': float((Decimal('1') - mtb) * self.book_value)
            },
            'analysis_warning': (
                'Wrap contract is insurance, not magic. Credit risk, liquidity restrictions, '
                'and potential put-back risk make this less safe than it appears.'
            )
        }

    def interest_rate_sensitivity(self, rate_change_bps: int) -> Dict[str, Any]:
        """
        Calculate sensitivity to interest rate changes

        CFA: Duration measures price sensitivity to rates
        Stable value HIDES this sensitivity via book value accounting

        Args:
            rate_change_bps: Rate change in basis points

        Returns:
            Interest rate sensitivity
        """
        rate_change = Decimal(str(rate_change_bps)) / Decimal('10000')

        # Market value impact (duration effect)
        market_value_change_pct = -self.portfolio_duration * rate_change
        new_market_value = self.market_value * (Decimal('1') + market_value_change_pct)

        # Book value stays the same (that's the wrap's job)
        new_book_value = self.book_value

        # New market-to-book ratio
        new_mtb = new_market_value / new_book_value if new_book_value > 0 else Decimal('1')

        # Change in crediting rate (lags market changes)
        # Simplified: Crediting rate adjusts slowly based on portfolio yield
        portfolio_yield_change = rate_change
        new_crediting_rate = self.crediting_rate + (portfolio_yield_change * Decimal('0.5'))  # 50% pass-through

        return {
            'rate_change_bps': rate_change_bps,
            'rate_change_pct': float(rate_change),
            'portfolio_duration': float(self.portfolio_duration),
            'market_value_impact': {
                'current_market_value': float(self.market_value),
                'new_market_value': float(new_market_value),
                'change_pct': float(market_value_change_pct),
                'change_dollar': float(new_market_value - self.market_value)
            },
            'book_value_impact': {
                'current_book_value': float(self.book_value),
                'new_book_value': float(new_book_value),
                'change': 0.0,
                'note': 'Book value protected by wrap - no immediate impact'
            },
            'market_to_book': {
                'current_ratio': float(self.market_value / self.book_value),
                'new_ratio': float(new_mtb),
                'deterioration': float(new_mtb - (self.market_value / self.book_value))
            },
            'crediting_rate': {
                'current_rate': float(self.crediting_rate),
                'new_rate': float(new_crediting_rate),
                'change_bps': float((new_crediting_rate - self.crediting_rate) * Decimal('10000'))
            },
            'analysis_insight': 'Rising rates hurt market value but wrap hides losses. Eventually crediting rate adjusts up.'
        }

    def opportunity_cost_analysis(self, stock_return: Decimal,
                                  bond_return: Decimal,
                                  years: int = 30) -> Dict[str, Any]:
        """
        Calculate opportunity cost for young investors

        Key insight: Stable value inappropriate for young 401(k) participants
        Opportunity cost of missing stock returns is ENORMOUS over 30+ years

        Args:
            stock_return: Expected stock return
            bond_return: Expected bond return
            years: Investment horizon

        Returns:
            Opportunity cost analysis
        """
        initial_investment = Decimal('10000')

        # Stable value outcome
        sv_rate = self.crediting_rate
        sv_wealth = initial_investment * ((Decimal('1') + sv_rate) ** Decimal(str(years)))

        # Bond outcome
        bond_wealth = initial_investment * ((Decimal('1') + bond_return) ** Decimal(str(years)))

        # Stock outcome
        stock_wealth = initial_investment * ((Decimal('1') + stock_return) ** Decimal(str(years)))

        # Opportunity costs
        vs_bonds = bond_wealth - sv_wealth
        vs_stocks = stock_wealth - sv_wealth

        return {
            'assumptions': {
                'initial_investment': float(initial_investment),
                'investment_horizon': years,
                'stable_value_rate': float(sv_rate),
                'bond_return': float(bond_return),
                'stock_return': float(stock_return)
            },
            'outcomes': {
                'stable_value_wealth': float(sv_wealth),
                'bond_wealth': float(bond_wealth),
                'stock_wealth': float(stock_wealth)
            },
            'opportunity_cost': {
                'vs_bonds': float(vs_bonds),
                'vs_bonds_pct': float(vs_bonds / sv_wealth),
                'vs_stocks': float(vs_stocks),
                'vs_stocks_pct': float(vs_stocks / sv_wealth)
            },
            'analysis_conclusion': self._opportunity_cost_message(years, vs_stocks, sv_wealth)
        }

    def _opportunity_cost_message(self, years: int, cost: Decimal, sv_wealth: Decimal) -> str:
        """Generate opportunity cost message"""
        cost_pct = (cost / sv_wealth) if sv_wealth > 0 else Decimal('0')

        if years > 25:
            return f'MASSIVE opportunity cost for young investors - giving up {float(cost_pct):.0%} by avoiding stocks'
        elif years > 15:
            return f'Significant opportunity cost - missing {float(cost_pct):.0%} potential wealth'
        elif years > 5:
            return f'Moderate opportunity cost - {float(cost_pct):.0%} less than stock allocation'
        else:
            return f'Small opportunity cost - appropriate for near-term needs'

    def suitability_analysis(self, investor_age: int, retirement_age: int,
                            risk_tolerance: str) -> Dict[str, Any]:
        """
        Determine suitability for investor

        Key insight: Stable value most suitable for:
        - Workers 5-10 years from retirement
        - Very conservative investors
        - Emergency fund in 401(k)

        NOT suitable for:
        - Young workers (huge opportunity cost)
        - Long investment horizons (inflation risk)

        Args:
            investor_age: Current age
            retirement_age: Target retirement age
            risk_tolerance: 'conservative', 'moderate', 'aggressive'

        Returns:
            Suitability analysis
        """
        years_to_retirement = retirement_age - investor_age

        # Suitability scoring
        if years_to_retirement <= 5:
            time_score = 'Highly Suitable'
            time_reason = 'Short horizon - principal protection valuable'
        elif years_to_retirement <= 10:
            time_score = 'Suitable'
            time_reason = 'Near retirement - capital preservation appropriate'
        elif years_to_retirement <= 20:
            time_score = 'Questionable'
            time_reason = 'Medium horizon - opportunity cost significant'
        else:
            time_score = 'Unsuitable'
            time_reason = 'Long horizon - massive opportunity cost vs stocks'

        # Risk tolerance consideration
        if risk_tolerance == 'conservative':
            risk_score = 'Suitable'
            risk_reason = 'Matches low risk tolerance'
        elif risk_tolerance == 'moderate':
            risk_score = 'Marginal'
            risk_reason = 'Consider balanced stock/bond mix instead'
        else:  # aggressive
            risk_score = 'Unsuitable'
            risk_reason = 'Inconsistent with growth objectives'

        # Overall recommendation
        if time_score in ['Highly Suitable', 'Suitable'] and risk_score != 'Unsuitable':
            recommendation = 'Appropriate allocation'
            suggested_allocation = '20-50% of portfolio'
        elif years_to_retirement > 10:
            recommendation = 'Avoid - use stock/bond mix'
            suggested_allocation = '0-10% (emergency fund only)'
        else:
            recommendation = 'Consider smaller allocation'
            suggested_allocation = '10-30% of portfolio'

        return {
            'investor_profile': {
                'age': investor_age,
                'retirement_age': retirement_age,
                'years_to_retirement': years_to_retirement,
                'risk_tolerance': risk_tolerance
            },
            'suitability_assessment': {
                'time_horizon_suitability': time_score,
                'time_reason': time_reason,
                'risk_tolerance_suitability': risk_score,
                'risk_reason': risk_reason
            },
            'recommendation': {
                'overall': recommendation,
                'suggested_allocation': suggested_allocation,
                'alternative': 'Age-based target-date fund' if years_to_retirement > 10 else 'Short-term bond fund'
            },
            'analysis_guidance': (
                f'For {years_to_retirement} years to retirement: {recommendation}. '
                'Stable value good for near-retirees, terrible for young accumulators.'
            )
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """
        Complete analytical verdict on Stable Value Funds
        Based on "Alternative Investments Analysis"

        Category: "THE FLAWED"

        Returns:
            Complete verdict
        """
        return {
            'asset_class': 'Stable Value Funds',
            'category': 'THE FLAWED',
            'overall_rating': '5/10 - Context dependent',

            'the_good': [
                'Principal protection via book value accounting',
                'Smooth returns - no visible volatility',
                'Premium over money market funds (usually)',
                'Useful for near-retirees (5-10 years out)',
                'Emergency fund alternative in 401(k)',
                'Psychologically comforting for nervous investors'
            ],

            'the_bad': [
                'Hidden risks - market value can be far below book value',
                'Wrap contract credit risk (insurance company)',
                'Liquidity restrictions (equity wash, 90-day rules)',
                'Put-back risk if plan terminates',
                'Opportunity cost for young investors is MASSIVE',
                'Inflation risk over long periods',
                'Complexity - participants don\'t understand risks'
            ],

            'the_ugly': [
                'Marketed as "safe" but has real risks',
                'Young workers misled into using for retirement (huge opportunity cost)',
                'AIG crisis (2008) showed wrap risk is real',
                'Book value accounting hides true risk during rate rises',
                'Restrictions trap investors when they want to leave',
                'Not suitable for long-term accumulation despite marketing'
            ],

            'key_findings': {
                'safety': 'Safer than bonds, but not risk-free',
                'liquidity': 'Limited - restrictions on withdrawals',
                'returns': 'Money market + modest premium',
                'suitability': 'Near-retirees (5-10 years), not young workers',
                'opportunity_cost': 'Catastrophic for long horizons (30+ years)',
                'better_alternative': 'Short-term bond fund (more liquid, similar return)'
            },

            'analysis_quote': (
                '"Stable value funds serve a purpose for workers nearing retirement who need '
                'capital preservation. However, they are vastly overused by young 401(k) participants '
                'who don\'t understand the enormous opportunity cost of avoiding stocks for 30-40 years. '
                'The \'stability\' comes at a huge price - missed wealth accumulation. And the safety '
                'is not absolute - wrap contracts have credit risk, liquidity restrictions, and put-back '
                'risk. For most investors most of the time, a short-term bond fund is better."'
            ),

            'investment_recommendation': {
                'suitable_for': [
                    'Workers 5-10 years from retirement',
                    'Very conservative investors who can\'t handle volatility',
                    'Emergency fund within 401(k) (if no other option)',
                    'Capital preservation with modest return goal'
                ],
                'not_suitable_for': [
                    'Young workers (under 50) - opportunity cost too high',
                    'Long investment horizons (15+ years)',
                    'Primary retirement savings vehicle',
                    'Investors who need liquidity',
                    'Those who don\'t understand wrap risks'
                ],
                'better_alternatives': [
                    'Short-term bond fund (more liquid, similar return)',
                    'Money market fund (if very short horizon)',
                    'Age-based target-date fund (young workers)',
                    'Balanced fund (60/40 stocks/bonds) for near-retirees'
                ]
            },

            'final_verdict': (
                'Stable value funds are FLAWED because they\'re often misused. They serve a valid '
                'purpose for near-retirees needing capital preservation, but are inappropriate for '
                'young workers who should be in stocks. The opportunity cost over 30-40 years is '
                'staggering - potentially hundreds of thousands of dollars in missed wealth. Hidden '
                'risks (wrap credit, liquidity, put-back) are understated. If you\'re 5-10 years from '
                'retirement and conservative, stable value makes sense. If you\'re young, it\'s a '
                'wealth killer. Know the difference.'
            )
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive stable value fund metrics"""
        mtb = self.calculate_market_to_book_ratio()

        return {
            'fund_name': self.fund_name,
            'book_value': float(self.book_value),
            'market_value': float(self.market_value),
            'crediting_rate': float(self.crediting_rate),
            'portfolio_duration': float(self.portfolio_duration),
            'wrap_provider': self.wrap_provider,
            'wrap_fee': float(self.wrap_fee),
            'market_to_book_analysis': mtb,
            'analysis_category': 'THE FLAWED',
            'suitable_for': 'Near-retirees (5-10 years), not young workers',
            'recommendation': 'Use only if appropriate for age/situation'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV - returns book value (what participants see)"""
        return self.book_value

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive stable value fund valuation summary"""
        return {
            "asset_overview": {
                "asset_class": "Stable Value Fund",
                "fund_name": self.fund_name,
                "book_value": float(self.book_value),
                "market_value": float(self.market_value),
                "crediting_rate": float(self.crediting_rate)
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE FLAWED",
            "recommendation": "Context-dependent - good for near-retirees, bad for young workers"
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Calculate performance metrics"""
        # Stable value typically shows smooth, steady returns
        return {
            'crediting_rate': float(self.crediting_rate),
            'volatility': 'Near zero (by design - book value accounting)',
            'sharpe_ratio': 'Not applicable - returns smoothed artificially',
            'note': 'Displayed performance does not reflect true market risk'
        }


# Export
__all__ = ['StableValueFundAnalyzer']
