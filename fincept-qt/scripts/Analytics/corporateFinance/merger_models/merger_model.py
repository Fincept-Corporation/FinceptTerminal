"""Complete Merger Model - Accretion/Dilution Analysis"""
import sys
from pathlib import Path
from typing import Dict, Any, Optional

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.merger_models.sources_uses import SourcesUsesBuilder
from corporateFinance.merger_models.pro_forma_builder import ProFormaBuilder, CompanyFinancials
from corporateFinance.merger_models.contribution_analysis import ContributionAnalyzer
from corporateFinance.merger_models.sensitivity_analysis import SensitivityAnalyzer

class MergerModel:
    """Complete M&A merger model with accretion/dilution analysis"""

    def __init__(self, acquirer_data: Dict[str, Any], target_data: Dict[str, Any],
                 deal_terms: Dict[str, Any]):

        self.acquirer_data = acquirer_data
        self.target_data = target_data
        self.deal_terms = deal_terms

        self.acquirer_financials = self._build_company_financials(acquirer_data)
        self.target_financials = self._build_company_financials(target_data)

        self.sources_uses = None
        self.pro_forma_builder = None
        self.contribution_analyzer = None
        self.sensitivity_analyzer = None

    def _build_company_financials(self, data: Dict[str, Any]) -> CompanyFinancials:
        """Build CompanyFinancials from dict"""

        return CompanyFinancials(
            revenue=data.get('revenue', 0),
            cogs=data.get('cogs', data.get('revenue', 0) * 0.6),
            gross_profit=data.get('gross_profit', data.get('revenue', 0) * 0.4),
            sg_a=data.get('sg_a', data.get('revenue', 0) * 0.2),
            r_d=data.get('r_d', 0),
            depreciation=data.get('depreciation', data.get('revenue', 0) * 0.03),
            ebitda=data.get('ebitda', 0),
            ebit=data.get('ebit', 0),
            interest_expense=data.get('interest_expense', 0),
            ebt=data.get('ebt', 0),
            taxes=data.get('taxes', 0),
            net_income=data.get('net_income', 0),
            shares_outstanding=data.get('shares_outstanding', 0),
            eps=data.get('eps', 0),
            total_assets=data.get('total_assets', 0),
            total_liabilities=data.get('total_liabilities', 0),
            shareholders_equity=data.get('shareholders_equity', 0),
            cash=data.get('cash', 0),
            debt=data.get('debt', 0)
        )

    def build_complete_model(self) -> Dict[str, Any]:
        """Build complete merger model"""

        purchase_price = self.deal_terms.get('purchase_price', 0)
        cash_consideration = self.deal_terms.get('cash_consideration', 0)
        stock_consideration = self.deal_terms.get('stock_consideration', 0)
        acquirer_stock_price = self.deal_terms.get('acquirer_stock_price', 0)

        cash_from_balance_sheet = min(cash_consideration, self.acquirer_financials.cash)
        new_debt_needed = max(0, cash_consideration - cash_from_balance_sheet)

        self.sources_uses = SourcesUsesBuilder(
            purchase_price=purchase_price,
            target_debt_refinanced=self.target_financials.debt,
            acquirer_cash=cash_from_balance_sheet,
            new_debt=new_debt_needed,
            new_equity=stock_consideration
        )

        self.sources_uses.estimate_transaction_fees(purchase_price)
        self.sources_uses.estimate_financing_fees(new_debt_needed)

        sources_uses_table = self.sources_uses.auto_balance()

        # Support both combined 'synergies' key and separate 'cost_synergies'/'revenue_synergies' keys
        total_synergies = (
            self.deal_terms.get('synergies') or
            (self.deal_terms.get('cost_synergies', 0) + self.deal_terms.get('revenue_synergies', 0))
        )

        deal_structure = {
            'new_debt': new_debt_needed,
            'synergies': total_synergies,
            'integration_costs': self.deal_terms.get('integration_costs', 0),
            'tax_rate': self.deal_terms.get('tax_rate', 0.21),
            'acquirer_stock_price': acquirer_stock_price,
            'stock_consideration': stock_consideration,
            'debt_interest_rate': self.deal_terms.get('debt_interest_rate', 0.05),
            'synergy_schedule': self.deal_terms.get('synergy_schedule', {
                1: 0.25, 2: 0.50, 3: 0.75, 4: 1.00, 5: 1.00
            })
        }

        self.pro_forma_builder = ProFormaBuilder(
            self.acquirer_financials,
            self.target_financials,
            deal_structure
        )

        new_shares = self.pro_forma_builder.calculate_new_shares_issued(
            stock_consideration,
            acquirer_stock_price
        )

        goodwill = self.pro_forma_builder.calculate_goodwill(
            purchase_price,
            self.target_financials.shareholders_equity
        )

        accretion_dilution = self.pro_forma_builder.calculate_accretion_dilution()

        pf_year1 = self.pro_forma_builder.build_pro_forma_income_statement(year=1)
        pf_balance_sheet = self.pro_forma_builder.build_pro_forma_balance_sheet()

        multi_year_projections = self.pro_forma_builder.build_multi_year_projections(years=5)

        standalone_vs_pf = self.pro_forma_builder.compare_standalone_vs_proforma()

        breakeven_synergies = self.pro_forma_builder.calculate_breakeven_synergies()

        self.contribution_analyzer = ContributionAnalyzer(
            self.acquirer_financials,
            self.target_financials
        )

        contribution_analysis = self.contribution_analyzer.analyze_contributions(
            self.acquirer_financials.shares_outstanding,
            new_shares
        )

        self.sensitivity_analyzer = SensitivityAnalyzer(self.pro_forma_builder)

        synergy_sensitivity = self.sensitivity_analyzer.synergy_sensitivity(
            min_synergies=0,
            max_synergies=total_synergies * 2,
            steps=11
        )

        price_sensitivity = self.sensitivity_analyzer.purchase_price_sensitivity(
            base_price=purchase_price,
            price_range_pct=0.20,
            steps=11
        )

        return {
            'deal_overview': {
                'acquirer': self.acquirer_data.get('company_name', 'Acquirer'),
                'target': self.target_data.get('company_name', 'Target'),
                'purchase_price': purchase_price,
                'cash_consideration': cash_consideration,
                'stock_consideration': stock_consideration,
                'payment_mix': {
                    'cash_pct': (cash_consideration / purchase_price * 100) if purchase_price else 0,
                    'stock_pct': (stock_consideration / purchase_price * 100) if purchase_price else 0
                }
            },
            'sources_uses': sources_uses_table,
            'new_shares_issued': new_shares,
            'goodwill': goodwill,
            'accretion_dilution': accretion_dilution,
            'pro_forma_year1': pf_year1,
            'pro_forma_balance_sheet': pf_balance_sheet,
            'multi_year_projections': multi_year_projections,
            'standalone_vs_proforma': standalone_vs_pf,
            'breakeven_synergies': breakeven_synergies,
            'contribution_analysis': contribution_analysis,
            'sensitivity_analysis': {
                'synergies': synergy_sensitivity,
                'purchase_price': price_sensitivity
            }
        }

    def generate_executive_summary(self) -> Dict[str, Any]:
        """Generate executive summary of merger model"""

        model_results = self.build_complete_model()

        ad = model_results['accretion_dilution']
        pf_y1 = model_results['pro_forma_year1']

        summary = {
            'deal_snapshot': {
                'acquirer': model_results['deal_overview']['acquirer'],
                'target': model_results['deal_overview']['target'],
                'purchase_price': model_results['deal_overview']['purchase_price'],
                'payment_structure': model_results['deal_overview']['payment_mix']
            },
            'financial_impact': {
                'eps_impact': f"{ad['accretion_dilution_pct']:.1f}% {ad['status']}",
                'standalone_eps': ad['standalone_eps'],
                'pro_forma_eps': ad['pro_forma_eps'],
                'pro_forma_revenue': pf_y1['revenue'],
                'pro_forma_ebitda': pf_y1['ebitda'],
                'pro_forma_net_income': pf_y1['net_income']
            },
            'key_assumptions': {
                'synergies': (
                    self.deal_terms.get('synergies') or
                    (self.deal_terms.get('cost_synergies', 0) + self.deal_terms.get('revenue_synergies', 0))
                ),
                'integration_costs': self.deal_terms.get('integration_costs', 0),
                'new_debt': model_results['sources_uses']['sources']['sources_breakdown']['new_debt']['amount'],
                'new_shares_issued': model_results['new_shares_issued']
            },
            'breakeven_analysis': {
                'breakeven_synergies': model_results['breakeven_synergies'],
                'synergies_cushion': (
                    self.deal_terms.get('synergies') or
                    (self.deal_terms.get('cost_synergies', 0) + self.deal_terms.get('revenue_synergies', 0))
                ) - model_results['breakeven_synergies']
            }
        }

        return summary

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `merger_model.py <command> <flat-params-json>` (argv length 3) for build /
# accretion_dilution / pro_forma. The native forms are positional (`build acquirer target terms`,
# `accretion_dilution model_json`, `pro_forma acquirer target year`), so a service-style call is handled here by
# building the three inputs from the flat panel / MCP params and running the same MergerModel. Any other argv
# shape is untouched.
_SERVICE_COMMANDS = ("build", "accretion_dilution", "pro_forma")


