"""Returns Calculator - IRR and MOIC for LBO equity.

CLI (python returns_calculator.py <command> <args>):
  calculate <json>  LBO Returns tab. Keys: entry_valuation (entry EV), exit_valuation (exit EV), equity_invested,
                    holding_period (years), senior_debt + senior_rate, sub_debt + sub_rate (rates as fractions);
                    aliases entry_ev, exit_ev, hold_years, debt (+ debt_rate). Debt is held to exit (no paydown in
                    this calculator; the LBO Model tab models it) with cash-pay interest, so exit equity =
                    exit EV - debt. Also reports the sources/uses gap against the entry EV and the interest carry.
  returns <entry_ev> <exit_equity> <equity_invested> <holding_period>   native positional form.
Output {"success": true, "data": {...}}; irr is null when no IRR exists (never a silent 0).
"""
import os
import sys
from typing import List, Dict, Any, Optional

try:
    from fincept_irr import irr as _irr
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..")))
    from fincept_irr import irr as _irr


class ReturnsCalculator:
    """Calculate LBO returns metrics"""

    @staticmethod
    def calculate_irr(cash_flows: List[float]) -> Optional[float]:
        """IRR as a fraction, or None when the cash flows have none (see fincept_irr)."""
        return _irr(cash_flows)

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
        """Return metrics for equity invested at t=0 and exited at the end of year `holding_period_years`;
        `interim_distributions[i]` is paid at the end of year i+1."""

        if interim_distributions is None:
            interim_distributions = []

        # One cash flow per year: the exit lands at t = holding period, not at t = 1.
        periods = max(int(holding_period_years), 1)
        cash_flows = [-initial_equity] + [0.0] * periods
        for i, dist in enumerate(interim_distributions):
            cash_flows[min(i + 1, periods)] += dist
        cash_flows[periods] += exit_equity_value

        irr = self.calculate_irr(cash_flows)
        moic = self.calculate_moic(initial_equity, exit_equity_value)
        coc = self.calculate_cash_on_cash(initial_equity, exit_equity_value, sum(interim_distributions))

        annualized_return = ((moic ** (1 / holding_period_years)) - 1) if holding_period_years > 0 else 0

        return {
            'initial_equity_investment': initial_equity,
            'exit_equity_value': exit_equity_value,
            'holding_period_years': holding_period_years,
            'irr': irr * 100 if irr is not None else None,
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

        actual_irr = returns['irr'] / 100 if returns['irr'] is not None else None

        return {
            'hurdle_irr': hurdle_irr * 100,
            'actual_irr': returns['irr'],
            'excess_irr': (actual_irr - hurdle_irr) * 100 if actual_irr is not None else None,
            'meets_hurdle': actual_irr >= hurdle_irr if actual_irr is not None else None,
            'hurdle_moic': (1 + hurdle_irr) ** returns['holding_period_years'],
            'actual_moic': returns['moic'],
            'excess_moic': returns['moic'] - ((1 + hurdle_irr) ** returns['holding_period_years'])
        }

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `returns_calculator.py calculate <flat-params-json>` (argv length 3). That call is
# answered here from every panel input (see the module docstring); any other argv shape falls through to the native
# dispatch untouched.
_SERVICE_COMMANDS = ("calculate",)


def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


def _service_result(p: Dict[str, Any]) -> Dict[str, Any]:
    entry = _svc_num(p, "entry_valuation", "entry_ev", default=0.0)
    exit_ev = _svc_num(p, "exit_valuation", "exit_ev", default=0.0)
    equity = _svc_num(p, "equity_invested", default=0.0)
    hold = max(int(_svc_num(p, "holding_period", "hold_years", default=5.0)), 1)
    tranches = [(name, _svc_num(p, amount_key, default=0.0), _svc_num(p, rate_key))
                for name, amount_key, rate_key in (("senior", "senior_debt", "senior_rate"),
                                                   ("subordinated", "sub_debt", "sub_rate"),
                                                   ("other", "debt", "debt_rate"))]
    tranches = [t for t in tranches if t[1] > 0]
    debt = sum(amount for _, amount, _ in tranches)
    if equity <= 0:
        raise ValueError("equity_invested must be positive")
    # Debt is held to exit: exit equity is what the exit enterprise value leaves after repaying it.
    exit_equity = max(exit_ev - debt, 0.0)
    data = ReturnsCalculator().comprehensive_returns(equity, exit_equity, hold)
    # The generic M&A result view rescales sub-1 values of rate-like keys as fractions; these are percents.
    data['irr_pct'] = data.pop('irr')
    data['annualized_return_pct'] = data.pop('annualized_return')
    rated = all(rate is not None for _, _, rate in tranches)
    annual_interest = sum(amount * rate for _, amount, rate in tranches) if rated else None
    gap = equity + debt - entry
    data.update({
        'entry_enterprise_value': entry,
        'exit_enterprise_value': exit_ev,
        'enterprise_value_change': exit_ev - entry,
        'total_debt': debt,
        'entry_debt_to_ev_pct': debt / entry * 100 if entry > 0 else None,
        'implied_entry_equity': entry - debt,
        'sources_uses_gap': gap,
        'annual_interest_expense': annual_interest,
        'total_interest_over_hold': annual_interest * hold if annual_interest is not None else None,
        'blended_cost_of_debt_pct': annual_interest / debt * 100 if (annual_interest is not None and debt > 0) else None,
    })
    warnings = []
    if entry > 0 and abs(gap) > 0.005 * entry:
        warnings.append("Equity {:,.0f} + debt {:,.0f} = {:,.0f} does not fund the entry valuation {:,.0f} ({} of {:,.0f})."
                        .format(equity, debt, equity + debt, entry, "excess" if gap > 0 else "shortfall", abs(gap)))
    if exit_ev < debt:
        warnings.append("Exit valuation is below the debt held to exit: the equity is wiped out.")
    if not rated:
        warnings.append("A debt tranche has no interest rate: interest carry not computed.")
    data['warnings'] = warnings
    data['model_note'] = ("Debt is held to exit with cash-pay interest (no amortisation or cash sweep in this "
                          "calculator), so the rates set the interest carry but not the equity cash flows; the LBO "
                          "Model tab projects cash flow and debt paydown.")
    return data


def _service_dispatch(argv) -> bool:
    import json
    if len(argv) != 3 or argv[1] not in _SERVICE_COMMANDS:
        return False
    try:
        p = json.loads(argv[2])
    except ValueError:
        return False
    if not isinstance(p, dict):
        return False
    try:
        print(json.dumps({"success": True, "data": _service_result(p)}, allow_nan=False))
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "command": argv[1]}))
        sys.exit(1)
    return True
# ---- service ABI shim (MAAnalyticsService) END ----


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    import json

    if _service_dispatch(sys.argv):
        return
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
