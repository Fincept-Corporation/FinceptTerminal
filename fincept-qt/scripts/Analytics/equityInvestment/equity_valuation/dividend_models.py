"""
Equity Investment Dividend Models Module
========================================

Dividend discount models and income-based valuation following CFA curriculum.

Covers:
- Gordon (Constant) Growth Model
- Two-Stage Dividend Discount Model
- Multi-Stage (H-Model) Dividend Discount Model
- Preferred Stock Valuation
- Dividend Payment Chronology
- Free Cash Flow to Equity (FCFE) Model

===== DATA SOURCES REQUIRED =====
INPUT:
  - Current dividend per share (D0 or D1)
  - Expected dividend growth rates
  - Required rate of return / cost of equity
  - Preferred stock par value and dividend rate
  - Free cash flow projections

OUTPUT:
  - Intrinsic value estimates
  - Implied growth rates
  - Implied required returns
  - Valuation sensitivity analysis

PARAMETERS:
  - dividend: Current or expected dividend
  - growth_rate: Expected dividend growth rate
  - required_return: Required rate of return (cost of equity)
  - stages: Number of growth stages for multi-stage models
"""

import numpy as np
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
from datetime import datetime, timedelta
import json
import sys


class DividendModelType(Enum):
    """Types of dividend discount models"""
    ZERO_GROWTH = "zero_growth"
    GORDON_GROWTH = "gordon_growth"
    TWO_STAGE = "two_stage"
    THREE_STAGE = "three_stage"
    H_MODEL = "h_model"


class DividendEventType(Enum):
    """Types of dividend-related corporate actions"""
    REGULAR_CASH = "regular_cash"
    EXTRA_DIVIDEND = "extra_dividend"
    SPECIAL_DIVIDEND = "special_dividend"
    STOCK_DIVIDEND = "stock_dividend"
    STOCK_SPLIT = "stock_split"
    REVERSE_SPLIT = "reverse_split"
    SHARE_REPURCHASE = "share_repurchase"


@dataclass
class DividendDate:
    """Dividend payment chronology dates"""
    declaration_date: datetime
    ex_dividend_date: datetime
    record_date: datetime
    payment_date: datetime


@dataclass
class ValuationResult:
    """Result of dividend model valuation"""
    intrinsic_value: float
    current_price: Optional[float]
    upside_potential: Optional[float]
    model_type: str
    assumptions: Dict[str, Any]
    sensitivity: Optional[Dict[str, List[float]]]


