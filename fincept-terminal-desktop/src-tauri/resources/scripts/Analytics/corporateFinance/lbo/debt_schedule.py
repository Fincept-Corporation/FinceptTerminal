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

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

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
            for t in debt_structure.get('tranches', []):
                tranche = DebtTranche(
                    t.get('name', ''),
                    t.get('principal', 0),
                    t.get('interest_rate', 0),
                    t.get('maturity', 0),
                    t.get('tranche_type', ''),
                    t.get('amortization_rate', 0),
                    t.get('sweep_priority', 0)
                )
                tranches.append(tranche)

            debt_schedule = DebtSchedule(tranches)
            schedule = debt_schedule.calculate_annual_schedule(
                cash_flows.get('fcf_projection', []),
                years=len(cash_flows.get('fcf_projection', [])),
                sweep_percentage=sweep_percentage
            )
            summary = debt_schedule.summarize_debt_paydown(schedule)

            result = {"success": True, "data": {"schedule": schedule, "summary": summary}}
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
