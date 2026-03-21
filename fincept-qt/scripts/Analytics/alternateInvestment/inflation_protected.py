"""inflation_protected Module"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class TIPSAnalyzer(AlternativeInvestmentBase):
    """
    Treasury Inflation-Protected Securities (TIPS) Analyzer

    CFA Standards: Fixed Income, Inflation protection, Real vs Nominal returns

    Key Concepts from Key insight: - Real return bonds vs nominal return bonds
    - Inflation hedge characteristics
    - Correlation with equities and inflation
    - Asset location considerations
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.principal = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('1000')
        self.coupon_rate = parameters.coupon_rate if hasattr(parameters, 'coupon_rate') else Decimal('0.02')
        self.maturity_years = parameters.maturity_years if hasattr(parameters, 'maturity_years') else 10
        self.current_price = parameters.current_market_value if hasattr(parameters, 'current_market_value') else self.principal
        self.inflation_adjustments: List[Dict[str, Any]] = []

    def add_inflation_adjustment(self, date: str, cpi_index: Decimal) -> None:
        """
        Record inflation adjustment for principal

        Args:
            date: Adjustment date
            cpi_index: Consumer Price Index value
        """
        self.inflation_adjustments.append({
            'date': date,
            'cpi_index': cpi_index,
            'timestamp': datetime.now().isoformat()
        })

    def calculate_inflation_adjusted_principal(self, reference_cpi: Decimal, current_cpi: Decimal) -> Decimal:
        """
        Calculate inflation-adjusted principal

        CFA: Principal adjusts with CPI
        Formula: Adjusted Principal = Original Principal × (Current CPI / Reference CPI)

        Args:
            reference_cpi: Reference CPI at issuance
            current_cpi: Current CPI

        Returns:
            Inflation-adjusted principal
        """
        if reference_cpi == 0:
            return self.principal

        adjustment_factor = current_cpi / reference_cpi
        return self.principal * adjustment_factor

    def calculate_real_coupon_payment(self, adjusted_principal: Decimal) -> Decimal:
        """
        Calculate coupon payment on adjusted principal

        CFA: Coupons paid on inflation-adjusted principal
        Formula: Coupon = (Coupon Rate / 2) × Adjusted Principal

        Args:
            adjusted_principal: Inflation-adjusted principal

        Returns:
            Semi-annual coupon payment
        """
        return (self.coupon_rate / Decimal('2')) * adjusted_principal

    def calculate_real_yield(self, current_price: Decimal = None) -> Dict[str, Decimal]:
        """
        Calculate real yield to maturity

        CFA: TIPS provide real (inflation-adjusted) yield

        Args:
            current_price: Current market price (default: par)

        Returns:
            Real yield metrics
        """
        if current_price is None:
            current_price = self.current_price

        # Simplified YTM calculation (assume no inflation for approximation)
        annual_coupon = self.coupon_rate * self.principal

        # Current yield
        current_yield = annual_coupon / current_price

        # Approximate YTM using bond pricing formula
        # YTM ≈ [Annual Coupon + (Face Value - Price) / Years] / [(Face Value + Price) / 2]
        years_to_maturity = Decimal(str(self.maturity_years))
        capital_gain_per_year = (self.principal - current_price) / years_to_maturity
        average_price = (self.principal + current_price) / Decimal('2')

        approximate_ytm = (annual_coupon + capital_gain_per_year) / average_price

        return {
            'current_yield': current_yield,
            'yield_to_maturity': approximate_ytm,
            'coupon_rate': self.coupon_rate,
            'years_to_maturity': years_to_maturity
        }

    def compare_real_vs_nominal(self, nominal_bond_yield: Decimal, expected_inflation: Decimal) -> Dict[str, Any]:
        """
        Compare TIPS (real) vs Nominal bonds

        Insight: TIPS eliminate inflation risk, nominal bonds don't

        Args:
            nominal_bond_yield: Yield on comparable nominal Treasury
            expected_inflation: Expected inflation rate

        Returns:
            Comparison analysis
        """
        tips_yield = self.calculate_real_yield()['yield_to_maturity']

        # Breakeven inflation rate
        breakeven_inflation = nominal_bond_yield - tips_yield

        # Expected real return on nominal bond
        expected_real_return_nominal = nominal_bond_yield - expected_inflation

        # TIPS advantage if inflation exceeds breakeven
        tips_advantage = expected_inflation - breakeven_inflation

        return {
            'tips_real_yield': float(tips_yield),
            'nominal_bond_yield': float(nominal_bond_yield),
            'breakeven_inflation': float(breakeven_inflation),
            'expected_inflation': float(expected_inflation),
            'expected_real_return_nominal': float(expected_real_return_nominal),
            'tips_advantage_if_inflation_as_expected': float(tips_advantage),
            'recommendation': 'TIPS' if tips_advantage > 0 else 'Nominal Bonds',
            'rationale': f"TIPS preferred if inflation > {float(breakeven_inflation):.2%}"
        }

    def calculate_inflation_protection_value(self, inflation_scenarios: List[Decimal]) -> Dict[str, Any]:
        """
        Value TIPS protection across inflation scenarios

        Args:
            inflation_scenarios: List of potential inflation rates

        Returns:
            Protection value analysis
        """
        results = []

        for inflation_rate in inflation_scenarios:
            # Assume reference CPI = 100, calculate adjusted CPI
            years = Decimal(str(self.maturity_years))
            adjusted_cpi = Decimal('100') * ((Decimal('1') + inflation_rate) ** years)

            adjusted_principal = self.calculate_inflation_adjusted_principal(
                Decimal('100'), adjusted_cpi
            )

            total_coupons = Decimal('0')
            for year in range(int(self.maturity_years)):
                year_cpi = Decimal('100') * ((Decimal('1') + inflation_rate) ** Decimal(str(year)))
                year_principal = self.calculate_inflation_adjusted_principal(Decimal('100'), year_cpi)
                annual_coupon = self.coupon_rate * year_principal
                total_coupons += annual_coupon

            total_value = adjusted_principal + total_coupons
            real_return = ((total_value / self.principal) ** (Decimal('1') / years)) - Decimal('1')

            results.append({
                'inflation_rate': float(inflation_rate),
                'adjusted_principal': float(adjusted_principal),
                'total_value': float(total_value),
                'real_return': float(real_return)
            })

        return {
            'scenarios': results,
            'protection_summary': 'TIPS maintain purchasing power across all inflation scenarios'
        }

    def calculate_duration(self, yield_change: Decimal = Decimal('0.01')) -> Dict[str, Decimal]:
        """
        Calculate Macaulay and Modified Duration

        CFA: Duration measures interest rate sensitivity

        Args:
            yield_change: Yield change for modified duration (default 1%)

        Returns:
            Duration metrics
        """
        ytm = self.calculate_real_yield()['yield_to_maturity']

        # Macaulay Duration (simplified for coupon bond)
        # Weighted average time to receive cash flows
        total_pv = Decimal('0')
        weighted_time = Decimal('0')

        for t in range(1, self.maturity_years + 1):
            t_decimal = Decimal(str(t))
            coupon = self.coupon_rate * self.principal
            if t == self.maturity_years:
                cash_flow = coupon + self.principal
            else:
                cash_flow = coupon

            pv = cash_flow / ((Decimal('1') + ytm) ** t_decimal)
            total_pv += pv
            weighted_time += t_decimal * pv

        macaulay_duration = weighted_time / total_pv if total_pv > 0 else Decimal('0')

        # Modified Duration
        modified_duration = macaulay_duration / (Decimal('1') + ytm)

        # Dollar Duration
        dollar_duration = modified_duration * self.current_price

        return {
            'macaulay_duration': macaulay_duration,
            'modified_duration': modified_duration,
            'dollar_duration': dollar_duration,
            'convexity_approx': macaulay_duration * (macaulay_duration + Decimal('1'))  # Approximate
        }

    def tax_efficiency_analysis(self, tax_rate_ordinary: Decimal, tax_rate_capital_gains: Decimal) -> Dict[str, Any]:
        """
        Analyze tax implications of TIPS

        Key insight: TIPS have unique tax treatment
        - Inflation adjustments taxable as ordinary income (phantom income)
        - Best held in tax-deferred accounts

        Args:
            tax_rate_ordinary: Ordinary income tax rate
            tax_rate_capital_gains: Capital gains tax rate

        Returns:
            Tax analysis
        """
        annual_coupon = self.coupon_rate * self.principal

        # Assume 2% inflation
        inflation_rate = Decimal('0.02')
        inflation_adjustment = self.principal * inflation_rate

        # Tax on coupon (ordinary income)
        tax_on_coupon = annual_coupon * tax_rate_ordinary

        # Tax on inflation adjustment (phantom income - paid yearly even though not received)
        tax_on_phantom = inflation_adjustment * tax_rate_ordinary

        # Total annual tax burden
        total_annual_tax = tax_on_coupon + tax_on_phantom

        # After-tax yield
        after_tax_real_yield = self.coupon_rate - (total_annual_tax / self.principal)

        return {
            'annual_coupon': float(annual_coupon),
            'inflation_adjustment': float(inflation_adjustment),
            'tax_on_coupon': float(tax_on_coupon),
            'tax_on_phantom_income': float(tax_on_phantom),
            'total_annual_tax': float(total_annual_tax),
            'pre_tax_real_yield': float(self.coupon_rate),
            'after_tax_real_yield': float(after_tax_real_yield),
            'tax_efficiency_ratio': float(after_tax_real_yield / self.coupon_rate),
            'recommendation': 'Hold in tax-deferred account (IRA, 401k) to avoid phantom income tax'
        }

    def correlation_analysis(self, equity_returns: List[Decimal], inflation_rates: List[Decimal]) -> Dict[str, Any]:
        """
        Analyze TIPS correlation with equities and inflation

        Finding: TIPS have low/negative correlation with equities, positive with inflation

        Args:
            equity_returns: Historical equity returns
            inflation_rates: Historical inflation rates

        Returns:
            Correlation metrics
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient market data'}

        # Calculate TIPS returns from market data
        tips_returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            tips_returns.append(ret)

        # Ensure equal lengths
        min_length = min(len(tips_returns), len(equity_returns), len(inflation_rates))
        tips_returns = tips_returns[:min_length]
        equity_returns = equity_returns[:min_length]
        inflation_rates = inflation_rates[:min_length]

        if min_length < 2:
            return {'error': 'Insufficient data for correlation'}

        # Calculate correlations
        tips_array = np.array([float(r) for r in tips_returns])
        equity_array = np.array([float(r) for r in equity_returns])
        inflation_array = np.array([float(r) for r in inflation_rates])

        corr_tips_equity = np.corrcoef(tips_array, equity_array)[0, 1]
        corr_tips_inflation = np.corrcoef(tips_array, inflation_array)[0, 1]

        return {
            'correlation_tips_equity': float(corr_tips_equity),
            'correlation_tips_inflation': float(corr_tips_inflation),
            'diversification_benefit': 'High' if abs(corr_tips_equity) < 0.3 else 'Moderate',
            'inflation_hedge_quality': 'Excellent' if corr_tips_inflation > 0.5 else 'Good',
            'analysis_benchmark': {
                'expected_equity_correlation': '0.18 (low)',
                'expected_inflation_correlation': 'Positive (good hedge)'
            }
        }

    def asset_location_recommendation(self, investor_tax_bracket: str) -> Dict[str, Any]:
        """
        Recommend optimal account type for TIPS

        Key insight: TIPS best in tax-deferred accounts due to phantom income

        Args:
            investor_tax_bracket: 'low', 'medium', 'high'

        Returns:
            Location recommendation
        """
        recommendations = {
            'low': {
                'priority_1': 'Tax-deferred (IRA, 401k)',
                'priority_2': 'Taxable account (acceptable)',
                'priority_3': 'Tax-exempt (Roth) - better for equities',
                'rationale': 'Phantom income tax is manageable at low rates'
            },
            'medium': {
                'priority_1': 'Tax-deferred (IRA, 401k)',
                'priority_2': 'Tax-exempt (Roth) if space limited',
                'priority_3': 'Avoid taxable account',
                'rationale': 'Phantom income tax significantly reduces returns'
            },
            'high': {
                'priority_1': 'Tax-deferred (IRA, 401k) - ESSENTIAL',
                'priority_2': 'Tax-exempt (Roth) if 401k full',
                'priority_3': 'Never in taxable account',
                'rationale': 'Phantom income tax makes taxable ownership very inefficient'
            }
        }

        return {
            'investor_bracket': investor_tax_bracket,
            'recommendations': recommendations.get(investor_tax_bracket, recommendations['medium']),
            'general_rule': 'TIPS should be first fixed-income assets in tax-deferred accounts, before nominal bonds',
            'exception': 'Municipal bonds can be held in taxable if in high bracket'
        }

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """
        Calculate comprehensive TIPS metrics

        Returns:
            All key metrics
        """
        yield_metrics = self.calculate_real_yield()
        duration_metrics = self.calculate_duration()

        return {
            'security_type': 'TIPS (Treasury Inflation-Protected Security)',
            'principal': float(self.principal),
            'coupon_rate': float(self.coupon_rate),
            'maturity_years': self.maturity_years,
            'current_price': float(self.current_price),
            'yield_metrics': {k: float(v) if isinstance(v, Decimal) else v for k, v in yield_metrics.items()},
            'duration_metrics': {k: float(v) for k, v in duration_metrics.items()},
            'inflation_hedge': 'Primary purpose - protects purchasing power',
            'best_use': 'Core holding in fixed income allocation, especially for retirees',
            'asset_location': 'Tax-deferred accounts preferred (avoid phantom income tax)'
        }

    def calculate_nav(self) -> Decimal:
        """Calculate current NAV"""
        return self.current_price

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive TIPS valuation summary"""
        return {
            "asset_overview": {
                "security_type": "TIPS (Treasury Inflation-Protected Security)",
                "principal": float(self.principal),
                "coupon_rate": float(self.coupon_rate),
                "maturity_years": self.maturity_years,
                "current_price": float(self.current_price)
            },
            "key_metrics": self.calculate_key_metrics(),
            "analysis_category": "THE GOOD",
            "recommendation": "Core holding for inflation protection in fixed income allocation"
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """
        Calculate performance metrics

        Returns:
            Performance analysis
        """
        if not self.market_data or len(self.market_data) < 2:
            return {'error': 'Insufficient data'}

        returns = []
        for i in range(1, len(self.market_data)):
            prev_price = self.market_data[i-1].price
            curr_price = self.market_data[i].price
            ret = (curr_price - prev_price) / prev_price
            returns.append(ret)

        if not returns:
            return {'error': 'No returns calculated'}

        avg_return = sum(returns) / len(returns)
        volatility = self.math.calculate_volatility(returns, annualized=True)
        sharpe = self.math.sharpe_ratio(returns, self.config.RISK_FREE_RATE)

        return {
            'average_return': float(avg_return),
            'volatility': float(volatility),
            'sharpe_ratio': float(sharpe),
            'observation_count': len(returns)
        }





class IBondAnalyzer(AlternativeInvestmentBase):
    """
    Series I Savings Bonds Analyzer

    CFA Standards: Government bonds, Inflation protection

    Key Concepts from Key insight: - Similar to TIPS but with restrictions
    - $5,000 annual purchase limit (paper) + $5,000 (electronic)
    - No secondary market (non-tradable)
    - Tax-deferred until redemption
    - 1-year minimum holding, 3-month penalty if < 5 years
    - Can be exchanged for higher yielding I Bonds

    Verdict: THE GOOD - Excellent for small investors
    Rating: 8/10 - Perfect for emergency funds and small savers
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.face_value = parameters.acquisition_price if hasattr(parameters, 'acquisition_price') else Decimal('1000')
        self.fixed_rate = parameters.fixed_rate if hasattr(parameters, 'fixed_rate') else Decimal('0.0')
        self.inflation_rate = parameters.inflation_rate if hasattr(parameters, 'inflation_rate') else Decimal('0.03')
        self.purchase_date = parameters.purchase_date if hasattr(parameters, 'purchase_date') else datetime.now().isoformat()
        self.years_held = parameters.years_held if hasattr(parameters, 'years_held') else 0

        # I Bond parameters
        self.annual_purchase_limit = Decimal('10000')  # $5K paper + $5K electronic
        self.minimum_holding_period = 1  # 1 year
        self.penalty_period = 5  # 5 years
        self.penalty_months = 3

    def calculate_composite_rate(self) -> Decimal:
        """
        Calculate I Bond composite rate

        Formula: Fixed + (2 Ã— Inflation) + (Fixed Ã— Inflation)
        Floor at 0% for deflation protection
        """
        composite = self.fixed_rate + (Decimal('2') * self.inflation_rate) + (self.fixed_rate * self.inflation_rate)
        return max(Decimal('0'), composite)

    def penalty_analysis(self) -> Dict[str, Any]:
        """Analyze 3-month penalty if redeemed before 5 years"""
        penalty_applies = self.years_held < self.penalty_period

        if not penalty_applies:
            return {
                'penalty_applies': False,
                'penalty_amount': 0.0,
                'note': 'After 5 years - no penalty'
            }

        # 3 months of interest penalty
        rate = self.calculate_composite_rate()
        penalty_rate = rate / Decimal('4')
        current_value = self.face_value * ((Decimal('1') + rate) ** Decimal(str(self.years_held)))
        penalty = current_value * penalty_rate

        return {
            'penalty_applies': True,
            'years_held': self.years_held,
            'years_until_penalty_free': self.penalty_period - self.years_held,
            'penalty_rate': float(penalty_rate),
            'penalty_amount': float(penalty),
            'net_value': float(current_value - penalty),
            'analysis_note': 'Minor penalty - acceptable for emergency funds'
        }

    def compare_to_tips(self, tips_yield: Decimal) -> Dict[str, Any]:
        """Compare I Bonds vs TIPS"""
        i_rate = float(self.calculate_composite_rate())

        return {
            'i_bond_rate': i_rate,
            'tips_yield': float(tips_yield),
            'rate_advantage': 'I Bonds' if i_rate > float(tips_yield) else 'TIPS',
            'i_bond_pros': [
                'No market price risk',
                'Tax-deferred until redemption',
                'Deflation floor',
                '$10K annual limit'
            ],
            'i_bond_cons': [
                'Purchase limits',
                'Non-tradable',
                '1-year minimum hold',
                '3-month penalty < 5 years'
            ],
            'analysis_recommendation': 'Max out I Bonds first for small investors'
        }

    def tax_efficiency_analysis(self) -> Dict[str, Any]:
        """Tax benefits of I Bonds"""
        return {
            'federal_tax': 'Deferred until redemption or maturity',
            'state_local_tax': 'Exempt',
            'education_benefit': 'Tax-free if used for qualified education',
            'vs_tips': 'I Bonds defer all tax; TIPS have annual phantom income',
            'advantage': 'Significant for taxable accounts'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """verdict on I Bonds"""
        return {
            'asset_class': 'Series I Savings Bonds',
            'category': 'THE GOOD',
            'rating': '8/10',
            'best_for': 'Small investors, emergency funds',
            'pros': [
                'Zero credit risk',
                'Deflation protection',
                'Tax-deferred',
                'State tax exempt',
                'Education tax benefit',
                'No market risk'
            ],
            'cons': [
                '$10K annual limit',
                'Non-tradable',
                '1-year minimum hold',
                '3-month penalty < 5 years',
                'Taxable account only'
            ],
            'analysis_quote': '"I Bonds are one of the best deals for individual investors. Max out the $10K annual limit before considering alternatives."',
            'recommendation': 'Buy maximum $10K annually for all individual investors'
        }

    def calculate_nav(self) -> Decimal:
        """Current accrued value"""
        rate = self.calculate_composite_rate()
        return self.face_value * ((Decimal('1') + rate) ** Decimal(str(self.years_held)))

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Key I Bond metrics"""
        return {
            'composite_rate': float(self.calculate_composite_rate()),
            'fixed_component': float(self.fixed_rate),
            'inflation_component': float(self.inflation_rate),
            'current_value': float(self.calculate_nav()),
            'annual_limit': float(self.annual_purchase_limit),
            'penalty_status': self.penalty_analysis(),
            'tax_treatment': 'Deferred',
            'analysis_category': 'THE GOOD'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Valuation summary"""
        return {
            'asset_overview': {
                'type': 'Series I Savings Bond',
                'face_value': float(self.face_value),
                'years_held': self.years_held
            },
            'key_metrics': self.calculate_key_metrics(),
            'analysis_category': 'THE GOOD',
            'recommendation': 'Max $10K annual purchase'
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Performance metrics"""
        return {
            'current_return': float(self.calculate_composite_rate()),
            'deflation_protection': True,
            'tax_efficiency': 'High',
            'note': 'Held at face value - no volatility'
        }


# Export
__all__ = ['TIPSAnalyzer', 'IBondAnalyzer']
