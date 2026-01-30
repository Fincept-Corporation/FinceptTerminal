"""Venture Capital (VC) Method of Valuation"""
from typing import Dict, Any, Optional

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

if __name__ == '__main__':
    vc = VCMethod()

    valuation = vc.comprehensive_valuation(
        exit_year_metric=100_000_000,
        exit_multiple=8.0,
        years_to_exit=5,
        investment_amount=10_000_000,
        stage='series_a'
    )

    print("=== VC METHOD VALUATION ===\n")
    print(f"Exit Value (Year 5): ${valuation['terminal_value']:,.0f}")
    print(f"Required ROR: {valuation['inputs']['required_ror']:.0f}%")
    print(f"\nPre-Money Valuation: ${valuation['pre_money_valuation']:,.0f}")
    print(f"Post-Money Valuation: ${valuation['post_money_valuation']:,.0f}")
    print(f"\nInvestor Ownership: {valuation['investor_ownership_pct']:.1f}%")
    print(f"Investor Return: {valuation['investor_return_multiple']:.2f}x")

    scenarios = vc.scenario_analysis(
        base_case={'exit_metric': 100_000_000, 'exit_multiple': 8.0, 'years_to_exit': 5},
        bear_case={'exit_metric': 60_000_000, 'exit_multiple': 6.0, 'years_to_exit': 6},
        bull_case={'exit_metric': 150_000_000, 'exit_multiple': 10.0, 'years_to_exit': 4},
        investment_amount=10_000_000
    )

    print(f"\nValuation Range:")
    print(f"  Bear: ${scenarios['valuation_range']['low']:,.0f}")
    print(f"  Base: ${scenarios['valuation_range']['base']:,.0f}")
    print(f"  Bull: ${scenarios['valuation_range']['high']:,.0f}")
