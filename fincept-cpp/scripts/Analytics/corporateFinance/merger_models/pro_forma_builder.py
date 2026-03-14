"""Pro Forma Financial Statement Builder"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
import numpy as np

@dataclass
class CompanyFinancials:
    """Company financial data structure"""
    revenue: float
    cogs: float
    gross_profit: float
    sg_a: float
    r_d: float
    depreciation: float
    ebitda: float
    ebit: float
    interest_expense: float
    ebt: float
    taxes: float
    net_income: float
    shares_outstanding: float
    eps: float

    total_assets: float = 0
    total_liabilities: float = 0
    shareholders_equity: float = 0
    cash: float = 0
    debt: float = 0

class ProFormaBuilder:
    """Build pro forma combined financial statements"""

    def __init__(self, acquirer_financials: CompanyFinancials,
                 target_financials: CompanyFinancials,
                 deal_structure: Dict[str, Any]):

        self.acquirer = acquirer_financials
        self.target = target_financials
        self.deal_structure = deal_structure

        self.new_shares_issued = 0
        self.new_debt = deal_structure.get('new_debt', 0)
        self.synergies = deal_structure.get('synergies', 0)
        self.integration_costs = deal_structure.get('integration_costs', 0)
        self.goodwill = 0
        self.purchase_price_allocation = {}

    def calculate_new_shares_issued(self, stock_consideration: float,
                                    acquirer_stock_price: float) -> float:
        """Calculate new shares to be issued"""

        self.new_shares_issued = stock_consideration / acquirer_stock_price
        return self.new_shares_issued

    def calculate_goodwill(self, purchase_price: float,
                          target_book_value: float,
                          fair_value_adjustments: float = 0) -> float:
        """Calculate goodwill from acquisition"""

        fair_value_net_assets = target_book_value + fair_value_adjustments
        self.goodwill = purchase_price - fair_value_net_assets

        return self.goodwill

    def build_pro_forma_income_statement(self, year: int = 1) -> Dict[str, float]:
        """Build pro forma income statement"""

        synergy_realization = self._get_synergy_realization(year)
        integration_cost_impact = self._get_integration_cost(year)

        pf_revenue = self.acquirer.revenue + self.target.revenue
        pf_cogs = self.acquirer.cogs + self.target.cogs
        pf_gross_profit = pf_revenue - pf_cogs

        pf_sg_a = self.acquirer.sg_a + self.target.sg_a - (self.synergies * synergy_realization * 0.6)
        pf_r_d = self.acquirer.r_d + self.target.r_d
        pf_depreciation = self.acquirer.depreciation + self.target.depreciation

        pf_ebitda = pf_gross_profit - pf_sg_a - pf_r_d + integration_cost_impact
        pf_ebit = pf_ebitda - pf_depreciation

        incremental_interest = self._calculate_incremental_interest()
        pf_interest_expense = self.acquirer.interest_expense + self.target.interest_expense + incremental_interest

        pf_ebt = pf_ebit - pf_interest_expense

        tax_rate = self.deal_structure.get('tax_rate', 0.21)
        pf_taxes = pf_ebt * tax_rate if pf_ebt > 0 else 0

        pf_net_income = pf_ebt - pf_taxes

        pf_shares = self.acquirer.shares_outstanding + self.new_shares_issued
        pf_eps = pf_net_income / pf_shares if pf_shares else 0

        return {
            'revenue': pf_revenue,
            'cogs': pf_cogs,
            'gross_profit': pf_gross_profit,
            'gross_margin': (pf_gross_profit / pf_revenue * 100) if pf_revenue else 0,
            'sg_a': pf_sg_a,
            'r_d': pf_r_d,
            'depreciation': pf_depreciation,
            'ebitda': pf_ebitda,
            'ebitda_margin': (pf_ebitda / pf_revenue * 100) if pf_revenue else 0,
            'ebit': pf_ebit,
            'interest_expense': pf_interest_expense,
            'ebt': pf_ebt,
            'taxes': pf_taxes,
            'effective_tax_rate': (pf_taxes / pf_ebt * 100) if pf_ebt else 0,
            'net_income': pf_net_income,
            'net_margin': (pf_net_income / pf_revenue * 100) if pf_revenue else 0,
            'shares_outstanding': pf_shares,
            'eps': pf_eps,
            'synergies_realized': self.synergies * synergy_realization,
            'integration_costs': integration_cost_impact
        }

    def build_pro_forma_balance_sheet(self) -> Dict[str, float]:
        """Build pro forma balance sheet"""

        pf_cash = self.acquirer.cash + self.target.cash - self.deal_structure.get('cash_used', 0)
        pf_total_assets = self.acquirer.total_assets + self.target.total_assets + self.goodwill

        pf_debt = self.acquirer.debt + self.target.debt + self.new_debt
        pf_total_liabilities = self.acquirer.total_liabilities + self.target.total_liabilities + self.new_debt

        equity_issuance = self.new_shares_issued * self.deal_structure.get('acquirer_stock_price', 0)
        pf_shareholders_equity = (self.acquirer.shareholders_equity +
                                 equity_issuance +
                                 self.target.shareholders_equity)

        return {
            'cash': pf_cash,
            'total_assets': pf_total_assets,
            'goodwill': self.goodwill,
            'total_debt': pf_debt,
            'total_liabilities': pf_total_liabilities,
            'shareholders_equity': pf_shareholders_equity,
            'debt_to_equity': pf_debt / pf_shareholders_equity if pf_shareholders_equity else 0,
            'equity_to_assets': pf_shareholders_equity / pf_total_assets if pf_total_assets else 0
        }

    def build_multi_year_projections(self, years: int = 5) -> Dict[int, Dict[str, float]]:
        """Build multi-year pro forma projections"""

        projections = {}

        for year in range(1, years + 1):
            projections[year] = self.build_pro_forma_income_statement(year)

        return projections

    def _get_synergy_realization(self, year: int) -> float:
        """Get synergy realization percentage by year"""

        realization_schedule = self.deal_structure.get('synergy_schedule', {
            1: 0.25,
            2: 0.50,
            3: 0.75,
            4: 1.00,
            5: 1.00
        })

        return realization_schedule.get(year, 1.0)

    def _get_integration_cost(self, year: int) -> float:
        """Get integration cost impact by year"""

        if year == 1:
            return -self.integration_costs
        elif year == 2:
            return -self.integration_costs * 0.5
        else:
            return 0

    def _calculate_incremental_interest(self) -> float:
        """Calculate incremental interest expense from new debt"""

        interest_rate = self.deal_structure.get('debt_interest_rate', 0.05)
        return self.new_debt * interest_rate

    def calculate_accretion_dilution(self) -> Dict[str, Any]:
        """Calculate EPS accretion/dilution"""

        standalone_acquirer_eps = self.acquirer.eps

        pf_year1 = self.build_pro_forma_income_statement(year=1)
        pf_eps = pf_year1['eps']

        eps_change = pf_eps - standalone_acquirer_eps
        accretion_dilution_pct = (eps_change / standalone_acquirer_eps * 100) if standalone_acquirer_eps else 0

        is_accretive = eps_change > 0

        return {
            'standalone_eps': standalone_acquirer_eps,
            'pro_forma_eps': pf_eps,
            'eps_change': eps_change,
            'accretion_dilution_pct': accretion_dilution_pct,
            'is_accretive': is_accretive,
            'status': 'Accretive' if is_accretive else 'Dilutive'
        }

    def calculate_breakeven_synergies(self) -> float:
        """Calculate synergies needed for EPS neutral deal"""

        original_synergies = self.synergies
        low, high = 0, original_synergies * 5

        while high - low > 0.01e6:
            mid = (low + high) / 2
            self.synergies = mid

            result = self.calculate_accretion_dilution()

            if result['eps_change'] < 0:
                low = mid
            else:
                high = mid

        breakeven = (low + high) / 2
        self.synergies = original_synergies

        return breakeven

    def compare_standalone_vs_proforma(self) -> Dict[str, Any]:
        """Compare standalone vs pro forma metrics"""

        pf_is = self.build_pro_forma_income_statement()

        combined_standalone_revenue = self.acquirer.revenue + self.target.revenue
        combined_standalone_ni = self.acquirer.net_income + self.target.net_income
        combined_standalone_ebitda = self.acquirer.ebitda + self.target.ebitda

        return {
            'revenue': {
                'standalone_combined': combined_standalone_revenue,
                'pro_forma': pf_is['revenue'],
                'difference': pf_is['revenue'] - combined_standalone_revenue,
                'difference_pct': ((pf_is['revenue'] - combined_standalone_revenue) /
                                  combined_standalone_revenue * 100) if combined_standalone_revenue else 0
            },
            'net_income': {
                'standalone_combined': combined_standalone_ni,
                'pro_forma': pf_is['net_income'],
                'difference': pf_is['net_income'] - combined_standalone_ni,
                'difference_pct': ((pf_is['net_income'] - combined_standalone_ni) /
                                  combined_standalone_ni * 100) if combined_standalone_ni else 0
            },
            'ebitda': {
                'standalone_combined': combined_standalone_ebitda,
                'pro_forma': pf_is['ebitda'],
                'difference': pf_is['ebitda'] - combined_standalone_ebitda,
                'difference_pct': ((pf_is['ebitda'] - combined_standalone_ebitda) /
                                  combined_standalone_ebitda * 100) if combined_standalone_ebitda else 0
            }
        }

if __name__ == '__main__':
    acquirer = CompanyFinancials(
        revenue=10_000_000_000,
        cogs=6_000_000_000,
        gross_profit=4_000_000_000,
        sg_a=2_000_000_000,
        r_d=500_000_000,
        depreciation=300_000_000,
        ebitda=1_500_000_000,
        ebit=1_200_000_000,
        interest_expense=100_000_000,
        ebt=1_100_000_000,
        taxes=231_000_000,
        net_income=869_000_000,
        shares_outstanding=500_000_000,
        eps=1.738,
        total_assets=15_000_000_000,
        total_liabilities=8_000_000_000,
        shareholders_equity=7_000_000_000,
        cash=2_000_000_000,
        debt=3_000_000_000
    )

    target = CompanyFinancials(
        revenue=2_000_000_000,
        cogs=1_200_000_000,
        gross_profit=800_000_000,
        sg_a=400_000_000,
        r_d=100_000_000,
        depreciation=60_000_000,
        ebitda=300_000_000,
        ebit=240_000_000,
        interest_expense=20_000_000,
        ebt=220_000_000,
        taxes=46_200_000,
        net_income=173_800_000,
        shares_outstanding=100_000_000,
        eps=1.738,
        total_assets=3_000_000_000,
        total_liabilities=1_500_000_000,
        shareholders_equity=1_500_000_000,
        cash=300_000_000,
        debt=500_000_000
    )

    deal_structure = {
        'new_debt': 2_000_000_000,
        'synergies': 100_000_000,
        'integration_costs': 50_000_000,
        'tax_rate': 0.21,
        'acquirer_stock_price': 50.0,
        'stock_consideration': 1_000_000_000
    }

    builder = ProFormaBuilder(acquirer, target, deal_structure)
    builder.calculate_new_shares_issued(1_000_000_000, 50.0)

    result = builder.calculate_accretion_dilution()
    print(f"Standalone EPS: ${result['standalone_eps']:.2f}")
    print(f"Pro Forma EPS: ${result['pro_forma_eps']:.2f}")
    print(f"Impact: {result['accretion_dilution_pct']:.1f}% {result['status']}")

    breakeven = builder.calculate_breakeven_synergies()
    print(f"\nBreakeven Synergies: ${breakeven:,.0f}")
