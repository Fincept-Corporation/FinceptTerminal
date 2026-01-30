"""Complete LBO Model"""
from typing import Dict, Any, List, Optional
from .capital_structure import CapitalStructureBuilder
from .debt_schedule import DebtSchedule
from .returns_calculator import ReturnsCalculator
from .exit_analysis import ExitAnalyzer

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

        revenue_growth = self.assumptions.get('revenue_growth', [0.05] * projection_years)
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

if __name__ == '__main__':
    target_company = {
        'company_name': 'Target LBO Co',
        'revenue': 3_000_000_000,
        'ebitda': 625_000_000
    }

    assumptions = {
        'entry_multiple': 8.0,
        'target_leverage': 5.5,
        'revenue_growth': [0.05, 0.06, 0.07, 0.07, 0.08],
        'ebitda_margin': 0.25,
        'capex_pct_revenue': 0.03,
        'nwc_pct_revenue': 0.05,
        'tax_rate': 0.21,
        'exit_year': 5,
        'exit_multiples': [9.0, 10.0, 11.0, 12.0],
        'hurdle_irr': 0.20
    }

    model = LBOModel(target_company, assumptions)
    results = model.build_complete_model(projection_years=5)

    print("=== LBO MODEL RESULTS ===\n")
    print(f"Entry EV: ${results['transaction_summary']['entry_enterprise_value']:,.0f}")
    print(f"Entry Multiple: {results['transaction_summary']['entry_multiple']:.1f}x")
    print(f"Total Leverage: {results['capital_structure']['leverage_metrics']['debt_to_ebitda']:.2f}x\n")

    print("Returns by Exit Multiple:")
    for multiple, returns in results['returns_analysis'].items():
        print(f"  {multiple:.1f}x: IRR {returns['irr']:.1f}% | MOIC {returns['moic']:.2f}x")

    print(f"\nDebt Paydown: ${results['debt_summary']['total_paydown']:,.0f} ({results['debt_summary']['paydown_percentage']:.1f}%)")
