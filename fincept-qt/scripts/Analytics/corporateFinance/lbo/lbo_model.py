"""Complete LBO Model - projections, debt paydown (cash sweep), exit and returns.

CLI (python lbo_model.py <command> <args>):
  build <json>        LBO Model tab. Keys: ebitda (or revenue + ebitda_margin), entry_multiple, exit_multiple,
                      revenue_growth, ebitda_margin, holding_period, senior_debt + senior_rate (100% cash sweep),
                      sub_debt + sub_rate (bullet); optional capex_pct_revenue, nwc_pct_revenue, tax_rate,
                      hurdle_irr; without senior/sub debt: debt_multiple (x EBITDA, template tranches).
  sensitivity <json>  Sensitivity tab: the same keys plus entry_base, exit_base, range, steps -> IRR / MOIC grid
                      over entry multiple x exit multiple (debt held fixed, equity = entry EV - debt).
  build <target_json> <assumptions_json> [years]               native positional form.
  sensitivity <base_json> <growth_scenarios> <exit_multiples>  native: revenue growth x exit multiple.
Rates are fractions; IRRs in the output are percents (irr / irr_pct), null when no IRR exists.
"""
import os
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))
try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(str(analytics_path)))
    import fincept_json

# Use absolute imports instead of relative imports
from corporateFinance.lbo.capital_structure import CapitalStructureBuilder
from corporateFinance.lbo.debt_schedule import DebtSchedule
from corporateFinance.lbo.returns_calculator import ReturnsCalculator
from corporateFinance.lbo.exit_analysis import ExitAnalyzer

