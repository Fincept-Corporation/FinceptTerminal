"""Exit Analysis for LBO"""
from typing import Dict, Any, List
import numpy as np

class ExitAnalyzer:
    """Analyze exit scenarios and valuations"""

    def __init__(self, exit_year_ebitda: float):
        self.exit_ebitda = exit_year_ebitda

    def calculate_exit_enterprise_value(self, exit_multiple: float) -> float:
        """Calculate exit enterprise value"""
        return self.exit_ebitda * exit_multiple

    def calculate_exit_equity_value(self, exit_ev: float, exit_year_debt: float) -> float:
        """Calculate exit equity value"""
        return max(0, exit_ev - exit_year_debt)

    def multiples_exit_analysis(self, exit_multiples: List[float],
                               exit_year_debt: float) -> Dict[float, Dict[str, float]]:
        """Analyze exit values across multiple exit multiples"""

        exit_scenarios = {}

        for multiple in exit_multiples:
            exit_ev = self.calculate_exit_enterprise_value(multiple)
            exit_equity = self.calculate_exit_equity_value(exit_ev, exit_year_debt)

            exit_scenarios[multiple] = {
                'exit_multiple': multiple,
                'exit_ebitda': self.exit_ebitda,
                'exit_enterprise_value': exit_ev,
                'exit_debt': exit_year_debt,
                'exit_equity_value': exit_equity
            }

        return exit_scenarios

    def calculate_implied_exit_multiple(self, target_irr: float, initial_equity: float,
                                       holding_period: int, exit_year_debt: float) -> float:
        """Calculate exit multiple needed for target IRR"""

        target_exit_equity = initial_equity * ((1 + target_irr) ** holding_period)

        required_ev = target_exit_equity + exit_year_debt

        implied_multiple = required_ev / self.exit_ebitda if self.exit_ebitda else 0

        return implied_multiple

    def sensitivity_table(self, ebitda_range: List[float], multiple_range: List[float],
                        exit_year_debt: float) -> Dict[str, Any]:
        """Two-way sensitivity: Exit EBITDA vs Exit Multiple"""

        matrix = []

        for ebitda in ebitda_range:
            row = []
            for multiple in multiple_range:
                exit_ev = ebitda * multiple
                exit_equity = max(0, exit_ev - exit_year_debt)
                row.append(exit_equity)
            matrix.append(row)

        return {
            'matrix': matrix,
            'ebitda_range': ebitda_range,
            'multiple_range': multiple_range,
            'base_ebitda': self.exit_ebitda,
            'exit_debt': exit_year_debt
        }

    def comparable_exit_multiples(self, industry: str) -> Dict[str, float]:
        """Historical exit multiples by industry"""

        typical_multiples = {
            'Technology': {'low': 8.0, 'median': 12.0, 'high': 16.0},
            'Healthcare': {'low': 9.0, 'median': 12.5, 'high': 16.0},
            'Industrials': {'low': 7.0, 'median': 10.0, 'high': 13.0},
            'Consumer': {'low': 8.0, 'median': 11.0, 'high': 14.0},
            'Financial Services': {'low': 9.0, 'median': 11.5, 'high': 14.0}
        }

        return typical_multiples.get(industry, {'low': 7.0, 'median': 10.0, 'high': 13.0})

if __name__ == '__main__':
    analyzer = ExitAnalyzer(exit_year_ebitda=850_000_000)

    exit_multiples = [8.0, 10.0, 12.0, 14.0]
    exit_debt = 1_200_000_000

    scenarios = analyzer.multiples_exit_analysis(exit_multiples, exit_debt)

    print("Exit Scenarios:\n")
    for multiple, data in scenarios.items():
        print(f"{multiple:.1f}x EBITDA:")
        print(f"  EV: ${data['exit_enterprise_value']:,.0f}")
        print(f"  Equity Value: ${data['exit_equity_value']:,.0f}\n")

    implied_multiple = analyzer.calculate_implied_exit_multiple(
        target_irr=0.25,
        initial_equity=1_500_000_000,
        holding_period=5,
        exit_year_debt=exit_debt
    )

    print(f"Implied Multiple for 25% IRR: {implied_multiple:.2f}x")
