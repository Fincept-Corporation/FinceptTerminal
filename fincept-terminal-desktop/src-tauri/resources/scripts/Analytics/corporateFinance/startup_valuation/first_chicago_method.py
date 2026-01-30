"""First Chicago Method - Scenario-Based Valuation"""
from typing import Dict, Any, List
from dataclasses import dataclass

@dataclass
class Scenario:
    """Valuation scenario"""
    name: str
    probability: float
    exit_year: int
    exit_value: float
    description: str

class FirstChicagoMethod:
    """First Chicago Method for scenario-based startup valuation"""

    def __init__(self):
        self.discount_rate = 0.40

    def create_scenario(self, name: str, probability: float,
                       exit_year: int, exit_value: float,
                       description: str = "") -> Scenario:
        """Create valuation scenario"""

        if not 0 <= probability <= 1:
            raise ValueError("Probability must be between 0 and 1")

        return Scenario(name, probability, exit_year, exit_value, description)

    def discount_to_present(self, future_value: float, years: int,
                          discount_rate: Optional[float] = None) -> float:
        """Discount future value to present"""

        rate = discount_rate or self.discount_rate
        return future_value / ((1 + rate) ** years)

    def calculate_expected_value(self, scenarios: List[Scenario],
                                discount_rate: Optional[float] = None) -> Dict[str, Any]:
        """Calculate probability-weighted expected value"""

        rate = discount_rate or self.discount_rate

        total_probability = sum(s.probability for s in scenarios)
        if abs(total_probability - 1.0) > 0.01:
            raise ValueError(f"Probabilities must sum to 1.0, got {total_probability}")

        scenario_details = []
        weighted_pv_sum = 0

        for scenario in scenarios:
            pv = self.discount_to_present(scenario.exit_value, scenario.exit_year, rate)
            weighted_pv = pv * scenario.probability

            weighted_pv_sum += weighted_pv

            scenario_details.append({
                'name': scenario.name,
                'probability': scenario.probability * 100,
                'exit_year': scenario.exit_year,
                'exit_value': scenario.exit_value,
                'present_value': pv,
                'weighted_present_value': weighted_pv,
                'description': scenario.description
            })

        return {
            'method': 'First Chicago Method',
            'scenarios': scenario_details,
            'discount_rate': rate * 100,
            'expected_present_value': weighted_pv_sum,
            'valuation': weighted_pv_sum
        }

    def three_scenario_valuation(self, best_case: Dict[str, Any],
                                 base_case: Dict[str, Any],
                                 worst_case: Dict[str, Any],
                                 probabilities: Optional[Dict[str, float]] = None) -> Dict[str, Any]:
        """
        Standard three-scenario valuation

        Args:
            best_case: {'exit_value': float, 'exit_year': int, 'description': str}
            base_case: {...}
            worst_case: {...}
            probabilities: Optional custom probabilities (default: 20%, 50%, 30%)

        Returns:
            Expected valuation
        """

        if probabilities is None:
            probabilities = {'best': 0.20, 'base': 0.50, 'worst': 0.30}

        scenarios = [
            self.create_scenario(
                'Best Case',
                probabilities['best'],
                best_case['exit_year'],
                best_case['exit_value'],
                best_case.get('description', '')
            ),
            self.create_scenario(
                'Base Case',
                probabilities['base'],
                base_case['exit_year'],
                base_case['exit_value'],
                base_case.get('description', '')
            ),
            self.create_scenario(
                'Worst Case',
                probabilities['worst'],
                worst_case['exit_year'],
                worst_case['exit_value'],
                worst_case.get('description', '')
            )
        ]

        return self.calculate_expected_value(scenarios)

    def sensitivity_to_probabilities(self, scenarios: List[Scenario],
                                    variable_scenario_index: int) -> Dict[str, Any]:
        """Analyze sensitivity to scenario probabilities"""

        results = []

        for prob in [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7]:
            adjusted_scenarios = scenarios.copy()

            remaining_prob = 1.0 - prob
            other_scenarios = [i for i in range(len(scenarios)) if i != variable_scenario_index]

            original_other_total = sum(scenarios[i].probability for i in other_scenarios)

            adjusted_scenarios[variable_scenario_index] = Scenario(
                scenarios[variable_scenario_index].name,
                prob,
                scenarios[variable_scenario_index].exit_year,
                scenarios[variable_scenario_index].exit_value,
                scenarios[variable_scenario_index].description
            )

            for i in other_scenarios:
                new_prob = (scenarios[i].probability / original_other_total) * remaining_prob
                adjusted_scenarios[i] = Scenario(
                    scenarios[i].name,
                    new_prob,
                    scenarios[i].exit_year,
                    scenarios[i].exit_value,
                    scenarios[i].description
                )

            valuation = self.calculate_expected_value(adjusted_scenarios)

            results.append({
                'variable_probability': prob * 100,
                'valuation': valuation['expected_present_value']
            })

        return {
            'variable_scenario': scenarios[variable_scenario_index].name,
            'sensitivity_data': results
        }

    def calculate_breakeven_probability(self, scenarios: List[Scenario],
                                       current_investment: float,
                                       success_scenario_index: int = 0) -> float:
        """Calculate probability needed for scenario to break even on investment"""

        success_scenario = scenarios[success_scenario_index]
        other_scenarios = [s for i, s in enumerate(scenarios) if i != success_scenario_index]

        success_pv = self.discount_to_present(success_scenario.exit_value, success_scenario.exit_year)

        other_weighted_pv = sum(
            self.discount_to_present(s.exit_value, s.exit_year) * s.probability
            for s in other_scenarios
        )

        other_total_prob = sum(s.probability for s in other_scenarios)

        if success_pv <= 0:
            return 1.0

        breakeven_prob = (current_investment - other_weighted_pv) / success_pv

        return max(0, min(1, breakeven_prob))

if __name__ == '__main__':
    fc = FirstChicagoMethod()

    valuation = fc.three_scenario_valuation(
        best_case={
            'exit_value': 500_000_000,
            'exit_year': 4,
            'description': 'IPO or strategic acquisition at premium'
        },
        base_case={
            'exit_value': 150_000_000,
            'exit_year': 5,
            'description': 'Moderate success, trade sale'
        },
        worst_case={
            'exit_value': 20_000_000,
            'exit_year': 6,
            'description': 'Acqui-hire or distressed sale'
        }
    )

    print("=== FIRST CHICAGO METHOD VALUATION ===\n")
    print(f"Expected Valuation: ${valuation['expected_present_value']:,.0f}")
    print(f"Discount Rate: {valuation['discount_rate']:.0f}%\n")

    print("Scenario Analysis:")
    for scenario in valuation['scenarios']:
        print(f"\n{scenario['name']} ({scenario['probability']:.0f}% probability):")
        print(f"  Exit Value (Year {scenario['exit_year']}): ${scenario['exit_value']:,.0f}")
        print(f"  Present Value: ${scenario['present_value']:,.0f}")
        print(f"  Weighted PV: ${scenario['weighted_present_value']:,.0f}")

    scenarios = [
        fc.create_scenario('Best', 0.20, 4, 500_000_000),
        fc.create_scenario('Base', 0.50, 5, 150_000_000),
        fc.create_scenario('Worst', 0.30, 6, 20_000_000)
    ]

    breakeven = fc.calculate_breakeven_probability(scenarios, 15_000_000, success_scenario_index=0)
    print(f"\nBreakeven Best Case Probability: {breakeven * 100:.1f}%")
