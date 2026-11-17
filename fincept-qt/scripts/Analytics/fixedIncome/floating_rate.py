"""
Floating Rate and Money Market Instruments Module
=================================================

Floating rate notes (FRNs) and money market yields (CFA-curriculum methods).

Conventions
-----------
* FRN cash flows: coupon_t = (reference rate + quoted margin) / m x face, m = reset_frequency; discounted
  at (reference rate + discount margin) / m per period (CFA "simple" FRN model, flat reference rate).
  Remaining life is a whole number of reset periods (periods_remaining, or years_to_maturity x m).
* Z-DM: spot curve given per reset period (annual rates compounded m times a year). Coupons are
  projected from the FORWARD rates implied by that curve; cash flows are discounted at (spot + Z-DM)/m.
* Caps / floors on the coupon are valued with Black-76: caplet_i = tau x face x DF_i x Black(F_i, K - QM,
  sigma, T_reset_i). The first coupon is already set (its caplet/floorlet is intrinsic only).
  rate_volatility is the lognormal (Black) volatility of the reference rate.
* Money market: bank discount (face, 360), money market / CD-equivalent (price, 360),
  bond-equivalent (price, 365), effective annual (compounded, 365).

PARAMETERS:
  quoted_margin, discount_margin, reference_rate, periods_remaining | years_to_maturity, reset_frequency (4),
  face_value (1000), price, spot_rates | forward_rates, cap_rate, floor_rate, rate_volatility,
  days_to_maturity, yield_type
"""

import logging
import math
import os
import sys
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, List, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num  # noqa: E402

logger = logging.getLogger(__name__)

N = fc.NORMAL


class ReferenceRate(Enum):
    """Reference rates for floating rate instruments"""
    SOFR = "sofr"  # Secured Overnight Financing Rate
    EFFR = "effr"  # Effective Federal Funds Rate
    EURIBOR = "euribor"  # Euro Interbank Offered Rate
    SONIA = "sonia"  # Sterling Overnight Index Average
    TONAR = "tonar"  # Tokyo Overnight Average Rate
    ESTR = "estr"  # Euro Short-Term Rate
    PRIME = "prime"  # US Prime Rate
    TREASURY = "treasury"  # Treasury rate


class MoneyMarketInstrument(Enum):
    """Money market instrument types"""
    TREASURY_BILL = "t_bill"
    COMMERCIAL_PAPER = "commercial_paper"
    CD = "certificate_of_deposit"
    REPO = "repurchase_agreement"
    BANKERS_ACCEPTANCE = "bankers_acceptance"
    EURODOLLAR = "eurodollar_deposit"
    FED_FUNDS = "federal_funds"


@dataclass
class FRNSpecification:
    """Floating rate note specification"""
    face_value: float = 1000.0
    quoted_margin: float = 0.01  # 100 bps
    reference_rate: ReferenceRate = ReferenceRate.SOFR
    reset_frequency: int = 4  # Quarterly
    maturity_years: float = 5.0
    cap_rate: Optional[float] = None
    floor_rate: Optional[float] = None
    current_reference_rate: float = 0.05


def black76(forward: float, strike: float, sigma: float, expiry: float, call: bool) -> float:
    """Undiscounted Black-76 option value on a rate (per unit notional, per unit accrual)."""
    if call and strike <= 0:
        return forward - strike  # always exercised under a lognormal forward
    if not call and strike <= 0:
        return 0.0
    if expiry <= 0 or sigma <= 0:
        return max(forward - strike, 0.0) if call else max(strike - forward, 0.0)
    st = sigma * math.sqrt(expiry)
    d1 = (math.log(forward / strike) + 0.5 * st * st) / st
    d2 = d1 - st
    if call:
        return forward * N.cdf(d1) - strike * N.cdf(d2)
    return strike * N.cdf(-d2) - forward * N.cdf(-d1)


def _check_frn(face_value: float, periods_remaining: int, reset_frequency: int) -> None:
    if face_value <= 0:
        raise FIError('face_value must be > 0')
    if periods_remaining < 1:
        raise FIError('periods_remaining must be >= 1')
    if reset_frequency not in (1, 2, 4, 12):
        raise FIError('reset_frequency must be 1, 2, 4 or 12')


