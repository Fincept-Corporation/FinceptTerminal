"""
Equity Investment Residual Income Module
======================================

Residual income and economic value added models

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
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
import math

from .base_models import (
    BaseValuationModel, CompanyData, MarketData, ValuationResult,
    ValuationMethod, CalculationEngine, ModelValidator, ValidationError
)


@dataclass
class ResidualIncomeParameters:
    """Parameters for Residual Income calculations"""
    book_values: List[float]
    net_incomes: List[float]
    required_return: float
    terminal_roe: Optional[float] = None
    terminal_growth: Optional[float] = None


class ResidualIncomeModel(BaseValuationModel):
    """Single-stage and Multi-stage Residual Income Model"""

    def __init__(self):
        super().__init__("Residual Income Model", "Residual income valuation")
        self.valuation_method = ValuationMethod.RESIDUAL_INCOME

    def calculate_residual_income(self, net_income: float, beginning_book_value: float,
                                  required_return: float) -> float:
        """Calculate residual income for a single period"""
        return CalculationEngine.residual_income(net_income, beginning_book_value, required_return)

    def calculate_continuing_residual_income(self, final_ri: float, required_return: float,
                                             growth_rate: float) -> float:
        """Calculate continuing residual income value"""
        if growth_rate >= required_return:
            raise ValidationError("Growth rate must be less than required return for continuing RI")

        # Continuing RI = RI_n+1 / (r - g)
        next_ri = final_ri * (1 + growth_rate)
        return next_ri / (required_return - growth_rate)

    def calculate_single_stage_ri_value(self, current_book_value: float, roe: float,
                                        required_return: float, growth_rate: float = 0) -> float:
        """Calculate single-stage constant growth RI value"""

        if growth_rate >= required_return:
            raise ValidationError("Growth rate must be less than required return")

        # Calculate next period's residual income
        next_net_income = current_book_value * roe * (1 + growth_rate)
        next_book_value = current_book_value * (1 + growth_rate)
        next_ri = self.calculate_residual_income(next_net_income, current_book_value, required_return)

        # Present value of all future RIs
        if growth_rate == 0:
            pv_future_ris = next_ri / required_return
        else:
            pv_future_ris = next_ri / (required_return - growth_rate)

        # Total value = Current BV + PV of future RIs
        return current_book_value + pv_future_ris

    def calculate_multistage_ri_value(self, current_book_value: float,
                                      projected_ris: List[float], required_return: float,
                                      terminal_ri: float = None, terminal_growth: float = None) -> Dict[str, float]:
        """Calculate multi-stage RI value"""

        # Present value of projected residual incomes
        pv_projected_ris = 0
        pv_details = []

        for year, ri in enumerate(projected_ris, 1):
            pv_ri = CalculationEngine.present_value(ri, required_return, year)
            pv_projected_ris += pv_ri
            pv_details.append({'year': year, 'ri': ri, 'pv_ri': pv_ri})

        # Terminal value
        if terminal_ri is not None:
            if terminal_growth is None:
                terminal_growth = 0

            continuing_ri_value = self.calculate_continuing_residual_income(
                terminal_ri, required_return, terminal_growth
            )
            pv_terminal = CalculationEngine.present_value(continuing_ri_value, required_return, len(projected_ris))
        else:
            pv_terminal = 0

        # Total value
        total_value = current_book_value + pv_projected_ris + pv_terminal

        return {
            'current_book_value': current_book_value,
            'pv_projected_ris': pv_projected_ris,
            'pv_terminal': pv_terminal,
            'total_value': total_value,
            'pv_details': pv_details
        }

    def calculate_intrinsic_value(self, company_data: CompanyData, market_data: MarketData) -> float:
        """Calculate intrinsic value using RI model"""
        financial_data = company_data.financial_data

        book_value_total = financial_data.get('book_value', 0) * company_data.shares_outstanding
        roe = financial_data.get('roe', 0)
        required_return = market_data.required_return
        growth_rate = market_data.growth_rate

        total_value = self.calculate_single_stage_ri_value(book_value_total, roe, required_return, growth_rate)
        return total_value / company_data.shares_outstanding if company_data.shares_outstanding > 0 else 0

    def calculate(self, current_book_value: float, projected_ris: List[float],
                  required_return: float, shares_outstanding: float,
                  terminal_ri: float = None, terminal_growth: float = None,
                  current_price: float = None) -> ValuationResult:
        """Calculate valuation using Residual Income model"""

        # Validate inputs
        ModelValidator.validate_positive_number(current_book_value, "Current book value")
        ModelValidator.validate_percentage(required_return, "Required return")

        if terminal_growth is not None:
            ModelValidator.validate_growth_vs_required_return(terminal_growth, required_return)

        # Calculate RI value
        ri_components = self.calculate_multistage_ri_value(
            current_book_value, projected_ris, required_return, terminal_ri, terminal_growth
        )

        # Per-share value
        intrinsic_value = ri_components['total_value'] / shares_outstanding if shares_outstanding > 0 else 0

        # Store assumptions
        assumptions = {
            'current_book_value': current_book_value,
            'required_return': required_return,
            'terminal_growth_rate': terminal_growth,
            'projection_periods': len(projected_ris),
            'book_value_percentage': (current_book_value / ri_components['total_value']) * 100,
            'terminal_value_percentage': (ri_components['pv_terminal'] / ri_components['total_value']) * 100,
            'model_type': 'Multi-stage Residual Income Model'
        }

        # Detailed calculations
        calculation_details = {
            'projected_ris': projected_ris,
            'pv_projected_ris': ri_components['pv_projected_ris'],
            'pv_terminal': ri_components['pv_terminal'],
            'total_value': ri_components['total_value'],
            'intrinsic_value_per_share': intrinsic_value,
            'pv_details': ri_components['pv_details']
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


class EconomicValueAddedModel(BaseValuationModel):
    """Economic Value Added (EVA) Model"""

    def __init__(self):
        super().__init__("EVA Model", "Economic Value Added valuation")
        self.valuation_method = ValuationMethod.RESIDUAL_INCOME

    def calculate_eva(self, nopat: float, invested_capital: float, wacc: float) -> float:
        """Calculate Economic Value Added"""
        return CalculationEngine.economic_value_added(nopat, invested_capital, wacc)

    def calculate_nopat(self, ebit: float, tax_rate: float) -> float:
        """Calculate Net Operating Profit After Tax"""
        return ebit * (1 - tax_rate)

    def calculate_invested_capital(self, total_assets: float, non_interest_bearing_liabilities: float) -> float:
        """Calculate invested capital"""
        return total_assets - non_interest_bearing_liabilities

    def calculate_eva_from_components(self, ebit: float, tax_rate: float, total_assets: float,
                                      non_interest_bearing_liabilities: float, wacc: float) -> float:
        """Calculate EVA from financial statement components"""
        nopat = self.calculate_nopat(ebit, tax_rate)
        invested_capital = self.calculate_invested_capital(total_assets, non_interest_bearing_liabilities)
        return self.calculate_eva(nopat, invested_capital, wacc)

    def calculate_market_value_added(self, market_value: float, invested_capital: float) -> float:
        """Calculate Market Value Added (MVA)"""
        return market_value - invested_capital

    def eva_valuation(self, current_invested_capital: float, projected_evas: List[float],
                      wacc: float, terminal_eva: float = None, terminal_growth: float = None) -> Dict[str, float]:
        """Calculate firm value using EVA approach"""

        # Present value of projected EVAs
        pv_evas = 0
        for year, eva in enumerate(projected_evas, 1):
            pv_eva = CalculationEngine.present_value(eva, wacc, year)
            pv_evas += pv_eva

        # Terminal value of EVAs
        if terminal_eva is not None:
            if terminal_growth is None:
                terminal_growth = 0

            if terminal_growth >= wacc:
                raise ValidationError("Terminal growth must be less than WACC for EVA terminal value")

            next_eva = terminal_eva * (1 + terminal_growth)
            terminal_value = next_eva / (wacc - terminal_growth)
            pv_terminal = CalculationEngine.present_value(terminal_value, wacc, len(projected_evas))
        else:
            pv_terminal = 0

        # Total firm value = Invested Capital + PV of future EVAs
        firm_value = current_invested_capital + pv_evas + pv_terminal

        return {
            'current_invested_capital': current_invested_capital,
            'pv_projected_evas': pv_evas,
            'pv_terminal_evas': pv_terminal,
            'total_firm_value': firm_value
        }


class ResidualIncomeAnalyzer:
    """Comprehensive Residual Income analysis tools"""

    def __init__(self):
        self.ri_model = ResidualIncomeModel()
        self.eva_model = EconomicValueAddedModel()

    def calculate_implied_growth_rate(self, current_price: float, current_book_value_per_share: float,
                                      roe: float, required_return: float) -> float:
        """Calculate implied growth rate from current market P/B ratio"""

        pb_ratio = current_price / current_book_value_per_share

        # From RI model: P/B = 1 + (ROE - r) / (r - g)
        # Solving for g: g = r - (ROE - r) / (P/B - 1)

        if pb_ratio <= 1:
            raise ValidationError("P/B ratio must be greater than 1 for growth calculation")

        implied_growth = required_return - (roe - required_return) / (pb_ratio - 1)
        return implied_growth

    def calculate_fundamental_pb_ratio(self, roe: float, required_return: float,
                                       growth_rate: float) -> float:
        """Calculate justified P/B ratio from fundamentals"""
        if required_return <= growth_rate:
            raise ValidationError("Required return must be greater than growth rate")

        # P/B = 1 + (ROE - r) / (r - g)
        return 1 + (roe - required_return) / (required_return - growth_rate)

    def analyze_roe_sustainability(self, historical_roe: List[float],
                                   industry_avg_roe: float) -> Dict[str, Any]:
        """Analyze ROE sustainability and mean reversion"""

        if not historical_roe:
            raise ValidationError("Historical ROE data required")

        current_roe = historical_roe[-1]
        avg_roe = np.mean(historical_roe)
        roe_volatility = np.std(historical_roe) if len(historical_roe) > 1 else 0

        # Mean reversion analysis
        roe_trend = np.polyfit(range(len(historical_roe)), historical_roe, 1)[0]

        # Sustainability assessment
        sustainability_score = "HIGH"
        warnings = []

        if current_roe > 0.25:  # 25% ROE
            sustainability_score = "LOW"
            warnings.append("ROE above 25% typically not sustainable long-term")

        if abs(current_roe - industry_avg_roe) > 0.1:  # 10% difference
            warnings.append("ROE significantly different from industry average")

        if roe_volatility > 0.05:  # 5% volatility
            warnings.append("High ROE volatility indicates uncertainty")

        return {
            'current_roe': current_roe,
            'average_roe': avg_roe,
            'industry_avg_roe': industry_avg_roe,
            'roe_volatility': roe_volatility,
            'roe_trend': roe_trend,
            'sustainability_score': sustainability_score,
            'warnings': warnings
        }

    def forecast_residual_income(self, current_book_value: float, projected_roes: List[float],
                                 required_return: float, payout_ratios: List[float] = None) -> List[float]:
        """Forecast residual income based on ROE and growth assumptions"""

        if payout_ratios is None:
            payout_ratios = [0.4] * len(projected_roes)  # 40% default payout ratio

        if len(payout_ratios) != len(projected_roes):
            raise ValidationError("Payout ratios and ROEs must have same length")

        projected_ris = []
        book_value = current_book_value

        for i, (roe, payout_ratio) in enumerate(zip(projected_roes, payout_ratios)):
            # Calculate net income
            net_income = book_value * roe

            # Calculate residual income
            ri = self.ri_model.calculate_residual_income(net_income, book_value, required_return)
            projected_ris.append(ri)

            # Update book value for next period
            retention_ratio = 1 - payout_ratio
            book_value *= (1 + roe * retention_ratio)

        return projected_ris

    def compare_ri_with_other_models(self, company_data: CompanyData, market_data: MarketData,
                                     ddm_value: float = None, dcf_value: float = None) -> Dict[str, Any]:
        """Compare RI valuation with DDM and DCF models"""

        # Calculate RI value
        ri_value = self.ri_model.calculate_intrinsic_value(company_data, market_data)

        comparison = {
            'ri_value': ri_value,
            'current_price': company_data.current_price
        }

        if ddm_value:
            comparison['ddm_value'] = ddm_value
            comparison['ri_vs_ddm_diff'] = (ri_value - ddm_value) / ddm_value * 100

        if dcf_value:
            comparison['dcf_value'] = dcf_value
            comparison['ri_vs_dcf_diff'] = (ri_value - dcf_value) / dcf_value * 100

        # Value recognition analysis
        book_value_per_share = (company_data.financial_data.get('book_value', 0) *
                                company_data.shares_outstanding) / company_data.shares_outstanding

        comparison['book_value_per_share'] = book_value_per_share
        comparison['market_to_book'] = company_data.current_price / book_value_per_share
        comparison['ri_to_book'] = ri_value / book_value_per_share

        return comparison

    def accounting_quality_assessment(self, financial_statements: Dict[str, pd.DataFrame]) -> Dict[str, Any]:
        """Assess accounting quality for RI model reliability"""

        income_stmt = financial_statements.get('income_statement', pd.DataFrame())
        balance_sheet = financial_statements.get('balance_sheet', pd.DataFrame())

        quality_issues = []
        quality_score = 100  # Start with perfect score

        if income_stmt.empty or balance_sheet.empty:
            return {'quality_score': 0, 'issues': ['Financial statements not available']}

        try:
            # Check for aggressive revenue recognition
            revenue_growth = income_stmt['revenue'].pct_change().iloc[-1] if 'revenue' in income_stmt.columns else 0
            receivables_growth = balance_sheet['accounts_receivable'].pct_change().iloc[
                -1] if 'accounts_receivable' in balance_sheet.columns else 0

            if abs(receivables_growth) > abs(revenue_growth) * 1.5:
                quality_issues.append("Receivables growing much faster than revenue")
                quality_score -= 20

            # Check for inventory issues
            if 'inventory' in balance_sheet.columns and 'cost_of_goods_sold' in income_stmt.columns:
                inventory_turnover = income_stmt['cost_of_goods_sold'].iloc[-1] / balance_sheet['inventory'].iloc[-1]
                if inventory_turnover < 2:  # Very low turnover
                    quality_issues.append("Low inventory turnover may indicate obsolete inventory")
                    quality_score -= 15

            # Check for excessive goodwill
            if 'goodwill' in balance_sheet.columns and 'total_assets' in balance_sheet.columns:
                goodwill_ratio = balance_sheet['goodwill'].iloc[-1] / balance_sheet['total_assets'].iloc[-1]
                if goodwill_ratio > 0.3:  # More than 30% goodwill
                    quality_issues.append("High goodwill percentage increases impairment risk")
                    quality_score -= 10

            # Check for debt structure
            if 'total_debt' in balance_sheet.columns and 'total_equity' in balance_sheet.columns:
                debt_to_equity = balance_sheet['total_debt'].iloc[-1] / balance_sheet['total_equity'].iloc[-1]
                if debt_to_equity > 2:  # High leverage
                    quality_issues.append("High leverage may distort ROE calculations")
                    quality_score -= 10

        except Exception as e:
            quality_issues.append(f"Error in accounting quality assessment: {str(e)}")
            quality_score -= 30

        return {
            'quality_score': max(0, quality_score),
            'issues': quality_issues,
            'recommendations': [
                "Verify revenue recognition policies",
                "Check for off-balance-sheet items",
                "Analyze working capital components",
                "Review goodwill and intangible assets"
            ]
        }


# Convenience functions
def single_stage_ri_valuation(current_book_value: float, roe: float, required_return: float,
                              growth_rate: float, shares_outstanding: float) -> float:
    """Quick single-stage RI valuation"""
    model = ResidualIncomeModel()
    total_value = model.calculate_single_stage_ri_value(current_book_value, roe, required_return, growth_rate)
    return total_value / shares_outstanding


def eva_firm_valuation(current_invested_capital: float, projected_evas: List[float],
                       wacc: float, terminal_growth: float = 0) -> float:
    """Quick EVA firm valuation"""
    model = EconomicValueAddedModel()
    terminal_eva = projected_evas[-1] if projected_evas else 0
    components = model.eva_valuation(current_invested_capital, projected_evas, wacc, terminal_eva, terminal_growth)
    return components['total_firm_value']


def calculate_justified_pb_ratio(roe: float, required_return: float, growth_rate: float) -> float:
    """Quick justified P/B ratio calculation"""
    analyzer = ResidualIncomeAnalyzer()
    return analyzer.calculate_fundamental_pb_ratio(roe, required_return, growth_rate)