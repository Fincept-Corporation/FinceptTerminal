"""
Real Estate Analytics Module
============================

Comprehensive analysis framework for real estate investments including direct property investments, REITs, and infrastructure projects. Provides specialized valuation methodologies, income analysis, and portfolio management tools for real estate assets.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Property acquisition costs and current market values
  - Gross rental income and operating expense data
  - Vacancy rates and occupancy information
  - REIT financial statements (income statement, balance sheet)
  - Infrastructure project cash flow projections
  - Property type and regional market data

OUTPUT:
  - Net Operating Income (NOI) and capitalization rates
  - Direct capitalization and DCF valuations
  - REIT FFO, AFFO, and NAV per share calculations
  - Infrastructure project valuation with regulatory analysis
  - Portfolio diversification and geographic analysis

PARAMETERS:
  - property_type: Real estate property type (OFFICE, RETAIL, RESIDENTIAL, etc.)
  - acquisition_price: Property acquisition cost
  - current_market_value: Current property market value
  - gross_rental_income: Annual gross rental income
  - operating_expenses: Annual operating expenses
  - vacancy_rate: Vacancy rate percentage - default: 0.05 (5%)
  - cap_rate: Capitalization rate
  - shares_outstanding: REIT shares outstanding
  - infrastructure_type: Infrastructure asset type
  - regulatory_framework: Regulatory environment (regulated, merchant)
  - concession_period: Infrastructure concession period in years
  - revenue_model: Revenue model (user_pays, availability, hybrid)
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config, RealEstateType
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class RealEstateAnalyzer(AlternativeInvestmentBase):
    """
    Real Estate investment analysis and valuation
    CFA Standards: DCF, Direct Capitalization, Sales Comparison approaches
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.property_type = getattr(parameters, 'property_type', RealEstateType.OFFICE)
        self.acquisition_price = getattr(parameters, 'acquisition_price', None)
        self.current_market_value = getattr(parameters, 'current_market_value', None)
        self.gross_rental_income = getattr(parameters, 'gross_rental_income', None)
        self.operating_expenses = getattr(parameters, 'operating_expenses', None)
        self.vacancy_rate = getattr(parameters, 'vacancy_rate', Decimal('0.05'))  # 5% default
        self.cap_rate = getattr(parameters, 'cap_rate', None)

    def calculate_noi(self) -> Decimal:
        """
        Calculate Net Operating Income
        CFA Standard: NOI = Gross Rental Income - Operating Expenses - Vacancy Loss
        """
        if not self.gross_rental_income:
            return Decimal('0')

        effective_gross_income = self.gross_rental_income * (Decimal('1') - self.vacancy_rate)
        operating_expenses = self.operating_expenses or Decimal('0')

        noi = effective_gross_income - operating_expenses
        return max(noi, Decimal('0'))

    def calculate_cap_rate(self, market_value: Decimal = None) -> Optional[Decimal]:
        """
        Calculate Capitalization Rate
        CFA Standard: Cap Rate = NOI / Property Value
        """
        noi = self.calculate_noi()
        value = market_value or self.current_market_value or self.acquisition_price

        if not value or value == 0 or noi <= 0:
            return None

        cap_rate = noi / value

        # Validate cap rate is within reasonable range
        if self.config.RE_CAP_RATE_MIN <= cap_rate <= self.config.RE_CAP_RATE_MAX:
            return cap_rate

        logger.warning(f"Calculated cap rate {cap_rate} outside normal range")
        return cap_rate

    def direct_capitalization_value(self, market_cap_rate: Decimal) -> Decimal:
        """
        Direct Capitalization valuation approach
        CFA Standard: Property Value = NOI / Cap Rate
        """
        noi = self.calculate_noi()

        if market_cap_rate <= 0:
            raise ValueError("Cap rate must be positive")

        return noi / market_cap_rate

    def dcf_valuation(self, projection_years: int = 10,
                      terminal_cap_rate: Decimal = None,
                      discount_rate: Decimal = None) -> Dict[str, Decimal]:
        """
        Discounted Cash Flow valuation
        CFA Standard: DCF approach for income-producing real estate
        """
        if not self.gross_rental_income:
            return {"error": "Gross rental income required for DCF"}

        if discount_rate is None:
            discount_rate = self.config.RISK_FREE_RATE + Decimal('0.04')  # Add 4% risk premium

        if terminal_cap_rate is None:
            terminal_cap_rate = self.calculate_cap_rate() or Decimal('0.06')

        # Project cash flows
        current_noi = self.calculate_noi()
        annual_growth_rate = Decimal('0.03')  # 3% annual growth assumption

        projected_cfs = []
        for year in range(1, projection_years + 1):
            projected_noi = current_noi * ((Decimal('1') + annual_growth_rate) ** year)
            projected_cfs.append(projected_noi)

        # Calculate terminal value
        terminal_noi = projected_cfs[-1] * (Decimal('1') + annual_growth_rate)
        terminal_value = terminal_noi / terminal_cap_rate

        # Discount cash flows to present value
        pv_cash_flows = Decimal('0')
        for year, cf in enumerate(projected_cfs, 1):
            pv = cf / ((Decimal('1') + discount_rate) ** year)
            pv_cash_flows += pv

        # Discount terminal value
        pv_terminal = terminal_value / ((Decimal('1') + discount_rate) ** projection_years)

        total_property_value = pv_cash_flows + pv_terminal

        return {
            "dcf_value": total_property_value,
            "pv_cash_flows": pv_cash_flows,
            "pv_terminal_value": pv_terminal,
            "terminal_value": terminal_value,
            "implied_cap_rate": current_noi / total_property_value if total_property_value > 0 else Decimal('0')
        }

    def calculate_real_estate_ratios(self) -> Dict[str, Decimal]:
        """Calculate key real estate financial ratios"""
        ratios = {}

        # Debt Service Coverage Ratio (if debt information available)
        noi = self.calculate_noi()

        # Operating Expense Ratio
        if self.gross_rental_income and self.operating_expenses:
            effective_gross = self.gross_rental_income * (Decimal('1') - self.vacancy_rate)
            expense_ratio = self.operating_expenses / effective_gross
            ratios['operating_expense_ratio'] = expense_ratio

        # NOI Margin
        if self.gross_rental_income:
            effective_gross = self.gross_rental_income * (Decimal('1') - self.vacancy_rate)
            noi_margin = noi / effective_gross if effective_gross > 0 else Decimal('0')
            ratios['noi_margin'] = noi_margin

        # Occupancy Rate
        occupancy_rate = Decimal('1') - self.vacancy_rate
        ratios['occupancy_rate'] = occupancy_rate

        return ratios

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV based on market value or DCF"""
        if self.current_market_value:
            return self.current_market_value

        # Use DCF valuation as fallback
        dcf_result = self.dcf_valuation()
        if isinstance(dcf_result, dict) and 'dcf_value' in dcf_result:
            return dcf_result['dcf_value']

        return self.acquisition_price or Decimal('0')

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key real estate metrics"""
        metrics = {}

        # Basic metrics
        noi = self.calculate_noi()
        metrics['noi'] = float(noi)

        cap_rate = self.calculate_cap_rate()
        if cap_rate:
            metrics['cap_rate'] = float(cap_rate)

        # Ratios
        ratios = self.calculate_real_estate_ratios()
        for key, value in ratios.items():
            metrics[key] = float(value)

        # Valuation metrics
        dcf_result = self.dcf_valuation()
        if isinstance(dcf_result, dict) and 'dcf_value' in dcf_result:
            metrics['dcf_valuation'] = float(dcf_result['dcf_value'])
            metrics['implied_cap_rate'] = float(dcf_result['implied_cap_rate'])

        # Performance metrics
        if self.acquisition_price and self.current_market_value:
            total_return = (self.current_market_value - self.acquisition_price) / self.acquisition_price
            metrics['capital_appreciation'] = float(total_return)

        # Income yield
        if self.current_market_value or self.acquisition_price:
            property_value = self.current_market_value or self.acquisition_price
            income_yield = noi / property_value if property_value > 0 else Decimal('0')
            metrics['income_yield'] = float(income_yield)

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive real estate valuation summary"""
        return {
            "property_overview": {
                "property_type": self.property_type.value,
                "acquisition_price": float(self.acquisition_price) if self.acquisition_price else None,
                "current_market_value": float(self.current_market_value) if self.current_market_value else None,
                "gross_rental_income": float(self.gross_rental_income) if self.gross_rental_income else None,
                "vacancy_rate": float(self.vacancy_rate)
            },
            "financial_metrics": self.calculate_key_metrics(),
            "valuation_approaches": {
                "direct_cap": float(self.direct_capitalization_value(self.cap_rate)) if self.cap_rate else None,
                "dcf": self.dcf_valuation()
            }
        }


class REITAnalyzer(AlternativeInvestmentBase):
    """
    Real Estate Investment Trust (REIT) analysis
    CFA Standards: NAV, FFO, AFFO calculations and valuation
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.shares_outstanding = getattr(parameters, 'shares_outstanding', None)
        self.total_assets = getattr(parameters, 'total_assets', None)
        self.total_debt = getattr(parameters, 'total_debt', None)
        self.property_value = getattr(parameters, 'property_value', None)
        self.net_income = getattr(parameters, 'net_income', None)
        self.depreciation = getattr(parameters, 'depreciation', None)
        self.amortization = getattr(parameters, 'amortization', None)
        self.gains_on_sales = getattr(parameters, 'gains_on_sales', Decimal('0'))
        self.recurring_capex = getattr(parameters, 'recurring_capex', None)
        self.leasing_costs = getattr(parameters, 'leasing_costs', None)

    def calculate_ffo(self) -> Optional[Decimal]:
        """
        Calculate Funds From Operations
        CFA Standard: FFO = Net Income + Depreciation + Amortization - Gains on Sales
        """
        if not all([self.net_income, self.depreciation]):
            return None

        ffo = self.net_income + self.depreciation

        if self.amortization:
            ffo += self.amortization

        if self.gains_on_sales:
            ffo -= self.gains_on_sales

        return ffo

    def calculate_affo(self) -> Optional[Decimal]:
        """
        Calculate Adjusted Funds From Operations
        CFA Standard: AFFO = FFO - Recurring Capital Expenditures - Leasing Costs
        """
        ffo = self.calculate_ffo()

        if ffo is None:
            return None

        affo = ffo

        if self.recurring_capex:
            affo -= self.recurring_capex

        if self.leasing_costs:
            affo -= self.leasing_costs

        return affo

    def calculate_nav_per_share(self, property_values: Dict[str, Decimal] = None) -> Optional[Decimal]:
        """
        Calculate Net Asset Value per Share
        CFA Standard: NAVPS = (Total Property Value - Total Debt) / Shares Outstanding
        """
        if not self.shares_outstanding:
            return None

        # Use provided property values or estimated value
        total_property_value = self.property_value
        if property_values:
            total_property_value = sum(property_values.values())

        if not total_property_value:
            # Estimate using asset value
            total_property_value = self.total_assets or Decimal('0')

        total_debt = self.total_debt or Decimal('0')
        equity_value = total_property_value - total_debt

        navps = equity_value / self.shares_outstanding
        return navps

    def reit_valuation_ratios(self, current_share_price: Decimal) -> Dict[str, Optional[Decimal]]:
        """Calculate key REIT valuation ratios"""
        ratios = {}

        # Price-to-FFO ratio
        ffo = self.calculate_ffo()
        if ffo and self.shares_outstanding:
            ffo_per_share = ffo / self.shares_outstanding
            if ffo_per_share > 0:
                ratios['price_to_ffo'] = current_share_price / ffo_per_share
                ratios['ffo_per_share'] = ffo_per_share

        # Price-to-AFFO ratio
        affo = self.calculate_affo()
        if affo and self.shares_outstanding:
            affo_per_share = affo / self.shares_outstanding
            if affo_per_share > 0:
                ratios['price_to_affo'] = current_share_price / affo_per_share
                ratios['affo_per_share'] = affo_per_share

        # Price-to-NAV ratio
        navps = self.calculate_nav_per_share()
        if navps:
            ratios['price_to_nav'] = current_share_price / navps
            ratios['nav_per_share'] = navps

        # Debt-to-equity ratio
        if self.total_debt and self.property_value:
            equity_value = self.property_value - self.total_debt
            if equity_value > 0:
                ratios['debt_to_equity'] = self.total_debt / equity_value

        return ratios

    def dividend_analysis(self, annual_dividend: Decimal, current_price: Decimal) -> Dict[str, Decimal]:
        """Analyze REIT dividend metrics"""
        analysis = {}

        # Dividend yield
        dividend_yield = annual_dividend / current_price
        analysis['dividend_yield'] = dividend_yield

        # FFO payout ratio
        ffo = self.calculate_ffo()
        if ffo and self.shares_outstanding:
            total_dividends = annual_dividend * self.shares_outstanding
            ffo_payout_ratio = total_dividends / ffo
            analysis['ffo_payout_ratio'] = ffo_payout_ratio

        # AFFO payout ratio
        affo = self.calculate_affo()
        if affo and self.shares_outstanding:
            total_dividends = annual_dividend * self.shares_outstanding
            affo_payout_ratio = total_dividends / affo
            analysis['affo_payout_ratio'] = affo_payout_ratio

        return analysis

    def calculate_nav(self) -> Decimal:
        """Calculate REIT NAV"""
        navps = self.calculate_nav_per_share()
        if navps and self.shares_outstanding:
            return navps * self.shares_outstanding

        # Fallback to property value minus debt
        property_val = self.property_value or self.total_assets or Decimal('0')
        debt = self.total_debt or Decimal('0')
        return property_val - debt

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key REIT metrics"""
        metrics = {}

        # Core REIT metrics
        ffo = self.calculate_ffo()
        if ffo:
            metrics['ffo'] = float(ffo)
            if self.shares_outstanding:
                metrics['ffo_per_share'] = float(ffo / self.shares_outstanding)

        affo = self.calculate_affo()
        if affo:
            metrics['affo'] = float(affo)
            if self.shares_outstanding:
                metrics['affo_per_share'] = float(affo / self.shares_outstanding)

        navps = self.calculate_nav_per_share()
        if navps:
            metrics['nav_per_share'] = float(navps)

        # Financial ratios
        if self.total_debt and self.total_assets:
            debt_ratio = self.total_debt / self.total_assets
            metrics['debt_to_assets'] = float(debt_ratio)

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive REIT valuation summary"""
        return {
            "reit_overview": {
                "shares_outstanding": float(self.shares_outstanding) if self.shares_outstanding else None,
                "total_assets": float(self.total_assets) if self.total_assets else None,
                "total_debt": float(self.total_debt) if self.total_debt else None,
                "property_value": float(self.property_value) if self.property_value else None
            },
            "operating_metrics": self.calculate_key_metrics(),
            "valuation_metrics": {
                "ffo": float(self.calculate_ffo()) if self.calculate_ffo() else None,
                "affo": float(self.calculate_affo()) if self.calculate_affo() else None,
                "nav_per_share": float(self.calculate_nav_per_share()) if self.calculate_nav_per_share() else None
            }
        }