class GordonGrowthModel:
    """
    Gordon (Constant) Growth Dividend Discount Model

    V0 = D1 / (r - g)

    Where:
    - V0 = Intrinsic value today
    - D1 = Expected dividend next period
    - r = Required rate of return
    - g = Constant growth rate (must be < r)

    Assumptions:
    1. Dividends grow at constant rate forever
    2. Growth rate < Required return
    3. Company pays dividends
    """

    def __init__(self):
        self.model_type = DividendModelType.GORDON_GROWTH

    def calculate_intrinsic_value(
        self,
        dividend: float,
        growth_rate: float,
        required_return: float,
        is_d0: bool = True
    ) -> ValuationResult:
        """
        Calculate intrinsic value using Gordon Growth Model.

        Args:
            dividend: Current dividend (D0) or next dividend (D1)
            growth_rate: Constant dividend growth rate
            required_return: Required rate of return
            is_d0: True if dividend is D0, False if D1

        Returns:
            ValuationResult with intrinsic value
        """
        if growth_rate >= required_return:
            raise ValueError(
                f"Growth rate ({growth_rate:.2%}) must be less than "
                f"required return ({required_return:.2%})"
            )

        if required_return <= 0:
            raise ValueError("Required return must be positive")

        # Calculate D1 if D0 provided
        d1 = dividend * (1 + growth_rate) if is_d0 else dividend

        # Gordon Growth formula
        intrinsic_value = d1 / (required_return - growth_rate)

        return ValuationResult(
            intrinsic_value=intrinsic_value,
            current_price=None,
            upside_potential=None,
            model_type="Gordon Growth Model",
            assumptions={
                "d0" if is_d0 else "d1": dividend,
                "d1": d1,
                "growth_rate": growth_rate,
                "required_return": required_return,
                "dividend_yield": d1 / intrinsic_value,
                "capital_gains_yield": growth_rate
            },
            sensitivity=None
        )

    def calculate_implied_growth_rate(
        self,
        price: float,
        dividend: float,
        required_return: float,
        is_d0: bool = True
    ) -> float:
        """
        Calculate implied growth rate given market price.

        g = r - (D1 / P0)
        """
        d1 = dividend * (1 + 0.0) if is_d0 else dividend  # Placeholder

        # Solve for g: P = D1/(r-g) => g = r - D1/P
        # But D1 = D0(1+g), so need to solve iteratively or use approximation

        # Using the relationship: g = r - dividend_yield
        if is_d0:
            # For D0: P = D0(1+g)/(r-g)
            # This requires solving quadratic, use approximation
            dividend_yield_approx = dividend / price
            implied_g = required_return - dividend_yield_approx
        else:
            dividend_yield = dividend / price
            implied_g = required_return - dividend_yield

        return implied_g

    def calculate_implied_required_return(
        self,
        price: float,
        dividend: float,
        growth_rate: float,
        is_d0: bool = True
    ) -> float:
        """
        Calculate implied required return given market price.

        r = (D1 / P0) + g
        """
        d1 = dividend * (1 + growth_rate) if is_d0 else dividend
        dividend_yield = d1 / price

        return dividend_yield + growth_rate

    def sensitivity_analysis(
        self,
        dividend: float,
        base_growth: float,
        base_required_return: float,
        growth_range: Tuple[float, float] = (-0.02, 0.02),
        return_range: Tuple[float, float] = (-0.02, 0.02),
        steps: int = 5
    ) -> Dict[str, Any]:
        """
        Perform sensitivity analysis on key inputs.

        Returns:
            Grid of intrinsic values for different growth/return combinations
        """
        growth_rates = np.linspace(
            base_growth + growth_range[0],
            base_growth + growth_range[1],
            steps
        )

        required_returns = np.linspace(
            base_required_return + return_range[0],
            base_required_return + return_range[1],
            steps
        )

        results = []
        for g in growth_rates:
            row = []
            for r in required_returns:
                if g < r:
                    try:
                        result = self.calculate_intrinsic_value(
                            dividend, g, r, is_d0=True
                        )
                        row.append(round(result.intrinsic_value, 2))
                    except:
                        row.append(None)
                else:
                    row.append(None)
            results.append(row)

        return {
            "growth_rates": [round(g, 4) for g in growth_rates],
            "required_returns": [round(r, 4) for r in required_returns],
            "intrinsic_values": results,
            "base_case": {
                "growth_rate": base_growth,
                "required_return": base_required_return
            }
        }

    def appropriate_for_company(self, characteristics: Dict[str, Any]) -> Dict[str, Any]:
        """
        Evaluate if Gordon Growth Model is appropriate for a company.

        Appropriate when:
        - Company pays dividends
        - Dividends grow at roughly constant rate
        - Mature, stable business
        - Growth rate sustainable long-term
        """
        is_appropriate = True
        reasons = []
        warnings = []

        # Check dividend history
        if not characteristics.get("pays_dividends", False):
            is_appropriate = False
            reasons.append("Company does not pay dividends")

        # Check dividend stability
        dividend_volatility = characteristics.get("dividend_volatility", 0)
        if dividend_volatility > 0.20:
            is_appropriate = False
            reasons.append(f"High dividend volatility ({dividend_volatility:.1%})")

        # Check growth stability
        if characteristics.get("high_growth", False):
            is_appropriate = False
            reasons.append("Company is in high-growth phase")
            warnings.append("Consider two-stage or H-model instead")

        # Check maturity
        if characteristics.get("years_paying_dividends", 0) < 5:
            warnings.append("Short dividend history - forecast uncertainty")

        # Check payout ratio
        payout_ratio = characteristics.get("payout_ratio", 0)
        if payout_ratio > 0.90:
            warnings.append("Very high payout ratio - growth sustainability concern")
        elif payout_ratio < 0.20:
            warnings.append("Low payout ratio - dividends may not reflect earnings")

        return {
            "is_appropriate": is_appropriate,
            "reasons": reasons,
            "warnings": warnings,
            "recommended_model": "Gordon Growth" if is_appropriate else "Two-Stage DDM"
        }