class FloatingRateAnalyzer:
    """FRN pricing, discount margin, Z-DM, caps/floors and reference-rate reference text."""

    def calculate_frn_price(
        self,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        discount_margin: float = 0.01,
        reference_rate: float = 0.05,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
    ) -> Dict[str, Any]:
        """Price = sum (ref + QM)/m x face / (1 + (ref + DM)/m)^t + face / (1 + (ref + DM)/m)^N"""
        _check_frn(face_value, periods_remaining, reset_frequency)
        periodic_discount = (reference_rate + discount_margin) / reset_frequency
        if periodic_discount <= -1:
            raise FIError('reference_rate + discount_margin is too negative')
        coupon = face_value * (reference_rate + quoted_margin) / reset_frequency
        d = 1 + periodic_discount
        pv = sum(coupon * d ** -t for t in range(1, periods_remaining + 1))
        pv += face_value * d ** -periods_remaining

        if abs(discount_margin - quoted_margin) < 1e-12:
            price_status, explanation = "Par", "DM = QM means FRN trades at par"
        elif discount_margin < quoted_margin:
            price_status, explanation = "Premium", "DM < QM means market requires less spread than contractual"
        else:
            price_status, explanation = "Discount", "DM > QM means market requires more spread than contractual"

        return {
            'price': round(pv, 4),
            'price_percent': round((pv / face_value) * 100, 4),
            'face_value': face_value,
            'quoted_margin': quoted_margin,
            'quoted_margin_bps': round(quoted_margin * 10000, 1),
            'discount_margin': discount_margin,
            'discount_margin_bps': round(discount_margin * 10000, 1),
            'reference_rate': reference_rate,
            'coupon_rate': round(reference_rate + quoted_margin, 6),
            'price_status': price_status,
            'explanation': explanation,
            'periods_remaining': periods_remaining,
            'years_remaining': periods_remaining / reset_frequency,
            'conventions': {'reference_rate': 'flat (current rate assumed for every reset)',
                            'compounding': f'{reset_frequency}x per year'},
        }

    def calculate_discount_margin(
        self,
        price: float,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        reference_rate: float = 0.05,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
    ) -> Dict[str, Any]:
        """Discount margin: the DM at which calculate_frn_price reproduces `price` (exact root)."""
        _check_frn(face_value, periods_remaining, reset_frequency)
        if price <= 0:
            raise FIError('price must be > 0')
        coupon = face_value * (reference_rate + quoted_margin) / reset_frequency
        m = reset_frequency

        def pv(dm):
            d = 1 + (reference_rate + dm) / m
            if d <= 0:
                return float('inf')
            return (sum(coupon * d ** -t for t in range(1, periods_remaining + 1))
                    + face_value * d ** -periods_remaining)

        lo = -reference_rate - m * 0.999999
        hi = 1.0
        fn = fc.overflow_safe(lambda dm: pv(dm) - price)
        if fn(lo) < 0:
            raise FIError('price is too high to be reached by any discount margin')
        while fn(hi) > 0:
            hi *= 2
            if hi > 1e4:
                raise FIError('price is too low to be reached by any discount margin')
        dm = fc.find_root(fn, lo, hi, tol=1e-14)
        spread_to_qm = dm - quoted_margin
        return {
            'discount_margin': round(dm, 8),
            'discount_margin_bps': round(dm * 10000, 2),
            'quoted_margin': quoted_margin,
            'quoted_margin_bps': round(quoted_margin * 10000, 1),
            'spread_to_quoted_margin': round(spread_to_qm, 8),
            'spread_to_qm_bps': round(spread_to_qm * 10000, 2),
            'price': price,
            'price_percent': round((price / face_value) * 100, 4),
            'interpretation': f"Market requires {round(dm * 10000, 1)}bps over reference vs "
                              f"{round(quoted_margin * 10000, 1)}bps contractual"
        }

    def calculate_zero_discount_margin(
        self,
        price: float,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        spot_rates: List[float] = None,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
        forward_rates: List[float] = None,
        reference_rate: float = 0.05,
    ) -> Dict[str, Any]:
        """
        Zero-discount margin. spot_rates[i] / forward_rates[i] belong to reset period i+1 (annual rates,
        compounded m times a year). Coupons use the forward rates; discounting uses spot + Z-DM.
        With neither curve given the curve is flat at reference_rate.
        """
        _check_frn(face_value, periods_remaining, reset_frequency)
        if price <= 0:
            raise FIError('price must be > 0')
        m = reset_frequency
        notes = []
        if spot_rates:
            source = 'spot_rates'
            spots = list(spot_rates)
        elif forward_rates:
            source = 'forward_rates'
            spots, growth = [], 1.0
            for i, f in enumerate(forward_rates, 1):
                if 1 + f / m <= 0:
                    raise FIError(f'forward_rates[{i - 1}] is too negative')
                growth *= 1 + f / m
                spots.append(m * (growth ** (1.0 / i) - 1))
        else:
            source = 'flat reference_rate'
            spots = [reference_rate] * periods_remaining
        if len(spots) < periods_remaining:
            notes.append(f'curve has {len(spots)} point(s) for {periods_remaining} periods; the last rate is '
                         'held flat for the remaining periods')
            if source == 'forward_rates':
                last_f = forward_rates[-1]
                growth = (1 + spots[-1] / m) ** len(spots)
                for i in range(len(spots) + 1, periods_remaining + 1):
                    growth *= 1 + last_f / m
                    spots.append(m * (growth ** (1.0 / i) - 1))
            else:
                spots += [spots[-1]] * (periods_remaining - len(spots))
        spots = spots[:periods_remaining]
        for i, s in enumerate(spots):
            if 1 + s / m <= 0:
                raise FIError(f'spot rate for period {i + 1} is too negative')
        # forwards implied by the spot curve
        forwards = []
        prev = 1.0
        for i, s in enumerate(spots, 1):
            cur = (1 + s / m) ** i
            forwards.append(m * (cur / prev - 1))
            prev = cur
        cfs = [face_value * (f + quoted_margin) / m for f in forwards]
        cfs[-1] += face_value

        def pv(z):
            total = 0.0
            for i, (cf, s) in enumerate(zip(cfs, spots), 1):
                d = 1 + (s + z) / m
                if d <= 0:
                    return float('inf')
                total += cf * d ** -i
            return total

        lo = -min(spots) - m * 0.999999
        hi = 1.0
        fn = fc.overflow_safe(lambda z: pv(z) - price)
        if fn(lo) < 0:
            raise FIError('price is too high to be reached by any Z-DM')
        while fn(hi) > 0:
            hi *= 2
            if hi > 1e4:
                raise FIError('price is too low to be reached by any Z-DM')
        zdm = fc.find_root(fn, lo, hi, tol=1e-14)
        out = {
            'zero_discount_margin': round(zdm, 8),
            'z_dm_bps': round(zdm * 10000, 2),
            'price': price,
            'calculated_price': round(pv(zdm), 4),
            'quoted_margin': quoted_margin,
            'curve_source': source,
            'projected_forward_rates': [round(f, 6) for f in forwards],
            'interpretation': 'Z-DM: spread over the spot curve that reprices the FRN, with coupons projected '
                              'from the forward rates implied by the curve',
            'conventions': {'rates': f'annual, compounded {m}x per year; one rate per reset period'},
        }
        if notes:
            out['notes'] = notes
        return out

    def frn_with_cap_floor(
        self,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        reference_rate: float = 0.05,
        cap_rate: Optional[float] = 0.08,
        floor_rate: Optional[float] = 0.02,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
        rate_volatility: float = 0.20,
        discount_margin: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Capped / floored FRN. The reference rate is assumed flat (forward = reference_rate for every
        reset); caplets and floorlets on the coupon (ref + QM) are valued with Black-76 using
        rate_volatility, discounted at (ref + DM)/m per period (DM defaults to QM, i.e. a par FRN).
        Investor view: FRN with cap & floor = straight FRN - cap + floor.
        """
        _check_frn(face_value, periods_remaining, reset_frequency)
        if cap_rate is None and floor_rate is None:
            raise FIError('give cap_rate and/or floor_rate (decimal); null means no cap / no floor')
        if cap_rate is not None and floor_rate is not None and floor_rate > cap_rate:
            raise FIError('floor_rate must not exceed cap_rate')
        if rate_volatility < 0:
            raise FIError('rate_volatility must be >= 0')
        if rate_volatility > 0 and reference_rate <= 0:
            raise FIError('the Black (lognormal) model needs reference_rate > 0')
        m = reset_frequency
        dm = quoted_margin if discount_margin is None else discount_margin
        tau = 1.0 / m
        current_coupon_rate = reference_rate + quoted_margin
        effective_rate = current_coupon_rate
        if cap_rate is not None:
            effective_rate = min(effective_rate, cap_rate)
        if floor_rate is not None:
            effective_rate = max(effective_rate, floor_rate)
        cap_binding = cap_rate is not None and current_coupon_rate > cap_rate
        floor_binding = floor_rate is not None and current_coupon_rate < floor_rate

        d = 1 + (reference_rate + dm) / m
        if d <= 0:
            raise FIError('reference_rate + discount_margin is too negative')
        cap_value = floor_value = 0.0
        caplets = []
        for i in range(1, periods_remaining + 1):
            df = d ** (-i)
            expiry = (i - 1) / m
            c = (tau * face_value * df * black76(reference_rate, cap_rate - quoted_margin, rate_volatility, expiry, True)
                 if cap_rate is not None else 0.0)
            f = (tau * face_value * df * black76(reference_rate, floor_rate - quoted_margin, rate_volatility, expiry, False)
                 if floor_rate is not None else 0.0)
            cap_value += c
            floor_value += f
            if i <= 8:
                caplets.append({'period': i, 'reset_years': round(expiry, 4), 'discount_factor': round(df, 6),
                                'caplet': round(c, 4), 'floorlet': round(f, 4)})
        straight = self.calculate_frn_price(face_value, quoted_margin, dm, reference_rate, periods_remaining, m)['price']
        with_options = straight - cap_value + floor_value

        return {
            'current_coupon_rate': round(current_coupon_rate, 6),
            'effective_coupon_rate': round(effective_rate, 6),
            'cap_rate': cap_rate,
            'floor_rate': floor_rate,
            'cap_binding': cap_binding,
            'floor_binding': floor_binding,
            'rate_status': 'At cap' if cap_binding else 'At floor' if floor_binding else 'Unconstrained',
            'embedded_options': {
                'cap_value_estimate': round(cap_value, 4),
                'floor_value_estimate': round(floor_value, 4),
                'collar_net_value': round(floor_value - cap_value, 4),
            },
            'straight_frn_price': round(straight, 4),
            'frn_price_with_options': round(with_options, 4),
            'first_periods': caplets,
            'model': {
                'name': 'Black-76 caplets/floorlets on the reference rate (strike = cap/floor - quoted margin)',
                'forward_rates': 'flat at reference_rate',
                'rate_volatility': rate_volatility,
                'discounting': f'(reference_rate + discount_margin)/{m} per period, discount_margin = {dm:g}',
                'first_period': 'coupon already set: intrinsic value only',
            },
            'investor_impact': {
                'cap': 'Limits upside in rising rate environment',
                'floor': 'Provides downside protection in falling rates',
                'collar': 'Combined cap and floor'
            }
        }

    def describe_reference_rates(
        self,
        rate_type: str = None,
    ) -> Dict[str, Any]:
        """
        Describe reference rates and LIBOR transition.

        Args:
            rate_type: Specific rate to describe

        Returns:
            Dictionary with reference rate information
        """
        rates = {
            'sofr': {
                'name': 'Secured Overnight Financing Rate',
                'administrator': 'Federal Reserve Bank of New York',
                'description': 'Overnight Treasury repo rate',
                'characteristics': {
                    'secured': True,
                    'overnight': True,
                    'risk_free': 'Near risk-free (Treasury collateral)',
                    'volume': 'Very high daily volume ($1+ trillion)'
                },
                'variants': {
                    'daily_sofr': 'Published each business day',
                    'sofr_averages': '30, 90, 180-day averages',
                    'sofr_index': 'Compounded daily index',
                    'term_sofr': 'Forward-looking term rates (CME)'
                },
                'use_cases': 'FRNs, loans, derivatives',
                'transition': 'Primary USD LIBOR replacement'
            },
            'euribor': {
                'name': 'Euro Interbank Offered Rate',
                'administrator': 'European Money Markets Institute',
                'description': 'Euro unsecured interbank lending rate',
                'tenors': ['1 week', '1 month', '3 month', '6 month', '12 month'],
                'characteristics': {
                    'unsecured': True,
                    'term_rate': True,
                    'panel_based': 'Submissions from panel banks'
                },
                'status': 'Continues (reformed methodology)'
            },
            'sonia': {
                'name': 'Sterling Overnight Index Average',
                'administrator': 'Bank of England',
                'description': 'GBP unsecured overnight rate',
                'characteristics': {
                    'overnight': True,
                    'unsecured': True,
                    'transaction_based': True
                },
                'transition': 'GBP LIBOR replacement'
            },
            'libor_transition': {
                'background': 'LIBOR ceased publication for most currencies in 2023',
                'reasons': [
                    'Declining underlying transaction volume',
                    'Manipulation scandals',
                    'Panel bank concerns'
                ],
                'replacement_rates': {
                    'usd': 'SOFR',
                    'gbp': 'SONIA',
                    'eur': 'ESTR (derivatives), EURIBOR (loans)',
                    'jpy': 'TONAR',
                    'chf': 'SARON'
                },
                'transition_challenges': [
                    'Term rate vs overnight rate',
                    'Secured vs unsecured basis',
                    'Spread adjustment for legacy contracts',
                    'Fallback language in contracts'
                ]
            }
        }

        if rate_type and rate_type.lower() != 'all':
            rate_lower = rate_type.lower()
            rate_lower = {'libor': 'libor_transition', 'transition': 'libor_transition'}.get(rate_lower, rate_lower)
            if rate_lower in rates:
                return {'rate': rate_type, 'details': rates[rate_lower]}
            raise FIError(f'Unknown rate: {rate_type}. Available: {", ".join(rates)}, libor or all')

        return {'reference_rates': rates}


def _mm_check(face_value: float, price: float, days_to_maturity: float) -> None:
    if face_value <= 0 or price <= 0:
        raise FIError('face_value and price must be > 0')
    if days_to_maturity <= 0:
        raise FIError('days_to_maturity must be > 0')


class MoneyMarketAnalyzer:
    """Money market yield measures for discount instruments (T-bills, CP, BAs)."""

    def calculate_discount_yield(self, face_value: float, price: float, days_to_maturity: int) -> Dict[str, Any]:
        """Bank discount yield = (FV - P) / FV x 360 / days"""
        _mm_check(face_value, price, days_to_maturity)
        discount = face_value - price
        discount_yield = (discount / face_value) * (360 / days_to_maturity)
        return {
            'discount_yield': round(discount_yield, 6),
            'discount_yield_pct': round(discount_yield * 100, 4),
            'discount_amount': round(discount, 2),
            'face_value': face_value,
            'price': price,
            'days_to_maturity': days_to_maturity,
            'day_count': '360',
            'note': 'Bank discount yield understates true yield (based on face value, not price)'
        }

    def calculate_money_market_yield(self, face_value: float, price: float, days_to_maturity: int) -> Dict[str, Any]:
        """Money market (CD-equivalent) yield = (FV - P) / P x 360 / days"""
        _mm_check(face_value, price, days_to_maturity)
        mm_yield = ((face_value - price) / price) * (360 / days_to_maturity)
        return {
            'money_market_yield': round(mm_yield, 6),
            'mm_yield_pct': round(mm_yield * 100, 4),
            'face_value': face_value,
            'price': price,
            'days_to_maturity': days_to_maturity,
            'note': 'Also called CD equivalent yield; based on price (more accurate than discount yield)'
        }

    def calculate_bond_equivalent_yield(self, face_value: float, price: float, days_to_maturity: int) -> Dict[str, Any]:
        """Bond equivalent yield (365-day, simple) = (FV - P) / P x 365 / days"""
        _mm_check(face_value, price, days_to_maturity)
        bey = ((face_value - price) / price) * (365 / days_to_maturity)
        return {
            'bond_equivalent_yield': round(bey, 6),
            'bey_pct': round(bey * 100, 4),
            'face_value': face_value,
            'price': price,
            'days_to_maturity': days_to_maturity,
            'day_count': '365',
            'note': 'Simple-interest 365-day basis (CFA definition for instruments of up to 6 months)'
        }

    def calculate_effective_annual_yield(self, face_value: float, price: float, days_to_maturity: int) -> Dict[str, Any]:
        """EAY = (FV / P)^(365 / days) - 1"""
        _mm_check(face_value, price, days_to_maturity)
        hpr = (face_value - price) / price
        eay = (1 + hpr) ** (365 / days_to_maturity) - 1
        return {
            'effective_annual_yield': round(eay, 6),
            'eay_pct': round(eay * 100, 4),
            'holding_period_return': round(hpr, 6),
            'hpr_pct': round(hpr * 100, 4),
            'days_to_maturity': days_to_maturity,
            'note': 'Accounts for compounding; best for comparing across maturities'
        }

    def compare_all_yields(self, face_value: float = 1000.0, price: float = 990.0,
                           days_to_maturity: int = 90) -> Dict[str, Any]:
        """All money market yield measures side by side (values in percent)."""
        _mm_check(face_value, price, days_to_maturity)
        discount_yield = (face_value - price) / face_value * (360 / days_to_maturity)
        mm_yield = (face_value - price) / price * (360 / days_to_maturity)
        bey = (face_value - price) / price * (365 / days_to_maturity)
        hpr = (face_value - price) / price
        eay = (1 + hpr) ** (365 / days_to_maturity) - 1
        return {
            'inputs': {'face_value': face_value, 'price': price, 'days_to_maturity': days_to_maturity},
            'yield_measures': {
                'bank_discount_yield': {'value': round(discount_yield * 100, 4), 'formula': '(FV-P)/FV × 360/t',
                                        'basis': 'Face value, 360 days', 'use': 'T-bill quotes'},
                'money_market_yield': {'value': round(mm_yield * 100, 4), 'formula': '(FV-P)/P × 360/t',
                                       'basis': 'Price, 360 days', 'use': 'CD quotes'},
                'bond_equivalent_yield': {'value': round(bey * 100, 4), 'formula': '(FV-P)/P × 365/t',
                                          'basis': 'Price, 365 days', 'use': 'Compare to bonds'},
                'effective_annual_yield': {'value': round(eay * 100, 4), 'formula': '(1 + HPR)^(365/t) - 1',
                                           'basis': 'Compounded, 365 days', 'use': 'True annualized return'},
            },
            'units': 'percent',
            'comparison': {
                'lowest_to_highest': 'Bank Discount < MM Yield < BEY < EAY (for a discount instrument under a year)',
                'why': 'Discount yield uses higher denominator (FV); EAY compounds'
            }
        }

    def price_from_discount_yield(self, face_value: float, discount_yield: float,
                                  days_to_maturity: int) -> Dict[str, Any]:
        """Price = FV x (1 - discount yield x days / 360)"""
        if face_value <= 0 or days_to_maturity <= 0:
            raise FIError('face_value and days_to_maturity must be > 0')
        price = face_value * (1 - discount_yield * days_to_maturity / 360)
        if price <= 0:
            raise FIError('discount_yield x days / 360 must be < 1')
        return {
            'price': round(price, 4),
            'discount': round(face_value - price, 4),
            'face_value': face_value,
            'discount_yield': discount_yield,
            'days_to_maturity': days_to_maturity
        }


ANALYSIS_TYPES = ('frn_price', 'discount_margin', 'zero_discount_margin', 'frn_cap_floor', 'reference_rates',
                  'discount_yield', 'money_market_yield', 'bond_equivalent_yield', 'effective_annual_yield',
                  'compare_yields', 'price_from_discount')

_YIELD_TYPES = {
    'money_market': 'money_market_yield', 'mm': 'money_market_yield', 'cd_equivalent': 'money_market_yield',
    'money_market_yield': 'money_market_yield',
    'discount': 'discount_yield', 'bank_discount': 'discount_yield', 'discount_yield': 'discount_yield',
    'bond_equivalent': 'bond_equivalent_yield', 'bey': 'bond_equivalent_yield',
    'bond_equivalent_yield': 'bond_equivalent_yield',
    'effective_annual': 'effective_annual_yield', 'eay': 'effective_annual_yield',
    'effective_annual_yield': 'effective_annual_yield',
    'all': 'compare_yields',
}


def _periods(params: Dict[str, Any], reset_frequency: int) -> int:
    """periods_remaining, or years_to_maturity x reset_frequency (must be a whole number)."""
    if params.get('periods_remaining') is not None:
        return num(params, 'periods_remaining', integer=True, lo=1)
    if params.get('years_to_maturity') is not None:
        years = num(params, 'years_to_maturity', lo=0, lo_open=True)
        x = years * reset_frequency
        if abs(x - round(x)) > 1e-6:
            raise FIError(f'years_to_maturity x reset_frequency must be a whole number of periods (got {x:g}); '
                          'pass periods_remaining instead')
        return max(int(round(x)), 1)
    return 20


def _mm_inputs(params):
    return dict(face_value=num(params, 'face_value', 1000), price=num(params, 'price', 990),
                days_to_maturity=num(params, 'days_to_maturity', 90))


def run_floating_rate_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Main entry point for floating rate and money market analysis (analysis_type in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', 'frn_price')

    try:
        frn = FloatingRateAnalyzer()
        mm = MoneyMarketAnalyzer()
        if analysis_type in ('frn_price', 'discount_margin', 'zero_discount_margin', 'frn_cap_floor'):
            freq = num(params, 'reset_frequency', 4, integer=True)
            common = dict(face_value=num(params, 'face_value', 1000),
                          quoted_margin=num(params, 'quoted_margin', 0.01),
                          periods_remaining=_periods(params, freq),
                          reset_frequency=freq)

        if analysis_type == 'frn_price':
            return frn.calculate_frn_price(discount_margin=num(params, 'discount_margin', 0.01),
                                           reference_rate=num(params, 'reference_rate', 0.05), **common)

        elif analysis_type == 'discount_margin':
            return frn.calculate_discount_margin(price=num(params, 'price', 1000),
                                                 reference_rate=num(params, 'reference_rate', 0.05), **common)

        elif analysis_type == 'zero_discount_margin':
            spots = fc.num_list(params, 'spot_rates', min_len=1) if params.get('spot_rates') is not None else None
            fwds = fc.num_list(params, 'forward_rates', min_len=1) if params.get('forward_rates') is not None else None
            return frn.calculate_zero_discount_margin(price=num(params, 'price', 1000), spot_rates=spots,
                                                      forward_rates=fwds,
                                                      reference_rate=num(params, 'reference_rate', 0.05), **common)

        elif analysis_type == 'frn_cap_floor':
            return frn.frn_with_cap_floor(
                reference_rate=num(params, 'reference_rate', 0.05),
                cap_rate=num(params, 'cap_rate', 0.08, allow_none=True),
                floor_rate=num(params, 'floor_rate', 0.02, allow_none=True),
                rate_volatility=num(params, 'rate_volatility', 0.20, aliases=('volatility',)),
                discount_margin=num(params, 'discount_margin', None, allow_none=True),
                **common)

        elif analysis_type == 'reference_rates':
            return frn.describe_reference_rates(rate_type=fc.text(params, 'rate_type', None, aliases=('rate_name',)))

        elif analysis_type == 'discount_yield':
            return mm.calculate_discount_yield(**_mm_inputs(params))

        elif analysis_type == 'money_market_yield':
            yt = fc.text(params, 'yield_type', 'money_market')
            if yt not in _YIELD_TYPES:
                raise FIError('yield_type must be one of: money_market, discount, bond_equivalent, '
                              'effective_annual, all')
            target = _YIELD_TYPES[yt]
            fn = {'money_market_yield': mm.calculate_money_market_yield,
                  'discount_yield': mm.calculate_discount_yield,
                  'bond_equivalent_yield': mm.calculate_bond_equivalent_yield,
                  'effective_annual_yield': mm.calculate_effective_annual_yield,
                  'compare_yields': mm.compare_all_yields}[target]
            out = fn(**_mm_inputs(params))
            out['yield_type'] = yt
            return out

        elif analysis_type == 'bond_equivalent_yield':
            return mm.calculate_bond_equivalent_yield(**_mm_inputs(params))

        elif analysis_type == 'effective_annual_yield':
            return mm.calculate_effective_annual_yield(**_mm_inputs(params))

        elif analysis_type == 'compare_yields':
            return mm.compare_all_yields(**_mm_inputs(params))

        elif analysis_type == 'price_from_discount':
            return mm.price_from_discount_yield(face_value=num(params, 'face_value', 1000),
                                                discount_yield=num(params, 'discount_yield', 0.04),
                                                days_to_maturity=num(params, 'days_to_maturity', 90))

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Floating rate analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_floating_rate_analysis, ANALYSIS_TYPES, 'floating_rate')
