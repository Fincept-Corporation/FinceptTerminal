"""Contribution Analysis for M&A"""
import sys
from pathlib import Path
from typing import Dict, Any

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.merger_models.pro_forma_builder import CompanyFinancials

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

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "contribution":
            if len(sys.argv) < 4:
                raise ValueError("Acquirer data, target data, and ownership split required")

            acquirer_data = json.loads(sys.argv[2])
            target_data = json.loads(sys.argv[3])
            ownership_split = float(sys.argv[4])  # This is target ownership percentage (0-1)

            acquirer = CompanyFinancials(**acquirer_data)
            target = CompanyFinancials(**target_data)

            # Calculate shares based on ownership split
            # ownership_split represents target's ownership in combined entity
            # If target gets X%, then acquirer_shares/(acquirer_shares + new_shares) = 1-X
            # Assume acquirer has 100M shares, calculate new shares issued
            acquirer_shares = 100_000_000  # Default assumption
            new_shares_issued = (acquirer_shares * ownership_split) / (1 - ownership_split) if ownership_split < 1 else 0

            analyzer = ContributionAnalyzer(acquirer, target)
            analysis = analyzer.analyze_contributions(acquirer_shares, new_shares_issued)

            result = {"success": True, "data": analysis}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