class TwoStageDDM:
    """
    Two-Stage Dividend Discount Model

    Stage 1: High growth period (years 1 to n)
    Stage 2: Stable growth forever (Gordon Growth)

    V0 = Σ[D0(1+g1)^t / (1+r)^t] + [Dn(1+g2) / (r-g2)] / (1+r)^n

    Where:
    - g1 = High growth rate (Stage 1)
    - g2 = Stable growth rate (Stage 2)
    - n = Length of high growth period
    """

    def __init__(self):
        self.model_type = DividendModelType.TWO_STAGE

    def calculate_intrinsic_value(
        self,
        d0: float,
        high_growth_rate: float,
        stable_growth_rate: float,
        required_return: float,
        high_growth_years: int
    ) -> ValuationResult:
        """
        Calculate intrinsic value using Two-Stage DDM.

        Args:
            d0: Current dividend
            high_growth_rate: Growth rate during Stage 1
            stable_growth_rate: Perpetual growth rate in Stage 2
            required_return: Required rate of return
            high_growth_years: Number of years in high-growth stage

        Returns:
            ValuationResult with intrinsic value
        """
        if stable_growth_rate >= required_return:
            raise ValueError(
                f"Stable growth rate ({stable_growth_rate:.2%}) must be less than "
                f"required return ({required_return:.2%})"
            )

        # Stage 1: Present value of dividends during high-growth period
        stage1_pv = 0
        dividends = []

        for t in range(1, high_growth_years + 1):
            dt = d0 * (1 + high_growth_rate) ** t
            pv_dt = dt / (1 + required_return) ** t
            stage1_pv += pv_dt
            dividends.append({"year": t, "dividend": dt, "pv": pv_dt})

        # Dividend at end of high-growth period
        dn = d0 * (1 + high_growth_rate) ** high_growth_years

        # Stage 2: Terminal value using Gordon Growth
        d_n_plus_1 = dn * (1 + stable_growth_rate)
        terminal_value = d_n_plus_1 / (required_return - stable_growth_rate)

        # Present value of terminal value
        pv_terminal = terminal_value / (1 + required_return) ** high_growth_years

        # Total intrinsic value
        intrinsic_value = stage1_pv + pv_terminal

        return ValuationResult(
            intrinsic_value=intrinsic_value,
            current_price=None,
            upside_potential=None,
            model_type="Two-Stage DDM",
            assumptions={
                "d0": d0,
                "high_growth_rate": high_growth_rate,
                "stable_growth_rate": stable_growth_rate,
                "required_return": required_return,
                "high_growth_years": high_growth_years,
                "stage1_pv": stage1_pv,
                "terminal_value": terminal_value,
                "pv_terminal_value": pv_terminal,
                "terminal_value_percentage": pv_terminal / intrinsic_value
            },
            sensitivity=None
        )

    def calculate_with_declining_growth(
        self,
        d0: float,
        initial_growth: float,
        terminal_growth: float,
        required_return: float,
        transition_years: int
    ) -> ValuationResult:
        """
        Two-stage model with linearly declining growth rate.

        Growth rate declines from initial to terminal over transition period.
        """
        if terminal_growth >= required_return:
            raise ValueError("Terminal growth must be less than required return")

        # Calculate annual growth rates (linear decline)
        growth_decline_per_year = (initial_growth - terminal_growth) / transition_years

        stage1_pv = 0
        current_dividend = d0

        for t in range(1, transition_years + 1):
            growth_t = initial_growth - (t - 1) * growth_decline_per_year
            current_dividend = current_dividend * (1 + growth_t)
            pv_dt = current_dividend / (1 + required_return) ** t
            stage1_pv += pv_dt

        # Terminal value
        d_terminal = current_dividend * (1 + terminal_growth)
        terminal_value = d_terminal / (required_return - terminal_growth)
        pv_terminal = terminal_value / (1 + required_return) ** transition_years

        intrinsic_value = stage1_pv + pv_terminal

        return ValuationResult(
            intrinsic_value=intrinsic_value,
            current_price=None,
            upside_potential=None,
            model_type="Two-Stage DDM (Declining Growth)",
            assumptions={
                "d0": d0,
                "initial_growth": initial_growth,
                "terminal_growth": terminal_growth,
                "required_return": required_return,
                "transition_years": transition_years,
                "stage1_pv": stage1_pv,
                "pv_terminal_value": pv_terminal
            },
            sensitivity=None
        )


