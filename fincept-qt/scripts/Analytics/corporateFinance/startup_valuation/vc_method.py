"""Venture Capital (VC) Method of Valuation"""
from typing import Dict, Any, Optional
import sys

class VCMethod:
    """Venture Capital Method for startup valuation"""

    def __init__(self):
        self.typical_ror_by_stage = {
            'seed': 0.60,
            'series_a': 0.50,
            'series_b': 0.40,
            'series_c': 0.30,
            'late_stage': 0.25
        }

        self.typical_exit_multiples = {
            'revenue': {'saas': 8.0, 'ecommerce': 1.5, 'marketplace': 3.0, 'fintech': 5.0},
            'ebitda': {'default': 12.0},
            'pe': {'default': 20.0}
        }

    def calculate_terminal_value(self, exit_year_metric: float,
                                 exit_multiple: float) -> float:
        """Calculate terminal/exit value"""
        return exit_year_metric * exit_multiple

    def calculate_present_value(self, terminal_value: float,
                               required_ror: float, years_to_exit: int) -> float:
        """Discount terminal value to present"""
        return terminal_value / ((1 + required_ror) ** years_to_exit)

    def calculate_post_money_valuation(self, terminal_value: float,
                                      required_ror: float, years_to_exit: int) -> float:
        """Calculate post-money valuation (before financing)"""
        return self.calculate_present_value(terminal_value, required_ror, years_to_exit)

    def calculate_pre_money_valuation(self, post_money: float,
                                     investment_amount: float) -> float:
        """Calculate pre-money valuation"""
        return post_money - investment_amount

    def calculate_ownership_percentage(self, investment_amount: float,
                                      post_money: float) -> float:
        """Calculate investor ownership percentage"""
        return (investment_amount / post_money) * 100

    def comprehensive_valuation(self, exit_year_metric: float,
                                exit_multiple: float,
                                years_to_exit: int,
                                investment_amount: float,
                                stage: str = 'series_a',
                                custom_ror: Optional[float] = None) -> Dict[str, Any]:
        """
        Complete VC method valuation

        Args:
            exit_year_metric: Projected revenue/EBITDA at exit
            exit_multiple: Exit valuation multiple
            years_to_exit: Years until liquidity event
            investment_amount: Amount being invested
            stage: Funding stage
            custom_ror: Custom required rate of return

        Returns:
            Complete valuation analysis
        """

        required_ror = custom_ror or self.typical_ror_by_stage.get(stage, 0.40)

        terminal_value = self.calculate_terminal_value(exit_year_metric, exit_multiple)

        post_money = self.calculate_post_money_valuation(terminal_value, required_ror, years_to_exit)

        pre_money = self.calculate_pre_money_valuation(post_money, investment_amount)

        ownership_pct = self.calculate_ownership_percentage(investment_amount, post_money)

        investor_exit_value = terminal_value * (ownership_pct / 100)

        investor_return = investor_exit_value / investment_amount

        return {
            'method': 'VC Method',
            'inputs': {
                'exit_year_metric': exit_year_metric,
                'exit_multiple': exit_multiple,
                'years_to_exit': years_to_exit,
                'investment_amount': investment_amount,
                'required_ror': required_ror * 100,
                'stage': stage
            },
            'terminal_value': terminal_value,
            'post_money_valuation': post_money,
            'pre_money_valuation': pre_money,
            'investor_ownership_pct': ownership_pct,
            'investor_exit_value': investor_exit_value,
            'investor_return_multiple': investor_return,
            'investor_irr': required_ror * 100
        }

    def reverse_engineer_valuation(self, investment_amount: float,
                                   target_ownership_pct: float) -> Dict[str, Any]:
        """Calculate implied valuation from desired ownership"""

        post_money = investment_amount / (target_ownership_pct / 100)
        pre_money = post_money - investment_amount

        return {
            'investment_amount': investment_amount,
            'target_ownership_pct': target_ownership_pct,
            'implied_post_money': post_money,
            'implied_pre_money': pre_money
        }

    def scenario_analysis(self, base_case: Dict[str, float],
                         bear_case: Dict[str, float],
                         bull_case: Dict[str, float],
                         investment_amount: float,
                         stage: str = 'series_a') -> Dict[str, Any]:
        """Run scenario analysis with three cases"""

        scenarios = {}

        for case_name, case_data in [('bear', bear_case), ('base', base_case), ('bull', bull_case)]:
            valuation = self.comprehensive_valuation(
                exit_year_metric=case_data['exit_metric'],
                exit_multiple=case_data['exit_multiple'],
                years_to_exit=case_data['years_to_exit'],
                investment_amount=investment_amount,
                stage=stage
            )

            scenarios[case_name] = valuation

        return {
            'scenarios': scenarios,
            'valuation_range': {
                'low': scenarios['bear']['pre_money_valuation'],
                'base': scenarios['base']['pre_money_valuation'],
                'high': scenarios['bull']['pre_money_valuation']
            }
        }

    def calculate_required_exit_multiple(self, current_valuation: float,
                                        investment_amount: float,
                                        target_return: float,
                                        years_to_exit: int,
                                        exit_year_metric: float) -> float:
        """Calculate exit multiple needed for target return"""

        post_money = current_valuation + investment_amount
        ownership_pct = investment_amount / post_money

        required_exit_value = investment_amount * target_return

        required_company_value = required_exit_value / ownership_pct

        required_multiple = required_company_value / exit_year_metric

        return required_multiple

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `vc_method.py calculate <flat-params-json>` (argv length 3). The native form is
# `vc_method exit_metric exit_multiple years investment stage`, so a service-style call is translated here and then
# falls through to the native dispatch. Any other argv shape is untouched.
_SERVICE_COMMANDS = ("calculate",)
_SVC_STAGES = {"early": "series_a", "growth": "series_b"}  # panel stages -> VCMethod stage keys


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
    stage = str(p.get("stage", "series_a")).strip().lower()
    stage = _SVC_STAGES.get(stage, stage)
    return [argv[0], "vc_method", repr(_svc_num(p, "exit_metric", "exit_year_metric", default=0.0)),
            repr(_svc_num(p, "exit_multiple", default=0.0)), str(int(_svc_num(p, "years", "years_to_exit", default=5.0))),
            repr(_svc_num(p, "investment", "investment_amount", default=0.0)), stage]
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
        if command == "vc_method":
            if len(sys.argv) < 7:
                raise ValueError("All VC method parameters required")
            exit_year_metric = float(sys.argv[2])
            exit_multiple = float(sys.argv[3])
            years_to_exit = int(sys.argv[4])
            investment_amount = float(sys.argv[5])
            stage = sys.argv[6]

            vc = VCMethod()
            valuation = vc.comprehensive_valuation(
                exit_year_metric=exit_year_metric,
                exit_multiple=exit_multiple,
                years_to_exit=years_to_exit,
                investment_amount=investment_amount,
                stage=stage
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
