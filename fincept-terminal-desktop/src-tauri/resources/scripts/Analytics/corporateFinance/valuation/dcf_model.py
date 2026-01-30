"""Discounted Cash Flow (DCF) Valuation Model"""
from typing import Dict, Any, List, Optional
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
                      market_value_debt: float) -> Dict[str, Any]:
        """Calculate Weighted Average Cost of Capital"""

        cost_of_equity = risk_free_rate + (beta * market_risk_premium)

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
                'tax_rate': tax_rate * 100
            }
        }

    def calculate_free_cash_flow(self, ebit: float,
                                 tax_rate: float,
                                 depreciation: float,
                                 capex: float,
                                 change_in_nwc: float) -> Dict[str, Any]:
        """Calculate Free Cash Flow to Firm"""

        nopat = ebit * (1 - tax_rate)

        fcf = nopat + depreciation - capex - change_in_nwc

        return {
            'ebit': ebit,
            'tax': ebit * tax_rate,
            'nopat': nopat,
            'add_depreciation': depreciation,
            'less_capex': capex,
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
                                method: str = 'perpetuity') -> Dict[str, Any]:
        """Calculate terminal value using perpetuity growth or exit multiple"""

        if method == 'perpetuity':
            if wacc <= terminal_growth_rate:
                raise ValueError("WACC must be greater than terminal growth rate")

            terminal_value = (final_year_fcf * (1 + terminal_growth_rate)) / (wacc - terminal_growth_rate)

            return {
                'method': 'perpetuity_growth',
                'terminal_value': terminal_value,
                'terminal_growth_rate': terminal_growth_rate * 100,
                'terminal_year_fcf': final_year_fcf,
                'wacc': wacc * 100
            }

        else:
            raise ValueError("Only perpetuity method supported currently")

    def calculate_enterprise_value(self, fcf_projections: List[float],
                                  terminal_value: float,
                                  wacc: float) -> Dict[str, Any]:
        """Calculate enterprise value from DCF"""

        pv_fcf_list = []
        pv_fcf_total = 0

        for year, fcf in enumerate(fcf_projections, 1):
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
        terminal_discount_factor = (1 + wacc) ** terminal_year
        pv_terminal_value = terminal_value / terminal_discount_factor

        enterprise_value = pv_fcf_total + pv_terminal_value

        return {
            'pv_of_fcf': pv_fcf_total,
            'pv_of_terminal_value': pv_terminal_value,
            'enterprise_value': enterprise_value,
            'terminal_value_contribution': (pv_terminal_value / enterprise_value * 100) if enterprise_value > 0 else 0,
            'fcf_details': pv_fcf_list,
            'wacc_used': wacc * 100
        }

    def calculate_equity_value(self, enterprise_value: float,
                              cash: float,
                              debt: float,
                              minority_interest: float = 0,
                              preferred_stock: float = 0) -> Dict[str, Any]:
        """Convert enterprise value to equity value"""

        equity_value = enterprise_value + cash - debt - minority_interest - preferred_stock

        return {
            'enterprise_value': enterprise_value,
            'add_cash': cash,
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

if __name__ == '__main__':
    dcf = DCFModel("Target Corp")

    wacc_inputs = {
        'risk_free_rate': 0.04,
        'market_risk_premium': 0.06,
        'beta': 1.2,
        'cost_of_debt': 0.05,
        'tax_rate': 0.25,
        'market_value_equity': 5_000_000_000,
        'market_value_debt': 1_000_000_000
    }

    fcf_inputs = {
        'ebit': 500_000_000,
        'tax_rate': 0.25,
        'depreciation': 100_000_000,
        'capex': 120_000_000,
        'change_in_nwc': 30_000_000
    }

    growth_rates = [0.10, 0.10, 0.08, 0.08, 0.06]

    balance_sheet = {
        'cash': 200_000_000,
        'debt': 1_000_000_000,
        'minority_interest': 0,
        'preferred_stock': 0
    }

    result = dcf.comprehensive_dcf(
        wacc_inputs=wacc_inputs,
        fcf_inputs=fcf_inputs,
        growth_rates=growth_rates,
        terminal_growth_rate=0.02,
        balance_sheet=balance_sheet,
        shares_outstanding=100_000_000
    )

    print("=== DCF VALUATION ===\n")
    print(f"Company: {result['company_name']}")
    print(f"\nWACC: {result['wacc']['wacc_pct']:.2f}%")
    print(f"  Cost of Equity: {result['wacc']['cost_of_equity']:.2f}%")
    print(f"  After-Tax Cost of Debt: {result['wacc']['cost_of_debt_aftertax']:.2f}%")

    print(f"\nBase Year FCF: ${result['base_year_fcf']['free_cash_flow']:,.0f}")

    print("\nFCF Projections:")
    for proj in result['fcf_projections']:
        print(f"  Year {proj['year']}: ${proj['fcf']:,.0f} (Growth: {proj['growth_rate']:.1f}%)")

    print(f"\nTerminal Value: ${result['terminal_value']['terminal_value']:,.0f}")
    print(f"Terminal Growth Rate: {result['terminal_value']['terminal_growth_rate']:.1f}%")

    print(f"\nEnterprise Value: ${result['enterprise_value']['enterprise_value']:,.0f}")
    print(f"  PV of FCF: ${result['enterprise_value']['pv_of_fcf']:,.0f}")
    print(f"  PV of Terminal Value: ${result['enterprise_value']['pv_of_terminal_value']:,.0f}")
    print(f"  Terminal Value %: {result['enterprise_value']['terminal_value_contribution']:.1f}%")

    print(f"\nEquity Value: ${result['equity_value']['equity_value']:,.0f}")
    print(f"\nPrice Per Share: ${result['valuation_per_share']['price_per_share']:.2f}")