class HModelDDM:
    """
    H-Model for Dividend Discount Valuation

    Assumes growth rate declines linearly from high initial rate to
    long-term sustainable rate over period H.

    V0 = D0(1+gL) / (r-gL) + D0 * H * (gS-gL) / (r-gL)

    Where:
    - gS = Short-term (high) growth rate
    - gL = Long-term (stable) growth rate
    - H = Half-life of high-growth period (years/2)
    """

    def __init__(self):
        self.model_type = DividendModelType.H_MODEL

    def calculate_intrinsic_value(
        self,
        d0: float,
        short_term_growth: float,
        long_term_growth: float,
        required_return: float,
        high_growth_period: int
    ) -> ValuationResult:
        """
        Calculate intrinsic value using H-Model.

        Args:
            d0: Current dividend
            short_term_growth: Initial high growth rate
            long_term_growth: Long-term sustainable growth rate
            required_return: Required rate of return
            high_growth_period: Full period of declining growth (H = period/2)

        Returns:
            ValuationResult with intrinsic value
        """
        if long_term_growth >= required_return:
            raise ValueError("Long-term growth must be less than required return")

        H = high_growth_period / 2  # Half-life

        # H-Model formula
        # Value from stable growth
        stable_value = d0 * (1 + long_term_growth) / (required_return - long_term_growth)

        # Additional value from above-normal growth
        growth_premium = d0 * H * (short_term_growth - long_term_growth) / (
            required_return - long_term_growth
        )

        intrinsic_value = stable_value + growth_premium

        return ValuationResult(
            intrinsic_value=intrinsic_value,
            current_price=None,
            upside_potential=None,
            model_type="H-Model DDM",
            assumptions={
                "d0": d0,
                "short_term_growth": short_term_growth,
                "long_term_growth": long_term_growth,
                "required_return": required_return,
                "H": H,
                "high_growth_period": high_growth_period,
                "stable_value_component": stable_value,
                "growth_premium_component": growth_premium
            },
            sensitivity=None
        )


class ThreeStageDDM:
    """
    Three-Stage Dividend Discount Model

    Stage 1: High growth (constant rate g1)
    Stage 2: Transition (declining growth from g1 to g3)
    Stage 3: Mature/Stable growth (constant rate g3)
    """

    def __init__(self):
        self.model_type = DividendModelType.THREE_STAGE

    def calculate_intrinsic_value(
        self,
        d0: float,
        growth_stage1: float,
        growth_stage3: float,
        required_return: float,
        years_stage1: int,
        years_stage2: int
    ) -> ValuationResult:
        """
        Calculate intrinsic value using Three-Stage DDM.

        Args:
            d0: Current dividend
            growth_stage1: High growth rate (Stage 1)
            growth_stage3: Stable growth rate (Stage 3)
            required_return: Required rate of return
            years_stage1: Length of high-growth stage
            years_stage2: Length of transition stage

        Returns:
            ValuationResult with intrinsic value
        """
        if growth_stage3 >= required_return:
            raise ValueError("Stable growth must be less than required return")

        total_pv = 0
        current_dividend = d0
        year = 0

        # Stage 1: High growth period
        stage1_pv = 0
        for t in range(1, years_stage1 + 1):
            year += 1
            current_dividend = current_dividend * (1 + growth_stage1)
            pv = current_dividend / (1 + required_return) ** year
            stage1_pv += pv

        total_pv += stage1_pv

        # Stage 2: Transition period (linear decline)
        stage2_pv = 0
        growth_decline = (growth_stage1 - growth_stage3) / years_stage2

        for t in range(1, years_stage2 + 1):
            year += 1
            growth_t = growth_stage1 - t * growth_decline
            current_dividend = current_dividend * (1 + growth_t)
            pv = current_dividend / (1 + required_return) ** year
            stage2_pv += pv

        total_pv += stage2_pv

        # Stage 3: Terminal value
        d_terminal = current_dividend * (1 + growth_stage3)
        terminal_value = d_terminal / (required_return - growth_stage3)
        pv_terminal = terminal_value / (1 + required_return) ** year

        total_pv += pv_terminal

        return ValuationResult(
            intrinsic_value=total_pv,
            current_price=None,
            upside_potential=None,
            model_type="Three-Stage DDM",
            assumptions={
                "d0": d0,
                "growth_stage1": growth_stage1,
                "growth_stage3": growth_stage3,
                "required_return": required_return,
                "years_stage1": years_stage1,
                "years_stage2": years_stage2,
                "stage1_pv": stage1_pv,
                "stage2_pv": stage2_pv,
                "pv_terminal": pv_terminal,
                "terminal_value_percentage": pv_terminal / total_pv
            },
            sensitivity=None
        )


