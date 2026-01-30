"""Contribution Analysis for M&A"""
from typing import Dict, Any
from .pro_forma_builder import CompanyFinancials

class ContributionAnalyzer:
    """Analyze contributions from each party to combined entity"""

    def __init__(self, acquirer: CompanyFinancials, target: CompanyFinancials):
        self.acquirer = acquirer
        self.target = target

    def analyze_contributions(self, acquirer_shares: float,
                            new_shares_issued: float) -> Dict[str, Any]:
        """Analyze financial and ownership contributions"""

        total_shares = acquirer_shares + new_shares_issued
        acquirer_ownership = acquirer_shares / total_shares * 100
        target_ownership = new_shares_issued / total_shares * 100

        combined_revenue = self.acquirer.revenue + self.target.revenue
        combined_ebitda = self.acquirer.ebitda + self.target.ebitda
        combined_net_income = self.acquirer.net_income + self.target.net_income
        combined_assets = self.acquirer.total_assets + self.target.total_assets

        return {
            'ownership': {
                'acquirer_pct': acquirer_ownership,
                'target_pct': target_ownership
            },
            'revenue_contribution': {
                'acquirer_pct': (self.acquirer.revenue / combined_revenue * 100) if combined_revenue else 0,
                'target_pct': (self.target.revenue / combined_revenue * 100) if combined_revenue else 0
            },
            'ebitda_contribution': {
                'acquirer_pct': (self.acquirer.ebitda / combined_ebitda * 100) if combined_ebitda else 0,
                'target_pct': (self.target.ebitda / combined_ebitda * 100) if combined_ebitda else 0
            },
            'net_income_contribution': {
                'acquirer_pct': (self.acquirer.net_income / combined_net_income * 100) if combined_net_income else 0,
                'target_pct': (self.target.net_income / combined_net_income * 100) if combined_net_income else 0
            },
            'assets_contribution': {
                'acquirer_pct': (self.acquirer.total_assets / combined_assets * 100) if combined_assets else 0,
                'target_pct': (self.target.total_assets / combined_assets * 100) if combined_assets else 0
            }
        }
