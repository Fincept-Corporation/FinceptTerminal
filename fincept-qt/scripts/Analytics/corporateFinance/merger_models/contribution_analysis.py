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

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `contribution_analysis.py analyze <flat-params-json>` (argv length 3). The native
# form is `contribution acquirer target ownership_split`, so a service-style call is translated here and then falls
# through to the native dispatch. Any other argv shape is untouched.
_SERVICE_COMMANDS = ("analyze",)


def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


def _svc_company(p, role):
    """Company dict from `p[role]` (panel) and/or `<role>_<field>` keys (MCP); `shares` -> `shares_outstanding`."""
    d = dict(p[role]) if isinstance(p.get(role), dict) else {}
    for k in ("revenue", "ebitda", "ebit", "net_income", "shares_outstanding", "shares", "cash", "debt",
              "total_assets", "shareholders_equity"):
        v = p.get(role + "_" + k)
        if v is not None and k not in d:
            d[k] = v
    if "shares" in d and "shares_outstanding" not in d:
        d["shares_outstanding"] = d["shares"]
    d.pop("shares", None)
    revenue = float(d.get("revenue") or 0.0)
    if not d.get("ebitda") and revenue:
        d["ebitda"] = revenue * 0.20  # not supplied: assume a 20% EBITDA margin
    if not d.get("ebit") and d.get("ebitda"):
        d["ebit"] = float(d["ebitda"]) - revenue * 0.03
    return d


def _service_argv(argv):
    import json
    if len(argv) != 3 or argv[1] not in _SERVICE_COMMANDS:
        return argv
    try:
        p = json.loads(argv[2])
    except ValueError:
        return argv
    if not isinstance(p, dict):
        return argv
    split = _svc_num(p, "ownership_split", default=None)
    if split is None:
        # MCP shape: the target's share of the combined entity from the two market values.
        acq_cap = _svc_num(p, "acquirer_market_cap", default=0.0)
        tgt_val = _svc_num(p, "target_deal_value", default=0.0)
        split = tgt_val / (acq_cap + tgt_val) if (acq_cap + tgt_val) > 0 else 0.2
    if split > 1.0:
        split /= 100.0
    split = min(max(split, 0.0), 0.99)
    return [argv[0], "contribution", json.dumps(_svc_company(p, "acquirer")), json.dumps(_svc_company(p, "target")),
            repr(split)]
# ---- service ABI shim (MAAnalyticsService) END ----


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    import json

    sys.argv = _service_argv(sys.argv)
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
            # ownership_split can be a float or a JSON dict with acquirer_shares/target_shares
            ownership_arg = sys.argv[4] if len(sys.argv) > 4 else '0.2'
            try:
                ownership_split = float(ownership_arg)
            except ValueError:
                ownership_json = json.loads(ownership_arg)
                acq_shares = ownership_json.get('acquirer_shares', 100000000)
                tgt_shares = ownership_json.get('target_shares', 20000000)
                ownership_split = tgt_shares / (acq_shares + tgt_shares)

            # Build CompanyFinancials with defaults for missing fields
            def build_financials(data):
                rev = data.get('revenue', 0)
                defaults = {
                    'revenue': rev, 'cogs': data.get('cogs', rev * 0.6),
                    'gross_profit': data.get('gross_profit', rev * 0.4),
                    'sg_a': data.get('sg_a', rev * 0.2), 'r_d': data.get('r_d', 0),
                    'depreciation': data.get('depreciation', rev * 0.03),
                    'ebitda': data.get('ebitda', 0), 'ebit': data.get('ebit', 0),
                    'interest_expense': data.get('interest_expense', 0),
                    'ebt': data.get('ebt', 0), 'taxes': data.get('taxes', 0),
                    'net_income': data.get('net_income', 0),
                    'shares_outstanding': data.get('shares_outstanding', 0),
                    'eps': data.get('eps', 0),
                    'total_assets': data.get('total_assets', 0),
                    'total_liabilities': data.get('total_liabilities', 0),
                    'shareholders_equity': data.get('shareholders_equity', 0),
                    'cash': data.get('cash', 0), 'debt': data.get('debt', 0)
                }
                defaults.update(data)
                return CompanyFinancials(**{k: defaults[k] for k in CompanyFinancials.__dataclass_fields__})

            acquirer = build_financials(acquirer_data)
            target = build_financials(target_data)

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