class PreferredStockValuation:
    """
    Preferred Stock Valuation Models

    Non-callable, non-convertible preferred stock valuation:
    V = D / r (perpetuity formula)

    Where:
    - D = Annual preferred dividend
    - r = Required rate of return
    """

    def calculate_value(
        self,
        par_value: float,
        dividend_rate: float,
        required_return: float
    ) -> Dict[str, Any]:
        """
        Calculate intrinsic value of non-callable, non-convertible preferred stock.

        Args:
            par_value: Par value of preferred stock
            dividend_rate: Annual dividend rate (as decimal)
            required_return: Required rate of return

        Returns:
            Valuation result
        """
        if required_return <= 0:
            raise ValueError("Required return must be positive")

        annual_dividend = par_value * dividend_rate
        intrinsic_value = annual_dividend / required_return

        return {
            "intrinsic_value": intrinsic_value,
            "par_value": par_value,
            "dividend_rate": dividend_rate,
            "annual_dividend": annual_dividend,
            "required_return": required_return,
            "current_yield": dividend_rate,
            "model": "Perpetuity (Non-callable, Non-convertible)"
        }

    def calculate_yield(
        self,
        market_price: float,
        par_value: float,
        dividend_rate: float
    ) -> Dict[str, float]:
        """
        Calculate yield measures for preferred stock.
        """
        annual_dividend = par_value * dividend_rate
        current_yield = annual_dividend / market_price

        return {
            "current_yield": current_yield,
            "nominal_yield": dividend_rate,
            "annual_dividend": annual_dividend,
            "market_price": market_price
        }

    def calculate_required_return(
        self,
        market_price: float,
        par_value: float,
        dividend_rate: float
    ) -> float:
        """
        Calculate implied required return from market price.

        r = D / P
        """
        annual_dividend = par_value * dividend_rate
        return annual_dividend / market_price

    def compare_preferred_types(self) -> Dict[str, Any]:
        """
        Compare different types of preferred stock.
        """
        return {
            "cumulative": {
                "description": "Missed dividends accumulate and must be paid before common dividends",
                "risk": "Lower",
                "typical_yield": "Lower than non-cumulative"
            },
            "non_cumulative": {
                "description": "Missed dividends are lost forever",
                "risk": "Higher",
                "typical_yield": "Higher than cumulative"
            },
            "participating": {
                "description": "Participates in additional dividends beyond stated rate",
                "risk": "Depends on terms",
                "typical_yield": "Lower due to upside potential"
            },
            "convertible": {
                "description": "Can be converted to common stock",
                "risk": "Equity-like if converted",
                "typical_yield": "Lower due to conversion option"
            },
            "callable": {
                "description": "Issuer can redeem at specified price",
                "risk": "Reinvestment risk",
                "typical_yield": "Higher to compensate for call risk"
            }
        }


