"""private_capital Module"""

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

    def lbo_returns_decomposition(self, entry_ebitda: Decimal, exit_ebitda: Decimal,
                                   entry_multiple: Decimal, exit_multiple: Decimal,
                                   initial_debt: Decimal, final_debt: Decimal,
                                   equity_invested: Decimal, holding_period_years: int) -> Dict[str, Any]:
        """
        Decompose LBO returns into component drivers (3-Lever Model)

        CFA Standards: LBO returns come from:
        1. EBITDA Growth (operational improvement)
        2. Multiple Expansion (entry vs exit valuation)
        3. Deleveraging (debt paydown increases equity value)

        Args:
            entry_ebitda: EBITDA at acquisition
            exit_ebitda: EBITDA at exit
            entry_multiple: Entry EV/EBITDA multiple
            exit_multiple: Exit EV/EBITDA multiple
            initial_debt: Debt at entry
            final_debt: Debt at exit
            equity_invested: Initial equity investment
            holding_period_years: Investment holding period

        Returns:
            LBO return decomposition
        """
        # Entry valuation
        entry_ev = entry_ebitda * entry_multiple
        entry_equity_value = entry_ev - initial_debt

        # Exit valuation
        exit_ev = exit_ebitda * exit_multiple
        exit_equity_value = exit_ev - final_debt

        # Return components

        # 1. EBITDA Growth impact (operational improvement)
        ebitda_growth = (exit_ebitda - entry_ebitda) / entry_ebitda
        ebitda_contribution_ev = (exit_ebitda - entry_ebitda) * entry_multiple

        # 2. Multiple Expansion impact (valuation arbitrage)
        multiple_expansion = (exit_multiple - entry_multiple) / entry_multiple
        multiple_contribution_ev = (exit_multiple - entry_multiple) * exit_ebitda

        # 3. Deleveraging impact (debt paydown)
        debt_paydown = initial_debt - final_debt
        deleveraging_contribution = debt_paydown

        # Total equity value creation
        equity_value_created = exit_equity_value - entry_equity_value

        # MOIC and IRR
        moic = exit_equity_value / equity_invested if equity_invested > 0 else Decimal('0')

        # IRR approximation: (Exit Value / Entry Value) ^ (1/years) - 1
        irr = (moic ** (Decimal('1') / Decimal(str(holding_period_years)))) - Decimal('1')

        # Attribution of value creation
        total_ev_change = exit_ev - entry_ev

        attribution = {}
        if total_ev_change != 0:
            attribution = {
                'ebitda_growth_contribution_pct': float(ebitda_contribution_ev / total_ev_change) if total_ev_change != 0 else 0,
                'multiple_expansion_contribution_pct': float(multiple_contribution_ev / total_ev_change) if total_ev_change != 0 else 0,
                'deleveraging_contribution_pct': float(deleveraging_contribution / equity_value_created) if equity_value_created != 0 else 0
            }

        return {
            'entry_metrics': {
                'ebitda': float(entry_ebitda),
                'ev_ebitda_multiple': float(entry_multiple),
                'enterprise_value': float(entry_ev),
                'debt': float(initial_debt),
                'equity_value': float(entry_equity_value),
                'leverage_ratio': float(initial_debt / entry_ebitda)
            },
            'exit_metrics': {
                'ebitda': float(exit_ebitda),
                'ev_ebitda_multiple': float(exit_multiple),
                'enterprise_value': float(exit_ev),
                'debt': float(final_debt),
                'equity_value': float(exit_equity_value),
                'leverage_ratio': float(final_debt / exit_ebitda) if exit_ebitda > 0 else 0
            },
            'return_components': {
                'ebitda_growth': float(ebitda_growth),
                'ebitda_contribution_value': float(ebitda_contribution_ev),
                'multiple_expansion': float(multiple_expansion),
                'multiple_contribution_value': float(multiple_contribution_ev),
                'debt_paydown': float(debt_paydown),
                'deleveraging_contribution': float(deleveraging_contribution)
            },
            'attribution': attribution,
            'returns': {
                'equity_invested': float(equity_invested),
                'exit_equity_value': float(exit_equity_value),
                'equity_value_created': float(equity_value_created),
                'moic': float(moic),
                'irr': float(irr),
                'holding_period_years': holding_period_years
            },
            'interpretation': self._interpret_lbo_drivers(ebitda_growth, multiple_expansion, debt_paydown, equity_value_created)
        }

    def _interpret_lbo_drivers(self, ebitda_growth: Decimal, multiple_expansion: Decimal,
                               debt_paydown: Decimal, equity_value: Decimal) -> str:
        """Interpret LBO return drivers"""
        drivers = []

        if ebitda_growth > Decimal('0.30'):
            drivers.append('Strong operational improvement')
        elif ebitda_growth > Decimal('0.10'):
            drivers.append('Moderate operational growth')
        else:
            drivers.append('Limited operational improvement')

        if multiple_expansion > Decimal('0.20'):
            drivers.append('significant multiple expansion')
        elif multiple_expansion > 0:
            drivers.append('modest multiple expansion')
        elif multiple_expansion < Decimal('-0.10'):
            drivers.append('multiple compression (headwind)')

        leverage_contribution_pct = (debt_paydown / equity_value) if equity_value > 0 else Decimal('0')
        if leverage_contribution_pct > Decimal('0.40'):
            drivers.append('substantial deleveraging')
        elif leverage_contribution_pct > Decimal('0.20'):
            drivers.append('meaningful debt paydown')

        return f"Returns driven by: {', '.join(drivers)}"

    def lbo_transaction_model(self, purchase_price: Decimal, ebitda: Decimal,
                              debt_percent: Decimal, interest_rate: Decimal,
                              exit_multiple: Decimal, years: int,
                              ebitda_growth_rate: Decimal = Decimal('0.05'),
                              annual_debt_paydown_pct: Decimal = Decimal('0.30')) -> Dict[str, Any]:
        """
        Full LBO transaction model with year-by-year projection

        CFA: Complete LBO financial model showing:
        - Sources & Uses
        - Cash flow projections
        - Debt schedule
        - Exit scenarios
        - Return calculations

        Args:
            purchase_price: Acquisition price (Enterprise Value)
            ebitda: Current EBITDA
            debt_percent: Debt as % of purchase price (e.g., 0.60 = 60% debt)
            interest_rate: Interest rate on debt
            exit_multiple: Exit EV/EBITDA multiple
            years: Holding period
            ebitda_growth_rate: Annual EBITDA growth rate
            annual_debt_paydown_pct: % of FCF used for debt paydown

        Returns:
            Complete LBO model output
        """
        # Sources & Uses
        debt = purchase_price * debt_percent
        equity = purchase_price * (Decimal('1') - debt_percent)

        sources = {
            'debt': float(debt),
            'equity': float(equity),
            'total': float(purchase_price)
        }

        uses = {
            'purchase_price': float(purchase_price),
            'transaction_fees': float(purchase_price * Decimal('0.02')),  # 2% fees
            'total': float(purchase_price * Decimal('1.02'))
        }

        # Adjusted equity for fees
        equity_with_fees = equity + (purchase_price * Decimal('0.02'))

        # Year-by-year projections
        projections = []
        current_debt = debt
        current_ebitda = ebitda

        for year in range(1, years + 1):
            # EBITDA growth
            current_ebitda = current_ebitda * (Decimal('1') + ebitda_growth_rate)

            # Interest expense
            interest_expense = current_debt * interest_rate

            # Free Cash Flow (simplified: EBITDA - CapEx - Interest)
            # Assume CapEx = depreciation (maintenance capex)
            capex = current_ebitda * Decimal('0.05')  # 5% of EBITDA
            fcf = current_ebitda - capex - interest_expense

            # Debt paydown
            debt_paydown = fcf * annual_debt_paydown_pct
            current_debt = max(Decimal('0'), current_debt - debt_paydown)

            projections.append({
                'year': year,
                'ebitda': float(current_ebitda),
                'interest_expense': float(interest_expense),
                'capex': float(capex),
                'free_cash_flow': float(fcf),
                'debt_paydown': float(debt_paydown),
                'ending_debt': float(current_debt),
                'leverage_ratio': float(current_debt / current_ebitda)
            })

        # Exit valuation
        exit_ebitda = current_ebitda
        exit_ev = exit_ebitda * exit_multiple
        exit_debt = current_debt
        exit_equity_value = exit_ev - exit_debt

        # Returns
        moic = exit_equity_value / equity_with_fees
        irr = (moic ** (Decimal('1') / Decimal(str(years)))) - Decimal('1')

        # Decompose returns
        entry_multiple = purchase_price / ebitda
        decomposition = self.lbo_returns_decomposition(
            entry_ebitda=ebitda,
            exit_ebitda=exit_ebitda,
            entry_multiple=entry_multiple,
            exit_multiple=exit_multiple,
            initial_debt=debt,
            final_debt=exit_debt,
            equity_invested=equity_with_fees,
            holding_period_years=years
        )

        return {
            'transaction_summary': {
                'purchase_price': float(purchase_price),
                'entry_ebitda': float(ebitda),
                'entry_multiple': float(entry_multiple),
                'sources_and_uses': {
                    'sources': sources,
                    'uses': uses
                },
                'initial_leverage': float(debt / ebitda)
            },
            'projections': projections,
            'exit_scenario': {
                'exit_year': years,
                'exit_ebitda': float(exit_ebitda),
                'exit_multiple': float(exit_multiple),
                'exit_enterprise_value': float(exit_ev),
                'exit_debt': float(exit_debt),
                'exit_equity_value': float(exit_equity_value),
                'exit_leverage': float(exit_debt / exit_ebitda) if exit_ebitda > 0 else 0
            },
            'returns': {
                'equity_invested': float(equity_with_fees),
                'equity_at_exit': float(exit_equity_value),
                'moic': float(moic),
                'irr': float(irr)
            },
            'return_decomposition': decomposition
        }

    def lbo_sensitivity_analysis(self, base_params: Dict[str, Decimal],
                                 variable: str, range_pct: Decimal = Decimal('0.20')) -> Dict[str, Any]:
        """
        Sensitivity analysis for LBO returns

        Tests impact of changing key variables:
        - Exit multiple
        - EBITDA growth
        - Interest rates
        - Leverage

        Args:
            base_params: Dictionary with base case parameters
            variable: Variable to test ('exit_multiple', 'ebitda_growth', 'interest_rate', 'leverage')
            range_pct: +/- range to test (e.g., 0.20 = +/- 20%)

        Returns:
            Sensitivity analysis results
        """
        scenarios = []
        base_value = base_params.get(variable, Decimal('0'))

        # Create range of values
        test_values = [
            base_value * (Decimal('1') - range_pct),
            base_value * (Decimal('1') - range_pct / Decimal('2')),
            base_value,
            base_value * (Decimal('1') + range_pct / Decimal('2')),
            base_value * (Decimal('1') + range_pct)
        ]

        for test_value in test_values:
            # Update params with test value
            test_params = base_params.copy()
            test_params[variable] = test_value

            # Run LBO model
            model = self.lbo_transaction_model(
                purchase_price=test_params.get('purchase_price', Decimal('1000')),
                ebitda=test_params.get('ebitda', Decimal('100')),
                debt_percent=test_params.get('debt_percent', Decimal('0.60')),
                interest_rate=test_params.get('interest_rate', Decimal('0.06')),
                exit_multiple=test_params.get('exit_multiple', Decimal('10')),
                years=int(test_params.get('years', 5)),
                ebitda_growth_rate=test_params.get('ebitda_growth_rate', Decimal('0.05'))
            )

            scenarios.append({
                f'{variable}': float(test_value),
                'moic': model['returns']['moic'],
                'irr': model['returns']['irr']
            })

        return {
            'variable_tested': variable,
            'base_value': float(base_value),
            'range_pct': float(range_pct),
            'scenarios': scenarios,
            'sensitivity_interpretation': self._interpret_sensitivity(scenarios, variable)
        }

    def _interpret_sensitivity(self, scenarios: List[Dict], variable: str) -> str:
        """Interpret sensitivity analysis results"""
        moics = [s['moic'] for s in scenarios]
        moic_range = max(moics) - min(moics)

        if moic_range > 2.0:
            sensitivity = 'Very High'
        elif moic_range > 1.0:
            sensitivity = 'High'
        elif moic_range > 0.5:
            sensitivity = 'Moderate'
        else:
            sensitivity = 'Low'

        return f"{sensitivity} sensitivity to {variable} - MOIC range: {min(moics):.2f}x to {max(moics):.2f}x"


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
