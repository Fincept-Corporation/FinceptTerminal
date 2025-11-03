"""
Equity Investment Dcf Models Module
======================================

Discounted cash flow valuation models

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Market price data and trading volume information
  - Industry reports and competitive analysis data
  - Management guidance and analyst estimates
  - Economic indicators affecting equity markets

OUTPUT:
  - Equity valuation models and fair value estimates
  - Fundamental analysis metrics and financial ratios
  - Investment recommendations and target prices
  - Risk assessments and portfolio implications
  - Sector and industry comparative analysis

PARAMETERS:
  - valuation_method: Primary valuation methodology (default: 'DCF')
  - discount_rate: Discount rate for valuation (default: 0.10)
  - terminal_growth: Terminal growth rate assumption (default: 0.025)
  - earnings_multiple: Target earnings multiple (default: 15.0)
  - reporting_currency: Reporting currency (default: 'USD')
"""

import numpy as np
import pandas as pd
from typing import List, Dict, Any, Optional, Tuple, Union
from dataclasses import dataclass
import math

from ..base.base_models import (
    BaseValuationModel, CompanyData, MarketData, ValuationResult,
    ValuationMethod, CalculationEngine, ModelValidator, ValidationError
)


@dataclass
class DCFParameters:
    """Parameters for DCF calculations"""
    cash_flows: List[float]
    discount_rate: float
    terminal_growth_rate: Optional[float] = None
    terminal_value: Optional[float] = None
    projection_years: int = 5


