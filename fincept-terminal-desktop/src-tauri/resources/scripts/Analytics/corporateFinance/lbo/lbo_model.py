"""Complete LBO Model"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

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
        """Build complete LBO model"""

        target_leverage = self.assumptions.get('target_leverage', 5.5)

        self.capital_structure = CapitalStructureBuilder(self.entry_ev, self.entry_ebitda)
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

        financials = self._project_financials(
            revenue_growth, ebitda_margin, capex_pct, nwc_pct, tax_rate, projection_years
        )

        fcf_projection = [fin['free_cash_flow'] for fin in financials.values()]

        self.debt_schedule = DebtSchedule(self.capital_structure.debt_tranches)
        debt_schedule = self.debt_schedule.calculate_annual_schedule(fcf_projection, projection_years)

        ebitda_projection = [fin['ebitda'] for fin in financials.values()]
        interest_coverage = self.debt_schedule.calculate_interest_coverage(ebitda_projection, debt_schedule)
        leverage_progression = self.debt_schedule.calculate_leverage_progression(ebitda_projection, debt_schedule)
        debt_summary = self.debt_schedule.summarize_debt_paydown(debt_schedule)

        exit_year = self.assumptions.get('exit_year', 5)
        exit_ebitda = financials[exit_year]['ebitda']
        exit_multiples = self.assumptions.get('exit_multiples', [8.0, 10.0, 12.0])

        self.exit_analyzer = ExitAnalyzer(exit_ebitda)

        exit_year_debt = debt_schedule[exit_year]['total_debt_ending']
        exit_scenarios = self.exit_analyzer.multiples_exit_analysis(exit_multiples, exit_year_debt)

        returns_analysis = {}
        initial_equity = cap_structure['leverage_metrics']['sponsor_equity_contribution']

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

        # Build a consolidated 'returns' dict for the base exit multiple
        base_exit_mult = self.assumptions.get('exit_multiples', [10.0])
        base_mult_key = str(float(base_exit_mult[len(base_exit_mult) // 2])) if base_exit_mult else None
        base_returns = returns_analysis.get(base_mult_key, {}) if base_mult_key else {}
        if not base_returns and returns_analysis:
            base_returns = next(iter(returns_analysis.values()))

        # Build sources & uses for frontend
        total_debt = cap_structure.get('leverage_metrics', {}).get('total_debt', 0)
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
                'transaction_fees': self.entry_ev * 0.02,
                'financing_fees': total_debt * 0.015,
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
                'irr': base_returns.get('irr', 0) / 100,
                'moic': base_returns.get('moic', 0),
                'entry_equity': initial_equity,
                'exit_equity': base_returns.get('exit_equity_value', 0),
            },
            'sources_uses': sources_uses,
            'sensitivity_analysis': sensitivity
        }

    def _project_financials(self, revenue_growth: List[float], ebitda_margin: float,
                           capex_pct: float, nwc_pct: float, tax_rate: float,
                           years: int) -> Dict[int, Dict[str, float]]:
        """Project financial statements"""

        base_revenue = self.target.get('revenue', self.entry_ebitda / ebitda_margin)

        financials = {}

        for year in range(1, years + 1):
            growth = revenue_growth[year - 1] if year <= len(revenue_growth) else revenue_growth[-1]

            if year == 1:
                revenue = base_revenue * (1 + growth)
            else:
                revenue = financials[year - 1]['revenue'] * (1 + growth)

            ebitda = revenue * ebitda_margin

            depreciation = revenue * 0.03

            ebit = ebitda - depreciation

            interest = self.capital_structure.calculate_annual_interest_expense(year) if self.capital_structure else 0

            ebt = ebit - interest

            taxes = max(0, ebt * tax_rate)

            net_income = ebt - taxes

            capex = revenue * capex_pct

            if year == 1:
                nwc_change = revenue * nwc_pct
            else:
                nwc_change = (revenue - financials[year - 1]['revenue']) * nwc_pct

            fcf = ebitda - taxes - capex - nwc_change - interest

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
                'free_cash_flow': fcf
            }

        return financials

    def _build_sensitivity_analysis(self, initial_equity: float, exit_year: int,
                                   ebitda_projection: List[float],
                                   debt_schedule: Dict[int, Dict[str, Any]]) -> Dict[str, Any]:
        """Build IRR sensitivity tables"""

        exit_ebitda_base = ebitda_projection[exit_year - 1]
        exit_debt = debt_schedule[exit_year]['total_debt_ending']

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

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

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
            print(json.dumps(result))

        elif command == "sensitivity":
            # Rust sends: "sensitivity" base_case revenue_growth_scenarios exit_multiple_scenarios
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
            print(json.dumps(result))

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
