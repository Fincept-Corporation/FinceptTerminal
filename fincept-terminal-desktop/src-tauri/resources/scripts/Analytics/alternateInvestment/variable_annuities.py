"""
Variable Annuities Module

Insurance products with variable returns based on mutual fund subaccounts

CFA Standards: Insurance Products, Retirement Planning

Key Concepts: - Deferred variable annuities (DVA) vs Variable immediate annuities (VIA)
- Mortality and expense (M&E) fees (~1.25%)
- Investment management fees (~0.75%)
- Administrative fees (~0.15%)
- Total costs: 2-3% annually
- Tax deferral benefit overstated
- Better alternatives: Low-cost index funds in taxable accounts

Verdict: THE BAD - Expensive, tax-inefficient, illiquid
Rating: 2/10 - High costs destroy value; rarely makes sense
"""

from decimal import Decimal, getcontext
from typing import Dict, Any, List
from datetime import datetime
import logging

from config import AssetParameters, AssetClass
from base_analytics import AlternativeInvestmentBase

logger = logging.getLogger(__name__)
getcontext().prec = 28


class VariableAnnuityAnalyzer(AlternativeInvestmentBase):
    """
    Variable Annuity Analyzer

    1. HIGH COSTS: M&E (1.25%) + Fund fees (0.75%) + Admin (0.15%) = 2%+
    2. TAX TRAP: Converts LTCG to ordinary income
    3. TAX DEFERRAL: Overstated benefit (index funds already tax-efficient)
    4. ILLIQUIDITY: Surrender charges (typically 7 years)
    5. COMPLEXITY: Riders, living benefits, death benefits add cost
    6. BETTER ALTERNATIVE: Low-cost index fund in taxable account

    Verdict: THE BAD
    Rating: 2/10 - High costs fatal; rarely appropriate
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)

        # Premium and account value
        self.premium = getattr(parameters, 'premium', Decimal('100000'))
        self.account_value = getattr(parameters, 'account_value', self.premium)

        # Fee structure (annual)
        self.mortality_expense_fee = getattr(parameters, 'mortality_expense_fee', Decimal('0.0125'))  # 1.25%
        self.investment_mgmt_fee = getattr(parameters, 'investment_mgmt_fee', Decimal('0.0075'))  # 0.75%
        self.admin_fee = getattr(parameters, 'admin_fee', Decimal('0.0015'))  # 0.15%

        # Optional rider fees
        self.guaranteed_withdrawal_benefit = getattr(parameters, 'guaranteed_withdrawal_benefit', Decimal('0.0'))  # 0-1%
        self.death_benefit_rider = getattr(parameters, 'death_benefit_rider', Decimal('0.0'))  # 0-0.5%

        # Surrender charge schedule
        self.surrender_years = getattr(parameters, 'surrender_years', 7)
        self.initial_surrender_charge = getattr(parameters, 'initial_surrender_charge', Decimal('0.07'))  # 7%

        # Tax rates
        self.ordinary_tax_rate = getattr(parameters, 'ordinary_tax_rate', Decimal('0.37'))
        self.ltcg_rate = getattr(parameters, 'ltcg_rate', Decimal('0.20'))

        # Age (for RMD and death benefit)
        self.age = getattr(parameters, 'age', 50)

    def total_annual_cost(self) -> Dict[str, Any]:
        """
        Calculate total annual cost

        Key insight: Variable annuities typically cost 2-3% annually
        This is ENORMOUS drag on returns
        """
        base_costs = self.mortality_expense_fee + self.investment_mgmt_fee + self.admin_fee
        rider_costs = self.guaranteed_withdrawal_benefit + self.death_benefit_rider
        total = base_costs + rider_costs

        # Cost in dollars
        annual_cost_dollars = total * self.account_value

        return {
            'base_costs': {
                'mortality_expense_fee': float(self.mortality_expense_fee),
                'investment_mgmt_fee': float(self.investment_mgmt_fee),
                'admin_fee': float(self.admin_fee),
                'subtotal': float(base_costs)
            },
            'rider_costs': {
                'guaranteed_withdrawal_benefit': float(self.guaranteed_withdrawal_benefit),
                'death_benefit_rider': float(self.death_benefit_rider),
                'subtotal': float(rider_costs)
            },
            'total_annual_cost_pct': float(total),
            'total_annual_cost_dollars': float(annual_cost_dollars),
            'analysis_note': 'Typical VA costs 2-3% annually - destroys returns over time'
        }

    def tax_deferral_myth(self, years: int = 20, gross_return: Decimal = Decimal('0.08')) -> Dict[str, Any]:
        """
        Analyze tax deferral "benefit"

        Key insight: Tax deferral benefit is OVERSTATED
        - Index funds already tax-efficient (qualified div + LTCG)
        - VA converts gains to ORDINARY INCOME (higher rate)
        - High fees eat tax deferral benefit
        """
        initial = Decimal('10000')

        # Variable Annuity path
        total_fees = self.mortality_expense_fee + self.investment_mgmt_fee + self.admin_fee
        va_net_return = gross_return - total_fees
        va_gross_value = initial * ((Decimal('1') + va_net_return) ** years)
        va_gain = va_gross_value - initial
        va_tax = va_gain * self.ordinary_tax_rate
        va_after_tax = va_gross_value - va_tax

        # Index fund path (taxable account)
        # Tax drag: ~0.3-0.5% annually for broad index fund
        tax_drag = Decimal('0.004')  # 0.4% annual drag
        index_net_return = gross_return - tax_drag
        index_value_before_tax = initial * ((Decimal('1') + index_net_return) ** years)
        # Gains already taxed along the way, only final adjustment
        index_after_tax = index_value_before_tax

        # Comparison
        difference = index_after_tax - va_after_tax

        return {
            'time_horizon_years': years,
            'gross_annual_return': float(gross_return),
            'variable_annuity': {
                'annual_fees': float(total_fees),
                'net_return': float(va_net_return),
                'ending_value_before_tax': float(va_gross_value),
                'tax_on_gains': float(va_tax),
                'tax_rate': float(self.ordinary_tax_rate),
                'final_value': float(va_after_tax)
            },
            'index_fund_taxable': {
                'annual_tax_drag': float(tax_drag),
                'net_return': float(index_net_return),
                'final_value': float(index_after_tax)
            },
            'winner': 'Index Fund' if index_after_tax > va_after_tax else 'Variable Annuity',
            'outperformance': float(difference),
            'analysis_conclusion': 'Index fund beats VA despite "tax deferral" - fees and ordinary income tax kill VA'
        }

    def surrender_charge_schedule(self) -> Dict[str, Any]:
        """
        Generate surrender charge schedule

        Key insight: Surrender charges trap your money
        Typically 7% declining to 0% over 7 years
        """
        schedule = []
        decline_per_year = self.initial_surrender_charge / Decimal(str(self.surrender_years))

        for year in range(self.surrender_years + 1):
            charge_pct = max(Decimal('0'), self.initial_surrender_charge - (decline_per_year * Decimal(str(year))))
            charge_dollars = charge_pct * self.account_value

            schedule.append({
                'year': year,
                'surrender_charge_pct': float(charge_pct),
                'surrender_charge_dollars': float(charge_dollars),
                'net_value_if_surrendered': float(self.account_value - charge_dollars)
            })

        return {
            'surrender_period_years': self.surrender_years,
            'initial_charge': float(self.initial_surrender_charge),
            'schedule': schedule,
            'analysis_warning': 'Surrender charges lock up money and create exit costs'
        }

    def death_benefit_analysis(self) -> Dict[str, Any]:
        """
        Analyze death benefit

        Key insight: Death benefit is NOT insurance
        - Typically just return of premium (not a meaningful benefit)
        - Better to buy term life insurance separately
        """
        # Typical death benefit = greater of account value or premium paid
        death_benefit = max(self.account_value, self.premium)

        # True insurance value
        insurance_value = death_benefit - self.account_value

        return {
            'death_benefit': float(death_benefit),
            'current_account_value': float(self.account_value),
            'insurance_value': float(insurance_value),
            'interpretation': 'No meaningful insurance value' if insurance_value <= 0 else 'Some downside protection',
            'analysis_recommendation': 'Buy separate term life insurance instead - much cheaper'
        }

    def compare_to_alternatives(self) -> Dict[str, Any]:
        """
        Compare VA to better alternatives

        Key insight: Multiple better alternatives exist
        """
        return {
            'variable_annuity': {
                'annual_cost': float(self.mortality_expense_fee + self.investment_mgmt_fee + self.admin_fee),
                'tax_treatment': 'Gains taxed as ordinary income (37% top bracket)',
                'liquidity': f'Surrender charges for {self.surrender_years} years',
                'complexity': 'High - riders, subaccounts, fees',
                'transparency': 'Low - prospectus 100+ pages'
            },
            'alternative_1_index_fund_taxable': {
                'annual_cost': '0.03-0.10% (index fund expense ratio)',
                'tax_treatment': 'Qualified dividends (15%) + LTCG (20%)',
                'liquidity': 'Full - sell anytime',
                'complexity': 'Low - simple',
                'transparency': 'High',
                'analysis_preference': 'BEST for most investors'
            },
            'alternative_2_401k_or_ira': {
                'annual_cost': '0.03-0.50% depending on plan',
                'tax_treatment': 'Tax-deferred (traditional) or tax-free (Roth)',
                'liquidity': 'Penalties before 59.5',
                'complexity': 'Low',
                'analysis_note': 'Max these out BEFORE considering VA'
            },
            'alternative_3_muni_bonds': {
                'annual_cost': '0.05-0.20%',
                'tax_treatment': 'Tax-free income',
                'liquidity': 'Good',
                'analysis_note': 'If in high tax bracket, consider for fixed income'
            }
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """verdict on Variable Annuities"""
        return {
            'product': 'Variable Annuities',
            'category': 'THE BAD',
            'rating': '2/10',
            'analysis_summary': 'Expensive, tax-inefficient, illiquid - rarely makes sense',
            'key_problems': [
                'HIGH COSTS: 2-3% annual fees destroy returns',
                'TAX TRAP: Converts LTCG (20%) to ordinary income (37%)',
                'TAX DEFERRAL MYTH: Index funds already tax-efficient',
                'ILLIQUIDITY: Surrender charges lock up money',
                'COMPLEXITY: Difficult to understand and compare',
                'COMMISSION DRIVEN: Salesperson makes 5-7% upfront'
            ],
            'when_might_make_sense': [
                'Already maxed 401k, IRA, HSA, 529',
                'Very long time horizon (20+ years)',
                'High tax bracket + will be in lower bracket in retirement',
                'Need guaranteed income rider (but annuitize regular annuity instead)'
            ],
            'analysis_recommendation': 'AVOID for almost everyone',
            'better_alternatives': [
                '1st: Max 401k/IRA (tax-advantaged + lower costs)',
                '2nd: Taxable index funds (lower cost, better tax treatment)',
                '3rd: I Bonds, TIPS, Muni bonds (specific needs)',
                '4th: Fixed immediate annuity if need guaranteed income'
            ],
            'analysis_quotes': [
                '"Variable annuities are sold, not bought"',
                '"The only people making money on VAs are the insurance companies and salespeople"',
                '"Tax deferral is not worth 2% annual fees plus ordinary income tax"',
                '"Just buy low-cost index funds in a taxable account - you\'ll do better"'
            ],
            'bottom_line': 'Costs and tax treatment make VAs poor choice; simple index funds superior'
        }

    def calculate_nav(self) -> Decimal:
        """Current account value"""
        return self.account_value

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Key VA metrics"""
        return {
            'account_value': float(self.account_value),
            'total_costs': self.total_annual_cost(),
            'tax_deferral_analysis': self.tax_deferral_myth(),
            'surrender_schedule': self.surrender_charge_schedule(),
            'alternatives': self.compare_to_alternatives(),
            'analysis_category': 'THE BAD'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Valuation summary"""
        return {
            'product_overview': {
                'type': 'Variable Annuity',
                'premium': float(self.premium),
                'account_value': float(self.account_value),
                'surrender_years': self.surrender_years,
                'owner_age': self.age
            },
            'key_metrics': self.calculate_key_metrics(),
            'analysis_category': 'THE BAD',
            'recommendation': 'Avoid - use low-cost index funds instead'
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Performance metrics"""
        total_cost = self.mortality_expense_fee + self.investment_mgmt_fee + self.admin_fee

        return {
            'total_annual_cost': float(total_cost),
            'cost_drag_on_8pct_return': float(total_cost / Decimal('0.08')),
            'note': 'High costs severely reduce returns over time'
        }


# Export
__all__ = ['VariableAnnuityAnalyzer']
