"""
Bond Pricing Analytics Module
=============================

Core bond valuation and pricing calculations (CFA-curriculum methods).

Conventions (see fi_common.py for the full list)
------------------------------------------------
* Yields are nominal annual rates compounded `frequency` times a year (frequency 2 =
  bond-equivalent basis); frequency 0 = zero-coupon bond with annual compounding.
* `price` is the clean (quoted) price; dirty_price = price + accrued_interest.
* Time to maturity is measured from settlement. A fractional number of coupon periods is
  priced with a fractional first period (no truncation). Alternatively give
  settlement_date + maturity_date (YYYY-MM-DD) and day_count.
* Day counts: 30/360 (default), 30E/360, ACT/360, ACT/365, ACT/ACT (ICMA).

PARAMETERS (cli.py commands price, ytm, ytc, ytw, clean_dirty, spot_rate, forward_rate,
price_spot_curve):
  face_value (1000), coupon_rate (0.05), years_to_maturity (10), ytm, price,
  frequency (2), day_count, settlement_date, maturity_date, price_type (clean|dirty)
"""

import logging
import math
import os
import sys
from dataclasses import dataclass, field
from datetime import date
from enum import Enum
from typing import Any, Dict, List, Optional, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num, r  # noqa: E402

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


def _yield_conventions(frequency: int, timing: fc.BondTiming) -> Dict[str, Any]:
    conv = dict(timing.info)
    conv["yield_compounding"] = ("annual (zero-coupon)" if frequency == 0
                                 else f"nominal annual, compounded {frequency}x per year")
    conv["price"] = "clean (quoted); dirty_price = price + accrued_interest = PV of remaining cash flows"
    if frequency == 0:
        conv["coupon"] = "none (zero-coupon bond; coupon_rate is ignored)"
    return conv


