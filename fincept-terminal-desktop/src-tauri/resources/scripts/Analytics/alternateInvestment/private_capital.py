"""
Private Capital Analytics Module
================================

Comprehensive analysis framework for private capital investments including private equity funds, private debt instruments, and direct investments. Provides specialized valuation methodologies, performance metrics, and cash flow analysis for illiquid investment vehicles.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Capital calls and distribution cash flow data
  - Fund NAV values and reporting dates
  - Commitment amounts and vintage years
  - Private debt instrument details (principal, coupon, maturity)
  - Credit ratings and seniority information for debt investments
  - Fee structures and terms

OUTPUT:
  - IRR, MOIC, DPI, RVPI, TVPI performance metrics
  - Private equity fund performance analysis
  - Private debt yield and duration calculations
  - Credit risk assessment for debt instruments
  - Portfolio-level private capital analysis

PARAMETERS:
  - fund_life: Expected fund life in years - default: Constants.PE_TYPICAL_FUND_LIFE
  - vintage_year: Fund vintage year
  - commitment: Total commitment amount
  - principal_amount: Principal amount for debt investments
  - coupon_rate: Annual coupon rate for debt instruments
  - maturity_date: Maturity date for debt instruments
  - credit_rating: Credit rating for debt investments
  - seniority: Debt seniority level (senior, mezzanine, subordinated) - default: senior
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config, InvestmentMethod
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class PrivateEquityAnalyzer(AlternativeInvestmentBase):
    """
    Private Equity investment analysis and valuation
    CFA Standards: IRR, MOIC, DPI, RVPI calculations and due diligence
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.fund_life = getattr(parameters, 'fund_life', Constants.PE_TYPICAL_FUND_LIFE)
        self.vintage_year = getattr(parameters, 'vintage_year', None)
        self.commitment = getattr(parameters, 'commitment', None)
        self.called_capital = Decimal('0')
        self.distributed_capital = Decimal('0')
        self.current_nav = Decimal('0')

    def add_commitment(self, commitment_amount: Decimal, vintage_year: int) -> None:
        """Record fund commitment"""
        self.commitment = commitment_amount
        self.vintage_year = vintage_year

    def process_capital_call(self, amount: Decimal, call_date: str, description: str = None) -> None:
        """Process capital call from fund"""
        cash_flow = CashFlow(
            date=call_date,
            amount=-abs(amount),  # Negative for outflow
            cf_type='capital_call',
            description=description or f"Capital call - {call_date}"
        )
        self.add_cash_flows([cash_flow])
        self.called_capital += abs(amount)

    def process_distribution(self, amount: Decimal, dist_date: str,
                             distribution_type: str = 'distribution') -> None:
        """Process distribution from fund"""
        cash_flow = CashFlow(
            date=dist_date,
            amount=amount,
            cf_type=distribution_type,
            description=f"{distribution_type} - {dist_date}"
        )
        self.add_cash_flows([cash_flow])
        self.distributed_capital += amount

    def update_nav(self, nav_value: Decimal, nav_date: str) -> None:
        """Update current Net Asset Value"""
        self.current_nav = nav_value
        # Add as market data point
        market_data = MarketData(
            timestamp=nav_date,
            price=nav_value,
            volume=None
        )
        self.add_market_data([market_data])

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.current_nav

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate key PE metrics following CFA standards
        """
        if not self.cash_flows:
            return {"error": "No cash flows available"}

        metrics = {}

        # IRR Calculation
        # Add current NAV as final cash flow for IRR calculation
        cf_for_irr = self.cash_flows.copy()
        if self.current_nav > 0:
            latest_date = max(cf.date for cf in self.cash_flows) if self.cash_flows else datetime.now().strftime(
                '%Y-%m-%d')
            cf_for_irr.append(CashFlow(
                date=latest_date,
                amount=self.current_nav,
                cf_type='nav',
                description='Current NAV'
            ))

        irr = self.math.irr(cf_for_irr)
        metrics['irr'] = float(irr) if irr else None

        # MOIC (Multiple of Invested Capital)
        moic = self.math.moic(cf_for_irr)
        metrics['moic'] = float(moic) if moic else None

        # DPI (Distributions to Paid-In Capital)
        dpi = self.math.dpi(self.cash_flows)
        metrics['dpi'] = float(dpi)

        # RVPI (Residual Value to Paid-In Capital)
        rvpi = self.math.rvpi(self.cash_flows, self.current_nav)
        metrics['rvpi'] = float(rvpi)

        # TVPI (Total Value to Paid-In Capital) = DPI + RVPI
        tvpi = dpi + rvpi
        metrics['tvpi'] = float(tvpi)

        # Fund metrics
        if self.commitment:
            called_ratio = self.called_capital / self.commitment
            metrics['called_capital_ratio'] = float(called_ratio)
            metrics['uncalled_commitment'] = float(self.commitment - self.called_capital)

        metrics['called_capital'] = float(self.called_capital)
        metrics['distributed_capital'] = float(self.distributed_capital)
        metrics['current_nav'] = float(self.current_nav)

        # Vintage year analysis
        if self.vintage_year:
            current_year = datetime.now().year
            fund_age = current_year - self.vintage_year
            metrics['fund_age'] = fund_age
            metrics['vintage_year'] = self.vintage_year

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive PE valuation summary"""
        key_metrics = self.calculate_key_metrics()

        valuation = {
            "investment_overview": {
                "asset_class": self.parameters.asset_class.value,
                "fund_name": self.parameters.name,
                "vintage_year": self.vintage_year,
                "commitment": float(self.commitment) if self.commitment else None
            },
            "capital_account": {
                "total_commitment": float(self.commitment) if self.commitment else None,
                "called_capital": float(self.called_capital),
                "uncalled_commitment": float(self.commitment - self.called_capital) if self.commitment else None,
                "distributed_capital": float(self.distributed_capital),
                "current_nav": float(self.current_nav)
            },
            "performance_metrics": key_metrics,
            "cash_flow_summary": {
                "number_of_capital_calls": len([cf for cf in self.cash_flows if cf.cf_type == 'capital_call']),
                "number_of_distributions": len([cf for cf in self.cash_flows if cf.cf_type == 'distribution']),
                "total_cash_flows": len(self.cash_flows)
            }
        }

        return valuation

    def benchmark_comparison(self, benchmark_irr: Decimal, benchmark_moic: Decimal) -> Dict[str, Any]:
        """Compare performance against benchmark"""
        metrics = self.calculate_key_metrics()

        if not metrics.get('irr') or not metrics.get('moic'):
            return {"error": "Insufficient data for benchmark comparison"}

        fund_irr = Decimal(str(metrics['irr']))
        fund_moic = Decimal(str(metrics['moic']))

        comparison = {
            "fund_performance": {
                "irr": float(fund_irr),
                "moic": float(fund_moic)
            },
            "benchmark_performance": {
                "irr": float(benchmark_irr),
                "moic": float(benchmark_moic)
            },
            "relative_performance": {
                "irr_difference": float(fund_irr - benchmark_irr),
                "moic_difference": float(fund_moic - benchmark_moic),
                "irr_outperformance": fund_irr > benchmark_irr,
                "moic_outperformance": fund_moic > benchmark_moic
            }
        }

        return comparison