class FCFFModel(BaseValuationModel):
    """Free Cash Flow to Firm Model"""

    def __init__(self):
        super().__init__("FCFF Model", "Free Cash Flow to Firm valuation")
        self.valuation_method = ValuationMethod.DCF_FCFF

    def calculate_intrinsic_value(self, company_data: CompanyData, market_data: MarketData) -> float:
        """Calculate intrinsic value using company and market data"""
        # This is a simplified implementation
        return 0.0

    def validate_inputs(self, wacc: float, fcff_projections: List[float],
                        terminal_growth: float = None) -> bool:
        """Validate FCFF model inputs"""
        ModelValidator.validate_percentage(wacc, "WACC")

        if not fcff_projections or len(fcff_projections) == 0:
            raise ValidationError("FCFF projections cannot be empty")

        if terminal_growth is not None:
            ModelValidator.validate_percentage(terminal_growth, "Terminal growth rate", allow_negative=True)
            ModelValidator.validate_growth_vs_required_return(terminal_growth, wacc)

        return True

    def calculate_fcff_from_components(self, ebit: float, tax_rate: float, depreciation: float,
                                       capex: float, working_capital_change: float) -> float:
        """Calculate FCFF from financial statement components"""
        return CalculationEngine.free_cash_flow_to_firm(ebit, tax_rate, depreciation,
                                                        capex, working_capital_change)

    def calculate_fcff_from_ebitda(self, ebitda: float, tax_rate: float, depreciation: float,
                                   capex: float, working_capital_change: float) -> float:
        """Calculate FCFF starting from EBITDA"""
        ebit = ebitda - depreciation
        return self.calculate_fcff_from_components(ebit, tax_rate, depreciation,
                                                   capex, working_capital_change)

    def calculate_fcff_from_net_income(self, net_income: float, interest_expense: float,
                                       tax_rate: float, depreciation: float, capex: float,
                                       working_capital_change: float) -> float:
        """Calculate FCFF starting from net income"""
        # Add back after-tax interest expense
        after_tax_interest = interest_expense * (1 - tax_rate)
        unlevered_net_income = net_income + after_tax_interest

        return unlevered_net_income + depreciation - capex - working_capital_change

    def calculate_fcff_from_cfo(self, cfo: float, interest_expense: float, tax_rate: float,
                                capex: float) -> float:
        """Calculate FCFF from Cash Flow from Operations"""
        after_tax_interest = interest_expense * (1 - tax_rate)
        return cfo + after_tax_interest - capex

    def calculate_terminal_value(self, final_fcff: float, terminal_growth: float,
                                 wacc: float) -> float:
        """Calculate terminal value using Gordon Growth"""
        if terminal_growth >= wacc:
            raise ValidationError("Terminal growth rate must be less than WACC")

        terminal_fcff = final_fcff * (1 + terminal_growth)
        return terminal_fcff / (wacc - terminal_growth)

    def calculate_enterprise_value(self, fcff_projections: List[float], wacc: float,
                                   terminal_growth: float = None, terminal_value: float = None) -> Dict[str, float]:
        """Calculate enterprise value from FCFF projections"""

        # Present value of projected cash flows
        pv_fcff = 0
        pv_details = []

        for year, fcff in enumerate(fcff_projections, 1):
            pv = CalculationEngine.present_value(fcff, wacc, year)
            pv_fcff += pv
            pv_details.append({'year': year, 'fcff': fcff, 'pv': pv})

        # Terminal value
        if terminal_value is None:
            if terminal_growth is None:
                raise ValidationError("Either terminal_growth or terminal_value must be provided")
            terminal_value = self.calculate_terminal_value(fcff_projections[-1], terminal_growth, wacc)

        # Present value of terminal value
        pv_terminal = CalculationEngine.present_value(terminal_value, wacc, len(fcff_projections))

        # Total enterprise value
        enterprise_value = pv_fcff + pv_terminal

        return {
            'pv_fcff': pv_fcff,
            'terminal_value': terminal_value,
            'pv_terminal': pv_terminal,
            'enterprise_value': enterprise_value,
            'pv_details': pv_details
        }

    def calculate_equity_value(self, enterprise_value: float, cash: float,
                               total_debt: float, preferred_stock: float = 0) -> float:
        """Calculate equity value from enterprise value"""
        return enterprise_value + cash - total_debt - preferred_stock

    def calculate(self, fcff_projections: List[float], wacc: float, shares_outstanding: float,
                  terminal_growth: float = None, terminal_value: float = None,
                  cash: float = 0, total_debt: float = 0, preferred_stock: float = 0,
                  current_price: float = None) -> ValuationResult:
        """Calculate valuation using FCFF model"""

        # Validate inputs
        self.validate_inputs(wacc, fcff_projections, terminal_growth)

        # Calculate enterprise value
        ev_components = self.calculate_enterprise_value(fcff_projections, wacc,
                                                        terminal_growth, terminal_value)

        # Calculate equity value
        equity_value = self.calculate_equity_value(ev_components['enterprise_value'],
                                                   cash, total_debt, preferred_stock)

        # Calculate per-share value
        intrinsic_value = equity_value / shares_outstanding if shares_outstanding > 0 else 0

        # Store assumptions
        assumptions = {
            'wacc': wacc,
            'terminal_growth_rate': terminal_growth,
            'projection_years': len(fcff_projections),
            'terminal_value_multiple': ev_components['pv_terminal'] / ev_components['enterprise_value'] * 100,
            'cash': cash,
            'total_debt': total_debt,
            'preferred_stock': preferred_stock,
            'shares_outstanding': shares_outstanding,
            'model_type': 'FCFF DCF Model'
        }

        # Detailed calculations
        calculation_details = {
            'fcff_projections': fcff_projections,
            'pv_fcff': ev_components['pv_fcff'],
            'terminal_value': ev_components['terminal_value'],
            'pv_terminal': ev_components['pv_terminal'],
            'enterprise_value': ev_components['enterprise_value'],
            'equity_value': equity_value,
            'intrinsic_value_per_share': intrinsic_value,
            'pv_details': ev_components['pv_details']
        }

        # Generate recommendation
        recommendation = "HOLD"
        upside_downside = 0
        if current_price:
            recommendation = self.generate_recommendation(intrinsic_value, current_price)
            upside_downside = self.calculate_upside_downside(intrinsic_value, current_price)

        return ValuationResult(
            method=self.valuation_method,
            intrinsic_value=intrinsic_value,
            current_price=current_price or 0,
            recommendation=recommendation,
            upside_downside=upside_downside,
            confidence_level="MEDIUM",
            assumptions=assumptions,
            calculation_details=calculation_details
        )