class LBOModel:
    """Complete Leveraged Buyout Model"""

    def __init__(self, target_data: Dict[str, Any], transaction_assumptions: Dict[str, Any]):
        self.target = target_data
        self.assumptions = transaction_assumptions

        self.entry_ebitda = target_data.get('ebitda')
        self.entry_multiple = transaction_assumptions.get('entry_multiple', 10.0)
        self.entry_ev = self.entry_ebitda * self.entry_multiple

        self.capital_structure = None
        self.debt_schedule = None
        self.returns_calc = ReturnsCalculator()
        self.exit_analyzer = None

    def build_complete_model(self, projection_years: int = 5) -> Dict[str, Any]:
        """Build complete LBO model.

        Capital structure: `assumptions['debt_tranches']` ([{name, type, amount, rate, sweep_percentage,
        mandatory_amortization, amortization_years}]) when given, equity = entry EV - debt; otherwise the template
        structure at `target_leverage` x EBITDA. Each year interest is charged on opening balances, levered free
        cash flow pays mandatory amortisation, then the cash sweep, and whatever is left accumulates as cash, so
        exit equity = exit EV - (debt - cash). Returns are measured on total equity (sponsor + rollover +
        management), the same claim the exit equity value represents.
        """

        self.capital_structure = CapitalStructureBuilder(self.entry_ev, self.entry_ebitda)
        tranches_in = self.assumptions.get('debt_tranches')
        if tranches_in:
            for t in tranches_in:
                self.capital_structure.add_debt_tranche(
                    name=t['name'], amount=float(t['amount']), interest_rate=float(t['rate']),
                    amortization_years=t.get('amortization_years', 10), debt_type=t.get('type', 'Term Loan B'),
                    mandatory_amortization=float(t.get('mandatory_amortization', 0.0)),
                    sweep_percentage=float(t.get('sweep_percentage', 0.0)))
            total_debt_in = sum(t.amount for t in self.capital_structure.debt_tranches)
            if total_debt_in >= self.entry_ev:
                raise ValueError("Debt (%.0f) must be below the entry enterprise value (%.0f = EBITDA x entry "
                                 "multiple)." % (total_debt_in, self.entry_ev))
            self.capital_structure.set_equity(sponsor_equity=self.entry_ev - total_debt_in)
            cap_structure = self.capital_structure.build_capital_structure_table()
        else:
            target_leverage = self.assumptions.get('target_leverage', 5.5)
            cap_structure = self.capital_structure.optimize_capital_structure(target_leverage)

        revenue_growth_raw = self.assumptions.get('revenue_growth', 0.05)
        if isinstance(revenue_growth_raw, (int, float)):
            revenue_growth = [revenue_growth_raw] * projection_years
        else:
            revenue_growth = list(revenue_growth_raw)
        ebitda_margin = self.assumptions.get('ebitda_margin', 0.15)
        capex_pct = self.assumptions.get('capex_pct_revenue', 0.03)
        nwc_pct = self.assumptions.get('nwc_pct_revenue', 0.05)
        tax_rate = self.assumptions.get('tax_rate', 0.21)

        financials, debt_schedule = self._project(
            revenue_growth, ebitda_margin, capex_pct, nwc_pct, tax_rate, projection_years
        )

        self.debt_schedule = DebtSchedule(self.capital_structure.debt_tranches)
        ebitda_projection = [fin['ebitda'] for fin in financials.values()]
        interest_coverage = self.debt_schedule.calculate_interest_coverage(ebitda_projection, debt_schedule)
        leverage_progression = self.debt_schedule.calculate_leverage_progression(ebitda_projection, debt_schedule)
        debt_summary = self.debt_schedule.summarize_debt_paydown(debt_schedule)
        debt_summary['exit_cash'] = debt_schedule[max(debt_schedule)]['cash_ending']

        exit_year = self.assumptions.get('exit_year', 5)
        exit_ebitda = financials[exit_year]['ebitda']
        exit_multiples = self.assumptions.get('exit_multiples', [8.0, 10.0, 12.0])

        self.exit_analyzer = ExitAnalyzer(exit_ebitda)

        exit_net_debt = debt_schedule[exit_year]['net_debt_ending']
        exit_scenarios = self.exit_analyzer.multiples_exit_analysis(exit_multiples, exit_net_debt)
        for scenario in exit_scenarios.values():
            scenario['exit_debt'] = debt_schedule[exit_year]['total_debt_ending']
            scenario['exit_cash'] = debt_schedule[exit_year]['cash_ending']
            scenario['exit_net_debt'] = exit_net_debt

        returns_analysis = {}
        initial_equity = cap_structure['leverage_metrics']['total_equity']

        for multiple, scenario in exit_scenarios.items():
            exit_equity = scenario['exit_equity_value']
            returns = self.returns_calc.comprehensive_returns(initial_equity, exit_equity, exit_year)
            hurdle_metrics = self.returns_calc.calculate_hurdle_metrics(returns, self.assumptions.get('hurdle_irr', 0.20))

            returns_analysis[multiple] = {
                **returns,
                'hurdle_metrics': hurdle_metrics
            }

        sensitivity = self._build_sensitivity_analysis(
            initial_equity, exit_year, ebitda_projection, debt_schedule
        )

        # Build a consolidated 'returns' dict for the base (middle) exit multiple; returns_analysis is keyed by
        # the multiple itself (a float), not its string form.
        base_exit_mult = self.assumptions.get('exit_multiples', [10.0])
        base_returns = returns_analysis.get(base_exit_mult[len(base_exit_mult) // 2], {}) if base_exit_mult else {}
        if not base_returns and returns_analysis:
            base_returns = next(iter(returns_analysis.values()))

        # Sources & uses: transaction/financing fees are not modelled (nothing funds them and the returns exclude
        # them), so they are not listed as uses; uses = entry EV = debt + equity.
        sources_uses = {
            'sources': {
                'senior_debt': sum(t['amount'] for t in cap_structure.get('debt_tranches', []) if t.get('type') in ['Term Loan A', 'Term Loan B', 'Senior Notes']),
                'subordinated_debt': sum(t['amount'] for t in cap_structure.get('debt_tranches', []) if t.get('type') in ['Subordinated', 'Mezzanine']),
                'revolver': sum(t['amount'] for t in cap_structure.get('debt_tranches', []) if t.get('type') == 'Revolver'),
                'sponsor_equity': cap_structure.get('leverage_metrics', {}).get('sponsor_equity_contribution', 0),
                'rollover_equity': cap_structure.get('leverage_metrics', {}).get('rollover_equity', 0),
                'management_equity': cap_structure.get('leverage_metrics', {}).get('management_equity', 0),
            },
            'uses': {
                'enterprise_value': self.entry_ev,
            }
        }

        return {
            'transaction_summary': {
                'target_company': self.target.get('company_name', 'Target'),
                'entry_ebitda': self.entry_ebitda,
                'entry_multiple': self.entry_multiple,
                'entry_enterprise_value': self.entry_ev,
                'holding_period': exit_year,
                'exit_year': exit_year
            },
            'capital_structure': cap_structure,
            'financial_projections': financials,
            'debt_schedule': debt_schedule,
            'debt_summary': debt_summary,
            'credit_metrics': {
                'interest_coverage': interest_coverage,
                'leverage_progression': leverage_progression
            },
            'exit_scenarios': exit_scenarios,
            'returns_analysis': returns_analysis,
            'returns': {
                # percent (the generic M&A result view labels *_pct keys as percents without rescaling)
                'irr_pct': base_returns.get('irr'),
                'moic': base_returns.get('moic'),
                'entry_equity': initial_equity,
                'exit_equity': base_returns.get('exit_equity_value'),
            },
            'sources_uses': sources_uses,
            'sensitivity_analysis': sensitivity
        }

    def _project(self, revenue_growth: List[float], ebitda_margin: float, capex_pct: float, nwc_pct: float,
                 tax_rate: float, years: int):
        """Project the income statement and the debt/cash schedule together, year by year.

        Interest is charged on each tranche's opening balance; levered FCF = EBITDA - taxes - capex - change in
        NWC - interest; it pays mandatory amortisation, then each sweeping tranche takes its share of the cash
        left (a dollar is swept once), and the remainder accumulates as cash (negative = a funding need that no
        revolver draw covers in this model). Returns ({year: income lines}, {year: debt schedule row}).
        """

        base_revenue = self.target.get('revenue') or self.entry_ebitda / ebitda_margin
        tranches = self.capital_structure.debt_tranches
        balances = {t.name: t.amount for t in tranches}
        cash = 0.0
        prev_revenue = base_revenue

        financials = {}
        schedule = {}

        for year in range(1, years + 1):
            growth = revenue_growth[year - 1] if year <= len(revenue_growth) else revenue_growth[-1]
            revenue = prev_revenue * (1 + growth)

            ebitda = revenue * ebitda_margin

            depreciation = revenue * 0.03

            ebit = ebitda - depreciation

            row = {}
            opening = dict(balances)
            interest = 0.0
            for t in tranches:
                row[f'{t.name}_interest'] = opening[t.name] * t.interest_rate
                interest += row[f'{t.name}_interest']

            ebt = ebit - interest

            taxes = max(0, ebt * tax_rate)

            net_income = ebt - taxes

            capex = revenue * capex_pct

            # Working capital tracks revenue, so only its change consumes cash (year 1 included).
            nwc_change = (revenue - prev_revenue) * nwc_pct

            fcf = ebitda - taxes - capex - nwc_change - interest

            cash_available = cash + fcf
            mandatory_total = 0.0
            for t in tranches:
                due = t.amount * t.mandatory_amortization if year <= t.amortization_years else 0.0
                paid = min(due, balances[t.name])
                balances[t.name] -= paid
                mandatory_total += paid
                row[f'{t.name}_mandatory_amort'] = paid
            cash_available -= mandatory_total
            sweep_total = 0.0
            for t in tranches:
                swept = min(max(cash_available, 0.0) * t.sweep_percentage, balances[t.name]) if t.sweep_percentage > 0 else 0.0
                balances[t.name] -= swept
                cash_available -= swept
                sweep_total += swept
                row[f'{t.name}_sweep'] = swept
            cash = cash_available
            for t in tranches:
                row[f'{t.name}_beginning'] = opening[t.name]
                row[f'{t.name}_ending'] = balances[t.name]
                row[f'{t.name}_total_paydown'] = row[f'{t.name}_mandatory_amort'] + row[f'{t.name}_sweep']
            row['total_debt_beginning'] = sum(opening.values())
            row['total_debt_ending'] = sum(balances.values())
            row['total_interest_expense'] = interest
            row['total_mandatory_amort'] = mandatory_total
            row['total_sweep'] = sweep_total
            row['total_debt_paydown'] = mandatory_total + sweep_total
            row['cash_ending'] = cash
            row['net_debt_ending'] = row['total_debt_ending'] - cash
            schedule[year] = row
            prev_revenue = revenue

            financials[year] = {
                'year': year,
                'revenue': revenue,
                'revenue_growth': growth * 100,
                'ebitda': ebitda,
                'ebitda_margin': ebitda_margin * 100,
                'depreciation': depreciation,
                'ebit': ebit,
                'interest_expense': interest,
                'ebt': ebt,
                'taxes': taxes,
                'net_income': net_income,
                'capex': capex,
                'nwc_change': nwc_change,
                'free_cash_flow': fcf,
                'cash_balance': cash
            }

        return financials, schedule

    def _build_sensitivity_analysis(self, initial_equity: float, exit_year: int,
                                   ebitda_projection: List[float],
                                   debt_schedule: Dict[int, Dict[str, Any]]) -> Dict[str, Any]:
        """Build IRR sensitivity tables"""

        exit_ebitda_base = ebitda_projection[exit_year - 1]
        exit_debt = debt_schedule[exit_year]['net_debt_ending']

        ebitda_variations = [exit_ebitda_base * (1 + delta) for delta in [-0.20, -0.10, 0, 0.10, 0.20]]
        multiple_variations = [8.0, 9.0, 10.0, 11.0, 12.0, 13.0]

        irr_matrix = []
        moic_matrix = []

        for ebitda in ebitda_variations:
            irr_row = []
            moic_row = []

            for multiple in multiple_variations:
                exit_ev = ebitda * multiple
                exit_equity = max(0, exit_ev - exit_debt)

                returns = self.returns_calc.comprehensive_returns(initial_equity, exit_equity, exit_year)

                irr_row.append(returns['irr'])
                moic_row.append(returns['moic'])

            irr_matrix.append(irr_row)
            moic_matrix.append(moic_row)

        return {
            'irr_sensitivity': {
                'matrix': irr_matrix,
                'ebitda_variations': ebitda_variations,
                'multiple_variations': multiple_variations
            },
            'moic_sensitivity': {
                'matrix': moic_matrix,
                'ebitda_variations': ebitda_variations,
                'multiple_variations': multiple_variations
            }
        }

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `lbo_model.py <command> <flat-params-json>` (argv length 3). `build` is translated
# to the native positional form and falls through to the native dispatch; `sensitivity` (entry x exit multiple, the
# tab's contract) is answered here. Native invocations (any other argv shape) are untouched.
def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


def _svc_entry(p):
    """Shared flat-param reading for both commands -> dict of floats."""
    ebitda = _svc_num(p, "ebitda", default=None)
    margin = _svc_num(p, "ebitda_margin", default=0.25)
    revenue = _svc_num(p, "revenue", default=None)
    if ebitda is None:
        ebitda = revenue * margin if revenue else 200e6
    if not revenue:
        revenue = ebitda / margin if margin > 0 else ebitda * 4
    entry_multiple = _svc_num(p, "entry_multiple", "entry_base", default=None)
    if entry_multiple is None:
        price = _svc_num(p, "purchase_price", "entry_valuation", "entry_ev", default=None)
        entry_multiple = price / ebitda if (price and ebitda) else 8.0
    exit_multiple = _svc_num(p, "exit_multiple", "exit_base", default=entry_multiple)
    hold = int(_svc_num(p, "holding_period", "hold_years", default=5.0))
    return {"ebitda": ebitda, "margin": margin, "revenue": revenue, "entry_multiple": entry_multiple,
            "exit_multiple": exit_multiple, "hold": max(hold, 1)}


def _svc_assumptions(p, e):
    """LBOModel assumptions from the flat params `p` and the entry/exit/hold values in `e`."""
    a = {
        "entry_multiple": e["entry_multiple"], "exit_multiples": [e["exit_multiple"]],
        "revenue_growth": _svc_num(p, "revenue_growth", default=0.05), "ebitda_margin": e["margin"],
        "capex_pct_revenue": _svc_num(p, "capex_pct_revenue", default=0.03),
        "nwc_pct_revenue": _svc_num(p, "nwc_pct_revenue", default=0.05),
        "tax_rate": _svc_num(p, "tax_rate", default=0.21), "exit_year": e["hold"],
        "hurdle_irr": _svc_num(p, "hurdle_irr", default=0.20), "projection_years": e["hold"],
    }
    senior = _svc_num(p, "senior_debt", default=0.0)
    sub = _svc_num(p, "sub_debt", default=0.0)
    tranches = []
    if senior > 0:  # prepayable term loan: 100% of excess cash sweeps it down
        tranches.append({"name": "Senior", "type": "Term Loan B", "amount": senior, "amortization_years": 7,
                         "rate": _svc_num(p, "senior_rate", default=0.055), "sweep_percentage": 1.0})
    if sub > 0:  # bullet: repaid at exit
        tranches.append({"name": "Subordinated", "type": "Subordinated", "amount": sub, "amortization_years": 10,
                         "rate": _svc_num(p, "sub_rate", default=0.085)})
    if tranches:
        a["debt_tranches"] = tranches
    else:
        # Template structure: leverage can never exceed what the entry price funds.
        leverage = _svc_num(p, "debt_multiple", "target_leverage", default=5.0)
        a["target_leverage"] = min(leverage, max(e["entry_multiple"] - 1.0, 1.0))
    return a


def _svc_target(p, e):
    return {"company_name": str(p.get("company_name", "Target")), "revenue": e["revenue"], "ebitda": e["ebitda"]}


def _service_sensitivity(p):
    """IRR / MOIC over entry multiple x exit multiple, each `entry_base`/`exit_base` +/- `range` in `steps` points."""
    e = _svc_entry(p)
    span = _svc_num(p, "range", default=2.0)
    steps = min(max(int(_svc_num(p, "steps", default=5.0)), 3), 11)

    def axis(centre):
        return [round(max(centre - span + 2.0 * span * i / (steps - 1), 0.5), 2) for i in range(steps)]

    entries, exits = axis(e["entry_multiple"]), axis(e["exit_multiple"])
    target = _svc_target(p, e)
    base = LBOModel(target, _svc_assumptions(p, e)).build_complete_model(e["hold"])
    rows, irr_matrix, moic_matrix = [], [], []
    for entry_multiple in entries:
        irr_row, moic_row = [], []
        for exit_multiple in exits:
            cell = dict(e, entry_multiple=entry_multiple, exit_multiple=exit_multiple)
            try:
                r = LBOModel(target, _svc_assumptions(p, cell)).build_complete_model(e["hold"])["returns"]
            except ValueError:  # the debt does not fit under this entry price
                r = {}
            rows.append({"entry_multiple": entry_multiple, "exit_multiple": exit_multiple,
                         "irr_pct": r.get("irr_pct"), "moic": r.get("moic"),
                         "equity_invested": r.get("entry_equity"), "exit_equity_value": r.get("exit_equity")})
            irr_row.append(r.get("irr_pct"))
            moic_row.append(r.get("moic"))
        irr_matrix.append(irr_row)
        moic_matrix.append(moic_row)
    return {"base_case": base, "sensitivity_matrix": rows, "irr_pct_matrix": irr_matrix, "moic_matrix": moic_matrix,
            "entry_multiple_range": entries, "exit_multiple_range": exits}


def _service_argv(argv):
    """Service `build` -> native positional `build` argv; any other argv is returned untouched."""
    import json
    if len(argv) != 3 or argv[1] != "build":
        return argv
    try:
        p = json.loads(argv[2])
    except ValueError:
        return argv
    if not isinstance(p, dict):
        return argv
    e = _svc_entry(p)
    return [argv[0], "build", json.dumps(_svc_target(p, e)), json.dumps(_svc_assumptions(p, e)), str(e["hold"])]


def _service_dispatch(argv) -> bool:
    """Answer a service-style `sensitivity <flat-json>` call; False for every other argv."""
    import json
    if len(argv) != 3 or argv[1] != "sensitivity":
        return False
    try:
        p = json.loads(argv[2])
    except ValueError:
        return False
    if not isinstance(p, dict):
        return False
    try:
        print(fincept_json.dumps({"success": True, "data": _service_sensitivity(p)}))
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
    sys.argv = _service_argv(sys.argv)
    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: lbo_model.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "build":
            # build_lbo_model(target_company, assumptions, projection_years)
            if len(sys.argv) < 4:
                raise ValueError("Target company data, assumptions, and projection years required")

            target_company = json.loads(sys.argv[2])
            assumptions = json.loads(sys.argv[3])
            projection_years = int(sys.argv[4]) if len(sys.argv) > 4 else 5

            model = LBOModel(target_company, assumptions)
            results = model.build_complete_model(projection_years)

            result = {
                "success": True,
                "data": results
            }
            print(fincept_json.dumps(result))

        elif command == "sensitivity":
            # Host sends: "sensitivity" base_case revenue_growth_scenarios exit_multiple_scenarios
            if len(sys.argv) < 5:
                raise ValueError("Base case, revenue growth scenarios, and exit multiple scenarios required")

            base_case = json.loads(sys.argv[2])
            revenue_growth_scenarios = json.loads(sys.argv[3])
            exit_multiple_scenarios = json.loads(sys.argv[4])

            # Handle both formats:
            # 1. Nested: {company_data: {...}, assumptions: {...}}
            # 2. Flat from frontend: {revenue, ebitda_margin, exit_multiple, debt_percent, holding_period}
            if 'company_data' in base_case:
                target_company = base_case['company_data']
                assumptions = base_case.get('assumptions', {})
            else:
                # Flat format: derive company_data and assumptions from base_case
                revenue = base_case.get('revenue', 1000000000)
                ebitda_margin = base_case.get('ebitda_margin', 0.25)
                ebitda = revenue * ebitda_margin
                target_company = {
                    'company_name': base_case.get('company_name', 'Target'),
                    'revenue': revenue,
                    'ebitda': ebitda,
                }
                assumptions = {
                    'entry_multiple': base_case.get('entry_multiple', 10.0),
                    'exit_multiples': [base_case.get('exit_multiple', 10.0)],
                    'target_leverage': (base_case.get('debt_percent', 60) / 100) * base_case.get('entry_multiple', 10.0),
                    'revenue_growth': revenue_growth_scenarios[len(revenue_growth_scenarios) // 2] if revenue_growth_scenarios else 0.05,
                    'ebitda_margin': ebitda_margin,
                    'capex_pct_revenue': 0.03,
                    'nwc_pct_revenue': 0.05,
                    'tax_rate': 0.21,
                    'exit_year': base_case.get('holding_period', 5),
                    'hurdle_irr': 0.20,
                    'projection_years': base_case.get('holding_period', 5),
                }

            model = LBOModel(target_company, assumptions)
            base_results = model.build_complete_model(assumptions.get('projection_years', 5))

            # Build sensitivity matrix: revenue growth vs exit multiple
            sensitivity_matrix = []
            for rev_growth in revenue_growth_scenarios:
                for exit_mult in exit_multiple_scenarios:
                    modified_assumptions = dict(assumptions)
                    modified_assumptions['revenue_growth'] = rev_growth
                    modified_assumptions['exit_multiples'] = [exit_mult]

                    mod_model = LBOModel(target_company, modified_assumptions)
                    mod_results = mod_model.build_complete_model(modified_assumptions.get('projection_years', 5))

                    # returns_analysis is keyed by exit multiple
                    ret_analysis = mod_results.get('returns_analysis', {})
                    # Pick the returns for the current exit_mult, or first available
                    ret = ret_analysis.get(exit_mult, ret_analysis.get(str(exit_mult), {}))
                    if not ret and ret_analysis:
                        ret = next(iter(ret_analysis.values()))
                    sensitivity_matrix.append({
                        'revenue_growth': rev_growth,
                        'exit_multiple': exit_mult,
                        'irr': ret.get('annualized_return', ret.get('irr', 0)),
                        'moic': ret.get('moic', 0),
                        'equity_value': ret.get('exit_equity_value', 0)
                    })

            # Build 2D matrices for frontend heatmap: rows=revenue_growth, cols=exit_multiple
            irr_matrix = []
            moic_matrix = []
            idx = 0
            for _rg in revenue_growth_scenarios:
                irr_row = []
                moic_row = []
                for _em in exit_multiple_scenarios:
                    entry = sensitivity_matrix[idx]
                    irr_row.append(entry['irr'] / 100)  # frontend multiplies by 100
                    moic_row.append(entry['moic'])
                    idx += 1
                irr_matrix.append(irr_row)
                moic_matrix.append(moic_row)

            result = {
                "success": True,
                "data": {
                    "base_case": base_results,
                    "sensitivity_matrix": sensitivity_matrix,
                    "irr_matrix": irr_matrix,
                    "moic_matrix": moic_matrix,
                    "revenue_growth_range": revenue_growth_scenarios,
                    "exit_multiple_range": exit_multiple_scenarios
                }
            }
            print(fincept_json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: build, sensitivity"
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
