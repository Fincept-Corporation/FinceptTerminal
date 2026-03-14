"""
Bond Pricing Analytics Module
=============================

Core bond valuation and pricing calculations implementing CFA Institute standard
methodologies for fixed income securities analysis.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Bond specifications (face value, coupon rate, maturity)
  - Market yields and discount rates
  - Settlement and maturity dates
  - Day count conventions
  - Call/put schedules for callable/putable bonds

OUTPUT:
  - Clean and dirty bond prices
  - Yield measures (YTM, YTC, YTW, current yield)
  - Accrued interest calculations
  - Spot rate derivations
  - Price-yield sensitivity metrics

PARAMETERS:
  - face_value: Par/face value of the bond - default: 1000
  - coupon_rate: Annual coupon rate as decimal - default: 0.05
  - years_to_maturity: Time to maturity in years
  - ytm: Yield to maturity as decimal
  - frequency: Coupon payment frequency per year - default: 2
  - day_count: Day count convention - default: DayCountConvention.THIRTY_360
  - settlement_date: Trade settlement date
  - maturity_date: Bond maturity date
  - call_price: Call price for callable bonds
  - call_date: First call date for callable bonds
"""

from abc import ABC, abstractmethod
from enum import Enum
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any, Tuple, Union
from datetime import datetime, date
import numpy as np
from scipy import optimize
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class BondType(Enum):
    """Classification of bond types"""
    ZERO_COUPON = "zero_coupon"
    FIXED_RATE = "fixed_rate"
    FLOATING_RATE = "floating_rate"
    CALLABLE = "callable"
    PUTABLE = "putable"
    CONVERTIBLE = "convertible"
    INFLATION_LINKED = "inflation_linked"


class CouponFrequency(Enum):
    """Coupon payment frequencies"""
    ANNUAL = 1
    SEMI_ANNUAL = 2
    QUARTERLY = 4
    MONTHLY = 12
    ZERO = 0


class DayCountConvention(Enum):
    """Day count conventions for accrued interest"""
    ACT_360 = "ACT/360"
    ACT_365 = "ACT/365"
    ACT_ACT = "ACT/ACT"
    THIRTY_360 = "30/360"
    THIRTY_360_EU = "30E/360"


@dataclass
class BondCashFlow:
    """Represents a single bond cash flow"""
    date: date
    amount: float
    period: int
    is_principal: bool = False


@dataclass
class BondSpecification:
    """Complete bond specification"""
    face_value: float = 1000.0
    coupon_rate: float = 0.05
    maturity_date: date = None
    issue_date: date = None
    settlement_date: date = None
    frequency: CouponFrequency = CouponFrequency.SEMI_ANNUAL
    day_count: DayCountConvention = DayCountConvention.THIRTY_360
    bond_type: BondType = BondType.FIXED_RATE
    call_schedule: List[Tuple[date, float]] = field(default_factory=list)
    put_schedule: List[Tuple[date, float]] = field(default_factory=list)


