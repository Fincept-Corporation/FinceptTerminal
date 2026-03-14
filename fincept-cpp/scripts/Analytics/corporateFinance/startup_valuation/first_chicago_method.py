"""First Chicago Method - Scenario-Based Valuation"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
import sys

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

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "first_chicago":
            if len(sys.argv) < 3:
                raise ValueError("Scenarios required")
            raw = json.loads(sys.argv[2])
            fc = FirstChicagoMethod()

            # Accept either a flat list [{name, probability, exit_year, exit_value, description}, ...]
            # or the legacy dict {best_case: {...}, base_case: {...}, worst_case: {...}}
            if isinstance(raw, list):
                scenarios = [
                    Scenario(
                        name=s.get("name", "Scenario"),
                        probability=float(s.get("probability", 0.33)),
                        exit_year=int(s.get("exit_year", s.get("years_to_exit", 5))),
                        exit_value=float(s.get("exit_value", 0)),
                        description=s.get("description", ""),
                    )
                    for s in raw
                ]
                valuation = fc.calculate_expected_value(scenarios)
            else:
                # Legacy dict format: {best_case, base_case, worst_case}
                for key in ("best_case", "base_case", "worst_case"):
                    sc = raw.get(key, {})
                    if "exit_year" not in sc:
                        sc["exit_year"] = sc.get("years_to_exit", sc.get("timeline_years", 5))
                    raw[key] = sc

                probs = raw.get("probabilities", None)
                if probs is None:
                    bc = raw.get("best_case", {})
                    ba = raw.get("base_case", {})
                    wc = raw.get("worst_case", {})
                    if "probability" in bc or "probability" in ba or "probability" in wc:
                        probs = {
                            "best": bc.get("probability", 0.20),
                            "base": ba.get("probability", 0.50),
                            "worst": wc.get("probability", 0.30)
                        }

                valuation = fc.three_scenario_valuation(
                    best_case=raw.get("best_case", {}),
                    base_case=raw.get("base_case", {}),
                    worst_case=raw.get("worst_case", {}),
                    probabilities=probs
                )

            result = {"success": True, "data": valuation}
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