class PrivateDebtAnalyzer(AlternativeInvestmentBase):
    """
    Private Debt investment analysis and risk assessment
    CFA Standards: Credit analysis, yield calculations, duration
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.principal_amount = getattr(parameters, 'principal_amount', None)
        self.coupon_rate = getattr(parameters, 'coupon_rate', None)
        self.maturity_date = getattr(parameters, 'maturity_date', None)
        self.credit_rating = getattr(parameters, 'credit_rating', None)
        self.seniority = getattr(parameters, 'seniority', 'senior')  # senior, mezzanine, subordinated
        self.current_price = Decimal('100')  # Par = 100

    def calculate_current_yield(self) -> Decimal:
        """Calculate current yield"""
        if not self.coupon_rate or self.current_price == 0:
            return Decimal('0')

        annual_coupon = self.principal_amount * self.coupon_rate if self.principal_amount else self.coupon_rate
        return annual_coupon / self.current_price

    def calculate_yield_to_maturity(self, current_price: Decimal = None) -> Optional[Decimal]:
        """
        Calculate Yield to Maturity using approximation method
        CFA Standard: YTM calculation for bonds
        """
        if not all([self.coupon_rate, self.maturity_date, self.principal_amount]):
            return None

        price = current_price or self.current_price

        # Simple approximation for YTM
        # YTM ≈ [Annual Coupon + (Face Value - Price) / Years to Maturity] / [(Face Value + Price) / 2]

        maturity = datetime.strptime(self.maturity_date, '%Y-%m-%d')
        years_to_maturity = (maturity - datetime.now()).days / 365.25

        if years_to_maturity <= 0:
            return self.coupon_rate

        face_value = Decimal('100')  # Assuming par value of 100
        annual_coupon = self.coupon_rate * face_value

        numerator = annual_coupon + (face_value - price) / Decimal(str(years_to_maturity))
        denominator = (face_value + price) / Decimal('2')

        ytm = numerator / denominator
        return ytm

    def calculate_duration(self, ytm: Decimal = None) -> Dict[str, Decimal]:
        """
        Calculate Macaulay and Modified Duration
        CFA Standard: Duration as price sensitivity measure
        """
        if not all([self.coupon_rate, self.maturity_date]):
            return {}

        if ytm is None:
            ytm = self.calculate_yield_to_maturity()
            if ytm is None:
                return {}

        maturity = datetime.strptime(self.maturity_date, '%Y-%m-%d')
        years_to_maturity = (maturity - datetime.now()).days / 365.25

        if years_to_maturity <= 0:
            return {}

        # Simplified duration calculation for annual payments
        coupon_rate = self.coupon_rate
        periods = int(years_to_maturity)

        # Macaulay Duration
        pv_weighted_time = Decimal('0')
        total_pv = Decimal('0')

        for t in range(1, periods + 1):
            if t < periods:
                cash_flow = coupon_rate * Decimal('100')  # Coupon payment
            else:
                cash_flow = (coupon_rate * Decimal('100')) + Decimal('100')  # Coupon + Principal

            pv = cash_flow / ((Decimal('1') + ytm) ** t)
            pv_weighted_time += pv * Decimal(str(t))
            total_pv += pv

        macaulay_duration = pv_weighted_time / total_pv if total_pv > 0 else Decimal('0')

        # Modified Duration
        modified_duration = macaulay_duration / (Decimal('1') + ytm)

        return {
            "macaulay_duration": macaulay_duration,
            "modified_duration": modified_duration,
            "years_to_maturity": Decimal(str(years_to_maturity))
        }

    def credit_risk_assessment(self) -> Dict[str, Any]:
        """
        Assess credit risk characteristics
        CFA Standard: Credit analysis framework
        """
        assessment = {
            "credit_profile": {
                "credit_rating": self.credit_rating,
                "seniority": self.seniority,
                "principal_amount": float(self.principal_amount) if self.principal_amount else None,
                "coupon_rate": float(self.coupon_rate) if self.coupon_rate else None,
                "maturity_date": self.maturity_date
            }
        }

        # Credit spread analysis (simplified)
        if self.coupon_rate:
            risk_free_rate = Config.RISK_FREE_RATE
            credit_spread = self.coupon_rate - risk_free_rate
            assessment["credit_spread"] = float(credit_spread)
            assessment["credit_spread_bps"] = float(credit_spread * Constants.BASIS_POINTS)

        # Risk categorization based on seniority
        risk_factors = {
            "senior": {"recovery_rate": 0.80, "risk_weight": 1.0},
            "mezzanine": {"recovery_rate": 0.50, "risk_weight": 1.5},
            "subordinated": {"recovery_rate": 0.20, "risk_weight": 2.0}
        }

        if self.seniority in risk_factors:
            assessment["risk_characteristics"] = risk_factors[self.seniority]

        return assessment

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV based on market price"""
        if self.principal_amount:
            return self.principal_amount * (self.current_price / Decimal('100'))
        return self.current_price

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key private debt metrics"""
        metrics = {}

        # Yield metrics
        current_yield = self.calculate_current_yield()
        ytm = self.calculate_yield_to_maturity()

        metrics['current_yield'] = float(current_yield)
        if ytm:
            metrics['yield_to_maturity'] = float(ytm)

        # Duration metrics
        duration_metrics = self.calculate_duration(ytm)
        for key, value in duration_metrics.items():
            metrics[key] = float(value)

        # Credit metrics
        credit_assessment = self.credit_risk_assessment()
        metrics.update(credit_assessment)

        # Price metrics
        metrics['current_price'] = float(self.current_price)
        metrics['nav'] = float(self.calculate_nav())

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive private debt valuation"""
        return {
            "debt_overview": {
                "asset_class": self.parameters.asset_class.value,
                "instrument_name": self.parameters.name,
                "principal_amount": float(self.principal_amount) if self.principal_amount else None,
                "seniority": self.seniority,
                "credit_rating": self.credit_rating
            },
            "performance_metrics": self.calculate_key_metrics(),
            "risk_assessment": self.credit_risk_assessment()
        }

    def interest_rate_sensitivity(self, rate_change_bps: int) -> Dict[str, Decimal]:
        """
        Calculate price sensitivity to interest rate changes
        CFA Standard: Duration-based price sensitivity
        """
        duration_metrics = self.calculate_duration()

        if 'modified_duration' not in duration_metrics:
            return {"error": "Cannot calculate duration"}

        modified_duration = duration_metrics['modified_duration']
        rate_change = Decimal(str(rate_change_bps)) / Constants.BASIS_POINTS

        # Price change approximation: ΔP/P ≈ -Modified Duration × Δy
        price_change_pct = -modified_duration * rate_change
        new_price = self.current_price * (Decimal('1') + price_change_pct)

        return {
            "rate_change_bps": Decimal(str(rate_change_bps)),
            "price_change_percent": price_change_pct,
            "new_price": new_price,
            "price_change_amount": new_price - self.current_price
        }