def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


def _svc_company(p, role, ref_margin, tax_rate):
    """Company dict from `p[role]` (panel) and/or `<role>_<field>` keys (MCP), with the gaps filled."""
    d = dict(p[role]) if isinstance(p.get(role), dict) else {}
    for k in ("revenue", "ebitda", "ebit", "net_income", "shares_outstanding", "shares", "eps", "cash", "debt",
              "stock_price", "total_assets", "total_liabilities", "shareholders_equity", "depreciation",
              "interest_expense"):
        v = p.get(role + "_" + k)
        if v is not None and k not in d:
            d[k] = v
    if "shares" in d and "shares_outstanding" not in d:
        d["shares_outstanding"] = d["shares"]
    revenue = float(d.get("revenue") or 0.0)
    dep = float(d.setdefault("depreciation", revenue * 0.03))
    interest = float(d.setdefault("interest_expense", 0.0))
    ni = d.get("net_income")
    if ni is None and d.get("eps") and d.get("shares_outstanding"):
        ni = float(d["eps"]) * float(d["shares_outstanding"])
    ebitda = d.get("ebitda")
    ebit = d.get("ebit")
    # Fill the income statement from whatever was supplied: EBITDA/EBIT first, then back-solve from net income,
    # then fall back to the acquirer's EBITDA margin applied to revenue.
    if not ebitda and not ebit:
        if ni:
            ebit = float(ni) / (1.0 - tax_rate) + interest
            ebitda = ebit + dep
        else:
            ebitda = revenue * ref_margin
            ebit = ebitda - dep
    elif not ebitda:
        ebitda = float(ebit) + dep
    elif ebit is None:
        ebit = float(ebitda) - dep
    d["ebitda"] = float(ebitda)
    d["ebit"] = float(ebit)
    if not revenue and d["ebitda"] and ref_margin > 0:
        revenue = d["ebitda"] / ref_margin  # only profit was supplied: infer a revenue base
        d["revenue"] = revenue
    # The pro-forma builder rebuilds EBIT from revenue - COGS - SG&A, so make those consistent with the EBITDA.
    d.setdefault("sg_a", revenue * 0.20)
    d.setdefault("r_d", 0.0)
    d.setdefault("cogs", max(revenue - float(d["sg_a"]) - float(d["r_d"]) - d["ebitda"], 0.0))
    d.setdefault("gross_profit", revenue - float(d["cogs"]))
    if ni is None:
        ni = (d["ebit"] - interest) * (1.0 - tax_rate)
    d["net_income"] = float(ni)
    ni = float(d["net_income"])
    if "ebt" not in d:
        d["ebt"] = ni / (1.0 - tax_rate) if tax_rate < 1 else ni
    d.setdefault("taxes", float(d["ebt"]) - ni)
    shares = float(d.get("shares_outstanding") or 0.0)
    if shares <= 0:
        eps_given = float(d.get("eps") or 0.0)
        # No share count supplied: derive it from EPS, else assume $2.00 of EPS (acquirer: 100M shares).
        shares = ni / eps_given if (eps_given > 0 and ni > 0) else (100e6 if role == "acquirer" else max(ni / 2.0, 1e6))
    d["shares_outstanding"] = shares
    d["eps"] = float(d.get("eps") or (ni / shares))
    return d