class DividendChronology:
    """
    Dividend Payment Chronology Analysis

    Key dates:
    1. Declaration Date: Board announces dividend
    2. Ex-Dividend Date: First day stock trades without dividend
    3. Record Date: Date to determine shareholders of record
    4. Payment Date: Dividend is actually paid
    """

    def explain_chronology(self) -> Dict[str, str]:
        """Explain dividend payment chronology."""
        return {
            "declaration_date": {
                "description": "Date board of directors announces the dividend",
                "significance": "Creates legal liability for company",
                "timing": "Typically 2-3 weeks before ex-date"
            },
            "ex_dividend_date": {
                "description": "First day stock trades without right to dividend",
                "significance": "Must buy BEFORE this date to receive dividend",
                "timing": "Usually 1 business day before record date",
                "price_impact": "Stock typically drops by dividend amount"
            },
            "record_date": {
                "description": "Date to determine shareholders who receive dividend",
                "significance": "Must be shareholder of record on this date",
                "timing": "1 business day after ex-date"
            },
            "payment_date": {
                "description": "Date dividend is actually paid to shareholders",
                "significance": "Cash or shares are distributed",
                "timing": "Usually 2-4 weeks after record date"
            }
        }

    def calculate_dates(
        self,
        declaration_date: datetime,
        record_date: datetime = None,
        days_to_ex: int = 1,
        days_to_payment: int = 14
    ) -> DividendDate:
        """
        Calculate all dividend dates from declaration date.

        Args:
            declaration_date: Date dividend is declared
            record_date: Record date (if known)
            days_to_ex: Business days before record date for ex-date
            days_to_payment: Days from record to payment

        Returns:
            DividendDate with all key dates
        """
        if record_date is None:
            # Estimate record date as 2 weeks after declaration
            record_date = declaration_date + timedelta(days=14)

        # Ex-date is 1 business day before record date
        ex_dividend_date = record_date - timedelta(days=days_to_ex)

        # Payment date
        payment_date = record_date + timedelta(days=days_to_payment)

        return DividendDate(
            declaration_date=declaration_date,
            ex_dividend_date=ex_dividend_date,
            record_date=record_date,
            payment_date=payment_date
        )

    def analyze_price_impact(
        self,
        pre_ex_price: float,
        post_ex_price: float,
        dividend_amount: float,
        tax_rate: float = 0
    ) -> Dict[str, Any]:
        """
        Analyze price impact around ex-dividend date.

        Theory: Price should drop by dividend amount (adjusted for taxes)
        """
        expected_drop = dividend_amount * (1 - tax_rate)
        actual_drop = pre_ex_price - post_ex_price

        drop_ratio = actual_drop / dividend_amount if dividend_amount > 0 else 0

        return {
            "pre_ex_price": pre_ex_price,
            "post_ex_price": post_ex_price,
            "dividend_amount": dividend_amount,
            "expected_drop": expected_drop,
            "actual_drop": actual_drop,
            "drop_ratio": drop_ratio,
            "interpretation": self._interpret_drop_ratio(drop_ratio)
        }

    def _interpret_drop_ratio(self, ratio: float) -> str:
        if ratio < 0.8:
            return "Price dropped less than dividend - possible arbitrage or tax effects"
        elif ratio > 1.2:
            return "Price dropped more than dividend - market conditions or news"
        else:
            return "Price dropped approximately by dividend amount - efficient market"


