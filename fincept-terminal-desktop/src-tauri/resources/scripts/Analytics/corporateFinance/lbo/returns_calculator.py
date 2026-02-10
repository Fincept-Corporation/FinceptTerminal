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

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "returns":
            if len(sys.argv) < 6:
                raise ValueError("Entry valuation, exit valuation, equity invested, and holding period required")

            entry_valuation = float(sys.argv[2])  # Not directly used, but enterprise value at entry
            exit_valuation = float(sys.argv[3])    # Not directly used, but enterprise value at exit
            equity_invested = float(sys.argv[4])   # Initial equity investment
            holding_period = int(sys.argv[5])

            # Calculate exit equity value from exit enterprise valuation
            # In an LBO, equity value at exit = exit EV - remaining debt
            # For simplification, we'll use exit_valuation directly as exit equity
            # In real scenario, you'd subtract remaining debt

            calc = ReturnsCalculator()
            analysis = calc.comprehensive_returns(equity_invested, exit_valuation, holding_period)

            result = {"success": True, "data": analysis}
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