def _svc_merger_inputs(p):
    tax_rate = _svc_num(p, "tax_rate", default=0.21)
    acq_src = p.get("acquirer") if isinstance(p.get("acquirer"), dict) else {}
    acq_rev = _svc_num(acq_src, "revenue", default=_svc_num(p, "acquirer_revenue", default=0.0))
    acq_ebitda = _svc_num(acq_src, "ebitda", default=_svc_num(p, "acquirer_ebitda"))
    ref_margin = (acq_ebitda / acq_rev) if (acq_rev and acq_ebitda) else 0.20
    acquirer = _svc_company(p, "acquirer", ref_margin, tax_rate)
    target = _svc_company(p, "target", ref_margin, tax_rate)
    if "acquirer_eps" in p and not (isinstance(p.get("acquirer"), dict) and p["acquirer"].get("eps")):
        acquirer["eps"] = float(p["acquirer_eps"])
    price = _svc_num(p, "deal_value", "purchase_price", "target_deal_value", default=None)
    if not price:
        price = max(float(target["net_income"]), 1.0) * 20.0
    cash_pct = _svc_num(p, "cash_pct", "cash_percentage", default=0.5)
    if cash_pct > 1.0:
        cash_pct /= 100.0
    synergies = _svc_num(p, "synergies", default=None)
    if synergies is None:
        synergies = _svc_num(p, "cost_synergies", default=0.0) + _svc_num(p, "revenue_synergies", default=0.0)
    terms = {
        "purchase_price": price,
        "cash_consideration": price * cash_pct,
        "stock_consideration": price * (1.0 - cash_pct),
        "acquirer_stock_price": _svc_num(p, "acquirer_stock_price", "new_share_price",
                                         default=float(acquirer.get("stock_price") or acquirer["eps"] * 15.0 or 50.0)),
        "synergies": synergies,
        "integration_costs": _svc_num(p, "integration_costs", default=0.0),
        "tax_rate": tax_rate,
        "debt_interest_rate": _svc_num(p, "debt_interest_rate", default=0.05),
    }
    return acquirer, target, terms