class BondPricer:
    """
    Bond pricing engine: price from yield, yield from price (YTM / YTC / YTW),
    accrued interest, clean and dirty prices, spot and forward rates.
    """

    def __init__(self, specification: Optional[BondSpecification] = None):
        self.spec = specification or BondSpecification()

    # -- price / yield -----------------------------------------------------------------

    def calculate_price(
        self,
        ytm: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        frequency: int = 2,
        timing: Optional[fc.BondTiming] = None,
    ) -> Dict[str, Any]:
        """
        Bond price given yield to maturity.

        dirty = sum_k CF_k / (1 + y/f)^(k - 1 + v);  clean = dirty - accrued
        (v = fraction of the current coupon period still to run; v = 1 on a coupon date)
        """
        fc.check_frequency(frequency)
        if timing is None:
            timing = fc.timing_from_years(years_to_maturity, frequency, coupon_rate)
        if ytm <= -(frequency or 1):
            raise FIError(f"ytm must be > {-(frequency or 1)} for frequency {frequency}")
        flows = fc.bond_flows(face_value, coupon_rate, timing)
        b = 1 + (ytm if frequency == 0 else ytm / frequency)
        pv_principal = face_value * b ** (-flows[-1][0])
        dirty = fc.dirty_price(ytm, flows, frequency)
        pv_coupons = dirty - pv_principal
        accrued = face_value * timing.accrued_per_face
        clean = dirty - accrued

        if frequency == 0:
            return {
                'price': round(clean, 6),
                'clean_price': round(clean, 6),
                'dirty_price': round(dirty, 6),
                'accrued_interest': 0.0,
                'pv_coupons': 0.0,
                'pv_principal': round(pv_principal, 4),
                'num_periods': round(timing.years_from_settlement, 6),
                'premium_discount': round(clean - face_value, 4),
                'price_percent': round(clean / face_value * 100, 4),
                'bond_type': 'zero_coupon',
                'conventions': _yield_conventions(frequency, timing),
            }

        return {
            'price': round(clean, 6),
            'clean_price': round(clean, 6),
            'dirty_price': round(dirty, 6),
            'accrued_interest': round(accrued, 6),
            'pv_coupons': round(pv_coupons, 4),
            'pv_principal': round(pv_principal, 4),
            'num_periods': timing.n,
            'periodic_coupon': round(face_value * coupon_rate / frequency, 4),
            'periodic_rate': round(ytm / frequency, 6),
            'premium_discount': round(clean - face_value, 4),
            'price_percent': round(clean / face_value * 100, 4),
            'conventions': _yield_conventions(frequency, timing),
        }

    def _dirty_target(self, price: float, face_value: float, timing: fc.BondTiming, price_type: str) -> float:
        if price <= 0:
            raise FIError("price must be > 0")
        return price + face_value * timing.accrued_per_face if price_type == 'clean' else price

    def calculate_ytm(
        self,
        price: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        frequency: int = 2,
        price_type: str = 'clean',
        timing: Optional[fc.BondTiming] = None,
    ) -> Dict[str, Any]:
        """
        Yield to maturity given price (exact root of the price-yield equation).

        ytm is nominal annual compounded `frequency` times a year (annual for frequency 0).
        """
        fc.check_frequency(frequency)
        if timing is None:
            timing = fc.timing_from_years(years_to_maturity, frequency, coupon_rate)
        flows = fc.bond_flows(face_value, coupon_rate, timing)
        dirty = self._dirty_target(price, face_value, timing, price_type)
        accrued = face_value * timing.accrued_per_face
        clean = dirty - accrued

        if frequency == 0:
            T = timing.years_from_settlement
            ytm = (face_value / dirty) ** (1.0 / T) - 1.0
            bey = 2.0 * ((1.0 + ytm) ** 0.5 - 1.0)
            eay = ytm
        else:
            ytm = fc.solve_yield(dirty, flows, frequency)
            bey = 2.0 * ((1.0 + ytm / frequency) ** (frequency / 2.0) - 1.0)
            eay = (1.0 + ytm / frequency) ** frequency - 1.0

        warnings = []
        undiscounted = sum(cf for _, cf in flows)
        if dirty > undiscounted:
            warnings.append('price exceeds the sum of all remaining cash flows, so the yield is negative')

        current_yield = coupon_rate * face_value / clean if clean > 0 else None
        out = {
            'ytm': round(ytm, 6),
            'ytm_percent': round(ytm * 100, 4),
            'current_yield': r(current_yield, 6),
            'current_yield_percent': r(current_yield * 100, 4) if current_yield is not None else None,
            'bond_equivalent_yield': round(bey, 6),
            'effective_annual_yield': round(eay, 6),
            'price_used': price,
            'price_type': price_type,
            'clean_price': round(clean, 6),
            'dirty_price': round(dirty, 6),
            'accrued_interest': round(accrued, 6),
            'is_premium': clean > face_value,
            'is_discount': clean < face_value,
            'conventions': _yield_conventions(frequency, timing),
        }
        if warnings:
            out['warnings'] = warnings
        return out

    def calculate_ytc(
        self,
        price: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_call: float = 5.0,
        call_price: float = 1050.0,
        frequency: int = 2,
        price_type: str = 'clean',
        accrued_per_face: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Yield to call: the call date is treated as a coupon date on which the bond is redeemed at
        call_price. Yield convention as for YTM.
        """
        fc.check_frequency(frequency, allow_zero=False)
        if call_price <= 0:
            raise FIError("call_price must be > 0")
        timing = fc.timing_from_years(years_to_call, frequency, coupon_rate)
        if accrued_per_face is not None:
            timing.accrued_per_face = accrued_per_face
        flows = fc.bond_flows(face_value, coupon_rate, timing, redemption=call_price)
        dirty = self._dirty_target(price, face_value, timing, price_type)
        ytc = fc.solve_yield(dirty, flows, frequency)
        return {
            'ytc': round(ytc, 6),
            'ytc_percent': round(ytc * 100, 4),
            'years_to_call': years_to_call,
            'call_price': call_price,
            'call_premium': round(call_price - face_value, 2),
            'price_type': price_type,
            'dirty_price': round(dirty, 6),
            'conventions': {'yield_compounding': f'nominal annual, compounded {frequency}x per year',
                            'call_date': 'assumed to fall on a coupon date'},
        }

    def calculate_ytw(
        self,
        price: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        call_schedule: List[Tuple[float, float]] = None,
        frequency: int = 2,
        price_type: str = 'clean',
    ) -> Dict[str, Any]:
        """
        Yield to worst: the lowest of YTM and the YTC for every call date in call_schedule
        ([[years_to_call, call_price], ...]). Call dates must fall on coupon dates.
        """
        fc.check_frequency(frequency, allow_zero=False)
        mat_timing = fc.timing_from_years(years_to_maturity, frequency, coupon_rate)
        yields = []
        ytm_result = self.calculate_ytm(price, face_value, coupon_rate, years_to_maturity, frequency,
                                        price_type, timing=mat_timing)
        yields.append(('YTM', ytm_result['ytm'], years_to_maturity))

        phase = (years_to_maturity * frequency) % 1.0
        for i, item in enumerate(call_schedule or []):
            if not isinstance(item, (list, tuple)) or len(item) != 2:
                raise FIError(f"call_schedule[{i}] must be [years_to_call, call_price]")
            t_call = fc.as_num(item[0], f'call_schedule[{i}] years_to_call', lo=0, lo_open=True)
            c_price = fc.as_num(item[1], f'call_schedule[{i}] call_price', lo=0, lo_open=True)
            if t_call > years_to_maturity + 1e-9:
                raise FIError(f"call_schedule[{i}]: call date {t_call:g}y is after maturity {years_to_maturity:g}y")
            p = (t_call * frequency) % 1.0
            if min(abs(p - phase), 1 - abs(p - phase)) > 1e-6:
                raise FIError(f"call_schedule[{i}]: call date {t_call:g}y is not a coupon date of this bond")
            ytc_result = self.calculate_ytc(price, face_value, coupon_rate, t_call, c_price, frequency,
                                            price_type, accrued_per_face=mat_timing.accrued_per_face)
            yields.append((f'YTC_{t_call:g}y', ytc_result['ytc'], t_call))

        min_yield = min(yields, key=lambda x: x[1])
        return {
            'ytw': round(min_yield[1], 6),
            'ytw_percent': round(min_yield[1] * 100, 4),
            'ytw_type': min_yield[0],
            'ytw_horizon': min_yield[2],
            'all_yields': [{'type': y[0], 'yield': round(y[1], 6), 'horizon': y[2]} for y in yields],
            'price_type': price_type,
        }

    # -- accrued interest / clean & dirty ---------------------------------------------

    def calculate_accrued_interest(
        self,
        coupon_rate: float,
        face_value: float = 1000.0,
        days_since_last_coupon: int = 0,
        days_in_coupon_period: Optional[float] = None,
        day_count: Any = DayCountConvention.THIRTY_360,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Accrued interest since the last coupon.

        30/360, 30E/360, ACT/360: AI = C_annual * days / 360;  ACT/365: C_annual * days / 365;
        ACT/ACT (ICMA): AI = C_period * days / days_in_period.
        """
        dc = fc.norm_day_count(day_count.value if isinstance(day_count, DayCountConvention) else day_count)
        timing = fc.timing_from_days(1.0, frequency, coupon_rate, days_since_last_coupon,
                                     days_in_coupon_period, dc, True)
        coupon_payment = coupon_rate * face_value / frequency
        accrued = face_value * timing.accrued_per_face
        return {
            'accrued_interest': round(accrued, 6),
            'accrual_fraction': round(accrued / coupon_payment, 6) if coupon_payment else 0.0,
            'coupon_payment': round(coupon_payment, 4),
            'days_since_last_coupon': days_since_last_coupon,
            'days_in_period': timing.info['days_in_coupon_period'],
            'day_count_convention': dc,
        }

    def calculate_clean_dirty_price(
        self,
        ytm: float,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        days_since_last_coupon: float = 45,
        days_in_coupon_period: Optional[float] = None,
        frequency: int = 2,
        day_count: str = '30/360',
        timing: Optional[fc.BondTiming] = None,
        days_given: bool = True,
    ) -> Dict[str, Any]:
        """
        Clean (quoted) and dirty (full / invoice) price between coupon dates.

        dirty = PV at the last coupon date * (1 + y/f)^(days_since / days_in_period)
              = sum_k CF_k / (1 + y/f)^(k - 1 + v)
        clean = dirty - accrued_interest (accrued per the day-count convention)

        years_to_maturity: a whole number of coupon periods = counted from the last coupon date;
        a fractional one = time from settlement (must agree with days_since_last_coupon).
        """
        fc.check_frequency(frequency, allow_zero=False)
        if timing is None:
            timing = fc.timing_from_days(years_to_maturity * frequency, frequency, coupon_rate,
                                         days_since_last_coupon, days_in_coupon_period,
                                         fc.norm_day_count(day_count), days_given)
        if ytm <= -frequency:
            raise FIError(f"ytm must be > {-frequency}")
        flows = fc.bond_flows(face_value, coupon_rate, timing)
        dirty = fc.dirty_price(ytm, flows, frequency)
        accrued = face_value * timing.accrued_per_face
        clean = dirty - accrued
        # price at the last coupon date (whole periods) for reference
        price_at_last_coupon = dirty / (1 + ytm / frequency) ** (1.0 - timing.v)
        return {
            'clean_price': round(clean, 6),
            'dirty_price': round(dirty, 6),
            'accrued_interest': round(accrued, 6),
            'clean_price_percent': round(clean / face_value * 100, 4),
            'dirty_price_percent': round(dirty / face_value * 100, 4),
            'price_at_last_coupon_date': round(price_at_last_coupon, 4),
            'conventions': _yield_conventions(frequency, timing),
        }

    # -- spot / forward ----------------------------------------------------------------

    def calculate_spot_rate(
        self,
        price: float,
        face_value: float = 1000.0,
        years_to_maturity: float = 1.0,
        coupon_rate: float = 0.0,
    ) -> Dict[str, Any]:
        """
        Spot rate from a zero-coupon bond price: s = (FV/PV)^(1/T) - 1 (annual compounding).
        """
        if coupon_rate:
            raise FIError("spot_rate needs a zero-coupon price (coupon_rate 0); use bootstrap for coupon bonds")
        if price <= 0 or years_to_maturity <= 0 or face_value <= 0:
            raise FIError("price, face_value and years_to_maturity must be > 0")
        spot_rate = (face_value / price) ** (1 / years_to_maturity) - 1
        return {
            'spot_rate': round(spot_rate, 6),
            'spot_rate_percent': round(spot_rate * 100, 4),
            'discount_factor': round(price / face_value, 6),
            'maturity': years_to_maturity,
            'compounding': 'annual',
        }

    def bootstrap_spot_rates(
        self,
        bonds: List[Dict[str, float]],
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Bootstrap the spot curve from coupon bond prices (same engine as cli.py `bootstrap`).

        bonds: [{years|maturity, coupon_rate, price, face_value, frequency}], any order, any
        maturities (coupons that fall between known maturities are discounted on the
        interpolated curve).
        """
        instruments = fc.bond_instruments(bonds, frequency)
        curve, inst = fc.bootstrap_zero_curve(instruments)
        spot_rates = []
        for d in inst:
            z = d['zero_rate_cc']
            spot = math.exp(z) - 1.0
            spot_rates.append({
                'maturity': d['maturity'],
                'spot_rate': round(spot, 6),
                'spot_rate_percent': round(spot * 100, 4),
                'discount_factor': round(curve.df(d['maturity']), 6),
            })
        return {
            'spot_curve': spot_rates,
            'num_points': len(spot_rates),
            'compounding': 'annual',
        }

    def calculate_forward_rate(
        self,
        spot_rate_1: float,
        spot_rate_2: float,
        t1: float,
        t2: float,
    ) -> Dict[str, Any]:
        """
        Implied forward rate (annual compounding):
        f(t1,t2) = [(1+s2)^t2 / (1+s1)^t1]^(1/(t2-t1)) - 1
        """
        if t1 < 0:
            raise FIError('t1 must be >= 0')
        if t2 <= t1:
            raise FIError('t2 must be greater than t1')
        if spot_rate_1 <= -1 or spot_rate_2 <= -1:
            raise FIError('spot rates must be > -1')
        forward_rate = (((1 + spot_rate_2) ** t2 / (1 + spot_rate_1) ** t1) ** (1 / (t2 - t1))) - 1
        return {
            'forward_rate': round(forward_rate, 6),
            'forward_rate_percent': round(forward_rate * 100, 4),
            'period': f'{t1:g}y x {t2:g}y',
            'notation': f'f({t1:g},{t2:g})',
            'spot_rate_t1': spot_rate_1,
            'spot_rate_t2': spot_rate_2,
            'compounding': 'annual',
        }

    def price_with_spot_rates(
        self,
        spot_rates: List[float],
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Price a bond on a term structure: spot_rates[i] is the annually-compounded spot rate
        for the cash flow at t = (i+1)/frequency; the bond matures at the last one.
        """
        fc.check_frequency(frequency, allow_zero=False)
        if not spot_rates:
            raise FIError('spot_rates must be a non-empty list (one rate per coupon period)')
        coupon = (coupon_rate * face_value) / frequency
        num_periods = len(spot_rates)
        cash_flow_pvs = []
        total_pv = 0.0
        for i, spot in enumerate(spot_rates):
            if spot <= -1:
                raise FIError(f'spot_rates[{i}] must be > -1')
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
            'num_periods': num_periods,
            'compounding': 'annual spot rates',
        }


ANALYSIS_TYPES = ('price', 'ytm', 'ytc', 'ytw', 'clean_dirty', 'spot_rate', 'bootstrap', 'forward_rate',
                  'price_spot_curve')


def _common(params: Dict[str, Any]):
    face = num(params, 'face_value', 1000, lo=0, lo_open=True)
    coupon = num(params, 'coupon_rate', 0.05, lo=0)
    freq = fc.check_frequency(num(params, 'frequency', 2, integer=True))
    return face, coupon, freq


def run_bond_pricing_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for bond pricing analysis.

    params['analysis_type'] in ANALYSIS_TYPES; other keys depend on the analysis.
    Returns a result dict or {'error': message}.
    """
    pricer = BondPricer()
    analysis_type = params.get('analysis_type', 'price')

    try:
        if analysis_type == 'price':
            face, coupon, freq = _common(params)
            timing = fc.resolve_timing(params, coupon, freq)
            return pricer.calculate_price(
                ytm=num(params, 'ytm', 0.05), face_value=face, coupon_rate=coupon,
                frequency=freq, timing=timing)

        elif analysis_type == 'ytm':
            face, coupon, freq = _common(params)
            timing = fc.resolve_timing(params, coupon, freq)
            return pricer.calculate_ytm(
                price=num(params, 'price', 1000, lo=0, lo_open=True), face_value=face, coupon_rate=coupon,
                frequency=freq, price_type=fc.text(params, 'price_type', 'clean', choices=('clean', 'dirty')),
                timing=timing)

        elif analysis_type == 'ytc':
            face, coupon, freq = _common(params)
            return pricer.calculate_ytc(
                price=num(params, 'price', 1000, lo=0, lo_open=True), face_value=face, coupon_rate=coupon,
                years_to_call=num(params, 'years_to_call', 5, lo=0, lo_open=True),
                call_price=num(params, 'call_price', 1050, lo=0, lo_open=True), frequency=freq,
                price_type=fc.text(params, 'price_type', 'clean', choices=('clean', 'dirty')))

        elif analysis_type == 'ytw':
            face, coupon, freq = _common(params)
            schedule = params.get('call_schedule')
            if schedule is not None and not isinstance(schedule, list):
                raise FIError('call_schedule must be a list of [years_to_call, call_price]')
            return pricer.calculate_ytw(
                price=num(params, 'price', 1000, lo=0, lo_open=True), face_value=face, coupon_rate=coupon,
                years_to_maturity=num(params, 'years_to_maturity', 10, lo=0, lo_open=True),
                call_schedule=schedule, frequency=freq,
                price_type=fc.text(params, 'price_type', 'clean', choices=('clean', 'dirty')))

        elif analysis_type == 'clean_dirty':
            face, coupon, freq = _common(params)
            dc = fc.norm_day_count(params.get('day_count'))
            if params.get('settlement_date') is not None or params.get('maturity_date') is not None:
                timing = fc.resolve_timing(params, coupon, freq)
                return pricer.calculate_clean_dirty_price(
                    ytm=num(params, 'ytm', 0.05), face_value=face, coupon_rate=coupon,
                    frequency=freq, timing=timing)
            days_given = params.get('days_since_last_coupon') is not None
            return pricer.calculate_clean_dirty_price(
                ytm=num(params, 'ytm', 0.05), face_value=face, coupon_rate=coupon,
                years_to_maturity=num(params, 'years_to_maturity', 10, lo=0, lo_open=True),
                days_since_last_coupon=num(params, 'days_since_last_coupon', 45, lo=0),
                days_in_coupon_period=num(params, 'days_in_coupon_period', None, lo=0, lo_open=True),
                frequency=freq, day_count=dc, days_given=days_given)

        elif analysis_type == 'spot_rate':
            return pricer.calculate_spot_rate(
                price=num(params, 'price', 950, lo=0, lo_open=True),
                face_value=num(params, 'face_value', 1000, lo=0, lo_open=True),
                years_to_maturity=num(params, 'years_to_maturity', 1, lo=0, lo_open=True),
                coupon_rate=num(params, 'coupon_rate', 0.0, lo=0))

        elif analysis_type == 'bootstrap':
            return pricer.bootstrap_spot_rates(
                bonds=params.get('bonds', []),
                frequency=fc.check_frequency(num(params, 'frequency', 2, integer=True)))

        elif analysis_type == 'forward_rate':
            return pricer.calculate_forward_rate(
                spot_rate_1=num(params, 'spot_rate_1', 0.03),
                spot_rate_2=num(params, 'spot_rate_2', 0.04),
                t1=num(params, 't1', 1), t2=num(params, 't2', 2))

        elif analysis_type == 'price_spot_curve':
            return pricer.price_with_spot_rates(
                spot_rates=fc.num_list(params, 'spot_rates', min_len=1),
                face_value=num(params, 'face_value', 1000, lo=0, lo_open=True),
                coupon_rate=num(params, 'coupon_rate', 0.05, lo=0),
                frequency=fc.check_frequency(num(params, 'frequency', 2, integer=True), allow_zero=False))

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Bond pricing analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_bond_pricing_analysis, ANALYSIS_TYPES, 'bond_pricing')