class FCFEModel(BaseValuationModel):
    """Free Cash Flow to Equity Model"""

    def __init__(self):
        super().__init__("FCFE Model", "Free Cash Flow to Equity valuation")
        self.valuation_method = ValuationMethod.DCF_FCFE

    def calculate_intrinsic_value(self, company_data: CompanyData, market_data: MarketData) -> float:
        """Calculate intrinsic value using company and market data"""
        # This is a simplified implementation
        return 0.0

    def validate_inputs(self, required_return: float, fcfe_projections: List[float],
                        terminal_growth: float = None) -> bool:
        """Validate FCFE model inputs"""
        ModelValidator.validate_percentage(required_return, "Required return on equity")

        if not fcfe_projections or len(fcfe_projections) == 0:
            raise ValidationError("FCFE projections cannot be empty")

        if terminal_growth is not None:
            ModelValidator.validate_percentage(terminal_growth, "Terminal growth rate", allow_negative=True)
            ModelValidator.validate_growth_vs_required_return(terminal_growth, required_return)

        return True

    def calculate_fcfe_from_components(self, net_income: float, depreciation: float, capex: float,
                                       working_capital_change: float, net_borrowing: float) -> float:
        """Calculate FCFE from financial statement components"""
        return CalculationEngine.free_cash_flow_to_equity(net_income, depreciation, capex,
                                                          working_capital_change, net_borrowing)

    def calculate_fcfe_from_fcff(self, fcff: float, interest_expense: float, tax_rate: float,
                                 net_borrowing: float) -> float:
        """Calculate FCFE from FCFF"""
        after_tax_interest = interest_expense * (1 - tax_rate)
        return fcff - after_tax_interest + net_borrowing

    def calculate_fcfe_from_ebit(self, ebit: float, tax_rate: float, depreciation: float,
                                 capex: float, working_capital_change: float,
                                 interest_expense: float, net_borrowing: float) -> float:
        """Calculate FCFE starting from EBIT"""
        # Calculate net income
        ebt = ebit - interest_expense
        net_income = ebt * (1 - tax_rate)

        return self.calculate_fcfe_from_components(net_income, depreciation, capex,
                                                   working_capital_change, net_borrowing)

    def calculate_fcfe_from_ebitda(self, ebitda: float, tax_rate: float, depreciation: float,
                                   capex: float, working_capital_change: float,
                                   interest_expense: float, net_borrowing: float) -> float:
        """Calculate FCFE starting from EBITDA"""
        ebit = ebitda - depreciation
        return self.calculate_fcfe_from_ebit(ebit, tax_rate, depreciation, capex,
                                             working_capital_change, interest_expense, net_borrowing)

    def calculate_fcfe_from_cfo(self, cfo: float, capex: float, net_borrowing: float) -> float:
        """Calculate FCFE from Cash Flow from Operations"""
        return cfo - capex + net_borrowing

    def calculate_terminal_value(self, final_fcfe: float, terminal_growth: float,
                                 required_return: float) -> float:
        """Calculate terminal value using Gordon Growth"""
        if terminal_growth >= required_return:
            raise ValidationError("Terminal growth rate must be less than required return")

        terminal_fcfe = final_fcfe * (1 + terminal_growth)
        return terminal_fcfe / (required_return - terminal_growth)

    def calculate_equity_value(self, fcfe_projections: List[float], required_return: float,
                               terminal_growth: float = None, terminal_value: float = None) -> Dict[str, float]:
        """Calculate equity value from FCFE projections"""

        # Present value of projected cash flows
        pv_fcfe = 0
        pv_details = []

        for year, fcfe in enumerate(fcfe_projections, 1):
            pv = CalculationEngine.present_value(fcfe, required_return, year)
            pv_fcfe += pv
            pv_details.append({'year': year, 'fcfe': fcfe, 'pv': pv})

        # Terminal value
        if terminal_value is None:
            if terminal_growth is None:
                raise ValidationError("Either terminal_growth or terminal_value must be provided")
            terminal_value = self.calculate_terminal_value(fcfe_projections[-1], terminal_growth, required_return)

        # Present value of terminal value
        pv_terminal = CalculationEngine.present_value(terminal_value, required_return, len(fcfe_projections))

        # Total equity value
        equity_value = pv_fcfe + pv_terminal

        return {
            'pv_fcfe': pv_fcfe,
            'terminal_value': terminal_value,
            'pv_terminal': pv_terminal,
            'equity_value': equity_value,
            'pv_details': pv_details
        }

    def calculate(self, fcfe_projections: List[float], required_return: float, shares_outstanding: float,
                  terminal_growth: float = None, terminal_value: float = None,
                  current_price: float = None) -> ValuationResult:
        """Calculate valuation using FCFE model"""

        # Validate inputs
        self.validate_inputs(required_return, fcfe_projections, terminal_growth)

        # Calculate equity value
        equity_components = self.calculate_equity_value(fcfe_projections, required_return,
                                                        terminal_growth, terminal_value)

        # Calculate per-share value
        intrinsic_value = equity_components['equity_value'] / shares_outstanding if shares_outstanding > 0 else 0

        # Store assumptions
        assumptions = {
            'required_return': required_return,
            'terminal_growth_rate': terminal_growth,
            'projection_years': len(fcfe_projections),
            'terminal_value_multiple': equity_components['pv_terminal'] / equity_components['equity_value'] * 100,
            'shares_outstanding': shares_outstanding,
            'model_type': 'FCFE DCF Model'
        }

        # Detailed calculations
        calculation_details = {
            'fcfe_projections': fcfe_projections,
            'pv_fcfe': equity_components['pv_fcfe'],
            'terminal_value': equity_components['terminal_value'],
            'pv_terminal': equity_components['pv_terminal'],
            'equity_value': equity_components['equity_value'],
            'intrinsic_value_per_share': intrinsic_value,
            'pv_details': equity_components['pv_details']
        }

        # Generate recommendation
        recommendation = "HOLD"
        upside_downside = 0
        if current_price:
            recommendation = self.generate_recommendation(intrinsic_value, current_price)
            upside_downside = self.calculate_upside_downside(intrinsic_value, current_price)

        return ValuationResult(
            method=self.valuation_method,
            intrinsic_value=intrinsic_value,
            current_price=current_price or 0,
            recommendation=recommendation,
            upside_downside=upside_downside,
            confidence_level="MEDIUM",
            assumptions=assumptions,
            calculation_details=calculation_details
        )


