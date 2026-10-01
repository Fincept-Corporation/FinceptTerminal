"""Debt Schedule and Amortization Calculator"""
import sys
from pathlib import Path
from typing import Dict, Any, List

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.lbo.capital_structure import DebtTranche

class DebtSchedule:
    """Calculate debt amortization schedule"""

    def __init__(self, debt_tranches: List[DebtTranche]):
        self.tranches = debt_tranches

    def calculate_annual_schedule(self, fcf_projection: List[float],
                                 years: int = 7) -> Dict[str, Any]:
        """Calculate annual debt schedule with cash flow sweep"""

        schedule = {year: {} for year in range(1, years + 1)}

        tranche_balances = {t.name: t.amount for t in self.tranches}

        for year in range(1, years + 1):
            year_data = {}

            beginning_balances = {name: bal for name, bal in tranche_balances.items()}

            total_mandatory_amort = 0
            for tranche in self.tranches:
                if year <= tranche.amortization_years:
                    mandatory = tranche.amount * tranche.mandatory_amortization
                    total_mandatory_amort += mandatory
                else:
                    mandatory = 0

                year_data[f'{tranche.name}_mandatory_amort'] = mandatory

            total_interest = 0
            for tranche in self.tranches:
                outstanding = tranche_balances[tranche.name]
                interest = outstanding * tranche.interest_rate
                total_interest += interest
                year_data[f'{tranche.name}_interest'] = interest

            fcf_available = fcf_projection[year - 1] if year <= len(fcf_projection) else 0
            cash_available_for_sweep = max(0, fcf_available - total_mandatory_amort)

            total_sweep = 0
            for tranche in self.tranches:
                if tranche.sweep_percentage > 0:
                    sweep_amount = min(
                        cash_available_for_sweep * tranche.sweep_percentage,
                        tranche_balances[tranche.name]
                    )
                    total_sweep += sweep_amount
                    year_data[f'{tranche.name}_sweep'] = sweep_amount
                else:
                    year_data[f'{tranche.name}_sweep'] = 0

            for tranche in self.tranches:
                mandatory = year_data[f'{tranche.name}_mandatory_amort']
                sweep = year_data[f'{tranche.name}_sweep']
                total_paydown = mandatory + sweep

                new_balance = max(0, tranche_balances[tranche.name] - total_paydown)
                tranche_balances[tranche.name] = new_balance

                year_data[f'{tranche.name}_beginning'] = beginning_balances[tranche.name]
                year_data[f'{tranche.name}_ending'] = new_balance
                year_data[f'{tranche.name}_total_paydown'] = total_paydown

            year_data['total_debt_beginning'] = sum(beginning_balances.values())
            year_data['total_debt_ending'] = sum(tranche_balances.values())
            year_data['total_interest_expense'] = total_interest
            year_data['total_mandatory_amort'] = total_mandatory_amort
            year_data['total_sweep'] = total_sweep
            year_data['total_debt_paydown'] = total_mandatory_amort + total_sweep

            schedule[year] = year_data

        return schedule

    def calculate_interest_coverage(self, ebitda_projection: List[float],
                                   schedule: Dict[int, Dict[str, Any]]) -> Dict[int, float]:
        """Calculate interest coverage ratios"""

        coverage_ratios = {}

        for year, data in schedule.items():
            if year <= len(ebitda_projection):
                ebitda = ebitda_projection[year - 1]
                interest = data['total_interest_expense']

                coverage = ebitda / interest if interest > 0 else float('inf')
                coverage_ratios[year] = coverage

        return coverage_ratios

    def calculate_leverage_progression(self, ebitda_projection: List[float],
                                      schedule: Dict[int, Dict[str, Any]]) -> Dict[int, float]:
        """Calculate Debt/EBITDA progression"""

        leverage_ratios = {}

        for year, data in schedule.items():
            if year <= len(ebitda_projection):
                ebitda = ebitda_projection[year - 1]
                debt = data['total_debt_ending']

                leverage = debt / ebitda if ebitda > 0 else float('inf')
                leverage_ratios[year] = leverage

        return leverage_ratios

    def summarize_debt_paydown(self, schedule: Dict[int, Dict[str, Any]]) -> Dict[str, Any]:
        """Summarize total debt paydown"""

        initial_debt = schedule[1]['total_debt_beginning']
        final_debt = schedule[max(schedule.keys())]['total_debt_ending']

        total_paydown = initial_debt - final_debt
        paydown_pct = (total_paydown / initial_debt * 100) if initial_debt else 0

        total_interest_paid = sum(data['total_interest_expense'] for data in schedule.values())

        return {
            'initial_debt': initial_debt,
            'final_debt': final_debt,
            'total_paydown': total_paydown,
            'paydown_percentage': paydown_pct,
            'total_interest_paid': total_interest_paid
        }

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `debt_schedule.py analyze <flat-params-json>` (argv length 3). The native form is
# `debt_schedule debt_structure cash_flows [sweep]`, so a service-style call is translated here and then falls
# through to the native dispatch. Any other argv shape is untouched.
_SERVICE_COMMANDS = ("analyze",)


