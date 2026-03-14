"""
Covered Calls Module

Options strategy of holding stock + selling call option

CFA Standards: Derivatives, Options Strategies

Key Concepts: - Sell call option on stock you own
- Collect premium but cap upside
- Tax inefficiency (converts LTCG to short-term income)
- Transaction costs eat returns
- Better alternative: buy fewer shares

Verdict: THE FLAWED - Tax inefficient, costly
Rating: 3/10 - Avoid in taxable accounts
"""

from decimal import Decimal, getcontext
from typing import Dict, Any, List
from datetime import datetime, timedelta
import logging

from config import AssetParameters, AssetClass
from base_analytics import AlternativeInvestmentBase

logger = logging.getLogger(__name__)
getcontext().prec = 28


class CoveredCallAnalyzer(AlternativeInvestmentBase):
    """
    Covered Call Strategy Analyzer

    1. Tax Problem: Converts long-term gains to short-term income
    2. Transaction Costs: Commissions + bid-ask spreads
    3. Opportunity Cost: Cap upside participation
    4. Better Alternative: Just hold fewer shares if want less risk

    Verdict: THE FLAWED
    Rating: 3/10 - Avoid, especially in taxable accounts
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)

        # Stock position
        self.stock_price = getattr(parameters, 'stock_price', Decimal('100'))
        self.shares_owned = getattr(parameters, 'shares_owned', 100)
        self.position_value = self.stock_price * Decimal(str(self.shares_owned))

        # Call option sold
        self.strike_price = getattr(parameters, 'strike_price', self.stock_price * Decimal('1.05'))  # 5% OTM
        self.option_premium = getattr(parameters, 'option_premium', self.stock_price * Decimal('0.02'))  # 2% premium
        self.days_to_expiration = getattr(parameters, 'days_to_expiration', 30)

        # Costs
        self.option_commission = getattr(parameters, 'option_commission', Decimal('0.65'))  # Per contract
        self.stock_commission = getattr(parameters, 'stock_commission', Decimal('0'))  # Zero commission brokers
        self.bid_ask_spread_pct = getattr(parameters, 'bid_ask_spread_pct', Decimal('0.01'))  # 1% spread

        # Tax rates
        self.ordinary_tax_rate = getattr(parameters, 'ordinary_tax_rate', Decimal('0.37'))  # Top bracket
        self.ltcg_rate = getattr(parameters, 'ltcg_rate', Decimal('0.20'))  # LTCG rate
        self.holding_period = getattr(parameters, 'holding_period_days', 400)  # Days held

    def calculate_option_income(self) -> Dict[str, Any]:
        """Calculate premium income from selling calls"""
        # Premium received (per share)
        premium_per_share = self.option_premium

        # Contracts (100 shares per contract)
        contracts = self.shares_owned / 100

        # Total premium
        total_premium = premium_per_share * Decimal(str(self.shares_owned))

        # Transaction costs
        commission_cost = self.option_commission * Decimal(str(contracts))
        bid_ask_cost = total_premium * self.bid_ask_spread_pct
        total_costs = commission_cost + bid_ask_cost

        # Net premium
        net_premium = total_premium - total_costs

        # Annualized return from premium
        annual_factor = Decimal('365') / Decimal(str(self.days_to_expiration))
        annualized_return = (net_premium / self.position_value) * annual_factor

        return {
            'premium_per_share': float(premium_per_share),
            'total_premium': float(total_premium),
            'contracts': float(contracts),
            'commission_cost': float(commission_cost),
            'bid_ask_cost': float(bid_ask_cost),
            'total_costs': float(total_costs),
            'net_premium': float(net_premium),
            'return_on_position': float(net_premium / self.position_value),
            'annualized_return_estimate': float(annualized_return),
            'note': 'Return capped at strike price'
        }

    def tax_consequences(self) -> Dict[str, Any]:
        """
        Analyze tax consequences of covered calls

        Key Issue: Tax treatment destroys strategy in taxable accounts

        Problem: If stock called away, gain taxed as SHORT-TERM even if held > 1 year
        """
        # Scenario 1: Stock NOT called (expires worthless or bought back)
        premium_income = self.option_premium * Decimal(str(self.shares_owned))

        # Commission to close position
        contracts = self.shares_owned / 100
        close_commission = self.option_commission * Decimal(str(contracts))

        # Premium taxed as short-term income
        premium_after_tax = premium_income * (Decimal('1') - self.ordinary_tax_rate)

        # Scenario 2: Stock CALLED AWAY
        # Any gain becomes short-term (even if held > 1 year)
        assumed_cost_basis = self.stock_price * Decimal('0.90')  # Assume bought 10% lower
        gain_per_share = self.strike_price - assumed_cost_basis

        # If held > 1 year but called: LTCG becomes short-term
        tax_if_ltcg = gain_per_share * Decimal(str(self.shares_owned)) * self.ltcg_rate
        tax_if_called = (gain_per_share * Decimal(str(self.shares_owned)) + premium_income) * self.ordinary_tax_rate

        # Tax penalty from call
        tax_penalty = tax_if_called - tax_if_ltcg

        return {
            'scenario_1_not_called': {
                'premium_income': float(premium_income),
                'tax_rate_on_premium': float(self.ordinary_tax_rate),
                'after_tax_premium': float(premium_after_tax),
                'note': 'Premium taxed as ordinary income'
            },
            'scenario_2_stock_called': {
                'gain_per_share': float(gain_per_share),
                'total_gain': float(gain_per_share * Decimal(str(self.shares_owned))),
                'holding_period_days': self.holding_period,
                'tax_if_ltcg': float(tax_if_ltcg),
                'tax_if_called_short_term': float(tax_if_called),
                'tax_penalty_from_call': float(tax_penalty),
                'penalty_pct_of_gain': float(tax_penalty / (gain_per_share * Decimal(str(self.shares_owned)))) if gain_per_share > 0 else 0,
                'analysis_warning': 'Call destroys LTCG treatment - converts to short-term'
            },
            'analysis_conclusion': 'Tax consequences make covered calls toxic in taxable accounts'
        }

    def opportunity_cost_analysis(self, market_scenarios: List[Decimal] = None) -> Dict[str, Any]:
        """
        Analyze opportunity cost of capping upside

        Key insight: You give up unlimited upside for small premium
        """
        if market_scenarios is None:
            # Stock return scenarios
            market_scenarios = [
                Decimal('-0.20'),  # -20%
                Decimal('-0.10'),  # -10%
                Decimal('0.00'),   # Flat
                Decimal('0.05'),   # +5%
                Decimal('0.10'),   # +10% (above strike)
                Decimal('0.15'),   # +15%
                Decimal('0.20'),   # +20%
                Decimal('0.30'),   # +30%
            ]

        strike_pct = (self.strike_price / self.stock_price) - Decimal('1')
        premium_pct = self.option_premium / self.stock_price

        results = []
        for scenario in market_scenarios:
            stock_return = scenario

            # Covered call return
            if stock_return <= strike_pct:
                # Stock not called
                cc_return = stock_return + premium_pct
            else:
                # Stock called - capped at strike
                cc_return = strike_pct + premium_pct

            # Opportunity cost
            opportunity_cost = stock_return - cc_return

            results.append({
                'stock_return': float(stock_return),
                'covered_call_return': float(cc_return),
                'premium_boost': float(premium_pct),
                'opportunity_cost': float(opportunity_cost),
                'upside_capped': stock_return > strike_pct
            })

        return {
            'strike_premium': float(strike_pct),
            'premium_collected': float(premium_pct),
            'scenarios': results,
            'analysis_insight': 'Small premium not worth giving up unlimited upside'
        }

    def better_alternative(self) -> Dict[str, Any]:
        """
        recommended alternative to covered calls

        Alternative: Just hold fewer shares if you want less equity risk
        - Same risk reduction
        - Better tax treatment
        - Lower costs
        - Maintain upside participation
        """
        # Current position
        current_shares = self.shares_owned
        current_value = self.position_value

        # Covered call effective exposure
        # Premium provides downside cushion
        # Upside capped at strike
        premium_cushion_pct = self.option_premium / self.stock_price

        # Equivalent position: Fewer shares + cash
        # To match risk profile, hold ~85-90% in stock (rough estimate)
        equivalent_shares = current_shares * Decimal('0.90')
        cash_amount = current_value * Decimal('0.10')

        # Tax comparison
        # Alternative: LTCG treatment preserved
        # Covered call: Loses LTCG if called

        return {
            'current_covered_call': {
                'shares': current_shares,
                'value': float(current_value),
                'downside_cushion': float(premium_cushion_pct),
                'upside': 'Capped at strike',
                'tax_treatment': 'Short-term gains if called',
                'transaction_costs': 'Option commissions + spreads'
            },
            'analysis_alternative': {
                'shares': float(equivalent_shares),
                'stock_value': float(current_value * Decimal('0.90')),
                'cash_or_bonds': float(cash_amount),
                'downside_cushion': '10% in safe assets',
                'upside': 'Unlimited on 90% position',
                'tax_treatment': 'LTCG preserved',
                'transaction_costs': 'Zero (or minimal)'
            },
            'advantages_of_alternative': [
                'Lower transaction costs',
                'Better tax treatment (LTCG preserved)',
                'Maintain upside participation',
                'Simpler to implement',
                'No risk of assignment issues'
            ],
            'analysis_quote': '"Why pay extra taxes and transaction costs to cap your upside? Just hold fewer shares."'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """verdict on covered calls"""
        return {
            'strategy': 'Covered Calls',
            'category': 'THE FLAWED',
            'rating': '3/10',
            'analysis_summary': 'Tax inefficient, costly, caps upside',
            'key_problems': [
                'TAX DISASTER: Converts LTCG to short-term ordinary income',
                'TRANSACTION COSTS: Commissions + bid-ask spreads',
                'OPPORTUNITY COST: Cap upside for small premium',
                'COMPLEXITY: Rolling positions, assignment risk',
                'ACCOUNT TYPE: Toxic in taxable accounts'
            ],
            'who_might_use': [
                'Tax-exempt accounts (401k, IRA) - avoid tax problem',
                'Very short-term traders (already short-term gains)',
                'Options market makers (different business model)'
            ],
            'analysis_recommendation': 'AVOID - Especially in taxable accounts',
            'better_alternative': 'Hold fewer shares + bonds/cash if want less risk',
            'analysis_quote': '"Covered calls are a tax-inefficient way to reduce equity exposure. Just hold fewer shares."',
            'bottom_line': 'Tax treatment alone makes this strategy unviable for most investors'
        }

    def calculate_nav(self) -> Decimal:
        """Current position value"""
        stock_value = self.stock_price * Decimal(str(self.shares_owned))
        # Short call is a liability
        call_value = -self.option_premium * Decimal(str(self.shares_owned))
        return stock_value + call_value

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Key covered call metrics"""
        return {
            'position_value': float(self.position_value),
            'option_income': self.calculate_option_income(),
            'tax_analysis': self.tax_consequences(),
            'opportunity_cost': self.opportunity_cost_analysis(),
            'better_alternative': self.better_alternative(),
            'analysis_category': 'THE FLAWED'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Valuation summary"""
        return {
            'strategy_overview': {
                'type': 'Covered Call',
                'stock_price': float(self.stock_price),
                'shares': self.shares_owned,
                'strike_price': float(self.strike_price),
                'option_premium': float(self.option_premium),
                'days_to_expiration': self.days_to_expiration
            },
            'key_metrics': self.calculate_key_metrics(),
            'analysis_category': 'THE FLAWED',
            'recommendation': 'Avoid - Tax inefficient and costly'
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Performance metrics"""
        option_income = self.calculate_option_income()

        return {
            'premium_return': option_income['return_on_position'],
            'annualized_estimate': option_income['annualized_return_estimate'],
            'max_upside': float((self.strike_price / self.stock_price) - Decimal('1')),
            'downside_cushion': float(self.option_premium / self.stock_price),
            'note': 'Returns capped, tax-inefficient'
        }


# Export
__all__ = ['CoveredCallAnalyzer']