def _service_dispatch(argv):
    import json
    if len(argv) != 3 or argv[1] not in _SERVICE_COMMANDS:
        return False
    try:
        p = json.loads(argv[2])
    except ValueError:
        return False
    if not isinstance(p, dict):
        return False
    if "acquirer_data" in p or "deal_terms" in p:
        return False  # the native `accretion_dilution <model_data_json>` form (same argv shape) — not a flat call
    try:
        acquirer, target, terms = _svc_merger_inputs(p)
        results = MergerModel(acquirer, target, terms).build_complete_model()
        if argv[1] == "build":
            data = results
        elif argv[1] == "accretion_dilution":
            data = {
                "accretion_dilution": results.get("accretion_dilution", {}),
                "breakeven_synergies": results.get("breakeven_synergies", 0),
                "deal_overview": results.get("deal_overview", {}),
            }
        else:
            year = int(_svc_num(p, "years", "year", default=1.0))
            projections = results.get("multi_year_projections", {})
            data = {
                "pro_forma": projections.get(str(year), results.get("pro_forma_year1", {})),
                "year": year,
                "standalone_vs_proforma": results.get("standalone_vs_proforma", {}),
            }
        print(json.dumps({"success": True, "data": data}, default=str))
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "command": argv[1]}))
        sys.exit(1)
    return True