class PrivateCapitalPortfolio:
    """
    Portfolio-level analysis for private capital investments
    CFA Standards: Portfolio construction and diversification
    """

    def __init__(self):
        self.pe_investments: List[PrivateEquityAnalyzer] = []
        self.pd_investments: List[PrivateDebtAnalyzer] = []

    def add_pe_investment(self, pe_investment: PrivateEquityAnalyzer) -> None:
        """Add private equity investment to portfolio"""
        self.pe_investments.append(pe_investment)

    def add_pd_investment(self, pd_investment: PrivateDebtAnalyzer) -> None:
        """Add private debt investment to portfolio"""
        self.pd_investments.append(pd_investment)

    def portfolio_summary(self) -> Dict[str, Any]:
        """Generate comprehensive portfolio summary"""
        total_nav = Decimal('0')
        total_commitments = Decimal('0')
        total_called = Decimal('0')
        total_distributed = Decimal('0')

        # PE Portfolio metrics
        pe_navs = []
        pe_irrs = []
        pe_moics = []

        for pe in self.pe_investments:
            nav = pe.calculate_nav()
            total_nav += nav
            pe_navs.append(nav)

            if pe.commitment:
                total_commitments += pe.commitment
            total_called += pe.called_capital
            total_distributed += pe.distributed_capital

            metrics = pe.calculate_key_metrics()
            if metrics.get('irr'):
                pe_irrs.append(Decimal(str(metrics['irr'])))
            if metrics.get('moic'):
                pe_moics.append(Decimal(str(metrics['moic'])))

        # PD Portfolio metrics
        pd_navs = []
        pd_yields = []

        for pd in self.pd_investments:
            nav = pd.calculate_nav()
            total_nav += nav
            pd_navs.append(nav)

            metrics = pd.calculate_key_metrics()
            if metrics.get('yield_to_maturity'):
                pd_yields.append(Decimal(str(metrics['yield_to_maturity'])))

        summary = {
            "portfolio_overview": {
                "total_nav": float(total_nav),
                "total_commitments": float(total_commitments),
                "total_called_capital": float(total_called),
                "total_distributions": float(total_distributed),
                "number_pe_investments": len(self.pe_investments),
                "number_pd_investments": len(self.pd_investments)
            },
            "pe_portfolio": {
                "average_irr": float(sum(pe_irrs) / len(pe_irrs)) if pe_irrs else None,
                "average_moic": float(sum(pe_moics) / len(pe_moics)) if pe_moics else None,
                "total_pe_nav": float(sum(pe_navs))
            },
            "pd_portfolio": {
                "average_yield": float(sum(pd_yields) / len(pd_yields)) if pd_yields else None,
                "total_pd_nav": float(sum(pd_navs))
            }
        }

        # Portfolio allocation
        if total_nav > 0:
            pe_allocation = sum(pe_navs) / total_nav
            pd_allocation = sum(pd_navs) / total_nav
            summary["allocation"] = {
                "pe_weight": float(pe_allocation),
                "pd_weight": float(pd_allocation)
            }

        return summary

    def diversification_analysis(self) -> Dict[str, Any]:
        """Analyze portfolio diversification"""
        analysis = {
            "vintage_year_diversification": {},
            "strategy_diversification": {},
            "geographic_diversification": {}
        }

        # Vintage year analysis for PE
        vintage_years = {}
        for pe in self.pe_investments:
            if pe.vintage_year:
                year = pe.vintage_year
                if year not in vintage_years:
                    vintage_years[year] = []
                vintage_years[year].append(pe.calculate_nav())

        analysis["vintage_year_diversification"] = {
            year: {
                "count": len(investments),
                "total_nav": float(sum(investments))
            } for year, investments in vintage_years.items()
        }

        return analysis


# Export main components
__all__ = ['PrivateEquityAnalyzer', 'PrivateDebtAnalyzer', 'PrivateCapitalPortfolio']