class BondPricer:
    """
    Bond pricing engine implementing CFA-standard valuation methods.

    Provides comprehensive bond analytics including:
    - Present value calculations
    - Yield measures (YTM, YTC, YTW)
    - Accrued interest
    - Clean and dirty prices
    """

    def __init__(self, specification: Optional[BondSpecification] = None):
        """
        Initialize bond pricer.

        Args:
            specification: Bond specification object
        """
        self.spec = specification or BondSpecification()
        self._cash_flows: List[BondCashFlow] = []

    def calculate_price(
        self,
        ytm: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate bond price given yield to maturity.

        PV = sum(C/(1+y/n)^t) + FV/(1+y/n)^N

        Args:
            ytm: Yield to maturity (decimal)
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            frequency: Coupon payments per year

        Returns:
            Dictionary with price and component details
        """
        if frequency == 0:
            # Zero coupon bond
            price = face_value / ((1 + ytm) ** years_to_maturity)
            return {
                'price': round(price, 4),
                'pv_coupons': 0.0,
                'pv_principal': round(price, 4),
                'num_periods': years_to_maturity,
                'bond_type': 'zero_coupon'
            }

        # Periodic values
        periods = int(years_to_maturity * frequency)
        periodic_rate = ytm / frequency
        coupon_payment = (coupon_rate * face_value) / frequency

        # PV of coupon payments (annuity formula)
        if periodic_rate > 0:
            pv_coupons = coupon_payment * (1 - (1 + periodic_rate) ** -periods) / periodic_rate
        else:
            pv_coupons = coupon_payment * periods

        # PV of principal
        pv_principal = face_value / ((1 + periodic_rate) ** periods)

        # Total price
        price = pv_coupons + pv_principal

        return {
            'price': round(price, 4),
            'pv_coupons': round(pv_coupons, 4),
            'pv_principal': round(pv_principal, 4),
            'num_periods': periods,
            'periodic_coupon': round(coupon_payment, 4),
            'periodic_rate': round(periodic_rate, 6),
            'premium_discount': round(price - face_value, 4),
            'price_percent': round((price / face_value) * 100, 4)
        }

    def calculate_ytm(
        self,
        price: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate yield to maturity given bond price.

        Uses Newton-Raphson iteration to solve for YTM.

        Args:
            price: Current market price
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            frequency: Coupon payments per year

        Returns:
            Dictionary with YTM and related metrics
        """
        coupon_payment = (coupon_rate * face_value) / frequency if frequency > 0 else 0
        periods = int(years_to_maturity * frequency) if frequency > 0 else years_to_maturity

        def price_diff(y):
            if frequency == 0:
                return face_value / ((1 + y) ** years_to_maturity) - price
            periodic_rate = y / frequency
            if periodic_rate <= -1:
                return float('inf')
            pv_coupons = coupon_payment * (1 - (1 + periodic_rate) ** -periods) / periodic_rate if periodic_rate != 0 else coupon_payment * periods
            pv_principal = face_value / ((1 + periodic_rate) ** periods)
            return pv_coupons + pv_principal - price

        try:
            ytm = optimize.brentq(price_diff, -0.99, 2.0, xtol=1e-10)
        except ValueError:
            # Fallback to Newton method
            try:
                ytm = optimize.newton(price_diff, coupon_rate, tol=1e-10)
            except:
                ytm = None

        if ytm is None:
            return {'error': 'Could not converge to YTM solution'}

        # Calculate related metrics
        current_yield = (coupon_rate * face_value) / price if price > 0 else 0

        # Bond equivalent yield (for comparison)
        bey = ytm if frequency == 2 else 2 * ((1 + ytm / frequency) ** (frequency / 2) - 1)

        # Effective annual yield
        eay = (1 + ytm / frequency) ** frequency - 1 if frequency > 0 else ytm

        return {
            'ytm': round(ytm, 6),
            'ytm_percent': round(ytm * 100, 4),
            'current_yield': round(current_yield, 6),
            'current_yield_percent': round(current_yield * 100, 4),
            'bond_equivalent_yield': round(bey, 6),
            'effective_annual_yield': round(eay, 6),
            'price_used': price,
            'is_premium': price > face_value,
            'is_discount': price < face_value
        }

    def calculate_ytc(
        self,
        price: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_call: float = 5.0,
        call_price: float = 1050.0,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate yield to call for callable bonds.

        Args:
            price: Current market price
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_call: Years until first call date
            call_price: Call redemption price
            frequency: Coupon payments per year

        Returns:
            Dictionary with YTC and related metrics
        """
        coupon_payment = (coupon_rate * face_value) / frequency
        periods = int(years_to_call * frequency)

        def price_diff(y):
            periodic_rate = y / frequency
            if periodic_rate <= -1:
                return float('inf')
            pv_coupons = coupon_payment * (1 - (1 + periodic_rate) ** -periods) / periodic_rate if periodic_rate != 0 else coupon_payment * periods
            pv_call = call_price / ((1 + periodic_rate) ** periods)
            return pv_coupons + pv_call - price

        try:
            ytc = optimize.brentq(price_diff, -0.99, 2.0, xtol=1e-10)
        except:
            try:
                ytc = optimize.newton(price_diff, coupon_rate, tol=1e-10)
            except:
                return {'error': 'Could not converge to YTC solution'}

        return {
            'ytc': round(ytc, 6),
            'ytc_percent': round(ytc * 100, 4),
            'years_to_call': years_to_call,
            'call_price': call_price,
            'call_premium': round(call_price - face_value, 2)
        }

    def calculate_ytw(
        self,
        price: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        call_schedule: List[Tuple[float, float]] = None,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate yield to worst - minimum of YTM and all YTCs.

        Args:
            price: Current market price
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            call_schedule: List of (years_to_call, call_price) tuples
            frequency: Coupon payments per year

        Returns:
            Dictionary with YTW and comparison of all yields
        """
        yields = []

        # Calculate YTM
        ytm_result = self.calculate_ytm(price, face_value, coupon_rate, years_to_maturity, frequency)
        if 'ytm' in ytm_result:
            yields.append(('YTM', ytm_result['ytm'], years_to_maturity))

        # Calculate YTC for each call date
        if call_schedule:
            for years_to_call, call_price in call_schedule:
                ytc_result = self.calculate_ytc(price, face_value, coupon_rate, years_to_call, call_price, frequency)
                if 'ytc' in ytc_result:
                    yields.append((f'YTC_{years_to_call}y', ytc_result['ytc'], years_to_call))

        if not yields:
            return {'error': 'No valid yields calculated'}

        # Find minimum yield
        min_yield = min(yields, key=lambda x: x[1])

        return {
            'ytw': round(min_yield[1], 6),
            'ytw_percent': round(min_yield[1] * 100, 4),
            'ytw_type': min_yield[0],
            'ytw_horizon': min_yield[2],
            'all_yields': [{'type': y[0], 'yield': round(y[1], 6), 'horizon': y[2]} for y in yields]
        }

    def calculate_accrued_interest(
        self,
        coupon_rate: float,
        face_value: float = 1000.0,
        days_since_last_coupon: int = 0,
        days_in_coupon_period: int = 180,
        day_count: DayCountConvention = DayCountConvention.THIRTY_360,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate accrued interest since last coupon payment.

        AI = (Coupon Payment) * (Days Since Last Coupon / Days in Period)

        Args:
            coupon_rate: Annual coupon rate (decimal)
            face_value: Face/par value
            days_since_last_coupon: Days elapsed since last coupon
            days_in_coupon_period: Total days in coupon period
            day_count: Day count convention
            frequency: Coupon payments per year

        Returns:
            Dictionary with accrued interest details
        """
        coupon_payment = (coupon_rate * face_value) / frequency

        # Calculate accrual fraction based on day count
        if day_count == DayCountConvention.THIRTY_360:
            accrual_fraction = days_since_last_coupon / 360 * frequency
        elif day_count == DayCountConvention.ACT_365:
            accrual_fraction = days_since_last_coupon / 365 * frequency
        elif day_count == DayCountConvention.ACT_360:
            accrual_fraction = days_since_last_coupon / 360 * frequency
        else:  # ACT/ACT
            accrual_fraction = days_since_last_coupon / days_in_coupon_period

        accrued_interest = coupon_payment * accrual_fraction

        return {
            'accrued_interest': round(accrued_interest, 4),
            'accrual_fraction': round(accrual_fraction, 6),
            'coupon_payment': round(coupon_payment, 4),
            'days_since_last_coupon': days_since_last_coupon,
            'days_in_period': days_in_coupon_period,
            'day_count_convention': day_count.value
        }

    def calculate_clean_dirty_price(
        self,
        ytm: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        days_since_last_coupon: int = 45,
        days_in_coupon_period: int = 180,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate clean price (quoted) and dirty price (invoice/full).

        Dirty Price = Clean Price + Accrued Interest

        Args:
            ytm: Yield to maturity (decimal)
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            days_since_last_coupon: Days since last coupon
            days_in_coupon_period: Days in coupon period
            frequency: Coupon payments per year

        Returns:
            Dictionary with clean and dirty prices
        """
        # Calculate full price at settlement
        price_result = self.calculate_price(ytm, face_value, coupon_rate, years_to_maturity, frequency)
        dirty_price = price_result['price']

        # Calculate accrued interest
        ai_result = self.calculate_accrued_interest(
            coupon_rate, face_value, days_since_last_coupon,
            days_in_coupon_period, DayCountConvention.THIRTY_360, frequency
        )
        accrued_interest = ai_result['accrued_interest']

        # Clean price = Dirty price - Accrued interest
        clean_price = dirty_price - accrued_interest

        return {
            'clean_price': round(clean_price, 4),
            'dirty_price': round(dirty_price, 4),
            'accrued_interest': round(accrued_interest, 4),
            'clean_price_percent': round((clean_price / face_value) * 100, 4),
            'dirty_price_percent': round((dirty_price / face_value) * 100, 4)
        }

    def calculate_spot_rate(
        self,
        price: float,
        face_value: float = 1000.0,
        years_to_maturity: float = 1.0,
        coupon_rate: float = 0.0,
    ) -> Dict[str, Any]:
        """
        Calculate spot rate from zero-coupon bond price.

        Spot Rate = (FV/PV)^(1/n) - 1

        Args:
            price: Current market price
            face_value: Face/par value
            years_to_maturity: Years until maturity
            coupon_rate: Should be 0 for spot rate calculation

        Returns:
            Dictionary with spot rate
        """
        if coupon_rate > 0:
            logger.warning("Spot rate calculation assumes zero-coupon bond")

        if price <= 0 or years_to_maturity <= 0:
            return {'error': 'Invalid inputs for spot rate calculation'}

        spot_rate = (face_value / price) ** (1 / years_to_maturity) - 1

        # Discount factor
        discount_factor = price / face_value

        return {
            'spot_rate': round(spot_rate, 6),
            'spot_rate_percent': round(spot_rate * 100, 4),
            'discount_factor': round(discount_factor, 6),
            'maturity': years_to_maturity
        }

    def bootstrap_spot_rates(
        self,
        bonds: List[Dict[str, float]],
    ) -> Dict[str, Any]:
        """
        Bootstrap spot rate curve from coupon bond prices.

        Args:
            bonds: List of dicts with keys: price, coupon_rate, years, face_value
                  Must be sorted by maturity, starting with shortest

        Returns:
            Dictionary with spot rate curve
        """
        spot_rates = []

        for i, bond in enumerate(bonds):
            price = bond.get('price', 1000)
            coupon_rate = bond.get('coupon_rate', 0)
            years = bond.get('years', i + 1)
            face_value = bond.get('face_value', 1000)
            frequency = bond.get('frequency', 2)

            if coupon_rate == 0:
                # Zero coupon - direct calculation
                spot = (face_value / price) ** (1 / years) - 1
            else:
                # Coupon bond - bootstrap using previous spot rates
                coupon = (coupon_rate * face_value) / frequency
                periods = int(years * frequency)

                # PV of known cash flows using known spot rates
                pv_known = 0
                for j, sr in enumerate(spot_rates):
                    t = (j + 1) / frequency
                    if t < years:
                        pv_known += coupon / ((1 + sr['spot_rate']) ** t)

                # Solve for current spot rate
                remaining_pv = price - pv_known
                final_cf = coupon + face_value

                if remaining_pv > 0:
                    spot = (final_cf / remaining_pv) ** (1 / years) - 1
                else:
                    spot = 0

            spot_rates.append({
                'maturity': years,
                'spot_rate': round(spot, 6),
                'spot_rate_percent': round(spot * 100, 4),
                'discount_factor': round(1 / ((1 + spot) ** years), 6)
            })

        return {
            'spot_curve': spot_rates,
            'num_points': len(spot_rates)
        }

    def calculate_forward_rate(
        self,
        spot_rate_1: float,
        spot_rate_2: float,
        t1: float,
        t2: float,
    ) -> Dict[str, Any]:
        """
        Calculate implied forward rate between two periods.

        f(t1,t2) = [(1+s2)^t2 / (1+s1)^t1]^(1/(t2-t1)) - 1

        Args:
            spot_rate_1: Spot rate for period t1
            spot_rate_2: Spot rate for period t2
            t1: First time period (years)
            t2: Second time period (years)

        Returns:
            Dictionary with forward rate
        """
        if t2 <= t1:
            return {'error': 't2 must be greater than t1'}

        forward_rate = (
            ((1 + spot_rate_2) ** t2 / (1 + spot_rate_1) ** t1) ** (1 / (t2 - t1))
        ) - 1

        return {
            'forward_rate': round(forward_rate, 6),
            'forward_rate_percent': round(forward_rate * 100, 4),
            'period': f'{t1}y x {t2}y',
            'notation': f'f({t1},{t2})',
            'spot_rate_t1': spot_rate_1,
            'spot_rate_t2': spot_rate_2
        }

    def price_with_spot_rates(
        self,
        spot_rates: List[float],
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Price bond using term structure of spot rates.

        Args:
            spot_rates: List of spot rates for each period
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            frequency: Coupon payments per year

        Returns:
            Dictionary with price from spot rates
        """
        coupon = (coupon_rate * face_value) / frequency
        num_periods = len(spot_rates)

        cash_flow_pvs = []
        total_pv = 0

        for i, spot in enumerate(spot_rates):
            t = (i + 1) / frequency
            cf = coupon if i < num_periods - 1 else coupon + face_value
            discount_factor = 1 / ((1 + spot) ** t)
            pv = cf * discount_factor

            cash_flow_pvs.append({
                'period': i + 1,
                'time': t,
                'cash_flow': round(cf, 2),
                'spot_rate': round(spot, 6),
                'discount_factor': round(discount_factor, 6),
                'present_value': round(pv, 4)
            })
            total_pv += pv

        return {
            'price': round(total_pv, 4),
            'price_percent': round((total_pv / face_value) * 100, 4),
            'cash_flows': cash_flow_pvs,
            'num_periods': num_periods
        }


def run_bond_pricing_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for bond pricing analysis.

    Args:
        params: Dictionary with analysis parameters
            - analysis_type: Type of analysis to run
            - Additional parameters based on analysis type

    Returns:
        Analysis results dictionary
    """
    pricer = BondPricer()
    analysis_type = params.get('analysis_type', 'price')

    try:
        if analysis_type == 'price':
            return pricer.calculate_price(
                ytm=params.get('ytm', 0.05),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'ytm':
            return pricer.calculate_ytm(
                price=params.get('price', 1000),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'ytc':
            return pricer.calculate_ytc(
                price=params.get('price', 1000),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_call=params.get('years_to_call', 5),
                call_price=params.get('call_price', 1050),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'ytw':
            return pricer.calculate_ytw(
                price=params.get('price', 1000),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                call_schedule=params.get('call_schedule'),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'clean_dirty':
            return pricer.calculate_clean_dirty_price(
                ytm=params.get('ytm', 0.05),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                days_since_last_coupon=params.get('days_since_last_coupon', 45),
                days_in_coupon_period=params.get('days_in_coupon_period', 180),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'spot_rate':
            return pricer.calculate_spot_rate(
                price=params.get('price', 950),
                face_value=params.get('face_value', 1000),
                years_to_maturity=params.get('years_to_maturity', 1)
            )

        elif analysis_type == 'bootstrap':
            return pricer.bootstrap_spot_rates(
                bonds=params.get('bonds', [])
            )

        elif analysis_type == 'forward_rate':
            return pricer.calculate_forward_rate(
                spot_rate_1=params.get('spot_rate_1', 0.03),
                spot_rate_2=params.get('spot_rate_2', 0.04),
                t1=params.get('t1', 1),
                t2=params.get('t2', 2)
            )

        elif analysis_type == 'price_spot_curve':
            return pricer.price_with_spot_rates(
                spot_rates=params.get('spot_rates', []),
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                frequency=params.get('frequency', 2)
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Bond pricing analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_bond_pricing_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Bond Pricing Demo:")
        pricer = BondPricer()

        # Price calculation
        result = pricer.calculate_price(ytm=0.06, coupon_rate=0.05, years_to_maturity=10)
        print(f"\nPrice at 6% YTM: ${result['price']}")

        # YTM calculation
        result = pricer.calculate_ytm(price=925.61, coupon_rate=0.05, years_to_maturity=10)
        print(f"YTM at $925.61: {result['ytm_percent']}%")