class DCFSensitivityAnalyzer:
    """Sensitivity analysis for DCF models"""

    @staticmethod
    def wacc_sensitivity_analysis(base_fcff_projections: List[float], base_wacc: float,
                                  terminal_growth: float, shares_outstanding: float,
                                  wacc_range: Tuple[float, float] = (-0.02, 0.02),
                                  steps: int = 5) -> pd.DataFrame:
        """Perform sensitivity analysis on WACC"""

        fcff_model = FCFFModel()
        results = []

        wacc_min, wacc_max = wacc_range
        wacc_values = np.linspace(base_wacc + wacc_min, base_wacc + wacc_max, steps)

        for wacc in wacc_values:
            try:
                ev_components = fcff_model.calculate_enterprise_value(base_fcff_projections, wacc, terminal_growth)
                equity_value = fcff_model.calculate_equity_value(ev_components['enterprise_value'], 0, 0, 0)
                per_share_value = equity_value / shares_outstanding

                results.append({
                    'wacc': wacc,
                    'enterprise_value': ev_components['enterprise_value'],
                    'equity_value': equity_value,
                    'per_share_value': per_share_value
                })
            except:
                continue

        return pd.DataFrame(results)

    @staticmethod
    def terminal_growth_sensitivity_analysis(base_fcff_projections: List[float], wacc: float,
                                             base_terminal_growth: float, shares_outstanding: float,
                                             growth_range: Tuple[float, float] = (-0.01, 0.01),
                                             steps: int = 5) -> pd.DataFrame:
        """Perform sensitivity analysis on terminal growth rate"""

        fcff_model = FCFFModel()
        results = []

        growth_min, growth_max = growth_range
        growth_values = np.linspace(base_terminal_growth + growth_min,
                                    base_terminal_growth + growth_max, steps)

        for growth in growth_values:
            if growth >= wacc:
                continue

            try:
                ev_components = fcff_model.calculate_enterprise_value(base_fcff_projections, wacc, growth)
                equity_value = fcff_model.calculate_equity_value(ev_components['enterprise_value'], 0, 0, 0)
                per_share_value = equity_value / shares_outstanding

                results.append({
                    'terminal_growth': growth,
                    'enterprise_value': ev_components['enterprise_value'],
                    'equity_value': equity_value,
                    'per_share_value': per_share_value
                })
            except:
                continue

        return pd.DataFrame(results)

    @staticmethod
    def two_way_sensitivity_analysis(base_fcff_projections: List[float], base_wacc: float,
                                     base_terminal_growth: float, shares_outstanding: float,
                                     wacc_range: Tuple[float, float] = (-0.015, 0.015),
                                     growth_range: Tuple[float, float] = (-0.01, 0.01),
                                     steps: int = 5) -> pd.DataFrame:
        """Perform two-way sensitivity analysis on WACC and terminal growth"""

        fcff_model = FCFFModel()
        results = []

        wacc_min, wacc_max = wacc_range
        growth_min, growth_max = growth_range

        wacc_values = np.linspace(base_wacc + wacc_min, base_wacc + wacc_max, steps)
        growth_values = np.linspace(base_terminal_growth + growth_min,
                                    base_terminal_growth + growth_max, steps)

        for wacc in wacc_values:
            for growth in growth_values:
                if growth >= wacc:
                    continue

                try:
                    ev_components = fcff_model.calculate_enterprise_value(base_fcff_projections, wacc, growth)
                    equity_value = fcff_model.calculate_equity_value(ev_components['enterprise_value'], 0, 0, 0)
                    per_share_value = equity_value / shares_outstanding

                    results.append({
                        'wacc': wacc,
                        'terminal_growth': growth,
                        'per_share_value': per_share_value
                    })
                except:
                    continue

        df = pd.DataFrame(results)
        return df.pivot_table(values='per_share_value', index='wacc', columns='terminal_growth')