# ---- service ABI shim (MAAnalyticsService) END ----


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    import json

    if _service_dispatch(sys.argv):
        return
    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: merger_model.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "build":
            # build_merger_model(acquirer_data, target_data, deal_terms)
            if len(sys.argv) < 5:
                raise ValueError("Acquirer data, target data, and deal terms required")

            acquirer = json.loads(sys.argv[2])
            target = json.loads(sys.argv[3])
            deal_terms = json.loads(sys.argv[4])

            # Convert percentage-based payment to absolute amounts if needed
            pp = deal_terms.get('purchase_price', 0)
            if 'cash_consideration' not in deal_terms and pp:
                cash_pct = deal_terms.get('payment_cash_pct', deal_terms.get('cash_pct', 50)) / 100
                stock_pct = deal_terms.get('payment_stock_pct', deal_terms.get('stock_pct', 50)) / 100
                deal_terms['cash_consideration'] = pp * cash_pct
                deal_terms['stock_consideration'] = pp * stock_pct
            if 'acquirer_stock_price' not in deal_terms:
                deal_terms['acquirer_stock_price'] = acquirer.get('stock_price',
                    acquirer.get('eps', 1) * 15 if acquirer.get('eps') else 50)

            model = MergerModel(acquirer, target, deal_terms)
            results = model.build_complete_model()

            result = {
                "success": True,
                "data": results
            }
            print(json.dumps(result))

        elif command == "accretion_dilution":
            # Host sends: "accretion_dilution" merger_model_data_json
            if len(sys.argv) < 3:
                raise ValueError("Merger model data required")

            model_data = json.loads(sys.argv[2])
            acquirer = model_data.get('acquirer_data', {})
            target = model_data.get('target_data', {})
            deal_terms = model_data.get('deal_terms', {})

            model = MergerModel(acquirer, target, deal_terms)
            results = model.build_complete_model()

            result = {
                "success": True,
                "data": {
                    "accretion_dilution": results.get('accretion_dilution', {}),
                    "breakeven_synergies": results.get('breakeven_synergies', 0),
                    "deal_overview": results.get('deal_overview', {})
                }
            }
            print(json.dumps(result))

        elif command == "pro_forma":
            # Host sends: "pro_forma" acquirer_data target_data year
            if len(sys.argv) < 5:
                raise ValueError("Acquirer data, target data, and year required")

            acquirer = json.loads(sys.argv[2])
            target = json.loads(sys.argv[3])
            year = int(sys.argv[4])

            # Derive purchase price from available data; fall back to 2x target EBITDA or 1x revenue
            target_ev = target.get('enterprise_value', target.get('market_cap', 0))
            if not target_ev:
                target_ebitda = target.get('ebitda', 0)
                target_revenue = target.get('revenue', 0)
                target_ev = target_ebitda * 8 if target_ebitda else target_revenue * 1.5

            deal_terms = {
                'purchase_price': target_ev,
                'cash_consideration': target_ev * 0.5,
                'stock_consideration': target_ev * 0.5,
                'acquirer_stock_price': acquirer.get('stock_price', 100),
                'synergies': acquirer.get('synergies', 0),
                'integration_costs': acquirer.get('integration_costs', 0),
                'tax_rate': acquirer.get('tax_rate', 0.21),
            }

            model = MergerModel(acquirer, target, deal_terms)
            results = model.build_complete_model()

            pro_forma_data = results.get('multi_year_projections', {})
            year_data = pro_forma_data.get(str(year), results.get('pro_forma_year1', {}))

            result = {
                "success": True,
                "data": {
                    "pro_forma": year_data,
                    "year": year,
                    "standalone_vs_proforma": results.get('standalone_vs_proforma', {})
                }
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: build, accretion_dilution, pro_forma"
            }
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {
            "success": False,
            "error": str(e),
            "command": command
        }
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