class CorporateActions:
    """
    Analyze dividend-related corporate actions.
    """

    def analyze_stock_dividend(
        self,
        shares_before: int,
        stock_dividend_rate: float,
        price_before: float
    ) -> Dict[str, Any]:
        """
        Analyze impact of stock dividend.

        Stock dividend: Additional shares given as percentage of holdings
        (e.g., 10% stock dividend = 10 new shares per 100 held)
        """
        new_shares = shares_before * stock_dividend_rate
        shares_after = shares_before + new_shares

        # Theoretical price adjustment (value unchanged)
        price_after = price_before * shares_before / shares_after

        return {
            "shares_before": shares_before,
            "stock_dividend_rate": stock_dividend_rate,
            "new_shares": new_shares,
            "shares_after": shares_after,
            "price_before": price_before,
            "theoretical_price_after": price_after,
            "total_value_before": shares_before * price_before,
            "total_value_after": shares_after * price_after,
            "interpretation": "Total value unchanged; more shares at lower price"
        }

    def analyze_stock_split(
        self,
        shares_before: int,
        split_ratio: Tuple[int, int],
        price_before: float
    ) -> Dict[str, Any]:
        """
        Analyze impact of stock split.

        Split ratio: (new_shares, old_shares)
        e.g., 2-for-1 split = (2, 1) - each old share becomes 2 new shares
        """
        new_shares_per_old = split_ratio[0] / split_ratio[1]
        shares_after = int(shares_before * new_shares_per_old)

        # Price adjusts inversely
        price_after = price_before / new_shares_per_old

        return {
            "shares_before": shares_before,
            "split_ratio": f"{split_ratio[0]}-for-{split_ratio[1]}",
            "shares_after": shares_after,
            "price_before": price_before,
            "price_after": price_after,
            "total_value_before": shares_before * price_before,
            "total_value_after": shares_after * price_after,
            "interpretation": "Total value unchanged; more shares at proportionally lower price"
        }

    def analyze_reverse_split(
        self,
        shares_before: int,
        split_ratio: Tuple[int, int],
        price_before: float
    ) -> Dict[str, Any]:
        """
        Analyze impact of reverse stock split.

        Split ratio: (new_shares, old_shares)
        e.g., 1-for-10 reverse split = (1, 10) - 10 old shares become 1 new share
        """
        new_shares_per_old = split_ratio[0] / split_ratio[1]
        shares_after = int(shares_before * new_shares_per_old)

        # Price adjusts inversely (increases)
        price_after = price_before / new_shares_per_old

        return {
            "shares_before": shares_before,
            "reverse_split_ratio": f"{split_ratio[0]}-for-{split_ratio[1]}",
            "shares_after": shares_after,
            "price_before": price_before,
            "price_after": price_after,
            "total_value_before": shares_before * price_before,
            "total_value_after": shares_after * price_after,
            "interpretation": "Total value unchanged; fewer shares at proportionally higher price",
            "typical_reason": "Avoid delisting, improve institutional appeal"
        }

    def analyze_share_repurchase(
        self,
        shares_outstanding_before: int,
        repurchase_amount: float,
        repurchase_price: float,
        earnings: float
    ) -> Dict[str, Any]:
        """
        Analyze impact of share repurchase on EPS.
        """
        shares_repurchased = int(repurchase_amount / repurchase_price)
        shares_after = shares_outstanding_before - shares_repurchased

        eps_before = earnings / shares_outstanding_before
        eps_after = earnings / shares_after
        eps_accretion = (eps_after - eps_before) / eps_before

        return {
            "shares_before": shares_outstanding_before,
            "repurchase_amount": repurchase_amount,
            "repurchase_price": repurchase_price,
            "shares_repurchased": shares_repurchased,
            "shares_after": shares_after,
            "eps_before": eps_before,
            "eps_after": eps_after,
            "eps_accretion_pct": eps_accretion * 100,
            "interpretation": f"EPS increases by {eps_accretion*100:.2f}% due to fewer shares"
        }


class FCFEModel:
    """
    Free Cash Flow to Equity (FCFE) Model

    Alternative to DDM when dividends don't reflect capacity to pay.

    FCFE = Net Income
           - (CapEx - Depreciation)
           - Change in Working Capital
           + Net Borrowing

    Value = Σ [FCFE_t / (1+r)^t] + Terminal Value
    """

    def calculate_fcfe(
        self,
        net_income: float,
        depreciation: float,
        capex: float,
        change_in_wc: float,
        net_borrowing: float
    ) -> float:
        """
        Calculate Free Cash Flow to Equity.
        """
        fcfe = net_income + depreciation - capex - change_in_wc + net_borrowing
        return fcfe

    def calculate_value(
        self,
        fcfe_current: float,
        growth_rate: float,
        required_return: float,
        is_fcfe0: bool = True
    ) -> Dict[str, Any]:
        """
        Single-stage FCFE valuation (constant growth).
        """
        if growth_rate >= required_return:
            raise ValueError("Growth rate must be less than required return")

        fcfe1 = fcfe_current * (1 + growth_rate) if is_fcfe0 else fcfe_current
        intrinsic_value = fcfe1 / (required_return - growth_rate)

        return {
            "intrinsic_value": intrinsic_value,
            "fcfe_current": fcfe_current,
            "fcfe_next": fcfe1,
            "growth_rate": growth_rate,
            "required_return": required_return,
            "model": "Single-Stage FCFE"
        }

    def two_stage_fcfe(
        self,
        fcfe0: float,
        high_growth: float,
        stable_growth: float,
        required_return: float,
        high_growth_years: int
    ) -> Dict[str, Any]:
        """
        Two-stage FCFE valuation.
        """
        if stable_growth >= required_return:
            raise ValueError("Stable growth must be less than required return")

        # Stage 1 PV
        stage1_pv = 0
        current_fcfe = fcfe0

        for t in range(1, high_growth_years + 1):
            current_fcfe = current_fcfe * (1 + high_growth)
            pv = current_fcfe / (1 + required_return) ** t
            stage1_pv += pv

        # Terminal value
        fcfe_terminal = current_fcfe * (1 + stable_growth)
        terminal_value = fcfe_terminal / (required_return - stable_growth)
        pv_terminal = terminal_value / (1 + required_return) ** high_growth_years

        intrinsic_value = stage1_pv + pv_terminal

        return {
            "intrinsic_value": intrinsic_value,
            "fcfe0": fcfe0,
            "high_growth_rate": high_growth,
            "stable_growth_rate": stable_growth,
            "required_return": required_return,
            "high_growth_years": high_growth_years,
            "stage1_pv": stage1_pv,
            "terminal_value": terminal_value,
            "pv_terminal": pv_terminal,
            "model": "Two-Stage FCFE"
        }


