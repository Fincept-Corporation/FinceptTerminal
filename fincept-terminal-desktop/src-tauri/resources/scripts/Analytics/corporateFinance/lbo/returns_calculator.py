"""Returns Calculator - IRR and MOIC"""
from typing import List, Dict, Any
import numpy as np

class ReturnsCalculator:
    """Calculate LBO returns metrics"""

    @staticmethod
    def calculate_irr(cash_flows: List[float]) -> float:
        """Calculate Internal Rate of Return"""
        try:
            irr = np.irr(cash_flows)
            return irr if not np.isnan(irr) else 0
        except:
            return 0

    @staticmethod
    def calculate_moic(initial_investment: float, exit_proceeds: float) -> float:
        """Calculate Multiple on Invested Capital"""
        if initial_investment <= 0:
            return 0
        return exit_proceeds / initial_investment

    @staticmethod
    def calculate_cash_on_cash(initial_equity: float, exit_equity_value: float,
                              interim_distributions: float = 0) -> float:
        """Calculate cash-on-cash return"""
        if initial_equity <= 0:
            return 0

        total_cash_received = exit_equity_value + interim_distributions
        return total_cash_received / initial_equity

    def comprehensive_returns(self, initial_equity: float, exit_equity_value: float,
                            holding_period_years: int,
                            interim_distributions: List[float] = None) -> Dict[str, Any]:
        """Calculate comprehensive return metrics"""

        if interim_distributions is None:
            interim_distributions = []

        cash_flows = [-initial_equity] + interim_distributions + [exit_equity_value]

        irr = self.calculate_irr(cash_flows)
        moic = self.calculate_moic(initial_equity, exit_equity_value)
        coc = self.calculate_cash_on_cash(initial_equity, exit_equity_value, sum(interim_distributions))

        annualized_return = ((moic ** (1 / holding_period_years)) - 1) if holding_period_years > 0 else 0

        return {
            'initial_equity_investment': initial_equity,
            'exit_equity_value': exit_equity_value,
            'holding_period_years': holding_period_years,
            'irr': irr * 100,
            'moic': moic,
            'cash_on_cash_multiple': coc,
            'annualized_return': annualized_return * 100,
            'absolute_gain': exit_equity_value - initial_equity,
            'absolute_gain_pct': ((exit_equity_value - initial_equity) / initial_equity * 100) if initial_equity else 0
        }

    def returns_by_exit_year(self, initial_equity: float, exit_values: Dict[int, float]) -> Dict[int, Dict[str, float]]:
        """Calculate returns for multiple exit scenarios"""

        returns_by_year = {}

        for year, exit_value in exit_values.items():
            returns = self.comprehensive_returns(initial_equity, exit_value, year)
            returns_by_year[year] = returns

        return returns_by_year

    def calculate_hurdle_metrics(self, returns: Dict[str, Any],
                                hurdle_irr: float = 0.20) -> Dict[str, Any]:
        """Calculate excess returns vs hurdle rate"""

        actual_irr = returns['irr'] / 100
        excess_irr = actual_irr - hurdle_irr

        return {
            'hurdle_irr': hurdle_irr * 100,
            'actual_irr': returns['irr'],
            'excess_irr': excess_irr * 100,
            'meets_hurdle': actual_irr >= hurdle_irr,
            'hurdle_moic': (1 + hurdle_irr) ** returns['holding_period_years'],
            'actual_moic': returns['moic'],
            'excess_moic': returns['moic'] - ((1 + hurdle_irr) ** returns['holding_period_years'])
        }

if __name__ == '__main__':
    calc = ReturnsCalculator()

    initial_equity = 1_500_000_000
    exit_equity = 4_200_000_000
    holding_period = 5

    returns = calc.comprehensive_returns(initial_equity, exit_equity, holding_period)

    print(f"Initial Investment: ${returns['initial_equity_investment']:,.0f}")
    print(f"Exit Value: ${returns['exit_equity_value']:,.0f}")
    print(f"Holding Period: {returns['holding_period_years']} years")
    print(f"\nIRR: {returns['irr']:.1f}%")
    print(f"MOIC: {returns['moic']:.2f}x")
    print(f"Absolute Gain: ${returns['absolute_gain']:,.0f} ({returns['absolute_gain_pct']:.1f}%)")

    hurdle = calc.calculate_hurdle_metrics(returns, hurdle_irr=0.20)
    print(f"\nHurdle IRR: {hurdle['hurdle_irr']:.1f}%")
    print(f"Excess IRR: {hurdle['excess_irr']:.1f}%")
    print(f"Meets Hurdle: {hurdle['meets_hurdle']}")