class DCFAnalyzer:
    """Comprehensive DCF analysis tool"""

    def __init__(self):
        self.fcff_model = FCFFModel()
        self.fcfe_model = FCFEModel()
        self.sensitivity_analyzer = DCFSensitivityAnalyzer()

    def compare_dcf_models(self, company_data: CompanyData, market_data: MarketData,
                           projections: Dict[str, List[float]]) -> Dict[str, ValuationResult]:
        """Compare FCFF and FCFE valuations"""

        results = {}

        # FCFF Model
        if 'fcff' in projections:
            try:
                # Estimate WACC (simplified)
                wacc = market_data.required_return * 0.8  # Rough approximation

                results['fcff'] = self.fcff_model.calculate(
                    projections['fcff'], wacc, company_data.shares_outstanding,
                    market_data.growth_rate, None,
                    company_data.financial_data.get('cash', 0),
                    company_data.financial_data.get('total_debt', 0),
                    0, company_data.current_price
                )
            except Exception as e:
                results['fcff'] = f"Error: {str(e)}"

        # FCFE Model
        if 'fcfe' in projections:
            try:
                results['fcfe'] = self.fcfe_model.calculate(
                    projections['fcfe'], market_data.required_return,
                    company_data.shares_outstanding, market_data.growth_rate,
                    None, company_data.current_price
                )
            except Exception as e:
                results['fcfe'] = f"Error: {str(e)}"

        return results

    def calculate_implicit_forecasts(self, current_price: float, shares_outstanding: float,
                                     wacc: float, terminal_growth: float,
                                     projection_years: int = 5) -> Dict[str, Any]:
        """Calculate implicit FCFF forecasts based on current market price"""

        # This is a reverse DCF - what FCF growth is implied by current price
        # Simplified approach: assume constant growth to terminal value

        # Market equity value
        market_equity_value = current_price * shares_outstanding

        # Assume terminal value is 80% of total value (typical assumption)
        terminal_value_percentage = 0.8
        pv_terminal = market_equity_value * terminal_value_percentage
        pv_growth_stage = market_equity_value * (1 - terminal_value_percentage)

        # Back-calculate required FCFF
        # This is simplified - actual implementation would be more complex
        implied_terminal_fcff = pv_terminal * (wacc - terminal_growth) / ((1 + wacc) ** projection_years)

        # Implied first year FCFF (assuming constant growth during projection period)
        growth_factor = ((1 + terminal_growth) ** projection_years)
        implied_initial_fcff = implied_terminal_fcff / growth_factor

        return {
            'market_equity_value': market_equity_value,
            'implied_terminal_fcff': implied_terminal_fcff,
            'implied_initial_fcff': implied_initial_fcff,
            'implied_growth_rate': terminal_growth,
            'assumptions': {
                'terminal_value_percentage': terminal_value_percentage,
                'projection_years': projection_years,
                'wacc': wacc,
                'terminal_growth': terminal_growth
            }
        }

    def forecast_cash_flows(self, historical_financials: pd.DataFrame,
                            growth_assumptions: Dict[str, float],
                            projection_years: int = 5) -> Dict[str, List[float]]:
        """Forecast future cash flows based on historical data and assumptions"""

        # Get base year data (most recent year)
        base_year = historical_financials.iloc[-1]

        # Revenue growth assumption
        revenue_growth = growth_assumptions.get('revenue_growth', 0.05)

        # Margin assumptions
        ebitda_margin = growth_assumptions.get('ebitda_margin',
                                               base_year.get('ebitda', 0) / base_year.get('revenue', 1))
        tax_rate = growth_assumptions.get('tax_rate', 0.25)

        # Investment assumptions
        capex_percentage = growth_assumptions.get('capex_percentage', 0.03)  # % of revenue
        depreciation_percentage = growth_assumptions.get('depreciation_percentage', 0.025)

        projections = {
            'revenue': [],
            'ebitda': [],
            'fcff': [],
            'fcfe': []
        }

        current_revenue = base_year.get('revenue', 0)

        for year in range(1, projection_years + 1):
            # Revenue projection
            current_revenue *= (1 + revenue_growth)
            projections['revenue'].append(current_revenue)

            # EBITDA projection
            ebitda = current_revenue * ebitda_margin
            projections['ebitda'].append(ebitda)

            # FCFF calculation
            depreciation = current_revenue * depreciation_percentage
            ebit = ebitda - depreciation
            capex = current_revenue * capex_percentage

            fcff = self.fcff_model.calculate_fcff_from_components(
                ebit, tax_rate, depreciation, capex, 0
            )
            projections['fcff'].append(fcff)

            # FCFE calculation (simplified)
            interest_expense = base_year.get('interest_expense', 0)
            net_borrowing = capex * 0.3  # Assume 30% debt financing

            fcfe = self.fcfe_model.calculate_fcfe_from_fcff(
                fcff, interest_expense, tax_rate, net_borrowing
            )
            projections['fcfe'].append(fcfe)

        return projections


# Convenience functions
def fcff_valuation(fcff_projections: List[float], wacc: float, shares_outstanding: float,
                   terminal_growth: float, cash: float = 0, debt: float = 0,
                   current_price: float = None) -> ValuationResult:
    """Quick FCFF valuation"""
    model = FCFFModel()
    return model.calculate(fcff_projections, wacc, shares_outstanding, terminal_growth,
                           None, cash, debt, 0, current_price)


def fcfe_valuation(fcfe_projections: List[float], required_return: float, shares_outstanding: float,
                   terminal_growth: float, current_price: float = None) -> ValuationResult:
    """Quick FCFE valuation"""
    model = FCFEModel()
    return model.calculate(fcfe_projections, required_return, shares_outstanding,
                           terminal_growth, None, current_price)