class InfrastructureAnalyzer(AlternativeInvestmentBase):
    """
    Infrastructure investment analysis
    CFA Standards: Project finance, regulated utility analysis
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.infrastructure_type = getattr(parameters, 'infrastructure_type', 'transportation')
        self.regulatory_framework = getattr(parameters, 'regulatory_framework', 'regulated')
        self.concession_period = getattr(parameters, 'concession_period', None)  # years
        self.revenue_model = getattr(parameters, 'revenue_model', 'user_pays')  # user_pays, availability, hybrid
        self.annual_revenue = getattr(parameters, 'annual_revenue', None)
        self.operating_costs = getattr(parameters, 'operating_costs', None)
        self.maintenance_capex = getattr(parameters, 'maintenance_capex', None)
        self.inflation_indexation = getattr(parameters, 'inflation_indexation', True)

    def project_cash_flows(self, projection_years: int = None) -> List[Dict[str, Decimal]]:
        """
        Project infrastructure cash flows
        CFA Standard: DCF for infrastructure projects
        """
        if projection_years is None:
            projection_years = self.concession_period or 25

        if not self.annual_revenue:
            return []

        cash_flows = []
        inflation_rate = Decimal('0.025')  # 2.5% annual inflation

        for year in range(1, projection_years + 1):
            # Escalate revenues and costs for inflation
            if self.inflation_indexation:
                revenue = self.annual_revenue * ((Decimal('1') + inflation_rate) ** (year - 1))
                opex = (self.operating_costs or Decimal('0')) * ((Decimal('1') + inflation_rate) ** (year - 1))
                maintenance = (self.maintenance_capex or Decimal('0')) * ((Decimal('1') + inflation_rate) ** (year - 1))
            else:
                revenue = self.annual_revenue
                opex = self.operating_costs or Decimal('0')
                maintenance = self.maintenance_capex or Decimal('0')

            ebitda = revenue - opex
            free_cash_flow = ebitda - maintenance

            cash_flows.append({
                'year': year,
                'revenue': revenue,
                'operating_costs': opex,
                'maintenance_capex': maintenance,
                'ebitda': ebitda,
                'free_cash_flow': free_cash_flow
            })

        return cash_flows

    def infrastructure_valuation(self, discount_rate: Decimal = None) -> Dict[str, Decimal]:
        """
        Value infrastructure investment using DCF
        """
        if discount_rate is None:
            # Infrastructure typically uses lower discount rates due to stable cash flows
            discount_rate = self.config.RISK_FREE_RATE + Decimal('0.03')  # 3% risk premium

        projected_cfs = self.project_cash_flows()

        if not projected_cfs:
            return {"error": "Cannot project cash flows"}

        present_value = Decimal('0')

        for cf in projected_cfs:
            year = cf['year']
            fcf = cf['free_cash_flow']
            pv = fcf / ((Decimal('1') + discount_rate) ** year)
            present_value += pv

        return {
            "enterprise_value": present_value,
            "discount_rate": discount_rate,
            "projection_years": len(projected_cfs),
            "terminal_year_fcf": projected_cfs[-1]['free_cash_flow'] if projected_cfs else Decimal('0')
        }

    def regulatory_risk_assessment(self) -> Dict[str, Any]:
        """Assess regulatory risk factors"""
        risk_factors = {
            "regulatory_framework": self.regulatory_framework,
            "concession_period": self.concession_period,
            "revenue_model": self.revenue_model,
            "inflation_protection": self.inflation_indexation
        }

        # Risk scoring (simplified)
        risk_score = 0

        if self.regulatory_framework == 'regulated':
            risk_score += 1  # Lower risk
        elif self.regulatory_framework == 'merchant':
            risk_score += 3  # Higher risk

        if self.revenue_model == 'availability':
            risk_score += 1  # Lower demand risk
        elif self.revenue_model == 'user_pays':
            risk_score += 2  # Higher demand risk

        if not self.inflation_indexation:
            risk_score += 1  # Additional inflation risk

        risk_factors['risk_score'] = risk_score
        risk_factors['risk_level'] = 'Low' if risk_score <= 3 else 'Medium' if risk_score <= 5 else 'High'

        return risk_factors

    def calculate_nav(self) -> Decimal:
        """Calculate infrastructure NAV using DCF"""
        valuation = self.infrastructure_valuation()
        if isinstance(valuation, dict) and 'enterprise_value' in valuation:
            return valuation['enterprise_value']
        return Decimal('0')

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key infrastructure metrics"""
        metrics = {}

        # Valuation metrics
        valuation = self.infrastructure_valuation()
        if isinstance(valuation, dict):
            for key, value in valuation.items():
                if isinstance(value, Decimal):
                    metrics[key] = float(value)
                else:
                    metrics[key] = value

        # Operating metrics
        if self.annual_revenue and self.operating_costs:
            ebitda_margin = (self.annual_revenue - self.operating_costs) / self.annual_revenue
            metrics['ebitda_margin'] = float(ebitda_margin)

        # Risk assessment
        risk_assessment = self.regulatory_risk_assessment()
        metrics.update(risk_assessment)

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive infrastructure valuation summary"""
        return {
            "infrastructure_overview": {
                "infrastructure_type": self.infrastructure_type,
                "regulatory_framework": self.regulatory_framework,
                "revenue_model": self.revenue_model,
                "concession_period": self.concession_period,
                "inflation_indexation": self.inflation_indexation
            },
            "financial_projections": self.project_cash_flows(),
            "valuation_analysis": self.infrastructure_valuation(),
            "risk_assessment": self.regulatory_risk_assessment()
        }


class RealEstatePortfolio:
    """
    Portfolio-level real estate analysis
    CFA Standards: Portfolio diversification and risk management
    """

    def __init__(self):
        self.real_estate_investments: List[RealEstateAnalyzer] = []
        self.reit_investments: List[REITAnalyzer] = []
        self.infrastructure_investments: List[InfrastructureAnalyzer] = []

    def add_real_estate(self, investment: RealEstateAnalyzer) -> None:
        """Add direct real estate investment"""
        self.real_estate_investments.append(investment)

    def add_reit(self, reit: REITAnalyzer) -> None:
        """Add REIT investment"""
        self.reit_investments.append(reit)

    def add_infrastructure(self, infrastructure: InfrastructureAnalyzer) -> None:
        """Add infrastructure investment"""
        self.infrastructure_investments.append(infrastructure)

    def portfolio_summary(self) -> Dict[str, Any]:
        """Generate comprehensive real estate portfolio summary"""
        total_nav = Decimal('0')

        # Direct real estate
        re_navs = [inv.calculate_nav() for inv in self.real_estate_investments]
        re_total = sum(re_navs)
        total_nav += re_total

        # REITs
        reit_navs = [reit.calculate_nav() for reit in self.reit_investments]
        reit_total = sum(reit_navs)
        total_nav += reit_total

        # Infrastructure
        infra_navs = [infra.calculate_nav() for infra in self.infrastructure_investments]
        infra_total = sum(infra_navs)
        total_nav += infra_total

        summary = {
            "portfolio_overview": {
                "total_portfolio_nav": float(total_nav),
                "direct_real_estate_count": len(self.real_estate_investments),
                "reit_count": len(self.reit_investments),
                "infrastructure_count": len(self.infrastructure_investments)
            },
            "allocation": {
                "direct_real_estate": {
                    "nav": float(re_total),
                    "weight": float(re_total / total_nav) if total_nav > 0 else 0
                },
                "reits": {
                    "nav": float(reit_total),
                    "weight": float(reit_total / total_nav) if total_nav > 0 else 0
                },
                "infrastructure": {
                    "nav": float(infra_total),
                    "weight": float(infra_total / total_nav) if total_nav > 0 else 0
                }
            }
        }

        return summary

    def geographic_diversification(self) -> Dict[str, Any]:
        """Analyze geographic diversification"""
        # Simplified geographic analysis
        regions = {}

        for inv in self.real_estate_investments:
            region = getattr(inv.parameters, 'region', 'Unknown')
            if region not in regions:
                regions[region] = []
            regions[region].append(inv.calculate_nav())

        diversification = {}
        total_value = sum(sum(navs) for navs in regions.values())

        for region, navs in regions.items():
            region_value = sum(navs)
            diversification[region] = {
                "count": len(navs),
                "total_value": float(region_value),
                "weight": float(region_value / total_value) if total_value > 0 else 0
            }

        return diversification


# Export main components
__all__ = ['RealEstateAnalyzer', 'REITAnalyzer', 'InfrastructureAnalyzer', 'RealEstatePortfolio']