def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


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
    senior = _svc_num(p, "senior_debt", default=0.0)
    sub = _svc_num(p, "sub_debt", default=0.0)
    revolver = _svc_num(p, "revolver", default=0.0)
    structure = {
        "senior": {"amount": senior, "rate": _svc_num(p, "senior_rate", default=0.055), "term": 7},
        "subordinated": {"amount": sub, "rate": _svc_num(p, "sub_rate", default=0.085), "term": 10},
        "revolver": {"amount": revolver, "rate": _svc_num(p, "revolver_rate", default=0.04)},
    }
    years = int(_svc_num(p, "years", "projection_years", default=7.0))
    years = min(max(years, 1), 20)
    ebitda = _svc_num(p, "ebitda", default=0.0)
    growth = _svc_num(p, "ebitda_growth", default=0.0)
    capex = _svc_num(p, "capex", default=0.0)
    tax_rate = _svc_num(p, "tax_rate", default=0.21)
    # Interest is approximated at the opening balances (the schedule recomputes it as debt is repaid); cash
    # taxes at `tax_rate` of EBITDA less capex.
    interest = (senior * structure["senior"]["rate"] + sub * structure["subordinated"]["rate"]
                + revolver * structure["revolver"]["rate"])
    ebitda_by_year = [ebitda * (1.0 + growth) ** i for i in range(years)]
    cash_flows = {
        "ebitda": ebitda_by_year,
        "capex": [capex] * years,
        "taxes": [max(e - capex, 0.0) * tax_rate for e in ebitda_by_year],
        "interest_paid": [interest] * years,
    }
    return [argv[0], "debt_schedule", json.dumps(structure), json.dumps(cash_flows),
            repr(_svc_num(p, "sweep_pct", "sweep_percentage", default=0.5))]
# ---- service ABI shim (MAAnalyticsService) END ----


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    import json

    sys.argv = _service_argv(sys.argv)
    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified. Usage: debt_schedule.py <command> [args...]"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "debt_schedule":
            if len(sys.argv) < 4:
                raise ValueError("Debt structure, cash flows, and sweep percentage required")
            debt_structure = json.loads(sys.argv[2])
            cash_flows = json.loads(sys.argv[3])
            sweep_percentage = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5

            from corporateFinance.lbo.capital_structure import DebtTranche

            tranches = []

            # Handle frontend format: {senior: {amount, rate, term}, subordinated: {...}, revolver: {...}}
            if isinstance(debt_structure, dict) and 'senior' in debt_structure:
                senior = debt_structure.get('senior', {})
                if senior.get('amount', 0) > 0:
                    tranches.append(DebtTranche(
                        name='Senior', amount=senior['amount'],
                        interest_rate=senior.get('rate', 0.055),
                        amortization_years=senior.get('term', 7),
                        type='Term Loan B',
                        mandatory_amortization=0.01,
                        sweep_percentage=sweep_percentage
                    ))
                sub = debt_structure.get('subordinated', {})
                if sub.get('amount', 0) > 0:
                    tranches.append(DebtTranche(
                        name='Subordinated', amount=sub['amount'],
                        interest_rate=sub.get('rate', 0.085),
                        amortization_years=sub.get('term', 10),
                        type='Subordinated',
                        mandatory_amortization=0,
                        sweep_percentage=0
                    ))
                rev = debt_structure.get('revolver', {})
                if rev.get('amount', 0) > 0:
                    tranches.append(DebtTranche(
                        name='Revolver', amount=rev['amount'],
                        interest_rate=rev.get('rate', 0.04),
                        amortization_years=5,
                        type='Revolver',
                        mandatory_amortization=0,
                        sweep_percentage=0
                    ))
            else:
                # Handle standard format: {tranches: [{name, amount, ...}]} or array
                for t in (debt_structure.get('tranches', []) if isinstance(debt_structure, dict) else debt_structure):
                    tranche = DebtTranche(
                        name=t.get('name', ''),
                        amount=t.get('amount', t.get('principal', 0)),
                        interest_rate=t.get('interest_rate', 0),
                        amortization_years=t.get('amortization_years', t.get('maturity_years', t.get('maturity', 7))),
                        type=t.get('type', t.get('tranche_type', 'Term Loan')),
                        mandatory_amortization=t.get('mandatory_amortization', t.get('mandatory_amort_pct', t.get('amortization_rate', 0))),
                        sweep_percentage=t.get('sweep_percentage', t.get('sweep_priority', 0))
                    )
                    tranches.append(tranche)

            # Handle cash flows: frontend sends {ebitda: [], capex: [], interest_paid: [], taxes: []}
            if isinstance(cash_flows, dict):
                ebitda = cash_flows.get('ebitda', [])
                capex = cash_flows.get('capex', [0] * len(ebitda))
                taxes = cash_flows.get('taxes', [0] * len(ebitda))
                interest = cash_flows.get('interest_paid', [0] * len(ebitda))
                fcf_list = [e - c - t - i for e, c, t, i in zip(ebitda, capex, taxes, interest)]
            elif isinstance(cash_flows, list):
                fcf_list = cash_flows
            else:
                fcf_list = []

            debt_sched = DebtSchedule(tranches)
            schedule = debt_sched.calculate_annual_schedule(
                fcf_list,
                years=len(fcf_list) if fcf_list else 7
            )
            raw_summary = debt_sched.summarize_debt_paydown(schedule)

            # Transform schedule dict to array with frontend-expected field names
            schedule_array = []
            for year_key in sorted(schedule.keys(), key=lambda k: int(k)):
                row = schedule[year_key]
                schedule_array.append({
                    'year': int(year_key),
                    'opening': row.get('total_debt_beginning', 0),
                    'interest': row.get('total_interest_expense', 0),
                    'paydown': row.get('total_debt_paydown', 0),
                    'closing': row.get('total_debt_ending', 0),
                })

            # Transform summary to frontend-expected field names
            summary = {
                'total_debt': raw_summary.get('initial_debt', 0),
                'total_interest': raw_summary.get('total_interest_paid', 0),
                'total_paydown': raw_summary.get('total_paydown', 0),
                'exit_debt': raw_summary.get('final_debt', 0),
                'paydown_percentage': raw_summary.get('paydown_percentage', 0),
            }

            result = {"success": True, "data": {"schedule": schedule_array, "summary": summary}}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}. Available: debt_schedule"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e), "command": command}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