def main():
    """CLI entry point for dividend models."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Command required",
            "available_commands": [
                "gordon_growth",
                "two_stage_ddm",
                "h_model",
                "three_stage_ddm",
                "preferred_stock",
                "dividend_chronology",
                "stock_split",
                "stock_dividend",
                "share_repurchase",
                "fcfe"
            ]
        }))
        return

    command = sys.argv[1]

    try:
        if command == "gordon_growth":
            model = GordonGrowthModel()
            result = model.calculate_intrinsic_value(
                dividend=2.50,
                growth_rate=0.05,
                required_return=0.10,
                is_d0=True
            )
            output = {
                "intrinsic_value": result.intrinsic_value,
                "model_type": result.model_type,
                "assumptions": result.assumptions
            }
            print(json.dumps(output, indent=2))

        elif command == "two_stage_ddm":
            model = TwoStageDDM()
            result = model.calculate_intrinsic_value(
                d0=2.00,
                high_growth_rate=0.15,
                stable_growth_rate=0.04,
                required_return=0.10,
                high_growth_years=5
            )
            output = {
                "intrinsic_value": result.intrinsic_value,
                "model_type": result.model_type,
                "assumptions": result.assumptions
            }
            print(json.dumps(output, indent=2))

        elif command == "h_model":
            model = HModelDDM()
            result = model.calculate_intrinsic_value(
                d0=2.00,
                short_term_growth=0.20,
                long_term_growth=0.04,
                required_return=0.10,
                high_growth_period=10
            )
            output = {
                "intrinsic_value": result.intrinsic_value,
                "model_type": result.model_type,
                "assumptions": result.assumptions
            }
            print(json.dumps(output, indent=2))

        elif command == "preferred_stock":
            model = PreferredStockValuation()
            result = model.calculate_value(
                par_value=100,
                dividend_rate=0.06,
                required_return=0.08
            )
            print(json.dumps(result, indent=2))

        elif command == "dividend_chronology":
            chronology = DividendChronology()
            result = chronology.explain_chronology()
            print(json.dumps(result, indent=2))

        elif command == "stock_split":
            actions = CorporateActions()
            result = actions.analyze_stock_split(
                shares_before=100,
                split_ratio=(2, 1),
                price_before=150.00
            )
            print(json.dumps(result, indent=2))

        elif command == "stock_dividend":
            actions = CorporateActions()
            result = actions.analyze_stock_dividend(
                shares_before=100,
                stock_dividend_rate=0.10,
                price_before=50.00
            )
            print(json.dumps(result, indent=2))

        elif command == "share_repurchase":
            actions = CorporateActions()
            result = actions.analyze_share_repurchase(
                shares_outstanding_before=1000000,
                repurchase_amount=10000000,
                repurchase_price=50.00,
                earnings=5000000
            )
            print(json.dumps(result, indent=2))

        elif command == "fcfe":
            model = FCFEModel()
            result = model.two_stage_fcfe(
                fcfe0=100000000,
                high_growth=0.15,
                stable_growth=0.04,
                required_return=0.10,
                high_growth_years=5
            )
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({"error": f"Unknown command: {command}"}))

    except Exception as e:
        print(json.dumps({"error": str(e)}))


if __name__ == "__main__":
    main()
