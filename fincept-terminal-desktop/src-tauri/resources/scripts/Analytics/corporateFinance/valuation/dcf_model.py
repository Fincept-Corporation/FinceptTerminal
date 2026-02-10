"""Discounted Cash Flow (DCF) Valuation Model"""
from typing import Dict, Any, List, Optional
import sys
import numpy as np

class DCFModel:
    """Comprehensive DCF valuation model"""

    def __init__(self, company_name: str):
        self.company_name = company_name

    def calculate_wacc(self, risk_free_rate: float,
                      market_risk_premium: float,
                      beta: float,
                      cost_of_debt: float,
                      tax_rate: float,
                      market_value_equity: float,
                      market_value_debt: float,
                      country_risk_premium: float = 0.0,
                      size_premium: float = 0.0) -> Dict[str, Any]:
        """Calculate Weighted Average Cost of Capital

        Args:
            risk_free_rate: Risk-free rate (e.g., 10-year Treasury)
            market_risk_premium: Equity risk premium
            beta: Company beta
            cost_of_debt: Pre-tax cost of debt
            tax_rate: Corporate tax rate
            market_value_equity: Market value of equity
            market_value_debt: Market value of debt
            country_risk_premium: Additional premium for country risk (default: 0)
            size_premium: Small cap premium (default: 0)
        """

        # Input validation
        if not (0 <= risk_free_rate <= 0.20):
            raise ValueError(f"Risk-free rate {risk_free_rate:.2%} outside valid range (0-20%)")
        if not (0 <= market_risk_premium <= 0.20):
            raise ValueError(f"Market risk premium {market_risk_premium:.2%} outside valid range (0-20%)")
        if not (0.1 <= beta <= 3.0):
            raise ValueError(f"Beta {beta} outside valid range (0.1-3.0)")
        if not (0 <= cost_of_debt <= 0.30):
            raise ValueError(f"Cost of debt {cost_of_debt:.2%} outside valid range (0-30%)")
        if not (0 <= tax_rate <= 0.50):
            raise ValueError(f"Tax rate {tax_rate:.2%} outside valid range (0-50%)")
        if market_value_equity < 0:
            raise ValueError("Market value of equity cannot be negative")
        if market_value_debt < 0:
            raise ValueError("Market value of debt cannot be negative")
        if not (0 <= country_risk_premium <= 0.15):
            raise ValueError(f"Country risk premium {country_risk_premium:.2%} outside valid range (0-15%)")
        if not (0 <= size_premium <= 0.10):
            raise ValueError(f"Size premium {size_premium:.2%} outside valid range (0-10%)")

        # Cost of equity = Rf + β(MRP) + Country Risk + Size Premium
        cost_of_equity = risk_free_rate + (beta * market_risk_premium) + country_risk_premium + size_premium

        after_tax_cost_of_debt = cost_of_debt * (1 - tax_rate)

        total_value = market_value_equity + market_value_debt
        equity_weight = market_value_equity / total_value if total_value > 0 else 0
        debt_weight = market_value_debt / total_value if total_value > 0 else 0

        wacc = (equity_weight * cost_of_equity) + (debt_weight * after_tax_cost_of_debt)

        return {
            'wacc': wacc,
            'wacc_pct': wacc * 100,
            'cost_of_equity': cost_of_equity * 100,
            'cost_of_equity_pretax': cost_of_equity,
            'cost_of_debt_pretax': cost_of_debt * 100,
            'cost_of_debt_aftertax': after_tax_cost_of_debt * 100,
            'equity_weight': equity_weight * 100,
            'debt_weight': debt_weight * 100,
            'inputs': {
                'risk_free_rate': risk_free_rate * 100,
                'market_risk_premium': market_risk_premium * 100,
                'beta': beta,
                'tax_rate': tax_rate * 100,
                'country_risk_premium': country_risk_premium * 100,
                'size_premium': size_premium * 100
            }
        }

    def unlever_beta(self, levered_beta: float, tax_rate: float,
                     debt_to_equity: float) -> float:
        """Unlever beta to remove debt impact

        Args:
            levered_beta: Current levered beta
            tax_rate: Corporate tax rate
            debt_to_equity: Debt/Equity ratio

        Returns:
            Unlevered beta
        """
        unlevered_beta = levered_beta / (1 + (1 - tax_rate) * debt_to_equity)
        return unlevered_beta

    def relever_beta(self, unlevered_beta: float, tax_rate: float,
                     target_debt_to_equity: float) -> float:
        """Relever beta to target capital structure

        Args:
            unlevered_beta: Unlevered (asset) beta
            tax_rate: Corporate tax rate
            target_debt_to_equity: Target Debt/Equity ratio

        Returns:
            Relevered beta
        """
        levered_beta = unlevered_beta * (1 + (1 - tax_rate) * target_debt_to_equity)
        return levered_beta

    def calculate_free_cash_flow(self, ebit: float,
                                 tax_rate: float,
                                 depreciation: float,
                                 capex: float,
                                 change_in_nwc: float,
                                 stock_based_comp: float = 0.0,
                                 maintenance_capex: Optional[float] = None,
                                 growth_capex: Optional[float] = None) -> Dict[str, Any]:
        """Calculate Free Cash Flow to Firm

        Args:
            ebit: Earnings before interest and tax
            tax_rate: Corporate tax rate
            depreciation: Depreciation & amortization
            capex: Total capital expenditures (if maintenance/growth not separated)
            change_in_nwc: Change in net working capital
            stock_based_comp: Stock-based compensation to add back (default: 0)
            maintenance_capex: CapEx to maintain operations (optional)
            growth_capex: CapEx for growth (optional)
        """

        # Use split CapEx if provided, otherwise use total
        if maintenance_capex is not None and growth_capex is not None:
            total_capex = maintenance_capex + growth_capex
        else:
            total_capex = capex
            # Default split: 60% maintenance, 40% growth if not provided
            maintenance_capex = capex * 0.6 if maintenance_capex is None else maintenance_capex
            growth_capex = capex * 0.4 if growth_capex is None else growth_capex

        # NOPAT = EBIT × (1 - Tax)
        nopat = ebit * (1 - tax_rate)

        # Add back stock-based comp (non-cash expense)
        nopat_adjusted = nopat + stock_based_comp

        # FCF = NOPAT + D&A - CapEx - ΔNWC
        fcf = nopat_adjusted + depreciation - total_capex - change_in_nwc

        return {
            'ebit': ebit,
            'tax': ebit * tax_rate,
            'nopat': nopat,
            'add_stock_based_comp': stock_based_comp,
            'nopat_adjusted': nopat_adjusted,
            'add_depreciation': depreciation,
            'less_capex': total_capex,
            'capex_breakdown': {
                'maintenance': maintenance_capex,
                'growth': growth_capex
            },
            'less_change_in_nwc': change_in_nwc,
            'free_cash_flow': fcf
        }

    def project_cash_flows(self, base_year_fcf: float,
                          growth_rates: List[float]) -> List[Dict[str, Any]]:
        """Project future free cash flows"""

        projections = []
        current_fcf = base_year_fcf

        for year, growth_rate in enumerate(growth_rates, 1):
            current_fcf = current_fcf * (1 + growth_rate)

            projections.append({
                'year': year,
                'growth_rate': growth_rate * 100,
                'fcf': current_fcf
            })

        return projections

    def calculate_terminal_value(self, final_year_fcf: float,
                                terminal_growth_rate: float,
                                wacc: float,
                                method: str = 'perpetuity',
                                exit_multiple: Optional[float] = None,
                                exit_metric: Optional[float] = None) -> Dict[str, Any]:
        """Calculate terminal value using perpetuity growth or exit multiple

        Args:
            final_year_fcf: Final year free cash flow
            terminal_growth_rate: Perpetual growth rate (for perpetuity method)
            wacc: Weighted average cost of capital
            method: 'perpetuity' or 'exit_multiple'
            exit_multiple: EV/EBITDA multiple for exit (optional, for exit_multiple method)
            exit_metric: Final year EBITDA (optional, for exit_multiple method)
        """

        # Input validation
        if not (0 <= terminal_growth_rate <= 0.05):
            raise ValueError(f"Terminal growth rate {terminal_growth_rate:.2%} should be 0-5% (GDP growth)")

        if method == 'perpetuity':
            if wacc <= terminal_growth_rate:
                raise ValueError(f"WACC ({wacc:.2%}) must be greater than terminal growth rate ({terminal_growth_rate:.2%})")

            terminal_value = (final_year_fcf * (1 + terminal_growth_rate)) / (wacc - terminal_growth_rate)

            return {
                'method': 'perpetuity_growth',
                'terminal_value': terminal_value,
                'terminal_growth_rate': terminal_growth_rate * 100,
                'terminal_year_fcf': final_year_fcf,
                'wacc': wacc * 100
            }

        elif method == 'exit_multiple':
            if exit_multiple is None or exit_metric is None:
                raise ValueError("exit_multiple and exit_metric required for exit_multiple method")

            if not (3.0 <= exit_multiple <= 30.0):
                raise ValueError(f"Exit multiple {exit_multiple}x outside typical range (3-30x)")

            terminal_value = exit_multiple * exit_metric

            return {
                'method': 'exit_multiple',
                'terminal_value': terminal_value,
                'exit_multiple': exit_multiple,
                'exit_metric': exit_metric,
                'implied_exit_enterprise_value': terminal_value
            }

        else:
            raise ValueError(f"Unknown method: {method}. Use 'perpetuity' or 'exit_multiple'")

    def calculate_enterprise_value(self, fcf_projections: List[float],
                                  terminal_value: float,
                                  wacc: float,
                                  mid_year_convention: bool = False) -> Dict[str, Any]:
        """Calculate enterprise value from DCF

        Args:
            fcf_projections: List of projected free cash flows
            terminal_value: Terminal value
            wacc: Weighted average cost of capital
            mid_year_convention: If True, assumes cash flows occur mid-year (default: False)
        """

        pv_fcf_list = []
        pv_fcf_total = 0

        # Mid-year adjustment: discount by year - 0.5 instead of year
        for year, fcf in enumerate(fcf_projections, 1):
            if mid_year_convention:
                discount_factor = (1 + wacc) ** (year - 0.5)
            else:
                discount_factor = (1 + wacc) ** year

            pv = fcf / discount_factor
            pv_fcf_total += pv

            pv_fcf_list.append({
                'year': year,
                'fcf': fcf,
                'discount_factor': discount_factor,
                'present_value': pv
            })

        terminal_year = len(fcf_projections)

        if mid_year_convention:
            terminal_discount_factor = (1 + wacc) ** (terminal_year - 0.5)
        else:
            terminal_discount_factor = (1 + wacc) ** terminal_year

        pv_terminal_value = terminal_value / terminal_discount_factor

        enterprise_value = pv_fcf_total + pv_terminal_value

        return {
            'pv_of_fcf': pv_fcf_total,
            'pv_of_terminal_value': pv_terminal_value,
            'enterprise_value': enterprise_value,
            'terminal_value_contribution': (pv_terminal_value / enterprise_value * 100) if enterprise_value > 0 else 0,
            'fcf_details': pv_fcf_list,
            'wacc_used': wacc * 100,
            'mid_year_convention': mid_year_convention
        }

    def calculate_equity_value(self, enterprise_value: float,
                              cash: float,
                              debt: float,
                              minority_interest: float = 0,
                              preferred_stock: float = 0,
                              excess_cash: Optional[float] = None) -> Dict[str, Any]:
        """Convert enterprise value to equity value

        Args:
            enterprise_value: Enterprise value from DCF
            cash: Total cash and equivalents
            debt: Total debt
            minority_interest: Minority interest to subtract
            preferred_stock: Preferred stock to subtract
            excess_cash: Cash above operating needs (optional, if not specified uses total cash)
        """

        # If excess cash not specified, use total cash
        # Operating cash typically = 2% of revenue (rule of thumb)
        cash_to_add = excess_cash if excess_cash is not None else cash

        equity_value = enterprise_value + cash_to_add - debt - minority_interest - preferred_stock

        return {
            'enterprise_value': enterprise_value,
            'add_cash': cash_to_add,
            'total_cash': cash,
            'excess_cash_used': excess_cash is not None,
            'less_debt': debt,
            'less_minority_interest': minority_interest,
            'less_preferred_stock': preferred_stock,
            'equity_value': equity_value
        }

    def calculate_price_per_share(self, equity_value: float,
                                 shares_outstanding: float,
                                 diluted_shares: Optional[float] = None) -> Dict[str, Any]:
        """Calculate price per share"""

        shares = diluted_shares if diluted_shares else shares_outstanding

        price_per_share = equity_value / shares if shares > 0 else 0

        return {
            'equity_value': equity_value,
            'shares_outstanding_basic': shares_outstanding,
            'shares_outstanding_diluted': diluted_shares,
            'shares_used': shares,
            'price_per_share': price_per_share
        }

    def comprehensive_dcf(self, wacc_inputs: Dict[str, float],
                         fcf_inputs: Dict[str, float],
                         growth_rates: List[float],
                         terminal_growth_rate: float,
                         balance_sheet: Dict[str, float],
                         shares_outstanding: float) -> Dict[str, Any]:
        """Complete DCF valuation from inputs to price per share"""

        wacc_result = self.calculate_wacc(**wacc_inputs)
        wacc = wacc_result['wacc']

        base_fcf_calc = self.calculate_free_cash_flow(**fcf_inputs)
        base_fcf = base_fcf_calc['free_cash_flow']

        fcf_projections = self.project_cash_flows(base_fcf, growth_rates)
        fcf_values = [p['fcf'] for p in fcf_projections]

        terminal_value_result = self.calculate_terminal_value(
            fcf_values[-1],
            terminal_growth_rate,
            wacc
        )

        ev_result = self.calculate_enterprise_value(
            fcf_values,
            terminal_value_result['terminal_value'],
            wacc
        )

        equity_result = self.calculate_equity_value(
            ev_result['enterprise_value'],
            balance_sheet['cash'],
            balance_sheet['debt'],
            balance_sheet.get('minority_interest', 0),
            balance_sheet.get('preferred_stock', 0)
        )

        price_result = self.calculate_price_per_share(
            equity_result['equity_value'],
            shares_outstanding,
            balance_sheet.get('diluted_shares')
        )

        return {
            'company_name': self.company_name,
            'wacc': wacc_result,
            'base_year_fcf': base_fcf_calc,
            'fcf_projections': fcf_projections,
            'terminal_value': terminal_value_result,
            'enterprise_value': ev_result,
            'equity_value': equity_result,
            'valuation_per_share': price_result,
            'summary': {
                'enterprise_value': ev_result['enterprise_value'],
                'equity_value': equity_result['equity_value'],
                'price_per_share': price_result['price_per_share'],
                'wacc': wacc * 100,
                'terminal_growth': terminal_growth_rate * 100
            }
        }

    def sensitivity_analysis(self, base_fcf: float,
                           growth_rates: List[float],
                           terminal_growth_scenarios: List[float],
                           wacc_scenarios: List[float],
                           balance_sheet: Dict[str, float],
                           shares_outstanding: float) -> Dict[str, Any]:
        """Two-way sensitivity analysis (WACC vs Terminal Growth)"""

        sensitivity_matrix = []

        for terminal_growth in terminal_growth_scenarios:
            for wacc in wacc_scenarios:
                if wacc <= terminal_growth:
                    continue

                fcf_projections_calc = self.project_cash_flows(base_fcf, growth_rates)
                fcf_values = [p['fcf'] for p in fcf_projections_calc]

                terminal_value_calc = self.calculate_terminal_value(
                    fcf_values[-1],
                    terminal_growth,
                    wacc
                )

                ev_calc = self.calculate_enterprise_value(
                    fcf_values,
                    terminal_value_calc['terminal_value'],
                    wacc
                )

                equity_calc = self.calculate_equity_value(
                    ev_calc['enterprise_value'],
                    balance_sheet['cash'],
                    balance_sheet['debt'],
                    balance_sheet.get('minority_interest', 0),
                    balance_sheet.get('preferred_stock', 0)
                )

                price_calc = self.calculate_price_per_share(
                    equity_calc['equity_value'],
                    shares_outstanding
                )

                sensitivity_matrix.append({
                    'wacc': wacc * 100,
                    'terminal_growth': terminal_growth * 100,
                    'price_per_share': price_calc['price_per_share'],
                    'equity_value': equity_calc['equity_value']
                })

        return {
            'sensitivity_matrix': sensitivity_matrix,
            'wacc_range': [min(wacc_scenarios) * 100, max(wacc_scenarios) * 100],
            'terminal_growth_range': [min(terminal_growth_scenarios) * 100, max(terminal_growth_scenarios) * 100]
        }

    def scenario_analysis(self, base_inputs: Dict[str, Any],
                         bear_adjustments: Optional[Dict[str, Any]] = None,
                         bull_adjustments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Run bear/base/bull scenario analysis

        Args:
            base_inputs: Base case DCF inputs (wacc_inputs, fcf_inputs, etc.)
            bear_adjustments: Bear case adjustments (e.g., {'growth_rates': [0.02, 0.02, ...]})
            bull_adjustments: Bull case adjustments
        """

        # Default adjustments if not provided
        if bear_adjustments is None:
            bear_adjustments = {
                'growth_multiplier': 0.5,  # 50% of base growth
                'wacc_adjustment': 0.02,   # +2% to WACC
                'terminal_growth_adjustment': -0.01  # -1% terminal growth
            }

        if bull_adjustments is None:
            bull_adjustments = {
                'growth_multiplier': 1.5,   # 150% of base growth
                'wacc_adjustment': -0.01,   # -1% to WACC
                'terminal_growth_adjustment': 0.005  # +0.5% terminal growth
            }

        scenarios = {}

        # Base case
        base_result = self.comprehensive_dcf(**base_inputs)
        scenarios['base'] = {
            'price_per_share': base_result['summary']['price_per_share'],
            'equity_value': base_result['summary']['equity_value'],
            'enterprise_value': base_result['summary']['enterprise_value'],
            'full_results': base_result
        }

        # Bear case
        bear_inputs = self._adjust_inputs(base_inputs, bear_adjustments, scenario_type='bear')
        bear_result = self.comprehensive_dcf(**bear_inputs)
        scenarios['bear'] = {
            'price_per_share': bear_result['summary']['price_per_share'],
            'equity_value': bear_result['summary']['equity_value'],
            'enterprise_value': bear_result['summary']['enterprise_value'],
            'full_results': bear_result
        }

        # Bull case
        bull_inputs = self._adjust_inputs(base_inputs, bull_adjustments, scenario_type='bull')
        bull_result = self.comprehensive_dcf(**bull_inputs)
        scenarios['bull'] = {
            'price_per_share': bull_result['summary']['price_per_share'],
            'equity_value': bull_result['summary']['equity_value'],
            'enterprise_value': bull_result['summary']['enterprise_value'],
            'full_results': bull_result
        }

        return {
            'scenarios': scenarios,
            'price_range': {
                'low': scenarios['bear']['price_per_share'],
                'base': scenarios['base']['price_per_share'],
                'high': scenarios['bull']['price_per_share']
            },
            'valuation_range': {
                'low': scenarios['bear']['equity_value'],
                'base': scenarios['base']['equity_value'],
                'high': scenarios['bull']['equity_value']
            }
        }

    def _adjust_inputs(self, base_inputs: Dict[str, Any],
                      adjustments: Dict[str, Any],
                      scenario_type: str) -> Dict[str, Any]:
        """Helper to adjust inputs for scenario analysis"""
        import copy
        inputs = copy.deepcopy(base_inputs)

        # Adjust growth rates
        if 'growth_rates' in adjustments:
            inputs['growth_rates'] = adjustments['growth_rates']
        elif 'growth_multiplier' in adjustments:
            multiplier = adjustments['growth_multiplier']
            inputs['growth_rates'] = [max(0.0, min(g * multiplier, 0.30)) for g in inputs['growth_rates']]

        # Adjust WACC
        if 'wacc_adjustment' in adjustments:
            # Adjust beta to change WACC
            current_beta = inputs['wacc_inputs']['beta']
            # Approximate: +1% WACC ≈ +0.15 beta (assuming 7% MRP)
            mrp = inputs['wacc_inputs']['market_risk_premium']
            beta_change = adjustments['wacc_adjustment'] / mrp if mrp > 0 else 0
            inputs['wacc_inputs']['beta'] = max(0.1, min(current_beta + beta_change, 3.0))

        # Adjust terminal growth
        if 'terminal_growth_adjustment' in adjustments:
            current_tg = inputs['terminal_growth_rate']
            inputs['terminal_growth_rate'] = max(0.0, min(current_tg + adjustments['terminal_growth_adjustment'], 0.05))

        return inputs

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: dcf_model.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command in ("calculate", "dcf"):
            # Rust sends: "dcf" wacc_inputs fcf_inputs growth_rates terminal_growth balance_sheet shares_outstanding
            if len(sys.argv) < 8:
                raise ValueError("All DCF inputs required: wacc_inputs, fcf_inputs, growth_rates, terminal_growth, balance_sheet, shares")

            wacc_inputs = json.loads(sys.argv[2])
            fcf_inputs = json.loads(sys.argv[3])
            growth_rates = json.loads(sys.argv[4])
            terminal_growth_rate = float(sys.argv[5])
            balance_sheet = json.loads(sys.argv[6])
            shares_outstanding = float(sys.argv[7])

            dcf = DCFModel("Target Company")
            dcf_result = dcf.comprehensive_dcf(
                wacc_inputs=wacc_inputs,
                fcf_inputs=fcf_inputs,
                growth_rates=growth_rates,
                terminal_growth_rate=terminal_growth_rate,
                balance_sheet=balance_sheet,
                shares_outstanding=shares_outstanding
            )

            result = {
                "success": True,
                "data": dcf_result
            }
            print(json.dumps(result))

        elif command == "sensitivity":
            # Rust sends: "sensitivity" base_fcf growth_rates terminal_growth_scenarios wacc_scenarios balance_sheet shares_outstanding
            if len(sys.argv) < 8:
                raise ValueError("Sensitivity inputs required: base_fcf, growth_rates, terminal_growth_scenarios, wacc_scenarios, balance_sheet, shares")

            base_fcf = float(sys.argv[2])
            growth_rates = json.loads(sys.argv[3])
            terminal_growth_scenarios = json.loads(sys.argv[4])
            wacc_scenarios = json.loads(sys.argv[5])
            balance_sheet = json.loads(sys.argv[6])
            shares_outstanding = float(sys.argv[7])

            dcf = DCFModel("Target Company")
            sensitivity_result = dcf.sensitivity_analysis(
                base_fcf=base_fcf,
                growth_rates=growth_rates,
                terminal_growth_scenarios=terminal_growth_scenarios,
                wacc_scenarios=wacc_scenarios,
                balance_sheet=balance_sheet,
                shares_outstanding=shares_outstanding
            )

            result = {
                "success": True,
                "data": sensitivity_result
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: dcf, sensitivity"
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
